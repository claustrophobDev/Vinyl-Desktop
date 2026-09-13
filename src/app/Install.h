#pragma once

#include <string>

// ярлык в меню пуск и строчка в "программах и компонентах", саму папку никуда не копируем
namespace install {

// отсюда же сборщик берет номер для имени папки, так что менять версию надо тут
inline const char* kVersion = "1.0.0";
inline const wchar_t* kDisplayName = L"Vinyl";

bool isRegistered();
// тихо, без ошибок наружу: не прописалось так не прописалось, программа все равно работает
void ensureRegistered();

bool registerApp();
bool unregisterApp();

std::wstring shortcutPath();

} // namespace install
