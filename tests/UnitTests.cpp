// тесты на логику: строки, ссылки, конфиг, флаги, состояние

#include <cstdio>
#include <string>
#include <vector>

#include "core/Errors.h"
#include "core/Flags.h"
#include "core/Json.h"
#include "core/LinkParser.h"
#include "core/ProxyUri.h"
#include "core/SingBoxConfig.h"
#include "core/Text.h"
#include "data/Storage.h"
#include "ui/Format.h"

namespace {

int gChecks = 0;
int gFailed = 0;
const char* gGroup = "";

void group(const char* name) {
    gGroup = name;
    printf("\n%s\n", name);
}

void fail(const char* what, int line, const std::string& got, const std::string& want) {
    gFailed++;
    printf("  ПРОВАЛ  %s (строка %d)\n", what, line);
    if (!want.empty()) printf("          ждали:   %s\n", want.c_str());
    if (!got.empty()) printf("          вышло:   %s\n", got.c_str());
}

void check(bool ok, const char* what, int line) {
    gChecks++;
    if (!ok) fail(what, line, "", "");
}

void equal(const std::string& got, const std::string& want, const char* what, int line) {
    gChecks++;
    if (got != want) fail(what, line, "\"" + got + "\"", "\"" + want + "\"");
}

void equal(long long got, long long want, const char* what, int line) {
    gChecks++;
    if (got != want) fail(what, line, std::to_string(got), std::to_string(want));
}

void equal(const std::wstring& got, const std::wstring& want, const char* what, int line) {
    gChecks++;
    if (got != want) fail(what, line, text::narrow(got), text::narrow(want));
}

#define CHECK(cond) check((cond), #cond, __LINE__)
#define EQ(got, want) equal((got), (want), #got, __LINE__)

// --- строки ---

void testText() {
    group("строки");

    EQ(text::urlDecode("a%20b"), "a b");
    // плюс должен остаться плюсом, иначе ломаются пароли
    EQ(text::urlDecode("a+b"), "a+b");
    EQ(text::urlDecode("%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82"), "Привет");
    EQ(text::urlDecode("100%"), "100%");
    EQ(text::urlDecode("%zz"), "%zz");

    std::string decoded;
    CHECK(text::fromBase64("aGVsbG8=", decoded));
    EQ(decoded, "hello");
    CHECK(text::fromBase64("aGVsbG8", decoded));
    EQ(decoded, "hello");
    CHECK(text::fromBase64("YS1iX2M=", decoded));
    CHECK(text::fromBase64("a GVsb G8=", decoded));
    EQ(decoded, "hello");
    CHECK(!text::fromBase64("###", decoded));
    CHECK(!text::fromBase64("", decoded));
    // пять символов это четыре плюс один, такого в base64 не бывает
    CHECK(!text::fromBase64("aGVsb", decoded));

    EQ(text::sha1Short("abc"), "a9993e364706816aba3e");
    EQ(text::sha1Short("abc").size(), 20);

    CHECK(text::isIp("1.2.3.4"));
    CHECK(text::isIp("255.255.255.255"));
    CHECK(!text::isIp("1.2.3"));
    CHECK(!text::isIp("example.com"));
    CHECK(text::isIp("2001:db8::1"));
    CHECK(!text::isIp("a.b.c.d"));

    EQ(text::trim("  привет \t\r\n"), "привет");
    EQ(text::lower("ABC-def"), "abc-def");
    EQ(text::before("a=b", "=", "нет"), "a");
    EQ(text::before("ab", "=", "весь"), "весь");
    EQ(text::after("a=b=c", "=", ""), "b=c");
    EQ(text::beforeLast("a@b@c", '@'), "a@b");
    EQ(text::afterLast("a@b@c", '@'), "c");
    EQ(text::afterLast("нет собаки", '@'), "нет собаки");

    std::vector<std::string> parts = text::split("a,,b", ',');
    EQ((long long)parts.size(), 3);
    EQ(parts[1], "");
    parts = text::split("a,,b", ',', true);
    EQ((long long)parts.size(), 2);

    EQ(text::toInt("42", -1), 42);
    EQ(text::toInt("-7", 0), -7);
    EQ(text::toInt("", 5), 5);
    EQ(text::toInt("12abc", 5), 5);
    EQ(text::toLong("999999999999999999999", 5), 5);
}

// --- разбор ссылки ---

void testProxyUri() {
    group("разбор ссылки");

    ProxyUri u = ProxyUri::parse(
        "vless://uuid-1@example.com:443?type=ws&Security=tls&path=%2Fa%20b#%F0%9F%87%A9%F0%9F%87%AA%20Germany");
    EQ(u.scheme, "vless");
    EQ(u.userInfo, "uuid-1");
    EQ(u.host, "example.com");
    EQ((long long)u.port, 443);
    EQ(u.q("type"), "ws");
    // регистр ключа не должен иметь значения
    EQ(u.q("security"), "tls");
    EQ(u.q("path"), "/a b");
    CHECK(u.name.find("Germany") != std::string::npos);

    // пароль с собакой внутри: берем последнюю
    ProxyUri withAt = ProxyUri::parse("trojan://pa@ss@host.tld:8443");
    EQ(withAt.userInfo, "pa@ss");
    EQ(withAt.host, "host.tld");
    EQ((long long)withAt.port, 8443);

    ProxyUri v6 = ProxyUri::parse("vless://id@[2001:db8::1]:443");
    EQ(v6.host, "2001:db8::1");
    EQ((long long)v6.port, 443);

    ProxyUri noPort = ProxyUri::parse("hysteria2://pass@host.tld");
    EQ((long long)noPort.port, -1);

    EQ(u.qOr({ "нетутакого" }, "по умолчанию"), "по умолчанию");
    CHECK(!u.flag({ "insecure" }));
    CHECK(ProxyUri::parse("x://a@b:1?insecure=1").flag({ "insecure" }));
    CHECK(ProxyUri::parse("x://a@b:1?insecure=true").flag({ "insecure" }));

    bool threw = false;
    try {
        ProxyUri::parse("просто текст");
    } catch (const LinkError&) {
        threw = true;
    }
    CHECK(threw);
}

// --- флаги ---

void testFlags() {
    group("флаги");

    std::string flag;
    std::string clean;
    Flags::split("🇩🇪 Germany #1", flag, clean);
    EQ(flag, Flags::flagOf("DE"));
    EQ(clean, "Germany #1");

    // флага нет, но страна угадывается по слову
    Flags::split("Frankfurt 10G", flag, clean);
    EQ(flag, Flags::flagOf("DE"));
    EQ(clean, "Frankfurt 10G");

    Flags::split("Москва", flag, clean);
    EQ(flag, Flags::flagOf("RU"));

    // отдельно стоящий код страны
    Flags::split("Fast NL server", flag, clean);
    EQ(flag, Flags::flagOf("NL"));

    // NLx это не код страны, а кусок слова
    Flags::split("NLxxx", flag, clean);
    EQ(flag, "\xF0\x9F\x8C\x90");

    // такие коды это обычные английские слова, по ним не угадываем
    Flags::split("Come IN now", flag, clean);
    EQ(flag, "\xF0\x9F\x8C\x90");

    // обратный разбор эмодзи в код страны
    EQ(Flags::codeOf(Flags::flagOf("NL")), "NL");
    EQ(Flags::codeOf(Flags::flagOf("KZ")), "KZ");
    EQ(Flags::codeOf("\xF0\x9F\x8C\x90"), "");
    EQ(Flags::codeOf(""), "");
}

// --- ссылки в конфиг ---

Json outboundOf(const std::string& link) {
    Proxy proxy = LinkParser::toProxy(link);
    return proxy.json;
}

void testLinkParser() {
    group("ссылки в outbound");

    Json vless = outboundOf(
        "vless://11111111-2222-3333-4444-555555555555@example.com:443"
        "?type=tcp&security=reality&flow=xtls-rprx-vision&fp=chrome"
        "&pbk=0123456789abcdefghijklmnopqrstuvwxyzABCDEFG&sid=abcd&sni=www.microsoft.com#DE");
    EQ(vless["type"].get<std::string>(), "vless");
    EQ(vless["tag"].get<std::string>(), "proxy");
    EQ(vless["uuid"].get<std::string>(), "11111111-2222-3333-4444-555555555555");
    EQ(vless["flow"].get<std::string>(), "xtls-rprx-vision");
    EQ(vless["packet_encoding"].get<std::string>(), "xudp");
    EQ(vless["tls"]["server_name"].get<std::string>(), "www.microsoft.com");
    EQ(vless["tls"]["reality"]["short_id"].get<std::string>(), "abcd");
    // reality без utls ядро не запускает
    CHECK(vless["tls"]["utls"]["enabled"].get<bool>());
    EQ(vless["tls"]["utls"]["fingerprint"].get<std::string>(), "chrome");

    // ?ed=2048 внутри path это early data, у sing-box это отдельное поле
    Json ws = outboundOf("vless://id@example.com:443?type=ws&security=tls&path=%2Fvinyl%3Fed%3D2048&host=cdn.tld");
    EQ(ws["transport"]["type"].get<std::string>(), "ws");
    EQ(ws["transport"]["path"].get<std::string>(), "/vinyl");
    EQ(ws["transport"]["max_early_data"].get<long long>(), 2048);
    EQ(ws["transport"]["headers"]["Host"].get<std::string>(), "cdn.tld");

    // непонятный отпечаток заменяем на chrome, none значит utls не нужен
    Json oddFp = outboundOf("vless://id@example.com:443?security=tls&fp=нечто");
    EQ(oddFp["tls"]["utls"]["fingerprint"].get<std::string>(), "chrome");
    Json noFp = outboundOf("vless://id@example.com:443?security=tls&fp=none");
    CHECK(noFp["tls"].find("utls") == noFp["tls"].end());

    Json vmess = outboundOf(
        "vmess://eyJ2IjoiMiIsInBzIjoiVG9reW8iLCJhZGQiOiJleGFtcGxlLmNvbSIsInBvcnQiOiI0NDMiLCJpZCI6IjExMTExMTEx"
        "LTIyMjItMzMzMy00NDQ0LTU1NTU1NTU1NTU1NSIsImFpZCI6IjAiLCJuZXQiOiJ3cyIsImhvc3QiOiJjZG4uZXhhbXBsZS5jb20i"
        "LCJwYXRoIjoiL3ZtIiwidGxzIjoidGxzIn0=");
    EQ(vmess["type"].get<std::string>(), "vmess");
    EQ(vmess["alter_id"].get<long long>(), 0);
    EQ(vmess["security"].get<std::string>(), "auto");
    EQ(vmess["transport"]["type"].get<std::string>(), "ws");
    CHECK(vmess["tls"]["enabled"].get<bool>());

    // у trojan tls включен даже если про него не написали
    Json trojan = outboundOf("trojan://secret@example.com:443");
    EQ(trojan["password"].get<std::string>(), "secret");
    CHECK(trojan["tls"]["enabled"].get<bool>());

    Json ss = outboundOf("ss://aes-256-gcm:pass@example.com:8388");
    EQ(ss["method"].get<std::string>(), "aes-256-gcm");
    EQ(ss["password"].get<std::string>(), "pass");

    Json ssBase64 = outboundOf("ss://YWVzLTI1Ni1nY206c2VjcmV0@example.com:8388");
    EQ(ssBase64["password"].get<std::string>(), "secret");

    // mport=20000-30000 превращается в диапазон, обычный порт при этом убирается
    Json hy2 = outboundOf("hysteria2://pass@example.com:443?mport=20000-30000&obfs=salamander&obfs-password=q");
    CHECK(hy2.find("server_port") == hy2.end());
    EQ(hy2["server_ports"][0].get<std::string>(), "20000:30000");
    EQ(hy2["obfs"]["type"].get<std::string>(), "salamander");
    EQ(hy2["obfs"]["password"].get<std::string>(), "q");

    Json tuic = outboundOf("tuic://uuid-1:pass@example.com:443?udp_relay_mode=native");
    EQ(tuic["uuid"].get<std::string>(), "uuid-1");
    EQ(tuic["password"].get<std::string>(), "pass");
    EQ(tuic["congestion_control"].get<std::string>(), "bbr");
    EQ(tuic["udp_relay_mode"].get<std::string>(), "native");
    EQ(tuic["tls"]["alpn"][0].get<std::string>(), "h3");

    // wireguard в 1.14 живет в endpoints, а не в outbounds
    ParsedLink wg = LinkParser::parse(
        "wireguard://aGVsbG8td29ybGQtcHJpdmF0ZS1rZXktc3RyaW5nMTI%3D@example.com:51820"
        "?publickey=aGVsbG8td29ybGQtcHVibGljLWtleS1zdHJpbmcxMjM%3D&address=10.7.0.2%2F32&reserved=1,2,3&mtu=1400");
    CHECK(wg.isEndpoint);
    EQ(wg.outbound["type"].get<std::string>(), "wireguard");
    EQ(wg.outbound["mtu"].get<long long>(), 1400);
    EQ(wg.outbound["address"][0].get<std::string>(), "10.7.0.2/32");
    EQ(wg.outbound["peers"][0]["reserved"][1].get<long long>(), 2);

    // такое ядро не умеет: сервер показываем, но подключиться нельзя
    ParsedLink xhttp = LinkParser::parse("vless://id@example.com:443?type=xhttp&security=tls");
    CHECK(xhttp.outbound.is_null());
    CHECK(!xhttp.unsupported.empty());
    ParsedLink kcp = LinkParser::parse("vless://id@example.com:443?type=kcp");
    CHECK(!kcp.unsupported.empty());

    bool threw = false;
    try {
        LinkParser::parse("vless://@example.com:443");
    } catch (const LinkError&) {
        threw = true;
    }
    CHECK(threw);
}

void testSubscription() {
    group("подписка");

    std::string plain =
        "vless://id@a.tld:443?security=tls#Первый\n"
        "мусор\n"
        "trojan://pass@b.tld:443#Второй\n"
        "vless://id@a.tld:443?security=tls#Первый\n";

    std::vector<Server> servers = LinkParser::parseSubscription(plain, "sub-1");
    // третья строка это повтор первой, он должен отвалиться
    EQ((long long)servers.size(), 2);
    EQ(servers[0].name, "Первый");
    EQ(servers[0].subscriptionId, "sub-1");
    EQ(servers[1].protocol == Protocol::Trojan ? "trojan" : "нет", "trojan");

    // то же самое, но телом в base64
    std::string encoded = "dmxlc3M6Ly9pZEBhLnRsZDo0NDM/c2VjdXJpdHk9dGxzI0E=";
    std::vector<Server> fromBase64 = LinkParser::parseSubscription(encoded, "sub-2");
    EQ((long long)fromBase64.size(), 1);
    EQ(fromBase64[0].host, "a.tld");

    // id должен зависеть от подписки, иначе один сервер в двух подписках схлопнется
    Server one;
    Server two;
    CHECK(LinkParser::toServer("vless://id@a.tld:443?security=tls#A", "sub-1", one));
    CHECK(LinkParser::toServer("vless://id@a.tld:443?security=tls#A", "sub-2", two));
    CHECK(one.id != two.id);

    Server broken;
    CHECK(!LinkParser::toServer("вообще не ссылка", "", broken));
}

// --- конфиг для ядра ---

void testConfig() {
    group("конфиг");

    std::string domain;
    CHECK(SingBoxConfig::cleanDomain("https://Site.RU/path?x=1", domain));
    EQ(domain, "site.ru");
    CHECK(SingBoxConfig::cleanDomain("*.example.com", domain));
    EQ(domain, "example.com");
    CHECK(SingBoxConfig::cleanDomain("  Example.COM:443  ", domain));
    EQ(domain, "example.com");
    CHECK(!SingBoxConfig::cleanDomain("две слова", domain));
    CHECK(!SingBoxConfig::cleanDomain("   ", domain));

    Routing routing;
    routing.apps = { "Chrome.exe", "telegram.exe", "chrome.exe" };
    std::vector<std::string> viaProxy;
    std::vector<std::string> viaDirect;

    routing.appMode = AppMode::All;
    SingBoxConfig::splitByProcess(routing, viaProxy, viaDirect);
    CHECK(viaProxy.empty() && viaDirect.empty());

    routing.appMode = AppMode::Only;
    SingBoxConfig::splitByProcess(routing, viaProxy, viaDirect);
    // регистр приводим к нижнему и повторы убираем
    EQ((long long)viaProxy.size(), 2);
    EQ(viaProxy[0], "chrome.exe");
    CHECK(viaDirect.empty());

    routing.appMode = AppMode::Except;
    SingBoxConfig::splitByProcess(routing, viaProxy, viaDirect);
    EQ((long long)viaDirect.size(), 2);
    CHECK(viaProxy.empty());

    Proxy proxy = LinkParser::toProxy("vless://id@example.com:443?security=tls");
    Settings settings;

    Routing all;
    Json config = Json::parse(SingBoxConfig::build(proxy, all, settings));
    EQ(config["route"]["final"].get<std::string>(), "proxy");
    EQ(config["inbounds"][0]["type"].get<std::string>(), "tun");
    CHECK(config["inbounds"][0]["auto_route"].get<bool>());
    CHECK(config["inbounds"][0]["strict_route"].get<bool>());
    EQ(config["dns"]["servers"][0]["detour"].get<std::string>(), "proxy");
    EQ(config["dns"]["strategy"].get<std::string>(), "ipv4_only");
    // в 1.14 это поле обязательное, без него ядро не стартует
    CHECK(config["route"].find("default_domain_resolver") != config["route"].end());
    CHECK(config["experimental"]["clash_api"].find("external_controller") !=
          config["experimental"]["clash_api"].end());

    bool rejectsIpv6 = false;
    bool sniffs = false;
    for (const Json& rule : config["route"]["rules"]) {
        if (rule.value("ip_version", 0) == 6 && rule.value("action", "") == "reject") rejectsIpv6 = true;
        if (rule.value("action", "") == "sniff") sniffs = true;
    }
    CHECK(rejectsIpv6);
    CHECK(sniffs);

    // с включенным ipv6 резать его не надо
    Settings withIpv6;
    withIpv6.ipv6 = true;
    Json ipv6Config = Json::parse(SingBoxConfig::build(proxy, all, withIpv6));
    EQ(ipv6Config["dns"]["strategy"].get<std::string>(), "prefer_ipv4");
    for (const Json& rule : ipv6Config["route"]["rules"]) {
        CHECK(!(rule.value("ip_version", 0) == 6 && rule.value("action", "") == "reject"));
    }

    // "только выбранные": остальное идет мимо впн, значит финал уже direct
    Routing only;
    only.appMode = AppMode::Only;
    only.apps = { "telegram.exe" };
    Json onlyConfig = Json::parse(SingBoxConfig::build(proxy, only, settings));
    EQ(onlyConfig["route"]["final"].get<std::string>(), "direct");

    bool hasProcessRule = false;
    for (const Json& rule : onlyConfig["route"]["rules"]) {
        if (rule.find("process_name") == rule.end()) continue;
        if (rule["process_name"][0].get<std::string>() == "telegram.exe" &&
            rule.value("outbound", "") == "proxy") {
            hasProcessRule = true;
        }
    }
    CHECK(hasProcessRule);

    // wireguard уезжает в endpoints, а в outbounds остается только direct
    Proxy wg = LinkParser::toProxy(
        "wireguard://aGVsbG8td29ybGQtcHJpdmF0ZS1rZXktc3RyaW5nMTI%3D@example.com:51820"
        "?publickey=aGVsbG8td29ybGQtcHVibGljLWtleS1zdHJpbmcxMjM%3D&address=10.7.0.2%2F32");
    Json wgConfig = Json::parse(SingBoxConfig::build(wg, all, settings));
    CHECK(wgConfig.find("endpoints") != wgConfig.end());
    EQ((long long)wgConfig["outbounds"].size(), 1);
    EQ(wgConfig["outbounds"][0]["type"].get<std::string>(), "direct");

    // домены пользователя и готовые наборы попадают и в маршруты, и в dns
    Routing withDomains;
    withDomains.directRuSites = true;
    withDomains.directDomains = { "https://KinoPoisk.ru/film" };
    Json domainsConfig = Json::parse(SingBoxConfig::build(proxy, withDomains, settings));
    bool hasDomain = false;
    for (const Json& rule : domainsConfig["route"]["rules"]) {
        if (rule.find("domain_suffix") == rule.end()) continue;
        for (const Json& item : rule["domain_suffix"]) {
            if (item.get<std::string>() == "kinopoisk.ru") hasDomain = true;
        }
    }
    CHECK(hasDomain);
    CHECK(domainsConfig["dns"].find("rules") != domainsConfig["dns"].end());
}

// --- файл состояния ---

void testStorage() {
    group("состояние");

    AppState state;
    Server server;
    server.id = "abc";
    server.name = "Тест";
    server.protocol = Protocol::Hysteria2;
    server.host = "h.tld";
    server.port = 443;
    server.link = "hysteria2://pass@h.tld:443";
    server.flag = Flags::flagOf("NL");
    server.subscriptionId = "sub-1";
    state.servers.push_back(server);

    Subscription sub;
    sub.id = "sub-1";
    sub.name = "Моя подписка";
    sub.url = "https://example.com/sub";
    sub.totalBytes = 1024;
    state.subscriptions.push_back(sub);

    state.selectedId = "abc";
    state.routing.appMode = AppMode::Except;
    state.routing.apps = { "chrome.exe" };
    state.routing.directDomains = { "sber.ru" };
    state.routing.directRuSites = true;
    state.settings.dns = RemoteDns::Quad9;
    state.settings.ipv6 = true;
    state.settings.autoConnect = true;

    AppState back = Storage::fromJson(Storage::toJson(state));
    EQ((long long)back.servers.size(), 1);
    EQ(back.servers[0].name, "Тест");
    EQ(back.servers[0].flag, Flags::flagOf("NL"));
    CHECK(back.servers[0].protocol == Protocol::Hysteria2);
    EQ(back.servers[0].subscriptionId, "sub-1");
    EQ((long long)back.subscriptions.size(), 1);
    EQ(back.subscriptions[0].name, "Моя подписка");
    EQ(back.selectedId, "abc");
    CHECK(back.routing.appMode == AppMode::Except);
    EQ((long long)back.routing.apps.size(), 1);
    CHECK(back.routing.directRuSites);
    CHECK(back.settings.dns == RemoteDns::Quad9);
    CHECK(back.settings.ipv6);
    CHECK(back.settings.autoConnect);

    // битый файл не должен ронять программу, просто пустое состояние
    AppState broken = Storage::fromJson("{ это не json");
    CHECK(broken.servers.empty());
    EQ(broken.selectedId, "auto");

    // сервер без обязательных полей пропускаем, остальные читаем
    AppState partial = Storage::fromJson(
        R"({"servers":[{"id":"1"},{"id":"2","protocol":"VLESS","link":"vless://x@h:1"}]})");
    EQ((long long)partial.servers.size(), 1);
    EQ(partial.servers[0].id, "2");
}

// --- форматирование ---

void testFormat() {
    group("форматирование");

    EQ(fmt::bytes(512), L"512 Б");
    EQ(fmt::bytes(2048), L"2 КБ");
    EQ(fmt::bytes(5 * 1024 * 1024), L"5,0 МБ");
    EQ(fmt::speed(1024), L"1 КБ/с");
    EQ(fmt::duration(59), L"00:59");
    EQ(fmt::duration(61), L"01:01");
    EQ(fmt::duration(3661), L"1:01:01");

    EQ(fmt::serversWord(1), L"сервер");
    EQ(fmt::serversWord(2), L"сервера");
    EQ(fmt::serversWord(5), L"серверов");
    EQ(fmt::serversWord(11), L"серверов");
    EQ(fmt::serversWord(21), L"сервер");
    EQ(fmt::serversWord(122), L"сервера");
}

} // namespace

int main() {
    testText();
    testProxyUri();
    testFlags();
    testLinkParser();
    testSubscription();
    testConfig();
    testStorage();
    testFormat();

    printf("\n----------------------------------------\n");
    if (gFailed == 0) {
        printf("всё хорошо: %d проверок\n", gChecks);
        return 0;
    }
    printf("провалено %d из %d проверок\n", gFailed, gChecks);
    return 1;
}
