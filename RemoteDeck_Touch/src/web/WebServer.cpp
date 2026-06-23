// Design Ref: §2.1, §4 API Spec — esp_http_server 래퍼 구현
//   - module-poc 단계: /api/status + / (welcome) 만 활성
//   - 향후 module-webui 에서 /api/images/*, /api/config, /api/log, /api/imagesconfig 추가
//   - module-control 에서 /api/control (Long polling), module-ota 에서 /api/ota
// Plan SC: FR-01, FR-02 (PoC), FR-08 (Basic Auth)

#include "WebServer.h"
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <string.h>

static const char* TAG = "WebServer";

bool WebServer::begin(uint16_t port, const TouchAuth* auth) {
    _auth = auth;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    // Design Ref: §2.1 — core 0 pinning, LVGL/MQTT (core 1) 와 물리 격리
    // v2.3-poc-B2: B1 (stack 16K, sockets 3) → httpd_start ESP_ERR_NO_MEM 추정 (47K heap 부족)
    //   - stack_size 12K  (8K → 12K, 16K 는 alloc fail)
    //   - sockets    4    (7 → 4, 동시성 부담 ↓ 정도)
    //   - priority   4    (LVGL/MQTT 와 경합 완화)
    cfg.server_port      = port;
    cfg.task_priority    = 4;
    cfg.stack_size       = 12288;
    cfg.core_id          = 0;
    cfg.max_open_sockets = 4;
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;
    cfg.backlog_conn     = 4;

    esp_err_t err = httpd_start(&_server, &cfg);
    if (err != ESP_OK) {
        Serial.printf("[%s] httpd_start failed: %d\n", TAG, err);
        _server = nullptr;
        return false;
    }

    registerHandlers();

    Serial.printf("[%s] started on port %u, core 0, task_priority 5, auth=%s\n",
                  TAG, port, _auth ? "on" : "off");
    return true;
}

void WebServer::stop() {
    if (_server) {
        httpd_stop(_server);
        _server = nullptr;
    }
}

void WebServer::send401(httpd_req_t* req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RemoteDeck_Touch\"");
    httpd_resp_set_type(req, "application/json");
    const char* body = "{\"ok\":false,\"error\":\"unauthorized\"}";
    httpd_resp_send(req, body, strlen(body));
}

void WebServer::sendJson(httpd_req_t* req, int statusCode, const char* json) {
    char status[16];
    switch (statusCode) {
        case 200: strcpy(status, "200 OK"); break;
        case 400: strcpy(status, "400 Bad Request"); break;
        case 404: strcpy(status, "404 Not Found"); break;
        case 413: strcpy(status, "413 Too Large"); break;
        case 500: strcpy(status, "500 Internal"); break;
        default:  snprintf(status, sizeof(status), "%d Status", statusCode); break;
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
}

// Basic Auth — Authorization 헤더 파싱 + base64 디코딩 + user:pass 비교
// Design Ref: §10.4, §7 — Single Source of Truth
bool WebServer::requireAuth(httpd_req_t* req) {
    if (!_auth) return true;  // auth 비활성 모드

    size_t hdrLen = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdrLen == 0 || hdrLen > 256) {
        send401(req);
        return false;
    }

    char hdr[260];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        send401(req);
        return false;
    }

    // "Basic <base64>"
    const char* prefix = "Basic ";
    if (strncmp(hdr, prefix, 6) != 0) {
        send401(req);
        return false;
    }

    // base64 decode
    unsigned char decoded[128];
    size_t outLen = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &outLen,
                              (const unsigned char*)(hdr + 6),
                              strlen(hdr + 6)) != 0) {
        send401(req);
        return false;
    }
    decoded[outLen] = '\0';

    // "user:pass" 비교
    char expected[64];
    snprintf(expected, sizeof(expected), "%s:%s", _auth->user, _auth->pass);
    if (strcmp((char*)decoded, expected) != 0) {
        send401(req);
        return false;
    }

    return true;
}

void WebServer::logEvent(const char* event, const char* detail) {
    if (_onLog) _onLog(event, detail);
}

// --- URI Handler 등록 ---
// module-poc: /api/status + / (welcome)
// 향후 모듈은 같은 패턴으로 trampoline + handle* 쌍을 추가
void WebServer::registerHandlers() {
    httpd_uri_t status_uri = {
        .uri      = "/api/status",
        .method   = HTTP_GET,
        .handler  = &WebServer::trampolineStatus,
        .user_ctx = this
    };
    httpd_register_uri_handler(_server, &status_uri);

    httpd_uri_t root_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = &WebServer::trampolineRoot,
        .user_ctx = this
    };
    httpd_register_uri_handler(_server, &root_uri);
}

// --- Trampolines (C → C++) ---
esp_err_t WebServer::trampolineStatus(httpd_req_t* req) {
    auto* self = static_cast<WebServer*>(req->user_ctx);
    return self->handleStatus(req);
}

esp_err_t WebServer::trampolineRoot(httpd_req_t* req) {
    auto* self = static_cast<WebServer*>(req->user_ctx);
    return self->handleRoot(req);
}

// --- Handlers ---
esp_err_t WebServer::handleStatus(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;

    if (_getStatus) {
        String body = _getStatus();
        sendJson(req, 200, body.c_str());
    } else {
        sendJson(req, 500, "{\"ok\":false,\"error\":\"no_handler\"}");
    }
    return ESP_OK;
}

// module-poc: 루트는 단순 welcome.
// module-webui 에서 SPIFFS /www/index.html 서빙으로 교체.
esp_err_t WebServer::handleRoot(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const char* html =
        "<!doctype html><meta charset=utf-8>"
        "<title>RemoteDeck_Touch v2.3.0 (PoC)</title>"
        "<h1>RemoteDeck_Touch v2.3.0 — module-poc</h1>"
        "<p>esp_http_server alive on core 0.</p>"
        "<p><a href=/api/status>/api/status</a></p>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}
