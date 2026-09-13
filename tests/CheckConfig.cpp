// Утилита для отладки: гоняет ссылки через парсер, складывает готовые конфиги в файлы
// и потом их можно скормить sing-box check. В релиз не идет.
//
//     vinyl_check <папка куда сложить конфиги>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/Errors.h"
#include "core/LinkParser.h"
#include "core/SingBoxConfig.h"
#include "data/Models.h"

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

    // base64 от {"v":"2","ps":"Tokyo","add":"example.com","port":"443","id":"1111...","aid":"0","net":"ws","host":"cdn.example.com","path":"/vm","tls":"tls"}
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

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("укажи папку куда складывать конфиги\n");
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
