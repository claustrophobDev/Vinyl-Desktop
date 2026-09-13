#include "ui/Format.h"

#include <windows.h>

#include <cstdio>

namespace fmt {
namespace {

// в русском разделитель запятая, а swprintf всегда ставит точку
std::wstring comma(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if (c == L'.') c = L',';
    }
    return out;
}

std::wstring twoDigits(double value, const wchar_t* unit, int digits) {
    wchar_t buf[64];
    if (digits == 2) swprintf_s(buf, L"%.2f %s", value, unit);
    else swprintf_s(buf, L"%.1f %s", value, unit);
    return comma(buf);
}

} // namespace

long long nowMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value;
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    // из 1601 года в 1970, и из сотен наносекунд в миллисекунды
    return (long long)((value.QuadPart - 116444736000000000ull) / 10000ull);
}

std::wstring speed(long long bytesPerSecond) {
    if (bytesPerSecond >= 1048576) return twoDigits(bytesPerSecond / 1048576.0, L"МБ/с", 1);
    if (bytesPerSecond >= 1024) return std::to_wstring(bytesPerSecond / 1024) + L" КБ/с";
    return std::to_wstring(bytesPerSecond) + L" Б/с";
}

std::wstring bytes(long long value) {
    if (value >= 1073741824) return twoDigits(value / 1073741824.0, L"ГБ", 2);
    if (value >= 1048576) return twoDigits(value / 1048576.0, L"МБ", 1);
    if (value >= 1024) return std::to_wstring(value / 1024) + L" КБ";
    return std::to_wstring(value) + L" Б";
}

std::wstring duration(long long seconds) {
    if (seconds < 0) seconds = 0;
    long long h = seconds / 3600;
    long long m = (seconds % 3600) / 60;
    long long s = seconds % 60;
    wchar_t buf[32];
    if (h > 0) swprintf_s(buf, L"%lld:%02lld:%02lld", h, m, s);
    else swprintf_s(buf, L"%02lld:%02lld", m, s);
    return buf;
}

std::wstring ago(long long timestampMs) {
    if (timestampMs <= 0) return L"не обновлялась";
    long long minutes = (nowMs() - timestampMs) / 60000;
    if (minutes < 1) return L"только что";
    if (minutes < 60) return std::to_wstring(minutes) + L" мин назад";
    if (minutes < 24 * 60) return std::to_wstring(minutes / 60) + L" ч назад";
    return std::to_wstring(minutes / (24 * 60)) + L" дн назад";
}

std::wstring date(long long timestampMs) {
    if (timestampMs <= 0) return L"";
    // из миллисекунд обратно в виндовое время
    ULARGE_INTEGER value;
    value.QuadPart = (unsigned long long)timestampMs * 10000ull + 116444736000000000ull;
    FILETIME ft;
    ft.dwLowDateTime = value.LowPart;
    ft.dwHighDateTime = value.HighPart;

    SYSTEMTIME utc, local;
    if (!FileTimeToSystemTime(&ft, &utc)) return L"";
    SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);

    static const wchar_t* months[] = {
        L"янв", L"фев", L"мар", L"апр", L"мая", L"июн",
        L"июл", L"авг", L"сен", L"окт", L"ноя", L"дек"
    };
    int index = local.wMonth - 1;
    if (index < 0 || index > 11) index = 0;

    wchar_t buf[48];
    swprintf_s(buf, L"%d %s %d", local.wDay, months[index], local.wYear);
    return buf;
}

std::wstring serversWord(size_t count) {
    size_t mod100 = count % 100;
    size_t mod10 = count % 10;
    if (mod100 >= 11 && mod100 <= 14) return L"серверов";
    if (mod10 == 1) return L"сервер";
    if (mod10 >= 2 && mod10 <= 4) return L"сервера";
    return L"серверов";
}

std::wstring appsWord(size_t count) {
    size_t mod100 = count % 100;
    size_t mod10 = count % 10;
    if (mod100 >= 11 && mod100 <= 14) return L"программ";
    if (mod10 == 1) return L"программа";
    if (mod10 >= 2 && mod10 <= 4) return L"программы";
    return L"программ";
}

} // namespace fmt
