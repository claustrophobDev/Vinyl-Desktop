#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/Processes.h"
#include "data/Models.h"
#include "ui/Actions.h"
#include "ui/Painter.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "vpn/Core.h"
#include "vpn/Stats.h"

// все что нужно экрану для отрисовки, менять состояние он не умеет
struct ViewContext {
    Painter& p;
    Ui& ui;

    const AppState& state;
    const std::map<std::string, int>& pings;
    const std::set<std::string>& refreshing;
    const std::vector<processes::Item>& apps;
    const std::vector<std::wstring>& logLines;
    const std::wstring& vpnError;
    const std::wstring& appSearch;
    const TrafficStats& stats;

    const Server* server;        // выбранный или лучший по пингу, бывает и пусто
    D2D1_RECT_F* editRect;       // сюда экран кладет место под системное поле ввода

    VpnState vpn;
    bool pinging;
    bool needReconnect;
    bool autostart;
    bool editingAdd;
    bool editingDomain;
    float scroll;
    float angle;
    float glow;
};

inline std::wstring statusText(VpnState vpn) {
    switch (vpn) {
    case VpnState::Stopped:  return L"Не подключено";
    case VpnState::Starting: return L"Подключение";
    case VpnState::Running:  return L"Подключено";
    case VpnState::Error:    return L"Не удалось подключиться";
    }
    return L"Не подключено";
}

inline D2D1_COLOR_F statusColor(VpnState vpn) {
    switch (vpn) {
    case VpnState::Running:  return theme::success;
    case VpnState::Starting: return theme::warning;
    case VpnState::Error:    return theme::danger;
    default:                 return theme::textMuted;
    }
}
