#include "ui/views/ServersView.h"

#include "core/Text.h"
#include "ui/Format.h"
#include "ui/Theme.h"

using w::alpha;

float ServersView::draw(ViewContext& c, D2D1_RECT_F area) {
    Painter& p = c.p;
    const AppState& state = c.state;

    float headerHeight = p.dp(52.f);
    size_t count = state.servers.size();
    std::wstring subtitle = count > 0 ? std::to_wstring(count) + L" " + fmt::serversWord(count) : L"";
    w::screenHeader(c.ui, D2D1::RectF(area.left, area.top, area.right - p.dp(160.f), area.top + headerHeight),
                    L"Серверы", subtitle);

    float buttonR = p.dp(21.f);
    float cy = area.top + headerHeight / 2.f;
    float bx = area.right - buttonR;
    w::circleButton(c.ui, D2D1::Point2F(bx, cy), buttonR, glyph::paste, ActPaste,
                    D2D1::ColorF(D2D1::ColorF::White), theme::accent);
    if (!state.subscriptions.empty()) {
        bx -= buttonR * 2 + p.dp(10.f);
        w::circleButton(c.ui, D2D1::Point2F(bx, cy), buttonR, glyph::refresh, ActRefreshAll,
                        c.refreshing.empty() ? theme::text : theme::accentBright, theme::surface);
    }
    if (count > 0) {
        bx -= buttonR * 2 + p.dp(10.f);
        w::circleButton(c.ui, D2D1::Point2F(bx, cy), buttonR, glyph::stopwatch, ActPingAll,
                        c.pinging ? theme::accentBright : theme::text, theme::surface);
    }

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + headerHeight + p.dp(10.f), area.right, area.bottom);
    if (state.servers.empty() && state.subscriptions.empty()) {
        drawEmpty(c, list);
        return 0.f;
    }

    p.pushClip(list);
    float dy = -c.scroll;
    p.offset(0, dy);
    c.ui.offsetY = dy;

    float y = list.top;
    float rowHeight = p.dp(66.f);

    D2D1_RECT_F autoRow = D2D1::RectF(list.left, y, list.right - p.dp(10.f), y + rowHeight);
    bool autoSelected = state.selectedId == kAutoServerId;
    w::cardButton(c.ui, autoRow, ActAutoSelect, autoSelected);
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

    for (size_t s = 0; s < state.subscriptions.size(); s++) {
        const Subscription& sub = state.subscriptions[s];
        std::vector<size_t> indexes;
        for (size_t i = 0; i < state.servers.size(); i++) {
            if (state.servers[i].subscriptionId == sub.id) indexes.push_back(i);
        }

        std::wstring info = std::to_wstring(indexes.size()) + L" " + fmt::serversWord(indexes.size()) + L" · ";
        info += c.refreshing.count(sub.id) > 0 ? L"обновляется" : fmt::ago(sub.updatedAt);
        y = drawGroupHeader(c, list, y, text::wide(sub.name), info, (int)s);

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
            // рисуем только видимое, иначе на большой подписке прокрутка дергается
            if (row.bottom + dy >= list.top && row.top + dy <= list.bottom) {
                auto it = c.pings.find(state.servers[i].id);
                drawRow(c, row, state.servers[i], (int)i,
                        state.servers[i].id == state.selectedId, it == c.pings.end() ? 0 : it->second);
            }
            y = row.bottom + p.dp(6.f);
        }
        y += p.dp(6.f);
    }

    std::vector<size_t> manual;
    for (size_t i = 0; i < state.servers.size(); i++) {
        if (state.servers[i].subscriptionId.empty()) manual.push_back(i);
    }
    if (!manual.empty()) {
        y = drawGroupHeader(c, list, y, L"Добавлены вручную",
                            std::to_wstring(manual.size()) + L" " + fmt::serversWord(manual.size()), -1);
        for (size_t i : manual) {
            D2D1_RECT_F row = D2D1::RectF(list.left, y, list.right - p.dp(10.f), y + rowHeight);
            if (row.bottom + dy >= list.top && row.top + dy <= list.bottom) {
                auto it = c.pings.find(state.servers[i].id);
                drawRow(c, row, state.servers[i], (int)i,
                        state.servers[i].id == state.selectedId, it == c.pings.end() ? 0 : it->second);
            }
            y = row.bottom + p.dp(6.f);
        }
    }

    float contentHeight = y - list.top + p.dp(20.f);
    p.resetOffset();
    c.ui.offsetY = 0.f;
    p.popClip();
    w::scrollbar(c.ui, list, contentHeight, c.scroll);
    return contentHeight;
}

float ServersView::drawGroupHeader(ViewContext& c, D2D1_RECT_F area, float y, const std::wstring& title,
                                   const std::wstring& info, int subIndex) {
    Painter& p = c.p;
    D2D1_RECT_F header = D2D1::RectF(area.left + p.dp(6.f), y, area.right - p.dp(10.f), y + p.dp(52.f));

    p.text(title, Font::TitleLarge,
           D2D1::RectF(header.left, header.top + p.dp(10.f), header.right - p.dp(90.f), header.top + p.dp(32.f)),
           theme::text, Align::Left, VAlign::Middle);
    p.text(info, Font::BodyMedium,
           D2D1::RectF(header.left, header.top + p.dp(30.f), header.right - p.dp(90.f), header.bottom),
           theme::textMuted, Align::Left, VAlign::Middle);

    if (subIndex >= 0) {
        float hcy = (header.top + header.bottom) / 2.f;
        w::circleButton(c.ui, D2D1::Point2F(header.right - p.dp(56.f), hcy), p.dp(16.f),
                        glyph::refresh, ActSubRefresh + subIndex, theme::textSecond, theme::surface);
        w::circleButton(c.ui, D2D1::Point2F(header.right - p.dp(18.f), hcy), p.dp(16.f),
                        glyph::trash, ActSubDelete + subIndex, theme::textSecond, theme::surface);
    }
    return header.bottom;
}

void ServersView::drawRow(ViewContext& c, D2D1_RECT_F r, const Server& server, int index,
                          bool selected, int ping) {
    Painter& p = c.p;
    bool broken = !server.unsupported.empty();
    w::cardButton(c.ui, r, ActServerBase + index, selected);

    float cy = (r.top + r.bottom) / 2.f;
    w::flagAvatar(c.ui, D2D1::Point2F(r.left + p.dp(34.f), cy), p.dp(16.f), server.flag, broken);

    float left = r.left + p.dp(64.f);
    float right = r.right - p.dp(16.f);

    D2D1_RECT_F del = D2D1::RectF(right - p.dp(28.f), cy - p.dp(14.f), right, cy + p.dp(14.f));
    bool overRow = c.ui.isHot(ActServerBase + index) || c.ui.isHot(ActServerDelete + index);
    if (overRow) {
        p.text(glyph::trash, Font::Icon, del,
               c.ui.isHot(ActServerDelete + index) ? theme::danger : theme::textMuted,
               Align::Center, VAlign::Middle);
        c.ui.zone(del, ActServerDelete + index);
        right -= p.dp(36.f);
    }
    if (selected) {
        p.text(glyph::check, Font::Icon,
               D2D1::RectF(right - p.dp(24.f), cy - p.dp(12.f), right, cy + p.dp(12.f)),
               theme::accentBright, Align::Center, VAlign::Middle);
        right -= p.dp(32.f);
    }
    if (!broken) {
        w::pingBadge(c.ui, D2D1::Point2F(right, cy), ping);
        right -= w::pingWidth(p, ping) + p.dp(10.f);
    }

    p.text(text::wide(server.name), Font::TitleMedium,
           D2D1::RectF(left, r.top + p.dp(10.f), right, cy + p.dp(1.f)),
           broken ? theme::textMuted : theme::text, Align::Left, VAlign::Bottom);

    std::wstring label = text::wide(protocolLabel(server.protocol));
    w::chip(c.ui, D2D1::Point2F(left, cy + p.dp(15.f)), label, broken ? theme::textMuted : theme::accentBright);

    std::wstring tail = broken ? text::wide(server.unsupported) : text::wide(server.host);
    p.text(tail, Font::BodyMedium,
           D2D1::RectF(left + w::chipWidth(p, label) + p.dp(8.f), cy + p.dp(4.f), right, r.bottom - p.dp(6.f)),
           theme::textMuted, Align::Left, VAlign::Middle);
}

void ServersView::drawEmpty(ViewContext& c, D2D1_RECT_F area) {
    Painter& p = c.p;
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
    w::primaryButton(c.ui, paste, L"Вставить из буфера", glyph::paste, ActPaste);

    D2D1_RECT_F manual = D2D1::RectF(cx - buttonWidth / 2, paste.bottom + p.dp(12.f),
                                     cx + buttonWidth / 2, paste.bottom + p.dp(12.f) + p.dp(w::kButtonHeight));
    if (c.editingAdd) {
        w::field(c.ui, manual, L"", false, true, ActAddField);
        D2D1_RECT_F ok = D2D1::RectF(manual.left, manual.bottom + p.dp(12.f),
                                     manual.right, manual.bottom + p.dp(12.f) + p.dp(w::kButtonHeight));
        w::primaryButton(c.ui, ok, L"Добавить", glyph::check, ActAddSubmit);
    } else {
        if (c.editRect != nullptr) *c.editRect = manual;
        w::secondaryButton(c.ui, manual, L"Ввести вручную", glyph::add, ActAddField);
    }
}
