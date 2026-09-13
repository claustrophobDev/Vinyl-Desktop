#include <windows.h>

#include "ui/MainWindow.h"
#include "app/Install.h"
#include "app/Log.h"
#include "app/Paths.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int cmdShow) {
    // второй экземпляр не запускаем, просто показываем уже открытое окно
    HANDLE once = CreateMutexW(nullptr, TRUE, L"VinylDesktopSingleInstance");
    if (once && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND prev = FindWindowW(L"VinylMainWindow", nullptr);
        if (prev) {
            ShowWindow(prev, SW_SHOW);
            SetForegroundWindow(prev);
        }
        return 0;
    }

    paths::ensureDataDir();
    applog::open();
    applog::line("запуск");

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;

    // ярлык и запись в системе делаем сами, чтобы программа находилась поиском винды
    install::ensureRegistered();

    MainWindow win;
    if (!win.create(inst)) {
        MessageBoxW(nullptr, L"Не удалось создать окно", L"Vinyl", MB_ICONERROR);
        return 1;
    }
    win.show(cmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    applog::line("выход");
    applog::close();
    return 0;
}
