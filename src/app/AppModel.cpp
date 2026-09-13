#include "app/AppModel.h"

#include <algorithm>
#include <thread>

#include "app/Log.h"
#include "core/Errors.h"
#include "core/LinkParser.h"
#include "core/SingBoxConfig.h"
#include "core/Text.h"
#include "data/Storage.h"
#include "net/Http.h"
#include "net/Ping.h"
#include "ui/Format.h"

namespace {

// заголовок вида: upload=123; download=456; total=789; expire=1700000000
std::map<std::string, long long> parseUserInfo(const std::string& header) {
    std::map<std::string, long long> out;
    for (const std::string& part : text::split(header, ';')) {
        std::string key = text::lower(text::trim(text::before(part, "=", part)));
        std::string value = text::trim(text::after(part, "=", ""));
        if (key.empty() || value.empty()) continue;
        out[key] = text::toLong(value, 0);
    }
    return out;
}

std::string parseTitle(const std::string& header) {
    std::string value = text::trim(header);
    if (value.empty()) return std::string();
    if (text::startsWith(value, "base64:")) {
        std::string decoded;
        if (text::fromBase64(value.substr(7), decoded)) return text::trim(decoded);
    }
    return value;
}

bool looksLikeSubscription(const std::string& line) {
    std::string lower = text::lower(line);
    return text::startsWith(lower, "http://") || text::startsWith(lower, "https://");
}

} // namespace

AppModel::AppModel() {
    worker_ = std::thread(&AppModel::worker, this);
}

AppModel::~AppModel() {
    stop();
}

void AppModel::stop() {
    if (stopping_.exchange(true)) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.clear();
    }
    queueReady_.notify_all();
    if (worker_.joinable()) worker_.join();

    // поток встал, значит никто больше не позовет колбеки, можно спокойно их убрать
    std::lock_guard<std::mutex> lock(mutex_);
    onChanged_ = nullptr;
    onMessage_ = nullptr;
}

void AppModel::post(std::function<void()> job) {
    if (stopping_.load()) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push_back(std::move(job));
    }
    queueReady_.notify_one();
}

void AppModel::worker() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueReady_.wait(lock, [this] { return stopping_.load() || !queue_.empty(); });
            if (stopping_.load()) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();
    }
}

void AppModel::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = Storage::load();
}

void AppModel::onChanged(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onChanged_ = std::move(callback);
}

void AppModel::onMessage(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onMessage_ = std::move(callback);
}

AppState AppModel::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::map<std::string, int> AppModel::pings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pings_;
}

bool AppModel::pinging() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pinging_;
}

std::set<std::string> AppModel::refreshing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return refreshing_;
}

void AppModel::changed() {
    std::function<void()> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = onChanged_;
    }
    if (callback) callback();
}

void AppModel::say(const std::string& message) {
    std::function<void(const std::string&)> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = onMessage_;
    }
    if (callback) callback(message);
}

void AppModel::saveLocked() {
    Storage::save(state_);
}

bool AppModel::findServer(const std::string& id, Server& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Server& s : state_.servers) {
        if (s.id == id) {
            out = s;
            return true;
        }
    }
    return false;
}

bool AppModel::pickServer(Server& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.servers.empty()) return false;

    if (state_.selectedId != kAutoServerId) {
        for (const Server& s : state_.servers) {
            if (s.id == state_.selectedId && s.unsupported.empty()) {
                out = s;
                return true;
            }
        }
    }

    // автовыбор: берем с самым маленьким пингом, а если не пинговали то первый рабочий
    const Server* best = nullptr;
    int bestPing = 0;
    for (const Server& s : state_.servers) {
        if (!s.unsupported.empty()) continue;
        auto it = pings_.find(s.id);
        int ms = (it == pings_.end()) ? ping::kTimeout : it->second;
        if (best == nullptr) {
            best = &s;
            bestPing = ms;
            continue;
        }
        bool better = (ms > 0) && (bestPing <= 0 || ms < bestPing);
        if (better) {
            best = &s;
            bestPing = ms;
        }
    }
    if (best == nullptr) return false;
    out = *best;
    return true;
}

int AppModel::mergeServers(const std::vector<Server>& servers, const std::string& subId) {
    int added = 0;
    for (const Server& server : servers) {
        bool exists = false;
        for (Server& old : state_.servers) {
            if (old.id != server.id) continue;
            // сервер уже есть, просто обновляем данные
            std::string keepSub = old.subscriptionId;
            old = server;
            if (old.subscriptionId.empty()) old.subscriptionId = keepSub;
            exists = true;
            break;
        }
        if (!exists) {
            state_.servers.push_back(server);
            added++;
        }
    }
    (void)subId;
    return added;
}

void AppModel::addFromText(const std::string& text) {
    std::string body = ::text::trim(text);
    if (body.empty()) {
        say("В буфере обмена ничего нет");
        return;
    }

    // сначала ищем обычные ключи
    std::vector<Server> direct = LinkParser::parseSubscription(body, "");
    if (!direct.empty()) {
        int added;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            added = mergeServers(direct, "");
            saveLocked();
        }
        say(added > 0 ? "Добавлено: " + std::to_string(added)
                      : "Такие серверы уже есть");
        changed();
        return;
    }

    // иначе считаем что это ссылка на подписку
    std::string url;
    for (const std::string& line : ::text::split(body, '\n')) {
        std::string item = ::text::trim(line);
        if (looksLikeSubscription(item)) {
            url = item;
            break;
        }
    }
    if (url.empty()) {
        say("Не похоже ни на ключ, ни на ссылку подписки");
        return;
    }

    std::string id = ::text::sha1Short(url);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool exists = false;
        for (const Subscription& sub : state_.subscriptions) {
            if (sub.id == id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            Subscription sub;
            sub.id = id;
            sub.url = url;
            // после решетки панели обычно пишут название подписки, до первого обновления берем его
            std::string title = ::text::trim(::text::urlDecode(::text::after(url, "#", "")));
            sub.name = title.empty() ? "Подписка" : title;
            state_.subscriptions.push_back(sub);
            saveLocked();
        }
    }
    changed();
    refresh(id);
}

void AppModel::refresh(const std::string& subId) {
    std::string url;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (refreshing_.count(subId) > 0) return;
        for (const Subscription& sub : state_.subscriptions) {
            if (sub.id == subId) {
                url = sub.url;
                break;
            }
        }
        if (url.empty()) return;
        refreshing_.insert(subId);
    }
    changed();

    post([this, subId, url] {
        http::Response response = http::get(url);

        std::string message;
        if (!response.ok) {
            message = response.error;
        } else {
            std::vector<Server> servers = LinkParser::parseSubscription(response.body, subId);
            if (servers.empty()) {
                message = "В подписке нет поддерживаемых серверов";
            } else {
                std::lock_guard<std::mutex> lock(mutex_);

                // серверы этой подписки заменяем целиком, чужие не трогаем
                std::vector<Server> keep;
                for (const Server& s : state_.servers) {
                    if (s.subscriptionId != subId) keep.push_back(s);
                }
                state_.servers = keep;
                mergeServers(servers, subId);

                std::map<std::string, long long> info = parseUserInfo(response.headers["subscription-userinfo"]);
                std::string title = parseTitle(response.headers["profile-title"]);

                for (Subscription& sub : state_.subscriptions) {
                    if (sub.id != subId) continue;
                    sub.updatedAt = fmt::nowMs();
                    if (!title.empty()) sub.name = title;
                    sub.uploadBytes = info["upload"];
                    sub.downloadBytes = info["download"];
                    sub.totalBytes = info["total"];
                    sub.expireAt = info["expire"] * 1000;
                    break;
                }
                saveLocked();
                message = "Обновлено серверов: " + std::to_string(servers.size());
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            refreshing_.erase(subId);
        }
        applog::line("подписка обновлена: " + message);
        say(message);
        changed();
    });
}

void AppModel::refreshAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const Subscription& sub : state_.subscriptions) ids.push_back(sub.id);
    }
    for (const std::string& id : ids) refresh(id);
}

void AppModel::deleteSubscription(const std::string& subId) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Server> keep;
        for (const Server& s : state_.servers) {
            if (s.subscriptionId != subId) keep.push_back(s);
        }
        state_.servers = keep;

        std::vector<Subscription> subs;
        for (const Subscription& sub : state_.subscriptions) {
            if (sub.id != subId) subs.push_back(sub);
        }
        state_.subscriptions = subs;
        saveLocked();
    }
    changed();
}

void AppModel::deleteServer(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Server> keep;
        for (const Server& s : state_.servers) {
            if (s.id != id) keep.push_back(s);
        }
        state_.servers = keep;
        if (state_.selectedId == id) state_.selectedId = kAutoServerId;
        saveLocked();
    }
    changed();
}

void AppModel::select(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.selectedId = id;
        saveLocked();
    }
    changed();
}

void AppModel::pingAll() {
    std::vector<Server> servers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pinging_) return;
        pinging_ = true;
        servers = state_.servers;
    }
    changed();

    post([this, servers] {
        ping::all(servers, [this](std::string id, int ms) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pings_[id] = ms;
            }
            changed();
        }, &stopping_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pinging_ = false;
        }
        changed();
    });
}

void AppModel::setNeedReconnect(bool on) {
    needReconnect_ = on;
    changed();
}

void AppModel::setAppMode(AppMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.routing.appMode = mode;
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::toggleApp(const std::string& exe) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string>& apps = state_.routing.apps;
        auto it = std::find(apps.begin(), apps.end(), exe);
        if (it == apps.end()) apps.push_back(exe);
        else apps.erase(it);
        std::sort(apps.begin(), apps.end());
        saveLocked();
    }
    setNeedReconnect(true);
}

bool AppModel::addDomain(const std::string& domain) {
    std::string clean;
    if (!SingBoxConfig::cleanDomain(domain, clean)) {
        say("Не похоже на адрес сайта");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string>& list = state_.routing.directDomains;
        if (std::find(list.begin(), list.end(), clean) != list.end()) {
            say("Такой сайт уже есть");
            return false;
        }
        list.push_back(clean);
        saveLocked();
    }
    setNeedReconnect(true);
    return true;
}

void AppModel::removeDomain(const std::string& domain) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string>& list = state_.routing.directDomains;
        list.erase(std::remove(list.begin(), list.end(), domain), list.end());
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::setBypassBanks(bool on) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.routing.bypassBanks = on;
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::setDirectRuSites(bool on) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.routing.directRuSites = on;
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::setIpv6(bool on) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.settings.ipv6 = on;
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::setDns(RemoteDns dns) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.settings.dns = dns;
        saveLocked();
    }
    setNeedReconnect(true);
}

void AppModel::setAutoConnect(bool on) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.settings.autoConnect = on;
        saveLocked();
    }
    changed();
}
