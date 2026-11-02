#include "Touch.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include "raymath.h"
#include "PadRaw.hpp"

// ---------------------------------------------------------------------------
// Disposizione di partenza dei tasti. dx, dy e r sono in pixel per uno schermo alto 720 e si
// ingrandiscono con lo schermo; dx e dy partono dall'angolo scelto (negativi = verso l'interno
// dagli angoli a destra / in basso). In partita il giocatore puo' poi spostarli con l'editor.
// ---------------------------------------------------------------------------
enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
enum class Icon { None, Up, Down, Left, Right, Pause };

using L = TouchControls::Layout;

// Menu e animazioni: tasti fissi
struct ButtonSpec {
    TouchButton id;
    TouchControls::Layout layout;
    const char* label;
    Icon icon;
    Corner corner;
    float dx, dy, r;
};
static const ButtonSpec BUTTONS[] = {
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
constexpr int BUTTON_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

constexpr float STICK_RADIUS = 80; // raggio degli stick a grandezza normale

// In partita: i comandi che il giocatore puo' sistemare (stesso ordine di TouchControl)
struct CtlSpec {
    const char* name;  // nome nell'editor
    TouchButton id;    // tasto che preme (TB_COUNT = nessuno: lo stick per muoversi)
    const char* label;
    Icon icon;
    Corner corner;
    float dx, dy, r;
    bool stick;
    bool top;          // tasti di menu in alto: niente sensibilita'
};
static const CtlSpec CTL[TC_COUNT] = {
    { "Stick per muoversi", TB_COUNT,  "",        Icon::None,  Corner::BottomLeft,   160, -170, STICK_RADIUS, true,  false },
    { "Stick di tiro",      TB_ATTACK, "ATTACCA", Icon::None,  Corner::BottomRight, -155, -170, STICK_RADIUS, true,  false },
    { "Salta",              TB_JUMP,   "SALTA",   Icon::None,  Corner::BottomRight, -275,  -75, 48, false, false },
    { "Scatto",             TB_DASH,   "SCATTO",  Icon::None,  Corner::BottomRight, -285, -235, 48, false, false },
    { "Cambia arma",        TB_SWAP,   "ARMA",    Icon::None,  Corner::BottomRight, -110, -295, 40, false, false },
    { "Raccogli",           TB_USE,    "PRENDI",  Icon::None,  Corner::BottomRight, -400,  -70, 42, false, false },
    { "Borsa",              TB_MATCH,  "BORSA",   Icon::None,  Corner::TopRight,    -150,   48, 36, false, true },
    { "Pausa",              TB_PAUSE,  "",        Icon::Pause, Corner::TopRight,     -60,   48, 36, false, true },
};

constexpr float AIM_DEAD = 0.2f;   // sotto questa spinta lo stick di tiro non sposta la visuale (si spara e basta)
constexpr float AIM_TURN = 0.7f;   // velocita' massima di rotazione dello stick di tiro (1 = quella dello stick destro)
constexpr float STICK_GRAB = 1.25f; // uno stick fisso si prende entro 1,25 raggi dal centro
constexpr float LOOK_BASE = 2.4f;  // visuale libera: sensibilita' alta di partenza, per girarsi anche in un angolino

// Limiti delle barre dell'editor
constexpr float SIZE_LO = 0.5f, SIZE_HI = 2.0f;
constexpr float SENS_LO = 0.3f, SENS_HI = 3.0f;

static float Scale() {
    return std::clamp(std::min(GetScreenWidth(), GetScreenHeight()) / 720.0f, 0.75f, 1.5f);
}

static Vector2 CornerPoint(Corner c, float dx, float dy) {
    const float s = Scale(), sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    const bool right = c == Corner::TopRight || c == Corner::BottomRight;
    const bool bottom = c == Corner::BottomLeft || c == Corner::BottomRight;
    return { (right ? sw : 0) + dx * s, (bottom ? sh : 0) + dy * s };
}

static Vector2 CenterOf(const ButtonSpec& b) { return CornerPoint(b.corner, b.dx, b.dy); }

// Il tasto di menu sotto il dito (con un po' di margine: le dita sono grosse), -1 se nessuno
static int ButtonAt(Vector2 p, L layout) {
    int best = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        if (BUTTONS[i].layout != layout) continue;
        float d = Vector2Distance(p, CenterOf(BUTTONS[i]));
        if (d < BUTTONS[i].r * Scale() * 1.15f && d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Disposizione del giocatore
// ---------------------------------------------------------------------------

void TouchControls::EnsureSetup() const {
    if (setup.valid) return;
    const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    for (int c = 0; c < TC_COUNT; ++c) {
        Vector2 p = CornerPoint(CTL[c].corner, CTL[c].dx, CTL[c].dy);
        setup.ctl[c] = { p.x / sw, p.y / sh, 1, 1, false };
    }
    setup.ctl[TC_MOVE].floating = true; // lo stick per muoversi parte da dove si appoggia il dito
    setup.lookSens = 1;
    setup.valid = true;
}

Vector2 TouchControls::CtlCenter(int c) const {
    EnsureSetup();
    return { setup.ctl[c].x * GetScreenWidth(), setup.ctl[c].y * GetScreenHeight() };
}

float TouchControls::CtlRadius(int c) const {
    EnsureSetup();
    return CTL[c].r * Scale() * setup.ctl[c].size;
}

// Testo: "v1;x,y,grandezza,sensibilita,mobile;...;visuale". Niente "|" (nel browser separa le righe)
std::string TouchControls::SaveSetup() const {
    EnsureSetup();
    std::ostringstream o;
    o << "v1";
    for (const auto& c : setup.ctl) o << ";" << c.x << "," << c.y << "," << c.size << "," << c.sens << "," << (c.floating ? 1 : 0);
    o << ";" << setup.lookSens;
    return o.str();
}

void TouchControls::LoadSetup(const std::string& text) {
    std::vector<std::string> parts;
    std::istringstream in(text);
    std::string part;
    while (std::getline(in, part, ';')) parts.push_back(part);
    if (parts.size() != TC_COUNT + 2 || parts[0] != "v1") return; // niente (o vecchio): disposizione di partenza
    TouchSetup s;
    for (int c = 0; c < TC_COUNT; ++c) {
        int floating = 0;
        auto& k = s.ctl[c];
        if (std::sscanf(parts[c + 1].c_str(), "%f,%f,%f,%f,%d", &k.x, &k.y, &k.size, &k.sens, &floating) != 5) return;
        k.x = std::clamp(k.x, 0.0f, 1.0f);
        k.y = std::clamp(k.y, 0.0f, 1.0f);
        k.size = std::clamp(k.size, SIZE_LO, SIZE_HI);
        k.sens = std::clamp(k.sens, SENS_LO, SENS_HI);
        k.floating = floating != 0 && CTL[c].stick;
    }
    s.lookSens = std::clamp((float)std::atof(parts[TC_COUNT + 1].c_str()), SENS_LO, SENS_HI);
    s.valid = true;
    setup = s;
}

// ---------------------------------------------------------------------------
// Dita
// ---------------------------------------------------------------------------

void TouchControls::SetLayout(Layout l) {
    if (l == layout) return;
    layout = l;
    // Le dita gia' appoggiate non premono niente nella nuova schermata finche' non si staccano
    for (Finger& f : fingers) f.role = Role::None;
}

// Dito nuovo in partita: decide subito a cosa serve, in base a dove si appoggia
void TouchControls::UpdateGame(std::vector<Finger>& now, Finger& f) {
    auto taken = [&](Role r) {
        auto has = [r](const Finger& o) { return o.role == r; };
        return std::any_of(now.begin(), now.end(), has) || std::any_of(fingers.begin(), fingers.end(), has);
    };
    // 1) un tasto (il piu' vicino, con un po' di margine)
    float best = 1e9f;
    for (int c = 0; c < TC_COUNT; ++c) {
        if (CTL[c].stick) continue;
        float d = Vector2Distance(f.pos, CtlCenter(c));
        if (d < CtlRadius(c) * 1.15f && d < best) { best = d; f.button = c; }
    }
    if (f.button >= 0) { f.role = Role::Button; return; }
    // 2) uno stick fisso: si parte dal suo centro
    const int sticks[2] = { TC_MOVE, TC_AIM };
    const Role roles[2] = { Role::Stick, Role::Aim };
    for (int k = 0; k < 2; ++k) {
        const int c = sticks[k];
        if (setup.ctl[c].floating || taken(roles[k])) continue;
        if (Vector2Distance(f.pos, CtlCenter(c)) < CtlRadius(c) * STICK_GRAB) {
            f.role = roles[k];
            f.start = CtlCenter(c);
            return;
        }
    }
    // 3) meta' sinistra = stick per muoversi, destra = stick di tiro, se sono mobili e liberi;
    //    altrimenti il dito sposta la visuale senza sparare
    const int k = f.pos.x < GetScreenWidth() * 0.5f ? 0 : 1;
    f.role = setup.ctl[sticks[k]].floating && !taken(roles[k]) ? roles[k] : Role::Look;
}

void TouchControls::Update() {
    EnsureSetup();
    if (editing) { UpdateEditor(); return; }
    std::copy(std::begin(down), std::end(down), std::begin(prev));
    std::fill(std::begin(down), std::end(down), false);
    tapped = false;
    move = look = moveVec = aimVec = { 0, 0 };
    aimTurn = aimPitch = 0;

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
        Finger f{ t.id, Role::None, -1, t.pos, t.pos, t.pos };
        if (layout == Layout::Game) UpdateGame(now, f);
        else if (layout == Layout::Tap) tapped = true;
        else if ((f.button = ButtonAt(t.pos, layout)) >= 0) f.role = Role::Button;
        now.push_back(f);
    }
    fingers = now;

    // Spinta di uno stick: da dove parte a dove e' il dito, lunga al massimo 1 (travel = pixel per arrivare a 1).
    // Uno stick mobile "segue" il dito se va oltre il bordo, cosi' tornare indietro e' immediato
    auto stickVector = [](Finger& f, float travel, bool follow) {
        Vector2 d = Vector2Scale(Vector2Subtract(f.pos, f.start), 1.0f / travel);
        float len = Vector2Length(d);
        if (len > 1) {
            if (follow) f.start = Vector2Add(f.start, Vector2Scale(d, (len - 1) / len * travel));
            d = Vector2Scale(d, 1 / len);
        }
        return d;
    };
    for (Finger& f : fingers) {
        if (f.role == Role::Button) {
            if (layout == Layout::Game) { if (CTL[f.button].id != TB_COUNT) down[CTL[f.button].id] = true; }
            else down[BUTTONS[f.button].id] = true;
        } else if (f.role == Role::Stick) {
            // Piu' sensibile = basta spostare meno il dito per correre al massimo
            const auto& c = setup.ctl[TC_MOVE];
            moveVec = stickVector(f, CtlRadius(TC_MOVE) / c.sens, c.floating);
            move = Vector2Length(moveVec) < 0.15f ? Vector2{ 0, 0 } : moveVec;
        } else if (f.role == Role::Aim) {
            // Stick di tiro: finche' il dito e' appoggiato si spara. Ogni asse: niente sotto la zona morta,
            // poi la risposta cresce col quadrato, cosi' con piccoli spostamenti si mira con precisione
            const auto& c = setup.ctl[TC_AIM];
            down[TB_ATTACK] = true;
            aimVec = stickVector(f, CtlRadius(TC_AIM), c.floating);
            auto curve = [](float v) {
                float n = std::max(0.0f, (fabsf(v) - AIM_DEAD) / (1 - AIM_DEAD));
                return copysignf(n * n, v);
            };
            aimTurn = curve(aimVec.x) * AIM_TURN * c.sens;
            aimPitch = -curve(aimVec.y) * c.sens; // dito in su = guarda in su
        } else if (f.role == Role::Look) {
            look = Vector2Add(look, Vector2Scale(Vector2Subtract(f.pos, f.last), LOOK_BASE * setup.lookSens));
        }
        f.last = f.pos;
    }
}

// ---------------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------------

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

static void DrawRoundButton(Vector2 c, float r, const char* label, Icon icon, bool held) {
    const float s = Scale();
    DrawCircleV(c, r, Fade(held ? GOLD : BLACK, held ? 0.45f : 0.3f));
    DrawRing(c, r - 2.5f * s, r, 0, 360, 48, Fade(WHITE, held ? 0.9f : 0.55f));
    DrawIcon(icon, c, r, Fade(WHITE, 0.85f));
    if (*label) {
        int size = std::max(10, (int)((strlen(label) > 6 ? 13 : 16) * s * std::min(1.4f, r / (48 * s))));
        DrawText(label, (int)(c.x - MeasureText(label, size) / 2.0f), (int)(c.y - size / 2.0f), size, Fade(WHITE, 0.85f));
    }
}

static void DrawStick(Vector2 base, Vector2 knob, float r, bool active, bool gold, const char* label) {
    const float s = Scale();
    DrawCircleV(base, r, Fade(active && gold ? GOLD : BLACK, active && gold ? 0.3f : 0.25f));
    DrawRing(base, r - 2.5f * s, r, 0, 360, 48, Fade(WHITE, active ? 0.8f : 0.4f));
    DrawCircleV(knob, r * 0.42f, Fade(active && gold ? GOLD : WHITE, active && gold ? 0.8f : 0.45f));
    if (!active && *label) {
        const int size = (int)(13 * s);
        DrawText(label, (int)(base.x - MeasureText(label, size) / 2.0f), (int)(base.y - size / 2.0f), size, Fade(BLACK, 0.7f));
    }
}

// I comandi della partita; nell'editor sempre "a riposo"
void TouchControls::DrawControls(bool editor) const {
    for (int c = 0; c < TC_COUNT; ++c) {
        const Vector2 home = CtlCenter(c);
        const float r = CtlRadius(c);
        if (!CTL[c].stick) {
            DrawRoundButton(home, r, CTL[c].label, CTL[c].icon, !editor && down[CTL[c].id]);
            continue;
        }
        const Role role = c == TC_MOVE ? Role::Stick : Role::Aim;
        const Vector2 vec = c == TC_MOVE ? moveVec : aimVec;
        Vector2 base = home, knob = home;
        bool active = false;
        if (!editor)
            for (const Finger& f : fingers)
                if (f.role == role) {
                    base = f.start;
                    knob = Vector2Add(base, Vector2Scale(vec, r));
                    active = true;
                }
        DrawStick(base, knob, r, active, c == TC_AIM, CTL[c].label);
    }
}

void TouchControls::Draw() const {
    if (editing) return; // l'editor si disegna con DrawEditor
    if (layout == Layout::Game) { DrawControls(false); return; }
    for (const ButtonSpec& b : BUTTONS)
        if (b.layout == layout) DrawRoundButton(CenterOf(b), b.r * Scale(), b.label, b.icon, down[b.id]);
}

// ---------------------------------------------------------------------------
// Editor dei comandi: si trascina un comando per spostarlo; toccandolo lo si sceglie e nel
// pannello si regolano grandezza, sensibilita' e stick fisso/mobile. Il pannello sta in alto o
// in basso, dalla parte opposta al comando scelto, per non coprirlo.
// ---------------------------------------------------------------------------
enum { SL_SIZE, SL_SENS, SL_LOOK, SL_COUNT };

struct EditorPanel {
    Rectangle panel{}, slider[SL_COUNT]{}, fixedBtn{}, floatBtn{}, resetBtn{}, doneBtn{};
    bool hasSens = false, hasMode = false;
};

static EditorPanel Panel(int selected, Vector2 selectedCenter) {
    const float s = Scale(), sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    EditorPanel p;
    p.hasSens = CTL[selected].stick;
    p.hasMode = CTL[selected].stick;
    const float w = std::min(620 * s, sw - 20), row = 46 * s;
    const float h = row * 5.6f + 20 * s;
    const bool top = selectedCenter.y > sh * 0.5f;
    p.panel = { (sw - w) / 2, top ? 10 * s : sh - h - 10 * s, w, h };
    const float x = p.panel.x + 14 * s, inner = w - 28 * s;
    const float labelW = inner * 0.38f;
    float y = p.panel.y + 10 * s + row; // la prima riga e' il titolo
    auto sliderRow = [&](int i) {
        p.slider[i] = { x + labelW, y + row * 0.25f, inner - labelW, row * 0.5f };
        y += row;
    };
    sliderRow(SL_SIZE);
    if (p.hasSens) sliderRow(SL_SENS);
    else y += row;
    if (p.hasMode) {
        p.fixedBtn = { x + labelW, y + 4 * s, (inner - labelW) / 2 - 4 * s, row - 8 * s };
        p.floatBtn = { x + labelW + (inner - labelW) / 2 + 4 * s, y + 4 * s, (inner - labelW) / 2 - 4 * s, row - 8 * s };
    }
    y += row;
    sliderRow(SL_LOOK);
    p.resetBtn = { x + inner - 2 * (130 * s) - 10 * s, p.panel.y + 6 * s, 130 * s, row - 12 * s };
    p.doneBtn = { x + inner - 130 * s, p.panel.y + 6 * s, 130 * s, row - 12 * s };
    return p;
}

static float SliderValue(const Rectangle& r, float x, float lo, float hi) {
    float t = std::clamp((x - r.x) / r.width, 0.0f, 1.0f);
    return std::round((lo + t * (hi - lo)) * 10) / 10; // a passi di 0,1
}

void TouchControls::StartEditing() {
    EnsureSetup();
    editing = true;
    editDone = false;
    editFinger = -2; // aspetta che si stacchi il dito che ha aperto l'editor
    dragCtl = dragSlider = -1;
    fingers.clear();
    std::fill(std::begin(down), std::end(down), false);
    std::fill(std::begin(prev), std::end(prev), false);
}

void TouchControls::UpdateEditor() {
    const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    // Un dito alla volta: si segue il primo appoggiato
    const int count = RawTouchCount();
    if (editFinger == -2) { if (count == 0) editFinger = -1; return; }
    bool held = false;
    Vector2 pos{};
    for (int i = 0; i < count; ++i) {
        RawTouch t = RawTouchAt(i);
        if (editFinger < 0 || t.id == editFinger) { pos = t.pos; held = true; if (editFinger < 0) editFinger = t.id; break; }
    }
    if (!held) { editFinger = dragCtl = dragSlider = -1; fingers.clear(); return; }

    auto& c = setup.ctl[selected];
    const EditorPanel p = Panel(selected, CtlCenter(selected));
    const bool fresh = fingers.empty(); // dito appena appoggiato
    if (fresh) {
        fingers.push_back({ editFinger, Role::None, -1, pos, pos, pos });
        if (CheckCollisionPointRec(pos, p.panel)) {
            if (CheckCollisionPointRec(pos, p.doneBtn)) {
                // Il dito ancora appoggiato non deve premere il tasto che c'e' sotto nel menu
                editing = false;
                editDone = true;
                fingers.clear();
                for (int i = 0; i < count; ++i) fingers.push_back({ RawTouchAt(i).id, Role::None, -1, pos, pos, pos });
                return;
            }
            if (CheckCollisionPointRec(pos, p.resetBtn)) { setup.valid = false; EnsureSetup(); return; }
            if (p.hasMode && CheckCollisionPointRec(pos, p.fixedBtn)) c.floating = false;
            if (p.hasMode && CheckCollisionPointRec(pos, p.floatBtn)) c.floating = true;
            for (int i = 0; i < SL_COUNT; ++i) {
                Rectangle hit = p.slider[i];
                hit.y -= hit.height * 0.6f; hit.height *= 2.2f; // piu' facile da prendere col dito
                if (hit.width > 0 && CheckCollisionPointRec(pos, hit)) dragSlider = i;
            }
        } else {
            // Il comando piu' vicino sotto il dito
            float best = 1e9f;
            for (int k = 0; k < TC_COUNT; ++k) {
                float d = Vector2Distance(pos, CtlCenter(k));
                if (d < std::max(CtlRadius(k), 30.0f) * 1.2f && d < best) { best = d; dragCtl = k; }
            }
            if (dragCtl >= 0) {
                selected = dragCtl;
                dragOffset = Vector2Subtract(CtlCenter(dragCtl), pos);
            }
        }
    }
    if (dragSlider == SL_SIZE) c.size = SliderValue(p.slider[SL_SIZE], pos.x, SIZE_LO, SIZE_HI);
    if (dragSlider == SL_SENS) c.sens = SliderValue(p.slider[SL_SENS], pos.x, SENS_LO, SENS_HI);
    if (dragSlider == SL_LOOK) setup.lookSens = SliderValue(p.slider[SL_LOOK], pos.x, SENS_LO, SENS_HI);
    if (dragCtl >= 0) {
        // Il comando segue il dito, ma resta tutto dentro lo schermo
        const float r = CtlRadius(dragCtl);
        Vector2 to = Vector2Add(pos, dragOffset);
        setup.ctl[dragCtl].x = std::clamp(to.x, r, sw - r) / sw;
        setup.ctl[dragCtl].y = std::clamp(to.y, r, sh - r) / sh;
    }
}

void TouchControls::DrawEditor() const {
    const float s = Scale(), sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    DrawControls(true);
    // Il comando scelto: anello dorato
    const Vector2 sc = CtlCenter(selected);
    DrawRing(sc, CtlRadius(selected) + 4 * s, CtlRadius(selected) + 9 * s, 0, 360, 48, GOLD);

    const EditorPanel p = Panel(selected, sc);
    DrawRectangleRounded(p.panel, 0.08f, 8, Fade({ 12, 12, 20, 255 }, 0.92f));
    DrawRectangleRoundedLinesEx(p.panel, 0.08f, 8, 2, Fade(GOLD, 0.6f));
    const float x = p.panel.x + 14 * s;
    const int big = (int)(22 * s), text = (int)(18 * s);
    DrawText(TextFormat("Comando: %s", CTL[selected].name), (int)x, (int)(p.panel.y + 16 * s), big, GOLD);
    auto button = [&](Rectangle r, const char* label, bool on) {
        DrawRectangleRounded(r, 0.3f, 6, on ? Fade(GOLD, 0.85f) : Fade(WHITE, 0.12f));
        DrawText(label, (int)(r.x + (r.width - MeasureText(label, text)) / 2), (int)(r.y + (r.height - text) / 2), text, on ? BLACK : WHITE);
    };
    button(p.resetBtn, "Ripristina", false);
    button(p.doneBtn, "Fatto", true);
    auto slider = [&](int i, const char* label, float v, float lo, float hi) {
        const Rectangle& r = p.slider[i];
        DrawText(label, (int)x, (int)(r.y + r.height / 2 - text / 2.0f), text, LIGHTGRAY);
        DrawRectangleRounded({ r.x, r.y + r.height * 0.35f, r.width, r.height * 0.3f }, 1, 6, Fade(WHITE, 0.25f));
        const float t = (v - lo) / (hi - lo);
        DrawRectangleRounded({ r.x, r.y + r.height * 0.35f, r.width * t, r.height * 0.3f }, 1, 6, Fade(GOLD, 0.7f));
        DrawCircleV({ r.x + r.width * t, r.y + r.height / 2 }, r.height * 0.55f, dragSlider == i ? GOLD : WHITE);
        const char* val = TextFormat("x%.1f", v);
        DrawText(val, (int)(r.x + r.width - MeasureText(val, text)), (int)(r.y - text * 0.6f), (int)(text * 0.8f), GRAY);
    };
    const auto& c = setup.ctl[selected];
    slider(SL_SIZE, "Grandezza", c.size, SIZE_LO, SIZE_HI);
    if (p.hasSens) slider(SL_SENS, "Sensibilita'", c.sens, SENS_LO, SENS_HI);
    else DrawText(CTL[selected].top ? "Tasto in alto: solo posizione e grandezza" : "Tasto: solo posizione e grandezza",
                  (int)x, (int)(p.slider[SL_SIZE].y + 46 * s), text, GRAY);
    if (p.hasMode) {
        DrawText("Stick", (int)x, (int)(p.fixedBtn.y + p.fixedBtn.height / 2 - text / 2.0f), text, LIGHTGRAY);
        button(p.fixedBtn, "Fisso", !c.floating);
        button(p.floatBtn, "Mobile", c.floating);
    }
    slider(SL_LOOK, "Visuale libera", setup.lookSens, SENS_LO, SENS_HI);
    DrawText("Trascina un comando per spostarlo, toccalo per sceglierlo", (int)x,
             (int)(p.panel.y + p.panel.height - 4 * s - text * 0.85f), (int)(text * 0.85f), Fade(WHITE, 0.5f));
}
