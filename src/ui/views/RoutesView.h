#pragma once

#include "ui/views/ViewContext.h"

// Маршруты: режим по программам, готовые наборы сайтов, свои домены и список программ.
class RoutesView {
public:
    float draw(ViewContext& c, D2D1_RECT_F area);

private:
    float drawMode(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawPresets(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawDomains(ViewContext& c, D2D1_RECT_F list, float y, float width);
    float drawApps(ViewContext& c, D2D1_RECT_F list, float y, float width, float dy);
};
