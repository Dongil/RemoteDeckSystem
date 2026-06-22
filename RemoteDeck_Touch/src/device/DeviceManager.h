#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <WiFi.h>
// EthernetClient → WiFiClient (lwIP 통합 후 ETH/WiFi 공용)
#include <HTTPClient.h>
#include <lvgl.h>

#include "ui.h"
#include "config/DeviceConfig.h"
#include "config/ServerConfig.h"
#include "config/ImagesConfig.h"
#include "config/ConfigManager.h"
#include "utils/FileUtils.h"

class DeviceManager {
public:
    DeviceManager(DeviceConfig& device, ServerConfig& server, ImagesConfig& images);
    
    void startWifiCheckTimer();
    void stopWifiCheckTimer();
    void showDeviceSet();
    void updateSSIDList();    
    void connectToWiFi();
    void connectToEthernet();
    void saveToDevice();

private:
    DeviceConfig& deviceConfig;
    ServerConfig& serverConfig;
    ImagesConfig& imagesConfig;

    lv_timer_t* wifi_check_timer;
    
    void wifiCheckCallback(lv_timer_t* timer);      
    static void showConnectionFailedMsgBox();
    static void msgBoxEventHandler(lv_event_t* e);    
    void updateEthernetInfo();    
    void updateDeviceInfo();
};

#endif  //DEVICE_MANAGER_H