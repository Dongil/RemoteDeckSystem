#include "TypeUtils.h"

std::string TypeUtils::replaceID(const std::string& template_str, const std::string& device_id) {
    std::string result = template_str;
    size_t start_pos = result.find("[device_id]");  // [device_id]의 위치 찾기

    if (start_pos != std::string::npos) {
        // [device_id]를 찾았으면 해당 부분을 device_id로 대체
        result.replace(start_pos, std::string("[device_id]").length(), device_id);
    }
    
    return result;
}

std::string TypeUtils::makeHttpPath(const std::string& template_str, const std::string& device_id, const std::string& status) {
    std::string result = template_str;
    size_t start_pos = result.find("[device_id]");  // [device_id]의 위치 찾기

    if (start_pos != std::string::npos) {
        // [device_id]를 찾았으면 해당 부분을 device_id로 대체
        result.replace(start_pos, std::string("[device_id]").length(), device_id);
    }
    
    start_pos = result.find("[status]");  // [status]의 위치 찾기

    if (start_pos != std::string::npos) {
        // [status]를 찾았으면 해당 부분을 status로 대체
        result.replace(start_pos, std::string("[status]").length(), status);
    }

    return result;
}


// 주소 문자열 파싱 함수
bool TypeUtils::parseAddress(const char* address, String& ip, uint16_t& port) {
    String addrStr = String(address);

    // 기본 포트 초기화 (80)
    port = 80;

    // "http://" 또는 "https://" 제거
    if (addrStr.startsWith("http://")) {
        addrStr = addrStr.substring(7);
    } else if (addrStr.startsWith("https://")) {
        addrStr = addrStr.substring(8);
    }

    // 포트가 포함되어 있는지 확인
    int colonIndex = addrStr.indexOf(':');
    if (colonIndex != -1) {
        // IP/도메인과 포트를 분리
        ip = addrStr.substring(0, colonIndex);
        port = addrStr.substring(colonIndex + 1).toInt();

        // 포트 번호가 유효한지 확인 (1 ~ 65535 범위)
        if (port < 1 || port > 65535) {
            return false; // 유효하지 않은 포트
        }
    } else {
        // 포트가 없는 경우, 전체를 IP/도메인으로 간주
        ip = addrStr;
    }

    return true; // 성공적으로 파싱됨
}