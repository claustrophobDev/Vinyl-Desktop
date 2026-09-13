#include "app/Autostart.h"

#include <windows.h>

#include <string>

#include "app/Paths.h"

namespace autostart {
namespace {

const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kValueName = L"Vinyl";

} // namespace

bool enabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    wchar_t buf[MAX_PATH * 2] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    bool found = RegQueryValueExW(key, kValueName, nullptr, &type,
                                  reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS && type == REG_SZ;
    RegCloseKey(key);
    if (!found) return false;

    // папку могли перенести, тогда запись уже не про нас
    std::wstring expected = L"\"" + paths::exePath() + L"\"";
    return expected == buf;
}

bool set(bool on) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;

    bool ok;
    if (on) {
        std::wstring value = L"\"" + paths::exePath() + L"\"";
        ok = RegSetValueExW(key, kValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value.c_str()),
                            (DWORD)((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    } else {
        LONG result = RegDeleteValueW(key, kValueName);
        ok = result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }
    RegCloseKey(key);
    return ok;
}

} // namespace autostart
