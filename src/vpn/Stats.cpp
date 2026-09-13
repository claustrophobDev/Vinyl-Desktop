#include "vpn/Stats.h"

#include <windows.h>

#include <chrono>
#include <string>

#include "core/Json.h"
#include "core/SingBoxConfig.h"
#include "net/Http.h"

namespace {

long long nowSeconds() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(now).count();
}

} // namespace

TrafficStats::~TrafficStats() {
    stop();
}

void TrafficStats::reset() {
    downSpeed_.store(0);
    upSpeed_.store(0);
    downTotal_.store(0);
    upTotal_.store(0);
}

void TrafficStats::start() {
    stop();
    reset();
    startedAt_.store(nowSeconds());
    running_.store(true);
    worker_ = std::thread(&TrafficStats::loop, this);
}

void TrafficStats::stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
    startedAt_.store(0);
}

long long TrafficStats::uptime() const {
    long long started = startedAt_.load();
    if (started == 0) return 0;
    return nowSeconds() - started;
}

void TrafficStats::loop() {
    std::string url = "http://127.0.0.1:" + std::to_string(SingBoxConfig::kClashApiPort) + "/connections";
    long long lastDown = -1;
    long long lastUp = -1;

    while (running_.load()) {
        http::Response response = http::get(url);
        if (response.ok) {
            try {
                Json json = Json::parse(response.body);
                long long down = json.value("downloadTotal", 0ll);
                long long up = json.value("uploadTotal", 0ll);

                if (lastDown >= 0) {
                    long long deltaDown = down - lastDown;
                    long long deltaUp = up - lastUp;
                    downSpeed_.store(deltaDown > 0 ? deltaDown : 0);
                    upSpeed_.store(deltaUp > 0 ? deltaUp : 0);
                }
                lastDown = down;
                lastUp = up;
                downTotal_.store(down);
                upTotal_.store(up);
            } catch (const std::exception&) {
                // ядро могло еще не поднять свой api, просто ждем следующий круг
            }
        }

        for (int i = 0; i < 10 && running_.load(); i++) {
            Sleep(100);
        }
    }
    downSpeed_.store(0);
    upSpeed_.store(0);
}
