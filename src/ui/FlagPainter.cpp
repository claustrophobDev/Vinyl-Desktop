#include "ui/FlagPainter.h"

namespace flags {
namespace {

enum Kind {
    H3,      // три горизонтальные полосы
    V3,      // три вертикальные
    H2,      // две горизонтальные
    NORDIC,  // скандинавский крест
    SPECIAL  // рисуется отдельно в switch
};

struct Flag {
    const char* code;
    Kind kind;
    UINT32 a, b, c;
};

// цвета примерные, на 30 пикселях разница все равно не видна
const Flag kFlags[] = {
    { "RU", H3, 0xFFFFFF, 0x0039A6, 0xD52B1E },
    { "DE", H3, 0x000000, 0xDD0000, 0xFFCE00 },
    { "NL", H3, 0xAE1C28, 0xFFFFFF, 0x21468B },
    { "FR", V3, 0x002395, 0xFFFFFF, 0xED2939 },
    { "IT", V3, 0x009246, 0xFFFFFF, 0xCE2B37 },
    { "IE", V3, 0x169B62, 0xFFFFFF, 0xFF883E },
    { "BE", V3, 0x000000, 0xFDDA24, 0xEF3340 },
    { "RO", V3, 0x002B7F, 0xFCD116, 0xCE1126 },
    { "TD", V3, 0x002664, 0xFECB00, 0xC60C30 },
    { "AT", H3, 0xED2939, 0xFFFFFF, 0xED2939 },
    { "LV", H3, 0x9E3039, 0xFFFFFF, 0x9E3039 },
    { "EE", H3, 0x0072CE, 0x000000, 0xFFFFFF },
    { "LT", H3, 0xFDB913, 0x006A44, 0xC1272D },
    { "BG", H3, 0xFFFFFF, 0x00966E, 0xD62612 },
    { "HU", H3, 0xCE2939, 0xFFFFFF, 0x477050 },
    { "NG", V3, 0x008751, 0xFFFFFF, 0x008751 },
    { "AM", H3, 0xD90012, 0x0033A0, 0xF2A800 },
    { "CO", H3, 0xFCD116, 0x003893, 0xCE1126 },
    { "LU", H3, 0xED2939, 0xFFFFFF, 0x00A1DE },
    { "RS", H3, 0xC6363C, 0x0C4076, 0xFFFFFF },
    { "SK", H3, 0xFFFFFF, 0x0B4EA2, 0xEE1C25 },
    { "SI", H3, 0xFFFFFF, 0x0000C6, 0xED1C24 },
    { "HR", H3, 0xFF0000, 0xFFFFFF, 0x171796 },
    { "NL2", H3, 0xAE1C28, 0xFFFFFF, 0x21468B },

    { "UA", H2, 0x0057B7, 0xFFD700, 0 },
    { "PL", H2, 0xFFFFFF, 0xDC143C, 0 },
    { "ID", H2, 0xCE1126, 0xFFFFFF, 0 },
    { "MC", H2, 0xCE1126, 0xFFFFFF, 0 },
    { "SG", H2, 0xED2939, 0xFFFFFF, 0 },
    { "VN", H2, 0xDA251D, 0xDA251D, 0 },

    { "FI", NORDIC, 0xFFFFFF, 0x003580, 0 },
    { "SE", NORDIC, 0x006AA7, 0xFECC00, 0 },
    { "NO", NORDIC, 0xEF2B2D, 0xFFFFFF, 0 },
    { "DK", NORDIC, 0xC8102E, 0xFFFFFF, 0 },
    { "IS", NORDIC, 0x02529C, 0xFFFFFF, 0 },

    { "JP", SPECIAL, 0, 0, 0 },
    { "CH", SPECIAL, 0, 0, 0 },
    { "GB", SPECIAL, 0, 0, 0 },
    { "US", SPECIAL, 0, 0, 0 },
    { "KR", SPECIAL, 0, 0, 0 },
    { "TR", SPECIAL, 0, 0, 0 },
    { "CA", SPECIAL, 0, 0, 0 },
    { "BR", SPECIAL, 0, 0, 0 },
    { "IN", SPECIAL, 0, 0, 0 },
    { "PT", SPECIAL, 0, 0, 0 },
    { "ES", SPECIAL, 0, 0, 0 },
    { "GR", SPECIAL, 0, 0, 0 },
    { "CZ", SPECIAL, 0, 0, 0 },
    { "KZ", SPECIAL, 0, 0, 0 },
    { "AE", SPECIAL, 0, 0, 0 },
    { "IL", SPECIAL, 0, 0, 0 },
    { "HK", SPECIAL, 0, 0, 0 },
    { "CN", SPECIAL, 0, 0, 0 },
    { "MY", SPECIAL, 0, 0, 0 },
    { "PH", SPECIAL, 0, 0, 0 },
    { "AU", SPECIAL, 0, 0, 0 },
    { "GE", SPECIAL, 0, 0, 0 },
    { "CY", SPECIAL, 0, 0, 0 },
    { "MD", SPECIAL, 0, 0, 0 },
    { "BY", SPECIAL, 0, 0, 0 },
    { "UZ", SPECIAL, 0, 0, 0 },
    { "AR", SPECIAL, 0, 0, 0 },
    { "MX", SPECIAL, 0, 0, 0 },
    { "ZA", SPECIAL, 0, 0, 0 },
    { "TH", SPECIAL, 0, 0, 0 },
};

D2D1_COLOR_F rgb(UINT32 hex) {
    return D2D1::ColorF(hex);
}

void band(Painter& p, D2D1_RECT_F r, float from, float to, UINT32 color, bool vertical) {
    float w = r.right - r.left;
    float h = r.bottom - r.top;
    D2D1_RECT_F part = vertical
        ? D2D1::RectF(r.left + w * from, r.top, r.left + w * to, r.bottom)
        : D2D1::RectF(r.left, r.top + h * from, r.right, r.top + h * to);
    p.rect(part, rgb(color));
}

void stripes3(Painter& p, D2D1_RECT_F r, const Flag& f, bool vertical) {
    band(p, r, 0.f, 1.f / 3.f, f.a, vertical);
    band(p, r, 1.f / 3.f, 2.f / 3.f, f.b, vertical);
    band(p, r, 2.f / 3.f, 1.f, f.c, vertical);
}

void nordic(Painter& p, D2D1_RECT_F r, const Flag& f) {
    p.rect(r, rgb(f.a));
    float w = r.right - r.left;
    float h = r.bottom - r.top;
    float thick = h * 0.22f;
    p.rect(D2D1::RectF(r.left, r.top + h * 0.5f - thick / 2, r.right, r.top + h * 0.5f + thick / 2), rgb(f.b));
    p.rect(D2D1::RectF(r.left + w * 0.34f - thick / 2, r.top, r.left + w * 0.34f + thick / 2, r.bottom), rgb(f.b));
}

// звездочка без лучей, на таком размере все равно читается как точка
void dot(Painter& p, D2D1_POINT_2F at, float radius, UINT32 color) {
    p.circle(at, radius, rgb(color));
}

void special(Painter& p, D2D1_RECT_F r, const std::string& code) {
    float w = r.right - r.left;
    float h = r.bottom - r.top;
    D2D1_POINT_2F center = D2D1::Point2F((r.left + r.right) / 2.f, (r.top + r.bottom) / 2.f);

    if (code == "JP") {
        p.rect(r, rgb(0xFFFFFF));
        dot(p, center, h * 0.30f, 0xBC002D);
    } else if (code == "CN") {
        p.rect(r, rgb(0xDE2910));
        dot(p, D2D1::Point2F(r.left + w * 0.22f, r.top + h * 0.32f), h * 0.16f, 0xFFDE00);
        dot(p, D2D1::Point2F(r.left + w * 0.40f, r.top + h * 0.18f), h * 0.06f, 0xFFDE00);
        dot(p, D2D1::Point2F(r.left + w * 0.46f, r.top + h * 0.34f), h * 0.06f, 0xFFDE00);
        dot(p, D2D1::Point2F(r.left + w * 0.40f, r.top + h * 0.52f), h * 0.06f, 0xFFDE00);
    } else if (code == "CH") {
        p.rect(r, rgb(0xD52B1E));
        float thick = h * 0.18f;
        float arm = h * 0.30f;
        p.rect(D2D1::RectF(center.x - arm, center.y - thick / 2, center.x + arm, center.y + thick / 2), rgb(0xFFFFFF));
        p.rect(D2D1::RectF(center.x - thick / 2, center.y - arm, center.x + thick / 2, center.y + arm), rgb(0xFFFFFF));
    } else if (code == "GB") {
        p.rect(r, rgb(0x012169));
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.right, r.bottom), rgb(0xFFFFFF), h * 0.22f);
        p.line(D2D1::Point2F(r.right, r.top), D2D1::Point2F(r.left, r.bottom), rgb(0xFFFFFF), h * 0.22f);
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.right, r.bottom), rgb(0xC8102E), h * 0.10f);
        p.line(D2D1::Point2F(r.right, r.top), D2D1::Point2F(r.left, r.bottom), rgb(0xC8102E), h * 0.10f);
        p.rect(D2D1::RectF(r.left, center.y - h * 0.15f, r.right, center.y + h * 0.15f), rgb(0xFFFFFF));
        p.rect(D2D1::RectF(center.x - w * 0.10f, r.top, center.x + w * 0.10f, r.bottom), rgb(0xFFFFFF));
        p.rect(D2D1::RectF(r.left, center.y - h * 0.09f, r.right, center.y + h * 0.09f), rgb(0xC8102E));
        p.rect(D2D1::RectF(center.x - w * 0.06f, r.top, center.x + w * 0.06f, r.bottom), rgb(0xC8102E));
    } else if (code == "US") {
        p.rect(r, rgb(0xFFFFFF));
        for (int i = 0; i < 7; i++) {
            float from = i * 2.f / 13.f;
            band(p, r, from, from + 1.f / 13.f, 0xB31942, false);
        }
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.42f, r.top + h * 7.f / 13.f), rgb(0x0A3161));
    } else if (code == "KR") {
        p.rect(r, rgb(0xFFFFFF));
        dot(p, center, h * 0.26f, 0xCD2E3A);
        p.rect(D2D1::RectF(center.x - h * 0.26f, center.y, center.x + h * 0.26f, center.y + h * 0.26f), rgb(0x0047A0));
        dot(p, center, h * 0.26f * 0.5f, 0xCD2E3A);
    } else if (code == "TR") {
        p.rect(r, rgb(0xE30A17));
        dot(p, D2D1::Point2F(r.left + w * 0.38f, center.y), h * 0.24f, 0xFFFFFF);
        dot(p, D2D1::Point2F(r.left + w * 0.45f, center.y), h * 0.19f, 0xE30A17);
        dot(p, D2D1::Point2F(r.left + w * 0.62f, center.y), h * 0.07f, 0xFFFFFF);
    } else if (code == "CA") {
        p.rect(r, rgb(0xFFFFFF));
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.25f, r.bottom), rgb(0xD80621));
        p.rect(D2D1::RectF(r.right - w * 0.25f, r.top, r.right, r.bottom), rgb(0xD80621));
        dot(p, center, h * 0.22f, 0xD80621);
    } else if (code == "BR") {
        p.rect(r, rgb(0x009B3A));
        dot(p, center, h * 0.34f, 0xFEDF00);
        dot(p, center, h * 0.18f, 0x002776);
    } else if (code == "IN") {
        band(p, r, 0.f, 1.f / 3.f, 0xFF9933, false);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFFFFF, false);
        band(p, r, 2.f / 3.f, 1.f, 0x138808, false);
        dot(p, center, h * 0.12f, 0x000080);
    } else if (code == "PT") {
        p.rect(r, rgb(0xFF0000));
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.40f, r.bottom), rgb(0x006600));
        dot(p, D2D1::Point2F(r.left + w * 0.40f, center.y), h * 0.20f, 0xFFFF00);
    } else if (code == "ES") {
        band(p, r, 0.f, 0.25f, 0xAA151B, false);
        band(p, r, 0.25f, 0.75f, 0xF1BF00, false);
        band(p, r, 0.75f, 1.f, 0xAA151B, false);
    } else if (code == "GR") {
        p.rect(r, rgb(0xFFFFFF));
        for (int i = 0; i < 5; i++) {
            float from = i * 2.f / 9.f;
            band(p, r, from, from + 1.f / 9.f, 0x0D5EAF, false);
        }
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.40f, r.top + h * 5.f / 9.f), rgb(0x0D5EAF));
        p.rect(D2D1::RectF(r.left + w * 0.14f, r.top, r.left + w * 0.26f, r.top + h * 5.f / 9.f), rgb(0xFFFFFF));
        p.rect(D2D1::RectF(r.left, r.top + h * 2.f / 9.f, r.left + w * 0.40f, r.top + h * 3.f / 9.f), rgb(0xFFFFFF));
    } else if (code == "CZ") {
        band(p, r, 0.f, 0.5f, 0xFFFFFF, false);
        band(p, r, 0.5f, 1.f, 0xD7141A, false);
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left + w * 0.42f, center.y), rgb(0x11457E), h * 0.55f);
    } else if (code == "KZ") {
        p.rect(r, rgb(0x00AFCA));
        dot(p, center, h * 0.20f, 0xFEC50C);
    } else if (code == "AE") {
        band(p, r, 0.f, 1.f / 3.f, 0x00732F, false);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFFFFF, false);
        band(p, r, 2.f / 3.f, 1.f, 0x000000, false);
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.26f, r.bottom), rgb(0xFF0000));
    } else if (code == "IL") {
        p.rect(r, rgb(0xFFFFFF));
        p.rect(D2D1::RectF(r.left, r.top + h * 0.12f, r.right, r.top + h * 0.26f), rgb(0x0038B8));
        p.rect(D2D1::RectF(r.left, r.bottom - h * 0.26f, r.right, r.bottom - h * 0.12f), rgb(0x0038B8));
        dot(p, center, h * 0.13f, 0x0038B8);
        dot(p, center, h * 0.07f, 0xFFFFFF);
    } else if (code == "HK") {
        p.rect(r, rgb(0xDE2910));
        dot(p, center, h * 0.22f, 0xFFFFFF);
        dot(p, center, h * 0.09f, 0xDE2910);
    } else if (code == "MY") {
        p.rect(r, rgb(0xFFFFFF));
        for (int i = 0; i < 7; i++) {
            float from = i * 2.f / 14.f;
            band(p, r, from, from + 1.f / 14.f, 0xCC0001, false);
        }
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.50f, r.top + h * 8.f / 14.f), rgb(0x010066));
        dot(p, D2D1::Point2F(r.left + w * 0.22f, r.top + h * 0.28f), h * 0.12f, 0xFFCC00);
        dot(p, D2D1::Point2F(r.left + w * 0.28f, r.top + h * 0.28f), h * 0.09f, 0x010066);
    } else if (code == "PH") {
        p.rect(r, rgb(0x0038A8));
        p.rect(D2D1::RectF(r.left, center.y, r.right, r.bottom), rgb(0xCE1126));
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left + w * 0.40f, center.y), rgb(0xFFFFFF), h * 0.55f);
        dot(p, D2D1::Point2F(r.left + w * 0.13f, center.y), h * 0.12f, 0xFCD116);
    } else if (code == "AU") {
        p.rect(r, rgb(0x012169));
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.45f, r.top + h * 0.5f), rgb(0x0A2A66));
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left + w * 0.45f, r.top + h * 0.5f), rgb(0xFFFFFF), h * 0.10f);
        p.line(D2D1::Point2F(r.left + w * 0.45f, r.top), D2D1::Point2F(r.left, r.top + h * 0.5f), rgb(0xFFFFFF), h * 0.10f);
        dot(p, D2D1::Point2F(r.left + w * 0.72f, r.top + h * 0.62f), h * 0.09f, 0xFFFFFF);
        dot(p, D2D1::Point2F(r.left + w * 0.60f, r.top + h * 0.30f), h * 0.06f, 0xFFFFFF);
    } else if (code == "GE") {
        p.rect(r, rgb(0xFFFFFF));
        p.rect(D2D1::RectF(r.left, center.y - h * 0.12f, r.right, center.y + h * 0.12f), rgb(0xFF0000));
        p.rect(D2D1::RectF(center.x - w * 0.09f, r.top, center.x + w * 0.09f, r.bottom), rgb(0xFF0000));
    } else if (code == "CY") {
        p.rect(r, rgb(0xFFFFFF));
        dot(p, center, h * 0.18f, 0xD57800);
    } else if (code == "MD") {
        band(p, r, 0.f, 1.f / 3.f, 0x0033A0, true);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFD200, true);
        band(p, r, 2.f / 3.f, 1.f, 0xCC092F, true);
        dot(p, center, h * 0.14f, 0xA77B3B);
    } else if (code == "BY") {
        p.rect(r, rgb(0xCE1720));
        p.rect(D2D1::RectF(r.left, r.top + h * 0.66f, r.right, r.bottom), rgb(0x007C30));
        p.rect(D2D1::RectF(r.left, r.top, r.left + w * 0.16f, r.bottom), rgb(0xFFFFFF));
    } else if (code == "UZ") {
        band(p, r, 0.f, 1.f / 3.f, 0x0099B5, false);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFFFFF, false);
        band(p, r, 2.f / 3.f, 1.f, 0x1EB53A, false);
        dot(p, D2D1::Point2F(r.left + w * 0.18f, r.top + h * 0.17f), h * 0.09f, 0xFFFFFF);
    } else if (code == "AR") {
        band(p, r, 0.f, 1.f / 3.f, 0x74ACDF, false);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFFFFF, false);
        band(p, r, 2.f / 3.f, 1.f, 0x74ACDF, false);
        dot(p, center, h * 0.12f, 0xF6B40E);
    } else if (code == "MX") {
        band(p, r, 0.f, 1.f / 3.f, 0x006847, true);
        band(p, r, 1.f / 3.f, 2.f / 3.f, 0xFFFFFF, true);
        band(p, r, 2.f / 3.f, 1.f, 0xCE1126, true);
        dot(p, center, h * 0.14f, 0x8B5A2B);
    } else if (code == "ZA") {
        p.rect(r, rgb(0x002395));
        p.rect(D2D1::RectF(r.left, r.top, r.right, center.y), rgb(0xDE3831));
        p.line(D2D1::Point2F(r.left, center.y), D2D1::Point2F(r.right, center.y), rgb(0x007A4D), h * 0.30f);
        p.line(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left + w * 0.45f, center.y), rgb(0xFFB612), h * 0.22f);
        p.line(D2D1::Point2F(r.left, r.bottom), D2D1::Point2F(r.left + w * 0.45f, center.y), rgb(0xFFB612), h * 0.22f);
    } else if (code == "TH") {
        band(p, r, 0.f, 1.f / 6.f, 0xA51931, false);
        band(p, r, 1.f / 6.f, 2.f / 6.f, 0xFFFFFF, false);
        band(p, r, 2.f / 6.f, 4.f / 6.f, 0x2D2A4A, false);
        band(p, r, 4.f / 6.f, 5.f / 6.f, 0xFFFFFF, false);
        band(p, r, 5.f / 6.f, 1.f, 0xA51931, false);
    } else {
        return;
    }
}

const Flag* find(const std::string& code) {
    for (const Flag& f : kFlags) {
        if (code == f.code) return &f;
    }
    return nullptr;
}

} // namespace

bool draw(Painter& p, D2D1_RECT_F r, const std::string& code) {
    const Flag* flag = find(code);
    if (flag == nullptr) return false;

    p.pushClip(r);
    switch (flag->kind) {
    case H3: stripes3(p, r, *flag, false); break;
    case V3: stripes3(p, r, *flag, true); break;
    case H2:
        p.rect(D2D1::RectF(r.left, r.top, r.right, (r.top + r.bottom) / 2.f), rgb(flag->a));
        p.rect(D2D1::RectF(r.left, (r.top + r.bottom) / 2.f, r.right, r.bottom), rgb(flag->b));
        break;
    case NORDIC: nordic(p, r, *flag); break;
    case SPECIAL: special(p, r, code); break;
    }
    p.popClip();
    return true;
}

} // namespace flags
