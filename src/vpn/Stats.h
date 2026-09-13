#pragma once

#include <atomic>
#include <thread>

// раз в секунду спрашивает у ядра сколько прокачано и считает скорость
// ядро отдает это через свой локальный api, который мы включили в конфиге
class TrafficStats {
public:
    ~TrafficStats();

    void start();
    void stop();
    void reset();

    long long downSpeed() const { return downSpeed_.load(); }
    long long upSpeed() const { return upSpeed_.load(); }
    long long downTotal() const { return downTotal_.load(); }
    long long upTotal() const { return upTotal_.load(); }
    // секунды с момента подключения
    long long uptime() const;

private:
    void loop();

    std::thread worker_;
    std::atomic<bool> running_{ false };
    std::atomic<long long> downSpeed_{ 0 };
    std::atomic<long long> upSpeed_{ 0 };
    std::atomic<long long> downTotal_{ 0 };
    std::atomic<long long> upTotal_{ 0 };
    std::atomic<long long> startedAt_{ 0 };
};
