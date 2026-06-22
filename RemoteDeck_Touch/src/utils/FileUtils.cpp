#include "FileUtils.h"

// 경로에서 디렉토리 부분을 파싱하고 디렉토리를 생성하는 함수
void FileUtils::createDir(const char* path) {
    String dirPath = "/";
    String folderPath = "";

    // path의 각 문자를 탐색하면서 디렉토리를 만들어야 할 부분을 확인
    for (size_t i = 0; i < strlen(path); i++) {
        if (path[i] == '/') {
            if (folderPath.length() > 0) {
                dirPath += folderPath + "/";
                // 디렉토리가 존재하는지 확인하고 없으면 생성
                if (!SPIFFS.exists(dirPath)) {
                    SPIFFS.mkdir(dirPath);
                }
                folderPath = "";  // 초기화하여 다음 폴더명 탐색
            }
        } else {
            folderPath += path[i];
        }
    }

    // 마지막 부분이 파일명이 아닌 경우 디렉토리로 간주하여 처리
    if (folderPath.length() > 0 && path[strlen(path) - 1] == '/') {
        dirPath += folderPath + "/";
        if (!SPIFFS.exists(dirPath)) {
            SPIFFS.mkdir(dirPath);
        }
    }
}

// SPIFFS 내 모든 파일과 디렉토리를 재귀적으로 출력하는 함수
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    bool isEmpty = true;
    File file = root.openNextFile();
    while (file) {
        isEmpty = false;  // 디렉토리 안에 파일 또는 하위 디렉토리가 있음을 표시

        if (file.isDirectory()) {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels) {
                listDir(fs, file.name(), levels - 1);  // 하위 디렉토리 탐색
            }
        } else {
            // 파일의 전체 경로와 크기 출력
            String fullPath = String(dirname) + "/" + String(file.name());  // 디렉토리 경로 + 파일명 결합
            Serial.print("  FILE: ");
            Serial.print(fullPath);  // 전체 경로 출력
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }

    // 디렉토리가 비어 있는 경우 빈 디렉토리 표시
    if (isEmpty) {
        Serial.print("  (empty) DIR : ");
        Serial.println(dirname);
    }
}

// 파일 삭제 함수
bool deleteFile(fs::FS &fs, const char * path) {
    if (fs.exists(path)) {
        Serial.printf("Deleting file: %s\n", path);
        fs.remove(path);
    } else {
        Serial.printf("File not found: %s\n", path);
        return false;    
    }
    
    return true;
}

// 파일 복사 함수
bool copyFile(fs::FS &fs, const char * sourcePath, const char * destPath) {
    // 원본 파일 열기
    File sourceFile = fs.open(sourcePath, FILE_READ);
    if (!sourceFile) {
        Serial.printf("Failed to open source file: %s\n", sourcePath);
        return false;
    }

    // 대상 경로의 디렉토리 생성
    FileUtils::createDir(destPath);

    // 대상 파일 열기
    File destFile = fs.open(destPath, FILE_WRITE);
    if (!destFile) {
        Serial.printf("Failed to open destination file: %s\n", destPath);
        sourceFile.close();
        return false;
    }

    // 원본 파일에서 대상 파일로 복사
    size_t bytesCopied = 0;
    uint8_t buf[512];
    while (sourceFile.available()) {
        size_t bytesRead = sourceFile.read(buf, sizeof(buf));
        if (bytesRead > 0) {
            size_t bytesWritten = destFile.write(buf, bytesRead);
            bytesCopied += bytesWritten;
        }
    }

    Serial.printf("Copied %d bytes from %s to %s\n", bytesCopied, sourcePath, destPath);

    sourceFile.close();
    destFile.close();

    return true;
}

void FileUtils::list(const char* filePath)
{
    listDir(SPIFFS, filePath, 0);    
}

bool FileUtils::remove(const char* filePath)
{
    return deleteFile(SPIFFS, filePath);    
}

bool FileUtils::copy(const char* filePath, const char* targetPath)
{
    return copyFile(SPIFFS, filePath, targetPath);    
}

bool FileUtils::move(const char* filePath,const char* targetPath)
{
    copyFile(SPIFFS, filePath, targetPath);
    return deleteFile(SPIFFS, filePath);    
}

void FileUtils::urlToDomain(const char* url, String& domain, int& port)
{
    String fullUrl = url;

    String protocol;
    port = 80;  // 기본 포트는 80으로 설정

    // 프로토콜 추출
    int protocolEnd = fullUrl.indexOf("://");
    if (protocolEnd != -1) {
        protocol = fullUrl.substring(0, protocolEnd);
        fullUrl.remove(0, protocolEnd + 3);  // "://" 이후 부분을 남김
    }

    // 도메인과 포트 추출
    int portIndex = fullUrl.indexOf(':');
    if (portIndex != -1) {
        domain = fullUrl.substring(0, portIndex);   // 도메인 추출
        port = fullUrl.substring(portIndex + 1).toInt();  // 포트 추출
    } else {
        domain = fullUrl;  // 포트가 없으면 전체를 도메인으로 취급
    }

    // 결과 출력
    Serial.print("Protocol: ");
    Serial.println(protocol);
    Serial.print("Domain: ");
    Serial.println(domain);
    Serial.print("Port: ");
    Serial.println(port);
}
