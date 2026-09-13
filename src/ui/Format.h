#pragma once

#include <string>

// те же форматы что и на телефоне, чтобы цифры выглядели одинаково
namespace fmt {

std::wstring speed(long long bytesPerSecond);   // 12,4 МБ/с
std::wstring bytes(long long value);            // 1,25 ГБ
std::wstring duration(long long seconds);       // 1:02:33
std::wstring ago(long long timestampMs);        // 5 мин назад
std::wstring date(long long timestampMs);       // 3 мар 2026
std::wstring serversWord(size_t count);         // сервер / сервера / серверов
std::wstring appsWord(size_t count);

long long nowMs();

} // namespace fmt
