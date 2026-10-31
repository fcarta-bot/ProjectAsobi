#include "Dungeon.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include "rlgl.h"

unsigned TileHash(int x, int y, unsigned seed) {
    unsigned h = seed * 374761393u + (unsigned)x * 668265263u + (unsigned)y * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ---------------------------------------------------------------------------
// Generazione: il labirinto e' caduto dal cielo, quindi non e' tutto squadrato.
// Stanze di forme diverse, corridoi che piegano, anelli, nicchie e brecce nei muri sottili
// ---------------------------------------------------------------------------

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
        roomOf.assign(w * h, -1);
        rooms.clear();

        for (int attempt = 0; attempt < 160 && rooms.size() < 14; ++attempt) {
            // Che stanza: 0-4 rettangolo, 5-6 con "lobi" (forme a L, a T), 7 piazza grande, 8-9 tondeggiante
            int kind = rnd(0, 9);
            Room r{ 0, 0, rnd(6, 11), rnd(5, 9) };
            if (kind == 7) { r.w = rnd(12, 15); r.h = rnd(9, 12); }
            if (kind >= 8) { r.w = rnd(8, 12); r.h = rnd(7, 10); }
            r.x = rnd(2, w - r.w - 3);
            r.y = rnd(2, h - r.h - 3);

            bool overlaps = false;
            for (const Room& o : rooms) {
                if (r.x - 3 < o.x + o.w && r.x + r.w + 3 > o.x && r.y - 3 < o.y + o.h && r.y + r.h + 3 > o.y) {
                    overlaps = true;
                    break;
                }
            }
            if (overlaps) continue;

            int index = (int)rooms.size();
            if (kind >= 8) {
                CarveEllipse(r, index);
                // La parte sicuramente libera (per far nascere nemici e oggetti) e' il rettangolo interno
                Room core{ r.x + r.w / 5, r.y + r.h / 5, r.w - 2 * (r.w / 5), r.h - 2 * (r.h / 5) };
                rooms.push_back(core);
                continue;
            }
            Carve(r.x, r.y, r.w, r.h, index);
            if (kind == 5 || kind == 6) {
                for (int lobe = 0; lobe < kind - 3; ++lobe) {
                    int lw = rnd(3, 5), lh = rnd(3, 5);
                    switch (rnd(0, 3)) {
                        case 0: Carve(r.x - lw + 1, r.y + rnd(0, std::max(0, r.h - lh)), lw, lh, index); break;
                        case 1: Carve(r.x + r.w - 1, r.y + rnd(0, std::max(0, r.h - lh)), lw, lh, index); break;
                        case 2: Carve(r.x + rnd(0, std::max(0, r.w - lw)), r.y - lh + 1, lw, lh, index); break;
                        default: Carve(r.x + rnd(0, std::max(0, r.w - lw)), r.y + r.h - 1, lw, lh, index); break;
                    }
                }
            }
            rooms.push_back(r);
        }
    } while (rooms.size() < 6);

    // Collegamenti: un albero che unisce tutte le stanze (ogni stanza alla piu' vicina gia' collegata)...
    const int n = (int)rooms.size();
    auto dist2 = [&](int a, int b) {
        float dx = rooms[a].Center().x - rooms[b].Center().x, dy = rooms[a].Center().y - rooms[b].Center().y;
        return dx * dx + dy * dy;
    };
    std::vector<bool> linked(n, false);
    std::vector<std::pair<int, int>> edges;
    linked[0] = true;
    for (int added = 1; added < n; ++added) {
        int bestA = -1, bestB = -1;
        float best = 1e18f;
        for (int a = 0; a < n; ++a) {
            if (!linked[a]) continue;
            for (int b = 0; b < n; ++b)
                if (!linked[b] && dist2(a, b) < best) { best = dist2(a, b); bestA = a; bestB = b; }
        }
        linked[bestB] = true;
        edges.push_back({ bestA, bestB });
    }
    // ...piu' qualche anello, cosi' c'e' sempre piu' di una strada per arrivare da qualche parte
    for (int a = 0; a < n; ++a) {
        if (rnd(0, 99) >= 40) continue;
        int bestB = -1;
        float best = 1e18f;
        for (int b = 0; b < n; ++b) {
            if (b == a) continue;
            bool already = std::any_of(edges.begin(), edges.end(), [&](const std::pair<int, int>& e) {
                return (e.first == a && e.second == b) || (e.first == b && e.second == a);
            });
            if (!already && dist2(a, b) < best) { best = dist2(a, b); bestB = b; }
        }
        if (bestB >= 0 && best < (TILE * 26.0f) * (TILE * 26.0f)) edges.push_back({ a, bestB });
    }
    for (const auto& e : edges) WindingCorridor(rooms[e.first], rooms[e.second], rng);

    Erode(rng);
    RemoveUnreachable();

    // L'uscita (un torii) va nella stanza piu' lontana a piedi da quella di partenza
    std::vector<int> d = DistancesFrom(rooms[0].CenterX(), rooms[0].CenterY());
    int farthest = n - 1, best = -1;
    for (int i = 1; i < n; ++i) {
        int v = d[rooms[i].CenterY() * width + rooms[i].CenterX()];
        if (v > best) { best = v; farthest = i; }
    }
    const Room& exitRoom = rooms[farthest];
    tiles[exitRoom.CenterY() * width + exitRoom.CenterX()] = Tile::Exit;
    exitPos = exitRoom.Center();
}

void Dungeon::Carve(int x, int y, int w, int h, int room) {
    for (int ty = y; ty < y + h; ++ty)
        for (int tx = x; tx < x + w; ++tx)
            if (tx >= 1 && ty >= 1 && tx < width - 1 && ty < height - 1) {
                tiles[ty * width + tx] = Tile::Floor;
                if (room >= 0) roomOf[ty * width + tx] = (int8_t)room;
            }
}

// Stanza tondeggiante: un'ellisse un po' irregolare
void Dungeon::CarveEllipse(const Room& r, int room) {
    float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f, rx = r.w / 2.0f, ry = r.h / 2.0f;
    for (int ty = r.y; ty < r.y + r.h; ++ty)
        for (int tx = r.x; tx < r.x + r.w; ++tx) {
            float dx = (tx + 0.5f - cx) / rx, dy = (ty + 0.5f - cy) / ry;
            float wobble = 0.85f + (TileHash(tx, ty, seed) % 30) / 100.0f; // bordi frastagliati
            if (dx * dx + dy * dy <= wobble) Carve(tx, ty, 1, 1, room);
        }
}

// Corridoio largo 2 (a volte 3) che passa per un punto intermedio a caso: piega una o due volte
void Dungeon::WindingCorridor(const Room& a, const Room& b, std::mt19937& rng) {
    auto rnd = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    int x1 = a.CenterX(), y1 = a.CenterY(), x2 = b.CenterX(), y2 = b.CenterY();
    int mx = std::clamp((x1 + x2) / 2 + rnd(-4, 4), 2, width - 4);
    int my = std::clamp((y1 + y2) / 2 + rnd(-4, 4), 2, height - 4);
    const int wide = rnd(0, 3) == 0 ? 3 : 2;
    const int pts[3][2] = { { x1, y1 }, { mx, my }, { x2, y2 } };
    for (int k = 0; k < 2; ++k) {
        int ax = pts[k][0], ay = pts[k][1], bx = pts[k + 1][0], by = pts[k + 1][1];
        bool horizontalFirst = rnd(0, 1) == 0;
        int cornerX = horizontalFirst ? bx : ax, cornerY = horizontalFirst ? ay : by;
        Carve(std::min(ax, bx), cornerY, std::abs(bx - ax) + wide, wide);
        Carve(cornerX, std::min(ay, by), wide, std::abs(by - ay) + wide);
    }
}

// Il labirinto si e' sbriciolato cadendo: angoli smussati, nicchie e muri sottili sfondati
void Dungeon::Erode(std::mt19937& rng) {
    auto chance = [&](int percent) { return (int)(rng() % 100) < percent; };
    auto floorAt = [&](int x, int y) { return At(x, y) != Tile::Wall; };
    std::vector<Tile> next = tiles;
    for (int ty = 2; ty < height - 2; ++ty)
        for (int tx = 2; tx < width - 2; ++tx) {
            if (floorAt(tx, ty)) continue;
            int around = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if ((dx || dy) && floorAt(tx + dx, ty + dy)) ++around;
            bool open4 = floorAt(tx - 1, ty) || floorAt(tx + 1, ty) || floorAt(tx, ty - 1) || floorAt(tx, ty + 1);
            bool thinX = floorAt(tx - 1, ty) && floorAt(tx + 1, ty);
            bool thinY = floorAt(tx, ty - 1) && floorAt(tx, ty + 1);
            if (around >= 6) next[ty * width + tx] = Tile::Floor;                        // spigolo che sporgeva
            else if ((thinX || thinY) && chance(22)) next[ty * width + tx] = Tile::Floor; // breccia
            else if (open4 && around <= 3 && chance(5)) next[ty * width + tx] = Tile::Floor; // nicchia
        }
    tiles = next;
}

// Tiene solo cio' che si raggiunge a piedi dalla partenza (le brecce potrebbero lasciare isolotti)
void Dungeon::RemoveUnreachable() {
    std::vector<int> d = DistancesFrom(rooms[0].CenterX(), rooms[0].CenterY());
    for (int i = 0; i < width * height; ++i)
        if (tiles[i] != Tile::Wall && d[i] < 0) {
            tiles[i] = Tile::Wall;
            roomOf[i] = -1;
        }
}

std::vector<int> Dungeon::DistancesFrom(int sx, int sy) const {
    std::vector<int> d(width * height, -1);
    std::queue<int> q;
    d[sy * width + sx] = 0;
    q.push(sy * width + sx);
    const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (!q.empty()) {
        int i = q.front();
        q.pop();
        int x = i % width, y = i / width;
        for (auto& dir : dirs) {
            int nx = x + dir[0], ny = y + dir[1];
            if (IsSolid(nx, ny) || d[ny * width + nx] >= 0) continue;
            d[ny * width + nx] = d[i] + 1;
            q.push(ny * width + nx);
        }
    }
    return d;
}

Tile Dungeon::At(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return Tile::Wall;
    return tiles[ty * width + tx];
}

bool Dungeon::IsSolidAt(Vector2 p) const {
    return IsSolid((int)std::floor(p.x / TILE), (int)std::floor(p.y / TILE));
}

int Dungeon::RoomAt(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return -1;
    return roomOf[ty * width + tx];
}

bool Dungeon::InRoom(int tx, int ty) const { return RoomAt(tx, ty) >= 0; }

// Da 2.5 a 3.5 caselle, a gradini di mezza casella (le file di blocchi). Il bordo e' sempre alto
float Dungeon::WallHeight(int tx, int ty) const {
    if (tx <= 0 || ty <= 0 || tx >= width - 1 || ty >= height - 1) return 3.5f;
    return 2.5f + (TileHash(tx, ty, seed ^ 0x5bd1e995u) % 3) * 0.5f;
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
    if (d.snow && g != Ground::Open && h % 3 == 0) base = Mix(base, { 235, 238, 242, 255 }, 0.6f); // neve calpestata
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

    // Accanto ai muri: crepe nell'asfalto, erba che spunta, foglie e pozzanghere. La citta' sotto e' viva
    bool nearWall = IsSolid(tx + 1, tz) || IsSolid(tx - 1, tz) || IsSolid(tx, tz + 1) || IsSolid(tx, tz - 1);
    unsigned k = h >> 8;
    if (nearWall && k % 4 == 0) {
        float cx = x0 + 0.15f + (k % 5) * 0.15f, cz = z0 + 0.15f + (k / 5 % 5) * 0.15f;
        Color tuft = d.snow ? Color{ 245, 246, 250, 255 } : Mix(d.foliage, { 80, 120, 60, 255 }, 0.6f);
        GroundRect(cx, cz, cx + 0.18f, cz + 0.05f, 0.006f, ApplyFog(tuft, fog, dist));
        GroundRect(cx + 0.05f, cz - 0.05f, cx + 0.1f, cz + 0.12f, 0.007f, ApplyFog(Shade(tuft, 0.85f), fog, dist));
    } else if (k % 23 == 0) { // pozzanghera che riflette il cielo
        float cx = x0 + 0.2f + (k % 3) * 0.1f, cz = z0 + 0.2f + (k / 3 % 3) * 0.1f;
        GroundRect(cx, cz, cx + 0.4f, cz + 0.3f, 0.003f, ApplyFog(Mix(d.skyTop, d.horizon, 0.5f), fog, dist));
    } else if (k % 11 == 0 && g != Ground::Road) { // una crepa
        GroundRect(x0 + 0.1f, z0 + 0.48f, x1 - 0.3f, z0 + 0.52f, 0.003f, ApplyFog(Shade(base, 0.6f), fog, dist));
    }
}

// ---------------------------------------------------------------------------
// Muri
// ---------------------------------------------------------------------------

// Strumenti per disegnare su una faccia di muro: u va da sinistra (0) a destra (1), v e' l'altezza in caselle
struct WallFace {
    Vector3 a, b;
    int nx, nz;
    float side;
    float dist;
    Color fog;
    Vector3 P(float u, float v, float lift) const {
        return { a.x + (b.x - a.x) * u - nx * lift, v, a.z + (b.z - a.z) * u - nz * lift };
    }
    void Rect(float u0, float v0, float u1, float v1, Color c, float lift = 0.012f) const {
        Quad(P(u0, v0, lift), P(u1, v0, lift), P(u1, v1, lift), P(u0, v1, lift), c);
    }
    // Quadrilatero qualsiasi (per diagonali, archi, tetti)
    void Poly(float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3, Color c, float lift = 0.014f) const {
        Quad(P(u0, v0, lift), P(u1, v1, lift), P(u2, v2, lift), P(u3, v3, lift), c);
    }
    Color F(Color c, float fogScale = 1.0f) const { return ApplyFog(Shade(c, side), fog, dist * fogScale); }
    Color Lit(Color c) const { return ApplyFog(c, fog, dist * 0.45f); } // luci: bucano la foschia
};

static const Color NEON[] = { { 255, 60, 160, 255 }, { 60, 230, 255, 255 }, { 255, 220, 60, 255 }, { 120, 255, 120, 255 } };

// Arco a tutto sesto fatto a gradini (scuro: e' un'apertura)
static void Arch(const WallFace& w, float u0, float u1, float v0, float v1, Color c) {
    float mid = (u0 + u1) / 2, r = (u1 - u0) / 2;
    w.Rect(u0, v0, u1, v1 - r, c, 0.016f);
    for (int k = 0; k < 4; ++k) {
        float a0 = k * PI / 8, a1 = (k + 1) * PI / 8;
        w.Poly(mid - cosf(a0) * r, v1 - r + sinf(a0) * r, mid + cosf(a0) * r, v1 - r + sinf(a0) * r,
               mid + cosf(a1) * r, v1 - r + sinf(a1) * r, mid - cosf(a1) * r, v1 - r + sinf(a1) * r, c, 0.016f);
    }
}

// Finestra rettangolare con cornice; di notte qualcuna e' accesa
static void Window(const WallFace& w, float u0, float v0, float u1, float v1, Color frame, bool lit, Color dark) {
    w.Rect(u0 - 0.03f, v0 - 0.03f, u1 + 0.03f, v1 + 0.03f, w.F(frame), 0.016f);
    w.Rect(u0, v0, u1, v1, lit ? w.Lit({ 255, 214, 140, 255 }) : w.F(dark), 0.02f);
}

static void DrawFacade(const District& d, const WallFace& w, unsigned v) {
    const bool night = d.night;
    auto lit = [&](int bit) { return night && ((v >> bit) & 1); };
    const Color glass = Mix(d.horizon, { 30, 36, 48, 255 }, 0.6f);
    switch (d.facade) {
        case Facade::Machiya: // legno scuro, grata verticale, tenda noren
            w.Rect(0.04f, 0, 0.96f, 2.0f, w.F({ 72, 46, 32, 255 }));
            for (int k = 0; k < 10; ++k) w.Rect(0.08f + k * 0.085f, 0.35f, 0.12f + k * 0.085f, 1.7f, w.F({ 120, 84, 56, 255 }), 0.02f);
            w.Rect(0.0f, 2.0f, 1.0f, 2.12f, w.F({ 48, 48, 54, 255 }), 0.03f);
            w.Rect(0.32f, 1.25f, 0.68f, 1.62f, w.F({ 40, 52, 112, 255 }), 0.03f);
            break;
        case Facade::Temple: { // intonaco bianco tra travi di legno
            Color wood{ 122, 62, 40, 255 };
            w.Rect(0.04f, 0.2f, 0.96f, 2.3f, w.F({ 226, 220, 204, 255 }));
            w.Rect(0.04f, 0.2f, 0.1f, 2.3f, w.F(wood), 0.02f);
            w.Rect(0.9f, 0.2f, 0.96f, 2.3f, w.F(wood), 0.02f);
            w.Rect(0.04f, 2.2f, 0.96f, 2.3f, w.F(wood), 0.02f);
            w.Rect(0.04f, 0.2f, 0.96f, 0.3f, w.F(wood), 0.02f);
            break;
        }
        case Facade::Neon:
        case Facade::Casino: {
            int kind = v % 3;
            if (d.facade == Facade::Casino && kind == 0) { // insegna del casino' con le lampadine
                Color sign = NEON[(v >> 3) % 4];
                w.Rect(0.04f, 0.9f, 0.96f, 2.2f, w.F({ 40, 20, 50, 255 }));
                w.Rect(0.1f, 1.2f, 0.9f, 1.9f, night ? w.Lit(sign) : w.F(sign), 0.02f);
                for (int k = 0; k < 9; ++k) {
                    bool on = night && ((k + (int)(GetTime() * 4)) % 2);
                    Color bulb = on ? w.Lit({ 255, 240, 180, 255 }) : w.F({ 160, 150, 120, 255 });
                    w.Rect(0.08f + k * 0.1f, 2.05f, 0.12f + k * 0.1f, 2.11f, bulb, 0.025f);
                    w.Rect(0.08f + k * 0.1f, 0.98f, 0.12f + k * 0.1f, 1.04f, bulb, 0.025f);
                }
                break;
            }
            if (kind == 0) { // facciata con finestre
                w.Rect(0.03f, 0, 0.97f, 2.8f, w.F(Mix(d.stone, { 190, 180, 160, 255 }, 0.5f)));
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 2; ++c)
                        w.Rect(0.12f + c * 0.42f, 0.35f + r * 0.6f, 0.46f + c * 0.42f, 0.75f + r * 0.6f,
                               lit(r * 2 + c + 2) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass), 0.02f);
            } else if (kind == 1) { // negozio con la serranda abbassata e l'insegna sopra
                w.Rect(0.06f, 0, 0.94f, 1.1f, w.F({ 150, 150, 156, 255 }));
                for (int k = 1; k < 10; ++k) w.Rect(0.06f, k * 0.11f, 0.94f, k * 0.11f + 0.02f, w.F({ 110, 110, 116, 255 }), 0.02f);
                Color sign = NEON[(v >> 3) % 4];
                w.Rect(0.04f, 1.2f, 0.96f, 1.55f, night ? w.Lit(sign) : w.F(sign), 0.02f);
            } else { // insegna al neon verticale
                Color glow = NEON[(v >> 3) % 4];
                w.Rect(0.3f, 0.9f, 0.7f, 2.8f, w.F({ 22, 20, 28, 255 }));
                for (int k = 0; k < 4; ++k)
                    w.Rect(0.38f, 1.05f + k * 0.44f, 0.62f, 1.37f + k * 0.44f, night ? w.Lit(glow) : w.F(glow), 0.025f);
            }
            break;
        }
        case Facade::Manhattan: {
            if (v % 3 == 0) { // cartellone luminoso di Times Square
                Color a = NEON[(v >> 3) % 4], b = NEON[(v >> 5) % 4];
                w.Rect(0.02f, 1.0f, 0.98f, 2.7f, w.F({ 20, 20, 24, 255 }));
                w.Rect(0.06f, 1.08f, 0.94f, 2.0f, night ? w.Lit(a) : w.F(a), 0.02f);
                w.Rect(0.06f, 2.06f, 0.6f, 2.62f, night ? w.Lit(b) : w.F(b), 0.02f);
                w.Rect(0.64f, 2.06f, 0.94f, 2.62f, night ? w.Lit({ 250, 250, 250, 255 }) : w.F(WHITE), 0.02f);
                break;
            }
            // Brownstone con la scala antincendio di ferro
            w.Rect(0.02f, 0, 0.98f, 2.9f, w.F({ 128, 76, 60, 255 }));
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 2; ++c)
                    Window(w, 0.14f + c * 0.44f, 0.3f + r * 0.9f, 0.42f + c * 0.44f, 0.85f + r * 0.9f, { 200, 190, 170, 255 }, lit(r * 2 + c), glass);
            Color iron{ 30, 30, 34, 255 };
            for (float h : { 1.15f, 2.05f }) {
                w.Rect(0.05f, h, 0.95f, h + 0.04f, w.F(iron), 0.12f);
                w.Rect(0.05f, h + 0.3f, 0.95f, h + 0.33f, w.F(iron), 0.12f);
                for (int k = 0; k < 8; ++k) w.Rect(0.06f + k * 0.12f, h, 0.08f + k * 0.12f, h + 0.32f, w.F(iron), 0.12f);
            }
            w.Poly(0.6f, 1.15f, 0.66f, 1.15f, 0.92f, 2.05f, 0.86f, 2.05f, w.F(iron), 0.1f); // scaletta
            break;
        }
        case Facade::IronBalcony: { // French Quarter: intonaco pastello, persiane, balcone di ferro battuto
            static const Color pastel[] = { { 232, 168, 170, 255 }, { 240, 214, 140, 255 }, { 150, 196, 210, 255 }, { 170, 210, 160, 255 } };
            w.Rect(0.02f, 0, 0.98f, 2.8f, w.F(pastel[v % 4]));
            Color shutter = (v >> 2) % 2 ? Color{ 40, 90, 60, 255 } : Color{ 60, 60, 110, 255 };
            for (float h : { 0.2f, 1.6f })
                for (float u : { 0.12f, 0.58f }) {
                    w.Rect(u, h, u + 0.3f, h + 0.95f, w.F(shutter), 0.02f);
                    w.Rect(u + 0.145f, h, u + 0.155f, h + 0.95f, w.F(Shade(shutter, 0.6f)), 0.024f);
                }
            Color iron{ 26, 26, 30, 255 };
            w.Rect(0.0f, 1.42f, 1.0f, 1.48f, w.F(iron), 0.22f);
            w.Rect(0.0f, 1.86f, 1.0f, 1.9f, w.F(iron), 0.22f);
            for (int k = 0; k < 14; ++k) w.Rect(0.02f + k * 0.07f, 1.48f, 0.03f + k * 0.07f, 1.86f, w.F(iron), 0.22f);
            w.Rect(0.18f, 1.5f, 0.3f, 1.62f, w.F({ 60, 130, 50, 255 }), 0.24f); // felci appese
            w.Rect(0.7f, 1.5f, 0.82f, 1.6f, w.F({ 60, 130, 50, 255 }), 0.24f);
            break;
        }
        case Facade::Brick: { // mattoni rossi di Chicago con le finestre grandi a telaio d'acciaio
            w.Rect(0.02f, 0, 0.98f, 2.9f, w.F({ 150, 70, 52, 255 }));
            for (int r = 0; r < 14; ++r) w.Rect(0.02f, r * 0.2f, 0.98f, r * 0.2f + 0.02f, w.F({ 190, 170, 150, 255 }), 0.014f);
            for (int r = 0; r < 2; ++r) {
                Window(w, 0.12f, 0.5f + r * 1.2f, 0.88f, 1.3f + r * 1.2f, { 40, 40, 44, 255 }, lit(r), glass);
                for (int k = 1; k < 4; ++k) w.Rect(0.12f + k * 0.19f, 0.5f + r * 1.2f, 0.135f + k * 0.19f, 1.3f + r * 1.2f, w.F({ 40, 40, 44, 255 }), 0.024f);
            }
            break;
        }
        case Facade::Travertine: { // Roma: blocchi di travertino, archi e una lapide SPQR
            Color trav{ 214, 202, 176, 255 };
            w.Rect(0.0f, 0, 1.0f, 2.9f, w.F(trav));
            for (int r = 1; r < 6; ++r) w.Rect(0.0f, r * 0.5f, 1.0f, r * 0.5f + 0.025f, w.F(Shade(trav, 0.8f)), 0.014f);
            if (v % 2) {
                Arch(w, 0.2f, 0.8f, 0.0f, 1.9f, w.F({ 50, 42, 36, 255 }));
                w.Rect(0.12f, 0.0f, 0.2f, 2.3f, w.F(Shade(trav, 1.05f)), 0.03f); // lesene
                w.Rect(0.8f, 0.0f, 0.88f, 2.3f, w.F(Shade(trav, 1.05f)), 0.03f);
            } else {
                w.Rect(0.25f, 1.2f, 0.75f, 1.6f, w.F({ 180, 170, 150, 255 }), 0.02f);
                for (int k = 0; k < 4; ++k) w.Rect(0.3f + k * 0.11f, 1.3f, 0.37f + k * 0.11f, 1.5f, w.F({ 70, 60, 50, 255 }), 0.024f);
            }
            break;
        }
        case Facade::Venetian: { // Venezia: intonaco rosato e finestre gotiche a punta con la cornice bianca
            static const Color plaster[] = { { 196, 110, 90, 255 }, { 220, 170, 130, 255 }, { 210, 190, 160, 255 } };
            w.Rect(0.02f, 0, 0.98f, 2.9f, w.F(plaster[v % 3]));
            for (int k = 0; k < 3; ++k) {
                float u = 0.12f + k * 0.28f;
                w.Rect(u - 0.03f, 1.0f, u + 0.2f, 1.9f, w.F({ 236, 232, 224, 255 }), 0.016f);
                w.Rect(u, 1.03f, u + 0.17f, 1.75f, lit(k) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass), 0.02f);
                w.Poly(u, 1.75f, u + 0.17f, 1.75f, u + 0.085f, 1.95f, u + 0.085f, 1.95f, w.F({ 236, 232, 224, 255 }), 0.02f);
            }
            w.Rect(0.08f, 0.95f, 0.92f, 1.0f, w.F({ 236, 232, 224, 255 }), 0.08f); // poggiolo
            break;
        }
        case Facade::Neapolitan: { // Napoli: intonaco giallo scrostato, persiane verdi e i panni stesi
            Color plaster = (v % 2) ? Color{ 222, 180, 100, 255 } : Color{ 200, 120, 80, 255 };
            w.Rect(0.02f, 0, 0.98f, 2.9f, w.F(plaster));
            w.Rect(0.6f, 0.3f, 0.85f, 0.7f, w.F(Shade(plaster, 0.78f)), 0.014f); // intonaco caduto
            for (float u : { 0.12f, 0.6f }) w.Rect(u, 1.2f, u + 0.28f, 2.0f, w.F({ 50, 110, 70, 255 }), 0.02f);
            w.Rect(0.0f, 2.2f, 1.0f, 2.21f, w.F({ 220, 220, 220, 255 }), 0.3f); // la corda
            static const Color cloth[] = { { 240, 240, 236, 255 }, { 220, 60, 60, 255 }, { 70, 120, 220, 255 }, { 250, 210, 70, 255 } };
            for (int k = 0; k < 4; ++k) {
                float u = 0.06f + k * 0.24f;
                w.Rect(u, 1.92f + (k % 2) * 0.08f, u + 0.16f, 2.2f, w.F(cloth[(v + k) % 4]), 0.3f);
            }
            break;
        }
        case Facade::Florentine: { // Firenze: bugnato di pietra forte e bifora
            Color stone{ 160, 134, 100, 255 };
            w.Rect(0.0f, 0, 1.0f, 2.9f, w.F(stone));
            for (int r = 0; r < 6; ++r)
                for (int c = 0; c < 3; ++c) {
                    float u0 = c * 0.34f + (r % 2) * 0.17f;
                    w.Rect(u0 + 0.02f, r * 0.48f + 0.03f, std::min(1.0f, u0 + 0.32f), r * 0.48f + 0.45f, w.F(Shade(stone, 0.9f + ((v >> (r + c)) & 1) * 0.12f)), 0.016f);
                }
            Arch(w, 0.3f, 0.48f, 1.6f, 2.4f, lit(1) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass));
            Arch(w, 0.52f, 0.7f, 1.6f, 2.4f, lit(2) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass));
            w.Rect(0.08f, 1.1f, 0.12f, 1.3f, w.F({ 40, 36, 32, 255 }), 0.06f); // porta-torcia
            break;
        }
        case Facade::Haussmann: { // Parigi: pietra color crema, balconi di ferro, tetto in ardesia
            w.Rect(0.0f, 0, 1.0f, 2.5f, w.F({ 222, 210, 184, 255 }));
            w.Rect(0.0f, 2.5f, 1.0f, 2.95f, w.F({ 90, 96, 108, 255 })); // mansarda
            w.Rect(0.4f, 2.58f, 0.6f, 2.85f, lit(5) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass), 0.02f);
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 2; ++c)
                    Window(w, 0.12f + c * 0.46f, 0.3f + r * 1.1f, 0.42f + c * 0.46f, 1.1f + r * 1.1f, { 236, 228, 210, 255 }, lit(r * 2 + c), glass);
            Color iron{ 28, 28, 32, 255 };
            for (float h : { 1.4f, 0.32f }) {
                w.Rect(0.05f, h, 0.95f, h + 0.03f, w.F(iron), 0.14f);
                w.Rect(0.05f, h + 0.24f, 0.95f, h + 0.27f, w.F(iron), 0.14f);
                for (int k = 0; k < 10; ++k) w.Rect(0.06f + k * 0.09f, h, 0.07f + k * 0.09f, h + 0.26f, w.F(iron), 0.14f);
            }
            break;
        }
        case Facade::Fachwerk: { // Baviera: casa a graticcio, intonaco bianco e travi scure, fiori ai davanzali
            Color wood{ 82, 50, 32, 255 };
            w.Rect(0.0f, 0, 1.0f, 2.8f, w.F({ 236, 230, 214, 255 }));
            for (float h : { 0.0f, 1.3f, 2.7f }) w.Rect(0.0f, h, 1.0f, h + 0.1f, w.F(wood), 0.02f);
            for (float u : { 0.0f, 0.47f, 0.92f }) w.Rect(u, 0.0f, u + 0.08f, 2.8f, w.F(wood), 0.02f);
            w.Poly(0.08f, 0.1f, 0.14f, 0.1f, 0.47f, 1.3f, 0.41f, 1.3f, w.F(wood), 0.022f);
            w.Poly(0.86f, 0.1f, 0.92f, 0.1f, 0.61f, 1.3f, 0.55f, 1.3f, w.F(wood), 0.022f);
            w.Poly(0.08f, 2.7f, 0.14f, 2.7f, 0.47f, 1.4f, 0.41f, 1.4f, w.F(wood), 0.022f);
            w.Poly(0.86f, 2.7f, 0.92f, 2.7f, 0.61f, 1.4f, 0.55f, 1.4f, w.F(wood), 0.022f);
            Window(w, 0.18f, 1.65f, 0.4f, 2.25f, { 236, 230, 214, 255 }, lit(1), glass);
            Window(w, 0.62f, 1.65f, 0.84f, 2.25f, { 236, 230, 214, 255 }, lit(2), glass);
            for (float u : { 0.16f, 0.6f }) w.Rect(u, 1.55f, u + 0.26f, 1.65f, w.F({ 200, 40, 50, 255 }), 0.06f);
            break;
        }
        case Facade::LondonBrick: { // Londra: mattoni scuri, finestre a ghigliottina bianche, porta nera
            w.Rect(0.0f, 0, 1.0f, 2.9f, w.F({ 96, 60, 50, 255 }));
            for (int r = 0; r < 14; ++r) w.Rect(0.0f, r * 0.2f, 1.0f, r * 0.2f + 0.018f, w.F({ 150, 130, 116, 255 }), 0.014f);
            if (v % 3 == 0) {
                w.Rect(0.32f, 0.0f, 0.68f, 1.3f, w.F({ 20, 20, 24, 255 }), 0.02f);
                Arch(w, 0.32f, 0.68f, 1.3f, 1.55f, lit(3) ? w.Lit({ 255, 214, 140, 255 }) : w.F(glass));
                w.Rect(0.48f, 0.6f, 0.52f, 0.66f, w.F({ 210, 180, 80, 255 }), 0.03f);
            } else {
                for (int c = 0; c < 2; ++c) {
                    Window(w, 0.14f + c * 0.44f, 0.4f, 0.42f + c * 0.44f, 1.25f, { 236, 236, 232, 255 }, lit(c), glass);
                    Window(w, 0.14f + c * 0.44f, 1.65f, 0.42f + c * 0.44f, 2.5f, { 236, 236, 232, 255 }, lit(c + 2), glass);
                    w.Rect(0.14f + c * 0.44f, 0.8f, 0.42f + c * 0.44f, 0.84f, w.F({ 236, 236, 232, 255 }), 0.024f);
                }
            }
            break;
        }
        case Facade::Andalusian: { // Siviglia: muro imbiancato, fascia di azulejos, inferriata e gerani
            w.Rect(0.0f, 0, 1.0f, 2.8f, w.F({ 244, 240, 228, 255 }));
            for (int k = 0; k < 10; ++k)
                for (int r = 0; r < 2; ++r) {
                    Color tile = (k + r) % 2 ? Color{ 40, 80, 170, 255 } : Color{ 240, 190, 50, 255 };
                    w.Rect(k * 0.1f, 0.1f + r * 0.1f, k * 0.1f + 0.1f, 0.2f + r * 0.1f, w.F(tile), 0.016f);
                }
            w.Rect(0.0f, 2.7f, 1.0f, 2.8f, w.F({ 230, 170, 30, 255 }), 0.02f); // fascia ocra
            Window(w, 0.3f, 1.0f, 0.7f, 1.9f, { 230, 170, 30, 255 }, lit(1), glass);
            for (int k = 0; k < 6; ++k) w.Rect(0.32f + k * 0.07f, 1.0f, 0.335f + k * 0.07f, 1.9f, w.F({ 20, 20, 22, 255 }), 0.07f);
            for (int k = 0; k < 3; ++k) {
                float u = 0.1f + k * 0.35f;
                w.Rect(u, 2.2f, u + 0.12f, 2.32f, w.F({ 180, 90, 50, 255 }), 0.06f); // vaso
                w.Rect(u - 0.02f, 2.32f, u + 0.14f, 2.42f, w.F({ 220, 40, 60, 255 }), 0.07f);
            }
            break;
        }
    }
}

// Faccia di un muro del labirinto vista dalla casella (tx, tz); il muro sta in (tx+nx, tz+nz).
// Pietra a blocchi, a volte un pezzo di edificio rimasto incastrato quando il labirinto e' caduto
void Dungeon::DrawWall(const District& d, int tx, int tz, int nx, int nz, float dist) const {
    WallFace w;
    if (nx == 1)       { w.a = { tx + 1.0f, 0, (float)tz };    w.b = { tx + 1.0f, 0, tz + 1.0f }; }
    else if (nx == -1) { w.a = { (float)tx, 0, tz + 1.0f };    w.b = { (float)tx, 0, (float)tz }; }
    else if (nz == 1)  { w.a = { tx + 1.0f, 0, tz + 1.0f };    w.b = { (float)tx, 0, tz + 1.0f }; }
    else               { w.a = { (float)tx, 0, (float)tz };    w.b = { tx + 1.0f, 0, (float)tz }; }
    w.nx = nx;
    w.nz = nz;
    w.side = nx != 0 ? 0.8f : 1.0f; // un lato piu' scuro: gli spigoli si distinguono
    w.dist = dist;
    w.fog = d.horizon;

    // Pietra: file di blocchi sfalsati, con muschio alla base. L'altezza cambia da muro a muro
    const unsigned wallHash = TileHash(tx * 4 + nx + 2, tz * 4 + nz + 2, seed);
    const int rows = (int)(WallHeight(tx + nx, tz + nz) * 2 + 0.5f);
    for (int row = 0; row < rows; ++row) {
        float v0 = row * 0.5f, v1 = v0 + 0.5f;
        float split = (row % 2) ? 0.3f : 0.7f;
        for (int k = 0; k < 2; ++k) {
            float shade = 0.84f + ((wallHash >> (row * 2 + k)) & 3) * 0.05f;
            Color c = Shade(d.stone, shade);
            if (row == 0) c = Mix(c, d.snow ? Color{ 230, 234, 240, 255 } : Color{ 70, 96, 60, 255 }, 0.3f);
            w.Rect(k ? split : 0.0f, v0, k ? 1.0f : split, v1, w.F(c), 0);
        }
        w.Rect(0, v1 - 0.03f, 1, v1, w.F(Shade(d.stone, 0.6f)), 0.004f); // fuga tra le file
    }
    // Il bordo in cima e' sbreccato: un blocco in piu' o in meno
    if (wallHash % 3 == 0) w.Rect(0.15f, rows * 0.5f, 0.55f, rows * 0.5f + 0.22f, w.F(Shade(d.stone, 0.9f)), 0);

    // Edera che scende dall'alto (fuori dalle citta' piu' fitte) e crepe nella pietra
    if (!d.urban && !d.snow && (wallHash >> 9) % 7 == 0) {
        Color ivy = Mix(d.foliage, { 50, 100, 45, 255 }, 0.5f);
        for (int k = 0; k < 5; ++k) {
            float u = 0.1f + k * 0.18f, len = 0.6f + ((wallHash >> (k + 12)) & 3) * 0.4f;
            w.Rect(u, rows * 0.5f - len, u + 0.1f, rows * 0.5f, w.F(Shade(ivy, 0.85f + k % 2 * 0.15f)), 0.01f);
        }
    } else if ((wallHash >> 9) % 5 == 0) {
        w.Poly(0.3f, 0.6f, 0.33f, 0.6f, 0.5f, 1.4f, 0.47f, 1.4f, w.F(Shade(d.stone, 0.45f)), 0.008f);
        w.Poly(0.47f, 1.4f, 0.5f, 1.4f, 0.42f, 2.0f, 0.39f, 2.0f, w.F(Shade(d.stone, 0.45f)), 0.008f);
    }

    // Pezzi di citta' inglobati nel muro
    if ((int)(wallHash % 100) >= d.facadeChance) return;
    DrawFacade(d, w, wallHash / 100);
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
