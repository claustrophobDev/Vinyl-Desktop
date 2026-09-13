#include "ui/views/HomeView.h"

#include "core/Text.h"
#include "ui/Format.h"
#include "ui/Theme.h"

using w::alpha;

float HomeView::draw(ViewContext& c, D2D1_RECT_F area) {
    Painter& p = c.p;

    w::screenHeader(c.ui, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Главная", L"");

    float top = area.top + p.dp(62.f);
    float bottom = area.bottom;

    float cardHeight = p.dp(76.f);
    D2D1_RECT_F serverCard = D2D1::RectF(area.left, bottom - cardHeight, area.right, bottom);

    float tilesHeight = p.dp(76.f);
    float tilesTop = serverCard.top - p.dp(12.f) - tilesHeight;
    float gap = p.dp(12.f);
    float tileWidth = (area.right - area.left - gap) / 2.f;

    bool active = c.vpn == VpnState::Running;
    drawTile(c, D2D1::RectF(area.left, tilesTop, area.left + tileWidth, tilesTop + tilesHeight),
             glyph::download, L"Загрузка",
             active ? fmt::speed(c.stats.downSpeed()) : L"—",
             active ? fmt::bytes(c.stats.downTotal()) : L"",
             theme::success);
    drawTile(c, D2D1::RectF(area.right - tileWidth, tilesTop, area.right, tilesTop + tilesHeight),
             glyph::upload, L"Отдача",
             active ? fmt::speed(c.stats.upSpeed()) : L"—",
             active ? fmt::bytes(c.stats.upTotal()) : L"",
             theme::accentBright);

    float discBottom = tilesTop - p.dp(16.f);
    if (c.needReconnect && active) {
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
        c.ui.zone(banner, ActReconnect);
        discBottom = banner.top - p.dp(12.f);
    }

    float available = discBottom - top;
    float radius = available / 2.f - p.dp(38.f);
    float maxRadius = (area.right - area.left) * 0.30f;
    if (radius > maxRadius) radius = maxRadius;

    if (radius > p.dp(40.f)) {
        D2D1_POINT_2F center = D2D1::Point2F((area.left + area.right) / 2.f, top + p.dp(30.f) + radius);
        D2D1_COLOR_F color = statusColor(c.vpn);
        std::wstring label = statusText(c.vpn);

        float pillWidth = p.measure(label, Font::LabelLarge, area.right - area.left).width + p.dp(44.f);
        float pillHeight = p.dp(32.f);
        D2D1_RECT_F pill = D2D1::RectF(center.x - pillWidth / 2, top, center.x + pillWidth / 2, top + pillHeight);
        p.round(pill, pillHeight / 2, alpha(color, 0.10f));
        p.circle(D2D1::Point2F(pill.left + p.dp(16.f), (pill.top + pill.bottom) / 2), p.dp(4.f), color);
        p.text(label, Font::LabelLarge,
               D2D1::RectF(pill.left + p.dp(26.f), pill.top, pill.right, pill.bottom),
               color, Align::Center, VAlign::Middle);

        drawDisc(c, center, radius);

        D2D1_RECT_F under = D2D1::RectF(area.left, center.y + radius + p.dp(10.f),
                                        area.right, center.y + radius + p.dp(62.f));
        if (c.vpn == VpnState::Running) {
            p.text(fmt::duration(c.stats.uptime()), Font::Display, under, theme::text, Align::Center, VAlign::Middle);
        } else if (c.vpn == VpnState::Error && !c.vpnError.empty()) {
            p.text(c.vpnError, Font::BodyMedium, under, theme::danger, Align::Center, VAlign::Middle);
        } else {
            p.text(L"Нажмите на пластинку", Font::BodyLarge, under, theme::textMuted, Align::Center, VAlign::Middle);
        }

        c.ui.zone(D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius),
                  ActConnect);
    }

    drawServerCard(c, serverCard);
    return 0.f;
}

void HomeView::drawDisc(ViewContext& c, D2D1_POINT_2F center, float radius) {
    Painter& p = c.p;
    bool error = c.vpn == VpnState::Error;
    float g = c.glow;

    p.glow(center, radius * 1.5f, error ? theme::danger : theme::accent, error ? 0.35f : g);
    p.circleGradient(center, radius, D2D1::ColorF(0x1C1A26), D2D1::ColorF(0x050409));

    float grooveR = radius * 0.44f;
    int index = 0;
    while (grooveR < radius * 0.965f) {
        float a = (index % 4 == 0) ? 0.075f : 0.032f;
        p.ring(center, grooveR, D2D1::ColorF(0xFFFFFF, a), 0.8f);
        grooveR += radius * 0.025f;
        index++;
    }
    p.ring(center, radius - 0.75f, D2D1::ColorF(0xFFFFFF, 0.10f), 1.5f);

    // по этой дуге и видно что пластинка крутится
    D2D1_COLOR_F arcColor = error ? alpha(theme::danger, 0.5f)
                                  : alpha(theme::accentBright, 0.18f + 0.4f * g);
    p.arc(center, radius * 0.8f, c.angle - 65.f, 48.f, arcColor, 1.4f);
    p.arc(center, radius * 0.62f, c.angle + 150.f, 26.f,
          alpha(D2D1::ColorF(0xFFFFFF), 0.06f + 0.12f * g), 1.2f);

    float label = radius * 0.40f;
    p.circleGradient(center, label,
                     D2D1::ColorF(0.17f + 0.09f * g, 0.14f + 0.03f * g, 0.24f + 0.26f * g),
                     D2D1::ColorF(0.09f, 0.08f, 0.14f + 0.09f * g));
    D2D1_COLOR_F ringColor = error ? alpha(theme::danger, 0.6f)
                                   : D2D1::ColorF(theme::accentBright.r, theme::accentBright.g,
                                                  theme::accentBright.b, 0.10f + 0.55f * g);
    p.ring(center, label - 0.75f, ringColor, 1.0f);

    // дырку рисуем до надписи, иначе она перечеркивает буквы
    p.circle(center, radius * 0.055f, theme::background);
    p.ring(center, radius * 0.055f, D2D1::ColorF(0xFFFFFF, 0.12f), 1.0f);

    D2D1_COLOR_F markColor = D2D1::ColorF(theme::text.r, theme::text.g, theme::text.b, 0.45f + 0.45f * g);
    p.text(L"V I N Y L", Font::LabelSmall,
           D2D1::RectF(center.x - label, center.y - label * 0.74f, center.x + label, center.y - label * 0.18f),
           markColor, Align::Center, VAlign::Middle);
    p.line(D2D1::Point2F(center.x - label * 0.34f, center.y + label * 0.40f),
           D2D1::Point2F(center.x + label * 0.34f, center.y + label * 0.40f),
           D2D1::ColorF(0xFFFFFF, 0.06f + 0.10f * g), 1.0f);
}

void HomeView::drawTile(ViewContext& c, D2D1_RECT_F r, const wchar_t* icon, const std::wstring& label,
                        const std::wstring& value, const std::wstring& total, const D2D1_COLOR_F& tint) {
    Painter& p = c.p;
    w::card(c.ui, r);

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

void HomeView::drawServerCard(ViewContext& c, D2D1_RECT_F r) {
    Painter& p = c.p;
    w::cardButton(c.ui, r, ActServerCard);
    float cy = (r.top + r.bottom) / 2.f;

    if (c.server == nullptr) {
        p.circle(D2D1::Point2F(r.left + p.dp(38.f), cy), p.dp(22.f), theme::accentSoft());
        p.text(glyph::add, Font::Icon,
               D2D1::RectF(r.left + p.dp(16.f), cy - p.dp(22.f), r.left + p.dp(60.f), cy + p.dp(22.f)),
               theme::accentBright, Align::Center, VAlign::Middle);
        p.text(L"Добавьте сервер", Font::TitleMedium,
               D2D1::RectF(r.left + p.dp(72.f), r.top + p.dp(16.f), r.right, cy + p.dp(2.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(L"Подписка или ключ из буфера обмена", Font::BodyMedium,
               D2D1::RectF(r.left + p.dp(72.f), cy + p.dp(2.f), r.right, r.bottom),
               theme::textMuted, Align::Left, VAlign::Top);
        return;
    }

    const Server& server = *c.server;
    w::flagAvatar(c.ui, D2D1::Point2F(r.left + p.dp(38.f), cy), p.dp(18.f), server.flag, false);

    std::wstring subtitle = text::wide(std::string(protocolLabel(server.protocol)) + " · " + server.host);
    if (c.state.selectedId == kAutoServerId) subtitle = L"Автовыбор · " + subtitle;

    p.text(text::wide(server.name), Font::TitleMedium,
           D2D1::RectF(r.left + p.dp(72.f), r.top + p.dp(16.f), r.right - p.dp(120.f), cy + p.dp(2.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(subtitle, Font::BodyMedium,
           D2D1::RectF(r.left + p.dp(72.f), cy + p.dp(2.f), r.right - p.dp(120.f), r.bottom),
           theme::textMuted, Align::Left, VAlign::Top);

    auto it = c.pings.find(server.id);
    w::pingBadge(c.ui, D2D1::Point2F(r.right - p.dp(46.f), cy), it == c.pings.end() ? 0 : it->second);
    p.text(glyph::chevron, Font::Icon,
           D2D1::RectF(r.right - p.dp(36.f), r.top, r.right - p.dp(12.f), r.bottom),
           theme::textMuted, Align::Center, VAlign::Middle);
}
