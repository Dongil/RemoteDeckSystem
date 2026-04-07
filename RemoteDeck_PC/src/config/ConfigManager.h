#pragma once
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "DeviceConfig.h"

class ConfigManager {
public:
    static bool load(DeviceConfig& config, const char* path = CONFIG_PATH);
    static bool save(const DeviceConfig& config, const char* path = CONFIG_PATH);
    static void loadDefaults(DeviceConfig& config);
};
