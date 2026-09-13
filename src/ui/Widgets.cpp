#include "ui/Widgets.h"

#include "core/Flags.h"
#include "net/Ping.h"
#include "ui/FlagPainter.h"
#include "ui/Theme.h"

void Ui::zone(D2D1_RECT_F r, int id) {
    if (id < 0) return;
    D2D1_RECT_F shifted = D2D1::RectF(r.left, r.top + offsetY, r.right, r.bottom + offsetY);
    zones.push_back({ shifted, id });
}

int Ui::hitTest(float x, float y) const {
    for (size_t i = zones.size(); i > 0; i--) {
        const Zone& z = zones[i - 1];
        if (x >= z.rect.left && x <= z.rect.right && y >= z.rect.top && y <= z.rect.bottom) {
            return z.id;
        }
    }
    return -1;
}

namespace w {

D2D1_COLOR_F alpha(const D2D1_COLOR_F& color, float a) {
    return D2D1::ColorF(color.r, color.g, color.b, a);
}

D2D1_RECT_F inset(D2D1_RECT_F r, float dx, float dy) {
    return D2D1::RectF(r.left + dx, r.top + dy, r.right - dx, r.bottom - dy);
}

void card(Ui& ui, D2D1_RECT_F r) {
    Painter& p = *ui.p;
    float radius = p.dp(kCardRadius);
    p.round(r, radius, theme::surface);
    p.roundBorder(r, radius, theme::stroke());
}

void cardButton(Ui& ui, D2D1_RECT_F r, int id, bool selected) {
    Painter& p = *ui.p;
    float radius = p.dp(kRowRadius);

    D2D1_COLOR_F fill = selected ? theme::accentSoft() : theme::surface;
    if (ui.isHot(id) && !selected) fill = theme::surfaceHigh;
    D2D1_COLOR_F border = selected ? alpha(theme::accent, 0.45f) : theme::stroke();

    p.round(r, radius, fill);
    p.roundBorder(r, radius, border);
    ui.zone(r, id);
}

void screenHeader(Ui& ui, D2D1_RECT_F r, const std::wstring& title, const std::wstring& subtitle) {
    Painter& p = *ui.p;
    if (subtitle.empty()) {
        p.text(title, Font::Headline, r, theme::text, Align::Left, VAlign::Middle);
        return;
    }
    float half = (r.bottom - r.top) / 2.f;
    p.text(title, Font::Headline,
           D2D1::RectF(r.left, r.top, r.right, r.top + half + p.dp(4.f)),
           theme::text, Align::Left, VAlign::Bottom);
    p.text(subtitle, Font::BodyMedium,
           D2D1::RectF(r.left, r.top + half + p.dp(5.f), r.right, r.bottom),
           theme::textMuted, Align::Left, VAlign::Top);
}

void sectionLabel(Ui& ui, D2D1_RECT_F r, const std::wstring& text) {
    Painter& p = *ui.p;
    // заглавными и вразрядку, как на телефоне
    std::wstring spaced;
    for (size_t i = 0; i < text.size(); i++) {
        spaced += text[i];
        if (i + 1 < text.size()) spaced += L' ';
    }
    std::wstring upper = spaced;
    CharUpperBuffW(upper.data(), (DWORD)upper.size());
    p.text(upper, Font::LabelSmall, r, theme::textMuted, Align::Left, VAlign::Middle);
}

bool circleButton(Ui& ui, D2D1_POINT_2F center, float radius, const wchar_t* icon, int id,
                  const D2D1_COLOR_F& tint, const D2D1_COLOR_F& background) {
    Painter& p = *ui.p;
    bool hot = ui.isHot(id);

    p.circle(center, radius, hot ? theme::surfaceHigh : background);
    p.ring(center, radius - 0.5f, hot ? theme::strokeStrong() : theme::stroke());
    p.text(icon, Font::Icon,
           D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius),
           tint, Align::Center, VAlign::Middle);

    ui.zone(D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius), id);
    return hot;
}

namespace {

void buttonContent(Painter& p, D2D1_RECT_F r, const std::wstring& text, const wchar_t* icon,
                   const D2D1_COLOR_F& color) {
    if (icon == nullptr) {
        p.text(text, Font::LabelLarge, r, color, Align::Center, VAlign::Middle);
        return;
    }
    // иконка и текст вместе по центру
    float textWidth = p.measure(text, Font::LabelLarge, r.right - r.left).width;
    float iconWidth = p.dp(20.f);
    float gap = p.dp(10.f);
    float total = textWidth + iconWidth + gap;
    float left = (r.left + r.right) / 2.f - total / 2.f;

    p.text(icon, Font::Icon, D2D1::RectF(left, r.top, left + iconWidth, r.bottom), color, Align::Center, VAlign::Middle);
    p.text(text, Font::LabelLarge,
           D2D1::RectF(left + iconWidth + gap, r.top, r.right, r.bottom), color, Align::Left, VAlign::Middle);
}

} // namespace

void primaryButton(Ui& ui, D2D1_RECT_F r, const std::wstring& text, const wchar_t* icon, int id, bool enabled) {
    Painter& p = *ui.p;
    float radius = p.dp(kButtonRadius);

    if (!enabled) {
        p.round(r, radius, theme::surfaceHigh);
        buttonContent(p, r, text, icon, theme::textMuted);
        return;
    }

    bool hot = ui.isHot(id);
    D2D1_COLOR_F from = hot ? theme::accent : theme::accentDeep;
    D2D1_COLOR_F to = hot ? theme::accentBright : theme::accent;
    if (ui.isPressed(id)) {
        from = theme::accentDeep;
        to = theme::accentDeep;
    }
    p.roundGradient(r, radius, from, to);
    buttonContent(p, r, text, icon, D2D1::ColorF(D2D1::ColorF::White));
    ui.zone(r, id);
}

void secondaryButton(Ui& ui, D2D1_RECT_F r, const std::wstring& text, const wchar_t* icon, int id) {
    Painter& p = *ui.p;
    float radius = p.dp(kButtonRadius);
    bool hot = ui.isHot(id);

    p.round(r, radius, hot ? alpha(theme::accent, 0.16f) : theme::surfaceHigh);
    p.roundBorder(r, radius, hot ? alpha(theme::accent, 0.35f) : theme::stroke());
    buttonContent(p, r, text, icon, theme::text);
    ui.zone(r, id);
}

void iconTile(Ui& ui, D2D1_RECT_F r, const wchar_t* icon, const D2D1_COLOR_F& tint) {
    Painter& p = *ui.p;
    p.round(r, p.dp(12.f), alpha(tint, 0.12f));
    p.text(icon, Font::Icon, r, tint, Align::Center, VAlign::Middle);
}

void flagAvatar(Ui& ui, D2D1_POINT_2F center, float radius, const std::string& flag, bool dimmed) {
    Painter& p = *ui.p;
    // пропорции примерно как у настоящего флага, 3 к 2
    float half = radius * 1.42f;
    D2D1_RECT_F box = D2D1::RectF(center.x - half, center.y - radius * 0.95f,
                                  center.x + half, center.y + radius * 0.95f);

    if (dimmed) {
        p.round(box, p.dp(4.f), theme::surfaceHigh);
        p.text(L"\xE774", Font::Icon, box, theme::textMuted, Align::Center, VAlign::Middle);
        return;
    }

    std::string code = Flags::codeOf(flag);
    if (!code.empty() && flags::draw(p, box, code)) {
        // рамка нужна, иначе белые флаги сливаются с карточкой
        p.roundBorder(box, 0.f, theme::strokeStrong());
        return;
    }

    // страну не угадали: код буквами, а если и его нет - глобус
    p.round(box, p.dp(4.f), theme::surfaceHigh);
    std::wstring letters(code.begin(), code.end());
    if (letters.empty()) {
        p.text(glyph::globe, Font::Icon, box, theme::textMuted, Align::Center, VAlign::Middle);
    } else {
        p.text(letters, Font::LabelMedium, box, theme::textSecond, Align::Center, VAlign::Middle);
    }
}

namespace {

void pingLook(int ms, std::wstring& text, D2D1_COLOR_F& color) {
    if (ms == 0) {
        text = L"—";
        color = theme::textMuted;
    } else if (ms == ping::kUdp) {
        text = L"UDP";
        color = theme::textMuted;
    } else if (ms < 0) {
        text = L"нет ответа";
        color = theme::danger;
    } else if (ms < 150) {
        text = std::to_wstring(ms) + L" мс";
        color = theme::success;
    } else if (ms < 400) {
        text = std::to_wstring(ms) + L" мс";
        color = theme::warning;
    } else {
        text = std::to_wstring(ms) + L" мс";
        color = theme::danger;
    }
}

} // namespace

float pingWidth(Painter& p, int ms) {
    std::wstring text;
    D2D1_COLOR_F color;
    pingLook(ms, text, color);
    return p.measure(text, Font::LabelMedium, 400.f).width + p.dp(16.f);
}

void pingBadge(Ui& ui, D2D1_POINT_2F rightCenter, int ms) {
    Painter& p = *ui.p;
    std::wstring text;
    D2D1_COLOR_F color;
    pingLook(ms, text, color);

    float width = p.measure(text, Font::LabelMedium, 400.f).width + p.dp(16.f);
    float height = p.dp(24.f);
    D2D1_RECT_F r = D2D1::RectF(rightCenter.x - width, rightCenter.y - height / 2,
                                rightCenter.x, rightCenter.y + height / 2);
    p.round(r, p.dp(8.f), alpha(color, 0.10f));
    p.text(text, Font::LabelMedium, r, color, Align::Center, VAlign::Middle);
}

float chipWidth(Painter& p, const std::wstring& text) {
    return p.measure(text, Font::LabelMedium, 400.f).width + p.dp(12.f);
}

void chip(Ui& ui, D2D1_POINT_2F leftCenter, const std::wstring& text, const D2D1_COLOR_F& color) {
    Painter& p = *ui.p;
    float width = chipWidth(p, text);
    float height = p.dp(20.f);
    D2D1_RECT_F r = D2D1::RectF(leftCenter.x, leftCenter.y - height / 2,
                                leftCenter.x + width, leftCenter.y + height / 2);
    p.round(r, p.dp(6.f), alpha(color, 0.12f));
    p.text(text, Font::LabelMedium, r, color, Align::Center, VAlign::Middle);
}

void toggle(Ui& ui, D2D1_POINT_2F rightCenter, bool on, int id) {
    Painter& p = *ui.p;
    float width = p.dp(46.f);
    float height = p.dp(26.f);
    D2D1_RECT_F r = D2D1::RectF(rightCenter.x - width, rightCenter.y - height / 2,
                                rightCenter.x, rightCenter.y + height / 2);

    bool hot = ui.isHot(id);
    p.round(r, height / 2, on ? theme::accent : theme::surfaceHigh);
    p.roundBorder(r, height / 2, on ? theme::accent : (hot ? theme::strokeStrong() : theme::stroke()));

    float knob = height / 2 - p.dp(4.f);
    float x = on ? (r.right - p.dp(4.f) - knob) : (r.left + p.dp(4.f) + knob);
    p.circle(D2D1::Point2F(x, rightCenter.y), knob,
             on ? D2D1::ColorF(D2D1::ColorF::White) : theme::textSecond);

    ui.zone(r, id);
}

void checkCircle(Ui& ui, D2D1_POINT_2F center, bool checked) {
    Painter& p = *ui.p;
    float radius = p.dp(12.f);
    if (checked) {
        p.circle(center, radius, theme::accent);
        p.text(glyph::check, Font::Icon,
               D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius),
               D2D1::ColorF(D2D1::ColorF::White), Align::Center, VAlign::Middle);
    } else {
        p.ring(center, radius - 0.75f, theme::strokeStrong(), 1.5f);
    }
}

void divider(Ui& ui, D2D1_RECT_F r) {
    Painter& p = *ui.p;
    p.rect(D2D1::RectF(r.left, r.top, r.right, r.top + 1.f), theme::stroke());
}

void segmented(Ui& ui, D2D1_RECT_F r, const std::vector<std::wstring>& options, int selected, int baseId) {
    Painter& p = *ui.p;
    if (options.empty()) return;

    float radius = p.dp(16.f);
    p.round(r, radius, theme::background);
    p.roundBorder(r, radius, theme::stroke());

    D2D1_RECT_F inner = inset(r, p.dp(4.f), p.dp(4.f));
    float itemWidth = (inner.right - inner.left) / options.size();

    // подложка под выбранным
    if (selected >= 0 && selected < (int)options.size()) {
        D2D1_RECT_F slider = D2D1::RectF(inner.left + itemWidth * selected, inner.top,
                                         inner.left + itemWidth * (selected + 1), inner.bottom);
        p.round(slider, p.dp(12.f), theme::accentSoft());
        p.roundBorder(slider, p.dp(12.f), alpha(theme::accent, 0.35f));
    }

    for (size_t i = 0; i < options.size(); i++) {
        D2D1_RECT_F item = D2D1::RectF(inner.left + itemWidth * i, inner.top,
                                       inner.left + itemWidth * (i + 1), inner.bottom);
        int id = baseId + (int)i;
        bool active = (int)i == selected;
        D2D1_COLOR_F color = active ? theme::text : (ui.isHot(id) ? theme::textSecond : theme::textMuted);
        p.text(options[i], Font::LabelLarge, item, color, Align::Center, VAlign::Middle);
        ui.zone(item, id);
    }
}

void field(Ui& ui, D2D1_RECT_F r, const std::wstring& placeholder, bool empty, bool focused, int id) {
    Painter& p = *ui.p;
    float radius = p.dp(14.f);
    p.round(r, radius, theme::background);
    p.roundBorder(r, radius, focused ? alpha(theme::accent, 0.55f)
                                     : (ui.isHot(id) ? theme::strokeStrong() : theme::stroke()));
    if (empty && !placeholder.empty()) {
        p.text(placeholder, Font::BodyLarge,
               D2D1::RectF(r.left + p.dp(14.f), r.top, r.right - p.dp(10.f), r.bottom),
               theme::textMuted, Align::Left, VAlign::Middle);
    }
    ui.zone(r, id);
}

void scrollbar(Ui& ui, D2D1_RECT_F area, float contentHeight, float scroll) {
    Painter& p = *ui.p;
    float viewHeight = area.bottom - area.top;
    if (contentHeight <= viewHeight + 1.f) return;

    float trackWidth = p.dp(4.f);
    float x = area.right - trackWidth - p.dp(2.f);
    float thumbHeight = viewHeight * (viewHeight / contentHeight);
    if (thumbHeight < p.dp(40.f)) thumbHeight = p.dp(40.f);

    float maxScroll = contentHeight - viewHeight;
    float part = maxScroll > 0 ? (scroll / maxScroll) : 0.f;
    if (part < 0.f) part = 0.f;
    if (part > 1.f) part = 1.f;
    float y = area.top + (viewHeight - thumbHeight) * part;

    p.round(D2D1::RectF(x, y, x + trackWidth, y + thumbHeight), trackWidth / 2, theme::strokeStrong());
}

} // namespace w
