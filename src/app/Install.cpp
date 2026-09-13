#include "app/Install.h"

#include <windows.h>
#include <shlobj.h>
#include <objbase.h>

#include "app/Log.h"
#include "app/Paths.h"

namespace install {
namespace {

// пишем в HKCU, так не нужны права админа и запись не остается для других пользователей
const wchar_t* kUninstallKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Vinyl";

bool setString(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(value.c_str()),
                          (DWORD)((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool setDword(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD,
                          reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
}

std::wstring programsDir() {
    wchar_t buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, buf))) {
        return buf;
    }
    return std::wstring();
}

bool makeShortcut(const std::wstring& link, const std::wstring& target, const std::wstring& workDir) {
    IShellLinkW* shell = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void**>(&shell));
    if (FAILED(hr) || shell == nullptr) return false;

    bool ok = false;
    shell->SetPath(target.c_str());
    shell->SetWorkingDirectory(workDir.c_str());
    shell->SetDescription(L"Vinyl, VPN на sing-box");
    shell->SetIconLocation(target.c_str(), 0);

    IPersistFile* file = nullptr;
    if (SUCCEEDED(shell->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file))) && file) {
        ok = SUCCEEDED(file->Save(link.c_str(), TRUE));
        file->Release();
    }
    shell->Release();
    return ok;
}

// сколько весит папка, винда показывает это в списке программ
DWORD folderSizeKb(const std::wstring& dir) {
    WIN32_FIND_DATAW item;
    HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &item);
    if (find == INVALID_HANDLE_VALUE) return 0;

    unsigned long long total = 0;
    do {
        std::wstring name = item.cFileName;
        if (name == L"." || name == L"..") continue;
        if (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += (unsigned long long)folderSizeKb(dir + L"\\" + name) * 1024ull;
        } else {
            total += ((unsigned long long)item.nFileSizeHigh << 32) | item.nFileSizeLow;
        }
    } while (FindNextFileW(find, &item));
    FindClose(find);
    return (DWORD)(total / 1024ull);
}

} // namespace

std::wstring shortcutPath() {
    std::wstring programs = programsDir();
    if (programs.empty()) return std::wstring();
    return programs + L"\\Vinyl.lnk";
}

bool isRegistered() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    // папку могли перенести, тогда запись в реестре врет и ее надо обновить
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    bool same = false;
    if (RegQueryValueExW(key, L"InstallLocation", nullptr, &type, reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS
        && type == REG_SZ) {
        same = paths::exeDir() == buf;
    }
    RegCloseKey(key);
    return same;
}

bool registerApp() {
    std::wstring dir = paths::exeDir();
    std::wstring exe = paths::exePath();

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kUninstallKey, 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    setString(key, L"DisplayName", kDisplayName);
    setString(key, L"DisplayVersion", std::wstring(kVersion, kVersion + strlen(kVersion)));
    setString(key, L"Publisher", L"claustrophobDev");
    setString(key, L"DisplayIcon", exe);
    setString(key, L"InstallLocation", dir);
    setString(key, L"UninstallString", L"\"" + dir + L"\\uninstall.exe\"");
    setDword(key, L"NoModify", 1);
    setDword(key, L"NoRepair", 1);
    setDword(key, L"EstimatedSize", folderSizeKb(dir));
    RegCloseKey(key);

    std::wstring link = shortcutPath();
    if (!link.empty()) makeShortcut(link, exe, dir);
    return true;
}

bool unregisterApp() {
    std::wstring link = shortcutPath();
    if (!link.empty()) DeleteFileW(link.c_str());
    return RegDeleteTreeW(HKEY_CURRENT_USER, kUninstallKey) == ERROR_SUCCESS;
}

void ensureRegistered() {
    if (isRegistered()) return;
    if (registerApp()) applog::line("программа прописана в системе");
    else applog::line("не получилось прописаться в системе, работаем как есть");
}

} // namespace install
