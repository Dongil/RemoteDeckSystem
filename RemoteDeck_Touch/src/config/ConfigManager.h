#pragma once

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "DeviceConfig.h"
#include "ServerConfig.h"
#include "ImagesConfig.h"
#include "utils/JsonUtils.h"


class ConfigManager {
public:
    static bool loadDeviceConfig(DeviceConfig& config, const char* filePath);
    static bool saveDeviceConfig(const DeviceConfig& config, const char* filePath);

    static bool loadServerConfig(ServerConfig& config, const char* filePath);
    static bool saveServerConfig(const ServerConfig& config, const char* filePath);
    
    static bool loadImagesConfig(ImagesConfig& config, const char* filePath);
    static bool saveImagesConfig(const ImagesConfig& config, const char* filePath);

    static bool loadDeviceConfig(DeviceConfig& config);
    static bool saveDeviceConfig(const DeviceConfig& config);

    static bool loadServerConfig(ServerConfig& config);
    static bool saveServerConfig(const ServerConfig& config);
    
    static bool loadImagesConfig(ImagesConfig& config);
    static bool saveImagesConfig(const ImagesConfig& config);
};
