#pragma once
#include <ArduinoJson.h>
#include "config/StatusConfig.h"
#include "config/CommandConfig.h"

class JsonUtils {
public:
    static bool serializeStatusConfig(const StatusConfig& config, String& output);
    static bool deserializeStatusConfig(StatusConfig& config, const String& json);
    static bool serializeCommandConfig(const CommandConfig& config, String& output);
    static bool deserializeCommandConfig(CommandConfig& config, const String& json);
};
