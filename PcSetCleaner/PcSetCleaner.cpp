#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <conio.h>  // 用于 _getch()

#define MAX_PATH_LEN 512
#define BUFFER_SIZE 1024
#define INDEX_THRESHOLD 850
#define FILE_NAME "pc_settings.bin"

// 获取用户文档目录
void getUserDocumentsPath(char* path, size_t size) {
    char userProfile[MAX_PATH_LEN];
    if (GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH_LEN) == 0) {
        // 如果获取失败，使用备用方法
        strcpy_s(userProfile, MAX_PATH_LEN, "C:\\Users\\Default");
    }
    sprintf_s(path, size, "%s\\Documents", userProfile);
}

// 读取文件到内存
unsigned char* readFile(const char* filePath, size_t* fileSize) {
    FILE* file;
    if (fopen_s(&file, filePath, "rb") != 0) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(*fileSize);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytesRead = fread(buffer, 1, *fileSize, file);
    fclose(file);

    if (bytesRead != *fileSize) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

// 写入文件
int writeFile(const char* filePath, unsigned char* data, size_t dataSize) {
    FILE* file;
    if (fopen_s(&file, filePath, "wb") != 0) {
        return 0;
    }

    size_t bytesWritten = fwrite(data, 1, dataSize, file);
    fclose(file);

    return bytesWritten == dataSize;
}

// 处理配置文件，删除索引大于850的项
int processConfigFile(const char* filePath) {
    size_t fileSize;
    unsigned char* fileData = readFile(filePath, &fileSize);

    if (fileData == NULL) {
        return 0; // 读取失败
    }

    // 检查文件大小是否为8的倍数
    if (fileSize % 8 != 0) {
        free(fileData);
        return 0; // 文件格式错误
    }

    // 计算项目数量
    size_t entryCount = fileSize / 8;

    // 创建新的缓冲区（最大为原文件大小）
    unsigned char* newBuffer = (unsigned char*)malloc(fileSize);
    if (newBuffer == NULL) {
        free(fileData);
        return 0;
    }

    size_t newSize = 0;
    int entriesKept = 0;
    int entriesRemoved = 0;

    // 遍历所有项目
    for (size_t i = 0; i < entryCount; i++) {
        size_t offset = i * 8;

        // 读取索引（4字节，小端）
        unsigned int index = 0;
        index |= (unsigned int)fileData[offset];
        index |= (unsigned int)fileData[offset + 1] << 8;
        index |= (unsigned int)fileData[offset + 2] << 16;
        index |= (unsigned int)fileData[offset + 3] << 24;

        // 如果索引小于等于850，保留此项目
        if (index < INDEX_THRESHOLD) {
            // 复制8字节到新缓冲区
            memcpy(newBuffer + newSize, fileData + offset, 8);
            newSize += 8;
            entriesKept++;
        }
        else {
            entriesRemoved++;
        }
    }

    // 如果有项目被删除，写回文件
    int result = 1;
    if (entriesRemoved > 0) {
        result = writeFile(filePath, newBuffer, newSize);
    }

    free(fileData);
    free(newBuffer);

    return result;
}

// 处理一个目录下的所有子文件夹
void processDirectory(const char* basePath, const char* dirName) {
    char searchPath[MAX_PATH_LEN];
    sprintf_s(searchPath, MAX_PATH_LEN, "%s\\*", basePath);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("%s....未找到任何账号\n", dirName);
        return;
    }

    int processedCount = 0;
    int successCount = 0;
    int failCount = 0;

    do {
        // 跳过 . 和 ..
        if (strcmp(findData.cFileName, ".") == 0 ||
            strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        // 检查是否为目录
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char subDirPath[MAX_PATH_LEN];
            char filePath[MAX_PATH_LEN];

            sprintf_s(subDirPath, MAX_PATH_LEN, "%s\\%s", basePath, findData.cFileName);
            sprintf_s(filePath, MAX_PATH_LEN, "%s\\%s", subDirPath, FILE_NAME);

            // 检查pc_settings.bin是否存在
            DWORD fileAttrs = GetFileAttributesA(filePath);
            if (fileAttrs != INVALID_FILE_ATTRIBUTES) {
                processedCount++;

                // 处理文件
                int result = processConfigFile(filePath);

                if (result) {
                    printf("%s....%s....清洗成功\n", dirName, findData.cFileName);
                    successCount++;
                }
                else {
                    printf("%s....%s....清洗失败（可能文件被占用）\n", dirName, findData.cFileName);
                    failCount++;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // 如果没有子目录或没有配置文件
    if (processedCount == 0) {
        printf("%s....未找到游戏设置文件\n", dirName);
    }
}

int main() {
    char documentsPath[MAX_PATH_LEN];
    char basePath1[MAX_PATH_LEN];
    char basePath2[MAX_PATH_LEN];

    // 设置控制台标题
    SetConsoleTitleA("PcSetCleaner");

    printf("      PcSetCleaner v1.0\n");
    printf("================================================================================\n");
    printf("   由MageAngela制作的一键清洗所有pc_settings.bin文件的小工具。\n");
    printf("   使用纯C++语言实现，无需运行库，体积小巧运行速度快，完全开源。\n");
    printf("   采用Unlicense license，任何人都可以使用和修改，无需任何条件。\n");
    printf("   如需在其他软件调用，可在GitHub下载静默参数版本，无需确认，体积更小巧。\n");
    printf("   如需完整功能，请下载QuellGTA实现退出游戏自动进行清洗。www.MageAngela.cn\n");
    printf("================================================================================\n\n");

    printf("按任意键开始清理...\n");
    _getch();

    // 获取用户文档目录
    getUserDocumentsPath(documentsPath, MAX_PATH_LEN);

    // 构建两个基础路径
    sprintf_s(basePath1, MAX_PATH_LEN, "%s\\Rockstar Games\\GTA V\\Profiles", documentsPath);
    sprintf_s(basePath2, MAX_PATH_LEN, "%s\\Rockstar Games\\GTAV Enhanced\\Profiles", documentsPath);

    printf("\n开始清洗文件...\n");
    printf("----------------------------------------\n");

    // 检查并处理GTA V目录
    DWORD attrs = GetFileAttributesA(basePath1);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        processDirectory(basePath1, "GTA V");
    }
    else {
        printf("GTA V....存档目录不存在: %s\n", basePath1);
    }

    // 检查并处理GTAV Enhanced目录
    attrs = GetFileAttributesA(basePath2);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        processDirectory(basePath2, "GTAV Enhanced");
    }
    else {
        printf("GTAV Enhanced....存档目录不存在: %s\n", basePath2);
    }

    printf("----------------------------------------\n");
    printf("\n所有操作已完成！\n");
    printf("按任意键退出程序...\n");
    _getch();

    return 0;
}