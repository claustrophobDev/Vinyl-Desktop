#include "net/Ping.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace ping {
namespace {

struct Winsock {
    Winsock() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~Winsock() { WSACleanup(); }
};

void ensureWinsock() {
    static Winsock once;
    (void)once;
}

} // namespace

int tcp(const std::string& host, int port, int timeoutMs) {
    ensureWinsock();

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* list = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &list) != 0 || list == nullptr) {
        return kTimeout;
    }

    auto started = std::chrono::steady_clock::now();
    int result = kTimeout;

    for (addrinfo* item = list; item != nullptr; item = item->ai_next) {
        SOCKET sock = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (sock == INVALID_SOCKET) continue;

        // неблокирующий коннект, иначе таймаут будет системный и очень долгий
        u_long nonBlocking = 1;
        ioctlsocket(sock, FIONBIO, &nonBlocking);
        connect(sock, item->ai_addr, (int)item->ai_addrlen);

        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(sock, &writable);
        timeval wait = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };

        if (select(0, nullptr, &writable, nullptr, &wait) > 0) {
            int error = 0;
            int size = sizeof(error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &size);
            if (error == 0) {
                auto spent = std::chrono::steady_clock::now() - started;
                int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(spent).count();
                result = ms < 1 ? 1 : ms;
            }
        }
        closesocket(sock);
        if (result != kTimeout) break;
    }

    freeaddrinfo(list);
    return result;
}

void all(const std::vector<Server>& servers, const std::function<void(std::string, int)>& onResult) {
    std::atomic<size_t> next{ 0 };
    unsigned threads = 16;
    if (servers.size() < threads) threads = (unsigned)servers.size();
    if (threads == 0) return;

    std::vector<std::thread> workers;
    for (unsigned i = 0; i < threads; i++) {
        workers.emplace_back([&] {
            for (;;) {
                size_t index = next.fetch_add(1);
                if (index >= servers.size()) return;
                const Server& server = servers[index];
                if (!server.unsupported.empty()) {
                    onResult(server.id, kTimeout);
                    continue;
                }
                // hy2, tuic и wireguard живут на udp, на tcp они не ответят
                int ms = protocolIsUdp(server.protocol) ? kUdp : tcp(server.host, server.port);
                onResult(server.id, ms);
            }
        });
    }
    for (std::thread& worker : workers) worker.join();
}

} // namespace ping
