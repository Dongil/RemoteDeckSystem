#include "JsonUtils.h"

bool JsonUtils::serializeStatusConfig(const StatusConfig& config, String& output) {
    StaticJsonDocument<2048> doc;
    doc["device_id"] = config.device_id.c_str();

    JsonArray statusArray = doc.createNestedArray("status");
    for (const auto& item : config.status) {
        JsonObject obj = statusArray.createNestedObject();
        obj["type"] = item.type.c_str();
        obj["sequence"] = item.sequence;
        obj["data"] = item.data;
    }

    doc["message"] = config.message.c_str();
    serializeJson(doc, output);
    return true;
}

bool JsonUtils::deserializeStatusConfig(StatusConfig& config, const String& json) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.printf("JSON parse error (StatusConfig): %s\n", error.f_str());
        return false;
    }

    config.device_id = doc["device_id"].as<std::string>();
    config.status.clear();
    JsonArray statusArray = doc["status"].as<JsonArray>();
    for (JsonVariant v : statusArray) {
        StatusSetting item;
        item.type = v["type"].as<std::string>();
        item.sequence = v["sequence"];
        item.data = v["data"];
        config.status.push_back(item);
    }
    config.message = doc["message"].as<std::string>();
    return true;
}

bool JsonUtils::serializeCommandConfig(const CommandConfig& config, String& output) {
    StaticJsonDocument<512> doc;
    doc["command"] = config.command.c_str();
    doc["sequence"] = config.sequence;
    doc["data"] = config.data;
    doc["message"] = config.message.c_str();
    serializeJson(doc, output);
    return true;
}

bool JsonUtils::deserializeCommandConfig(CommandConfig& config, const String& json) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.printf("JSON parse error (CommandConfig): %s\n", error.f_str());
        return false;
    }

    config.command = doc["command"].as<std::string>();
    config.sequence = doc["sequence"];
    config.data = doc["data"];
    config.message = doc["message"].as<std::string>();
    return true;
}
