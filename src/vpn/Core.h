#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "data/Models.h"

enum class VpnState { Stopped, Starting, Running, Error };

// запускает рядом лежащий sing-box.exe и следит чтобы он не убежал
// само ядро в отдельном процессе: если оно упадет, окно останется живым
class VpnCore {
public:
    VpnCore() = default;
    ~VpnCore();

    VpnCore(const VpnCore&) = delete;
    VpnCore& operator=(const VpnCore&) = delete;

    // зовется из чужого потока, в ui надо перекидывать сообщением
    void onState(std::function<void(VpnState, const std::string&)> callback);

    bool start(const Server& server, const Routing& routing, const Settings& settings);
    void stop();

    VpnState state() const { return state_.load(); }
    std::string message() const;

private:
    void setState(VpnState state, const std::string& message);
    void watch();
    void closeProcess();
    // просит ядро закрыться самостоятельно, false если не получилось даже попросить
    bool askToStop();

    std::atomic<VpnState> state_{ VpnState::Stopped };

    mutable std::mutex mutex_;
    std::string message_;
    std::function<void(VpnState, const std::string&)> callback_;

    HANDLE process_ = nullptr;
    HANDLE job_ = nullptr;
    HANDLE logFile_ = nullptr;
    std::thread watcher_;
    std::atomic<bool> stopping_{ false };
};
