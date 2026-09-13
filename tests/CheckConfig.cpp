// гоняет ссылки через парсер и складывает конфиги в файлы, дальше их жует sing-box check

#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "app/Log.h"
#include "app/Paths.h"
#include "core/Errors.h"
#include "core/LinkParser.h"
#include "core/SingBoxConfig.h"
#include "data/Models.h"
#include "net/Http.h"
#include "net/Ping.h"
#include "vpn/Core.h"
#include "vpn/Stats.h"

namespace {

struct Sample {
    const char* file;
    const char* link;
};

// ссылки выдуманные, важен только формат
// но ключи подделать нельзя: ядро декодирует их из base64 и требует ровно 32 байта,
// иначе ловим invalid public_key еще на проверке конфига
const Sample kSamples[] = {
    { "vless-reality",
      "vless://11111111-2222-3333-4444-555555555555@example.com:443"
      "?type=tcp&security=reality&flow=xtls-rprx-vision&fp=chrome"
      "&pbk=0123456789abcdefghijklmnopqrstuvwxyzABCDEFG&sid=abcd1234&sni=www.microsoft.com#%F0%9F%87%A9%F0%9F%87%AA%20Germany" },

    { "vless-ws-tls",
      "vless://11111111-2222-3333-4444-555555555555@example.com:443"
      "?type=ws&security=tls&path=%2Fvinyl%3Fed%3D2048&host=cdn.example.com&sni=cdn.example.com#Netherlands%20NL" },

    { "vless-grpc",
      "vless://11111111-2222-3333-4444-555555555555@example.com:443"
      "?type=grpc&security=tls&serviceName=grpcvinyl&fp=firefox#Finland" },

    // у vmess внутри base64 лежит обычный json
    { "vmess-ws",
      "vmess://eyJ2IjoiMiIsInBzIjoiVG9reW8iLCJhZGQiOiJleGFtcGxlLmNvbSIsInBvcnQiOiI0NDMiLCJpZCI6IjExMTExMTExLTIyMjItMzMzMy00NDQ0LTU1NTU1NTU1NTU1NSIsImFpZCI6IjAiLCJuZXQiOiJ3cyIsImhvc3QiOiJjZG4uZXhhbXBsZS5jb20iLCJwYXRoIjoiL3ZtIiwidGxzIjoidGxzIn0=" },

    { "trojan",
      "trojan://secret-password@example.com:443?sni=example.com&type=tcp#Paris%20FR" },

    // ss://base64(aes-256-gcm:password)@host:port
    { "shadowsocks",
      "ss://YWVzLTI1Ni1nY206c2VjcmV0LXBhc3N3b3Jk@example.com:8388#Singapore" },

    { "hysteria2",
      "hysteria2://secret-password@example.com:443?sni=example.com&obfs=salamander&obfs-password=qwerty&mport=20000-30000#Istanbul%20TR" },

    { "tuic",
      "tuic://11111111-2222-3333-4444-555555555555:secret@example.com:443?congestion_control=bbr&udp_relay_mode=native&sni=example.com#Seoul" },

    { "wireguard",
      "wireguard://aGVsbG8td29ybGQtcHJpdmF0ZS1rZXktc3RyaW5nMTI%3D@example.com:51820"
      "?publickey=aGVsbG8td29ybGQtcHVibGljLWtleS1zdHJpbmcxMjM%3D&address=10.7.0.2%2F32&reserved=1,2,3&mtu=1408#Amsterdam" },
};

bool writeFile(const std::string& path, const std::string& text) {
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return (bool)out;
}

Routing routingFor(AppMode mode) {
    Routing routing;
    routing.appMode = mode;
    routing.apps = { "telegram.exe", "chrome.exe" };
    routing.directDomains = { "https://Gosuslugi.RU/path", "*.sber.ru" };
    routing.bypassBanks = true;
    routing.directRuSites = true;
    return routing;
}

int dump(const std::string& dir, const std::string& name, const std::string& link,
         const Routing& routing, const Settings& settings) {
    Server server;
    if (!LinkParser::toServer(link, "", server)) {
        printf("  %-22s ССЫЛКА НЕ РАЗОБРАНА\n", name.c_str());
        return 1;
    }
    try {
        Proxy proxy = LinkParser::toProxy(link);
        std::string config = SingBoxConfig::build(proxy, routing, settings);
        std::string path = dir + "\\" + name + ".json";
        if (!writeFile(path, config)) {
            printf("  %-22s НЕ ЗАПИСАЛСЯ ФАЙЛ\n", name.c_str());
            return 1;
        }
        printf("  %-22s %s:%d  флаг %s  ->  %s.json\n", name.c_str(),
               server.host.c_str(), server.port, server.flag.c_str(), name.c_str());
        return 0;
    } catch (const NotSupportedError& e) {
        printf("  %-22s не поддерживается: %s\n", name.c_str(), e.what());
        return 0;
    } catch (const std::exception& e) {
        printf("  %-22s ОШИБКА: %s\n", name.c_str(), e.what());
        return 1;
    }
}

} // namespace

// проверка подписки живьем: что ответил сервер и сколько серверов удалось разобрать
static int checkSubscription(const std::string& url) {
    printf("тянем подписку...\n");
    http::Response response = http::get(url);
    printf("  код ответа:   %d\n", response.status);
    if (!response.error.empty()) printf("  ошибка:       %s\n", response.error.c_str());
    printf("  тело:         %zu байт\n", response.body.size());

    std::string head = response.body.substr(0, 90);
    for (char& c : head) {
        if (c == '\r' || c == '\n') c = ' ';
    }
    printf("  начало тела:  %s\n", head.c_str());

    const std::string& title = response.headers["profile-title"];
    const std::string& info = response.headers["subscription-userinfo"];
    if (!title.empty()) printf("  profile-title: %s\n", title.c_str());
    if (!info.empty()) printf("  трафик:        %s\n", info.c_str());

    if (!response.ok) return 1;

    std::vector<Server> servers = LinkParser::parseSubscription(response.body, "test");
    printf("  разобрано серверов: %zu\n", servers.size());
    if (servers.empty()) return 1;

    size_t bad = 0;
    for (const Server& s : servers) {
        if (!s.unsupported.empty()) bad++;
    }
    printf("  из них ядру не по зубам: %zu\n", bad);

    // пингуем первые несколько, заодно проверяем что сам пинг работает
    size_t show = servers.size() < 8 ? servers.size() : 8;
    std::vector<Server> sample(servers.begin(), servers.begin() + show);

    std::mutex guard;
    std::map<std::string, int> results;
    printf("\n  пинг первых %zu:\n", show);
    ping::all(sample, [&](std::string id, int ms) {
        std::lock_guard<std::mutex> lock(guard);
        results[id] = ms;
    });

    for (const Server& s : sample) {
        std::string ms = "нет ответа";
        auto it = results.find(s.id);
        if (it != results.end()) {
            if (it->second == ping::kUdp) ms = "udp";
            else if (it->second > 0) ms = std::to_string(it->second) + " мс";
        }
        printf("    %s %-30s %-12s %-24s %s\n", s.flag.c_str(), s.name.c_str(),
               protocolLabel(s.protocol), (s.host + ":" + std::to_string(s.port)).c_str(), ms.c_str());
    }
    return 0;
}

// поднимает настоящий туннель и гасит его. нужны права администратора
static int checkVpn(const std::string& link, int seconds) {
    paths::ensureDataDir();
    applog::open();

    Server server;
    if (!LinkParser::toServer(link, "", server)) {
        printf("ссылку не разобрать\n");
        return 2;
    }
    if (!server.unsupported.empty()) {
        printf("такой сервер ядро не умеет: %s\n", server.unsupported.c_str());
        return 2;
    }
    printf("сервер: %s  %s:%d\n", server.name.c_str(), server.host.c_str(), server.port);

    VpnCore core;
    Routing routing;
    Settings settings;

    if (!core.start(server, routing, settings)) {
        printf("не поднялось: %s\n", core.message().c_str());
        return 1;
    }
    printf("ядро запущено, держим %d секунд\n", seconds);

    TrafficStats stats;
    stats.start();
    for (int i = 0; i < seconds; i++) {
        Sleep(1000);
        if (core.state() != VpnState::Running) {
            printf("ядро отвалилось на %d секунде: %s\n", i + 1, core.message().c_str());
            stats.stop();
            core.stop();
            return 1;
        }
    }
    printf("прокачано: вниз %lld, вверх %lld\n", stats.downTotal(), stats.upTotal());
    stats.stop();

    printf("останавливаем...\n");
    ULONGLONG started = GetTickCount64();
    core.stop();
    printf("остановилось за %llu мс\n", GetTickCount64() - started);

    applog::close();
    return 0;
}

// проверяет только запуск и остановку ядра. tun не поднимается, маршруты системы не трогаются,
// поэтому и права администратора не нужны
static int checkStop() {
    paths::ensureDataDir();

    // в свежем клоне ядра рядом нет, тогда просто пропускаем тест
    if (GetFileAttributesW(paths::coreExe().c_str()) == INVALID_FILE_ATTRIBUTES) {
        printf("рядом нет core/sing-box.exe, пропускаем\n");
        return 2;
    }
    applog::open();

    std::string config =
        R"({"log":{"level":"warn","timestamp":false},)"
        R"("dns":{"servers":[{"type":"local","tag":"local"}],"final":"local"},)"
        R"("inbounds":[{"type":"mixed","tag":"in","listen":"127.0.0.1","listen_port":18080}],)"
        R"("outbounds":[{"type":"direct","tag":"direct"}],)"
        R"("route":{"final":"direct","default_domain_resolver":"local"}})";

    VpnCore core;
    if (!core.startWithConfig(config)) {
        printf("ядро не запустилось: %s\n", core.message().c_str());
        return 1;
    }
    printf("ядро работает\n");
    Sleep(2000);
    if (core.state() != VpnState::Running) {
        printf("ядро умерло само: %s\n", core.message().c_str());
        core.stop();
        return 1;
    }

    ULONGLONG started = GetTickCount64();
    core.stop();
    ULONGLONG spent = GetTickCount64() - started;

    bool graceful = core.stoppedGracefully();
    std::string verdict = "остановка заняла " + std::to_string(spent) + " мс: " +
                          (graceful ? "закрылось само по ctrl+break" : "пришлось прибивать");
    printf("%s\n", verdict.c_str());

    // на время ctrl+break консоль отцепляется, поэтому дублируем вывод в файл
    std::ofstream out(paths::dataDir() + L"\\stoptest.txt", std::ios::binary | std::ios::trunc);
    out << verdict << "\n";

    applog::close();
    return graceful ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--stoptest") {
        return checkStop();
    }
    if (argc >= 3 && std::string(argv[1]) == "--sub") {
        return checkSubscription(argv[2]);
    }
    if (argc >= 3 && std::string(argv[1]) == "--vpn") {
        return checkVpn(argv[2], argc >= 4 ? atoi(argv[3]) : 8);
    }
    if (argc < 2) {
        printf("укажи папку куда складывать конфиги, --sub <подписка> или --vpn <ссылка> [секунды]\n");
        return 2;
    }
    std::string dir = argv[1];
    Settings settings;
    int bad = 0;

    printf("все протоколы, режим \"все через впн\":\n");
    for (const Sample& s : kSamples) {
        bad += dump(dir, s.file, s.link, routingFor(AppMode::All), settings);
    }

    printf("\nразделение трафика по процессам:\n");
    bad += dump(dir, "split-only", kSamples[0].link, routingFor(AppMode::Only), settings);
    bad += dump(dir, "split-except", kSamples[0].link, routingFor(AppMode::Except), settings);

    printf("\nвключенный ipv6:\n");
    Settings withIpv6;
    withIpv6.ipv6 = true;
    bad += dump(dir, "ipv6", kSamples[0].link, routingFor(AppMode::All), withIpv6);

    printf("\nне разобралось: %d\n", bad);
    return bad == 0 ? 0 : 1;
}
