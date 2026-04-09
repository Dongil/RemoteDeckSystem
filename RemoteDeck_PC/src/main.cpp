#include <Arduino.h>
#include <SPIFFS.h>

#include "config/PinConfig.h"
#include "config/DeviceConfig.h"
#include "config/ConfigManager.h"
#include "config/CommandConfig.h"
#include "config/StatusConfig.h"

#include "control/RelayController.h"
#include "control/PCMonitor.h"
#include "control/ScheduleManager.h"
#include "control/WOLSender.h"

#include "network/NetManager.h"
#include "network/MQTTHandler.h"
#include "network/UDPDiscovery.h"
#include "network/NTPSync.h"

#include "web/WebServer.h"

#include "serial/RS485Handler.h"

#include "utils/JsonUtils.h"
#include "utils/Logger.h"

// ─── Global instances ───────────────────────────────
DeviceConfig config;
RelayController relayController;
PCMonitor pcMonitor;
ScheduleManager scheduleManager;
WOLSender wolSender;
NetManager networkManager;
MQTTHandler mqttHandler;
UDPDiscovery udpDiscovery;
NTPSync ntpSync;
WebServer webServer;
RS485Handler rs485Handler;
Logger logger;

// ─── STATUS2 network activity blink ─────────────────
unsigned long status2OffTime = 0;

void blinkStatus2() {
    digitalWrite(PIN_STATUS2, HIGH);
    status2OffTime = millis() + 3000;
}

// ─── Forward declarations ───────────────────────────
void processCommand(const CommandConfig& cmd, const char* source);
StatusSetting getStatus(const std::string& type, int sequence);
void sendStatus(const StatusConfig& status);
void sendFullStatus();
String buildStatusJson();
String buildConfigJson();

// ─── Callbacks ──────────────────────────────────────

void onRS485Command(const char* json) {
    blinkStatus2();
    CommandConfig cmd;
    JsonUtils::deserializeCommandConfig(cmd, String(json));
    processCommand(cmd, "RS485");
}

void onMQTTCommand(const char* payload, unsigned int length) {
    blinkStatus2();
    CommandConfig cmd;
    JsonUtils::deserializeCommandConfig(cmd, String(payload));
    processCommand(cmd, "MQTT");
}

void onRelayChange(uint8_t relay, bool state) {
    logger.log("RELAY", (String("Relay") + relay + (state ? " ON" : " OFF")).c_str());
    StatusConfig status(config.deviceId, "RELAY", relay, state ? 1 : 0, "null");
    sendStatus(status);
    webServer.ws().broadcastStatus(buildStatusJson().c_str());
}

void onPCStateChange(bool pcOn) {
    logger.log("PCLED", pcOn ? "PC ON" : "PC OFF");
    StatusConfig status(config.deviceId, "PCLED", 1, pcOn ? 1 : 0, "null");
    sendStatus(status);
    webServer.ws().broadcastStatus(buildStatusJson().c_str());
}

void onScheduleAction(uint8_t relay, const std::string& action) {
    logger.log("SCHEDULE", (String("Relay") + relay + " " + action.c_str()).c_str());
    if (action == "on") {
        relayController.pulse(relay, config.relay.pulseShortMs);
    } else if (action == "off") {
        relayController.pulse(relay, config.relay.pulseLongMs);
    } else if (action == "toggle") {
        relayController.pulse(relay, config.relay.pulseShortMs);
    }
}

bool onUDPConfig(const char* jsonConfig) {
    // Merge incoming config into current config and save
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, jsonConfig)) return false;

    // Update fields that are present
    if (doc.containsKey("device_id")) config.deviceId = doc["device_id"].as<std::string>();
    if (doc.containsKey("device_name")) config.deviceName = doc["device_name"].as<std::string>();
    if (doc.containsKey("network")) {
        JsonObject net = doc["network"];
        if (net.containsKey("mode")) config.network.mode = net["mode"].as<std::string>();
        if (net.containsKey("ethernet")) {
            config.network.ethDhcp = net["ethernet"]["dhcp"] | config.network.ethDhcp;
            if (net["ethernet"].containsKey("ip")) config.network.ethIp = net["ethernet"]["ip"].as<std::string>();
            if (net["ethernet"].containsKey("gateway")) config.network.ethGateway = net["ethernet"]["gateway"].as<std::string>();
            if (net["ethernet"].containsKey("subnet")) config.network.ethSubnet = net["ethernet"]["subnet"].as<std::string>();
            if (net["ethernet"].containsKey("dns1")) config.network.ethDns1 = net["ethernet"]["dns1"].as<std::string>();
        }
        if (net.containsKey("wifi")) {
            if (net["wifi"].containsKey("ssid")) config.network.wifiSsid = net["wifi"]["ssid"].as<std::string>();
            if (net["wifi"].containsKey("password")) config.network.wifiPassword = net["wifi"]["password"].as<std::string>();
            config.network.wifiDhcp = net["wifi"]["dhcp"] | config.network.wifiDhcp;
            if (net["wifi"].containsKey("ip")) config.network.wifiIp = net["wifi"]["ip"].as<std::string>();
            if (net["wifi"].containsKey("gateway")) config.network.wifiGateway = net["wifi"]["gateway"].as<std::string>();
            if (net["wifi"].containsKey("subnet")) config.network.wifiSubnet = net["wifi"]["subnet"].as<std::string>();
            if (net["wifi"].containsKey("dns1")) config.network.wifiDns1 = net["wifi"]["dns1"].as<std::string>();
        }
    }
    if (doc.containsKey("mqtt")) {
        JsonObject mq = doc["mqtt"];
        if (mq.containsKey("broker")) config.mqtt.broker = mq["broker"].as<std::string>();
        if (mq.containsKey("port")) config.mqtt.port = mq["port"];
        if (mq.containsKey("user")) config.mqtt.user = mq["user"].as<std::string>();
        if (mq.containsKey("password")) config.mqtt.password = mq["password"].as<std::string>();
    }

    return ConfigManager::save(config);
}

void onUDPReboot() {
    logger.log("REBOOT", "UDP Discovery");
    delay(1000);
    ESP.restart();
}

// ─── Command processor ─────────────────────────────

void processCommand(const CommandConfig& cmd, const char* source) {
    logger.log("CMD", (cmd.command + " seq=" + String(cmd.sequence).c_str() +
                       " from " + source).c_str());

    if (cmd.command == "RELAY") {
        relayController.setRelay(cmd.sequence, cmd.data == 1);

    } else if (cmd.command == "PULSE") {
        uint16_t duration = cmd.data > 0 ? cmd.data : config.relay.pulseShortMs;
        relayController.pulse(cmd.sequence, duration);

    } else if (cmd.command == "PCLED") {
        StatusConfig status(config.deviceId, "PCLED", 1,
                            pcMonitor.isPCOn() ? 1 : 0, "null");
        sendStatus(status);

    } else if (cmd.command == "GETGPIO") {
        StatusConfig status(config.deviceId, getStatus("GPIO", 1), "null");
        status.addStatus(getStatus("GPIO", 2));
        status.addStatus(getStatus("GPIO", 3));
        sendStatus(status);

    } else if (cmd.command == "GETSTATUS") {
        sendFullStatus();

    } else if (cmd.command == "WOL") {
        if (cmd.message != "null" && !cmd.message.empty()) {
            wolSender.send(cmd.message.c_str());
        } else {
            wolSender.sendDefault();
        }

    } else if (cmd.command == "WIFION" || cmd.command == "WIFIOFF") {
        // v2.1: network mode is fixed by config, report current state
        bool connected = networkManager.isConnected();
        StatusConfig status(config.deviceId, connected ? "WIFION" : "WIFIOFF", 1,
                            connected ? 1 : 0, networkManager.localIP().toString().c_str());
        sendStatus(status);

    } else if (cmd.command == "REBOOT") {
        logger.log("REBOOT", source);
        delay(1000);
        ESP.restart();
    }
}

// ─── Status helpers ─────────────────────────────────

StatusSetting getStatus(const std::string& type, int sequence) {
    StatusSetting s;
    s.type = type;
    s.sequence = sequence;

    if (type == "RELAY") {
        s.data = relayController.getRelay(sequence) ? 1 : 0;
    } else if (type == "GPIO") {
        uint8_t pin = (sequence == 1) ? PIN_GPIO1 : (sequence == 2) ? PIN_GPIO2 : PIN_GPIO3;
        s.data = digitalRead(pin);
    } else if (type == "PCLED") {
        s.data = pcMonitor.isPCOn() ? 1 : 0;
    } else if (type == "ONLINE") {
        s.data = 1;
    }
    return s;
}

void sendStatus(const StatusConfig& status) {
    blinkStatus2();

    // MQTT
    mqttHandler.publishStatus(status);

    // RS485
    String json;
    if (JsonUtils::serializeStatusConfig(status, json)) {
        rs485Handler.send(json.c_str());
    }
}

void sendFullStatus() {
    // Part 1: ONLINE + PCLED
    StatusConfig s1(config.deviceId, getStatus("ONLINE", 1),
                    networkManager.localIP().toString().c_str());
    s1.addStatus(getStatus("PCLED", 1));
    sendStatus(s1);

    // Part 2: RELAYs
    StatusConfig s2(config.deviceId, getStatus("RELAY", 1), "null");
    s2.addStatus(getStatus("RELAY", 2));
    sendStatus(s2);

    // Part 3: GPIOs
    StatusConfig s3(config.deviceId, getStatus("GPIO", 1), "null");
    s3.addStatus(getStatus("GPIO", 2));
    s3.addStatus(getStatus("GPIO", 3));
    sendStatus(s3);
}

String buildStatusJson() {
    StaticJsonDocument<512> doc;
    doc["pc_on"] = pcMonitor.isPCOn();
    doc["relay1"] = relayController.getRelay(1);
    doc["relay2"] = relayController.getRelay(2);
    JsonArray gpio = doc.createNestedArray("gpio");
    gpio.add(digitalRead(PIN_GPIO1));
    gpio.add(digitalRead(PIN_GPIO2));
    gpio.add(digitalRead(PIN_GPIO3));
    doc["uptime"] = ntpSync.getUptime();
    doc["ip"] = networkManager.localIP().toString();
    doc["mac"] = networkManager.macAddress();
    doc["net_mode"] = networkManager.activeMode() == NetMode::ETHERNET ? "ethernet" : "wifi";
    doc["fw_ver"] = config.firmware.version;
    doc["device_name"] = config.deviceName;
    doc["ntp_synced"] = ntpSync.isSynced();
    doc["time"] = ntpSync.getTimeString();
    doc["mqtt_connected"] = mqttHandler.isConnected();

    String output;
    serializeJson(doc, output);
    return output;
}

String buildConfigJson() {
    StaticJsonDocument<2048> doc;
    doc["device_id"] = config.deviceId;
    doc["device_name"] = config.deviceName;
    doc["product"] = config.product;

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
    wifi["dhcp"] = config.network.wifiDhcp;
    wifi["ip"] = config.network.wifiIp;
    wifi["gateway"] = config.network.wifiGateway;
    wifi["subnet"] = config.network.wifiSubnet;
    wifi["dns1"] = config.network.wifiDns1;
    wifi["mac"] = config.network.wifiMac;
    wifi["password"] = config.network.wifiPassword;  // IPSetupTool needs this

    JsonObject mq = doc.createNestedObject("mqtt");
    mq["broker"] = config.mqtt.broker;
    mq["port"] = config.mqtt.port;
    mq["user"] = config.mqtt.user;
    mq["keepalive"] = config.mqtt.keepalive;
    mq["topic_pub"] = config.mqtt.topicPub;
    mq["topic_sub"] = config.mqtt.topicSub;

    JsonObject rl = doc.createNestedObject("relay");
    rl["pulse_short_ms"] = config.relay.pulseShortMs;
    rl["pulse_long_ms"] = config.relay.pulseLongMs;

    JsonObject mon = doc.createNestedObject("monitor");
    mon["pcled_poll_interval_ms"] = config.monitor.pcledPollMs;
    mon["auto_notify"] = config.monitor.autoNotify;

    JsonObject wolObj = doc.createNestedObject("wol");
    wolObj["target_mac"] = config.wol.targetMac;

    JsonObject ntp = doc.createNestedObject("ntp");
    ntp["server"] = config.ntp.server;
    ntp["timezone"] = config.ntp.timezone;

    JsonObject fw = doc.createNestedObject("firmware");
    fw["version"] = config.firmware.version;
    fw["date"] = config.firmware.date;

    // Auth (user only, password masked)
    doc["auth_user"] = config.auth.user;

    String output;
    serializeJson(doc, output);
    return output;
}

// ─── Setup ──────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Load config
    ConfigManager::load(config);

    // Init STATUS LEDs early (needed for factory reset blink)
    pinMode(PIN_STATUS1, OUTPUT);
    pinMode(PIN_STATUS2, OUTPUT);
    digitalWrite(PIN_STATUS1, LOW);
    digitalWrite(PIN_STATUS2, LOW);

    // ── Factory Reset Check ──
    // Short GPIO1 (pin 12) to GND during boot for 3 seconds = reset to factory defaults
    pinMode(PIN_GPIO1, INPUT_PULLUP);
    delay(100);  // Let pullup stabilize
    if (digitalRead(PIN_GPIO1) == LOW) {
        Serial.println("*** GPIO1 LOW detected - Hold 3 seconds for factory reset ***");
        Serial.println("*** STATUS1 LED blinking... Release to cancel ***");
        unsigned long holdStart = millis();
        while (digitalRead(PIN_GPIO1) == LOW && millis() - holdStart < 3000) {
            digitalWrite(PIN_STATUS1, (millis() / 200) % 2 ? HIGH : LOW);
            delay(50);
        }
        digitalWrite(PIN_STATUS1, LOW);

        if (millis() - holdStart >= 3000) {
            Serial.println("*** FACTORY RESET - Restoring default config ***");
            ConfigManager::loadDefaults(config);
            ConfigManager::save(config);
            Serial.println("*** Config reset to factory defaults (IP: 192.168.1.200) ***");
            // STATUS1 solid ON for 2 seconds to indicate success
            digitalWrite(PIN_STATUS1, HIGH);
            Serial.println("*** Remove the jumper wire now! Rebooting in 5 seconds... ***");
            delay(5000);
            digitalWrite(PIN_STATUS1, LOW);
            ESP.restart();
        } else {
            Serial.println("*** Factory reset cancelled (released before 3 seconds) ***");
        }
    }

    // GPIO init
    relayController.begin(PIN_RELAY1, PIN_RELAY2);
    relayController.setPulseConfig(config.relay.pulseShortMs, config.relay.pulseLongMs);

    pcMonitor.begin(PIN_PCLED);
    pcMonitor.setPollInterval(config.monitor.pcledPollMs);
    pcMonitor.setAutoNotify(config.monitor.autoNotify);

    pinMode(PIN_GPIO1, INPUT);
    pinMode(PIN_GPIO2, INPUT);
    pinMode(PIN_GPIO3, INPUT);
    // STATUS1/2 already initialized above (before factory reset check)

    // Network v2.1: single mode (ethernet OR wifi STA), non-blocking
    networkManager.setOnConnected([]() {
        Serial.println("=== Network connected - starting services ===");

        // MQTT
        if (!config.mqtt.broker.empty()) {
            mqttHandler.begin(config.mqtt, config.deviceId, networkManager.getNetClient());
        }

        // NTP
        ntpSync.begin(config.ntp.server.c_str(), config.ntp.timezone.c_str());

        // ONLINE status via MQTT
        StatusConfig online(config.deviceId, "ONLINE", 1, 1,
                            networkManager.localIP().toString().c_str());
        mqttHandler.publishStatus(online);

        logger.log("NETWORK", networkManager.activeMode() == NetMode::ETHERNET ?
                   "Ethernet connected" : "WiFi connected");
    });
    networkManager.begin(config.network);

    // Logger NTP time getter
    logger.setNTPTimeGetter([]() -> String { return String(ntpSync.getTimeString().c_str()); });

    // UDP Discovery (with auth)
    udpDiscovery.begin(UDP_DISC_PORT, &config);
    udpDiscovery.setAuth(&config.auth);
    udpDiscovery.setOnConfigRequest(onUDPConfig);
    udpDiscovery.setOnRebootRequest(onUDPReboot);
    udpDiscovery.setOnGetConfig(buildConfigJson);
    udpDiscovery.setOnChangeAuth([](const char* newUser, const char* newPass) {
        config.auth.user = newUser;
        config.auth.pass = newPass;
        return ConfigManager::save(config);
    });

    // WOL
    wolSender.setDefaultMac(config.wol.targetMac);

    // Schedule
    scheduleManager.begin(SCHEDULE_PATH);
    scheduleManager.setOnAction(onScheduleAction);

    // RS485
    rs485Handler.begin(PIN_RS485_RX, PIN_RS485_TX, RS485_BAUD);
    rs485Handler.setOnCommand(onRS485Command);

    // MQTT callback
    mqttHandler.setOnCommand(onMQTTCommand);

    // Relay/PC callbacks
    relayController.setOnChange(onRelayChange);
    pcMonitor.setOnChange(onPCStateChange);

    // Web Server
    webServer.setStatusGetter(buildStatusJson);
    webServer.setRelayHandler([](uint8_t relay, const String& action, uint16_t duration) {
        if (action == "on") relayController.setRelay(relay, true);
        else if (action == "off") relayController.setRelay(relay, false);
        else if (action == "pulse") relayController.pulse(relay, duration);
    });
    webServer.setConfigGetter(buildConfigJson);
    webServer.setConfigSetter([](const String& json) {
        return onUDPConfig(json.c_str());
    });
    webServer.setScheduleGetter([]() { return scheduleManager.toJson(); });
    webServer.setScheduleSetter([](const String& json) {
        Schedule s;
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, json)) return false;
        s.id = doc["id"] | 0;
        s.enabled = doc["enabled"] | true;
        s.hour = doc["hour"] | 0;
        s.minute = doc["minute"] | 0;
        s.days = doc["days"] | 0;
        s.action = doc["action"] | "on";
        s.relay = doc["relay"] | 1;

        if (s.id == 0) return scheduleManager.addSchedule(s);
        return scheduleManager.updateSchedule(s);
    });
    webServer.setScheduleDeleter([](uint8_t id) {
        return scheduleManager.deleteSchedule(id);
    });
    webServer.setWOLHandler([](const String& mac) {
        return wolSender.send(mac.c_str());
    });
    webServer.setRebootHandler([]() {
        delay(1000);
        ESP.restart();
    });
    webServer.setLogGetter([]() { return logger.toJson(); });
    webServer.setAuthChanger([](const String& curPass, const String& newUser, const String& newPass) {
        if (curPass != String(config.auth.pass.c_str())) return false;
        config.auth.user = newUser.c_str();
        config.auth.pass = newPass.c_str();
        return ConfigManager::save(config);
    });
    webServer.ota().setOnProgress([](uint8_t pct) {
        webServer.ws().broadcastOTAProgress(pct);
    });
    webServer.begin(WEB_PORT, &config.auth);

    // Send ONLINE status via RS485
    StatusConfig onlineStatus(config.deviceId, "ONLINE", 0, 0, "null");
    String json;
    if (JsonUtils::serializeStatusConfig(onlineStatus, json)) {
        rs485Handler.send(json.c_str());
    }

    if (networkManager.isConnected()) {
        digitalWrite(PIN_STATUS1, HIGH);
    }

    logger.log("SYSTEM", "Boot complete");
    Serial.println("=== RemoteDeck PC Power Manager v2.1 ===");
    Serial.printf("    Mode: %s\n", config.network.mode.c_str());
    if (networkManager.isConnected()) {
        Serial.printf("    Primary IP: %s\n", networkManager.localIP().toString().c_str());
        Serial.printf("    Web UI: http://%s:%d\n",
                      networkManager.localIP().toString().c_str(), WEB_PORT);
    } else {
        Serial.println("    Waiting for primary network...");
    }
    if (networkManager.isEthManagementActive()) {
        Serial.printf("    ETH Management: %s (IPSetupTool)\n",
                      networkManager.ethManagementIP().toString().c_str());
    }
}

// ─── Loop ───────────────────────────────────────────

void loop() {
    networkManager.loop();
    mqttHandler.loop();
    rs485Handler.loop();
    relayController.loop();
    pcMonitor.loop();
    scheduleManager.loop();
    udpDiscovery.loop();
    webServer.ws().loop();

    // STATUS2: auto-off after 3 seconds
    if (status2OffTime > 0 && millis() >= status2OffTime) {
        digitalWrite(PIN_STATUS2, LOW);
        status2OffTime = 0;
    }
}
