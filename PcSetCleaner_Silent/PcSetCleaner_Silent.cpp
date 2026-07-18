#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#define MAX_PATH_LEN 512
#define BUFFER_SIZE 1024
#define INDEX_THRESHOLD 850
#define FILE_NAME "pc_settings.bin"

// 白名单数组
int whitelist[] = {
    0, 1, 2, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    100, 203, 204, 205, 207, 208, 211, 212, 213, 220, 221, 222, 223, 224, 225,
    226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240,
    241, 242, 243, 244, 245, 251, 260, 261, 262, 300, 301, 302, 303, 304, 305,
    306, 307, 308, 310, 311, 312, 313, 314, 315, 316, 317, 318, 320, 321, 322,
    323, 324, 325, 326, 327, 328, 329, 330, 331, 333, 334, 335, 336, 337, 338,
    339, 340, 412, 415, 710, 720, 721, 722, 723, 724, 725, 726, 727, 728, 729,
    750, 751, 752, 753, 754, 755, 756, 757, 758, 759, 760, 761, 762, 800, 801,
    802, 803, 804, 805, 806, 807, 810, 811, 900, 950, 951, 952, 953, 954, 955,
    956, 957, 958, 960, 961, 962, 964
};

// 计算白名单数组大小
#define WHITELIST_SIZE (sizeof(whitelist) / sizeof(whitelist[0]))

// 检查索引是否在白名单中
int isInWhitelist(unsigned int index) {
    // 使用二分查找（因为白名单是递增的）
    int left = 0;
    int right = WHITELIST_SIZE - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        if (whitelist[mid] == index) {
            return 1; // 在白名单中
        }
        else if (whitelist[mid] < index) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }
    return 0; // 不在白名单中
}

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

// 处理配置文件，只保留白名单中的索引
int processConfigFile(const char* filePath) {
    size_t fileSize;
    unsigned char* fileData = readFile(filePath, &fileSize);

    if (fileData == NULL) {
        return 0;
    }

    // 检查文件大小是否为8的倍数
    if (fileSize % 8 != 0) {
        free(fileData);
        return 0;
    }

    // 计算项目数量
    size_t entryCount = fileSize / 8;

    // 创建新的缓冲区
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

        // 如果索引在白名单中，保留此项目
        if (isInWhitelist(index)) {
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

                    successCount++;
                }
                else {

                    failCount++;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // 如果没有子目录或没有配置文件
    if (processedCount == 0) {

    }
}

int main() {
    char documentsPath[MAX_PATH_LEN];
    char basePath1[MAX_PATH_LEN];
    char basePath2[MAX_PATH_LEN];

    // 获取用户文档目录
    getUserDocumentsPath(documentsPath, MAX_PATH_LEN);

    // 构建两个基础路径
    sprintf_s(basePath1, MAX_PATH_LEN, "%s\\Rockstar Games\\GTA V\\Profiles", documentsPath);
    sprintf_s(basePath2, MAX_PATH_LEN, "%s\\Rockstar Games\\GTAV Enhanced\\Profiles", documentsPath);

    // 检查并处理GTA V目录
    DWORD attrs = GetFileAttributesA(basePath1);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        processDirectory(basePath1, "GTA V");
    }
    else {

    }

    // 检查并处理GTAV Enhanced目录
    attrs = GetFileAttributesA(basePath2);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        processDirectory(basePath2, "GTAV Enhanced");
    }
    else {

    }


    return 0;
}