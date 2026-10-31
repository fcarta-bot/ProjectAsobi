#include "Dungeon.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include "rlgl.h"

const District& GetDistrict(int floor) {
    static const District districts[] = {
        { DistrictId::Kyoto, "Kyoto", "Fushimi Inari al tramonto",
          { 70, 50, 110, 255 }, { 232, 140, 100, 255 },                         // cielo
          { 62, 60, 64, 255 }, { 150, 140, 126, 255 }, { 172, 160, 136, 255 },  // asfalto, marciapiede, ghiaia
          { 92, 88, 94, 255 }, { 196, 64, 40, 255 }, { 230, 70, 40, 255 },      // pietra, aceri rossi, torii
          false, Skyline::Temples, 12 },
        { DistrictId::Nara, "Nara", "Parco dei cervi",
          { 92, 150, 220, 255 }, { 205, 222, 232, 255 },
          { 72, 72, 76, 255 }, { 160, 154, 140, 255 }, { 98, 142, 72, 255 },    // prato
          { 100, 104, 96, 255 }, { 72, 132, 62, 255 }, { 240, 200, 120, 255 },
          false, Skyline::Hills, 8 },
        { DistrictId::Osaka, "Osaka", "Dotonbori di sera",
          { 30, 20, 62, 255 }, { 150, 72, 112, 255 },
          { 50, 48, 56, 255 }, { 112, 106, 112, 255 }, { 126, 112, 102, 255 },  // piazza
          { 84, 80, 92, 255 }, { 64, 116, 72, 255 }, { 255, 200, 40, 255 },
          true, Skyline::Downtown, 26 },
        { DistrictId::Tokyo, "Tokyo", "Shibuya di notte",
          { 8, 10, 26, 255 }, { 40, 46, 82, 255 },
          { 38, 38, 44, 255 }, { 86, 86, 96, 255 }, { 96, 96, 106, 255 },
          { 72, 74, 86, 255 }, { 214, 182, 60, 255 }, { 60, 230, 255, 255 },  // ginkgo gialli
          true, Skyline::Skyscrapers, 28 },
    };
    constexpr int count = sizeof(districts) / sizeof(districts[0]);
    return districts[(std::max(floor, 1) - 1) % count];
}

unsigned TileHash(int x, int y, unsigned seed) {
    unsigned h = seed * 374761393u + (unsigned)x * 668265263u + (unsigned)y * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void Dungeon::Generate(int w, int h, unsigned s) {
    seed = s;
    std::mt19937 rng(seed);
    auto rnd = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    roadOffsetX = rnd(0, 13);
    roadOffsetY = rnd(0, 10);

    // Ripete finche' non ottiene abbastanza stanze
    do {
        width = w;
        height = h;
        tiles.assign(w * h, Tile::Wall);
        rooms.clear();

        for (int attempt = 0; attempt < 80 && rooms.size() < 12; ++attempt) {
            Room r{ 0, 0, rnd(6, 12), rnd(5, 10) };
            r.x = rnd(1, w - r.w - 2);
            r.y = rnd(1, h - r.h - 2);

            bool overlaps = false;
            for (const Room& o : rooms) {
                if (r.x - 2 < o.x + o.w && r.x + r.w + 2 > o.x &&
                    r.y - 2 < o.y + o.h && r.y + r.h + 2 > o.y) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) continue;

            Carve(r.x, r.y, r.w, r.h);
            if (!rooms.empty()) Corridor(rooms.back(), r, rnd(0, 1) == 0);
            rooms.push_back(r);
        }
    } while (rooms.size() < 5);

    // L'uscita (un torii) va nella stanza piu' lontana da quella di partenza
    const Room* farthest = &rooms[1];
    float best = 0;
    for (const Room& r : rooms) {
        float dx = r.Center().x - rooms[0].Center().x;
        float dy = r.Center().y - rooms[0].Center().y;
        float d = dx * dx + dy * dy;
        if (d > best) { best = d; farthest = &r; }
    }
    tiles[farthest->CenterY() * width + farthest->CenterX()] = Tile::Exit;
    exitPos = farthest->Center();
}

void Dungeon::Carve(int x, int y, int w, int h, Tile t) {
    for (int ty = y; ty < y + h; ++ty)
        for (int tx = x; tx < x + w; ++tx)
            if (tx >= 1 && ty >= 1 && tx < width - 1 && ty < height - 1)
                tiles[ty * width + tx] = t;
}

// Corridoio a "L" largo 2 caselle tra i centri di due stanze
void Dungeon::Corridor(const Room& a, const Room& b, bool horizontalFirst) {
    int x1 = a.CenterX(), y1 = a.CenterY();
    int x2 = b.CenterX(), y2 = b.CenterY();
    int cornerX = horizontalFirst ? x2 : x1;
    int cornerY = horizontalFirst ? y1 : y2;
    Carve(std::min(x1, x2), cornerY, std::abs(x2 - x1) + 2, 2);
    Carve(cornerX, std::min(y1, y2), 2, std::abs(y2 - y1) + 2);
}

Tile Dungeon::At(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return Tile::Wall;
    return tiles[ty * width + tx];
}

bool Dungeon::IsSolidAt(Vector2 p) const {
    return IsSolid((int)std::floor(p.x / TILE), (int)std::floor(p.y / TILE));
}

bool Dungeon::InRoom(int tx, int ty) const {
    for (const Room& r : rooms)
        if (tx >= r.x && tx < r.x + r.w && ty >= r.y && ty < r.y + r.h) return true;
    return false;
}

static int Wrap(int v, int n) { return (v % n + n) % n; }

// La citta' sotto il labirinto ha un reticolo di strade tutto suo: i muri la tagliano a caso
Ground Dungeon::GroundAt(int tx, int ty) const {
    int gx = Wrap(tx + roadOffsetX, 14), gy = Wrap(ty + roadOffsetY, 11);
    bool roadX = gy >= 4 && gy <= 6; // strada di 3 caselle che corre lungo x
    bool roadZ = gx >= 6 && gx <= 8;
    if (roadX && roadZ) return Ground::Road; // incrocio
    if (roadX) {
        if (gx == 5 || gx == 9) return Ground::CrossX; // strisce prima dell'incrocio
        return gy == 5 ? Ground::LineX : Ground::Road;
    }
    if (roadZ) {
        if (gy == 3 || gy == 7) return Ground::CrossZ;
        return gx == 7 ? Ground::LineZ : Ground::Road;
    }
    if (gy == 3 || gy == 7 || gx == 5 || gx == 9) return Ground::Sidewalk;
    return Ground::Open;
}

// Controlla la linea a piccoli passi: basta e avanza per caselle da 32 pixel
bool Dungeon::HasLineOfSight(Vector2 a, Vector2 b) const {
    float dx = b.x - a.x, dy = b.y - a.y;
    int steps = (int)(std::sqrt(dx * dx + dy * dy) / (TILE / 4.0f)) + 1;
    for (int i = 1; i < steps; ++i) {
        float t = (float)i / steps;
        if (IsSolidAt({ a.x + dx * t, a.y + dy * t })) return false;
    }
    return true;
}

Color Mix(Color a, Color b, float t) {
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), 255 };
}

Color Shade(Color c, float k) {
    auto ch = [k](unsigned char v) { return (unsigned char)std::min(255.0f, v * k); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

Color ApplyFog(Color c, Color fog, float dist) {
    float t = std::clamp(dist / VIEW_DIST, 0.0f, 1.0f);
    return Mix(c, fog, t * t * (3 - 2 * t)); // vicino quasi nitido, poi sfuma in fretta
}

static void Quad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color col) {
    DrawTriangle3D(a, b, c, col);
    DrawTriangle3D(a, c, d, col);
}

// Rettangolo orizzontale appena sopra il suolo (per linee e strisce)
static void GroundRect(float x0, float z0, float x1, float z1, float y, Color col) {
    Quad({ x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 }, col);
}

void Dungeon::DrawGround(const District& d, int tx, int tz, float dist) const {
    const Color fog = d.horizon;
    const unsigned h = TileHash(tx, tz, seed);
    const float x0 = (float)tx, x1 = tx + 1.0f, z0 = (float)tz, z1 = tz + 1.0f;
    const float jitter = 0.94f + (h % 12) / 100.0f; // ogni casella un po' diversa
    const Color paint = d.night ? Color{ 200, 200, 190, 255 } : Color{ 235, 232, 220, 255 };

    Ground g = GroundAt(tx, tz);
    Color base;
    switch (g) {
        case Ground::Sidewalk: base = Shade(d.sidewalk, ((tx + tz) & 1) ? 1.0f : 0.92f); break;
        case Ground::Open:     base = Shade(d.open, jitter); break;
        default:               base = Shade(d.road, jitter); break;
    }
    if (At(tx, tz) == Tile::Exit) base = Mix(base, d.accent, 0.35f);
    GroundRect(x0, z0, x1, z1, 0, ApplyFog(base, fog, dist));

    // Segnaletica orizzontale
    Color p = ApplyFog(paint, fog, dist);
    switch (g) {
        case Ground::LineX: if (tx % 2 == 0) GroundRect(x0 + 0.1f, z0 + 0.46f, x1 - 0.1f, z0 + 0.54f, 0.004f, p); break;
        case Ground::LineZ: if (tz % 2 == 0) GroundRect(x0 + 0.46f, z0 + 0.1f, x0 + 0.54f, z1 - 0.1f, 0.004f, p); break;
        case Ground::CrossX:
            for (int k = 0; k < 3; ++k) GroundRect(x0 + 0.15f, z0 + 0.08f + k * 0.33f, x1 - 0.15f, z0 + 0.25f + k * 0.33f, 0.004f, p);
            break;
        case Ground::CrossZ:
            for (int k = 0; k < 3; ++k) GroundRect(x0 + 0.08f + k * 0.33f, z0 + 0.15f, x0 + 0.25f + k * 0.33f, z1 - 0.15f, 0.004f, p);
            break;
        case Ground::Open:
            // Ciuffi o sassi sparsi, cosi' il terreno libero non sembra un pavimento piatto
            if (h % 5 == 0) {
                float cx = x0 + 0.2f + (h % 7) * 0.09f, cz = z0 + 0.2f + (h / 7 % 7) * 0.09f;
                GroundRect(cx, cz, cx + 0.12f, cz + 0.08f, 0.004f, ApplyFog(Shade(d.open, 0.75f), fog, dist));
            }
            break;
        default: break;
    }
}

// Faccia di un muro del labirinto vista dalla casella (tx, tz); il muro sta in (tx+nx, tz+nz).
// Pietra a blocchi, e a volte un pezzo di edificio rimasto incastrato quando il labirinto e' caduto
void Dungeon::DrawWall(const District& d, int tx, int tz, int nx, int nz, float dist) const {
    const Color fog = d.horizon;
    // a e b sono gli estremi della faccia: guardandola, u va da sinistra (a) a destra (b)
    Vector3 a, b;
    if (nx == 1)       { a = { tx + 1.0f, 0, (float)tz };    b = { tx + 1.0f, 0, tz + 1.0f }; }
    else if (nx == -1) { a = { (float)tx, 0, tz + 1.0f };    b = { (float)tx, 0, (float)tz }; }
    else if (nz == 1)  { a = { tx + 1.0f, 0, tz + 1.0f };    b = { (float)tx, 0, tz + 1.0f }; }
    else               { a = { (float)tx, 0, (float)tz };    b = { tx + 1.0f, 0, (float)tz }; }
    const float side = nx != 0 ? 0.8f : 1.0f; // un lato piu' scuro: gli spigoli si distinguono
    const float off = 0.012f;                 // le decorazioni stanno un filo davanti alla pietra

    auto P = [&](float u, float v, float lift) {
        return Vector3{ a.x + (b.x - a.x) * u - nx * lift, v, a.z + (b.z - a.z) * u - nz * lift };
    };
    auto Rect = [&](float u0, float v0, float u1, float v1, Color c, float lift = 0.012f) {
        Quad(P(u0, v0, lift), P(u1, v0, lift), P(u1, v1, lift), P(u0, v1, lift), c);
    };
    auto F = [&](Color c, float fogScale = 1.0f) { return ApplyFog(Shade(c, side), fog, dist * fogScale); };

    // Pietra: 6 file di blocchi sfalsati, con muschio alla base
    const unsigned wallHash = TileHash(tx * 4 + nx + 2, tz * 4 + nz + 2, seed);
    for (int row = 0; row < 6; ++row) {
        float v0 = row * 0.5f, v1 = v0 + 0.5f;
        float split = (row % 2) ? 0.3f : 0.7f;
        for (int k = 0; k < 2; ++k) {
            float shade = 0.84f + ((wallHash >> (row * 2 + k)) & 3) * 0.05f;
            Color c = Shade(d.stone, shade);
            if (row == 0) c = Mix(c, { 70, 96, 60, 255 }, 0.3f);
            Rect(k ? split : 0.0f, v0, k ? 1.0f : split, v1, F(c), 0);
        }
        Rect(0, v1 - 0.03f, 1, v1, F(Shade(d.stone, 0.6f)), 0.004f); // fuga tra le file
    }

    // Pezzi di citta' inglobati nel muro
    if ((int)(wallHash % 100) >= d.facadeChance) return;
    const unsigned v = wallHash / 100;
    switch (d.id) {
        case DistrictId::Kyoto: { // facciata di machiya: legno scuro, grata verticale, tenda noren
            Rect(0.04f, 0, 0.96f, 2.0f, F({ 72, 46, 32, 255 }));
            for (int k = 0; k < 10; ++k) Rect(0.08f + k * 0.085f, 0.35f, 0.12f + k * 0.085f, 1.7f, F({ 120, 84, 56, 255 }), 0.02f);
            Rect(0.0f, 2.0f, 1.0f, 2.12f, F({ 48, 48, 54, 255 }), 0.03f);                 // tettoia
            Rect(0.32f, 1.25f, 0.68f, 1.62f, F({ 40, 52, 112, 255 }), 0.03f);             // noren
            break;
        }
        case DistrictId::Nara: { // muro di tempio: intonaco bianco tra travi di legno
            Color wood{ 122, 62, 40, 255 };
            Rect(0.04f, 0.2f, 0.96f, 2.3f, F({ 226, 220, 204, 255 }));
            Rect(0.04f, 0.2f, 0.1f, 2.3f, F(wood), 0.02f);
            Rect(0.9f, 0.2f, 0.96f, 2.3f, F(wood), 0.02f);
            Rect(0.04f, 2.2f, 0.96f, 2.3f, F(wood), 0.02f);
            Rect(0.04f, 0.2f, 0.96f, 0.3f, F(wood), 0.02f);
            break;
        }
        default: {
            static const Color neon[] = { { 255, 60, 160, 255 }, { 60, 230, 255, 255 }, { 255, 220, 60, 255 }, { 120, 255, 120, 255 } };
            int kind = v % 3;
            if (kind == 0) { // facciata con finestre: di notte qualcuna e' ancora accesa
                Rect(0.03f, 0, 0.97f, 2.8f, F(Mix(d.stone, { 190, 180, 160, 255 }, 0.5f)));
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 2; ++c) {
                        bool lit = d.night && ((v >> (r * 2 + c + 2)) & 1);
                        Color glass = lit ? Color{ 255, 214, 140, 255 } : Mix(d.horizon, { 30, 36, 48, 255 }, 0.6f);
                        Rect(0.12f + c * 0.42f, 0.35f + r * 0.6f, 0.46f + c * 0.42f, 0.75f + r * 0.6f,
                             lit ? ApplyFog(glass, fog, dist * 0.5f) : F(glass), 0.02f);
                    }
            } else if (kind == 1) { // negozio con la serranda abbassata e l'insegna sopra
                Rect(0.06f, 0, 0.94f, 1.1f, F({ 150, 150, 156, 255 }));
                for (int k = 1; k < 10; ++k) Rect(0.06f, k * 0.11f, 0.94f, k * 0.11f + 0.02f, F({ 110, 110, 116, 255 }), 0.02f);
                Color sign = neon[(v >> 3) % 4];
                Rect(0.04f, 1.2f, 0.96f, 1.55f, d.night ? ApplyFog(sign, fog, dist * 0.5f) : F(sign), 0.02f);
            } else { // insegna al neon verticale con i suoi "caratteri" luminosi
                Color glow = neon[(v >> 3) % 4];
                Rect(0.3f, 0.9f, 0.7f, 2.8f, F({ 22, 20, 28, 255 }));
                for (int k = 0; k < 4; ++k)
                    Rect(0.38f, 1.05f + k * 0.44f, 0.62f, 1.37f + k * 0.44f,
                         d.night ? ApplyFog(glow, fog, dist * 0.4f) : F(glow), 0.025f);
            }
            break;
        }
    }
}

static void DrawTorii(Vector3 c, Color col) {
    const Color dark{ 20, 10, 10, 255 };
    for (float side : { -0.75f, 0.75f })                                  // pilastri
        DrawCylinder({ c.x + side, 0, c.z }, 0.1f, 0.11f, 1.75f, 10, col);
    DrawCube({ c.x, 1.86f, c.z }, 2.3f, 0.08f, 0.3f, dark);               // kasagi (trave alta)
    DrawCube({ c.x, 1.76f, c.z }, 2.05f, 0.14f, 0.22f, col);
    DrawCube({ c.x, 1.42f, c.z }, 1.7f, 0.1f, 0.14f, col);                // nuki (trave bassa)
    float pulse = 0.35f + 0.1f * std::sin((float)GetTime() * 3.0f);
    DrawCircle3D({ c.x, 0.01f, c.z }, 1.1f, { 1, 0, 0 }, 90, Fade(col, pulse));
}

void Dungeon::Draw3D(const District& d, Vector3 eye, Vector2 forward) const {
    const int r = (int)VIEW_DIST + 1;
    const int cx = (int)std::floor(eye.x), cz = (int)std::floor(eye.z);

    // I triangoli sono a "due facce": non serve badare al verso in cui sono disegnati.
    // raylib disegna a blocchi: va svuotato prima di cambiare il culling, e dopo
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    for (int tz = cz - r; tz <= cz + r; ++tz) {
        for (int tx = cx - r; tx <= cx + r; ++tx) {
            if (IsSolid(tx, tz)) continue;
            float dx = tx + 0.5f - eye.x, dz = tz + 0.5f - eye.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > VIEW_DIST) continue;
            if (dx * forward.x + dz * forward.y < -1.5f) continue; // alle spalle della telecamera

            DrawGround(d, tx, tz, dist);
            // Un muro si vede solo dal lato che confina col suolo: disegniamo solo quelle facce
            if (IsSolid(tx + 1, tz)) DrawWall(d, tx, tz, 1, 0, dist);
            if (IsSolid(tx - 1, tz)) DrawWall(d, tx, tz, -1, 0, dist);
            if (IsSolid(tx, tz + 1)) DrawWall(d, tx, tz, 0, 1, dist);
            if (IsSolid(tx, tz - 1)) DrawWall(d, tx, tz, 0, -1, dist);
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();

    DrawTorii(To3D(exitPos, 0), d.accent);
}
