#include "Sky.hpp"
#include <cmath>
#include <random>
#include <vector>

namespace {

enum class Shape {
    Building, Pagoda, Tower, Hill, Castle, Spire,
    Dome, Eiffel, ClockTower, Wheel, Palm, Colosseum, OnionTowers, Volcano, Liberty, Campanile,
};

struct SkyItem {
    Shape shape;
    float angle;  // direzione in radianti (0 = verso destra sulla mappa)
    float width;  // in pixel a 720 di altezza schermo
    float height;
    unsigned seed;
};

// Ogni distretto ha il suo profilo, sempre uguale: lo si riconosce girandosi
const std::vector<SkyItem>& Items(const District& d) {
    static std::vector<SkyItem> cache[DISTRICT_COUNT];
    std::vector<SkyItem>& items = cache[(int)d.id];
    if (!items.empty()) return items;

    std::mt19937 rng(1234 + (unsigned)d.id);
    auto rf = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    const float TWO_PI = 6.2831853f;
    auto add = [&](Shape s, float angle, float w, float h) { items.push_back({ s, angle, w, h, (unsigned)rng() }); };
    auto buildings = [&](int n, float wMin, float wMax, float hMin, float hMax) {
        for (int i = 0; i < n; ++i) add(Shape::Building, rf(0, TWO_PI), rf(wMin, wMax), rf(hMin, hMax));
    };
    auto hills = [&](int n, float hMin, float hMax) {
        for (int i = 0; i < n; ++i) add(Shape::Hill, i * TWO_PI / n + rf(0, 0.4f), rf(500, 850), rf(hMin, hMax));
    };

    switch (d.id) {
        case DistrictId::Kyoto: // colline di Higashiyama, tetti bassi e qualche pagoda
            hills(7, 60, 110);
            buildings(40, 30, 70, 12, 30);
            for (int i = 0; i < 3; ++i) add(Shape::Pagoda, i * TWO_PI / 3 + rf(0, 1), 50, rf(120, 150));
            break;
        case DistrictId::Nara: // il monte Wakakusa, boschi e una pagoda tra gli alberi
            hills(6, 80, 140);
            for (int i = 0; i < 2; ++i) add(Shape::Pagoda, i * 3.1f + rf(0, 1), 46, rf(110, 130));
            break;
        case DistrictId::Osaka: // palazzi fitti, il castello e una torre panoramica
            buildings(70, 40, 110, 40, 150);
            add(Shape::Castle, rf(0, TWO_PI), 120, 130);
            add(Shape::Tower, rf(0, TWO_PI), 60, 210);
            break;
        case DistrictId::Tokyo: // grattacieli altissimi e due torri
            buildings(90, 40, 120, 60, 260);
            add(Shape::Tower, rf(0, TWO_PI), 70, 260);
            add(Shape::Spire, rf(0, TWO_PI), 30, 340);
            break;
        case DistrictId::NewYork: // la selva di grattacieli, l'Empire State e la Statua della Liberta' sul fiume
            buildings(110, 40, 110, 80, 300);
            add(Shape::Spire, rf(0, TWO_PI), 50, 380);
            add(Shape::Liberty, rf(0, TWO_PI), 40, 150);
            break;
        case DistrictId::NewOrleans: // case basse, la cattedrale di St. Louis e le querce del bayou
            buildings(40, 40, 80, 20, 60);
            add(Shape::Campanile, rf(0, TWO_PI), 40, 150);
            for (int i = 0; i < 12; ++i) add(Shape::Hill, rf(0, TWO_PI), rf(80, 160), rf(30, 60)); // chiome lontane
            break;
        case DistrictId::Chicago: // grattacieli sul lago e la Willis Tower
            buildings(70, 50, 120, 80, 260);
            add(Shape::Spire, rf(0, TWO_PI), 70, 400);
            break;
        case DistrictId::LasVegas: // casino', la torre Stratosphere, palme e le montagne del deserto
            hills(5, 70, 130);
            buildings(30, 60, 140, 60, 180);
            add(Shape::Tower, rf(0, TWO_PI), 50, 300);
            add(Shape::Eiffel, rf(0, TWO_PI), 60, 160); // la copia della Tour Eiffel!
            for (int i = 0; i < 14; ++i) add(Shape::Palm, rf(0, TWO_PI), 40, rf(60, 90));
            break;
        case DistrictId::Rome: // la cupola di San Pietro, il Colosseo e i pini
            buildings(40, 40, 90, 20, 60);
            add(Shape::Dome, rf(0, TWO_PI), 150, 150);
            add(Shape::Colosseum, rf(0, TWO_PI), 220, 90);
            for (int i = 0; i < 10; ++i) add(Shape::Hill, rf(0, TWO_PI), rf(70, 110), rf(18, 28)); // pini a ombrello
            break;
        case DistrictId::Venice: // campanile di San Marco e cupole nella nebbia della laguna
            buildings(30, 40, 80, 20, 50);
            add(Shape::Campanile, rf(0, TWO_PI), 40, 230);
            add(Shape::Dome, rf(0, TWO_PI), 90, 80);
            add(Shape::Dome, rf(0, TWO_PI), 110, 100);
            break;
        case DistrictId::Naples: // il Vesuvio domina tutto
            buildings(60, 40, 90, 30, 90);
            add(Shape::Volcano, rf(0, TWO_PI), 900, 230);
            add(Shape::Castle, rf(0, TWO_PI), 140, 80);
            break;
        case DistrictId::Florence: // la cupola del Brunelleschi, il campanile di Giotto e le colline
            hills(6, 50, 90);
            buildings(40, 40, 80, 20, 50);
            add(Shape::Dome, rf(0, TWO_PI), 160, 180);
            add(Shape::Campanile, rf(0, TWO_PI), 40, 200);
            break;
        case DistrictId::Paris: // la Tour Eiffel e il Sacre'-Coeur
            buildings(70, 50, 100, 30, 70);
            add(Shape::Eiffel, rf(0, TWO_PI), 110, 330);
            add(Shape::Dome, rf(0, TWO_PI), 120, 110);
            break;
        case DistrictId::Munich: // le torri a cipolla della Frauenkirche e le Alpi innevate
            hills(5, 140, 220);
            buildings(40, 40, 80, 30, 70);
            add(Shape::OnionTowers, rf(0, TWO_PI), 90, 220);
            break;
        case DistrictId::London: // il Big Ben, la ruota panoramica e i palazzi nella nebbia
            buildings(60, 40, 100, 40, 140);
            add(Shape::ClockTower, rf(0, TWO_PI), 50, 260);
            add(Shape::Wheel, rf(0, TWO_PI), 200, 200);
            add(Shape::Dome, rf(0, TWO_PI), 120, 120); // St Paul
            break;
        case DistrictId::Seville: // la Giralda, le palme e le case basse
            buildings(50, 40, 90, 20, 50);
            add(Shape::Campanile, rf(0, TWO_PI), 50, 240);
            for (int i = 0; i < 10; ++i) add(Shape::Palm, rf(0, TWO_PI), 40, rf(60, 90));
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

// I triangoli 2D di raylib si vedono solo in un verso: li disegniamo in entrambi
void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
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

    // Stelle (di notte) o sole basso
    if (d.night) {
        for (int i = 0; i < 120; ++i) {
            unsigned h = TileHash(i, 7, 99);
            float x = screenX((h % 6283) / 1000.0f);
            float y = horizonY * (0.05f + (h / 6283 % 60) / 100.0f);
            if (x >= 0 && x < sw) DrawPixelV({ x, y }, Fade(WHITE, 0.5f + (h % 5) / 10.0f));
        }
        float mx = screenX(2.2f);
        DrawCircleV({ mx, horizonY * 0.3f }, 16 * scale, { 235, 235, 215, 255 });
    } else if (d.sunAngle >= 0) {
        float sx = screenX(d.sunAngle);
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
                if (d.snow && h > 100 * scale) // cime innevate
                    DrawEllipse((int)x, (int)(base - h * 0.82f), w * 0.12f, h * 0.18f, Mix(far, WHITE, 0.5f));
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
                    Tri({ x - tw / 2, y - th }, { x + tw / 2, y - th }, { x, y - th * 1.5f }, nearer);
                    y -= th;
                }
                DrawRectangle((int)(x - 1.5f * scale), (int)(y - h * 0.25f), (int)(3 * scale), (int)(h * 0.25f), nearer);
                break;
            }
            case Shape::Castle: {
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.35f), (int)w, (int)(h * 0.35f), nearer);
                for (int k = 0; k < 3; ++k) {
                    float tw = w * (0.9f - k * 0.22f), y = base - h * (0.35f + k * 0.2f);
                    Tri({ x - tw / 2, y }, { x + tw / 2, y }, { x, y - h * 0.14f }, nearer);
                    DrawRectangle((int)(x - tw * 0.3f), (int)(y - h * 0.08f), (int)(tw * 0.6f), (int)(h * 0.08f), nearer);
                }
                break;
            }
            case Shape::Tower: { // torre a traliccio che si assottiglia
                Tri({ x - w / 2, base }, { x + w / 2, base }, { x, base - h }, nearer);
                DrawRectangle((int)(x - w * 0.2f), (int)(base - h * 0.55f), (int)(w * 0.4f), (int)(6 * scale), d.night ? d.accent : nearer);
                if (d.night) DrawCircleV({ x, base - h }, 3 * scale, RED);
                break;
            }
            case Shape::Spire:
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.7f), (int)w, (int)(h * 0.7f), nearer);
                Tri({ x - w / 2, base - h * 0.7f }, { x + w / 2, base - h * 0.7f }, { x, base - h }, nearer);
                if (d.night) DrawRectangle((int)(x - w / 2), (int)(base - h * 0.45f), (int)w, (int)(4 * scale), d.accent);
                break;
            case Shape::Dome: // tamburo, cupola e lanterna in cima
                DrawRectangle((int)(x - w * 0.4f), (int)(base - h * 0.45f), (int)(w * 0.8f), (int)(h * 0.45f) + 4, nearer);
                DrawCircleSector({ x, base - h * 0.45f }, w * 0.36f, 180, 360, 16, nearer);
                DrawRectangle((int)(x - w * 0.05f), (int)(base - h), (int)(w * 0.1f), (int)(h * 0.25f), nearer);
                break;
            case Shape::Eiffel: { // quattro gambe che si uniscono, con le due terrazze
                Tri({ x - w / 2, base }, { x - w * 0.1f, base - h * 0.5f }, { x - w * 0.25f, base }, nearer);
                Tri({ x + w / 2, base }, { x + w * 0.1f, base - h * 0.5f }, { x + w * 0.25f, base }, nearer);
                DrawRectangle((int)(x - w * 0.3f), (int)(base - h * 0.3f), (int)(w * 0.6f), (int)(5 * scale), nearer);
                Tri({ x - w * 0.12f, base - h * 0.48f }, { x + w * 0.12f, base - h * 0.48f }, { x, base - h }, nearer);
                if (d.night) // scintilla allo scoccare dell'ora (qui sempre)
                    for (int k = 0; k < 8; ++k) {
                        unsigned r = TileHash(k, (int)(GetTime() * 6), 3);
                        DrawPixelV({ x + ((int)(r % 40) - 20) * w / 80, base - h * (0.2f + (r >> 8) % 70 / 100.0f) }, { 255, 240, 200, 255 });
                    }
                break;
            }
            case Shape::ClockTower: // il Big Ben: torre, quadrante e punta
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.8f), (int)w, (int)(h * 0.8f) + 4, nearer);
                DrawCircleV({ x, base - h * 0.68f }, w * 0.35f, d.night ? Color{ 250, 230, 170, 255 } : Mix(nearer, WHITE, 0.3f));
                Tri({ x - w / 2, base - h * 0.8f }, { x + w / 2, base - h * 0.8f }, { x, base - h }, nearer);
                break;
            case Shape::Wheel: { // ruota panoramica
                Vector2 c{ x, base - h * 0.5f };
                DrawRing(c, w * 0.45f, w * 0.48f, 0, 360, 40, nearer);
                for (int k = 0; k < 8; ++k) {
                    float a = k * PI / 4 + (float)GetTime() * 0.05f;
                    DrawLineEx(c, { c.x + cosf(a) * w * 0.46f, c.y + sinf(a) * w * 0.46f }, 1.5f * scale, nearer);
                }
                Tri({ c.x, c.y }, { c.x - w * 0.25f, base }, { c.x - w * 0.2f, base }, nearer);
                Tri({ c.x, c.y }, { c.x + w * 0.25f, base }, { c.x + w * 0.2f, base }, nearer);
                break;
            }
            case Shape::Palm: {
                DrawLineEx({ x, base }, { x + w * 0.2f, base - h }, 3 * scale, nearer);
                for (int k = 0; k < 5; ++k) {
                    float a = PI + k * PI / 4;
                    DrawLineEx({ x + w * 0.2f, base - h }, { x + w * 0.2f + cosf(a) * w * 0.6f, base - h + sinf(a) * w * 0.3f + w * 0.2f },
                               3 * scale, nearer);
                }
                break;
            }
            case Shape::Colosseum: // anello di archi su tre piani, piu' basso da un lato (e' crollato)
                for (int k = 0; k < 3; ++k) {
                    float y0 = base - (k + 1) * h / 3;
                    float tw = k == 2 ? w * 0.55f : w;
                    DrawRectangle((int)(x - w / 2), (int)y0, (int)tw, (int)(h / 3) + 1, nearer);
                    for (int a = 0; a < (int)(tw / (14 * scale)); ++a)
                        DrawRectangle((int)(x - w / 2 + 4 * scale + a * 14 * scale), (int)(y0 + h * 0.08f), (int)(7 * scale), (int)(h * 0.2f), far);
                }
                break;
            case Shape::OnionTowers: // due torri gemelle con le cupole a cipolla
                for (float s : { -0.25f, 0.25f }) {
                    float tx = x + s * w;
                    DrawRectangle((int)(tx - w * 0.15f), (int)(base - h * 0.75f), (int)(w * 0.3f), (int)(h * 0.75f) + 4, nearer);
                    DrawCircleV({ tx, base - h * 0.8f }, w * 0.17f, nearer);
                    Tri({ tx - w * 0.06f, base - h * 0.88f }, { tx + w * 0.06f, base - h * 0.88f }, { tx, base - h }, nearer);
                }
                DrawRectangle((int)(x - w * 0.5f), (int)(base - h * 0.4f), (int)w, (int)(h * 0.4f) + 4, nearer);
                break;
            case Shape::Volcano: { // il Vesuvio: cono troncato con un filo di fumo
                Tri({ x - w / 2, base }, { x + w / 2, base }, { x + w * 0.06f, base - h }, far);
                Tri({ x - w / 2, base }, { x + w * 0.06f, base - h }, { x - w * 0.08f, base - h * 0.95f }, far);
                float t = (float)GetTime();
                for (int k = 0; k < 4; ++k)
                    DrawCircleV({ x + sinf(t * 0.3f + k) * 10 * scale + k * 8 * scale, base - h - k * 14 * scale },
                                (6 + k * 4) * scale, Fade(Mix(far, WHITE, 0.4f), 0.4f - k * 0.08f));
                break;
            }
            case Shape::Liberty: // piedistallo e statua col braccio alzato
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.45f), (int)w, (int)(h * 0.45f) + 4, nearer);
                DrawRectangle((int)(x - w * 0.18f), (int)(base - h * 0.85f), (int)(w * 0.36f), (int)(h * 0.4f), nearer);
                DrawRectangle((int)(x + w * 0.1f), (int)(base - h), (int)(w * 0.08f), (int)(h * 0.2f), nearer);
                if (d.night) DrawCircleV({ x + w * 0.14f, base - h }, 3 * scale, { 255, 220, 120, 255 });
                break;
            case Shape::Campanile: // torre campanaria con la cella e la punta
                DrawRectangle((int)(x - w / 2), (int)(base - h * 0.78f), (int)w, (int)(h * 0.78f) + 4, nearer);
                DrawRectangle((int)(x - w * 0.3f), (int)(base - h * 0.74f), (int)(w * 0.6f), (int)(h * 0.06f), far);
                Tri({ x - w / 2, base - h * 0.78f }, { x + w / 2, base - h * 0.78f }, { x, base - h }, nearer);
                break;
        }
    }
}
