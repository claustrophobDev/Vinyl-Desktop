#include "app/Paths.h"

#include <windows.h>
#include <shlwapi.h>

namespace paths {

std::wstring exePath() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    return std::wstring(buf, n);
}

std::wstring exeDir() {
    std::wstring p = exePath();
    size_t slash = p.find_last_of(L'\\');
    if (slash == std::wstring::npos) return L".";
    return p.substr(0, slash);
}

static std::wstring join(const std::wstring& dir, const wchar_t* name) {
    return dir + L"\\" + name;
}

std::wstring dataDir()     { return join(exeDir(), L"data"); }
std::wstring stateFile()   { return join(dataDir(), L"state.json"); }
std::wstring logFile()     { return join(dataDir(), L"vinyl.log"); }
std::wstring coreLogFile() { return join(dataDir(), L"core.log"); }
std::wstring configFile()  { return join(dataDir(), L"config.json"); }
std::wstring coreDir()     { return join(exeDir(), L"core"); }
// ядро запускаем из его же папки, иначе оно не найдет рядом wintun.dll
std::wstring coreExe()     { return join(coreDir(), L"sing-box.exe"); }

void ensureDataDir() {
    CreateDirectoryW(dataDir().c_str(), nullptr);
}

} // namespace paths
