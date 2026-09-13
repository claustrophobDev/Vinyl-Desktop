#include "ui/views/SettingsView.h"

#include "app/Install.h"
#include "core/Text.h"

float SettingsView::draw(ViewContext& c, D2D1_RECT_F area) {
    Painter& p = c.p;

    w::screenHeader(c.ui, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Настройки", L"");

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + p.dp(62.f), area.right, area.bottom);
    p.pushClip(list);
    float dy = -c.scroll;
    p.offset(0, dy);
    c.ui.offsetY = dy;

    float width = list.right - list.left - p.dp(10.f);
    float y = list.top;

    y = drawConnection(c, list, y, width);
    y = drawDns(c, list, y, width);
    y = drawLog(c, list, y, width);
    y = drawFooter(c, list, y, width);

    float contentHeight = y - list.top + p.dp(24.f);
    p.resetOffset();
    c.ui.offsetY = 0.f;
    p.popClip();
    w::scrollbar(c.ui, list, contentHeight, c.scroll);
    return contentHeight;
}

float SettingsView::drawConnection(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;
    const Settings& settings = c.state.settings;

    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Подключение");
    y += p.dp(28.f);

    struct Item {
        const wchar_t* icon;
        const wchar_t* title;
        const wchar_t* subtitle;
        bool on;
        int id;
    };
    Item items[3] = {
        { glyph::power, L"Подключать при запуске", L"Включать VPN, когда открываете Vinyl",
          settings.autoConnect, ActAutoConnect },
        { glyph::play, L"Запускать вместе с Windows", L"Vinyl появится в автозагрузке",
          c.autostart, ActAutostart },
        { glyph::globe, L"IPv6", L"Отдавать программам IPv6-адреса", settings.ipv6, ActIpv6 },
    };

    D2D1_RECT_F card = D2D1::RectF(list.left, y, list.left + width, y + p.dp(212.f));
    w::card(c.ui, card);
    for (int i = 0; i < 3; i++) {
        float rowTop = card.top + p.dp(8.f) + i * p.dp(66.f);
        D2D1_RECT_F row = D2D1::RectF(card.left, rowTop, card.right, rowTop + p.dp(66.f));
        if (c.ui.isHot(items[i].id)) p.round(w::inset(row, p.dp(6.f), p.dp(3.f)), p.dp(14.f), theme::hover());

        float cy = (row.top + row.bottom) / 2.f;
        w::iconTile(c.ui, D2D1::RectF(row.left + p.dp(16.f), cy - p.dp(20.f), row.left + p.dp(56.f), cy + p.dp(20.f)),
                    items[i].icon, theme::accentBright);
        p.text(items[i].title, Font::TitleMedium,
               D2D1::RectF(row.left + p.dp(70.f), row.top + p.dp(12.f), row.right - p.dp(80.f), cy + p.dp(1.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(items[i].subtitle, Font::BodyMedium,
               D2D1::RectF(row.left + p.dp(70.f), cy + p.dp(2.f), row.right - p.dp(80.f), row.bottom - p.dp(10.f)),
               theme::textMuted, Align::Left, VAlign::Top);
        w::toggle(c.ui, D2D1::Point2F(row.right - p.dp(16.f), cy), items[i].on, items[i].id);
        c.ui.zone(row, items[i].id);
        if (i < 2) {
            w::divider(c.ui, D2D1::RectF(row.left + p.dp(70.f), row.bottom, row.right - p.dp(16.f), row.bottom));
        }
    }
    return card.bottom + p.dp(18.f);
}

float SettingsView::drawDns(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;

    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"DNS");
    y += p.dp(28.f);

    D2D1_RECT_F card = D2D1::RectF(list.left, y, list.left + width, y + p.dp(114.f));
    w::card(c.ui, card);
    w::segmented(c.ui, D2D1::RectF(card.left + p.dp(16.f), card.top + p.dp(16.f),
                                   card.right - p.dp(16.f), card.top + p.dp(64.f)),
                 { L"Cloudflare", L"Google", L"Quad9" }, (int)c.state.settings.dns, ActDnsBase);
    p.text(L"Запросы шифруются (DNS-over-HTTPS) и уходят через сервер VPN", Font::BodyMedium,
           D2D1::RectF(card.left + p.dp(16.f), card.top + p.dp(72.f), card.right - p.dp(16.f), card.bottom - p.dp(10.f)),
           theme::textMuted, Align::Left, VAlign::Middle);
    return card.bottom + p.dp(18.f);
}

float SettingsView::drawLog(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;

    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Диагностика");
    y += p.dp(28.f);

    D2D1_RECT_F card = D2D1::RectF(list.left, y, list.left + width, y + p.dp(76.f) + p.dp(240.f));
    w::card(c.ui, card);

    float cy = card.top + p.dp(38.f);
    w::iconTile(c.ui, D2D1::RectF(card.left + p.dp(16.f), cy - p.dp(20.f), card.left + p.dp(56.f), cy + p.dp(20.f)),
                glyph::terminal, theme::accentBright);
    p.text(L"Журнал ядра", Font::TitleMedium,
           D2D1::RectF(card.left + p.dp(70.f), card.top + p.dp(14.f), card.right - p.dp(200.f), cy + p.dp(1.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(L"Здесь видно, почему не удалось подключиться", Font::BodyMedium,
           D2D1::RectF(card.left + p.dp(70.f), cy + p.dp(2.f), card.right - p.dp(200.f), card.top + p.dp(66.f)),
           theme::textMuted, Align::Left, VAlign::Top);

    float bx = card.right - p.dp(34.f);
    w::circleButton(c.ui, D2D1::Point2F(bx, cy), p.dp(18.f), glyph::refresh, ActLogRefresh,
                    theme::text, theme::surfaceHigh);
    bx -= p.dp(46.f);
    w::circleButton(c.ui, D2D1::Point2F(bx, cy), p.dp(18.f), glyph::copy, ActLogCopy,
                    theme::text, theme::surfaceHigh);
    bx -= p.dp(46.f);
    w::circleButton(c.ui, D2D1::Point2F(bx, cy), p.dp(18.f), glyph::list, ActLogFolder,
                    theme::text, theme::surfaceHigh);

    D2D1_RECT_F box = D2D1::RectF(card.left + p.dp(16.f), card.top + p.dp(76.f),
                                  card.right - p.dp(16.f), card.bottom - p.dp(16.f));
    p.round(box, p.dp(14.f), theme::background);
    p.roundBorder(box, p.dp(14.f), theme::stroke());
    p.pushClip(box);
    if (c.logLines.empty()) {
        p.text(L"Пока пусто", Font::Mono,
               D2D1::RectF(box.left + p.dp(12.f), box.top + p.dp(10.f), box.right, box.top + p.dp(28.f)),
               theme::textMuted, Align::Left, VAlign::Middle);
    } else {
        float ly = box.top + p.dp(8.f);
        for (const std::wstring& line : c.logLines) {
            if (ly > box.bottom - p.dp(14.f)) break;
            D2D1_COLOR_F color = theme::textSecond;
            if (line.find(L"FATAL") != std::wstring::npos || line.find(L"ERROR") != std::wstring::npos) {
                color = theme::danger;
            } else if (line.find(L"WARN") != std::wstring::npos) {
                color = theme::warning;
            }
            p.text(line, Font::Mono,
                   D2D1::RectF(box.left + p.dp(12.f), ly, box.right - p.dp(10.f), ly + p.dp(16.f)),
                   color, Align::Left, VAlign::Middle);
            ly += p.dp(16.f);
        }
    }
    p.popClip();
    return card.bottom + p.dp(28.f);
}

float SettingsView::drawFooter(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;
    float cx = list.left + width / 2.f;

    p.circleGradient(D2D1::Point2F(cx, y + p.dp(26.f)), p.dp(26.f),
                     w::alpha(theme::accentBright, 0.5f), w::alpha(theme::accentDeep, 0.35f));
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
    p.round(devRect, p.dp(18.f), c.ui.isHot(ActGithub) ? theme::surfaceHigh : theme::surface);
    p.roundBorder(devRect, p.dp(18.f), theme::stroke());
    p.text(dev, Font::LabelMedium, devRect, theme::accentBright, Align::Center, VAlign::Middle);
    c.ui.zone(devRect, ActGithub);

    return devRect.bottom;
}
