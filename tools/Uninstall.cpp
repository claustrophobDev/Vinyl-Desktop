// убирает ярлык и строчку в "программах и компонентах".
// папку не трогаем: запущенный exe сам себя все равно не удалит

#include <windows.h>
#include <objbase.h>

#include "app/Install.h"
#include "app/Paths.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // пока Vinyl запущен, ядро может держать туннель, поэтому сначала просим закрыть
    HANDLE running = OpenMutexW(SYNCHRONIZE, FALSE, L"VinylDesktopSingleInstance");
    if (running != nullptr) {
        CloseHandle(running);
        MessageBoxW(nullptr, L"Сначала закрой Vinyl через меню в трее, потом запусти удаление заново.",
                    L"Vinyl", MB_ICONWARNING);
        return 1;
    }

    int answer = MessageBoxW(nullptr,
                             L"Убрать Vinyl из списка программ и удалить ярлык из меню пуск?\n\n"
                             L"Папку с программой и настройками надо будет удалить вручную.",
                             L"Удаление Vinyl", MB_ICONQUESTION | MB_YESNO);
    if (answer != IDYES) return 0;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = install::unregisterApp();
    CoUninitialize();

    if (ok) {
        std::wstring text = L"Готово. Осталось удалить папку:\n\n" + paths::exeDir();
        MessageBoxW(nullptr, text.c_str(), L"Vinyl", MB_ICONINFORMATION);
    } else {
        MessageBoxW(nullptr, L"Не получилось убрать запись из системы.", L"Vinyl", MB_ICONERROR);
    }
    return ok ? 0 : 1;
}
