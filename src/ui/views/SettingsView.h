#pragma once

#include "ui/views/ViewContext.h"

// Настройки: подключение, DNS, журнал ядра и подвал.
class SettingsView {
public:
    float draw(ViewContext& c, D2D1_RECT_F area);

private:
    float drawConnection(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawDns(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawLog(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawFooter(ViewContext& c, D2D1_RECT_F list, float y, float width);
};
