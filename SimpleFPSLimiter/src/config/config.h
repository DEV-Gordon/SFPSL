#pragma once
#include "pch.h"

class Config {
public:
    float  targetFPS = 60.0f;
    bool   limiterEnabled = true;
    int    toggleKey = VK_INSERT; // key INSERT

    void Load(const std::string& path);
    void Save(const std::string& path);

    static Config& Instance() {
        static Config instance;
        return instance;
    }

private:
    Config() = default;
};