#pragma once

#include "ui/views/ViewContext.h"

// Список серверов: автовыбор, группы по подпискам и добавленные руками.
class ServersView {
public:
    float draw(ViewContext& c, D2D1_RECT_F area);

private:
    void drawRow(ViewContext& c, D2D1_RECT_F r, const Server& server, int index, bool selected, int ping);
    void drawEmpty(ViewContext& c, D2D1_RECT_F area);
    float drawGroupHeader(ViewContext& c, D2D1_RECT_F area, float y, const std::wstring& title,
                          const std::wstring& info, int subIndex);
};
