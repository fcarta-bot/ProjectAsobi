#include "Props.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include "raymath.h"
#include "rlgl.h"

constexpr float STEP = 0.2f; // quota che si supera camminando (macerie, panchine): oltre serve il salto

// ---------------------------------------------------------------------------
// Generazione
// ---------------------------------------------------------------------------

static const Color CAR_COLORS[] = {
    { 232, 232, 228, 255 }, { 172, 176, 182, 255 }, { 36, 37, 42, 255 }, { 168, 32, 36, 255 },
    { 42, 72, 132, 255 },   { 150, 200, 180, 255 }, { 230, 214, 170, 255 },
};
static const Color BIN_COLORS[] = { // i cestini giapponesi dividono i rifiuti per colore
    { 170, 40, 40, 255 },   // combustibili
    { 40, 82, 150, 255 },   // plastica
    { 200, 170, 50, 255 },  // lattine
    { 120, 120, 126, 255 }, // indifferenziato
};
static const Color VENDING_COLORS[] = { { 236, 236, 236, 255 }, { 190, 30, 40, 255 }, { 30, 90, 170, 255 } };
static const Color NEON[] = { { 220, 60, 60, 255 }, { 40, 110, 200, 255 }, { 240, 190, 40, 255 }, { 60, 160, 90, 255 } };

// Le auto del posto: taxi gialli a New York, cab neri a Londra, utilitarie colorate in Italia...
static Color CarColor(const District& d, std::mt19937& rng) {
    unsigned r = rng() % 100;
    switch (d.id) {
        case DistrictId::NewYork: if (r < 45) return { 246, 196, 30, 255 }; break;
        case DistrictId::London:  if (r < 40) return { 24, 24, 28, 255 }; break;
        case DistrictId::Rome: case DistrictId::Naples: case DistrictId::Florence: {
            static const Color fiat[] = { { 210, 220, 200, 255 }, { 120, 170, 200, 255 }, { 200, 60, 50, 255 }, { 240, 230, 200, 255 } };
            if (r < 50) return fiat[r % 4];
            break;
        }
        default: break;
    }
    return CAR_COLORS[rng() % 7];
}

// Riquadro di collisione orientato lungo x o lungo z (dimensioni in caselle)
static Vector2 Half(float lengthX, float widthZ, bool alongX) {
    Vector2 h{ lengthX * TILE / 2, widthZ * TILE / 2 };
    return alongX ? h : Vector2{ h.y, h.x };
}

void CityProps::Generate(const Dungeon& dg, const District& d, Vector2 start) {
    props.clear();
    std::mt19937 rng(dg.Seed() ^ 0x9e3779b9u);
    auto roll = [&](float percent) { return (rng() % 10000) < (unsigned)(percent * 100); };
    auto pick = [&](int n) { return (int)(rng() % n); };

    const int W = dg.Width(), H = dg.Height();
    std::vector<char> used(W * H, 0);
    auto isFree = [&](int tx, int ty) {
        return tx >= 0 && ty >= 0 && tx < W && ty < H && !dg.IsSolid(tx, ty) && !used[ty * W + tx];
    };
    auto center = [](int tx, int ty) { return Vector2{ (tx + 0.5f) * TILE, (ty + 0.5f) * TILE }; };
    const Vector2 exit = dg.ExitPos();
    auto reserved = [&](int tx, int ty) { // spazio libero attorno alla partenza e al torii
        Vector2 c = center(tx, ty);
        return Vector2Distance(c, start) < 3.5f * TILE || Vector2Distance(c, exit) < 3.0f * TILE;
    };
    auto add = [&](Prop p, int tx, int ty) {
        p.seed = (unsigned)rng();
        props.push_back(p);
        used[ty * W + tx] = 1;
        return &props.back();
    };
    const bool urban = d.urban;

    // I mille torii di Fushimi Inari: a cavallo dei corridoi di Kyoto, uno ogni tre caselle
    if (d.id == DistrictId::Kyoto) {
        for (int ty = 1; ty < H - 2; ++ty)
            for (int tx = 1; tx < W - 2; ++tx) {
                if (dg.IsSolid(tx, ty) || dg.InRoom(tx, ty) || reserved(tx, ty)) continue;
                Prop t{ PropType::Torii };
                t.solid = false;
                if (dg.IsSolid(tx, ty - 1) && !dg.IsSolid(tx, ty + 1) && dg.IsSolid(tx, ty + 2) && tx % 3 == 0) {
                    t.pos = { (tx + 0.5f) * TILE, (ty + 1.0f) * TILE };
                    t.alongX = true;
                    props.push_back(t);
                } else if (dg.IsSolid(tx - 1, ty) && !dg.IsSolid(tx + 1, ty) && dg.IsSolid(tx + 2, ty) && ty % 3 == 0) {
                    t.pos = { (tx + 1.0f) * TILE, (ty + 0.5f) * TILE };
                    t.alongX = false;
                    props.push_back(t);
                }
            }
    }

    const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (int ty = 1; ty < H - 1; ++ty) {
        for (int tx = 1; tx < W - 1; ++tx) {
            if (!isFree(tx, ty) || reserved(tx, ty)) continue;
            const Ground g = dg.GroundAt(tx, ty);
            const bool road = g != Ground::Sidewalk && g != Ground::Open;
            const bool corridor = !dg.InRoom(tx, ty);
            const Vector2 c = center(tx, ty);

            // Caselle contro un muro: qui il labirinto ha schiacciato e tagliato la citta'
            int wallDir = -1;
            for (int k = 0; k < 4 && wallDir < 0; ++k)
                if (dg.IsSolid(tx + dirs[k][0], ty + dirs[k][1])) wallDir = k;
            if (wallDir >= 0) {
                int nx = dirs[wallDir][0], nz = dirs[wallDir][1];
                Vector2 n{ (float)nx, (float)nz };
                bool alongX = nx != 0;
                bool facingOut = nx == 1 || nz == 1; // "flipped": il lato +x locale guarda via dal muro

                if (road && roll(13)) {
                    // Mezza auto: il taglio e' contro il muro, l'altra meta' e' sparita nella pietra
                    Prop car{ PropType::CarHalf };
                    car.pos = Vector2Add(c, Vector2Scale(n, TILE * 0.5f - 12.8f));
                    car.half = Half(0.8f, 0.72f, alongX);
                    car.height = 0.5f;
                    car.alongX = alongX;
                    car.flipped = !facingOut;
                    car.flammable = true;
                    car.burning = roll(30);
                    car.color = CarColor(d, rng);
                    add(car, tx, ty);
                    // Se il muro e' sottile, dall'altra parte spunta l'altra meta' (stessa auto)
                    int ox = tx + 2 * nx, oy = ty + 2 * nz;
                    if (dg.IsSolid(tx + nx, ty + nz) && isFree(ox, oy) && !reserved(ox, oy)) {
                        Prop other = car;
                        other.pos = Vector2Add(center(ox, oy), Vector2Scale(n, -(TILE * 0.5f - 12.8f)));
                        other.flipped = !car.flipped;
                        other.burning = false;
                        add(other, ox, oy);
                    }
                    continue;
                }
                // Contro i muri delle citta' fitte: cio' che c'e' di tipico sui marciapiedi del posto
                if (urban && d.country != Country::Japan && roll(6)) {
                    PropType type = PropType::NewsBox;
                    if (d.country == Country::England) type = roll(50) ? PropType::PhoneBox : PropType::PostBox;
                    else if (d.country == Country::France) type = PropType::MorrisColumn;
                    else if (d.country == Country::Italy || d.country == Country::Spain) type = PropType::Scooter;
                    Prop v{ type };
                    v.alongX = alongX;
                    v.flipped = facingOut;
                    v.pos = Vector2Add(c, Vector2Scale(n, TILE * 0.25f));
                    switch (type) {
                        case PropType::PhoneBox:     v.half = { 6.5f, 6.5f }; v.height = 1.3f; break;
                        case PropType::PostBox:      v.half = { 4, 4 }; v.height = 0.6f; break;
                        case PropType::MorrisColumn: v.half = { 7, 7 }; v.height = 1.4f; break;
                        case PropType::Scooter:
                            v.knocked = roll(40);
                            v.half = Half(0.8f, 0.3f, !alongX);
                            v.alongX = !alongX; // parcheggiato lungo il muro
                            v.height = v.knocked ? 0.2f : 0.42f;
                            v.flammable = true;
                            v.color = CAR_COLORS[pick(7)];
                            break;
                        default: v.half = { 5, 5 }; v.height = 0.45f; v.color = NEON[pick(4)]; break;
                    }
                    add(v, tx, ty);
                    continue;
                }
                if (urban && d.country == Country::Japan && roll(7)) {
                    Prop v{ PropType::Vending };
                    v.knocked = roll(30);
                    v.alongX = alongX;
                    v.flipped = facingOut;
                    v.color = VENDING_COLORS[pick(3)];
                    if (v.knocked) { // a terra, sdraiato lontano dal muro
                        v.pos = Vector2Add(c, Vector2Scale(n, TILE * (0.5f - 0.4f)));
                        v.half = Half(0.75f, 0.46f, alongX);
                        v.height = 0.32f;
                    } else {
                        v.pos = Vector2Add(c, Vector2Scale(n, TILE * (0.5f - 0.17f)));
                        v.half = Half(0.32f, 0.46f, alongX);
                        v.height = 0.75f;
                    }
                    add(v, tx, ty);
                    continue;
                }
                if (roll(16)) { // macerie staccate dal labirinto alla base del muro
                    Prop r{ PropType::Rubble };
                    r.pos = Vector2Add(c, Vector2Scale(n, TILE * 0.22f));
                    r.half = { 8, 8 };
                    r.height = 0.18f;
                    r.color = d.stone;
                    add(r, tx, ty);
                    continue;
                }
                if (!urban && !corridor && roll(7)) { // albero mezzo inghiottito, piegato dal muro
                    Prop t{ PropType::Tree };
                    t.pos = Vector2Add(c, Vector2Scale(n, TILE * 0.3f));
                    t.half = { 3, 3 };
                    t.height = 3;
                    t.knocked = true;
                    t.alongX = alongX;
                    t.flipped = facingOut;
                    t.color = d.foliage;
                    add(t, tx, ty);
                    continue;
                }
                if (roll(4)) {
                    Prop l{ PropType::Lamp };
                    l.pos = Vector2Add(c, Vector2Scale(n, TILE * 0.35f));
                    l.half = { 2, 2 };
                    l.height = 2.2f;
                    l.knocked = roll(60); // piegato
                    l.alongX = alongX;
                    l.flipped = facingOut;
                    add(l, tx, ty);
                    continue;
                }
            }

            // Caselle libere: cosa c'era su strade, marciapiedi e prati
            if (road) {
                bool alongX = g == Ground::LineX || g == Ground::CrossX ||
                              (g == Ground::Road && !(dg.GroundAt(tx, ty + 1) == Ground::LineZ || dg.GroundAt(tx, ty - 1) == Ground::LineZ) && pick(2));
                int ox = tx + (alongX ? 1 : 0), oy = ty + (alongX ? 0 : 1);
                if (roll(4) && isFree(ox, oy) && !reserved(ox, oy)) {
                    Prop car{ PropType::Car };
                    car.pos = Vector2Add(c, { alongX ? TILE * 0.5f : 0, alongX ? 0 : TILE * 0.5f });
                    car.half = Half(1.5f, 0.72f, alongX);
                    car.knocked = roll(35); // schiacciata da un pezzo di labirinto
                    car.height = car.knocked ? 0.38f : 0.58f;
                    car.alongX = alongX;
                    car.flipped = pick(2);
                    car.flammable = true;
                    car.burning = roll(car.knocked ? 25 : 8);
                    car.color = CarColor(d, rng);
                    add(car, tx, ty);
                    used[oy * W + ox] = 1;
                    continue;
                }
            }

            // Tabella delle probabilita' per il terreno della casella
            struct Choice { PropType type; float percent; };
            std::vector<Choice> table;
            if (road) {
                table = { { PropType::Cone, 2.5f }, { PropType::Rubble, 1.5f } };
            } else if (g == Ground::Sidewalk) {
                table = { { PropType::Bin, 4 }, { PropType::Lamp, 3 }, { PropType::Bench, 2.5f }, { PropType::Cone, 1.5f } };
                if (!urban) table.push_back({ PropType::Tree, 3 });
                if (d.country == Country::USA) table.insert(table.begin(), { PropType::Hydrant, 3 });
                if (d.snow) table.push_back({ PropType::Snowman, 1.5f });
            } else switch (d.id) {
                case DistrictId::Kyoto:
                    table = { { PropType::StoneLantern, 3 }, { PropType::Tree, 4 }, { PropType::FallenTree, 1 }, { PropType::Rubble, 2 } };
                    break;
                case DistrictId::Nara:
                    table = { { PropType::Tree, 6 }, { PropType::Deer, 4 }, { PropType::StoneLantern, 2 },
                              { PropType::FallenTree, 1.5f }, { PropType::Bench, 1.5f } };
                    break;
                case DistrictId::Osaka:
                    table = { { PropType::Yatai, 2.5f }, { PropType::Bin, 3 }, { PropType::Cone, 2 },
                              { PropType::Rubble, 2 }, { PropType::Bench, 1.5f } };
                    break;
                case DistrictId::Tokyo:
                    table = { { PropType::Bin, 3 }, { PropType::Cone, 3 }, { PropType::Rubble, 2.5f },
                              { PropType::Bench, 2 }, { PropType::Tree, 1.5f } };
                    break;
                case DistrictId::NewYork:
                    table = { { PropType::Hydrant, 2.5f }, { PropType::NewsBox, 2 }, { PropType::Yatai, 1.5f },
                              { PropType::Bin, 3 }, { PropType::Cone, 2.5f }, { PropType::Rubble, 2 } };
                    break;
                case DistrictId::NewOrleans:
                    table = { { PropType::Tree, 5 }, { PropType::Bench, 2 }, { PropType::Lamp, 2 },
                              { PropType::FallenTree, 1.5f }, { PropType::Rubble, 1.5f } };
                    break;
                case DistrictId::Chicago:
                    table = { { PropType::Tree, 6 }, { PropType::FallenTree, 1.5f }, { PropType::Bench, 3 },
                              { PropType::Hydrant, 1.5f }, { PropType::Rubble, 1 } };
                    break;
                case DistrictId::LasVegas:
                    table = { { PropType::Tree, 3 }, { PropType::Rubble, 2.5f }, { PropType::Cone, 2 },
                              { PropType::NewsBox, 1.5f }, { PropType::Vending, 1 } };
                    break;
                case DistrictId::Rome:
                    table = { { PropType::Tree, 3.5f }, { PropType::Column, 2.5f }, { PropType::Fountain, 1.2f },
                              { PropType::Scooter, 2.5f }, { PropType::Rubble, 2.5f }, { PropType::CafeTable, 1 } };
                    break;
                case DistrictId::Venice:
                    table = { { PropType::CafeTable, 3 }, { PropType::Fountain, 1.5f }, { PropType::Bench, 1.5f },
                              { PropType::Rubble, 1.5f }, { PropType::Tree, 1 } };
                    break;
                case DistrictId::Naples:
                    table = { { PropType::Scooter, 5 }, { PropType::Bin, 3 }, { PropType::Rubble, 2 },
                              { PropType::Yatai, 1 }, { PropType::Cone, 1 } };
                    break;
                case DistrictId::Florence:
                    table = { { PropType::CafeTable, 2 }, { PropType::Statue, 1.5f }, { PropType::Scooter, 2 },
                              { PropType::Bench, 1.5f }, { PropType::Tree, 1.5f }, { PropType::Rubble, 1.5f } };
                    break;
                case DistrictId::Paris:
                    table = { { PropType::CafeTable, 4 }, { PropType::MorrisColumn, 1 }, { PropType::Tree, 2.5f },
                              { PropType::Bench, 2 }, { PropType::Lamp, 1.5f } };
                    break;
                case DistrictId::Munich:
                    table = { { PropType::Tree, 4 }, { PropType::Bench, 3 }, { PropType::Snowman, 1.5f },
                              { PropType::Yatai, 1 }, { PropType::FallenTree, 1 } };
                    break;
                case DistrictId::London:
                    table = { { PropType::PhoneBox, 1.2f }, { PropType::PostBox, 1.5f }, { PropType::Bench, 2 },
                              { PropType::Tree, 2.5f }, { PropType::Lamp, 2 }, { PropType::Bin, 1.5f } };
                    break;
                case DistrictId::Seville:
                    table = { { PropType::Tree, 5 }, { PropType::Bench, 2 }, { PropType::Scooter, 2 },
                              { PropType::Fountain, 1 }, { PropType::CafeTable, 1.5f } };
                    break;
            }

            for (const Choice& ch : table) {
                if (!roll(ch.percent)) continue;
                // Nei corridoi solo cose basse: si devono poter scavalcare
                if (corridor && (ch.type == PropType::Tree || ch.type == PropType::Yatai ||
                                 ch.type == PropType::StoneLantern || ch.type == PropType::Lamp ||
                                 ch.type == PropType::Deer || ch.type == PropType::PhoneBox ||
                                 ch.type == PropType::Statue || ch.type == PropType::MorrisColumn ||
                                 ch.type == PropType::Fountain)) break;
                Prop p{ ch.type };
                p.pos = c;
                p.alongX = pick(2);
                p.flipped = pick(2);
                switch (ch.type) {
                    case PropType::Bin:
                        p.knocked = roll(30);
                        p.half = p.knocked ? Half(0.34f, 0.28f, p.alongX) : Vector2{ 4.5f, 4.5f };
                        p.height = p.knocked ? 0.28f : 0.32f;
                        p.color = BIN_COLORS[pick(4)];
                        p.flammable = p.color.r == 170 || p.color.r == 120; // combustibili e indifferenziato
                        p.burning = p.flammable && roll(10);
                        break;
                    case PropType::Cone:
                        p.solid = false;
                        p.knocked = roll(40);
                        break;
                    case PropType::Lamp:
                        p.half = { 2, 2 };
                        p.height = 2.2f;
                        p.knocked = roll(35);
                        break;
                    case PropType::Bench:
                        p.half = Half(0.62f, 0.24f, p.alongX);
                        p.height = 0.18f;
                        p.flammable = true;
                        break;
                    case PropType::Rubble:
                        p.half = { 8, 8 };
                        p.height = 0.18f;
                        p.color = d.stone;
                        break;
                    case PropType::Tree:
                        p.half = { 3, 3 };
                        p.height = 3;
                        p.color = d.foliage;
                        break;
                    case PropType::FallenTree: {
                        int ax = p.alongX ? 1 : 0, ay = p.alongX ? 0 : 1;
                        if (!isFree(tx - ax, ty - ay) || !isFree(tx + ax, ty + ay)) continue;
                        p.half = Half(2.1f, 0.3f, p.alongX);
                        p.height = 0.26f;
                        p.color = d.foliage;
                        used[(ty - ay) * W + tx - ax] = used[(ty + ay) * W + tx + ax] = 1;
                        break;
                    }
                    case PropType::StoneLantern:
                        p.half = { 5, 5 };
                        p.height = 0.8f;
                        break;
                    case PropType::Deer:
                        p.half = Half(0.5f, 0.2f, p.alongX);
                        p.height = 0.55f;
                        break;
                    case PropType::Yatai:
                        p.half = Half(0.9f, 0.56f, p.alongX);
                        p.height = 1.05f;
                        p.flammable = true;
                        p.burning = roll(15);
                        break;
                    case PropType::Hydrant:  p.half = { 3.5f, 3.5f }; p.height = 0.3f; break;
                    case PropType::NewsBox:  p.half = { 5, 5 }; p.height = 0.45f; p.color = NEON[pick(4)]; break;
                    case PropType::Vending:  p.half = Half(0.32f, 0.46f, p.alongX); p.height = 0.75f; p.color = VENDING_COLORS[pick(3)]; break;
                    case PropType::Scooter:
                        p.knocked = roll(40);
                        p.half = Half(0.8f, 0.3f, p.alongX);
                        p.height = p.knocked ? 0.2f : 0.42f;
                        p.flammable = true;
                        p.burning = roll(6);
                        p.color = CAR_COLORS[pick(7)];
                        break;
                    case PropType::CafeTable:
                        p.knocked = roll(30);
                        p.half = { 9, 9 };
                        p.height = 0.3f;
                        p.flammable = true;
                        p.color = NEON[pick(4)];
                        break;
                    case PropType::PhoneBox:     p.half = { 6.5f, 6.5f }; p.height = 1.3f; break;
                    case PropType::PostBox:      p.half = { 4, 4 }; p.height = 0.6f; break;
                    case PropType::MorrisColumn: p.half = { 7, 7 }; p.height = 1.4f; break;
                    case PropType::Statue:       p.half = { 7, 7 }; p.height = 1.6f; break;
                    case PropType::Snowman:      p.half = { 6, 6 }; p.height = 0.6f; break;
                    case PropType::Fountain:     p.half = { 6, 6 }; p.height = 0.5f; break;
                    case PropType::Column: {
                        int ax = p.alongX ? 1 : 0, ay = p.alongX ? 0 : 1;
                        if (!isFree(tx - ax, ty - ay) || !isFree(tx + ax, ty + ay)) continue;
                        p.half = Half(2.4f, 0.4f, p.alongX);
                        p.height = 0.36f; // si scavalca con un salto
                        used[(ty - ay) * W + tx - ax] = used[(ty + ay) * W + tx + ax] = 1;
                        break;
                    }
                    default: break;
                }
                add(p, tx, ty);
                break;
            }
        }
    }
}

void CityProps::Update(float dt) {
    for (Prop& p : props) {
        if (!p.burning || p.fireTime < 0) continue;
        p.fireTime -= dt;
        if (p.fireTime <= 0) {
            p.burning = false;
            p.charred = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Collisioni e fuoco
// ---------------------------------------------------------------------------

static Rectangle Box(const Prop& p) {
    return { p.pos.x - p.half.x, p.pos.y - p.half.y, p.half.x * 2, p.half.y * 2 };
}

// Distanza tra un punto e il riquadro dell'oggetto (0 se ci sta dentro)
static float DistanceToBox(const Prop& p, Vector2 q) {
    float dx = std::max(std::fabs(q.x - p.pos.x) - p.half.x, 0.0f);
    float dy = std::max(std::fabs(q.y - p.pos.y) - p.half.y, 0.0f);
    return std::sqrt(dx * dx + dy * dy);
}

bool CityProps::BlocksCircle(Vector2 p, float r, float z) const {
    for (const Prop& o : props)
        if (o.solid && o.height > z + STEP && CheckCollisionCircleRec(p, r, Box(o))) return true;
    return false;
}

float CityProps::GroundHeight(Vector2 p, float r, float z) const {
    float best = 0;
    for (const Prop& o : props)
        if (o.solid && o.height <= z + STEP && o.height > best && CheckCollisionCircleRec(p, r * 0.6f, Box(o)))
            best = o.height;
    return best;
}

bool CityProps::BlocksShot(Vector2 p, float height) const {
    for (const Prop& o : props)
        if (o.solid && o.height > height && CheckCollisionPointRec(p, Box(o))) return true;
    return false;
}

bool CityProps::Overlaps(Vector2 p, float r) const {
    for (const Prop& o : props)
        if (o.solid && CheckCollisionCircleRec(p, r, Box(o))) return true;
    return false;
}

int CityProps::Ignite(Vector2 p, float radius) {
    int count = 0;
    for (Prop& o : props) {
        if (!o.flammable || o.burning || o.charred || DistanceToBox(o, p) > radius) continue;
        o.burning = true;
        o.fireTime = 25.0f;
        ++count;
    }
    return count;
}

bool CityProps::NearFire(Vector2 p, float r, float z) const {
    for (const Prop& o : props)
        if (o.burning && DistanceToBox(o, p) < r + 6 && z < o.height + 0.4f) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------------

namespace {

// Applica foschia e annerimento (bruciato) ai colori di un oggetto
struct Paint {
    Color fog;
    float dist;
    float soot; // 0 = pulito, 1 = carbonizzato
    Color operator()(Color c) const { return ApplyFog(Mix(c, { 28, 26, 24, 255 }, soot), fog, dist); }
    Color Glow(Color c) const { return ApplyFog(c, fog, dist * 0.5f); } // luci: bucano la foschia
};

void DrawCar(const Prop& p, const Paint& f, bool night) {
    const float W = 0.72f;
    const Color body = f(p.color), glass = f({ 42, 52, 64, 255 }), wheel = f({ 22, 22, 24, 255 });
    const Color tail = night ? f.Glow({ 230, 40, 40, 255 }) : f({ 170, 30, 30, 255 });
    if (p.type == PropType::Car) {
        const float L = 1.5f;
        if (p.knocked) rlRotatef(5, 1, 0, 0); // schiacciata: storta e col tetto sfondato
        float cabinH = p.knocked ? 0.06f : 0.2f;
        DrawCube({ 0, 0.24f, 0 }, L, 0.26f, W, body);
        DrawCube({ -0.08f, 0.37f + cabinH / 2, 0 }, 0.78f, cabinH, W * 0.9f, f(Shade(p.color, 0.92f)));
        if (!p.knocked) DrawCube({ -0.08f, 0.37f + cabinH * 0.5f, 0 }, 0.8f, cabinH * 0.6f, W * 0.92f, glass);
        for (float x : { -0.48f, 0.48f })
            DrawCylinderEx({ x, 0.11f, -W / 2 - 0.01f }, { x, 0.11f, W / 2 + 0.01f }, 0.11f, 0.11f, 8, wheel);
        for (float z : { -0.24f, 0.24f }) {
            DrawCube({ L / 2, 0.28f, z }, 0.02f, 0.06f, 0.12f, night ? f.Glow({ 255, 240, 200, 255 }) : f({ 220, 220, 200, 255 }));
            DrawCube({ -L / 2, 0.28f, z }, 0.02f, 0.06f, 0.12f, tail);
        }
        if (p.knocked) DrawCube({ -0.08f, 0.44f, 0 }, 0.5f, 0.1f, 0.4f, f(Shade(p.color, 0.6f))); // lamiera accartocciata
    } else {
        // Mezza auto: si vede la parte posteriore, il taglio netto sta a +x contro il muro
        DrawCube({ 0, 0.24f, 0 }, 0.8f, 0.26f, W, body);
        DrawCube({ 0.05f, 0.47f, 0 }, 0.7f, 0.2f, W * 0.9f, f(Shade(p.color, 0.92f)));
        DrawCube({ 0.05f, 0.47f, 0 }, 0.72f, 0.12f, W * 0.92f, glass);
        DrawCylinderEx({ -0.15f, 0.11f, -W / 2 - 0.01f }, { -0.15f, 0.11f, W / 2 + 0.01f }, 0.11f, 0.11f, 8, wheel);
        for (float z : { -0.24f, 0.24f }) DrawCube({ -0.4f, 0.28f, z }, 0.02f, 0.06f, 0.12f, tail);
        // Sezione del taglio: lamiera scura, sedili e motore a vista
        DrawCube({ 0.39f, 0.33f, 0 }, 0.02f, 0.44f, W * 0.96f, f({ 44, 42, 40, 255 }));
        DrawCube({ 0.37f, 0.36f, -0.15f }, 0.04f, 0.16f, 0.22f, f({ 120, 40, 40, 255 }));
        DrawCube({ 0.37f, 0.22f, 0.12f }, 0.04f, 0.12f, 0.2f, f({ 110, 110, 116, 255 }));
    }
}

// Ogni citta' ha i suoi alberi: pini a ombrello a Roma, palme a Las Vegas, aranci a Siviglia...
void DrawTree(const Prop& p, const Paint& f, TreeStyle style) {
    if (p.knocked) rlRotatef(-14, 0, 0, 1); // piegato via dal muro
    const Color bark = f({ 92, 64, 44, 255 });
    float tint = 0.9f + (p.seed % 20) / 100.0f;
    Color leaves = f(Shade(p.color, tint)), dark = f(Shade(p.color, tint * 0.8f));
    switch (style) {
        case TreeStyle::UmbrellaPine: // tronco alto e storto, chioma piatta a ombrello
            DrawCylinderEx({ 0, 0, 0 }, { 0.15f, 1.9f, 0 }, 0.09f, 0.06f, 7, bark);
            DrawCylinder({ 0.15f, 1.9f, 0 }, 0.85f, 0.7f, 0.32f, 10, leaves);
            DrawCylinder({ 0.15f, 2.2f, 0 }, 0.6f, 0.2f, 0.16f, 10, dark);
            return;
        case TreeStyle::Palm: { // tronco sottile ad anelli e foglie che ricadono
            DrawCylinderEx({ 0, 0, 0 }, { 0.12f, 2.4f, 0 }, 0.07f, 0.05f, 6, f({ 140, 110, 80, 255 }));
            for (int k = 0; k < 7; ++k) {
                float a = k * 2 * PI / 7;
                Vector3 top{ 0.12f, 2.4f, 0 }, tip{ 0.12f + cosf(a) * 0.8f, 2.0f, sinf(a) * 0.8f };
                DrawCylinderEx(top, tip, 0.08f, 0.01f, 4, k % 2 ? leaves : dark);
            }
            return;
        }
        case TreeStyle::Cypress: // il cipresso toscano: un fuso scuro
            DrawCylinder({ 0, 0, 0 }, 0.05f, 0.06f, 0.4f, 6, bark);
            DrawCylinder({ 0, 0.3f, 0 }, 0.28f, 0.02f, 2.6f, 8, dark);
            DrawCylinder({ 0, 0.3f, 0 }, 0.3f, 0.26f, 0.9f, 8, leaves);
            return;
        case TreeStyle::Conifer: { // abete con la neve sopra
            DrawCylinder({ 0, 0, 0 }, 0.07f, 0.08f, 0.5f, 6, bark);
            for (int k = 0; k < 3; ++k)
                DrawCylinder({ 0, 0.45f + k * 0.65f, 0 }, 0.0f, 0.7f - k * 0.18f, 0.9f, 8, k % 2 ? dark : leaves);
            DrawCylinder({ 0, 0.95f, 0 }, 0.0f, 0.5f, 0.3f, 8, f({ 240, 242, 246, 255 }));
            return;
        }
        default: break;
    }
    DrawCylinder({ 0, 0, 0 }, 0.07f, 0.1f, 1.3f, 7, bark);
    DrawSphereEx({ 0, 1.5f, 0 }, 0.55f, 7, 7, leaves);
    DrawSphereEx({ 0.28f, 1.22f, 0.12f }, 0.38f, 6, 6, dark);
    DrawSphereEx({ -0.22f, 1.28f, -0.16f }, 0.4f, 6, 6, dark);
    if (style == TreeStyle::Moss) // il muschio spagnolo che pende dalle querce della Louisiana
        for (int k = 0; k < 6; ++k) {
            float a = k * 1.05f;
            DrawCylinderEx({ cosf(a) * 0.45f, 1.25f, sinf(a) * 0.45f }, { cosf(a) * 0.5f, 0.75f, sinf(a) * 0.5f },
                           0.06f, 0.01f, 4, f({ 150, 160, 130, 255 }));
        }
    if (style == TreeStyle::Orange) // arance tra le foglie
        for (int k = 0; k < 7; ++k) {
            unsigned h = TileHash((int)p.seed, k, 41);
            float a = (h % 628) / 100.0f, y = 1.2f + (h >> 10) % 50 / 100.0f;
            DrawSphereEx({ cosf(a) * 0.48f, y, sinf(a) * 0.48f }, 0.06f, 4, 4, f({ 250, 150, 30, 255 }));
        }
}

void DrawHydrant(const Paint& f) {
    Color red = f({ 200, 40, 36, 255 });
    DrawCylinder({ 0, 0, 0 }, 0.08f, 0.08f, 0.24f, 8, red);
    DrawSphereEx({ 0, 0.25f, 0 }, 0.08f, 6, 6, red);
    DrawCylinderEx({ -0.12f, 0.15f, 0 }, { 0.12f, 0.15f, 0 }, 0.035f, 0.035f, 6, f({ 220, 210, 190, 255 }));
}

void DrawNewsBox(const Prop& p, const Paint& f) {
    DrawCube({ 0, 0.26f, 0 }, 0.3f, 0.34f, 0.28f, f(p.color));
    DrawCube({ 0.151f, 0.32f, 0 }, 0.005f, 0.14f, 0.2f, f({ 230, 230, 220, 255 }));
    for (float z : { -0.11f, 0.11f }) DrawCube({ 0, 0.05f, z }, 0.26f, 0.1f, 0.02f, f({ 50, 50, 54, 255 }));
}

void DrawScooter(const Prop& p, const Paint& f) {
    if (p.knocked) { rlTranslatef(0, 0.12f, 0); rlRotatef(80, 1, 0, 0); } // caduto su un fianco
    Color body = f(p.color), dark = f({ 26, 26, 28, 255 });
    for (float x : { -0.32f, 0.32f })
        DrawCylinderEx({ x, 0.1f, -0.04f }, { x, 0.1f, 0.04f }, 0.1f, 0.1f, 8, dark);
    DrawCube({ -0.1f, 0.24f, 0 }, 0.4f, 0.16f, 0.22f, body);            // scocca
    DrawCube({ -0.12f, 0.34f, 0 }, 0.28f, 0.05f, 0.18f, dark);          // sella
    DrawCube({ 0.26f, 0.3f, 0 }, 0.06f, 0.36f, 0.18f, body);            // scudo
    DrawCylinderEx({ 0.3f, 0.5f, -0.16f }, { 0.3f, 0.5f, 0.16f }, 0.015f, 0.015f, 4, dark); // manubrio
}

void DrawCafeTable(const Prop& p, const Paint& f) {
    Color metal = f({ 40, 40, 44, 255 }), top = f({ 230, 230, 222, 255 });
    if (p.knocked) { rlTranslatef(0, 0.1f, 0); rlRotatef(85, 1, 0, 0); }
    DrawCylinder({ 0, 0, 0 }, 0.02f, 0.02f, 0.3f, 6, metal);
    DrawCylinder({ 0, 0.3f, 0 }, 0.2f, 0.2f, 0.02f, 10, top);
    for (float s : { -1.0f, 1.0f }) { // due sedie
        Vector3 c{ s * 0.32f, 0, 0 };
        DrawCube({ c.x, 0.2f, 0 }, 0.18f, 0.02f, 0.18f, f(p.color));
        DrawCube({ c.x + s * 0.09f, 0.32f, 0 }, 0.02f, 0.24f, 0.18f, f(p.color));
        DrawCylinder({ c.x, 0, 0 }, 0.012f, 0.012f, 0.2f, 4, metal);
    }
}

void DrawPhoneBox(const Paint& f) {
    Color red = f({ 200, 30, 36, 255 }), glass = f({ 60, 70, 80, 255 });
    DrawCube({ 0, 0.62f, 0 }, 0.38f, 1.24f, 0.38f, red);
    for (float s : { -1.0f, 1.0f }) {
        DrawCube({ s * 0.191f, 0.7f, 0 }, 0.005f, 0.8f, 0.28f, glass);
        DrawCube({ 0, 0.7f, s * 0.191f }, 0.28f, 0.8f, 0.005f, glass);
    }
    DrawCube({ 0, 1.28f, 0 }, 0.4f, 0.08f, 0.4f, red);
    DrawCube({ 0.201f, 1.15f, 0 }, 0.005f, 0.06f, 0.24f, f({ 240, 240, 230, 255 })); // "TELEPHONE"
}

void DrawPostBox(const Paint& f) {
    Color red = f({ 200, 30, 36, 255 });
    DrawCylinder({ 0, 0, 0 }, 0.12f, 0.12f, 0.52f, 10, red);
    DrawSphereEx({ 0, 0.52f, 0 }, 0.12f, 8, 8, f({ 30, 30, 32, 255 }));
    DrawCube({ 0.12f, 0.4f, 0 }, 0.01f, 0.03f, 0.12f, f({ 20, 20, 20, 255 }));
}

void DrawColumn(const Paint& f) { // fusto scanalato a terra, spezzato in due, col capitello
    Color marble = f({ 226, 218, 200, 255 });
    DrawCylinderEx({ -1.15f, 0.18f, 0 }, { 0.0f, 0.18f, 0.05f }, 0.18f, 0.17f, 10, marble);
    DrawCylinderEx({ 0.12f, 0.17f, 0.08f }, { 1.0f, 0.17f, 0.18f }, 0.17f, 0.16f, 10, f({ 214, 206, 190, 255 }));
    DrawCube({ 1.15f, 0.2f, 0.2f }, 0.18f, 0.4f, 0.42f, marble);
}

void DrawFountain(const Paint& f, Country country) {
    if (country == Country::Italy) { // il "nasone": cilindro di ghisa con il becco
        Color iron = f({ 50, 54, 52, 255 });
        DrawCylinder({ 0, 0, 0 }, 0.1f, 0.1f, 0.5f, 8, iron);
        DrawCylinderEx({ 0.08f, 0.32f, 0 }, { 0.2f, 0.28f, 0 }, 0.025f, 0.02f, 5, iron);
        DrawCube({ 0.22f, 0.02f, 0 }, 0.2f, 0.03f, 0.2f, f({ 90, 110, 130, 255 })); // l'acqua sotto
        return;
    }
    Color stone = f({ 190, 180, 160, 255 });
    DrawCylinder({ 0, 0, 0 }, 0.2f, 0.2f, 0.3f, 10, stone);
    DrawCylinder({ 0, 0.3f, 0 }, 0.17f, 0.17f, 0.005f, 10, f({ 90, 120, 150, 255 }));
    DrawCylinder({ 0, 0.3f, 0 }, 0.05f, 0.05f, 0.2f, 6, stone);
}

void DrawStatue(const Paint& f) {
    Color marble = f({ 236, 232, 222, 255 });
    DrawCube({ 0, 0.3f, 0 }, 0.4f, 0.6f, 0.4f, f({ 150, 140, 126, 255 }));
    DrawCylinder({ 0, 0.6f, 0 }, 0.09f, 0.12f, 0.6f, 8, marble);   // corpo
    DrawSphereEx({ 0, 1.3f, 0 }, 0.09f, 7, 7, marble);              // testa
    DrawCylinderEx({ 0, 1.1f, 0.1f }, { 0.05f, 0.8f, 0.2f }, 0.035f, 0.03f, 5, marble); // braccio
    DrawCylinderEx({ 0, 1.1f, -0.1f }, { 0.1f, 1.3f, -0.18f }, 0.035f, 0.03f, 5, marble);
}

void DrawMorrisColumn(const Paint& f) {
    Color green = f({ 40, 80, 60, 255 });
    DrawCylinder({ 0, 0, 0 }, 0.22f, 0.22f, 1.2f, 12, green);
    static const Color posters[] = { { 230, 200, 80, 255 }, { 220, 80, 70, 255 }, { 240, 236, 220, 255 }, { 80, 120, 200, 255 } };
    for (int k = 0; k < 4; ++k) {
        float a = k * PI / 2;
        DrawCube({ cosf(a) * 0.22f, 0.65f, sinf(a) * 0.22f }, 0.06f + fabsf(sinf(a)) * 0.2f, 0.5f, 0.06f + fabsf(cosf(a)) * 0.2f,
                 f(posters[k]));
    }
    DrawCylinder({ 0, 1.2f, 0 }, 0.26f, 0.1f, 0.2f, 12, green);
}

void DrawSnowman(const Paint& f) {
    Color snow = f({ 246, 248, 252, 255 });
    DrawSphereEx({ 0, 0.18f, 0 }, 0.2f, 8, 8, snow);
    DrawSphereEx({ 0, 0.44f, 0 }, 0.14f, 8, 8, snow);
    DrawCylinderEx({ 0.12f, 0.46f, 0 }, { 0.24f, 0.45f, 0 }, 0.02f, 0.0f, 5, f({ 240, 120, 30, 255 })); // carota
    DrawCylinder({ 0, 0.56f, 0 }, 0.1f, 0.1f, 0.1f, 8, f({ 30, 30, 34, 255 }));                         // cappello
}

void DrawFallenTree(const Prop& p, const Paint& f) {
    const Color bark = f({ 92, 64, 44, 255 });
    DrawCylinderEx({ -1.05f, 0.13f, 0 }, { 0.9f, 0.13f, 0 }, 0.13f, 0.1f, 7, bark);
    DrawSphereEx({ -1.08f, 0.16f, 0 }, 0.2f, 6, 6, f({ 70, 50, 36, 255 })); // radici
    DrawSphereEx({ 1.2f, 0.32f, 0 }, 0.45f, 6, 6, f(p.color));
    DrawSphereEx({ 1.0f, 0.26f, 0.32f }, 0.32f, 6, 6, f(Shade(p.color, 0.82f)));
}

void DrawBin(const Prop& p, const Paint& f) {
    Color c = f(p.color), lid = f(Shade(p.color, 0.7f));
    if (p.knocked) {
        rlTranslatef(-0.06f, 0.14f, 0);
        rlRotatef(90, 0, 0, 1); // rovesciato su un fianco, rifiuti sparsi davanti
        DrawCylinder({ 0, -0.16f, 0 }, 0.13f, 0.14f, 0.32f, 10, c);
        rlRotatef(-90, 0, 0, 1);
        for (int k = 0; k < 4; ++k) {
            unsigned h = TileHash((int)p.seed, k, 3);
            DrawCube({ -0.3f - (h % 20) / 100.0f, -0.12f, ((int)(h >> 5) % 30 - 15) / 100.0f }, 0.07f, 0.04f, 0.06f,
                     f({ (unsigned char)(150 + h % 100), (unsigned char)(150 + (h >> 8) % 100), 140, 255 }));
        }
        return;
    }
    DrawCylinder({ 0, 0, 0 }, 0.13f, 0.14f, 0.32f, 10, c);
    DrawCylinder({ 0, 0.32f, 0 }, 0.15f, 0.15f, 0.03f, 10, lid);
}

void DrawVending(const Prop& p, const Paint& f, bool night) {
    Color body = f(p.color), panel = night ? f.Glow({ 225, 235, 245, 255 }) : f({ 205, 215, 225, 255 });
    static const Color drinks[] = { { 220, 60, 40, 255 }, { 60, 140, 220, 255 }, { 240, 200, 60, 255 }, { 90, 180, 90, 255 } };
    if (p.knocked) { // a terra sulla schiena: il pannello guarda il cielo
        DrawCube({ 0, 0.16f, 0 }, 0.75f, 0.32f, 0.46f, body);
        DrawCube({ 0.08f, 0.321f, 0 }, 0.34f, 0.005f, 0.38f, panel);
        return;
    }
    DrawCube({ 0, 0.375f, 0 }, 0.32f, 0.75f, 0.46f, body);
    DrawCube({ 0.161f, 0.5f, 0 }, 0.005f, 0.34f, 0.38f, panel);
    for (int r = 0; r < 3; ++r)
        for (int k = 0; k < 5; ++k)
            DrawCube({ 0.166f, 0.39f + r * 0.11f, -0.15f + k * 0.075f }, 0.01f, 0.06f, 0.04f, f(drinks[(r + k + p.seed) % 4]));
    DrawCube({ 0.161f, 0.12f, 0 }, 0.006f, 0.1f, 0.3f, f({ 30, 30, 34, 255 }));
}

void DrawCone(const Prop& p, const Paint& f) {
    if (p.knocked) { rlTranslatef(0, 0.08f, 0); rlRotatef(90, 0, 0, 1); rlTranslatef(0, -0.12f, 0); }
    DrawCube({ 0, 0.01f, 0 }, 0.18f, 0.02f, 0.18f, f({ 200, 90, 30, 255 }));
    DrawCylinder({ 0, 0, 0 }, 0.015f, 0.08f, 0.24f, 8, f({ 240, 112, 30, 255 }));
    DrawCylinder({ 0, 0.1f, 0 }, 0.045f, 0.058f, 0.04f, 8, f({ 240, 240, 240, 255 }));
}

void DrawLamp(const Prop& p, const Paint& f, bool night) {
    if (p.knocked) rlRotatef(-24, 0, 0, 1); // piegato dall'urto del labirinto
    Color metal = f({ 82, 86, 92, 255 });
    DrawCylinder({ 0, 0, 0 }, 0.035f, 0.045f, 2.2f, 6, metal);
    DrawCylinderEx({ 0, 2.18f, 0 }, { 0.36f, 2.24f, 0 }, 0.025f, 0.025f, 5, metal);
    DrawCube({ 0.4f, 2.22f, 0 }, 0.2f, 0.06f, 0.12f, f({ 50, 52, 56, 255 }));
    DrawCube({ 0.4f, 2.185f, 0 }, 0.16f, 0.015f, 0.09f, night && !p.knocked ? f.Glow({ 255, 232, 170, 255 }) : f({ 120, 120, 110, 255 }));
}

void DrawBench(const Paint& f) {
    Color wood = f({ 140, 96, 60, 255 }), metal = f({ 50, 50, 54, 255 });
    DrawCube({ 0, 0.17f, 0 }, 0.6f, 0.04f, 0.2f, wood);
    DrawCube({ 0, 0.3f, -0.09f }, 0.6f, 0.16f, 0.03f, wood);
    for (float x : { -0.25f, 0.25f }) DrawCube({ x, 0.085f, 0 }, 0.04f, 0.17f, 0.18f, metal);
}

void DrawRubble(const Prop& p, const Paint& f) {
    for (int k = 0; k < 5; ++k) {
        unsigned h = TileHash((int)p.seed, k, 11);
        float s = 0.12f + (h % 14) / 100.0f;
        rlPushMatrix();
        rlTranslatef(((int)(h >> 4) % 36 - 18) / 100.0f, s * 0.4f, ((int)(h >> 10) % 36 - 18) / 100.0f);
        rlRotatef((float)(h % 90), 0.3f, 1, 0.2f);
        DrawCube({ 0, 0, 0 }, s, s * 0.8f, s * 1.1f, f(Shade(p.color, 0.8f + (h % 30) / 100.0f)));
        rlPopMatrix();
    }
}

void DrawStoneLantern(const Paint& f) {
    Color stone = f({ 150, 148, 140, 255 }), dark = f({ 40, 38, 36, 255 });
    DrawCube({ 0, 0.06f, 0 }, 0.3f, 0.12f, 0.3f, stone);
    DrawCylinder({ 0, 0.12f, 0 }, 0.06f, 0.06f, 0.3f, 8, stone);
    DrawCube({ 0, 0.45f, 0 }, 0.28f, 0.06f, 0.28f, stone);
    DrawCube({ 0, 0.56f, 0 }, 0.2f, 0.16f, 0.2f, stone);
    DrawCube({ 0, 0.56f, 0 }, 0.21f, 0.08f, 0.1f, dark);
    DrawCylinder({ 0, 0.64f, 0 }, 0.02f, 0.22f, 0.12f, 4, stone);
    DrawSphereEx({ 0, 0.78f, 0 }, 0.04f, 4, 4, stone);
}

// Torii a cavallo di un corridoio largo 2: i pilastri stanno ai lati (asse z locale)
void DrawSmallTorii(const Paint& f) {
    Color red = f({ 222, 62, 30, 255 }), black = f({ 24, 18, 18, 255 });
    for (float z : { -0.86f, 0.86f }) {
        DrawCylinder({ 0, 0, z }, 0.06f, 0.07f, 1.25f, 8, red);
        DrawCylinder({ 0, 0, z }, 0.075f, 0.075f, 0.14f, 8, black);
    }
    DrawCube({ 0, 1.33f, 0 }, 0.16f, 0.07f, 2.1f, black);
    DrawCube({ 0, 1.25f, 0 }, 0.13f, 0.09f, 1.95f, red);
    DrawCube({ 0, 1.04f, 0 }, 0.08f, 0.06f, 1.8f, red);
}

void DrawDeer(const Prop& p, const Paint& f) {
    Color fur = f({ 150, 102, 62, 255 }), dark = f({ 90, 60, 40, 255 });
    DrawCube({ 0, 0.38f, 0 }, 0.42f, 0.18f, 0.16f, fur);
    for (float x : { -0.15f, 0.15f })
        for (float z : { -0.05f, 0.05f }) DrawCube({ x, 0.15f, z }, 0.04f, 0.3f, 0.04f, dark);
    // Testa bassa: il famoso inchino dei cervi di Nara (o mangia qualcosa)
    bool bowing = p.seed % 2;
    DrawCube({ 0.22f, bowing ? 0.42f : 0.52f, 0 }, 0.07f, 0.2f, 0.07f, fur);
    DrawCube({ 0.29f, bowing ? 0.36f : 0.63f, 0 }, 0.14f, 0.08f, 0.08f, fur);
    DrawCube({ -0.22f, 0.44f, 0 }, 0.04f, 0.06f, 0.06f, f({ 240, 236, 225, 255 })); // coda bianca
}

void DrawYatai(const Prop& p, const Paint& f, const District& d) {
    const bool night = d.night;
    if (d.country != Country::Japan) { // carretto del posto: hot dog, castagne, wurstel... con la tenda a righe
        Color cart = f({ 200, 200, 196, 255 }), stripe = f(d.accent);
        DrawCube({ 0, 0.35f, 0 }, 0.9f, 0.4f, 0.5f, cart);
        for (float x : { -0.3f, 0.3f }) DrawCylinderEx({ x, 0.12f, -0.27f }, { x, 0.12f, 0.27f }, 0.12f, 0.12f, 8, f({ 30, 30, 32, 255 }));
        DrawCylinder({ 0, 0.55f, 0 }, 0.02f, 0.02f, 0.6f, 4, f({ 60, 60, 64, 255 }));
        for (int k = 0; k < 6; ++k)
            DrawCylinder({ 0, 1.12f, 0 }, k % 2 ? 0.55f - k * 0.08f : 0.6f - k * 0.08f, 0.0f, 0.18f, 6, k % 2 ? stripe : f(WHITE));
        return;
    }
    Color wood = f({ 132, 92, 56, 255 }), cloth = f({ 172, 40, 40, 255 });
    DrawCube({ 0, 0.27f, 0 }, 0.9f, 0.46f, 0.55f, wood);
    DrawCube({ 0, 0.52f, 0 }, 0.96f, 0.04f, 0.6f, f({ 170, 130, 90, 255 }));
    for (float x : { -0.42f, 0.42f })
        for (float z : { -0.25f, 0.25f }) DrawCube({ x, 0.75f, z }, 0.04f, 0.5f, 0.04f, wood);
    DrawCube({ 0, 1.02f, 0 }, 1.05f, 0.05f, 0.7f, cloth);
    DrawCube({ 0, 0.92f, 0.3f }, 0.9f, 0.14f, 0.01f, f({ 40, 52, 112, 255 })); // noren
    for (float x : { -0.32f, 0.32f })                                          // lanterne di carta
        DrawSphereEx({ x, 0.8f, 0.38f }, 0.08f, 6, 6, night ? f.Glow({ 240, 80, 50, 255 }) : f({ 220, 60, 40, 255 }));
    DrawCylinderEx({ -0.3f, 0.12f, -0.3f }, { -0.3f, 0.12f, -0.34f }, 0.12f, 0.12f, 8, f({ 40, 30, 24, 255 }));
    (void)p;
}

} // namespace

void CityProps::Draw(const District& d, Vector3 eye, Vector2 forward) const {
    for (const Prop& p : props) {
        Vector3 c = To3D(p.pos, 0);
        float dx = c.x - eye.x, dz = c.z - eye.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > VIEW_DIST + 1 || dx * forward.x + dz * forward.y < -2.5f) continue;

        Paint f{ d.horizon, dist, p.charred ? 0.8f : p.burning ? 0.35f : 0.0f };
        rlPushMatrix();
        rlTranslatef(c.x, 0, c.z);
        rlRotatef((p.alongX ? 0.0f : -90.0f) + (p.flipped ? 180.0f : 0.0f), 0, 1, 0);
        switch (p.type) {
            case PropType::Car:
            case PropType::CarHalf:      DrawCar(p, f, d.night); break;
            case PropType::Tree:         DrawTree(p, f, d.trees); break;
            case PropType::FallenTree:   DrawFallenTree(p, f); break;
            case PropType::Bin:          DrawBin(p, f); break;
            case PropType::Vending:      DrawVending(p, f, d.night); break;
            case PropType::Cone:         DrawCone(p, f); break;
            case PropType::Lamp:         DrawLamp(p, f, d.night); break;
            case PropType::Bench:        DrawBench(f); break;
            case PropType::Rubble:       DrawRubble(p, f); break;
            case PropType::StoneLantern: DrawStoneLantern(f); break;
            case PropType::Torii:        DrawSmallTorii(f); break;
            case PropType::Deer:         DrawDeer(p, f); break;
            case PropType::Yatai:        DrawYatai(p, f, d); break;
            case PropType::Hydrant:      DrawHydrant(f); break;
            case PropType::NewsBox:      DrawNewsBox(p, f); break;
            case PropType::Scooter:      DrawScooter(p, f); break;
            case PropType::CafeTable:    DrawCafeTable(p, f); break;
            case PropType::PhoneBox:     DrawPhoneBox(f); break;
            case PropType::PostBox:      DrawPostBox(f); break;
            case PropType::Column:       DrawColumn(f); break;
            case PropType::Fountain:     DrawFountain(f, d.country); break;
            case PropType::Statue:       DrawStatue(f); break;
            case PropType::MorrisColumn: DrawMorrisColumn(f); break;
            case PropType::Snowman:      DrawSnowman(f); break;
        }
        rlPopMatrix();
    }
}

void CityProps::DrawFires(Vector3 eye) const {
    const float t = (float)GetTime();
    bool any = false;
    for (const Prop& p : props) {
        if (!p.burning) continue;
        Vector3 base = To3D(p.pos, p.height * 0.85f);
        if (Vector3Distance(base, eye) > VIEW_DIST) continue;
        any = true;
        float spreadX = std::min(p.half.x / TILE, 0.5f), spreadZ = std::min(p.half.y / TILE, 0.5f);

        // Lingue di fuoco: coni che tremolano, arancioni fuori e gialli dentro
        for (int i = 0; i < 6; ++i) {
            unsigned h = TileHash((int)p.seed, i, 21);
            Vector3 b{ base.x + ((int)(h % 100) - 50) / 50.0f * spreadX * 0.7f, base.y,
                       base.z + ((int)(h / 100 % 100) - 50) / 50.0f * spreadZ * 0.7f };
            float flick = 0.6f + 0.4f * std::sin(t * 9 + i * 1.7f + (h % 7));
            Vector3 tip{ b.x + std::sin(t * 5 + i) * 0.05f, b.y + 0.25f + 0.25f * flick, b.z + std::cos(t * 4 + i) * 0.05f };
            DrawCylinderEx(b, tip, 0.06f + 0.06f * flick, 0.0f, 6, { 255, 112, 30, 255 });
            DrawCylinderEx(b, Vector3Lerp(b, tip, 0.6f), 0.04f + 0.03f * flick, 0.0f, 6, { 255, 222, 90, 255 });
        }
        // Scintille che salgono
        for (int i = 0; i < 5; ++i) {
            float ph = std::fmod(t * 0.9f + i * 0.2f + (p.seed % 10) * 0.1f, 1.0f);
            DrawSphereEx({ base.x + std::sin(i * 2.3f + t) * 0.2f, base.y + 0.2f + ph * 1.3f, base.z + std::cos(i * 1.9f) * 0.2f },
                         0.02f, 3, 3, Fade({ 255, 170, 60, 255 }, 1 - ph));
        }
    }
    if (!any) return;

    // Fumo: trasparente, quindi senza scrivere la profondita' (non deve "bucare" cio' che sta dietro)
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const Prop& p : props) {
        if (!p.burning) continue;
        Vector3 base = To3D(p.pos, p.height + 0.4f);
        if (Vector3Distance(base, eye) > VIEW_DIST) continue;
        for (int i = 0; i < 4; ++i) {
            float ph = std::fmod(t * 0.25f + i * 0.25f + (p.seed % 10) * 0.07f, 1.0f);
            DrawSphereEx({ base.x + ph * 0.5f, base.y + ph * 2.4f, base.z + ph * 0.2f }, 0.18f + ph * 0.45f, 6, 6,
                         Fade({ 40, 38, 38, 255 }, 0.4f * (1 - ph)));
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
