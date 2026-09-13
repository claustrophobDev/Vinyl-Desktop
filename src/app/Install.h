#pragma once

#include <string>

// после первого запуска программа сама прописывается в систему:
// ярлык в меню пуск (чтобы находилась поиском винды) и запись в "программы и компоненты"
// сама папка никуда не копируется, она и так портативная
namespace install {

inline const char* kVersion = "1.0";
inline const wchar_t* kDisplayName = L"Vinyl";

bool isRegistered();
// тихо, без ошибок наружу: не прописалось так не прописалось, программа все равно работает
void ensureRegistered();

bool registerApp();
bool unregisterApp();

std::wstring shortcutPath();

} // namespace install
