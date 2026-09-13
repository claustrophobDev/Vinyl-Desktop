#pragma once

#include <windows.h>

#include <mutex>
#include <string>
#include <vector>

#include "app/AppModel.h"
#include "app/Processes.h"
#include "ui/Painter.h"
#include "ui/Widgets.h"
#include "vpn/Core.h"
#include "vpn/Stats.h"

enum class Tab { Home = 0, Servers = 1, Routes = 2, Settings = 3 };

// что можно нажать. динамические списки идут диапазонами, id = база + номер строки
enum Action {
    ActNone = -1,
    ActMinimize = 1,
    ActCloseWindow,
    ActConnect,
    ActNavBase,          // +0..3 вкладки
    ActPaste = 20,
    ActPingAll,
    ActRefreshAll,
    ActAddField,
    ActAddSubmit,
    ActAutoSelect,
    ActServerCard,
    ActReconnect,
    ActModeBase = 40,    // +0..2 режимы приложений
    ActBypassBanks,
    ActDirectRu,
    ActDomainField,
    ActDomainAdd,
    ActAppSearch,
    ActDnsBase = 60,     // +0..2
    ActIpv6,
    ActAutoConnect,
    ActAutostart,
    ActLogRefresh,
    ActLogCopy,
    ActLogFolder,
    ActGithub,

    ActServerBase = 1000,
    ActServerDelete = 2000,
    ActSubRefresh = 3000,
    ActSubDelete = 4000,
    ActAppBase = 5000,
    ActDomainBase = 6000,
};

// куда сейчас печатает пользователь
enum class EditTarget { None, Add, Domain, AppSearch };

class MainWindow {
public:
    bool create(HINSTANCE inst);
    void show(int cmdShow);
    HWND hwnd() const { return hwnd_; }

    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l);
    static LRESULT CALLBACK editProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data);

private:
    LRESULT onMessage(UINT msg, WPARAM w, LPARAM l);

    // отрисовка
    void render();
    void drawChrome(D2D1_SIZE_F size);
    void drawSidebar(D2D1_SIZE_F size);
    void drawToast(D2D1_SIZE_F size);

    // экраны, лежат в Screens.cpp
    void drawHome(D2D1_RECT_F area);
    void drawServers(D2D1_RECT_F area);
    void drawRoutes(D2D1_RECT_F area);
    void drawSettings(D2D1_RECT_F area);
    void drawDisc(D2D1_POINT_2F center, float radius);
    void drawTrafficTile(D2D1_RECT_F r, const wchar_t* icon, const std::wstring& label,
                         const std::wstring& value, const std::wstring& total, const D2D1_COLOR_F& tint);
    void drawServerRow(D2D1_RECT_F r, const Server& server, int index, bool selected, int ping);
    void drawEmptyServers(D2D1_RECT_F area);

    // действия
    void onClick(int id);
    void toggleVpn();
    void applyVpnState();
    void pasteFromClipboard();
    void copyLog();
    void reloadLog();
    void setTab(Tab tab);
    void say(const std::string& message);

    // поле ввода
    void openEditor(EditTarget target, D2D1_RECT_F rect, const wchar_t* text);
    void closeEditor(bool commit);
    std::wstring editorText() const;
    void layoutEditor();

    // трей
    void addTray();
    void removeTray();
    void trayMenu();
    void updateTrayTip();

    void updateDpi();
    D2D1_RECT_F contentArea(D2D1_SIZE_F size) const;
    float titleBarH() const { return painter_.dp(46.f); }
    float sidebarW() const { return painter_.dp(212.f); }
    float scroll() const { return scroll_[(int)tab_]; }
    void setScroll(float value);
    std::wstring statusText() const;

    HWND hwnd_ = nullptr;
    HWND edit_ = nullptr;
    HFONT editFont_ = nullptr;
    HINSTANCE inst_ = nullptr;
    UINT dpi_ = 96;

    Painter painter_;
    Ui ui_;
    AppModel model_;
    VpnCore core_;
    TrafficStats stats_;

    Tab tab_ = Tab::Home;
    float scroll_[4] = { 0, 0, 0, 0 };
    float contentHeight_ = 0.f;
    float angle_ = 0.f;
    float glow_ = 0.f;

    VpnState vpn_ = VpnState::Stopped;
    std::wstring vpnError_;

    EditTarget editTarget_ = EditTarget::None;
    D2D1_RECT_F editRect_ = {};
    std::wstring appSearch_;

    std::vector<processes::Item> apps_;
    std::vector<std::wstring> logLines_;

    mutable std::mutex toastMutex_;
    std::wstring toast_;
    DWORD toastUntil_ = 0;

    bool trayAdded_ = false;
    bool tracking_ = false;
    bool logOpen_ = false;
};
