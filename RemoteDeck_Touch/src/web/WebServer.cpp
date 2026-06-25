// Design Ref: §2.1, §4 — esp_http_server 래퍼 구현 (module-poc + module-webui)
// Plan SC: FR-01, FR-02, FR-03, FR-04, FR-08

#include "WebServer.h"
#include "embedded_assets.h"
#include <SPIFFS.h>
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <string.h>

static const char* TAG = "WebServer";

bool WebServer::begin(uint16_t port, const TouchAuth* auth) {
    _auth = auth;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    // Design Ref: §2.1 — core 0 pinning
    // v2.3-poc-B2 config 유지 (PoC 검증 완료)
    cfg.server_port      = port;
    cfg.task_priority    = 4;
    cfg.stack_size       = 12288;
    cfg.core_id          = 0;
    cfg.max_open_sockets = 4;
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;
    cfg.backlog_conn     = 4;
    cfg.uri_match_fn     = httpd_uri_match_wildcard;  // /api/images/* 등 wildcard

    esp_err_t err = httpd_start(&_server, &cfg);
    if (err != ESP_OK) {
        Serial.printf("[%s] httpd_start failed: %d\n", TAG, err);
        _server = nullptr;
        return false;
    }

    registerHandlers();
    Serial.printf("[%s] started port=%u core=0 stack=12K sockets=4 prio=4\n", TAG, port);
    return true;
}

void WebServer::stop() {
    if (_server) {
        httpd_stop(_server);
        _server = nullptr;
    }
}

// --- Auth + response helpers ---
void WebServer::send401(httpd_req_t* req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RemoteDeck_Touch\"");
    httpd_resp_set_type(req, "application/json");
    const char* body = "{\"ok\":false,\"error\":\"unauthorized\"}";
    httpd_resp_send(req, body, strlen(body));
}

void WebServer::sendJson(httpd_req_t* req, int statusCode, const char* json) {
    char status[24];
    switch (statusCode) {
        case 200: strcpy(status, "200 OK"); break;
        case 400: strcpy(status, "400 Bad Request"); break;
        case 404: strcpy(status, "404 Not Found"); break;
        case 413: strcpy(status, "413 Too Large"); break;
        case 500: strcpy(status, "500 Internal"); break;
        default:  snprintf(status, sizeof(status), "%d", statusCode); break;
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
}

void WebServer::sendJsonString(httpd_req_t* req, int statusCode, const String& json) {
    sendJson(req, statusCode, json.c_str());
}

bool WebServer::requireAuth(httpd_req_t* req) {
    if (!_auth) return true;
    size_t hdrLen = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdrLen == 0 || hdrLen > 256) { send401(req); return false; }
    char hdr[260];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        send401(req); return false;
    }
    if (strncmp(hdr, "Basic ", 6) != 0) { send401(req); return false; }
    unsigned char decoded[128];
    size_t outLen = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &outLen,
                              (const unsigned char*)(hdr + 6), strlen(hdr + 6)) != 0) {
        send401(req); return false;
    }
    decoded[outLen] = '\0';
    char expected[64];
    snprintf(expected, sizeof(expected), "%s:%s", _auth->user, _auth->pass);
    if (strcmp((char*)decoded, expected) != 0) { send401(req); return false; }
    return true;
}

void WebServer::logEvent(const char* event, const char* detail) {
    if (_onLog) _onLog(event, detail);
}

bool WebServer::sanitizeImageName(const String& in, String& out) const {
    int s1 = in.lastIndexOf('/');
    int s2 = in.lastIndexOf('\\');
    int sep = s1 > s2 ? s1 : s2;
    String base = (sep >= 0) ? in.substring(sep + 1) : in;
    base.trim();
    if (base.length() == 0 || base.indexOf("..") >= 0) return false;
    String lower = base; lower.toLowerCase();
    if (!lower.endsWith(".png") && !lower.endsWith(".bmp")) return false;
    out = base;
    return true;
}

// --- SPIFFS static file streaming ---
esp_err_t WebServer::serveSpiffsFile(httpd_req_t* req, const char* path, const char* mime) {
    if (!SPIFFS.exists(path)) {
        sendJson(req, 404, "{\"ok\":false,\"error\":\"not_found\"}");
        return ESP_OK;
    }
    File f = SPIFFS.open(path, "r");
    if (!f) {
        sendJson(req, 500, "{\"ok\":false,\"error\":\"open_failed\"}");
        return ESP_OK;
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, mime);
    // 캐시 비활성 — 펌웨어 갱신 시 즉시 반영
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    uint8_t buf[1024];
    while (f.available()) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        if (httpd_resp_send_chunk(req, (const char*)buf, n) != ESP_OK) {
            f.close();
            return ESP_FAIL;
        }
    }
    f.close();
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

// --- URI registration ---
void WebServer::registerHandlers() {
    auto reg = [&](const char* uri, httpd_method_t method, esp_err_t(*h)(httpd_req_t*)) {
        httpd_uri_t u = { .uri = uri, .method = method, .handler = h, .user_ctx = this };
        httpd_register_uri_handler(_server, &u);
    };
    // 정적 자산
    reg("/",          HTTP_GET, &WebServer::trampolineRoot);
    reg("/style.css", HTTP_GET, &WebServer::trampolineStyle);
    reg("/app.js",    HTTP_GET, &WebServer::trampolineAppJs);
    // 상태/이미지/설정/로그/리부트
    reg("/api/status",       HTTP_GET,    &WebServer::trampolineStatus);
    reg("/api/images/list",  HTTP_GET,    &WebServer::trampolineImagesList);
    reg("/api/images/upload",HTTP_POST,   &WebServer::trampolineImagesUpload);
    reg("/api/images/*",     HTTP_GET,    &WebServer::trampolineImagesGet);
    reg("/api/images/*",     HTTP_DELETE, &WebServer::trampolineImagesDel);
    reg("/api/imagesconfig", HTTP_GET,    &WebServer::trampolineImagesConfig);
    reg("/api/config",       HTTP_GET,    &WebServer::trampolineConfigGet);
    reg("/api/config",       HTTP_POST,   &WebServer::trampolineConfigPost);
    reg("/api/log",          HTTP_GET,    &WebServer::trampolineLog);
    reg("/api/reboot",       HTTP_POST,   &WebServer::trampolineReboot);
    reg("/api/ota",          HTTP_POST,   &WebServer::trampolineOtaUpload);
}

// --- Trampolines (boilerplate) ---
#define TRAMP(NAME, FUNC) \
    esp_err_t WebServer::NAME(httpd_req_t* req) { \
        return static_cast<WebServer*>(req->user_ctx)->FUNC(req); \
    }
TRAMP(trampolineRoot,         handleRoot)
TRAMP(trampolineStyle,        handleStyle)
TRAMP(trampolineAppJs,        handleAppJs)
TRAMP(trampolineStatus,       handleStatus)
TRAMP(trampolineImagesList,   handleImagesList)
TRAMP(trampolineImagesUpload, handleImagesUpload)
TRAMP(trampolineImagesGet,    handleImagesGet)
TRAMP(trampolineImagesDel,    handleImagesDel)
TRAMP(trampolineImagesConfig, handleImagesConfig)
TRAMP(trampolineConfigGet,    handleConfigGet)
TRAMP(trampolineConfigPost,   handleConfigPost)
TRAMP(trampolineLog,          handleLog)
TRAMP(trampolineReboot,       handleReboot)
TRAMP(trampolineOtaUpload,    handleOtaUpload)
#undef TRAMP

// --- Static handlers (PROGMEM embed — SPIFFS 미사용) ---
// Design Ref: §5.3 — 펌웨어 Flash 에 인라인. uploadfs 불필요, /images/* 등 SPIFFS 보존.
// 4KB 이상 단일 httpd_resp_send 시 TCP send 가 hang 하는 현상 → chunked 전송.
static esp_err_t sendProgmem(httpd_req_t* req, const char* data, const char* mime) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, mime);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    size_t total = strlen(data);
    const size_t CHUNK = 1024;
    size_t sent = 0;
    while (sent < total) {
        size_t n = (total - sent) > CHUNK ? CHUNK : (total - sent);
        if (httpd_resp_send_chunk(req, data + sent, n) != ESP_OK) {
            return ESP_FAIL;
        }
        sent += n;
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}
esp_err_t WebServer::handleRoot(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    return sendProgmem(req, INDEX_HTML, "text/html; charset=utf-8");
}
esp_err_t WebServer::handleStyle(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    return sendProgmem(req, STYLE_CSS, "text/css");
}
esp_err_t WebServer::handleAppJs(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    return sendProgmem(req, APP_JS, "application/javascript");
}

// --- Status / list / config getters ---
esp_err_t WebServer::handleStatus(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (_getStatus) sendJsonString(req, 200, _getStatus());
    else            sendJson(req, 500, "{\"ok\":false,\"error\":\"no_handler\"}");
    return ESP_OK;
}
esp_err_t WebServer::handleImagesList(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (_getImagesList) sendJsonString(req, 200, _getImagesList());
    else                sendJson(req, 500, "{\"ok\":false}");
    return ESP_OK;
}
esp_err_t WebServer::handleImagesConfig(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (_getImagesConfig) sendJsonString(req, 200, _getImagesConfig());
    else                  sendJson(req, 404, "{\"ok\":false,\"error\":\"not_found\"}");
    return ESP_OK;
}
esp_err_t WebServer::handleConfigGet(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (_getDeviceConfig) sendJsonString(req, 200, _getDeviceConfig());
    else                  sendJson(req, 500, "{\"ok\":false}");
    return ESP_OK;
}
esp_err_t WebServer::handleLog(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (_getLogJson) sendJsonString(req, 200, _getLogJson());
    else             sendJson(req, 500, "{\"ok\":false}");
    return ESP_OK;
}

// --- Config POST ---
esp_err_t WebServer::handleConfigPost(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (!_setDeviceConfig) { sendJson(req, 500, "{\"ok\":false,\"error\":\"no_handler\"}"); return ESP_OK; }
    if (req->content_len == 0 || req->content_len > 4096) {
        sendJson(req, 413, "{\"ok\":false,\"error\":\"too_large\"}");
        return ESP_OK;
    }
    String body; body.reserve(req->content_len + 4);
    char buf[513];
    size_t remain = req->content_len;
    while (remain > 0) {
        int r = httpd_req_recv(req, buf, remain > 512 ? 512 : remain);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            sendJson(req, 500, "{\"ok\":false,\"error\":\"recv_failed\"}");
            return ESP_OK;
        }
        buf[r] = 0;
        body += buf;
        remain -= r;
    }
    String err;
    if (_setDeviceConfig(body, err)) {
        logEvent("CFG_SAVE", "ok");
        sendJson(req, 200, "{\"ok\":true}");
    } else {
        char e[160];
        snprintf(e, sizeof(e), "{\"ok\":false,\"error\":\"%s\"}", err.c_str());
        logEvent("CFG_SAVE", err.c_str());
        sendJson(req, 400, e);
    }
    return ESP_OK;
}

// --- Reboot ---
esp_err_t WebServer::handleReboot(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    logEvent("REBOOT", "armed");
    sendJson(req, 200, "{\"ok\":true,\"reboot\":true}");
    if (_onReboot) _onReboot();
    return ESP_OK;
}

// --- Images GET/DELETE by name (wildcard route extracts name from URI) ---
static String parseImageNameFromUri(const char* uri) {
    // /api/images/<name>
    const char* prefix = "/api/images/";
    size_t pl = strlen(prefix);
    if (strncmp(uri, prefix, pl) != 0) return String();
    return String(uri + pl);
}

esp_err_t WebServer::handleImagesGet(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    String raw = parseImageNameFromUri(req->uri);
    if (raw == "list" || raw == "upload") {  // wildcard 가 더 우선 매칭되지 않도록 가드
        sendJson(req, 404, "{\"ok\":false,\"error\":\"not_found\"}");
        return ESP_OK;
    }
    String name;
    if (!sanitizeImageName(raw, name)) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"bad_name\"}");
        return ESP_OK;
    }
    String path = "/images/" + name;
    const char* mime = name.endsWith(".png") || name.endsWith(".PNG")
        ? "image/png" : "image/bmp";
    return serveSpiffsFile(req, path.c_str(), mime);
}

esp_err_t WebServer::handleImagesDel(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    String raw = parseImageNameFromUri(req->uri);
    String name;
    if (!sanitizeImageName(raw, name)) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"bad_name\"}");
        return ESP_OK;
    }
    String path = "/images/" + name;
    bool removed = SPIFFS.remove(path);
    if (_deleteImage) _deleteImage(name);
    char detail[80]; snprintf(detail, sizeof(detail), "%s (%s)", name.c_str(), removed ? "ok" : "missing");
    logEvent("IMG_DELETE", detail);
    String body = String("{\"ok\":") + (removed ? "true" : "false") + ",\"name\":\"" + name + "\"}";
    sendJsonString(req, 200, body);
    return ESP_OK;
}

// --- Multipart upload streaming (binary-safe) ---
// 핵심 fix: Arduino String 의 indexOf 는 strstr 기반 → BMP 등 binary 의 0x00 에서 검색 종료.
//   → raw uint8_t* 버퍼 + 직접 byte search 로 교체.
//
// 파서:
//   1) Content-Type 헤더에서 boundary 추출
//   2) preamble + 첫 boundary 스킵 (헤더 영역은 ASCII 전용이라 String OK)
//   3) Content-Disposition 의 filename 추출
//   4) "\r\n\r\n" 이후 body 스트리밍, 끝은 "\r\n--<boundary>"
// 메모리: rolling tail buffer (boundary + 4) 유지하며 streaming.
static int memfind(const uint8_t* hay, size_t hl, const uint8_t* needle, size_t nl) {
    if (nl == 0 || hl < nl) return -1;
    for (size_t i = 0; i + nl <= hl; ++i) {
        if (memcmp(hay + i, needle, nl) == 0) return (int)i;
    }
    return -1;
}

esp_err_t WebServer::handleImagesUpload(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (!_uploadStart || !_uploadChunk) {
        sendJson(req, 500, "{\"ok\":false,\"error\":\"no_handler\"}");
        return ESP_OK;
    }

    char ct[256];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) != ESP_OK) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"no_content_type\"}");
        return ESP_OK;
    }
    char* bptr = strstr(ct, "boundary=");
    if (!bptr) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"no_boundary\"}");
        return ESP_OK;
    }
    // boundary 값은 ASCII — String OK (header 부분)
    String boundary = String("--") + (bptr + 9);
    // body 끝 마커 = "\r\n--<boundary>" — binary search 용 raw bytes
    String boundaryNl = String("\r\n") + boundary;
    size_t bndLen   = boundary.length();
    size_t bndNlLen = boundaryNl.length();
    const uint8_t* bndBytes   = (const uint8_t*)boundary.c_str();
    const uint8_t* bndNlBytes = (const uint8_t*)boundaryNl.c_str();

    enum { ST_PREAMBLE, ST_HEADER, ST_BODY, ST_DONE } st = ST_PREAMBLE;

    String headerBuf;  // 헤더 영역은 ASCII 라 String OK
    String filename;
    bool uploadStarted = false;
    bool uploadOk      = true;
    size_t totalContent = req->content_len;
    size_t consumed = 0;

    // body 용 raw tail buffer
    const size_t TAIL_CAP = 256;  // bndNlLen 보통 50 이내, 안전 4배
    uint8_t* tailBuf = (uint8_t*)malloc(TAIL_CAP);
    size_t tailLen = 0;

    const size_t RECV_BUF = 1024;
    uint8_t* rbuf = (uint8_t*)malloc(RECV_BUF);
    if (!rbuf || !tailBuf) {
        if (rbuf) free(rbuf);
        if (tailBuf) free(tailBuf);
        sendJson(req, 500, "{\"ok\":false,\"error\":\"alloc_fail\"}");
        return ESP_OK;
    }
    if (bndNlLen + 4 > TAIL_CAP) {
        free(rbuf); free(tailBuf);
        sendJson(req, 400, "{\"ok\":false,\"error\":\"boundary_too_long\"}");
        return ESP_OK;
    }

    while (consumed < totalContent && st != ST_DONE) {
        size_t want = totalContent - consumed;
        if (want > RECV_BUF) want = RECV_BUF;
        int r = httpd_req_recv(req, (char*)rbuf, want);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            uploadOk = false; break;
        }
        consumed += r;

        size_t off = 0;
        while (off < (size_t)r && st != ST_DONE) {
            if (st == ST_PREAMBLE) {
                // 헤더는 ASCII — String 누적 + indexOf 안전
                headerBuf += (char)rbuf[off]; off++;
                int idx = headerBuf.indexOf(boundary);
                if (idx >= 0) {
                    int afterB = idx + bndLen;
                    if (headerBuf.length() >= (unsigned)(afterB + 2)) {
                        headerBuf = headerBuf.substring(afterB + 2);
                        st = ST_HEADER;
                    }
                }
            } else if (st == ST_HEADER) {
                headerBuf += (char)rbuf[off]; off++;
                int blank = headerBuf.indexOf("\r\n\r\n");
                if (blank >= 0) {
                    String hdrs = headerBuf.substring(0, blank);
                    int fnPos = hdrs.indexOf("filename=\"");
                    if (fnPos >= 0) {
                        int end = hdrs.indexOf("\"", fnPos + 10);
                        if (end > fnPos) filename = hdrs.substring(fnPos + 10, end);
                    }
                    // 헤더 종료 직후 - 잔여 (보통 0 byte, 단 byte-by-byte 라 정확히 끝)
                    headerBuf = "";
                    st = ST_BODY;
                    _uploadStart(filename, totalContent);
                    uploadStarted = true;
                }
            } else if (st == ST_BODY) {
                // 남은 rbuf 를 tail 에 append. 단 tail+new > TAIL_CAP 이면 먼저 flush
                size_t avail = (size_t)r - off;
                while (avail > 0) {
                    size_t room = TAIL_CAP - tailLen;
                    size_t take = avail < room ? avail : room;
                    memcpy(tailBuf + tailLen, rbuf + off, take);
                    tailLen += take;
                    off += take;
                    avail -= take;

                    // boundary 검색 (binary-safe memfind)
                    int bIdx = memfind(tailBuf, tailLen, bndNlBytes, bndNlLen);
                    if (bIdx >= 0) {
                        if (bIdx > 0) {
                            if (!_uploadChunk(tailBuf, bIdx, false)) { uploadOk = false; }
                        }
                        _uploadChunk(nullptr, 0, true);
                        st = ST_DONE;
                        break;
                    }
                    // boundary 없음 + tail 이 거의 가득 → 안전 마진 (bndNlLen+4) 만 남기고 flush
                    if (tailLen > bndNlLen + 4) {
                        size_t flush = tailLen - bndNlLen - 4;
                        if (!_uploadChunk(tailBuf, flush, false)) { uploadOk = false; st = ST_DONE; break; }
                        memmove(tailBuf, tailBuf + flush, tailLen - flush);
                        tailLen -= flush;
                    }
                }
            }
        }
    }

    if (st != ST_DONE && uploadStarted) {
        // body 종료 marker 못 찾았으면 남은 데이터 flush 후 close
        if (tailLen > 0) _uploadChunk(tailBuf, tailLen, true);
        else _uploadChunk(nullptr, 0, true);
    }

    free(rbuf);
    free(tailBuf);

    if (uploadStarted && uploadOk) {
        char d[140];
        snprintf(d, sizeof(d), "%s (%u bytes)", filename.c_str(), (unsigned)totalContent);
        logEvent("IMG_UPLOAD", d);
        sendJson(req, 200, "{\"ok\":true,\"reloaded\":true}");
    } else {
        logEvent("IMG_UPLOAD", "fail");
        sendJson(req, 500, "{\"ok\":false,\"error\":\"upload_failed\"}");
    }
    return ESP_OK;
}

// --- OTA upload (Update.h 기반) ---
// handleImagesUpload 의 binary-safe multipart parser 동일 패턴.
// 차이: file 저장 대신 _otaStart / _otaChunk 콜백 (OtaApi → Update.write/end) 사용.
esp_err_t WebServer::handleOtaUpload(httpd_req_t* req) {
    if (!requireAuth(req)) return ESP_OK;
    if (!_otaStart || !_otaChunk) {
        sendJson(req, 500, "{\"ok\":false,\"error\":\"no_handler\"}");
        return ESP_OK;
    }

    char ct[256];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) != ESP_OK) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"no_content_type\"}");
        return ESP_OK;
    }
    char* bptr = strstr(ct, "boundary=");
    if (!bptr) {
        sendJson(req, 400, "{\"ok\":false,\"error\":\"no_boundary\"}");
        return ESP_OK;
    }
    String boundary = String("--") + (bptr + 9);
    String boundaryNl = String("\r\n") + boundary;
    size_t bndLen   = boundary.length();
    size_t bndNlLen = boundaryNl.length();
    const uint8_t* bndNlBytes = (const uint8_t*)boundaryNl.c_str();

    enum { ST_PREAMBLE, ST_HEADER, ST_BODY, ST_DONE } st = ST_PREAMBLE;
    String headerBuf;
    String filename;
    bool started = false;
    bool ok = true;
    size_t totalContent = req->content_len;
    size_t consumed = 0;

    const size_t TAIL_CAP = 256;
    uint8_t* tailBuf = (uint8_t*)malloc(TAIL_CAP);
    size_t tailLen = 0;
    const size_t RECV_BUF = 4096;  // OTA 는 큰 chunk 권장
    uint8_t* rbuf = (uint8_t*)malloc(RECV_BUF);
    if (!rbuf || !tailBuf) {
        if (rbuf) free(rbuf);
        if (tailBuf) free(tailBuf);
        sendJson(req, 500, "{\"ok\":false,\"error\":\"alloc_fail\"}");
        return ESP_OK;
    }
    if (bndNlLen + 4 > TAIL_CAP) {
        free(rbuf); free(tailBuf);
        sendJson(req, 400, "{\"ok\":false,\"error\":\"boundary_too_long\"}");
        return ESP_OK;
    }

    // OTA payload 크기 = totalContent - multipart overhead. 정확히 알기 어려우니
    // _otaStart 는 totalContent 로 호출 (Update.begin 의 size 인자는 약간 over 일 수 있어도
    // Update.h 가 실제 write 양만큼 처리하므로 무방).
    while (consumed < totalContent && st != ST_DONE) {
        size_t want = totalContent - consumed;
        if (want > RECV_BUF) want = RECV_BUF;
        int r = httpd_req_recv(req, (char*)rbuf, want);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ok = false; break;
        }
        consumed += r;

        size_t off = 0;
        while (off < (size_t)r && st != ST_DONE) {
            if (st == ST_PREAMBLE) {
                headerBuf += (char)rbuf[off]; off++;
                int idx = headerBuf.indexOf(boundary);
                if (idx >= 0) {
                    int afterB = idx + bndLen;
                    if (headerBuf.length() >= (unsigned)(afterB + 2)) {
                        headerBuf = headerBuf.substring(afterB + 2);
                        st = ST_HEADER;
                    }
                }
            } else if (st == ST_HEADER) {
                headerBuf += (char)rbuf[off]; off++;
                int blank = headerBuf.indexOf("\r\n\r\n");
                if (blank >= 0) {
                    String hdrs = headerBuf.substring(0, blank);
                    int fnPos = hdrs.indexOf("filename=\"");
                    if (fnPos >= 0) {
                        int end = hdrs.indexOf("\"", fnPos + 10);
                        if (end > fnPos) filename = hdrs.substring(fnPos + 10, end);
                    }
                    headerBuf = "";
                    st = ST_BODY;
                    if (!_otaStart(totalContent)) {
                        ok = false; st = ST_DONE; break;
                    }
                    started = true;
                }
            } else if (st == ST_BODY) {
                size_t avail = (size_t)r - off;
                while (avail > 0) {
                    size_t room = TAIL_CAP - tailLen;
                    size_t take = avail < room ? avail : room;
                    memcpy(tailBuf + tailLen, rbuf + off, take);
                    tailLen += take;
                    off += take;
                    avail -= take;

                    int bIdx = memfind(tailBuf, tailLen, bndNlBytes, bndNlLen);
                    if (bIdx >= 0) {
                        if (bIdx > 0) {
                            if (!_otaChunk(tailBuf, bIdx, false)) { ok = false; }
                        }
                        _otaChunk(nullptr, 0, true);
                        st = ST_DONE;
                        break;
                    }
                    if (tailLen > bndNlLen + 4) {
                        size_t flush = tailLen - bndNlLen - 4;
                        if (!_otaChunk(tailBuf, flush, false)) { ok = false; st = ST_DONE; break; }
                        memmove(tailBuf, tailBuf + flush, tailLen - flush);
                        tailLen -= flush;
                    }
                }
            }
        }
    }

    if (st != ST_DONE && started) {
        if (tailLen > 0) _otaChunk(tailBuf, tailLen, true);
        else _otaChunk(nullptr, 0, true);
    }

    free(rbuf);
    free(tailBuf);

    if (started && ok) {
        char d[140];
        snprintf(d, sizeof(d), "%s (%u bytes) — reboot", filename.c_str(), (unsigned)totalContent);
        logEvent("OTA", d);
        sendJson(req, 200, "{\"ok\":true,\"reboot\":true}");
    } else {
        logEvent("OTA", "fail");
        sendJson(req, 500, "{\"ok\":false,\"error\":\"ota_failed\"}");
    }
    return ESP_OK;
}
