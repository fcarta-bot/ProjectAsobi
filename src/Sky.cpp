#include "Sky.hpp"
#include <cmath>
#include <random>
#include <vector>

namespace {

enum class Shape { Building, Pagoda, Tower, Hill, Castle, Spire };

struct SkyItem {
    Shape shape;
    float angle;  // direzione in radianti (0 = verso destra sulla mappa)
    float width;  // in pixel a 720 di altezza schermo
    float height;
    unsigned seed;
};

// Ogni distretto ha il suo profilo, sempre uguale: lo si riconosce girandosi
const std::vector<SkyItem>& Items(const District& d) {
    static std::vector<SkyItem> cache[4];
    std::vector<SkyItem>& items = cache[(int)d.id];
    if (!items.empty()) return items;

    std::mt19937 rng(1234 + (unsigned)d.id);
    auto rf = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    const float TWO_PI = 6.2831853f;
    auto add = [&](Shape s, float angle, float w, float h) { items.push_back({ s, angle, w, h, (unsigned)rng() }); };

    switch (d.skyline) {
        case Skyline::Temples: // colline di Higashiyama, tetti bassi e qualche pagoda
            for (int i = 0; i < 7; ++i) add(Shape::Hill, i * TWO_PI / 7 + rf(0, 0.4f), rf(500, 800), rf(60, 110));
            for (int i = 0; i < 40; ++i) add(Shape::Building, rf(0, TWO_PI), rf(30, 70), rf(12, 30));
            for (int i = 0; i < 3; ++i) add(Shape::Pagoda, i * TWO_PI / 3 + rf(0, 1), 50, rf(120, 150));
            break;
        case Skyline::Hills: // il monte Wakakusa, boschi e una pagoda tra gli alberi
            for (int i = 0; i < 6; ++i) add(Shape::Hill, i * TWO_PI / 6 + rf(0, 0.5f), rf(600, 900), rf(80, 140));
            for (int i = 0; i < 2; ++i) add(Shape::Pagoda, i * 3.1f + rf(0, 1), 46, rf(110, 130));
            break;
        case Skyline::Downtown: // palazzi fitti, un castello e una torre panoramica
            for (int i = 0; i < 70; ++i) add(Shape::Building, rf(0, TWO_PI), rf(40, 110), rf(40, 150));
            add(Shape::Castle, rf(0, TWO_PI), 120, 130);
            add(Shape::Tower, rf(0, TWO_PI), 60, 210);
            break;
        case Skyline::Skyscrapers: // grattacieli altissimi e due torri
            for (int i = 0; i < 90; ++i) add(Shape::Building, rf(0, TWO_PI), rf(40, 120), rf(60, 260));
            add(Shape::Tower, rf(0, TWO_PI), 70, 260);
            add(Shape::Spire, rf(0, TWO_PI), 30, 340);
            break;
    }
    return items;
}

// Differenza tra angoli riportata tra -pi e pi
float AngleDelta(float a, float b) {
    float d = std::fmod(a - b, 6.2831853f);
    if (d > 3.1415927f) d -= 6.2831853f;
    if (d < -3.1415927f) d += 6.2831853f;
    return d;
}

} // namespace

void DrawSky(const District& d, float yaw, float fovy) {
    const int sw = GetScreenWidth(), sh = GetScreenHeight();
    const float horizonY = sh * 0.5f; // la telecamera non guarda ne' su ne' giu'
    const float scale = sh / 720.0f;
    const float pxPerRad = horizonY / std::tan(fovy * DEG2RAD * 0.5f);

    DrawRectangleGradientV(0, 0, sw, (int)horizonY + 1, d.skyTop, d.horizon);
    DrawRectangle(0, (int)horizonY, sw, sh - (int)horizonY, d.horizon);

    auto screenX = [&](float angle) { return sw * 0.5f + AngleDelta(angle, yaw) * pxPerRad; };

    // Stelle (di notte) o sole basso al tramonto
    if (d.night) {
        for (int i = 0; i < 120; ++i) {
            unsigned h = TileHash(i, 7, 99);
            float x = screenX((h % 6283) / 1000.0f);
            float y = horizonY * (0.05f + (h / 6283 % 60) / 100.0f);
            if (x >= 0 && x < sw) DrawPixelV({ x, y }, Fade(WHITE, 0.5f + (h % 5) / 10.0f));
        }
        float mx = screenX(2.2f);
        DrawCircleV({ mx, horizonY * 0.3f }, 16 * scale, { 235, 235, 215, 255 });
    } else if (d.id == DistrictId::Kyoto) {
        float sx = screenX(4.0f);
        DrawCircleV({ sx, horizonY - 40 * scale }, 60 * scale, Fade({ 255, 200, 120, 255 }, 0.35f));
        DrawCircleV({ sx, horizonY - 40 * scale }, 34 * scale, { 255, 220, 150, 255 });
    }

    // Profilo della citta': piu' scuro del cielo, come visto controluce nella foschia
    const Color far = Mix(d.horizon, d.skyTop, 0.35f);
    const Color nearer = Mix(d.horizon, d.skyTop, 0.6f);
    for (const SkyItem& it : Items(d)) {
        float w = it.width * scale, h = it.height * scale;
        float x = screenX(it.angle);
        if (x + w < -50 || x - w > sw + 50) continue;
        float base = horizonY + 4;
        switch (it.shape) {
            case Shape::Hill:
                DrawEllipse((int)x, (int)base, w * 0.5f, h, far);
                break;
            case Shape::Building: {
                DrawRectangle((int)(x - w / 2), (int)(base - h), (int)w, (int)h + 4, nearer);
                if (d.night) { // finestre accese sparse
                    for (int k = 0; k < 10; ++k) {
                        unsigned r = TileHash((int)it.seed, k, 5);
                        if (r % 3) continue;
                        float wx = x - w / 2 + 4 + (r >> 4) % (int)std::max(1.0f, w - 8);
                        float wy = base - h + 6 + (r >> 10) % (int)std::max(1.0f, h - 10);
                        DrawRectangle((int)wx, (int)wy, 3, 2, { 255, 210, 140, 200 });
                    }
                }
                break;
            }
            case Shape::Pagoda: { // cinque tetti che si stringono, con la guglia
                float y = base;
                for (int k = 0; k < 5; ++k) {
                    float tw = w * (1.0f - k * 0.13f), th = h / 7;
                    DrawRectangle((int)(x - tw * 0.3f), (int)(y - th), (int)(tw * 0.6f), (int)th, nearer);
                    DrawTriangle({ x - tw / 2, y - th }, { x + tw / 2, y - th }, { x, y - th * 1.5f }, nearer);
                    y -= th;
                }
                DrawRectangle((int)(x - 1.5f * scale), (int)(y - h * 0.25f), (int)(3 * scale), (int)(h * 0.25f), nearer);
                break;
            }
            case Shape::Castle: {
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.35f), (int)w, (int)(h * 0.35f), nearer);
                for (int k = 0; k < 3; ++k) {
                    float tw = w * (0.9f - k * 0.22f), y = base - h * (0.35f + k * 0.2f);
                    DrawTriangle({ x - tw / 2, y }, { x + tw / 2, y }, { x, y - h * 0.14f }, nearer);
                    DrawRectangle((int)(x - tw * 0.3f), (int)(y - h * 0.08f), (int)(tw * 0.6f), (int)(h * 0.08f), nearer);
                }
                break;
            }
            case Shape::Tower: { // torre a traliccio che si assottiglia
                DrawTriangle({ x - w / 2, base }, { x + w / 2, base }, { x, base - h }, nearer);
                DrawRectangle((int)(x - w * 0.2f), (int)(base - h * 0.55f), (int)(w * 0.4f), (int)(6 * scale), d.night ? d.accent : nearer);
                if (d.night) DrawCircleV({ x, base - h }, 3 * scale, RED);
                break;
            }
            case Shape::Spire:
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.7f), (int)w, (int)(h * 0.7f), nearer);
                DrawTriangle({ x - w / 2, base - h * 0.7f }, { x + w / 2, base - h * 0.7f }, { x, base - h }, nearer);
                if (d.night) DrawRectangle((int)(x - w / 2), (int)(base - h * 0.45f), (int)w, (int)(4 * scale), d.accent);
                break;
        }
    }
}
