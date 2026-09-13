#pragma once

#include <windows.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "app/AppModel.h"
#include "app/Processes.h"
#include "ui/Actions.h"
#include "ui/Painter.h"
#include "ui/Widgets.h"
#include "ui/views/HomeView.h"
#include "ui/views/RoutesView.h"
#include "ui/views/ServersView.h"
#include "ui/views/SettingsView.h"
#include "vpn/Core.h"
#include "vpn/Stats.h"

enum class Tab { Home = 0, Servers = 1, Routes = 2, Settings = 3 };

// куда сейчас печатает пользователь
enum class EditTarget { None, Add, Domain, AppSearch };

// Окно: рамка, боковое меню, трей, поле ввода и разбор кликов.
// Сами экраны рисуются отдельными классами из ui/views.
class MainWindow {
public:
    bool create(HINSTANCE inst);
    void show(int cmdShow);
    HWND hwnd() const { return hwnd_; }

    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l);
    static LRESULT CALLBACK editProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data);

private:
    LRESULT onMessage(UINT msg, WPARAM w, LPARAM l);

    void render();
    void drawChrome(D2D1_SIZE_F size);
    void drawSidebar(D2D1_SIZE_F size);
    void drawToast(D2D1_SIZE_F size);
    ViewContext makeContext();

    void onClick(int id);
    void toggleVpn();
    void applyVpnState();
    void pasteFromClipboard();
    void copyLog();
    void reloadLog();
    void setTab(Tab tab);
    void say(const std::string& message);

    void openEditor(EditTarget target, D2D1_RECT_F rect, const wchar_t* text);
    void closeEditor(bool commit);
    std::wstring editorText() const;
    void layoutEditor();

    void addTray();
    void removeTray();
    void trayMenu();
    void updateTrayTip();

    // состояние берем из модели на изменение, а не каждый кадр
    void refreshCache();

    void updateDpi();
    D2D1_RECT_F contentArea(D2D1_SIZE_F size) const;
    float titleBarH() const { return painter_.dp(46.f); }
    float sidebarW() const { return painter_.dp(212.f); }
    float scroll() const { return scroll_[(int)tab_]; }
    void setScroll(float value);

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

    HomeView homeView_;
    ServersView serversView_;
    RoutesView routesView_;
    SettingsView settingsView_;

    AppState cached_;
    std::map<std::string, int> cachedPings_;
    std::set<std::string> cachedRefreshing_;
    Server cachedServer_;
    bool hasCachedServer_ = false;
    bool cachedPinging_ = false;
    bool cachedAutostart_ = false;

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
};
