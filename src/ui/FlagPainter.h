#pragma once

#include <string>

#include "ui/Painter.h"

// винда вместо эмодзи-флага рисует две буквы кода, так что рисуем сами
namespace flags {

// false если страну не знаем, тогда рисовать нечего
bool draw(Painter& p, D2D1_RECT_F r, const std::string& code);

} // namespace flags
