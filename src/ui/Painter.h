#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

// шрифты те же по размерам что и в мобильной типографике
enum class Font {
    Display,      // большой таймер
    Headline,     // заголовок экрана
    TitleLarge,
    TitleMedium,
    BodyLarge,
    BodyMedium,
    LabelLarge,
    LabelMedium,
    LabelSmall,   // заглавными, подписи секций
    Icon,         // Segoe MDL2 Assets
    Emoji,        // флаги
    Mono,         // журнал
    Count
};

enum class Align { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

// обертка над Direct2D: держит цель отрисовки, кисти и шрифты
class Painter {
public:
    bool create(HWND hwnd, UINT dpi);
    void discard();
    void resize(UINT width, UINT height);
    void setDpi(UINT dpi);
    bool ready() const { return rt_ != nullptr; }

    void begin(const D2D1_COLOR_F& clear);
    // false значит цель потерялась и надо пересоздать
    bool end();

    D2D1_SIZE_F size() const;
    float dp(float value) const { return value * scale_; }

    // фигуры
    void rect(D2D1_RECT_F r, const D2D1_COLOR_F& color);
    void round(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& color);
    void roundBorder(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& color, float width = 1.0f);
    void roundGradient(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to);
    void circle(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color);
    void circleGradient(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to);
    void ring(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color, float width = 1.0f);
    void arc(D2D1_POINT_2F center, float radius, float startDegrees, float sweepDegrees,
             const D2D1_COLOR_F& color, float width);
    void line(D2D1_POINT_2F a, D2D1_POINT_2F b, const D2D1_COLOR_F& color, float width = 1.0f);
    void glow(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color, float strength);

    // текст
    void text(const std::wstring& s, Font font, D2D1_RECT_F r, const D2D1_COLOR_F& color,
              Align align = Align::Left, VAlign valign = VAlign::Top);
    void text(const std::string& utf8, Font font, D2D1_RECT_F r, const D2D1_COLOR_F& color,
              Align align = Align::Left, VAlign valign = VAlign::Top);
    // цветные эмодзи, размер подбираем под высоту
    void emoji(const std::string& utf8, D2D1_POINT_2F center, float size);
    D2D1_SIZE_F measure(const std::wstring& s, Font font, float maxWidth);

    // прокрутка и обрезка
    void pushClip(D2D1_RECT_F r);
    void popClip();
    void offset(float dx, float dy);
    void resetOffset();

private:
    IDWriteTextFormat* format(Font font) const;
    void makeFonts();

    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> dwrite_;
    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<IDWriteTextFormat> fonts_[(int)Font::Count];
    ComPtr<IDWriteInlineObject> ellipsis_;

    HWND hwnd_ = nullptr;
    float scale_ = 1.0f;
    int clipDepth_ = 0;
    float dx_ = 0.f;
    float dy_ = 0.f;
};
