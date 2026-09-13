#pragma once

#include "ui/views/ViewContext.h"

// Главный экран: пластинка, счетчики трафика и карточка выбранного сервера.
class HomeView {
public:
    // возвращает высоту содержимого, тут прокрутки нет и она всегда 0
    float draw(ViewContext& c, D2D1_RECT_F area);

private:
    void drawDisc(ViewContext& c, D2D1_POINT_2F center, float radius);
    void drawTile(ViewContext& c, D2D1_RECT_F r, const wchar_t* icon, const std::wstring& label,
                  const std::wstring& value, const std::wstring& total, const D2D1_COLOR_F& tint);
    void drawServerCard(ViewContext& c, D2D1_RECT_F r);
};
