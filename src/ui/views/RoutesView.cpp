#include "ui/views/RoutesView.h"

#include "core/Text.h"
#include "ui/Format.h"

using w::alpha;

namespace {

struct SwitchItem {
    const wchar_t* icon;
    const wchar_t* title;
    const wchar_t* subtitle;
    bool on;
    int id;
};

// одинаковая строка "иконка, заголовок, подпись, переключатель"
void switchRow(ViewContext& c, D2D1_RECT_F row, const SwitchItem& item) {
    Painter& p = c.p;
    if (c.ui.isHot(item.id)) {
        p.round(w::inset(row, p.dp(6.f), p.dp(3.f)), p.dp(14.f), theme::hover());
    }
    float cy = (row.top + row.bottom) / 2.f;
    w::iconTile(c.ui, D2D1::RectF(row.left + p.dp(16.f), cy - p.dp(20.f), row.left + p.dp(56.f), cy + p.dp(20.f)),
                item.icon, theme::accentBright);
    p.text(item.title, Font::TitleMedium,
           D2D1::RectF(row.left + p.dp(70.f), row.top + p.dp(12.f), row.right - p.dp(80.f), cy + p.dp(1.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(item.subtitle, Font::BodyMedium,
           D2D1::RectF(row.left + p.dp(70.f), cy + p.dp(2.f), row.right - p.dp(80.f), row.bottom - p.dp(10.f)),
           theme::textMuted, Align::Left, VAlign::Top);
    w::toggle(c.ui, D2D1::Point2F(row.right - p.dp(16.f), cy), item.on, item.id);
    c.ui.zone(row, item.id);
}

} // namespace

float RoutesView::draw(ViewContext& c, D2D1_RECT_F area) {
    Painter& p = c.p;

    w::screenHeader(c.ui, D2D1::RectF(area.left, area.top, area.right, area.top + p.dp(52.f)),
                    L"Маршруты", L"Что идет через VPN, а что напрямую");

    D2D1_RECT_F list = D2D1::RectF(area.left, area.top + p.dp(62.f), area.right, area.bottom);
    p.pushClip(list);
    float dy = -c.scroll;
    p.offset(0, dy);
    c.ui.offsetY = dy;

    float width = list.right - list.left - p.dp(10.f);
    float y = list.top;

    y = drawMode(c, list, y, width);
    y = drawPresets(c, list, y, width);
    y = drawDomains(c, list, y, width);
    y = drawApps(c, list, y, width, dy);

    float contentHeight = y - list.top + p.dp(20.f);
    p.resetOffset();
    c.ui.offsetY = 0.f;
    p.popClip();
    w::scrollbar(c.ui, list, contentHeight, c.scroll);
    return contentHeight;
}

float RoutesView::drawMode(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;
    const Routing& routing = c.state.routing;

    float height = p.dp(152.f);
    bool warn = routing.appMode == AppMode::Only && routing.apps.empty();
    if (warn) height += p.dp(24.f);

    D2D1_RECT_F mode = D2D1::RectF(list.left, y, list.left + width, y + height);
    w::card(c.ui, mode);

    w::iconTile(c.ui, D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(16.f),
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

    w::segmented(c.ui, D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(70.f),
                                   mode.right - p.dp(16.f), mode.top + p.dp(118.f)),
                 { L"Все", L"Только", L"Кроме" }, (int)routing.appMode, ActModeBase);

    if (warn) {
        p.text(L"Отметьте программы ниже. Пока ничего не выбрано, VPN работает для всех",
               Font::BodyMedium,
               D2D1::RectF(mode.left + p.dp(16.f), mode.top + p.dp(124.f), mode.right - p.dp(16.f), mode.bottom - p.dp(6.f)),
               theme::warning, Align::Left, VAlign::Middle);
    }
    return mode.bottom + p.dp(18.f);
}

float RoutesView::drawPresets(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;
    const Routing& routing = c.state.routing;

    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Напрямую, без VPN");
    y += p.dp(28.f);

    D2D1_RECT_F presets = D2D1::RectF(list.left, y, list.left + width, y + p.dp(148.f));
    w::card(c.ui, presets);

    SwitchItem items[2] = {
        { glyph::bank, L"Банки и Госуслуги", L"Сбер, Т-Банк, Альфа, ВТБ, Госуслуги", routing.bypassBanks, ActBypassBanks },
        { glyph::globe, L"Российские сайты", L".ru, .рф, Яндекс, VK, Mail.ru", routing.directRuSites, ActDirectRu },
    };
    for (int i = 0; i < 2; i++) {
        float rowTop = presets.top + p.dp(8.f) + i * p.dp(66.f);
        D2D1_RECT_F row = D2D1::RectF(presets.left, rowTop, presets.right, rowTop + p.dp(66.f));
        switchRow(c, row, items[i]);
        if (i == 0) {
            w::divider(c.ui, D2D1::RectF(row.left + p.dp(70.f), row.bottom, row.right - p.dp(16.f), row.bottom));
        }
    }
    return presets.bottom + p.dp(18.f);
}

float RoutesView::drawDomains(ViewContext& c, D2D1_RECT_F list, float y, float width) {
    Painter& p = c.p;
    const std::vector<std::string>& domains = c.state.routing.directDomains;

    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)), L"Свои сайты напрямую");
    y += p.dp(28.f);

    int rows = (int)((domains.size() + 2) / 3);
    float height = p.dp(92.f) + (domains.empty() ? p.dp(24.f) : rows * p.dp(36.f));
    D2D1_RECT_F box = D2D1::RectF(list.left, y, list.left + width, y + height);
    w::card(c.ui, box);

    D2D1_RECT_F input = D2D1::RectF(box.left + p.dp(16.f), box.top + p.dp(16.f),
                                    box.right - p.dp(72.f), box.top + p.dp(16.f) + p.dp(w::kFieldHeight));
    if (!c.editingDomain && c.editRect != nullptr) *c.editRect = input;
    w::field(c.ui, input, L"kinopoisk.ru", true, c.editingDomain, ActDomainField);
    w::circleButton(c.ui, D2D1::Point2F(box.right - p.dp(40.f), (input.top + input.bottom) / 2.f),
                    p.dp(24.f), glyph::add, ActDomainAdd, D2D1::ColorF(D2D1::ColorF::White), theme::accent);

    if (domains.empty()) {
        p.text(L"Сайт и все его поддомены будут открываться без VPN", Font::BodyMedium,
               D2D1::RectF(box.left + p.dp(16.f), input.bottom + p.dp(8.f), box.right - p.dp(16.f), box.bottom),
               theme::textMuted, Align::Left, VAlign::Top);
        return box.bottom + p.dp(18.f);
    }

    float chipX = box.left + p.dp(16.f);
    float chipY = input.bottom + p.dp(18.f);
    for (size_t i = 0; i < domains.size(); i++) {
        std::wstring name = text::wide(domains[i]);
        float chipW = p.measure(name, Font::BodyMedium, width).width + p.dp(44.f);
        if (chipX + chipW > box.right - p.dp(16.f)) {
            chipX = box.left + p.dp(16.f);
            chipY += p.dp(36.f);
        }
        D2D1_RECT_F chip = D2D1::RectF(chipX, chipY - p.dp(14.f), chipX + chipW, chipY + p.dp(14.f));
        bool hot = c.ui.isHot(ActDomainBase + (int)i);
        p.round(chip, p.dp(14.f), hot ? alpha(theme::danger, 0.14f) : theme::surfaceHigh);
        p.roundBorder(chip, p.dp(14.f), hot ? alpha(theme::danger, 0.4f) : theme::stroke());
        p.text(name, Font::BodyMedium,
               D2D1::RectF(chip.left + p.dp(12.f), chip.top, chip.right - p.dp(26.f), chip.bottom),
               theme::text, Align::Left, VAlign::Middle);
        p.text(glyph::close, Font::Icon,
               D2D1::RectF(chip.right - p.dp(26.f), chip.top, chip.right - p.dp(6.f), chip.bottom),
               hot ? theme::danger : theme::textMuted, Align::Center, VAlign::Middle);
        c.ui.zone(chip, ActDomainBase + (int)i);
        chipX += chipW + p.dp(8.f);
    }
    return box.bottom + p.dp(18.f);
}

float RoutesView::drawApps(ViewContext& c, D2D1_RECT_F list, float y, float width, float dy) {
    Painter& p = c.p;
    const Routing& routing = c.state.routing;
    if (routing.appMode == AppMode::All) return y;

    std::wstring title = routing.appMode == AppMode::Only ? L"Через VPN" : L"Без VPN";
    w::sectionLabel(c.ui, D2D1::RectF(list.left + p.dp(6.f), y, list.right, y + p.dp(22.f)),
                    title + L" · " + std::to_wstring(routing.apps.size()));
    y += p.dp(28.f);

    std::string search = text::lower(text::narrow(c.appSearch));
    for (size_t i = 0; i < c.apps.size(); i++) {
        const processes::Item& app = c.apps[i];
        if (!search.empty() && app.exe.find(search) == std::string::npos) continue;

        D2D1_RECT_F row = D2D1::RectF(list.left, y, list.left + width, y + p.dp(52.f));
        if (row.bottom + dy < list.top || row.top + dy > list.bottom) {
            y = row.bottom + p.dp(2.f);
            continue;
        }

        bool checked = false;
        for (const std::string& selected : routing.apps) {
            if (selected == app.exe) {
                checked = true;
                break;
            }
        }

        int id = ActAppBase + (int)i;
        if (c.ui.isHot(id)) p.round(row, p.dp(14.f), theme::hover());

        float cy = (row.top + row.bottom) / 2.f;
        p.round(D2D1::RectF(row.left + p.dp(12.f), cy - p.dp(16.f), row.left + p.dp(44.f), cy + p.dp(16.f)),
                p.dp(9.f), theme::surfaceHigh);
        p.text(app.title.substr(0, 1), Font::TitleMedium,
               D2D1::RectF(row.left + p.dp(12.f), cy - p.dp(16.f), row.left + p.dp(44.f), cy + p.dp(16.f)),
               theme::textSecond, Align::Center, VAlign::Middle);
        p.text(app.title, Font::TitleMedium,
               D2D1::RectF(row.left + p.dp(56.f), row.top, row.right - p.dp(60.f), cy + p.dp(2.f)),
               theme::text, Align::Left, VAlign::Bottom);
        p.text(text::wide(app.exe), Font::BodyMedium,
               D2D1::RectF(row.left + p.dp(56.f), cy, row.right - p.dp(60.f), row.bottom),
               theme::textMuted, Align::Left, VAlign::Top);
        w::checkCircle(c.ui, D2D1::Point2F(row.right - p.dp(26.f), cy), checked);
        c.ui.zone(row, id);
        y = row.bottom + p.dp(2.f);
    }
    return y;
}
