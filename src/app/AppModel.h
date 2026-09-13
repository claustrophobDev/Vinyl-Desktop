#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "data/Models.h"

// вся логика приложения без окон: серверы, подписки, пинг, настройки
// тяжелое (сеть, пинг) уходит в отдельные потоки, наружу дергается onChanged
class AppModel {
public:
    AppModel();
    ~AppModel();

    AppModel(const AppModel&) = delete;
    AppModel& operator=(const AppModel&) = delete;

    // звать пока окно еще живо: колбеки держат на него указатель
    void stop();

    void load();
    // зовется из любого потока, окно просто шлет себе сообщение и перерисовывается
    void onChanged(std::function<void()> callback);
    void onMessage(std::function<void(const std::string&)> callback);

    AppState state() const;
    std::map<std::string, int> pings() const;
    bool pinging() const;
    std::set<std::string> refreshing() const;

    // какой сервер реально пойдет в ядро: выбранный или лучший по пингу
    bool pickServer(Server& out) const;
    bool findServer(const std::string& id, Server& out) const;

    // серверы
    void addFromText(const std::string& text);
    void deleteServer(const std::string& id);
    void select(const std::string& id);
    void pingAll();

    // подписки
    void refresh(const std::string& subId);
    void refreshAll();
    void deleteSubscription(const std::string& subId);

    // маршруты
    void setAppMode(AppMode mode);
    void toggleApp(const std::string& exe);
    bool addDomain(const std::string& domain);
    void removeDomain(const std::string& domain);
    void setBypassBanks(bool on);
    void setDirectRuSites(bool on);

    // настройки
    void setIpv6(bool on);
    void setDns(RemoteDns dns);
    void setAutoConnect(bool on);

    // после смены маршрутов надо переподключиться чтобы применилось
    bool needReconnect() const { return needReconnect_; }
    void setNeedReconnect(bool on);

private:
    // вся фоновая работа через одну очередь и один поток, его join-им в stop()
    void post(std::function<void()> job);
    void worker();

    void changed();
    void say(const std::string& message);
    void saveLocked();
    // добавляет разобранные серверы, возвращает сколько новых
    int mergeServers(const std::vector<Server>& servers, const std::string& subId);

    mutable std::mutex mutex_;
    AppState state_;
    std::map<std::string, int> pings_;
    std::set<std::string> refreshing_;
    bool pinging_ = false;
    bool needReconnect_ = false;

    std::function<void()> onChanged_;
    std::function<void(const std::string&)> onMessage_;

    std::thread worker_;
    std::mutex queueMutex_;
    std::condition_variable queueReady_;
    std::deque<std::function<void()>> queue_;
    std::atomic<bool> stopping_{ false };
};
