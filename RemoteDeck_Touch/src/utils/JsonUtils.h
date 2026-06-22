#pragma once

#include <ArduinoJson.h>
#include "config/DeviceConfig.h"
#include "config/ServerConfig.h"
#include "config/ImagesConfig.h"

class JsonUtils {
public:
    static bool serializeDeviceConfig(const DeviceConfig& config, String& output);
    static bool deserializeDeviceConfig(DeviceConfig& config, const String& json);

    static bool serializeServerConfig(const ServerConfig& config, String& output);
    static bool deserializeServerConfig(ServerConfig& config, const String& json);

    static bool serializeImagesConfig(const ImagesConfig& config, String& output);
    static bool deserializeImagesConfig(ImagesConfig& config, const String& json);
};
