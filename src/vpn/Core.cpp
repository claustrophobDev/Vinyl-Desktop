#include "vpn/Core.h"

#include <fstream>

#include "app/Log.h"
#include "app/Paths.h"
#include "core/Errors.h"
#include "core/LinkParser.h"
#include "core/SingBoxConfig.h"
#include "core/Text.h"

namespace {

// пока шлем ctrl+break, это событие прилетает и нам. обработчик, который вернул TRUE,
// считается отработавшим, и винда нас не закрывает
BOOL WINAPI swallowCtrl(DWORD) {
    return TRUE;
}

bool writeConfig(const std::string& config, std::string& error) {
    std::ofstream out(paths::configFile().c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Не удалось записать конфиг рядом с программой";
        return false;
    }
    out << config;
    if (!out) {
        error = "Конфиг записался не полностью";
        return false;
    }
    return true;
}

} // namespace

VpnCore::~VpnCore() {
    stop();
}

void VpnCore::onState(std::function<void(VpnState, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

std::string VpnCore::message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return message_;
}

void VpnCore::setState(VpnState state, const std::string& message) {
    std::function<void(VpnState, const std::string&)> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        message_ = message;
        callback = callback_;
    }
    state_.store(state);
    if (callback) callback(state, message);
}

bool VpnCore::start(const Server& server, const Routing& routing, const Settings& settings) {
    if (!server.unsupported.empty()) {
        stop();
        setState(VpnState::Error, server.unsupported);
        return false;
    }

    std::string config;
    try {
        Proxy proxy = LinkParser::toProxy(server.link);
        config = SingBoxConfig::build(proxy, routing, settings);
    } catch (const std::exception& e) {
        stop();
        setState(VpnState::Error, e.what());
        return false;
    }

    if (!startWithConfig(config)) return false;
    applog::line("сервер " + server.host + ":" + std::to_string(server.port));
    return true;
}

bool VpnCore::startWithConfig(const std::string& config) {
    stop();
    stopping_.store(false);
    setState(VpnState::Starting, "");

    std::string error;
    if (!writeConfig(config, error)) {
        setState(VpnState::Error, error);
        return false;
    }

    std::wstring exe = paths::coreExe();
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        setState(VpnState::Error, "Рядом с программой нет папки core с sing-box.exe");
        return false;
    }

    // весь вывод ядра пишем в свой файл, иначе он просто теряется
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    logFile_ = CreateFileW(paths::coreLogFile().c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logFile_ == INVALID_HANDLE_VALUE) logFile_ = nullptr;

    // ядро в job, чтобы при падении окна туннель не остался висеть в системе
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }

    std::wstring commandLine = L"\"" + exe + L"\" run -c \"" + paths::configFile() + L"\" -D \"" + paths::dataDir() + L"\"";
    std::wstring workDir = paths::coreDir();

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = logFile_;
    si.hStdError = logFile_;
    si.hStdInput = nullptr;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableLine(commandLine.begin(), commandLine.end());
    mutableLine.push_back(L'\0');

    // консоль скрытая, она нужна только чтобы потом послать ctrl+break.
    // группа своя, иначе это событие прилетит и нам
    BOOL ok = CreateProcessW(exe.c_str(), mutableLine.data(), nullptr, nullptr, TRUE,
                             CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                             nullptr, workDir.c_str(), &si, &pi);
    if (!ok) {
        DWORD code = GetLastError();
        applog::line("не запустился sing-box, ошибка " + std::to_string(code));
        closeProcess();
        setState(VpnState::Error, "Не удалось запустить ядро, ошибка " + std::to_string(code));
        return false;
    }

    if (job_) AssignProcessToJobObject(job_, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    process_ = pi.hProcess;

    // если конфиг кривой, ядро умирает почти сразу, так что немного подождем
    if (WaitForSingleObject(process_, 700) == WAIT_OBJECT_0) {
        DWORD code = 1;
        GetExitCodeProcess(process_, &code);
        closeProcess();
        applog::line("ядро сразу вышло с кодом " + std::to_string(code));
        setState(VpnState::Error, "Ядро не запустилось, подробности в data\\core.log");
        return false;
    }

    applog::line("ядро поднялось");
    setState(VpnState::Running, "");

    watcher_ = std::thread(&VpnCore::watch, this);
    return true;
}

void VpnCore::watch() {
    if (!process_) return;
    WaitForSingleObject(process_, INFINITE);
    if (stopping_.load()) return;

    // сюда попадаем только если ядро отвалилось само
    DWORD code = 1;
    GetExitCodeProcess(process_, &code);
    applog::line("ядро неожиданно завершилось, код " + std::to_string(code));
    setState(VpnState::Error, "Ядро остановилось само, смотри data\\core.log");
}

bool VpnCore::askToStop() {
    if (!process_) return false;
    DWORD pid = GetProcessId(process_);
    if (pid == 0) return false;

    FreeConsole();
    if (!AttachConsole(pid)) return false;

    // ctrl+break через nullptr не игнорируется, в отличие от ctrl+c, поэтому ставим свой
    // обработчик. и шлем событие ровно в группу ядра, а не всем в консоли
    SetConsoleCtrlHandler(&swallowCtrl, TRUE);
    bool sent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid) != 0;
    if (!sent) releaseConsole();
    return sent;
}

// консоль и обработчик отпускаем только когда ядро уже завершилось
void VpnCore::releaseConsole() {
    SetConsoleCtrlHandler(&swallowCtrl, FALSE);
    FreeConsole();
}

void VpnCore::stop() {
    stopping_.store(true);

    if (process_) {
        // сначала по-хорошему, иначе в системе остается мертвый адаптер
        bool asked = askToStop();
        if (asked && WaitForSingleObject(process_, 5000) == WAIT_OBJECT_0) {
            gracefulStop_ = true;
            applog::line("ядро закрылось само");
        } else {
            gracefulStop_ = false;
            applog::line(asked ? "ядро не закрылось за 5 секунд, прибиваем" : "ядро прибиваем сразу");
            TerminateProcess(process_, 0);
            WaitForSingleObject(process_, 3000);
        }
        if (asked) releaseConsole();
    }
    if (watcher_.joinable()) watcher_.join();

    closeProcess();
    if (state_.load() != VpnState::Stopped) {
        applog::line("ядро остановлено");
        setState(VpnState::Stopped, "");
    }
}

void VpnCore::closeProcess() {
    if (process_) {
        CloseHandle(process_);
        process_ = nullptr;
    }
    if (job_) {
        CloseHandle(job_);
        job_ = nullptr;
    }
    if (logFile_) {
        CloseHandle(logFile_);
        logFile_ = nullptr;
    }
}
