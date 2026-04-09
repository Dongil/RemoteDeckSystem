#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include "WebSocketHandler.h"
#include "OTAHandler.h"
#include "config/DeviceConfig.h"

class WebServer {
public:
    void begin(uint16_t port, const AuthConfig* auth);

    WebSocketHandler& ws() { return _ws; }

    using StatusGetter = std::function<String()>;
    using RelayHandler = std::function<void(uint8_t relay, const String& action, uint16_t duration)>;
    using ConfigGetter = std::function<String()>;
    using ConfigSetter = std::function<bool(const String& json)>;
    using ScheduleGetter = std::function<String()>;
    using ScheduleSetter = std::function<bool(const String& json)>;
    using ScheduleDeleter = std::function<bool(uint8_t id)>;
    using WOLHandler = std::function<bool(const String& mac)>;
    using RebootHandler = std::function<void()>;
    using LogGetter = std::function<String()>;
    using AuthChanger = std::function<bool(const String& curPass, const String& newUser, const String& newPass)>;

    void setStatusGetter(StatusGetter cb) { _getStatus = cb; }
    void setRelayHandler(RelayHandler cb) { _handleRelay = cb; }
    void setConfigGetter(ConfigGetter cb) { _getConfig = cb; }
    void setConfigSetter(ConfigSetter cb) { _setConfig = cb; }
    void setScheduleGetter(ScheduleGetter cb) { _getSchedule = cb; }
    void setScheduleSetter(ScheduleSetter cb) { _setSchedule = cb; }
    void setScheduleDeleter(ScheduleDeleter cb) { _deleteSchedule = cb; }
    void setWOLHandler(WOLHandler cb) { _handleWOL = cb; }
    void setRebootHandler(RebootHandler cb) { _handleReboot = cb; }
    void setLogGetter(LogGetter cb) { _getLog = cb; }
    void setAuthChanger(AuthChanger cb) { _changeAuth = cb; }

    OTAHandler& ota() { return _ota; }

private:
    AsyncWebServer* _server = nullptr;
    WebSocketHandler _ws;
    OTAHandler _ota;
    const AuthConfig* _auth = nullptr;

    StatusGetter _getStatus = nullptr;
    RelayHandler _handleRelay = nullptr;
    ConfigGetter _getConfig = nullptr;
    ConfigSetter _setConfig = nullptr;
    ScheduleGetter _getSchedule = nullptr;
    ScheduleSetter _setSchedule = nullptr;
    ScheduleDeleter _deleteSchedule = nullptr;
    WOLHandler _handleWOL = nullptr;
    RebootHandler _handleReboot = nullptr;
    LogGetter _getLog = nullptr;
    AuthChanger _changeAuth = nullptr;

    bool requireAuth(AsyncWebServerRequest* req);
    void setupStaticFiles();
    void setupAPI();
};
