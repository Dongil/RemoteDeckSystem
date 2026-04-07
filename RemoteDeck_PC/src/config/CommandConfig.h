#pragma once
#include <string>

class CommandConfig {
public:
    std::string command;
    int sequence;
    int data;
    std::string message;

    CommandConfig()
        : command(""), sequence(0), data(0), message("") {}

    CommandConfig(const std::string& cmd, int seq, int d, const std::string& msg)
        : command(cmd), sequence(seq), data(d), message(msg) {}

    CommandConfig(const char* cmd, int seq, int d)
        : command(cmd), sequence(seq), data(d), message("") {}
};
