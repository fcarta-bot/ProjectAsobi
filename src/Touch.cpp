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
};

using L = TouchControls::Layout;
static const ButtonSpec BUTTONS[] = {
    // In partita: azioni attorno al pollice destro, menu in alto a destra
    // ATTACCA non e' un tasto ma il secondo stick (vedi AIM_*): qui c'e' solo dove si disegna a riposo
    { TB_ATTACK, L::Game, "ATTACCA", Icon::None,  Corner::BottomRight, -155, -170, 80 },
    { TB_JUMP,   L::Game, "SALTA",   Icon::None,  Corner::BottomRight, -275,  -75, 48 },
    { TB_DASH,   L::Game, "SCATTO",  Icon::None,  Corner::BottomRight, -285, -235, 48 },
    { TB_SWAP,   L::Game, "ARMA",    Icon::None,  Corner::BottomRight, -110, -295, 40 },
    { TB_USE,    L::Game, "PRENDI",  Icon::None,  Corner::BottomRight, -400,  -70, 42 },
    { TB_MATCH,  L::Game, "BORSA",   Icon::None,  Corner::TopRight,    -150,   48, 36 },
    { TB_PAUSE,  L::Game, "",        Icon::Pause, Corner::TopRight,     -60,   48, 36 },
    // Nei menu: frecce a sinistra, OK e Indietro a destra
    { TB_UP,     L::Menu, "", Icon::Up,    Corner::BottomLeft,  105, -180, 38 },
    { TB_DOWN,   L::Menu, "", Icon::Down,  Corner::BottomLeft,  105,  -56, 38 },
    { TB_LEFT,   L::Menu, "", Icon::Left,  Corner::BottomLeft,   43, -118, 38 },
    { TB_RIGHT,  L::Menu, "", Icon::Right, Corner::BottomLeft,  167, -118, 38 },
    { TB_OK,     L::Menu, "OK",       Icon::None, Corner::BottomRight, -95,  -105, 54 },
    { TB_BACK,   L::Menu, "INDIETRO", Icon::None, Corner::BottomRight, -225,  -56, 42 },
    // Nelle animazioni si tocca ovunque per andare avanti; questo salta tutto
    { TB_SKIP,   L::Tap,  "SALTA",    Icon::None, Corner::TopRight,     -70,   56, 40 },
};

constexpr float STICK_RADIUS = 80;        // quanto spostare il dito per correre al massimo
constexpr float STICK_ZONE_X = 0.45f;     // a sinistra di qui il dito fa da stick per muoversi, a destra da stick di tiro
constexpr float AIM_DEAD = 0.2f;          // sotto questa spinta lo stick di tiro non gira la visuale (si spara e basta)
constexpr float AIM_TURN = 0.7f;          // velocita' massima di rotazione dello stick di tiro (1 = quella dello stick destro)
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

static int AttackIndex() {
    for (int i = 0; i < (int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])); ++i)
        if (BUTTONS[i].id == TB_ATTACK) return i;
    return -1;
}

// Il tasto sotto il dito (con un po' di margine: le dita sono grosse), -1 se nessuno
static int ButtonAt(Vector2 p, L layout) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < (int)(sizeof(BUTTONS) / sizeof(BUTTONS[0])); ++i) {
        if (BUTTONS[i].layout != layout) continue;
        if (BUTTONS[i].id == TB_ATTACK) continue; // ATTACCA e' lo stick di tiro, non un tasto
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
    aimTurn = aimPitch = 0;
    aimVec = { 0, 0 };

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
        if (layout == Layout::Game) {
            // I tasti hanno la precedenza; fuori dai tasti la meta' sinistra e' lo stick per muoversi e
            // la destra lo stick di tiro, che parte da dove si appoggia il dito (come quello sinistro).
            // Un secondo dito a destra, mentre lo stick di tiro e' gia' preso, gira solo la visuale.
            auto taken = [&](Role r) {
                auto has = [r](const Finger& o) { return o.role == r; };
                return std::any_of(now.begin(), now.end(), has) || std::any_of(fingers.begin(), fingers.end(), has);
            };
            if ((f.button = ButtonAt(t.pos, layout)) >= 0) f.role = Role::Button;
            else if (t.pos.x < GetScreenWidth() * STICK_ZONE_X) f.role = taken(Role::Stick) ? Role::None : Role::Stick;
            else f.role = taken(Role::Aim) ? Role::Look : Role::Aim;
        } else if (layout == Layout::Tap) {
            tapped = true;
        } else if ((f.button = ButtonAt(t.pos, layout)) >= 0) {
            f.role = Role::Button;
        }
        now.push_back(f);
    }
    fingers = now;

    const float s = Scale();
    // Spinta di uno stick: da dove si e' appoggiato il dito a dove e' adesso, lunga al massimo 1.
    // Se il dito va oltre il bordo lo stick lo "segue", cosi' tornare indietro e' immediato
    auto stickVector = [s](Finger& f) {
        Vector2 d = Vector2Scale(Vector2Subtract(f.pos, f.start), 1.0f / (STICK_RADIUS * s));
        float len = Vector2Length(d);
        if (len > 1) {
            f.start = Vector2Add(f.start, Vector2Scale(d, (len - 1) / len * STICK_RADIUS * s));
            d = Vector2Scale(d, 1 / len);
        }
        return d;
    };
    for (Finger& f : fingers) {
        if (f.role == Role::Button) {
            down[BUTTONS[f.button].id] = true;
        } else if (f.role == Role::Stick) {
            Vector2 d = stickVector(f);
            move = Vector2Length(d) < 0.15f ? Vector2{ 0, 0 } : d;
        } else if (f.role == Role::Aim) {
            // Stick di tiro: finche' il dito e' appoggiato si spara; conta solo lo spostamento
            // laterale (tutto a destra o a sinistra = ci si gira alla massima velocita'), su e giu' niente
            down[TB_ATTACK] = true;
            aimVec = stickVector(f);
            // Ogni asse: niente sotto la zona morta, poi la risposta cresce col quadrato, cosi'
            // con piccoli spostamenti si mira con precisione e solo spingendo tutto si gira veloce
            auto curve = [](float v) {
                float n = std::max(0.0f, (fabsf(v) - AIM_DEAD) / (1 - AIM_DEAD));
                return copysignf(n * n, v);
            };
            aimTurn = curve(aimVec.x) * AIM_TURN;
            aimPitch = -curve(aimVec.y); // dito in su = guarda in su
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
        if (b.layout != layout || b.id == TB_ATTACK) continue; // lo stick di tiro si disegna sotto
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

    // Stick di tiro: come quello sinistro, dorato mentre si spara
    const ButtonSpec& a = BUTTONS[AttackIndex()];
    Vector2 aBase = CenterOf(a), aKnob = aBase;
    for (const Finger& f : fingers)
        if (f.role == Role::Aim) {
            aBase = f.start;
            aKnob = Vector2Add(aBase, Vector2Scale(aimVec, STICK_RADIUS * s));
        }
    const bool firing = down[TB_ATTACK];
    DrawCircleV(aBase, STICK_RADIUS * s, Fade(firing ? GOLD : BLACK, firing ? 0.3f : 0.25f));
    DrawRing(aBase, STICK_RADIUS * s - 2.5f * s, STICK_RADIUS * s, 0, 360, 48, Fade(WHITE, firing ? 0.8f : 0.4f));
    DrawCircleV(aKnob, 34 * s, Fade(firing ? GOLD : WHITE, firing ? 0.8f : 0.45f));
    if (!firing) {
        const int size = (int)(13 * s);
        DrawText(a.label, (int)(aBase.x - MeasureText(a.label, size) / 2.0f), (int)(aBase.y - size / 2.0f), size, Fade(BLACK, 0.7f));
    }
}
