#include "ConfigManager.h"

bool ConfigManager::load(DeviceConfig& config, const char* path) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.printf("ConfigManager: Failed to open %s\n", path);
        loadDefaults(config);
        return false;
    }

    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("ConfigManager: JSON parse error: %s\n", error.f_str());
        loadDefaults(config);
        return false;
    }

    config.deviceId = doc["device_id"] | "node_1";
    config.deviceName = doc["device_name"] | "새기기";
    config.product = doc["product"] | "RemoteDeck_PC";

    // Network
    JsonObject net = doc["network"];
    config.network.mode = net["mode"] | "ethernet";
    config.network.ethDhcp = net["ethernet"]["dhcp"] | false;
    config.network.ethIp = net["ethernet"]["ip"] | "192.168.1.200";
    config.network.ethGateway = net["ethernet"]["gateway"] | "192.168.1.1";
    config.network.ethSubnet = net["ethernet"]["subnet"] | "255.255.255.0";
    config.network.ethDns1 = net["ethernet"]["dns1"] | "";
    config.network.ethMac = net["ethernet"]["mac"] | "";
    config.network.wifiSsid = net["wifi"]["ssid"] | "";
    config.network.wifiPassword = net["wifi"]["password"] | "";
    config.network.wifiDhcp = net["wifi"]["dhcp"] | true;
    config.network.wifiIp = net["wifi"]["ip"] | "";
    config.network.wifiGateway = net["wifi"]["gateway"] | "";
    config.network.wifiSubnet = net["wifi"]["subnet"] | "255.255.255.0";
    config.network.wifiDns1 = net["wifi"]["dns1"] | "";
    config.network.wifiMac = net["wifi"]["mac"] | "";

    // MQTT
    JsonObject mq = doc["mqtt"];
    config.mqtt.broker = mq["broker"] | "";
    config.mqtt.port = mq["port"] | 1883;
    config.mqtt.user = mq["user"] | "";
    config.mqtt.password = mq["password"] | "";
    config.mqtt.keepalive = mq["keepalive"] | 120;
    config.mqtt.topicPub = mq["topic_pub"] | "RemoteDeck/server";
    config.mqtt.topicSub = mq["topic_sub"] | "RemoteDeck/[device_id]";
    config.mqtt.topicPing = mq["topic_ping"] | "RemoteDeck/ping";

    // Relay
    config.relay.pulseShortMs = doc["relay"]["pulse_short_ms"] | 500;
    config.relay.pulseLongMs = doc["relay"]["pulse_long_ms"] | 5000;

    // Monitor
    config.monitor.pcledPollMs = doc["monitor"]["pcled_poll_interval_ms"] | 1000;
    config.monitor.autoNotify = doc["monitor"]["auto_notify"] | true;

    // WOL
    config.wol.targetMac = doc["wol"]["target_mac"] | "";

    // NTP
    config.ntp.server = doc["ntp"]["server"] | "pool.ntp.org";
    config.ntp.timezone = doc["ntp"]["timezone"] | "KST-9";

    // Firmware
    config.firmware.version = doc["firmware"]["version"] | "2.4.7";
    config.firmware.date = doc["firmware"]["date"] | "2026-07-01";

    // Auth
    config.auth.user = doc["auth"]["user"] | "admin";
    config.auth.pass = doc["auth"]["pass"] | "12345";

    // Web Request
    JsonObject wr = doc["web_request"];
    config.webRequest.enabled = wr["enabled"] | false;
    config.webRequest.timeoutMs = wr["timeout_ms"] | 5000;
    config.webRequest.relay1_on = wr["relay1_on"] | "";
    config.webRequest.relay1_off = wr["relay1_off"] | "";
    config.webRequest.relay2_on = wr["relay2_on"] | "";
    config.webRequest.relay2_off = wr["relay2_off"] | "";
    config.webRequest.pcled_on = wr["pcled_on"] | "";
    config.webRequest.pcled_off = wr["pcled_off"] | "";
    config.webRequest.gpio1_high = wr["gpio1_high"] | "";
    config.webRequest.gpio1_low = wr["gpio1_low"] | "";
    config.webRequest.gpio2_high = wr["gpio2_high"] | "";
    config.webRequest.gpio2_low = wr["gpio2_low"] | "";
    config.webRequest.gpio3_high = wr["gpio3_high"] | "";
    config.webRequest.gpio3_low = wr["gpio3_low"] | "";

    Serial.printf("ConfigManager: Loaded config for %s\n", config.deviceId.c_str());
    return true;
}

bool ConfigManager::save(const DeviceConfig& config, const char* path) {
    StaticJsonDocument<4096> doc;

    doc["device_id"] = config.deviceId;
    doc["device_name"] = config.deviceName;
    doc["product"] = config.product;

    // Network
    JsonObject net = doc.createNestedObject("network");
    net["mode"] = config.network.mode;
    JsonObject eth = net.createNestedObject("ethernet");
    eth["dhcp"] = config.network.ethDhcp;
    eth["ip"] = config.network.ethIp;
    eth["gateway"] = config.network.ethGateway;
    eth["subnet"] = config.network.ethSubnet;
    eth["dns1"] = config.network.ethDns1;
    eth["mac"] = config.network.ethMac;
    JsonObject wifi = net.createNestedObject("wifi");
    wifi["ssid"] = config.network.wifiSsid;
    wifi["password"] = config.network.wifiPassword;
    wifi["dhcp"] = config.network.wifiDhcp;
    wifi["ip"] = config.network.wifiIp;
    wifi["gateway"] = config.network.wifiGateway;
    wifi["subnet"] = config.network.wifiSubnet;
    wifi["dns1"] = config.network.wifiDns1;
    wifi["mac"] = config.network.wifiMac;

    // MQTT
    JsonObject mq = doc.createNestedObject("mqtt");
    mq["broker"] = config.mqtt.broker;
    mq["port"] = config.mqtt.port;
    mq["user"] = config.mqtt.user;
    mq["password"] = config.mqtt.password;
    mq["keepalive"] = config.mqtt.keepalive;
    mq["topic_pub"] = config.mqtt.topicPub;
    mq["topic_sub"] = config.mqtt.topicSub;
    mq["topic_ping"] = config.mqtt.topicPing;

    // Relay
    JsonObject rl = doc.createNestedObject("relay");
    rl["pulse_short_ms"] = config.relay.pulseShortMs;
    rl["pulse_long_ms"] = config.relay.pulseLongMs;

    // Monitor
    JsonObject mon = doc.createNestedObject("monitor");
    mon["pcled_poll_interval_ms"] = config.monitor.pcledPollMs;
    mon["auto_notify"] = config.monitor.autoNotify;

    // WOL
    JsonObject wol = doc.createNestedObject("wol");
    wol["target_mac"] = config.wol.targetMac;

    // NTP
    JsonObject ntp = doc.createNestedObject("ntp");
    ntp["server"] = config.ntp.server;
    ntp["timezone"] = config.ntp.timezone;

    // Firmware
    JsonObject fw = doc.createNestedObject("firmware");
    fw["version"] = config.firmware.version;
    fw["date"] = config.firmware.date;

    // Auth
    JsonObject auth = doc.createNestedObject("auth");
    auth["user"] = config.auth.user;
    auth["pass"] = config.auth.pass;

    // Web Request
    JsonObject wr = doc.createNestedObject("web_request");
    wr["enabled"] = config.webRequest.enabled;
    wr["timeout_ms"] = config.webRequest.timeoutMs;
    wr["relay1_on"] = config.webRequest.relay1_on;
    wr["relay1_off"] = config.webRequest.relay1_off;
    wr["relay2_on"] = config.webRequest.relay2_on;
    wr["relay2_off"] = config.webRequest.relay2_off;
    wr["pcled_on"] = config.webRequest.pcled_on;
    wr["pcled_off"] = config.webRequest.pcled_off;
    wr["gpio1_high"] = config.webRequest.gpio1_high;
    wr["gpio1_low"] = config.webRequest.gpio1_low;
    wr["gpio2_high"] = config.webRequest.gpio2_high;
    wr["gpio2_low"] = config.webRequest.gpio2_low;
    wr["gpio3_high"] = config.webRequest.gpio3_high;
    wr["gpio3_low"] = config.webRequest.gpio3_low;

    File file = SPIFFS.open(path, "w");
    if (!file) {
        Serial.printf("ConfigManager: Failed to open %s for writing\n", path);
        return false;
    }

    serializeJson(doc, file);
    file.close();

    Serial.printf("ConfigManager: Saved config for %s\n", config.deviceId.c_str());
    return true;
}

void ConfigManager::loadDefaults(DeviceConfig& config) {
    config.deviceId = "node_1";
    config.deviceName = "새기기";
    config.product = "RemoteDeck_PC";
    config.network.mode = "ethernet";
    config.network.ethDhcp = false;
    config.network.ethIp = "192.168.1.200";
    config.network.ethGateway = "192.168.1.1";
    config.network.ethSubnet = "255.255.255.0";
    config.network.ethDns1 = "";
    config.network.ethMac = "";
    config.network.wifiSsid = "";
    config.network.wifiPassword = "";
    config.network.wifiDhcp = true;
    config.network.wifiIp = "";
    config.network.wifiGateway = "";
    config.network.wifiSubnet = "255.255.255.0";
    config.network.wifiDns1 = "";
    config.network.wifiMac = "";
    config.mqtt.broker = "";
    config.mqtt.port = 1883;
    config.mqtt.user = "";
    config.mqtt.password = "";
    config.mqtt.keepalive = 120;
    config.mqtt.topicPub = "RemoteDeck/server";
    config.mqtt.topicSub = "RemoteDeck/[device_id]";
    config.mqtt.topicPing = "RemoteDeck/ping";
    config.relay.pulseShortMs = 500;
    config.relay.pulseLongMs = 5000;
    config.monitor.pcledPollMs = 1000;
    config.monitor.autoNotify = true;
    config.wol.targetMac = "";
    config.ntp.server = "pool.ntp.org";
    config.ntp.timezone = "KST-9";
    config.firmware.version = "2.4.7";
    config.firmware.date = "2026-07-01";
    config.auth.user = "admin";
    config.auth.pass = "12345";
    // Web Request defaults (disabled)
    config.webRequest = WebRequestConfig();
}
