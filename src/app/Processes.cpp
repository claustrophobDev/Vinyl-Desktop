#include "app/Processes.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <set>

#include "core/Text.h"

namespace processes {
namespace {

// служебные процессы винды в списке только мешают
bool boring(const std::string& exe) {
    static const char* list[] = {
        "system", "system idle process", "registry", "smss.exe", "csrss.exe", "wininit.exe",
        "services.exe", "lsass.exe", "winlogon.exe", "fontdrvhost.exe", "dwm.exe", "svchost.exe",
        "spoolsv.exe", "sihost.exe", "taskhostw.exe", "ctfmon.exe", "conhost.exe", "dllhost.exe",
        "runtimebroker.exe", "searchhost.exe", "startmenuexperiencehost.exe", "shellexperiencehost.exe",
        "textinputhost.exe", "lockapp.exe", "wudfhost.exe", "audiodg.exe", "memory compression",
        "securityhealthservice.exe", "wmiprvse.exe", "sppsvc.exe", "backgroundtaskhost.exe",
        "applicationframehost.exe", "sing-box.exe"
    };
    for (const char* item : list) {
        if (exe == item) return true;
    }
    return false;
}

// chrome.exe -> Chrome
std::wstring prettify(const std::string& exe) {
    std::string name = exe;
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    if (name.empty()) return text::wide(exe);
    name[0] = (char)toupper((unsigned char)name[0]);
    return text::wide(name);
}

} // namespace

std::vector<Item> running() {
    std::vector<Item> out;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return out;

    std::set<std::string> seen;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            std::string exe = text::lower(text::narrow(entry.szExeFile));
            if (exe.empty() || boring(exe)) continue;
            if (!seen.insert(exe).second) continue;
            out.push_back({ exe, prettify(exe) });
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

    std::sort(out.begin(), out.end(), [](const Item& a, const Item& b) { return a.exe < b.exe; });
    return out;
}

} // namespace processes
