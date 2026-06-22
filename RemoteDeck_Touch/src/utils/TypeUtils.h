#pragma once

#include <ArduinoJson.h>

class TypeUtils {
public:
    static std::string replaceID(const std::string& template_str, const std::string& device_id);
    static std::string makeHttpPath(const std::string& template_str, const std::string& device_id, const std::string& status);
    static bool parseAddress(const char* address, String& ip, uint16_t& port);
};