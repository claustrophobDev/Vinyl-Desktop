#pragma once
#include <string>

// все пути считаем от папки с exe, чтобы версия была портативной
// ничего не пишем в AppData и прочие места, кроме ярлыка при установке
namespace paths {

std::wstring exeDir();
std::wstring exePath();

// файлы рядом с exe
std::wstring dataDir();      // папка data, там состояние и логи
std::wstring stateFile();    // сохраненные серверы и настройки
std::wstring logFile();      // лог приложения
std::wstring coreLogFile();  // лог ядра
std::wstring configFile();   // сгенерированный конфиг для sing-box
std::wstring coreDir();      // папка core, рядом с sing-box лежит wintun.dll
std::wstring coreExe();      // sing-box.exe

void ensureDataDir();

} // namespace paths
