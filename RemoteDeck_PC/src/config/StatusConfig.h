#pragma once
#include <vector>
#include <string>

// status item 설정 구조체
struct StatusSetting {
    std::string type;
    int sequence;
    int data;  

    // 기본 생성자
    StatusSetting()
        : type(""), sequence(0), data(0) {}

    // 필드 초기화 생성자
    StatusSetting(const std::string& t, int seq, int d)
        : type(t), sequence(seq), data(d) {}  
};

// status 클래스
class StatusConfig {
public:
    std::string device_id;
    std::vector<StatusSetting> status;
    std::string message;

    // 기본 생성자
    StatusConfig()
        : device_id(""), message("") 
    {
        status.emplace_back("", 0, 0);
    }

    // 필드 초기화 생성자
    StatusConfig(const std::string& id, 
                 const std::string& t,
                 int seq,
                 int d,                 
                 const std::string& msg)
        : device_id(id), message(msg) 
    {
        status.emplace_back(t, seq, d);
    }

    // ───────────────────────────────────────────────
    // (d) 새로 추가: 단일 StatusSetting을 받는 생성자
    // ───────────────────────────────────────────────
    StatusConfig(const std::string& id,
                 const StatusSetting& st,
                 const std::string& msg)
        : device_id(id), message(msg)
    {
        // status 벡터에 단일 항목 추가
        status.emplace_back(st);
    }

    // ───────────────────────────────────────────────
    // (e) status에 새 항목을 추가하는 함수
    // ───────────────────────────────────────────────
    void addStatus(const StatusSetting& newone) {
        status.emplace_back(newone);
    }

    void addStatus( const std::string& t, int seq, int d) {
        status.emplace_back(t, seq, d);
    }
};

/*
{
    "device_id": "node_1",
    "status": [
        {
            "type": "RELAY",
            "sequence" : 1,
            "data" : 1
        },
        {
            "type": "RELAY",
            "sequence" : 2,
            "data" : 1
        }, 
        {
            "type": "GPIO",
            "sequence" : 1, 
            "data" : 1
        }     
    ],
    "message": "null"
}
*/