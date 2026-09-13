// Четыре экрана. Верстка повторяет мобильную: те же карточки, отступы и порядок блоков.

#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include "app/Autostart.h"
#include "app/Install.h"
#include "core/Text.h"
#include "net/Ping.h"
#include "ui/Format.h"

#include <cmath>

using w::alpha;

// --- главная ---

void MainWindow::drawHome(D2D1_RECT_F area) {
    Painter& p = painter_;
    AppState state = model_.state();
    contentHeight_ = 0.f;

    // подзаголовок не нужен, статус и так висит таблеткой над пластинкой
    w::screenHeader(ui_, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Главная", L"");

    float top = area.top + p.dp(62.f);
    float bottom = area.bottom;

    // нижний блок: карточка сервера
    float cardHeight = p.dp(76.f);
    D2D1_RECT_F serverCard = D2D1::RectF(area.left, bottom - cardHeight, area.right, bottom);

    // над ней плитки скорости
    float tilesHeight = p.dp(76.f);
    float tilesTop = serverCard.top - p.dp(12.f) - tilesHeight;
    float gap = p.dp(12.f);
    float tileWidth = (area.right - area.left - gap) / 2.f;

    bool active = vpn_ == VpnState::Running;
    drawTrafficTile(D2D1::RectF(area.left, tilesTop, area.left + tileWidth, tilesTop + tilesHeight),
                    glyph::download, L"Загрузка",
                    active ? fmt::speed(stats_.downSpeed()) : L"—",
                    active ? fmt::bytes(stats_.downTotal()) : L"",
                    theme::success);
    drawTrafficTile(D2D1::RectF(area.right - tileWidth, tilesTop, area.right, tilesTop + tilesHeight),
                    glyph::upload, L"Отдача",
                    active ? fmt::speed(stats_.upSpeed()) : L"—",
                    active ? fmt::bytes(stats_.upTotal()) : L"",
                    theme::accentBright);

    // баннер про переподключение
    float discBottom = tilesTop - p.dp(16.f);
    if (model_.needReconnect() && active) {
        float bannerHeight = p.dp(48.f);
        D2D1_RECT_F banner = D2D1::RectF(area.left, discBottom - bannerHeight, area.right, discBottom);
        p.round(banner, p.dp(16.f), theme::accentSoft());
        p.roundBorder(banner, p.dp(16.f), alpha(theme::accent, 0.3f));
        p.text(glyph::refresh, Font::Icon,
               D2D1::RectF(banner.left + p.dp(16.f), banner.top, banner.left + p.dp(44.f), banner.bottom),
               theme::accentBright, Align::Left, VAlign::Middle);
        p.text(L"Настройки изменились, нужно переподключиться", Font::BodyMedium,
               D2D1::RectF(banner.left + p.dp(48.f), banner.top, banner.right - p.dp(110.f), banner.bottom),
               theme::text, Align::Left, VAlign::Middle);
        p.text(L"Применить", Font::LabelLarge,
               D2D1::RectF(banner.right - p.dp(110.f), banner.top, banner.right - p.dp(16.f), banner.bottom),
               theme::accentBright, Align::Right, VAlign::Middle);
        ui_.zone(banner, ActReconnect);
        discBottom = banner.top - p.dp(12.f);
    }

    // пластинка по центру оставшегося места
    float available = discBottom - top;
    float radius = available / 2.f - p.dp(38.f);
    float maxRadius = (area.right - area.left) * 0.30f;
    if (radius > maxRadius) radius = maxRadius;

    if (radius > p.dp(40.f)) {
        D2D1_POINT_2F center = D2D1::Point2F((area.left + area.right) / 2.f, top + p.dp(30.f) + radius);

        // таблетка со статусом над диском
        D2D1_COLOR_F color = theme::textMuted;
        if (vpn_ == VpnState::Running) color = theme::success;
        else if (vpn_ == VpnState::Starting) color = theme::warning;
        else if (vpn_ == VpnState::Error) color = theme::danger;

        std::wstring label = statusText();
        float pillWidth = p.measure(label, Font::LabelLarge, area.right - area.left).width + p.dp(44.f);
        float pillHeight = p.dp(32.f);
        D2D1_RECT_F pill = D2D1::RectF(center.x - pillWidth / 2, top,
                                       center.x + pillWidth / 2, top + pillHeight);
        p.round(pill, pillHeight / 2, alpha(color, 0.10f));
        p.circle(D2D1::Point2F(pill.left + p.dp(16.f), (pill.top + pill.bottom) / 2), p.dp(4.f), color);
        p.text(label, Font::LabelLarge,
               D2D1::RectF(pill.left + p.dp(26.f), pill.top, pill.right, pill.bottom),
               color, Align::Center, VAlign::Middle);

        drawDisc(center, radius);

        // под диском таймер или подсказка
        D2D1_RECT_F under = D2D1::RectF(area.left, center.y + radius + p.dp(10.f),
                                        area.right, center.y + radius + p.dp(62.f));
        if (vpn_ == VpnState::Running) {
            p.text(fmt::duration(stats_.uptime()), Font::Display, under, theme::text, Align::Center, VAlign::Middle);
        } else if (vpn_ == VpnState::Error && !vpnError_.empty()) {
            p.text(vpnError_, Font::BodyMedium, under, theme::danger, Align::Center, VAlign::Middle);
        } else {
            p.text(L"Нажмите на пластинку", Font::BodyLarge, under, theme::textMuted, Align::Center, VAlign::Middle);
        }

        ui_.zone(D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius), ActConnect);
    }

    // карточка выбранного сервера
    w::cardButton(ui_, serverCard, ActServerCard);
    Server picked;
    bool has = model_.pickServer(picked);
    float cy = (serverCard.top + serverCard.bottom) / 2.f;

    if (!has) {
        p.circle(D2D1::Point2F(serverCard.left + p.dp(38.f), cy), p.dp(22.f), theme::accentSoft());
        p.text(glyph::add, Font::Icon,
               D2D1::RectF(serverCard.left + p.dp(16.f), cy - p.dp(22.f), serverCard.left + p.dp(60.f), cy + p.dp(22.f)),
               theme::accentBright, Align::Center, VAlign::Middle);
        p.text(L"Добавьте сервер", Font::TitleMedium,
               D2D1::RectF(serverCard.left + p.dp(72.f), serverCard.top + p.dp(16.f), serverCard.right, cy + p.dp(2.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(L"Подписка или ключ из буфера обмена", Font::BodyMedium,
               D2D1::RectF(serverCard.left + p.dp(72.f), cy + p.dp(2.f), serverCard.right, serverCard.bottom),
               theme::textMuted, Align::Left, VAlign::Top);
    } else {
        w::flagAvatar(ui_, D2D1::Point2F(serverCard.left + p.dp(38.f), cy), p.dp(22.f), picked.flag, false);
        std::wstring subtitle = text::wide(std::string(protocolLabel(picked.protocol)) + " · " + picked.host);
        if (state.selectedId == kAutoServerId) {
            subtitle = L"Автовыбор · " + subtitle;
        }
        p.text(text::wide(picked.name), Font::TitleMedium,
               D2D1::RectF(serverCard.left + p.dp(72.f), serverCard.top + p.dp(16.f),
                           serverCard.right - p.dp(120.f), cy + p.dp(2.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(subtitle, Font::BodyMedium,
               D2D1::RectF(serverCard.left + p.dp(72.f), cy + p.dp(2.f),
                           serverCard.right - p.dp(120.f), serverCard.bottom),
               theme::textMuted, Align::Left, VAlign::Top);

        std::map<std::string, int> pings = model_.pings();
        auto it = pings.find(picked.id);
        w::pingBadge(ui_, D2D1::Point2F(serverCard.right - p.dp(46.f), cy), it == pings.end() ? 0 : it->second);
        p.text(glyph::chevron, Font::Icon,
               D2D1::RectF(serverCard.right - p.dp(36.f), serverCard.top, serverCard.right - p.dp(12.f), serverCard.bottom),
               theme::textMuted, Align::Center, VAlign::Middle);
    }
}

void MainWindow::drawDisc(D2D1_POINT_2F center, float radius) {
    Painter& p = painter_;
    bool error = vpn_ == VpnState::Error;
    float g = glow_;

    // свечение вокруг
    p.glow(center, radius * 1.5f, error ? theme::danger : theme::accent, error ? 0.35f : g);

    // тело пластинки
    p.circleGradient(center, radius, D2D1::ColorF(0x1C1A26), D2D1::ColorF(0x050409));

    // дорожки
    float grooveR = radius * 0.44f;
    int index = 0;
    while (grooveR < radius * 0.965f) {
        float a = (index % 4 == 0) ? 0.075f : 0.032f;
        p.ring(center, grooveR, D2D1::ColorF(0xFFFFFF, a), 0.8f);
        grooveR += radius * 0.025f;
        index++;
    }
    p.ring(center, radius - 0.75f, D2D1::ColorF(0xFFFFFF, 0.10f), 1.5f);

    // блик-дуга, по ней видно что диск крутится
    D2D1_COLOR_F arcColor = error ? alpha(theme::danger, 0.5f)
                                  : alpha(theme::accentBright, 0.18f + 0.4f * g);
    p.arc(center, radius * 0.8f, angle_ - 65.f, 48.f, arcColor, 1.4f);
    p.arc(center, radius * 0.62f, angle_ + 150.f, 26.f, alpha(D2D1::ColorF(0xFFFFFF), 0.06f + 0.12f * g), 1.2f);

    // наклейка
    float label = radius * 0.40f;
    p.circleGradient(center, label,
                     D2D1::ColorF(0.17f + 0.09f * g, 0.14f + 0.03f * g, 0.24f + 0.26f * g),
                     D2D1::ColorF(0.09f, 0.08f, 0.14f + 0.09f * g));
    D2D1_COLOR_F ringColor = error ? alpha(theme::danger, 0.6f)
                                   : D2D1::ColorF(theme::accentBright.r, theme::accentBright.g,
                                                  theme::accentBright.b, 0.10f + 0.55f * g);
    p.ring(center, label - 0.75f, ringColor, 1.0f);

    // дырка посередине. рисуем ее до надписи, иначе она перечеркивает буквы
    p.circle(center, radius * 0.055f, theme::background);
    p.ring(center, radius * 0.055f, D2D1::ColorF(0xFFFFFF, 0.12f), 1.0f);

    // название над дыркой, как на настоящей наклейке
    D2D1_COLOR_F markColor = D2D1::ColorF(theme::text.r, theme::text.g, theme::text.b, 0.45f + 0.45f * g);
    p.text(L"V I N Y L", Font::LabelSmall,
           D2D1::RectF(center.x - label, center.y - label * 0.74f, center.x + label, center.y - label * 0.18f),
           markColor, Align::Center, VAlign::Middle);
    p.line(D2D1::Point2F(center.x - label * 0.34f, center.y + label * 0.40f),
           D2D1::Point2F(center.x + label * 0.34f, center.y + label * 0.40f),
           D2D1::ColorF(0xFFFFFF, 0.06f + 0.10f * g), 1.0f);
}

void MainWindow::drawTrafficTile(D2D1_RECT_F r, const wchar_t* icon, const std::wstring& label,
                                 const std::wstring& value, const std::wstring& total,
                                 const D2D1_COLOR_F& tint) {
    Painter& p = painter_;
    w::card(ui_, r);

    float cy = (r.top + r.bottom) / 2.f;
    p.circle(D2D1::Point2F(r.left + p.dp(32.f), cy), p.dp(16.f), alpha(tint, 0.12f));
    p.text(icon, Font::Icon,
           D2D1::RectF(r.left + p.dp(16.f), cy - p.dp(16.f), r.left + p.dp(48.f), cy + p.dp(16.f)),
           tint, Align::Center, VAlign::Middle);

    float left = r.left + p.dp(60.f);
    p.text(label, Font::LabelMedium,
           D2D1::RectF(left, r.top + p.dp(14.f), r.right - p.dp(12.f), r.top + p.dp(32.f)),
           theme::textMuted, Align::Left, VAlign::Middle);
    p.text(value, Font::TitleMedium,
           D2D1::RectF(left, r.top + p.dp(32.f), r.right - p.dp(12.f), r.top + p.dp(54.f)),
           theme::text, Align::Left, VAlign::Middle);
    if (!total.empty()) {
        p.text(L"всего " + total, Font::BodyMedium,
               D2D1::RectF(left, r.top + p.dp(52.f), r.right - p.dp(12.f), r.bottom - p.dp(8.f)),
               theme::textMuted, Align::Left, VAlign::Middle);
    }
}

// --- серверы ---

void MainWindow::drawEmptyServers(D2D1_RECT_F area) {
    Painter& p = painter_;
    float cx = (area.left + area.right) / 2.f;
    float cy = (area.top + area.bottom) / 2.f;

    p.circleGradient(D2D1::Point2F(cx, cy - p.dp(90.f)), p.dp(44.f),
                     alpha(theme::accentBright, 0.35f), alpha(theme::accentDeep, 0.25f));
    p.circle(D2D1::Point2F(cx, cy - p.dp(90.f)), p.dp(12.f), theme::background);

    p.text(L"Пока пусто", Font::TitleLarge,
           D2D1::RectF(area.left, cy - p.dp(34.f), area.right, cy - p.dp(6.f)),
           theme::text, Align::Center, VAlign::Middle);
    p.text(L"Скопируйте ссылку на подписку или ключ vless://, trojan://, ss:// и вставьте сюда",
           Font::BodyLarge, D2D1::RectF(area.left, cy - p.dp(4.f), area.right, cy + p.dp(24.f)),
           theme::textMuted, Align::Center, VAlign::Middle);

    float buttonWidth = p.dp(280.f);
    D2D1_RECT_F paste = D2D1::RectF(cx - buttonWidth / 2, cy + p.dp(44.f),
                                    cx + buttonWidth / 2, cy + p.dp(44.f) + p.dp(w::kButtonHeight));
    w::primaryButton(ui_, paste, L"Вставить из буфера", glyph::paste, ActPaste);

    D2D1_RECT_F manual = D2D1::RectF(cx - buttonWidth / 2, paste.bottom + p.dp(12.f),
                                     cx + buttonWidth / 2, paste.bottom + p.dp(12.f) + p.dp(w::kButtonHeight));
    if (editTarget_ == EditTarget::Add) {
        // пока печатают, вместо кнопки показываем поле, системный edit лежит прямо на нем
        w::field(ui_, manual, L"", false, true, ActAddField);
        D2D1_RECT_F ok = D2D1::RectF(manual.left, manual.bottom + p.dp(12.f),
                                     manual.right, manual.bottom + p.dp(12.f) + p.dp(w::kButtonHeight));
        w::primaryButton(ui_, ok, L"Добавить", glyph::check, ActAddSubmit);
    } else {
        editRect_ = manual;
        w::secondaryButton(ui_, manual, L"Ввести вручную", glyph::add, ActAddField);
    }
}

void MainWindow::drawServerRow(D2D1_RECT_F r, const Server& server, int index, bool selected, int ping) {
    Painter& p = painter_;
    bool broken = !server.unsupported.empty();
    w::cardButton(ui_, r, ActServerBase + index, selected);

    float cy = (r.top + r.bottom) / 2.f;
    w::flagAvatar(ui_, D2D1::Point2F(r.left + p.dp(34.f), cy), p.dp(20.f), server.flag, broken);

    float left = r.left + p.dp(64.f);
    float right = r.right - p.dp(16.f);

    // справа: крестик удаления, галочка выбора и пинг
    D2D1_RECT_F del = D2D1::RectF(right - p.dp(28.f), cy - p.dp(14.f), right, cy + p.dp(14.f));
    bool overRow = ui_.isHot(ActServerBase + index) || ui_.isHot(ActServerDelete + index);
    if (overRow) {
        p.text(glyph::trash, Font::Icon, del,
               ui_.isHot(ActServerDelete + index) ? theme::danger : theme::textMuted,
               Align::Center, VAlign::Middle);
        ui_.zone(del, ActServerDelete + index);
        right -= p.dp(36.f);
    }
    if (selected) {
        p.text(glyph::check, Font::Icon,
               D2D1::RectF(right - p.dp(24.f), cy - p.dp(12.f), right, cy + p.dp(12.f)),
               theme::accentBright, Align::Center, VAlign::Middle);
        right -= p.dp(32.f);
    }
    if (!broken) {
        w::pingBadge(ui_, D2D1::Point2F(right, cy), ping);
        right -= w::pingWidth(p, ping) + p.dp(10.f);
    }

    p.text(text::wide(server.name), Font::TitleMedium,
           D2D1::RectF(left, r.top + p.dp(10.f), right, cy + p.dp(1.f)),
           broken ? theme::textMuted : theme::text, Align::Left, VAlign::Bottom);

    std::wstring label = text::wide(protocolLabel(server.protocol));
    D2D1_POINT_2F chipAt = D2D1::Point2F(left, cy + p.dp(15.f));
    w::chip(ui_, chipAt, label, broken ? theme::textMuted : theme::accentBright);

    std::wstring tail = broken ? text::wide(server.unsupported) : text::wide(server.host);
    p.text(tail, Font::BodyMedium,
           D2D1::RectF(left + w::chipWidth(p, label) + p.dp(8.f), cy + p.dp(4.f), right, r.bottom - p.dp(6.f)),
           theme::textMuted, Align::Left, VAlign::Middle);
}

void MainWindow::drawServers(D2D1_RECT_F area) {
    Painter& p = painter_;
    AppState state = model_.state();
    std::map<std::string, int> pings = model_.pings();
    std::set<std::string> refreshing = model_.refreshing();

    // шапка не прокручивается
    float headerHeight = p.dp(52.f);
    size_t count = state.servers.size();
    std::wstring subtitle = count > 0 ? std::to_wstring(count) + L" " + fmt::serversWord(count) : L"";
    w::screenHeader(ui_, D2D1::RectF(area.left, area.top, area.right - p.dp(160.f), area.top + headerHeight),
                    L"Серверы", subtitle);

    float buttonR = p.dp(21.f);
    float cy = area.top + headerHeight / 2.f;
    float bx = area.right - buttonR;
    w::circleButton(ui_, D2D1::Point2F(bx, cy), buttonR, glyph::paste, ActPaste,
                    D2D1::ColorF(D2D1::ColorF::White), theme::accent);
    if (!state.subscriptions.empty()) {
        bx -= buttonR * 2 + p.dp(10.f);
        w::circleButton(ui_, D2D1::Point2F(bx, cy), buttonR, glyph::refresh, ActRefreshAll,
                        refreshing.empty() ? theme::text : theme::accentBright, theme::surface);
    }
    if (count > 0) {
        bx -= buttonR * 2 + p.dp(10.f);
        w::circleButton(ui_, D2D1::Point2F(bx, cy), buttonR, glyph::stopwatch, ActPingAll,
                        model_.pinging() ? theme::accentBright : theme::text, theme::surface);
    }

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + headerHeight + p.dp(10.f), area.right, area.bottom);
    if (state.servers.empty() && state.subscriptions.empty()) {
        contentHeight_ = 0.f;
        drawEmptyServers(list);
        return;
    }

    p.pushClip(list);
    float dy = -scroll();
    p.offset(0, dy);
    ui_.offsetY = dy;

    float y = list.top;
    float rowHeight = p.dp(66.f);

    // автовыбор
    D2D1_RECT_F autoRow = D2D1::RectF(list.left, y, list.right - p.dp(10.f), y + rowHeight);
    bool autoSelected = state.selectedId == kAutoServerId;
    w::cardButton(ui_, autoRow, ActAutoSelect, autoSelected);
    float acy = (autoRow.top + autoRow.bottom) / 2.f;
    p.circle(D2D1::Point2F(autoRow.left + p.dp(34.f), acy), p.dp(20.f), alpha(theme::accent, 0.18f));
    p.text(glyph::bolt, Font::Icon,
           D2D1::RectF(autoRow.left + p.dp(14.f), acy - p.dp(20.f), autoRow.left + p.dp(54.f), acy + p.dp(20.f)),
           theme::accentBright, Align::Center, VAlign::Middle);
    p.text(L"Автовыбор", Font::TitleMedium,
           D2D1::RectF(autoRow.left + p.dp(64.f), autoRow.top + p.dp(10.f), autoRow.right - p.dp(40.f), acy + p.dp(1.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(L"Подключаться к серверу с наименьшим пингом", Font::BodyMedium,
           D2D1::RectF(autoRow.left + p.dp(64.f), acy + p.dp(2.f), autoRow.right - p.dp(40.f), autoRow.bottom),
           theme::textMuted, Align::Left, VAlign::Top);
    if (autoSelected) {
        p.text(glyph::check, Font::Icon,
               D2D1::RectF(autoRow.right - p.dp(38.f), autoRow.top, autoRow.right - p.dp(14.f), autoRow.bottom),
               theme::accentBright, Align::Center, VAlign::Middle);
    }
    y = autoRow.bottom + p.dp(8.f);

    // группы: сначала подписки, потом добавленные руками
    auto drawGroup = [&](const std::wstring& title, const std::wstring& info, int subIndex) {
        D2D1_RECT_F header = D2D1::RectF(list.left + p.dp(6.f), y, list.right - p.dp(10.f), y + p.dp(52.f));
        p.text(title, Font::TitleLarge,
               D2D1::RectF(header.left, header.top + p.dp(10.f), header.right - p.dp(90.f), header.top + p.dp(32.f)),
               theme::text, Align::Left, VAlign::Middle);
        p.text(info, Font::BodyMedium,
               D2D1::RectF(header.left, header.top + p.dp(30.f), header.right - p.dp(90.f), header.bottom),
               theme::textMuted, Align::Left, VAlign::Middle);
        if (subIndex >= 0) {
            float hcy = (header.top + header.bottom) / 2.f;
            w::circleButton(ui_, D2D1::Point2F(header.right - p.dp(56.f), hcy), p.dp(16.f),
                            glyph::refresh, ActSubRefresh + subIndex, theme::textSecond, theme::surface);
            w::circleButton(ui_, D2D1::Point2F(header.right - p.dp(18.f), hcy), p.dp(16.f),
                            glyph::trash, ActSubDelete + subIndex, theme::textSecond, theme::surface);
        }
        y = header.bottom;
    };

    for (size_t s = 0; s < state.subscriptions.size(); s++) {
        const Subscription& sub = state.subscriptions[s];
        std::vector<size_t> indexes;
        for (size_t i = 0; i < state.servers.size(); i++) {
            if (state.servers[i].subscriptionId == sub.id) indexes.push_back(i);
        }

        std::wstring info = std::to_wstring(indexes.size()) + L" " + fmt::serversWord(indexes.size()) + L" · ";
        info += refreshing.count(sub.id) > 0 ? L"обновляется" : fmt::ago(sub.updatedAt);
        drawGroup(text::wide(sub.name), info, (int)s);

        // остаток трафика
        if (sub.totalBytes > 0) {
            float used = (float)(sub.uploadBytes + sub.downloadBytes);
            float part = used / (float)sub.totalBytes;
            if (part < 0.f) part = 0.f;
            if (part > 1.f) part = 1.f;
            D2D1_RECT_F track = D2D1::RectF(list.left + p.dp(6.f), y, list.right - p.dp(16.f), y + p.dp(4.f));
            p.round(track, p.dp(2.f), theme::surfaceHigh);
            p.round(D2D1::RectF(track.left, track.top, track.left + (track.right - track.left) * part, track.bottom),
                    p.dp(2.f), part > 0.9f ? theme::danger : theme::accent);
            std::wstring line = fmt::bytes(sub.uploadBytes + sub.downloadBytes) + L" из " + fmt::bytes(sub.totalBytes);
            if (sub.expireAt > 0) line += L" · до " + fmt::date(sub.expireAt);
            p.text(line, Font::BodyMedium,
                   D2D1::RectF(track.left, track.bottom + p.dp(4.f), track.right, track.bottom + p.dp(22.f)),
                   theme::textSecond, Align::Left, VAlign::Middle);
            y = track.bottom + p.dp(26.f);
        }

        for (size_t i : indexes) {
            D2D1_RECT_F row = D2D1::RectF(list.left, y, list.right - p.dp(10.f), y + rowHeight);
            auto it = pings.find(state.servers[i].id);
            drawServerRow(row, state.servers[i], (int)i,
                          state.servers[i].id == state.selectedId, it == pings.end() ? 0 : it->second);
            y = row.bottom + p.dp(6.f);
        }
        y += p.dp(6.f);
    }

    std::vector<size_t> manual;
    for (size_t i = 0; i < state.servers.size(); i++) {
        if (state.servers[i].subscriptionId.empty()) manual.push_back(i);
    }
    if (!manual.empty()) {
        drawGroup(L"Добавлены вручную",
                  std::to_wstring(manual.size()) + L" " + fmt::serversWord(manual.size()), -1);
        for (size_t i : manual) {
            D2D1_RECT_F row = D2D1::RectF(list.left, y, list.right - p.dp(10.f), y + rowHeight);
            auto it = pings.find(state.servers[i].id);
            drawServerRow(row, state.servers[i], (int)i,
                          state.servers[i].id == state.selectedId, it == pings.end() ? 0 : it->second);
            y = row.bottom + p.dp(6.f);
        }
    }

    contentHeight_ = y - list.top + p.dp(20.f);
    p.resetOffset();
    ui_.offsetY = 0.f;
    p.popClip();
    w::scrollbar(ui_, list, contentHeight_, scroll());
}

// --- маршруты ---

void MainWindow::drawRoutes(D2D1_RECT_F area) {
    Painter& p = painter_;
    AppState state = model_.state();
    const Routing& routing = state.routing;

    w::screenHeader(ui_, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Маршруты", L"Что идет через VPN, а что напрямую");

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + p.dp(62.f), area.right, area.bottom);
    p.pushClip(list);
    float dy = -scroll();
    p.offset(0, dy);
    ui_.offsetY = dy;

    float y = list.top;
    float width = list.right - list.left - p.dp(10.f);

    // режим
    float modeHeight = p.dp(152.f);
    if (routing.appMode == AppMode::Only && routing.apps.empty()) modeHeight += p.dp(24.f);
    D2D1_RECT_F mode = D2D1::RectF(list.left, y, list.left + width, y + modeHeight);
    w::card(ui_, mode);

    w::iconTile(ui_, D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(16.f),
                                 mode.left + p.dp(56.f), mode.top + p.dp(56.f)),
                glyph::apps, theme::accentBright);
    p.text(L"Программы", Font::TitleMedium,
           D2D1::RectF(mode.left + p.dp(70.f), mode.top + p.dp(16.f), mode.right - p.dp(16.f), mode.top + p.dp(38.f)),
           theme::text, Align::Left, VAlign::Middle);

    const wchar_t* hint = L"Весь трафик компьютера идет через VPN";
    if (routing.appMode == AppMode::Only) hint = L"Через VPN идут только выбранные программы";
    else if (routing.appMode == AppMode::Except) hint = L"Выбранные программы работают без VPN";
    p.text(hint, Font::BodyMedium,
           D2D1::RectF(mode.left + p.dp(70.f), mode.top + p.dp(36.f), mode.right - p.dp(16.f), mode.top + p.dp(56.f)),
           theme::textMuted, Align::Left, VAlign::Middle);

    w::segmented(ui_, D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(70.f),
                                  mode.right - p.dp(16.f), mode.top + p.dp(118.f)),
                 { L"Все", L"Только", L"Кроме" }, (int)routing.appMode, ActModeBase);

    if (routing.appMode == AppMode::Only && routing.apps.empty()) {
        p.text(L"Отметьте программы ниже. Пока ничего не выбрано, VPN работает для всех",
               Font::BodyMedium,
               D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(124.f), mode.right - p.dp(16.f), mode.bottom - p.dp(6.f)),
               theme::warning, Align::Left, VAlign::Middle);
    }
    y = mode.bottom + p.dp(18.f);

    // готовые наборы
    w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Напрямую, без VPN");
    y += p.dp(28.f);

    D2D1_RECT_F presets = D2D1::RectF(list.left, y, list.left + width, y + p.dp(148.f));
    w::card(ui_, presets);

    struct SwitchItem { const wchar_t* icon; const wchar_t* title; const wchar_t* subtitle; bool on; int id; };
    SwitchItem items[2] = {
        { glyph::bank, L"Банки и Госуслуги", L"Сбер, Т-Банк, Альфа, ВТБ, Госуслуги", routing.bypassBanks, ActBypassBanks },
        { glyph::globe, L"Российские сайты", L".ru, .рф, Яндекс, VK, Mail.ru", routing.directRuSites, ActDirectRu },
    };
    for (int i = 0; i < 2; i++) {
        float rowTop = presets.top + p.dp(8.f) + i * p.dp(66.f);
        D2D1_RECT_F row = D2D1::RectF(presets.left, rowTop, presets.right, rowTop + p.dp(66.f));
        if (ui_.isHot(items[i].id)) {
            p.round(w::inset(row, p.dp(6.f), p.dp(3.f)), p.dp(14.f), theme::hover());
        }
        float rcy = (row.top + row.bottom) / 2.f;
        w::iconTile(ui_, D2D1::RectF(row.left + p.dp(16.f), rcy - p.dp(20.f), row.left + p.dp(56.f), rcy + p.dp(20.f)),
                    items[i].icon, theme::accentBright);
        p.text(items[i].title, Font::TitleMedium,
               D2D1::RectF(row.left + p.dp(70.f), row.top + p.dp(12.f), row.right - p.dp(80.f), rcy + p.dp(1.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(items[i].subtitle, Font::BodyMedium,
               D2D1::RectF(row.left + p.dp(70.f), rcy + p.dp(2.f), row.right - p.dp(80.f), row.bottom - p.dp(10.f)),
               theme::textMuted, Align::Left, VAlign::Top);
        w::toggle(ui_, D2D1::Point2F(row.right - p.dp(16.f), rcy), items[i].on, items[i].id);
        ui_.zone(row, items[i].id);
        if (i == 0) w::divider(ui_, D2D1::RectF(row.left + p.dp(70.f), row.bottom, row.right - p.dp(16.f), row.bottom));
    }
    y = presets.bottom + p.dp(18.f);

    // свои домены
    w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Свои сайты напрямую");
    y += p.dp(28.f);

    const std::vector<std::string>& domains = routing.directDomains;
    int rows = (int)((domains.size() + 2) / 3);
    float domainsHeight = p.dp(92.f) + (domains.empty() ? p.dp(24.f) : rows * p.dp(36.f));
    D2D1_RECT_F box = D2D1::RectF(list.left, y, list.left + width, y + domainsHeight);
    w::card(ui_, box);

    D2D1_RECT_F input = D2D1::RectF(box.left + p.dp(16.f), box.top + p.dp(16.f),
                                    box.right - p.dp(72.f), box.top + p.dp(16.f) + p.dp(w::kFieldHeight));
    bool editing = editTarget_ == EditTarget::Domain;
    if (!editing) editRect_ = input;
    w::field(ui_, input, L"kinopoisk.ru", true, editing, ActDomainField);
    w::circleButton(ui_, D2D1::Point2F(box.right - p.dp(40.f), (input.top + input.bottom) / 2.f),
                    p.dp(24.f), glyph::add, ActDomainAdd, D2D1::ColorF(D2D1::ColorF::White), theme::accent);

    if (domains.empty()) {
        p.text(L"Сайт и все его поддомены будут открываться без VPN", Font::BodyMedium,
               D2D1::RectF(box.left + p.dp(16.f), input.bottom + p.dp(8.f), box.right - p.dp(16.f), box.bottom),
               theme::textMuted, Align::Left, VAlign::Top);
    } else {
        float chipX = box.left + p.dp(16.f);
        float chipY = input.bottom + p.dp(18.f);
        for (size_t i = 0; i < domains.size(); i++) {
            std::wstring name = text::wide(domains[i]);
            float chipW = p.measure(name, Font::BodyMedium, width).width + p.dp(44.f);
            if (chipX + chipW > box.right - p.dp(16.f)) {
                chipX = box.left + p.dp(16.f);
                chipY += p.dp(36.f);
            }
            D2D1_RECT_F chipRect = D2D1::RectF(chipX, chipY - p.dp(14.f), chipX + chipW, chipY + p.dp(14.f));
            bool hot = ui_.isHot(ActDomainBase + (int)i);
            p.round(chipRect, p.dp(14.f), hot ? alpha(theme::danger, 0.14f) : theme::surfaceHigh);
            p.roundBorder(chipRect, p.dp(14.f), hot ? alpha(theme::danger, 0.4f) : theme::stroke());
            p.text(name, Font::BodyMedium,
                   D2D1::RectF(chipRect.left + p.dp(12.f), chipRect.top, chipRect.right - p.dp(26.f), chipRect.bottom),
                   theme::text, Align::Left, VAlign::Middle);
            p.text(glyph::close, Font::Icon,
                   D2D1::RectF(chipRect.right - p.dp(26.f), chipRect.top, chipRect.right - p.dp(6.f), chipRect.bottom),
                   hot ? theme::danger : theme::textMuted, Align::Center, VAlign::Middle);
            ui_.zone(chipRect, ActDomainBase + (int)i);
            chipX += chipW + p.dp(8.f);
        }
    }
    y = box.bottom + p.dp(18.f);

    // список программ
    if (routing.appMode != AppMode::All) {
        std::wstring title = routing.appMode == AppMode::Only ? L"Через VPN" : L"Без VPN";
        w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)),
                        title + L" · " + std::to_wstring(routing.apps.size()));
        y += p.dp(28.f);

        std::string search = text::lower(text::narrow(appSearch_));
        for (size_t i = 0; i < apps_.size(); i++) {
            const processes::Item& app = apps_[i];
            if (!search.empty() && app.exe.find(search) == std::string::npos) continue;

            bool checked = false;
            for (const std::string& selected : routing.apps) {
                if (selected == app.exe) {
                    checked = true;
                    break;
                }
            }

            D2D1_RECT_F row = D2D1::RectF(list.left, y, list.left + width, y + p.dp(52.f));
            int id = ActAppBase + (int)i;
            if (ui_.isHot(id)) {
                p.round(row, p.dp(14.f), theme::hover());
            }
            float rcy = (row.top + row.bottom) / 2.f;
            p.round(D2D1::RectF(row.left + p.dp(12.f), rcy - p.dp(16.f), row.left + p.dp(44.f), rcy + p.dp(16.f)),
                    p.dp(9.f), theme::surfaceHigh);
            p.text(app.title.substr(0, 1), Font::TitleMedium,
                   D2D1::RectF(row.left + p.dp(12.f), rcy - p.dp(16.f), row.left + p.dp(44.f), rcy + p.dp(16.f)),
                   theme::textSecond, Align::Center, VAlign::Middle);
            p.text(app.title, Font::TitleMedium,
                   D2D1::RectF(row.left + p.dp(56.f), row.top, row.right - p.dp(60.f), rcy + p.dp(2.f)),
                   theme::text, Align::Left, VAlign::Bottom);
            p.text(text::wide(app.exe), Font::BodyMedium,
                   D2D1::RectF(row.left + p.dp(56.f), rcy, row.right - p.dp(60.f), row.bottom),
                   theme::textMuted, Align::Left, VAlign::Top);
            w::checkCircle(ui_, D2D1::Point2F(row.right - p.dp(26.f), rcy), checked);
            ui_.zone(row, id);
            y = row.bottom + p.dp(2.f);
        }
    }

    contentHeight_ = y - list.top + p.dp(20.f);
    p.resetOffset();
    ui_.offsetY = 0.f;
    p.popClip();
    w::scrollbar(ui_, list, contentHeight_, scroll());
}

// --- настройки ---

void MainWindow::drawSettings(D2D1_RECT_F area) {
    Painter& p = painter_;
    AppState state = model_.state();

    w::screenHeader(ui_, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Настройки", L"");

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + p.dp(62.f), area.right, area.bottom);
    p.pushClip(list);
    float dy = -scroll();
    p.offset(0, dy);
    ui_.offsetY = dy;

    float y = list.top;
    float width = list.right - list.left - p.dp(10.f);

    w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Подключение");
    y += p.dp(28.f);

    struct SwitchItem { const wchar_t* icon; const wchar_t* title; const wchar_t* subtitle; bool on; int id; };
    SwitchItem items[3] = {
        { glyph::power, L"Подключать при запуске", L"Включать VPN, когда открываете Vinyl",
          state.settings.autoConnect, ActAutoConnect },
        { glyph::play, L"Запускать вместе с Windows", L"Vinyl появится в автозагрузке",
          autostart::enabled(), ActAutostart },
        { glyph::globe, L"IPv6", L"Отдавать программам IPv6-адреса", state.settings.ipv6, ActIpv6 },
    };

    D2D1_RECT_F connection = D2D1::RectF(list.left, y, list.left + width, y + p.dp(212.f));
    w::card(ui_, connection);
    for (int i = 0; i < 3; i++) {
        float rowTop = connection.top + p.dp(8.f) + i * p.dp(66.f);
        D2D1_RECT_F row = D2D1::RectF(connection.left, rowTop, connection.right, rowTop + p.dp(66.f));
        if (ui_.isHot(items[i].id)) p.round(w::inset(row, p.dp(6.f), p.dp(3.f)), p.dp(14.f), theme::hover());

        float rcy = (row.top + row.bottom) / 2.f;
        w::iconTile(ui_, D2D1::RectF(row.left + p.dp(16.f), rcy - p.dp(20.f), row.left + p.dp(56.f), rcy + p.dp(20.f)),
                    items[i].icon, theme::accentBright);
        p.text(items[i].title, Font::TitleMedium,
               D2D1::RectF(row.left + p.dp(70.f), row.top + p.dp(12.f), row.right - p.dp(80.f), rcy + p.dp(1.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(items[i].subtitle, Font::BodyMedium,
               D2D1::RectF(row.left + p.dp(70.f), rcy + p.dp(2.f), row.right - p.dp(80.f), row.bottom - p.dp(10.f)),
               theme::textMuted, Align::Left, VAlign::Top);
        w::toggle(ui_, D2D1::Point2F(row.right - p.dp(16.f), rcy), items[i].on, items[i].id);
        ui_.zone(row, items[i].id);
        if (i < 2) w::divider(ui_, D2D1::RectF(row.left + p.dp(70.f), row.bottom, row.right - p.dp(16.f), row.bottom));
    }
    y = connection.bottom + p.dp(18.f);

    // dns
    w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"DNS");
    y += p.dp(28.f);

    D2D1_RECT_F dns = D2D1::RectF(list.left, y, list.left + width, y + p.dp(114.f));
    w::card(ui_, dns);
    w::segmented(ui_, D2D1::RectF(dns.left + p.dp(16.f), dns.top + p.dp(16.f),
                                  dns.right - p.dp(16.f), dns.top + p.dp(64.f)),
                 { L"Cloudflare", L"Google", L"Quad9" }, (int)state.settings.dns, ActDnsBase);
    p.text(L"Запросы шифруются (DNS-over-HTTPS) и уходят через сервер VPN", Font::BodyMedium,
           D2D1::RectF(dns.left + p.dp(16.f), dns.top + p.dp(72.f), dns.right - p.dp(16.f), dns.bottom - p.dp(10.f)),
           theme::textMuted, Align::Left, VAlign::Middle);
    y = dns.bottom + p.dp(18.f);

    // журнал
    w::sectionLabel(ui_, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Диагностика");
    y += p.dp(28.f);

    float logHeight = p.dp(76.f) + p.dp(240.f);
    D2D1_RECT_F logs = D2D1::RectF(list.left, y, list.left + width, y + logHeight);
    w::card(ui_, logs);

    float lcy = logs.top + p.dp(38.f);
    w::iconTile(ui_, D2D1::RectF(logs.left + p.dp(16.f), lcy - p.dp(20.f), logs.left + p.dp(56.f), lcy + p.dp(20.f)),
                glyph::terminal, theme::accentBright);
    p.text(L"Журнал ядра", Font::TitleMedium,
           D2D1::RectF(logs.left + p.dp(70.f), logs.top + p.dp(14.f), logs.right - p.dp(200.f), lcy + p.dp(1.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(L"Здесь видно, почему не удалось подключиться", Font::BodyMedium,
           D2D1::RectF(logs.left + p.dp(70.f), lcy + p.dp(2.f), logs.right - p.dp(200.f), logs.top + p.dp(66.f)),
           theme::textMuted, Align::Left, VAlign::Top);

    float bx = logs.right - p.dp(34.f);
    w::circleButton(ui_, D2D1::Point2F(bx, lcy), p.dp(18.f), glyph::refresh, ActLogRefresh, theme::text, theme::surfaceHigh);
    bx -= p.dp(46.f);
    w::circleButton(ui_, D2D1::Point2F(bx, lcy), p.dp(18.f), glyph::copy, ActLogCopy, theme::text, theme::surfaceHigh);
    bx -= p.dp(46.f);
    w::circleButton(ui_, D2D1::Point2F(bx, lcy), p.dp(18.f), glyph::list, ActLogFolder, theme::text, theme::surfaceHigh);

    D2D1_RECT_F box = D2D1::RectF(logs.left + p.dp(16.f), logs.top + p.dp(76.f),
                                  logs.right - p.dp(16.f), logs.bottom - p.dp(16.f));
    p.round(box, p.dp(14.f), theme::background);
    p.roundBorder(box, p.dp(14.f), theme::stroke());
    p.pushClip(box);
    if (logLines_.empty()) {
        p.text(L"Пока пусто", Font::Mono,
               D2D1::RectF(box.left + p.dp(12.f), box.top + p.dp(10.f), box.right, box.top + p.dp(28.f)),
               theme::textMuted, Align::Left, VAlign::Middle);
    } else {
        float ly = box.top + p.dp(8.f);
        for (const std::wstring& line : logLines_) {
            if (ly > box.bottom - p.dp(14.f)) break;
            D2D1_COLOR_F color = theme::textSecond;
            if (line.find(L"FATAL") != std::wstring::npos || line.find(L"ERROR") != std::wstring::npos) color = theme::danger;
            else if (line.find(L"WARN") != std::wstring::npos) color = theme::warning;
            p.text(line, Font::Mono,
                   D2D1::RectF(box.left + p.dp(12.f), ly, box.right - p.dp(10.f), ly + p.dp(16.f)),
                   color, Align::Left, VAlign::Middle);
            ly += p.dp(16.f);
        }
    }
    p.popClip();
    y = logs.bottom + p.dp(28.f);

    // подвал
    float cx = (list.left + list.left + width) / 2.f;
    p.circleGradient(D2D1::Point2F(cx, y + p.dp(26.f)), p.dp(26.f),
                     alpha(theme::accentBright, 0.5f), alpha(theme::accentDeep, 0.35f));
    p.circle(D2D1::Point2F(cx, y + p.dp(26.f)), p.dp(7.f), theme::background);
    p.text(L"V I N Y L", Font::TitleMedium,
           D2D1::RectF(list.left, y + p.dp(60.f), list.left + width, y + p.dp(82.f)),
           theme::textSecond, Align::Center, VAlign::Middle);
    p.text(text::wide(std::string("версия ") + install::kVersion), Font::BodyMedium,
           D2D1::RectF(list.left, y + p.dp(82.f), list.left + width, y + p.dp(102.f)),
           theme::textMuted, Align::Center, VAlign::Middle);

    std::wstring dev = L"Developer  claustrophobDev";
    float devWidth = p.measure(dev, Font::LabelMedium, width).width + p.dp(36.f);
    D2D1_RECT_F devRect = D2D1::RectF(cx - devWidth / 2, y + p.dp(114.f), cx + devWidth / 2, y + p.dp(150.f));
    p.round(devRect, p.dp(18.f), ui_.isHot(ActGithub) ? theme::surfaceHigh : theme::surface);
    p.roundBorder(devRect, p.dp(18.f), theme::stroke());
    p.text(dev, Font::LabelMedium, devRect, theme::accentBright, Align::Center, VAlign::Middle);
    ui_.zone(devRect, ActGithub);

    y = devRect.bottom;
    contentHeight_ = y - list.top + p.dp(24.f);
    p.resetOffset();
    ui_.offsetY = 0.f;
    p.popClip();
    w::scrollbar(ui_, list, contentHeight_, scroll());
}
