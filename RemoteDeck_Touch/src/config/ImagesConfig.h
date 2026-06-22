#pragma once
#include <vector>
#include <string>

// Buzzer 설정 구조체
struct BuzzerSetting {
    int freq;
    int duration;
    int duty;
};

// Image 설정 구조체
struct ImageSetting {
    std::string url;
    int x;
    int y;
    int z;
    std::string state;
};

// ImagesConfig 설정 클래스
class ImagesConfig {
public:
    std::string version;
    std::vector<BuzzerSetting> buzzerOn;
    std::vector<BuzzerSetting> buzzerOff;
    std::vector<ImageSetting> images;
};