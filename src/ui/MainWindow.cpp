#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include "app/Autostart.h"
#include "app/Log.h"
#include "app/Paths.h"
#include "core/Text.h"
#include "ui/Format.h"

#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <fstream>

// в старых sdk этих констант нет
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static const wchar_t* kClassName = L"VinylMainWindow";
static const UINT WM_VINYL_TRAY = WM_APP + 1;
static const UINT WM_VINYL_STATE = WM_APP + 2;
static const UINT WM_VINYL_MODEL = WM_APP + 3;
static const UINT kTrayId = 1;
static const UINT kTickTimer = 1;
static const int kEditId = 4100;

struct NavItem { const wchar_t* title; const wchar_t* icon; };
static const NavItem kNav[4] = {
    { L"Главная",   glyph::play },
    { L"Серверы",   glyph::list },
    { L"Маршруты",  glyph::routes },
    { L"Настройки", glyph::settings },
};

bool MainWindow::create(HINSTANCE inst) {
    inst_ = inst;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &MainWindow::wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = kClassName;
    wc.hbrBackground = nullptr;
    if (!RegisterClassExW(&wc)) return false;

    // рамку рисуем сами, но окно обычное, чтобы остались тень, скругления и привязка к краям
    hwnd_ = CreateWindowExW(0, kClassName, L"Vinyl", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760,
                            nullptr, nullptr, inst, this);
    if (!hwnd_) return false;

    // без этого окно остается с первым расчетом рамки и сверху висит родная полоса заголовка
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    updateDpi();

    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DWORD corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    if (!painter_.create(hwnd_, dpi_)) return false;
    ui_.p = &painter_;

    // одно поле ввода на все экраны, просто переставляем его куда надо
    edit_ = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL | ES_LEFT,
                            0, 0, 10, 10, hwnd_, (HMENU)(INT_PTR)kEditId, inst, nullptr);
    if (edit_) {
        SetWindowSubclass(edit_, &MainWindow::editProc, 1, (DWORD_PTR)this);
    }

    model_.load();
    HWND target = hwnd_;
    model_.onChanged([target] { PostMessageW(target, WM_VINYL_MODEL, 0, 0); });
    model_.onMessage([this, target](const std::string& message) {
        say(message);
        PostMessageW(target, WM_VINYL_MODEL, 0, 0);
    });
    core_.onState([target](VpnState, const std::string&) { PostMessageW(target, WM_VINYL_STATE, 0, 0); });

    applog::line("загружено серверов: " + std::to_string(model_.state().servers.size()));

    SetTimer(hwnd_, kTickTimer, 33, nullptr);
    addTray();

    AppState state = model_.state();
    if (state.settings.autoConnect && !state.servers.empty()) toggleVpn();
    return true;
}

void MainWindow::show(int cmdShow) {
    ShowWindow(hwnd_, cmdShow);
    UpdateWindow(hwnd_);
}

void MainWindow::updateDpi() {
    dpi_ = GetDpiForWindow(hwnd_);
    if (dpi_ == 0) dpi_ = 96;
    painter_.setDpi(dpi_);
}

void MainWindow::say(const std::string& message) {
    if (message.empty()) return;
    std::lock_guard<std::mutex> lock(toastMutex_);
    toast_ = text::wide(message);
    toastUntil_ = GetTickCount() + 3500;
}

LRESULT CALLBACK MainWindow::wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = h;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    }
    if (self) return self->onMessage(msg, w, l);
    return DefWindowProcW(h, msg, w, l);
}

// ловим enter и escape, обычный edit их не отдает
LRESULT CALLBACK MainWindow::editProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR data) {
    MainWindow* self = reinterpret_cast<MainWindow*>(data);
    if (msg == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE)) return 0;
    if (msg == WM_KEYDOWN && self != nullptr) {
        if (w == VK_RETURN) {
            self->closeEditor(true);
            return 0;
        }
        if (w == VK_ESCAPE) {
            self->closeEditor(false);
            return 0;
        }
    }
    return DefSubclassProc(h, msg, w, l);
}

LRESULT MainWindow::onMessage(UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {

    case WM_NCCALCSIZE: {
        if (w == TRUE) {
            if (IsZoomed(hwnd_)) {
                // в развернутом виде иначе часть окна уезжает за экран
                auto p = reinterpret_cast<NCCALCSIZE_PARAMS*>(l);
                int frame = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi_) +
                            GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi_);
                p->rgrc[0].left += frame;
                p->rgrc[0].right -= frame;
                p->rgrc[0].bottom -= frame;
                p->rgrc[0].top += frame;
            }
            return 0;
        }
        break;
    }

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        RECT wr;
        GetWindowRect(hwnd_, &wr);
        int b = (int)painter_.dp(6.f);

        if (!IsZoomed(hwnd_)) {
            bool left = pt.x < wr.left + b;
            bool right = pt.x >= wr.right - b;
            bool top = pt.y < wr.top + b;
            bool bottom = pt.y >= wr.bottom - b;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }

        POINT cp = pt;
        ScreenToClient(hwnd_, &cp);
        if (cp.y < (int)titleBarH()) {
            int id = ui_.hitTest((float)cp.x, (float)cp.y);
            if (id == ActMinimize || id == ActCloseWindow) return HTCLIENT;
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_DPICHANGED: {
        dpi_ = HIWORD(w);
        painter_.setDpi(dpi_);
        RECT* target = reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd_, nullptr, target->left, target->top,
                     target->right - target->left, target->bottom - target->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO: {
        auto mm = reinterpret_cast<MINMAXINFO*>(l);
        mm->ptMinTrackSize.x = (LONG)painter_.dp(940);
        mm->ptMinTrackSize.y = (LONG)painter_.dp(660);
        return 0;
    }

    case WM_SIZE: {
        if (w != SIZE_MINIMIZED) painter_.resize(LOWORD(l), HIWORD(l));
        closeEditor(false);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd_, &ps);
        render();
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_TIMER: {
        if (w != kTickTimer) return 0;
        bool running = vpn_ == VpnState::Running;
        bool busy = vpn_ == VpnState::Starting;
        if (running || busy) {
            // полный оборот за 5,2 секунды как на телефоне, при подключении быстрее
            angle_ += busy ? 8.3f : 2.1f;
            if (angle_ >= 360.f) angle_ -= 360.f;
        }
        float wanted = running ? 1.f : (busy ? 0.6f : 0.f);
        glow_ += (wanted - glow_) * 0.08f;

        bool needRedraw = running || busy || fabsf(wanted - glow_) > 0.01f;
        {
            std::lock_guard<std::mutex> lock(toastMutex_);
            if (!toast_.empty()) {
                if (GetTickCount() > toastUntil_) toast_.clear();
                needRedraw = true;
            }
        }
        if (needRedraw) InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_VINYL_STATE: {
        applyVpnState();
        return 0;
    }

    case WM_VINYL_MODEL: {
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!tracking_) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
            tracking_ = true;
        }
        int id = ui_.hitTest((float)GET_X_LPARAM(l), (float)GET_Y_LPARAM(l));
        if (id != ui_.hot) {
            ui_.hot = id;
            SetCursor(LoadCursorW(nullptr, id >= 0 ? IDC_HAND : IDC_ARROW));
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE: {
        tracking_ = false;
        ui_.hot = ActNone;
        ui_.pressed = ActNone;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (tab_ == Tab::Home) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(w);
        setScroll(scroll() - delta * painter_.dp(0.6f));
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int id = ui_.hitTest((float)GET_X_LPARAM(l), (float)GET_Y_LPARAM(l));
        ui_.pressed = id;
        if (editTarget_ != EditTarget::None && id != ActAddField && id != ActDomainField && id != ActAppSearch) {
            closeEditor(false);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        int id = ui_.hitTest((float)GET_X_LPARAM(l), (float)GET_Y_LPARAM(l));
        int pressed = ui_.pressed;
        ui_.pressed = ActNone;
        if (id >= 0 && id == pressed) onClick(id);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(244, 242, 250));
        SetBkColor(dc, RGB(7, 6, 11));
        static HBRUSH back = CreateSolidBrush(RGB(7, 6, 11));
        return (LRESULT)back;
    }

    case WM_VINYL_TRAY: {
        if (LOWORD(l) == WM_LBUTTONUP || LOWORD(l) == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
        } else if (LOWORD(l) == WM_RBUTTONUP) {
            trayMenu();
        }
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(w)) {
        case 100: ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_); break;
        case 101: toggleVpn(); break;
        case 102: setTab(Tab::Servers); ShowWindow(hwnd_, SW_SHOW); SetForegroundWindow(hwnd_); break;
        case 103: DestroyWindow(hwnd_); break;
        }
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hwnd_, SW_HIDE);  // закрытие прячет в трей, выход через меню трея
        return 0;

    case WM_DESTROY:
        stats_.stop();
        core_.stop();
        KillTimer(hwnd_, kTickTimer);
        removeTray();
        painter_.discard();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, w, l);
}

// --- отрисовка ---

D2D1_RECT_F MainWindow::contentArea(D2D1_SIZE_F size) const {
    return D2D1::RectF(sidebarW() + painter_.dp(28.f), titleBarH() + painter_.dp(14.f),
                       size.width - painter_.dp(28.f), size.height - painter_.dp(18.f));
}

void MainWindow::setScroll(float value) {
    D2D1_RECT_F area = contentArea(painter_.size());
    float view = area.bottom - area.top;
    float maxScroll = contentHeight_ - view;
    if (maxScroll < 0) maxScroll = 0;
    if (value < 0) value = 0;
    if (value > maxScroll) value = maxScroll;
    scroll_[(int)tab_] = value;
}

void MainWindow::render() {
    if (!painter_.ready()) return;

    ui_.reset();
    ui_.offsetY = 0.f;
    painter_.begin(theme::background);
    D2D1_SIZE_F size = painter_.size();

    // мягкое свечение по углам, как на телефоне
    painter_.glow(D2D1::Point2F(size.width * 0.1f, 0), size.width * 0.55f, theme::accentDeep, 0.35f);
    painter_.glow(D2D1::Point2F(size.width, size.height), size.width * 0.5f, theme::accent, 0.20f);

    drawSidebar(size);

    D2D1_RECT_F area = contentArea(size);
    switch (tab_) {
    case Tab::Home:     drawHome(area); break;
    case Tab::Servers:  drawServers(area); break;
    case Tab::Routes:   drawRoutes(area); break;
    case Tab::Settings: drawSettings(area); break;
    }

    drawChrome(size);
    drawToast(size);

    if (!painter_.end()) {
        painter_.discard();
        painter_.create(hwnd_, dpi_);
    }
}

void MainWindow::drawChrome(D2D1_SIZE_F size) {
    float bar = titleBarH();
    float w = painter_.dp(46.f);

    for (int i = 0; i < 2; i++) {
        bool isClose = (i == 0);
        float right = size.width - (isClose ? 0.f : w);
        D2D1_RECT_F r = D2D1::RectF(right - w, 0, right, bar);
        int id = isClose ? ActCloseWindow : ActMinimize;

        if (ui_.hot == id) {
            painter_.rect(r, isClose ? D2D1::ColorF(0xE81123, 0.9f) : theme::hover());
        }
        painter_.text(isClose ? glyph::close : glyph::minimize, Font::Icon, r,
                      theme::text, Align::Center, VAlign::Middle);
        ui_.zone(r, id);
    }
}

void MainWindow::drawSidebar(D2D1_SIZE_F size) {
    float width = sidebarW();
    painter_.rect(D2D1::RectF(0, 0, width, size.height), D2D1::ColorF(0x0B0A11));
    painter_.line(D2D1::Point2F(width, 0), D2D1::Point2F(width, size.height), theme::stroke());

    // логотип
    float markSize = painter_.dp(17.f);
    D2D1_POINT_2F mark = D2D1::Point2F(painter_.dp(28.f), painter_.dp(24.f));
    painter_.circleGradient(mark, markSize, theme::accentBright, theme::accentDeep);
    painter_.circle(mark, markSize * 0.28f, D2D1::ColorF(0x0B0A11));
    painter_.text(L"V I N Y L", Font::TitleMedium,
                  D2D1::RectF(painter_.dp(54.f), 0, width, painter_.dp(48.f)),
                  theme::text, Align::Left, VAlign::Middle);

    // пункты меню
    float top = painter_.dp(72.f);
    for (int i = 0; i < 4; i++) {
        D2D1_RECT_F r = D2D1::RectF(painter_.dp(12.f), top + i * painter_.dp(48.f),
                                    width - painter_.dp(12.f), top + i * painter_.dp(48.f) + painter_.dp(42.f));
        int id = ActNavBase + i;
        bool selected = (int)tab_ == i;

        if (selected) {
            painter_.round(r, painter_.dp(14.f), theme::accentSoft());
            painter_.roundBorder(r, painter_.dp(14.f), w::alpha(theme::accent, 0.3f));
        } else if (ui_.hot == id) {
            painter_.round(r, painter_.dp(14.f), theme::hover());
        }
        painter_.text(kNav[i].icon, Font::Icon,
                      D2D1::RectF(r.left + painter_.dp(14.f), r.top, r.left + painter_.dp(42.f), r.bottom),
                      selected ? theme::accentBright : theme::textMuted, Align::Left, VAlign::Middle);
        painter_.text(kNav[i].title, Font::TitleMedium,
                      D2D1::RectF(r.left + painter_.dp(46.f), r.top, r.right, r.bottom),
                      selected ? theme::text : theme::textSecond, Align::Left, VAlign::Middle);
        ui_.zone(r, id);
    }

    // состояние внизу
    D2D1_COLOR_F dot = theme::textMuted;
    if (vpn_ == VpnState::Running) dot = theme::success;
    else if (vpn_ == VpnState::Starting) dot = theme::warning;
    else if (vpn_ == VpnState::Error) dot = theme::danger;

    float bottom = size.height - painter_.dp(58.f);
    painter_.circle(D2D1::Point2F(painter_.dp(26.f), bottom + painter_.dp(9.f)), painter_.dp(4.f), dot);
    painter_.text(statusText(), Font::LabelMedium,
                  D2D1::RectF(painter_.dp(38.f), bottom, width, bottom + painter_.dp(18.f)),
                  dot, Align::Left, VAlign::Middle);
    painter_.text(L"claustrophobDev", Font::LabelMedium,
                  D2D1::RectF(painter_.dp(26.f), size.height - painter_.dp(32.f), width, size.height - painter_.dp(12.f)),
                  theme::textMuted, Align::Left, VAlign::Middle);
}

void MainWindow::drawToast(D2D1_SIZE_F size) {
    std::wstring message;
    {
        std::lock_guard<std::mutex> lock(toastMutex_);
        message = toast_;
    }
    if (message.empty()) return;

    float width = painter_.measure(message, Font::BodyLarge, size.width).width + painter_.dp(40.f);
    float height = painter_.dp(44.f);
    float cx = sidebarW() + (size.width - sidebarW()) / 2.f;
    D2D1_RECT_F r = D2D1::RectF(cx - width / 2, size.height - painter_.dp(38.f) - height,
                                cx + width / 2, size.height - painter_.dp(38.f));

    painter_.round(r, height / 2, theme::surfaceHigh);
    painter_.roundBorder(r, height / 2, theme::strokeStrong());
    painter_.text(message, Font::BodyLarge, r, theme::text, Align::Center, VAlign::Middle);
}

std::wstring MainWindow::statusText() const {
    switch (vpn_) {
    case VpnState::Stopped:  return L"Не подключено";
    case VpnState::Starting: return L"Подключение";
    case VpnState::Running:  return L"Подключено";
    case VpnState::Error:    return L"Не удалось подключиться";
    }
    return L"Не подключено";
}

// --- впн ---

void MainWindow::toggleVpn() {
    if (vpn_ == VpnState::Running || vpn_ == VpnState::Starting) {
        core_.stop();
        return;
    }
    Server server;
    if (!model_.pickServer(server)) {
        say("Сначала добавьте сервер");
        setTab(Tab::Servers);
        return;
    }
    AppState state = model_.state();
    core_.start(server, state.routing, state.settings);
}

void MainWindow::applyVpnState() {
    VpnState before = vpn_;
    vpn_ = core_.state();
    vpnError_ = text::wide(core_.message());

    if (vpn_ == VpnState::Running && before != VpnState::Running) {
        stats_.start();
        model_.setNeedReconnect(false);
    }
    if (vpn_ != VpnState::Running) stats_.stop();

    updateTrayTip();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// --- действия ---

void MainWindow::setTab(Tab tab) {
    if (tab_ == tab) return;
    closeEditor(false);
    tab_ = tab;
    if (tab == Tab::Routes) apps_ = processes::running();
    if (tab == Tab::Settings) reloadLog();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onClick(int id) {
    if (id >= ActDomainBase) {
        AppState state = model_.state();
        size_t index = (size_t)(id - ActDomainBase);
        if (index < state.routing.directDomains.size()) model_.removeDomain(state.routing.directDomains[index]);
        return;
    }
    if (id >= ActAppBase) {
        size_t index = (size_t)(id - ActAppBase);
        if (index < apps_.size()) model_.toggleApp(apps_[index].exe);
        return;
    }
    if (id >= ActSubDelete) {
        AppState state = model_.state();
        size_t index = (size_t)(id - ActSubDelete);
        if (index < state.subscriptions.size()) model_.deleteSubscription(state.subscriptions[index].id);
        return;
    }
    if (id >= ActSubRefresh) {
        AppState state = model_.state();
        size_t index = (size_t)(id - ActSubRefresh);
        if (index < state.subscriptions.size()) model_.refresh(state.subscriptions[index].id);
        return;
    }
    if (id >= ActServerDelete) {
        AppState state = model_.state();
        size_t index = (size_t)(id - ActServerDelete);
        if (index < state.servers.size()) model_.deleteServer(state.servers[index].id);
        return;
    }
    if (id >= ActServerBase) {
        AppState state = model_.state();
        size_t index = (size_t)(id - ActServerBase);
        if (index >= state.servers.size()) return;
        const Server& server = state.servers[index];
        if (!server.unsupported.empty()) say(server.unsupported);
        else model_.select(server.id);
        return;
    }

    if (id >= ActNavBase && id < ActNavBase + 4) {
        setTab((Tab)(id - ActNavBase));
        return;
    }
    if (id >= ActModeBase && id < ActModeBase + 3) {
        model_.setAppMode((AppMode)(id - ActModeBase));
        return;
    }
    if (id >= ActDnsBase && id < ActDnsBase + 3) {
        model_.setDns((RemoteDns)(id - ActDnsBase));
        return;
    }

    switch (id) {
    case ActMinimize: ShowWindow(hwnd_, SW_MINIMIZE); break;
    case ActCloseWindow: ShowWindow(hwnd_, SW_HIDE); break;
    case ActConnect: toggleVpn(); break;
    case ActReconnect: core_.stop(); toggleVpn(); break;
    case ActPaste: pasteFromClipboard(); break;
    case ActPingAll: model_.pingAll(); break;
    case ActRefreshAll: model_.refreshAll(); break;
    case ActAutoSelect: model_.select(kAutoServerId); break;
    case ActServerCard: setTab(Tab::Servers); break;
    case ActBypassBanks: model_.setBypassBanks(!model_.state().routing.bypassBanks); break;
    case ActDirectRu: model_.setDirectRuSites(!model_.state().routing.directRuSites); break;
    case ActIpv6: model_.setIpv6(!model_.state().settings.ipv6); break;
    case ActAutoConnect: model_.setAutoConnect(!model_.state().settings.autoConnect); break;
    case ActAutostart: {
        bool on = !autostart::enabled();
        autostart::set(on);
        say(on ? "Vinyl будет запускаться вместе с Windows" : "Автозапуск выключен");
        break;
    }
    case ActDomainAdd: closeEditor(true); break;
    case ActAddSubmit: closeEditor(true); break;
    case ActLogRefresh: reloadLog(); break;
    case ActLogCopy: copyLog(); break;
    case ActLogFolder: ShellExecuteW(hwnd_, L"open", paths::dataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
    case ActGithub: ShellExecuteW(hwnd_, L"open", L"https://github.com/claustrophobDev", nullptr, nullptr, SW_SHOWNORMAL); break;
    case ActAddField: openEditor(EditTarget::Add, editRect_, L""); break;
    case ActDomainField: openEditor(EditTarget::Domain, editRect_, L""); break;
    case ActAppSearch: openEditor(EditTarget::AppSearch, editRect_, appSearch_.c_str()); break;
    default: break;
    }
}

void MainWindow::pasteFromClipboard() {
    if (!OpenClipboard(hwnd_)) {
        say("Не получилось открыть буфер обмена");
        return;
    }
    std::wstring value;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const wchar_t* locked = (const wchar_t*)GlobalLock(data);
        if (locked != nullptr) {
            value = locked;
            GlobalUnlock(data);
        }
    }
    CloseClipboard();

    if (value.empty()) {
        say("В буфере обмена ничего нет");
        return;
    }
    model_.addFromText(text::narrow(value));
}

void MainWindow::reloadLog() {
    logLines_.clear();
    std::ifstream in(paths::coreLogFile().c_str());
    if (!in) return;

    std::vector<std::string> all;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) all.push_back(line);
    }
    size_t from = all.size() > 200 ? all.size() - 200 : 0;
    for (size_t i = all.size(); i > from; i--) {
        logLines_.push_back(text::wide(all[i - 1]));   // новые сверху
    }
}

void MainWindow::copyLog() {
    std::wstring all;
    for (const std::wstring& line : logLines_) all += line + L"\r\n";
    if (all.empty()) {
        say("Журнал пустой");
        return;
    }
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    size_t bytes = (all.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory != nullptr) {
        void* target = GlobalLock(memory);
        memcpy(target, all.c_str(), bytes);
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
    }
    CloseClipboard();
    say("Журнал скопирован");
}

// --- поле ввода ---

void MainWindow::openEditor(EditTarget target, D2D1_RECT_F rect, const wchar_t* value) {
    if (!edit_) return;
    editTarget_ = target;
    editRect_ = rect;

    if (editFont_) DeleteObject(editFont_);
    editFont_ = CreateFontW((int)-painter_.dp(15.f), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessageW(edit_, WM_SETFONT, (WPARAM)editFont_, TRUE);
    SetWindowTextW(edit_, value);

    layoutEditor();
    ShowWindow(edit_, SW_SHOW);
    SetFocus(edit_);
    SendMessageW(edit_, EM_SETSEL, 0, -1);
}

void MainWindow::layoutEditor() {
    if (!edit_ || editTarget_ == EditTarget::None) return;
    int pad = (int)painter_.dp(14.f);
    int height = (int)painter_.dp(22.f);
    int top = (int)((editRect_.top + editRect_.bottom) / 2.f) - height / 2;
    MoveWindow(edit_, (int)editRect_.left + pad, top,
               (int)(editRect_.right - editRect_.left) - pad * 2, height, TRUE);
}

std::wstring MainWindow::editorText() const {
    if (!edit_) return std::wstring();
    int length = GetWindowTextLengthW(edit_);
    if (length <= 0) return std::wstring();
    std::wstring value((size_t)length, L'\0');
    GetWindowTextW(edit_, value.data(), length + 1);
    return value;
}

void MainWindow::closeEditor(bool commit) {
    if (editTarget_ == EditTarget::None) return;
    EditTarget target = editTarget_;
    std::wstring value = editorText();

    if (target == EditTarget::AppSearch) {
        appSearch_ = value;
        if (!commit) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;   // поиск не закрываем, он живет пока печатают
        }
    }

    editTarget_ = EditTarget::None;
    ShowWindow(edit_, SW_HIDE);
    SetFocus(hwnd_);

    if (!commit) {
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    std::string utf8 = text::narrow(value);
    if (target == EditTarget::Add) model_.addFromText(utf8);
    else if (target == EditTarget::Domain && !text::trim(utf8).empty()) model_.addDomain(utf8);

    InvalidateRect(hwnd_, nullptr, FALSE);
}

// --- трей ---

void MainWindow::addTray() {
    if (trayAdded_) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_VINYL_TRAY;
    nid.hIcon = LoadIconW(inst_, MAKEINTRESOURCEW(1));
    wcscpy_s(nid.szTip, L"Vinyl — не подключено");
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &nid) == TRUE;
}

void MainWindow::updateTrayTip() {
    if (!trayAdded_) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayId;
    nid.uFlags = NIF_TIP;
    std::wstring tip = L"Vinyl — " + statusText();
    wcsncpy_s(nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void MainWindow::removeTray() {
    if (!trayAdded_) return;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd_;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    trayAdded_ = false;
}

void MainWindow::trayMenu() {
    bool on = vpn_ == VpnState::Running || vpn_ == VpnState::Starting;
    Server server;
    bool ready = model_.pickServer(server);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 100, L"Открыть Vinyl");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (ready ? 0 : MF_GRAYED), 101, on ? L"Отключить" : L"Подключить");
    if (ready) {
        std::wstring name = L"Сервер: " + text::wide(server.name);
        AppendMenuW(menu, MF_STRING, 102, name.c_str());
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 103, L"Выход");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}
