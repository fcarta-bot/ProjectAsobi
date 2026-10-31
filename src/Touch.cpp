#include "Touch.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "raymath.h"
#include "PadRaw.hpp"

// ---------------------------------------------------------------------------
// Disposizione dei tasti: per spostarli basta cambiare questa tabella.
// dx, dy e r sono in pixel per uno schermo alto 720 e si ingrandiscono con lo schermo;
// dx e dy partono dall'angolo scelto (negativi = verso l'interno dagli angoli a destra / in basso)
// ---------------------------------------------------------------------------
enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
enum class Icon { None, Up, Down, Left, Right, Pause };

struct ButtonSpec {
    TouchButton id;
    TouchControls::Layout layout;
    const char* label;
    Icon icon;
    Corner corner;
    float dx, dy, r;
    bool look; // trascinando il dito sul tasto ci si gira anche (per attaccare e mirare insieme)
};

using L = TouchControls::Layout;
static const ButtonSpec BUTTONS[] = {
    // In partita: azioni attorno al pollice destro, menu in alto a destra
    { TB_ATTACK, L::Game, "ATTACCA", Icon::None,  Corner::BottomRight, -125, -135, 72, true },
    { TB_JUMP,   L::Game, "SALTA",   Icon::None,  Corner::BottomRight, -275,  -75, 48, false },
    { TB_DASH,   L::Game, "SCATTO",  Icon::None,  Corner::BottomRight, -265, -215, 48, false },
    { TB_SWAP,   L::Game, "ARMA",    Icon::None,  Corner::BottomRight, -110, -295, 40, false },
    { TB_USE,    L::Game, "PRENDI",  Icon::None,  Corner::BottomRight, -400,  -70, 42, false },
    { TB_MATCH,  L::Game, "BORSA",   Icon::None,  Corner::TopRight,    -150,   48, 36, false },
    { TB_PAUSE,  L::Game, "",        Icon::Pause, Corner::TopRight,     -60,   48, 36, false },
    // Nei menu: frecce a sinistra, OK e Indietro a destra
    { TB_UP,     L::Menu, "", Icon::Up,    Corner::BottomLeft,  130, -215, 44, false },
    { TB_DOWN,   L::Menu, "", Icon::Down,  Corner::BottomLeft,  130,  -75, 44, false },
    { TB_LEFT,   L::Menu, "", Icon::Left,  Corner::BottomLeft,   60, -145, 44, false },
    { TB_RIGHT,  L::Menu, "", Icon::Right, Corner::BottomLeft,  200, -145, 44, false },
    { TB_OK,     L::Menu, "OK",       Icon::None, Corner::BottomRight, -115, -125, 60, false },
    { TB_BACK,   L::Menu, "INDIETRO", Icon::None, Corner::BottomRight, -265,  -75, 46, false },
    // Nelle animazioni si tocca ovunque per andare avanti; questo salta tutto
    { TB_SKIP,   L::Tap,  "SALTA",    Icon::None, Corner::TopRight,     -70,   56, 40, false },
};

constexpr float STICK_RADIUS = 80;        // quanto spostare il dito per correre al massimo
constexpr float STICK_ZONE_X = 0.45f;     // lo stick si appoggia nella parte sinistra dello schermo...
constexpr float STICK_ZONE_TOP = 0.25f;   // ...sotto questa quota (sopra ci sono vita e armi)
constexpr Vector2 STICK_HOME{ 160, -170 }; // dove si disegna lo stick a riposo (dall'angolo in basso a sinistra)

static float Scale() {
    return std::clamp(std::min(GetScreenWidth(), GetScreenHeight()) / 720.0f, 0.75f, 1.5f);
}

static Vector2 CenterOf(const ButtonSpec& b) {
    const float s = Scale(), sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    const bool right = b.corner == Corner::TopRight || b.corner == Corner::BottomRight;
    const bool bottom = b.corner == Corner::BottomLeft || b.corner == Corner::BottomRight;
    return { (right ? sw : 0) + b.dx * s, (bottom ? sh : 0) + b.dy * s };
}

// Il tasto sotto il dito (con un po' di margine: le dita sono grosse), -1 se nessuno
static int ButtonAt(Vector2 p, L layout) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < (int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])); ++i) {
        if (BUTTONS[i].layout != layout) continue;
        float d = Vector2Distance(p, CenterOf(BUTTONS[i]));
        if (d < BUTTONS[i].r * Scale() * 1.15f && d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

void TouchControls::SetLayout(Layout l) {
    if (l == layout) return;
    layout = l;
    // Le dita gia' appoggiate non premono niente nella nuova schermata finche' non si staccano
    for (Finger& f : fingers) f.role = Role::None;
}

void TouchControls::Update() {
    std::copy(std::begin(down), std::end(down), std::begin(prev));
    std::fill(std::begin(down), std::end(down), false);
    tapped = false;
    move = { 0, 0 };
    lookDx = 0;

    // Allinea l'elenco delle dita con quelle appoggiate adesso
    const int count = RawTouchCount();
    std::vector<Finger> now;
    for (int i = 0; i < count; ++i) {
        RawTouch t = RawTouchAt(i);
        auto old = std::find_if(fingers.begin(), fingers.end(), [&](const Finger& f) { return f.id == t.id; });
        if (old != fingers.end()) {
            Finger f = *old;
            f.pos = t.pos;
            now.push_back(f);
            continue;
        }
        // Dito nuovo: decide subito a cosa serve, in base a dove si appoggia
        Finger f{ t.id, Role::None, -1, t.pos, t.pos, t.pos };
        f.button = ButtonAt(t.pos, layout);
        if (f.button >= 0) f.role = Role::Button;
        else if (layout == Layout::Game) {
            const bool stickZone = t.pos.x < GetScreenWidth() * STICK_ZONE_X && t.pos.y > GetScreenHeight() * STICK_ZONE_TOP;
            f.role = stickZone ? Role::Stick : Role::Look;
        } else if (layout == Layout::Tap) tapped = true;
        now.push_back(f);
    }
    fingers = now;

    const float s = Scale();
    for (Finger& f : fingers) {
        if (f.role == Role::Button) {
            const ButtonSpec& b = BUTTONS[f.button];
            down[b.id] = true;
            if (b.look) lookDx += f.pos.x - f.last.x;
        } else if (f.role == Role::Stick) {
            Vector2 d = Vector2Scale(Vector2Subtract(f.pos, f.start), 1.0f / (STICK_RADIUS * s));
            float len = Vector2Length(d);
            if (len > 1) {
                // Lo stick "segue" il dito se va oltre il bordo, cosi' tornare indietro e' immediato
                f.start = Vector2Add(f.start, Vector2Scale(d, (len - 1) / len * STICK_RADIUS * s));
                d = Vector2Scale(d, 1 / len);
            }
            move = len < 0.15f ? Vector2{ 0, 0 } : d;
        } else if (f.role == Role::Look) {
            lookDx += f.pos.x - f.last.x;
        }
        f.last = f.pos;
    }
}

// Triangolo visibile da qualsiasi verso (raylib ne disegna solo uno)
static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static void DrawIcon(Icon icon, Vector2 c, float r, Color col) {
    const float k = r * 0.45f;
    switch (icon) {
        case Icon::Up:    Tri({ c.x, c.y - k }, { c.x - k, c.y + k * 0.6f }, { c.x + k, c.y + k * 0.6f }, col); break;
        case Icon::Down:  Tri({ c.x, c.y + k }, { c.x - k, c.y - k * 0.6f }, { c.x + k, c.y - k * 0.6f }, col); break;
        case Icon::Left:  Tri({ c.x - k, c.y }, { c.x + k * 0.6f, c.y - k }, { c.x + k * 0.6f, c.y + k }, col); break;
        case Icon::Right: Tri({ c.x + k, c.y }, { c.x - k * 0.6f, c.y - k }, { c.x - k * 0.6f, c.y + k }, col); break;
        case Icon::Pause:
            DrawRectangleRec({ c.x - k * 0.75f, c.y - k, k * 0.5f, k * 2 }, col);
            DrawRectangleRec({ c.x + k * 0.25f, c.y - k, k * 0.5f, k * 2 }, col);
            break;
        default: break;
    }
}

void TouchControls::Draw() const {
    const float s = Scale();
    for (const ButtonSpec& b : BUTTONS) {
        if (b.layout != layout) continue;
        Vector2 c = CenterOf(b);
        float r = b.r * s;
        bool held = down[b.id];
        DrawCircleV(c, r, Fade(held ? GOLD : BLACK, held ? 0.45f : 0.3f));
        DrawRing(c, r - 2.5f * s, r, 0, 360, 48, Fade(WHITE, held ? 0.9f : 0.55f));
        DrawIcon(b.icon, c, r, Fade(WHITE, 0.85f));
        if (*b.label) {
            int size = (int)((strlen(b.label) > 6 ? 13 : 16) * s);
            DrawText(b.label, (int)(c.x - MeasureText(b.label, size) / 2.0f), (int)(c.y - size / 2.0f), size, Fade(WHITE, 0.85f));
        }
    }
    if (layout != Layout::Game) return;

    // Stick: dove e' appoggiato il dito, oppure a riposo nell'angolo
    Vector2 base{ STICK_HOME.x * s, GetScreenHeight() + STICK_HOME.y * s }, knob = base;
    for (const Finger& f : fingers)
        if (f.role == Role::Stick) {
            base = f.start;
            knob = Vector2Add(base, Vector2Scale(move, STICK_RADIUS * s));
        }
    DrawCircleV(base, STICK_RADIUS * s, Fade(BLACK, 0.25f));
    DrawRing(base, STICK_RADIUS * s - 2.5f * s, STICK_RADIUS * s, 0, 360, 48, Fade(WHITE, 0.4f));
    DrawCircleV(knob, 34 * s, Fade(WHITE, 0.45f));
}
