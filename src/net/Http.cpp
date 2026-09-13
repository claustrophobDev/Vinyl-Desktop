#include "net/Http.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

#include "core/Text.h"

namespace http {
namespace {

// панели (marzban, remnawave, 3x-ui) с таким агентом отдают обычный список ссылок,
// а не свою html-страничку
const wchar_t* kUserAgent = L"v2rayNG/1.10.5";

std::string headerValue(HINTERNET request, const wchar_t* name) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) return std::string();

    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, buffer.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
        return std::string();
    }
    while (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
    return text::narrow(buffer);
}

} // namespace

Response get(const std::string& url) {
    Response out;
    std::wstring wide = text::wide(text::trim(url));

    // хвост после решетки это название подписки, серверу он не нужен.
    // если его не отрезать, он уезжает прямо в путь запроса и панель отвечает 404
    size_t hash = wide.find(L'#');
    if (hash != std::wstring::npos) wide = wide.substr(0, hash);

    if (wide.empty()) {
        out.error = "Пустая ссылка";
        return out;
    }

    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    wchar_t extra[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    // без своего буфера строка запроса просто теряется, а в подписках сплошь и рядом ?token=...
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = (DWORD)(sizeof(extra) / sizeof(extra[0]));

    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
        out.error = "Не получилось разобрать ссылку";
        return out;
    }

    HINTERNET session = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        out.error = "Не поднялась сетевая сессия";
        return out;
    }

    WinHttpSetTimeouts(session, 15000, 15000, 20000, 20000);

    // панели любят перекидывать на другой адрес, иногда с https на http.
    // по умолчанию winhttp такие переходы не делает, а телефон их отрабатывал руками
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        out.error = "Не получилось соединиться с сервером";
        return out;
    }

    // путь и строка запроса идут вместе, иначе сервер не поймет какую подписку отдавать
    std::wstring target = path;
    target += extra;

    // winhttp по умолчанию не шлет Accept, а некоторые панели без него отдают html вместо списка
    static const wchar_t* kAccept[] = { L"*/*", nullptr };

    DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, kAccept, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        out.error = "Не получилось отправить запрос";
        return out;
    }

    bool sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE;
    if (sent) sent = WinHttpReceiveResponse(request, nullptr) != FALSE;

    if (!sent) {
        DWORD code = GetLastError();
        out.error = "Сервер не ответил, ошибка " + std::to_string(code);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return out;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    out.status = (int)status;

    // эти заголовки панели кладут рядом с подпиской: остаток трафика и название
    out.headers["subscription-userinfo"] = headerValue(request, L"subscription-userinfo");
    out.headers["profile-title"] = headerValue(request, L"profile-title");

    std::string body;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::vector<char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        body.append(chunk.data(), read);
        if (body.size() > 8 * 1024 * 1024) break;   // подписка столько весить не может
    }
    out.body = body;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (out.status < 200 || out.status > 299) {
        out.error = "Сервер подписки ответил кодом " + std::to_string(out.status);
        return out;
    }
    out.ok = true;
    return out;
}

} // namespace http
