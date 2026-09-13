#include "app/Log.h"

#include <windows.h>

#include <fstream>
#include <mutex>

#include "app/Paths.h"

namespace applog {
namespace {

std::mutex mutex;
std::ofstream file;

std::string stamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char buf[32];
    sprintf_s(buf, "%02d:%02d:%02d.%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return buf;
}

} // namespace

void open() {
    std::lock_guard<std::mutex> lock(mutex);
    if (file.is_open()) return;

    std::wstring path = paths::logFile();
    // если лог разросся, начинаем заново, старый не храним
    WIN32_FILE_ATTRIBUTE_DATA info;
    bool truncate = false;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info)) {
        if (info.nFileSizeLow > 2 * 1024 * 1024 || info.nFileSizeHigh > 0) truncate = true;
    }
    bool fresh = truncate || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES;
    file.open(path.c_str(), truncate ? std::ios::trunc : std::ios::app);
    // без метки в начале блокнот считает файл виндовой кодировкой и русский превращается в кашу
    if (fresh && file.is_open()) file << "\xEF\xBB\xBF";
}

void line(const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!file.is_open()) return;
    file << stamp() << "  " << text << "\n";
    file.flush();
}

void close() {
    std::lock_guard<std::mutex> lock(mutex);
    if (file.is_open()) file.close();
}

} // namespace applog
