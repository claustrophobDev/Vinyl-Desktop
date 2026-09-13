#include "ui/Painter.h"

#include "core/Text.h"

#include <cmath>

namespace {

struct FontSpec {
    const wchar_t* family;
    float size;
    DWRITE_FONT_WEIGHT weight;
};

// размеры один в один с мобильной типографикой
const FontSpec kFonts[(int)Font::Count] = {
    { L"Segoe UI",            44.f, DWRITE_FONT_WEIGHT_LIGHT },        // Display
    { L"Segoe UI",            28.f, DWRITE_FONT_WEIGHT_SEMI_BOLD },    // Headline
    { L"Segoe UI",            20.f, DWRITE_FONT_WEIGHT_SEMI_BOLD },    // TitleLarge
    { L"Segoe UI",            16.f, DWRITE_FONT_WEIGHT_MEDIUM },       // TitleMedium
    { L"Segoe UI",            15.f, DWRITE_FONT_WEIGHT_NORMAL },       // BodyLarge
    { L"Segoe UI",            13.f, DWRITE_FONT_WEIGHT_NORMAL },       // BodyMedium
    { L"Segoe UI",            15.f, DWRITE_FONT_WEIGHT_SEMI_BOLD },    // LabelLarge
    { L"Segoe UI",            12.f, DWRITE_FONT_WEIGHT_MEDIUM },       // LabelMedium
    { L"Segoe UI",            11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD },    // LabelSmall
    { L"Segoe MDL2 Assets",   16.f, DWRITE_FONT_WEIGHT_NORMAL },       // Icon
    { L"Segoe UI Emoji",      20.f, DWRITE_FONT_WEIGHT_NORMAL },       // Emoji
    { L"Consolas",            11.f, DWRITE_FONT_WEIGHT_NORMAL },       // Mono
};

} // namespace

bool Painter::create(HWND hwnd, UINT dpi) {
    hwnd_ = hwnd;
    scale_ = dpi / 96.0f;

    if (!d2d_ && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.GetAddressOf()))) {
        return false;
    }
    if (!dwrite_ && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                               reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf())))) {
        return false;
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U px = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    if (px.width == 0 || px.height == 0) px = D2D1::SizeU(1, 1);

    if (FAILED(d2d_->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, px, D2D1_PRESENT_OPTIONS_NONE),
            rt_.ReleaseAndGetAddressOf()))) {
        return false;
    }

    rt_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush_.ReleaseAndGetAddressOf());
    makeFonts();
    return true;
}

void Painter::makeFonts() {
    for (int i = 0; i < (int)Font::Count; i++) {
        const FontSpec& spec = kFonts[i];
        dwrite_->CreateTextFormat(spec.family, nullptr, spec.weight, DWRITE_FONT_STYLE_NORMAL,
                                  DWRITE_FONT_STRETCH_NORMAL, dp(spec.size), L"ru-ru",
                                  fonts_[i].ReleaseAndGetAddressOf());
        if (!fonts_[i]) continue;
        fonts_[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    // многоточие вместо обрезанного хвоста, как на телефоне
    dwrite_->CreateEllipsisTrimmingSign(fonts_[(int)Font::BodyMedium].Get(), ellipsis_.ReleaseAndGetAddressOf());
}

void Painter::discard() {
    brush_.Reset();
    rt_.Reset();
}

void Painter::resize(UINT width, UINT height) {
    if (rt_) rt_->Resize(D2D1::SizeU(width, height));
}

void Painter::setDpi(UINT dpi) {
    scale_ = dpi / 96.0f;
    if (dwrite_) makeFonts();
}

D2D1_SIZE_F Painter::size() const {
    if (!rt_) return D2D1::SizeF(0, 0);
    return rt_->GetSize();
}

void Painter::begin(const D2D1_COLOR_F& clear) {
    if (!rt_) return;
    rt_->BeginDraw();
    rt_->Clear(clear);
    clipDepth_ = 0;
    dx_ = 0.f;
    dy_ = 0.f;
}

bool Painter::end() {
    if (!rt_) return true;
    while (clipDepth_ > 0) popClip();
    resetOffset();
    HRESULT hr = rt_->EndDraw();
    return hr != D2DERR_RECREATE_TARGET;
}

IDWriteTextFormat* Painter::format(Font font) const {
    int index = (int)font;
    if (index < 0 || index >= (int)Font::Count) return nullptr;
    return fonts_[index].Get();
}

void Painter::rect(D2D1_RECT_F r, const D2D1_COLOR_F& color) {
    if (!rt_) return;
    brush_->SetColor(color);
    rt_->FillRectangle(r, brush_.Get());
}

void Painter::round(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& color) {
    if (!rt_) return;
    brush_->SetColor(color);
    rt_->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush_.Get());
}

void Painter::roundBorder(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& color, float width) {
    if (!rt_) return;
    brush_->SetColor(color);
    // рисуем по середине линии, иначе рамка съезжает на полпикселя и мылит
    D2D1_RECT_F inner = D2D1::RectF(r.left + width / 2, r.top + width / 2,
                                    r.right - width / 2, r.bottom - width / 2);
    rt_->DrawRoundedRectangle(D2D1::RoundedRect(inner, radius, radius), brush_.Get(), width);
}

void Painter::roundGradient(D2D1_RECT_F r, float radius, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to) {
    if (!rt_) return;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.f;
    stops[0].color = from;
    stops[1].position = 1.f;
    stops[1].color = to;

    ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(rt_->CreateGradientStopCollection(stops, 2, collection.GetAddressOf()))) return;

    ComPtr<ID2D1LinearGradientBrush> brush;
    rt_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.right, r.top)),
        collection.Get(), brush.GetAddressOf());
    if (!brush) return;
    rt_->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush.Get());
}

void Painter::circle(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color) {
    if (!rt_) return;
    brush_->SetColor(color);
    rt_->FillEllipse(D2D1::Ellipse(center, radius, radius), brush_.Get());
}

void Painter::circleGradient(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& from, const D2D1_COLOR_F& to) {
    if (!rt_) return;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.f;
    stops[0].color = from;
    stops[1].position = 1.f;
    stops[1].color = to;

    ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(rt_->CreateGradientStopCollection(stops, 2, collection.GetAddressOf()))) return;

    ComPtr<ID2D1LinearGradientBrush> brush;
    rt_->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(D2D1::Point2F(center.x - radius, center.y - radius),
                                            D2D1::Point2F(center.x + radius, center.y + radius)),
        collection.Get(), brush.GetAddressOf());
    if (!brush) return;
    rt_->FillEllipse(D2D1::Ellipse(center, radius, radius), brush.Get());
}

void Painter::ring(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color, float width) {
    if (!rt_) return;
    brush_->SetColor(color);
    rt_->DrawEllipse(D2D1::Ellipse(center, radius, radius), brush_.Get(), width);
}

void Painter::arc(D2D1_POINT_2F center, float radius, float startDegrees, float sweepDegrees,
                  const D2D1_COLOR_F& color, float width) {
    if (!rt_ || !d2d_) return;

    const float pi = 3.14159265f;
    float a0 = startDegrees * pi / 180.f;
    float a1 = (startDegrees + sweepDegrees) * pi / 180.f;
    D2D1_POINT_2F from = D2D1::Point2F(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
    D2D1_POINT_2F to = D2D1::Point2F(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);

    ComPtr<ID2D1PathGeometry> path;
    if (FAILED(d2d_->CreatePathGeometry(path.GetAddressOf()))) return;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(path->Open(sink.GetAddressOf()))) return;

    sink->BeginFigure(from, D2D1_FIGURE_BEGIN_HOLLOW);
    D2D1_ARC_SEGMENT segment = D2D1::ArcSegment(
        to, D2D1::SizeF(radius, radius), 0.f,
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        fabsf(sweepDegrees) > 180.f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL);
    sink->AddArc(segment);
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();

    ComPtr<ID2D1StrokeStyle> style;
    d2d_->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND),
        nullptr, 0, style.GetAddressOf());

    brush_->SetColor(color);
    rt_->DrawGeometry(path.Get(), brush_.Get(), width, style.Get());
}

void Painter::line(D2D1_POINT_2F a, D2D1_POINT_2F b, const D2D1_COLOR_F& color, float width) {
    if (!rt_) return;
    brush_->SetColor(color);
    rt_->DrawLine(a, b, brush_.Get(), width);
}

void Painter::glow(D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color, float strength) {
    if (!rt_ || strength <= 0.01f || radius <= 0.f) return;

    D2D1_GRADIENT_STOP stops[3];
    stops[0].position = 0.55f;
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, 0.40f * strength);
    stops[1].position = 0.78f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, 0.12f * strength);
    stops[2].position = 1.0f;
    stops[2].color = D2D1::ColorF(color.r, color.g, color.b, 0.f);

    ComPtr<ID2D1GradientStopCollection> collection;
    if (FAILED(rt_->CreateGradientStopCollection(stops, 3, collection.GetAddressOf()))) return;

    ComPtr<ID2D1RadialGradientBrush> brush;
    rt_->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(center, D2D1::Point2F(0, 0), radius, radius),
        collection.Get(), brush.GetAddressOf());
    if (!brush) return;
    rt_->FillEllipse(D2D1::Ellipse(center, radius, radius), brush.Get());
}

void Painter::text(const std::wstring& s, Font font, D2D1_RECT_F r, const D2D1_COLOR_F& color,
                   Align align, VAlign valign) {
    if (!rt_ || s.empty()) return;
    IDWriteTextFormat* f = format(font);
    if (!f) return;

    f->SetTextAlignment(align == Align::Left ? DWRITE_TEXT_ALIGNMENT_LEADING
                        : align == Align::Center ? DWRITE_TEXT_ALIGNMENT_CENTER
                        : DWRITE_TEXT_ALIGNMENT_TRAILING);
    f->SetParagraphAlignment(valign == VAlign::Top ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR
                             : valign == VAlign::Middle ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER
                             : DWRITE_PARAGRAPH_ALIGNMENT_FAR);

    DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    f->SetTrimming(&trimming, ellipsis_.Get());

    brush_->SetColor(color);
    rt_->DrawTextW(s.c_str(), (UINT32)s.size(), f, r, brush_.Get(),
                   D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
}

void Painter::text(const std::string& utf8, Font font, D2D1_RECT_F r, const D2D1_COLOR_F& color,
                   Align align, VAlign valign) {
    text(text::wide(utf8), font, r, color, align, valign);
}

void Painter::emoji(const std::string& utf8, D2D1_POINT_2F center, float size) {
    if (!rt_ || !dwrite_ || utf8.empty()) return;
    std::wstring s = text::wide(utf8);

    ComPtr<IDWriteTextFormat> f;
    if (FAILED(dwrite_->CreateTextFormat(L"Segoe UI Emoji", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                         DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                         size, L"ru-ru", f.GetAddressOf()))) {
        return;
    }
    f->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_RECT_F box = D2D1::RectF(center.x - size, center.y - size, center.x + size, center.y + size);
    brush_->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    // без этого флага вместо флагов стран рисуются черно-белые квадратики
    rt_->DrawTextW(s.c_str(), (UINT32)s.size(), f.Get(), box, brush_.Get(),
                   D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, DWRITE_MEASURING_MODE_NATURAL);
}

D2D1_SIZE_F Painter::measure(const std::wstring& s, Font font, float maxWidth) {
    if (!dwrite_ || s.empty()) return D2D1::SizeF(0, 0);
    IDWriteTextFormat* f = format(font);
    if (!f) return D2D1::SizeF(0, 0);

    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(dwrite_->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, maxWidth, 10000.f,
                                         layout.GetAddressOf()))) {
        return D2D1::SizeF(0, 0);
    }
    DWRITE_TEXT_METRICS metrics = {};
    layout->GetMetrics(&metrics);
    return D2D1::SizeF(metrics.widthIncludingTrailingWhitespace, metrics.height);
}

void Painter::pushClip(D2D1_RECT_F r) {
    if (!rt_) return;
    rt_->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_ALIASED);
    clipDepth_++;
}

void Painter::popClip() {
    if (!rt_ || clipDepth_ <= 0) return;
    rt_->PopAxisAlignedClip();
    clipDepth_--;
}

void Painter::offset(float dx, float dy) {
    if (!rt_) return;
    dx_ = dx;
    dy_ = dy;
    rt_->SetTransform(D2D1::Matrix3x2F::Translation(dx, dy));
}

void Painter::resetOffset() {
    if (!rt_) return;
    dx_ = 0.f;
    dy_ = 0.f;
    rt_->SetTransform(D2D1::Matrix3x2F::Identity());
}
