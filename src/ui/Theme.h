#pragma once
#include <d2d1.h>

// цвета один в один с мобильной версией
namespace theme {

inline D2D1_COLOR_F rgb(UINT32 hex, float a = 1.0f) {
    return D2D1::ColorF(hex, a);
}

const D2D1_COLOR_F background   = D2D1::ColorF(0x07060B);
const D2D1_COLOR_F surface      = D2D1::ColorF(0x110F18);
const D2D1_COLOR_F surfaceHigh  = D2D1::ColorF(0x1A1724);

const D2D1_COLOR_F accent       = D2D1::ColorF(0x8B6CFF);
const D2D1_COLOR_F accentBright = D2D1::ColorF(0xB9A6FF);
const D2D1_COLOR_F accentDeep   = D2D1::ColorF(0x5B3DF5);

const D2D1_COLOR_F text         = D2D1::ColorF(0xF4F2FA);
const D2D1_COLOR_F textSecond   = D2D1::ColorF(0xA6A2B8);
const D2D1_COLOR_F textMuted    = D2D1::ColorF(0x6D6982);

const D2D1_COLOR_F success      = D2D1::ColorF(0x3DDC97);
const D2D1_COLOR_F warning      = D2D1::ColorF(0xFFB547);
const D2D1_COLOR_F danger       = D2D1::ColorF(0xFF5C7A);

// полупрозрачные, как Stroke/AccentSoft на телефоне
inline D2D1_COLOR_F stroke()       { return D2D1::ColorF(0xFFFFFF, 0.08f); }
inline D2D1_COLOR_F strokeStrong() { return D2D1::ColorF(0xFFFFFF, 0.14f); }
inline D2D1_COLOR_F accentSoft()   { return D2D1::ColorF(0x8B6CFF, 0.14f); }
inline D2D1_COLOR_F hover()        { return D2D1::ColorF(0xFFFFFF, 0.05f); }

} // namespace theme
