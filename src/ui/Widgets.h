#pragma once

#include <string>
#include <vector>

#include "ui/Painter.h"

// рисуем сразу, без дерева виджетов: каждый кадр заново, а по пути запоминаем
// куда можно кликнуть. так проще всего, и состояние живет в одном месте
struct Zone {
    D2D1_RECT_F rect;
    int id;
};

struct Ui {
    Painter* p = nullptr;
    std::vector<Zone> zones;
    int hot = -1;        // под мышкой
    int pressed = -1;    // зажата кнопка мыши
    float offsetY = 0.f; // сдвиг прокрутки, чтобы зоны считались по экрану

    void reset() { zones.clear(); }
    void zone(D2D1_RECT_F r, int id);
    bool isHot(int id) const { return hot == id && id >= 0; }
    bool isPressed(int id) const { return pressed == id && id >= 0; }
    // последняя подходящая зона выигрывает: то что нарисовано позже, лежит сверху
    int hitTest(float x, float y) const;
};

// иконки из системного шрифта Segoe MDL2 Assets
namespace glyph {
inline const wchar_t* add        = L"\xE710";
inline const wchar_t* close      = L"\xE711";
inline const wchar_t* settings   = L"\xE713";
inline const wchar_t* more       = L"\xE712";
inline const wchar_t* search     = L"\xE721";
inline const wchar_t* refresh    = L"\xE72C";
inline const wchar_t* check      = L"\xE73E";
inline const wchar_t* trash      = L"\xE74D";
inline const wchar_t* globe      = L"\xE774";
inline const wchar_t* power      = L"\xE7E8";
inline const wchar_t* chevron    = L"\xE76C";
inline const wchar_t* down       = L"\xE70D";
inline const wchar_t* up         = L"\xE70E";
inline const wchar_t* copy       = L"\xE8C8";
inline const wchar_t* routes     = L"\xE8AB";
inline const wchar_t* list       = L"\xE968";
inline const wchar_t* play       = L"\xE768";
inline const wchar_t* bolt       = L"\xE945";
inline const wchar_t* download   = L"\xE896";
inline const wchar_t* upload     = L"\xE898";
inline const wchar_t* shield     = L"\xE83D";
inline const wchar_t* bank       = L"\xE825";
inline const wchar_t* apps       = L"\xECAA";
inline const wchar_t* terminal   = L"\xE756";
inline const wchar_t* paste      = L"\xE77F";
inline const wchar_t* minimize   = L"\xE921";
inline const wchar_t* stopwatch  = L"\xE916";
} // namespace glyph

namespace w {

// размеры как в мобильной верстке
const float kCardRadius = 24.f;
const float kRowRadius = 20.f;
const float kButtonRadius = 18.f;
const float kButtonHeight = 54.f;
const float kFieldHeight = 48.f;
const float kAvatar = 44.f;

D2D1_COLOR_F alpha(const D2D1_COLOR_F& color, float a);
D2D1_RECT_F inset(D2D1_RECT_F r, float dx, float dy);

void card(Ui& ui, D2D1_RECT_F r);
// карточка на которую можно нажать, подсвечивается под мышкой
void cardButton(Ui& ui, D2D1_RECT_F r, int id, bool selected = false);

void screenHeader(Ui& ui, D2D1_RECT_F r, const std::wstring& title, const std::wstring& subtitle);
void sectionLabel(Ui& ui, D2D1_RECT_F r, const std::wstring& text);

bool circleButton(Ui& ui, D2D1_POINT_2F center, float radius, const wchar_t* icon, int id,
                  const D2D1_COLOR_F& tint, const D2D1_COLOR_F& background);
void primaryButton(Ui& ui, D2D1_RECT_F r, const std::wstring& text, const wchar_t* icon, int id, bool enabled = true);
void secondaryButton(Ui& ui, D2D1_RECT_F r, const std::wstring& text, const wchar_t* icon, int id);

void iconTile(Ui& ui, D2D1_RECT_F r, const wchar_t* icon, const D2D1_COLOR_F& tint);
void flagAvatar(Ui& ui, D2D1_POINT_2F center, float radius, const std::string& flag, bool dimmed);

float pingWidth(Painter& p, int ms);
void pingBadge(Ui& ui, D2D1_POINT_2F rightCenter, int ms);
float chipWidth(Painter& p, const std::wstring& text);
void chip(Ui& ui, D2D1_POINT_2F leftCenter, const std::wstring& text, const D2D1_COLOR_F& color);

void toggle(Ui& ui, D2D1_POINT_2F rightCenter, bool on, int id);
void checkCircle(Ui& ui, D2D1_POINT_2F center, bool checked);
void divider(Ui& ui, D2D1_RECT_F r);

// возвращает индекс на который нажали, или -1
void segmented(Ui& ui, D2D1_RECT_F r, const std::vector<std::wstring>& options, int selected, int baseId);

// внешний вид поля ввода, сам текст рисует системный контрол поверх
void field(Ui& ui, D2D1_RECT_F r, const std::wstring& placeholder, bool empty, bool focused, int id);

// полоска прокрутки справа
void scrollbar(Ui& ui, D2D1_RECT_F area, float contentHeight, float scroll);

} // namespace w
