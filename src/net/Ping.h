#pragma once

#include <functional>
#include <string>
#include <vector>

#include "data/Models.h"

namespace ping {

const int kTimeout = -1;   // не ответил
const int kUdp = -2;       // протокол по udp, обычным пингом не проверить

// миллисекунды до установки tcp соединения
int tcp(const std::string& host, int port, int timeoutMs = 2500);

// гоняет все серверы в несколько потоков, колбек зовется из чужого потока
void all(const std::vector<Server>& servers, const std::function<void(std::string, int)>& onResult);

} // namespace ping
