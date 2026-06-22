#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

class FileUtils {
public:  
    static void createDir(const char* path); 
    static void list(const char* filePath);
    static bool remove(const char* filePath);
    static bool copy(const char* filePath, const char* targetPath);
    static bool move(const char* filePath, const char* targetPath);
    static void urlToDomain(const char* url, String& domain, int& port);
};
