#pragma once

#include <map>
#include <string>

// простой синхронный GET на winhttp, зовем его только из рабочего потока
namespace http {

struct Response {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
    // заголовки которые просили, ключи в нижнем регистре
    std::map<std::string, std::string> headers;
};

Response get(const std::string& url);

} // namespace http
