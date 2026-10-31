#include "Cutscene.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include "raymath.h"
#include "rlgl.h"
#include "Dungeon.hpp" // Mix e Shade per i colori
#include "Ui.hpp"

// ---------------------------------------------------------------------------
// Strumenti
// ---------------------------------------------------------------------------

static float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
static float Smooth(float a, float b, float t) { float k = Clamp01((t - a) / (b - a)); return k * k * (3 - 2 * k); }
static unsigned Hash(int i, int salt) {
    unsigned h = (unsigned)i * 2654435761u + (unsigned)salt * 97531u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return h;
}
static float HashF(int i, int salt) { return (Hash(i, salt) % 10000) / 10000.0f; }

// Tela virtuale 1280x720: le scene si disegnano sempre con le stesse coordinate
static void BeginCanvas() {
    float s = std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
    rlPushMatrix();
    rlTranslatef((GetScreenWidth() - 1280 * s) / 2, (GetScreenHeight() - 720 * s) / 2, 0);
    rlScalef(s, s, 1);
}
static void EndCanvas() { rlPopMatrix(); }

// I triangoli 2D di raylib si vedono solo in un verso: li disegniamo in entrambi
static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

void DrawTextWrapped(const char* text, Rectangle box, int size, Color color, int maxChars) {
    std::string all = text;
    if (maxChars >= 0 && maxChars < (int)all.size()) all.resize(maxChars);
    float x = box.x, y = box.y;
    size_t i = 0;
    while (i < all.size()) {
        size_t end = all.find(' ', i);
        if (end == std::string::npos) end = all.size();
        std::string word = all.substr(i, end - i);
        int w = MeasureText(word.c_str(), size);
        if (x > box.x && x + w > box.x + box.width) { x = box.x; y += size + 6; }
        DrawText(word.c_str(), (int)x, (int)y, size, color);
        x += w + MeasureText(" ", size);
        i = end + 1;
    }
}

static void Caption(const char* text, float t, float alpha = 1.0f) {
    if (t <= 0 || alpha <= 0) return;
    int chars = (int)(t * 38);
    DrawRectangle(0, 610, 1280, 110, Fade(BLACK, 0.55f * alpha));
    DrawTextWrapped(text, { 140, 632, 1000, 80 }, 26, Fade({ 240, 236, 225, 255 }, alpha), chars);
}

// ---------------------------------------------------------------------------
// Il GameMaster
// ---------------------------------------------------------------------------

void DrawSparkles(Vector2 c, float radius, float t, float alpha, int count) {
    if (alpha <= 0) return;
    for (int i = 0; i < count; ++i) {
        float a = i * 2 * PI / count + t * 1.7f;
        float r = radius * (0.45f + 0.55f * (0.5f + 0.5f * sinf(t * 3 + i * 1.3f)));
        Vector2 p{ c.x + cosf(a) * r, c.y + sinf(a) * r };
        float s = 3 + 3 * (0.5f + 0.5f * sinf(t * 6 + i));
        Color col = Fade(i % 3 ? WHITE : Color{ 255, 220, 120, 255 }, alpha);
        DrawLineEx({ p.x - s, p.y }, { p.x + s, p.y }, 1.5f, col);
        DrawLineEx({ p.x, p.y - s }, { p.x, p.y + s }, 1.5f, col);
    }
}

// Arto che parte da "joint": angolo 0 = verso il basso, positivo = verso destra
static Vector2 Limb(Vector2 joint, float angle, float len) {
    return { joint.x + sinf(angle) * len, joint.y + cosf(angle) * len };
}

void DrawStickman(Vector2 feet, float h, float t, StickPose pose, float alpha, bool glow) {
    if (h < 2 || alpha <= 0) return;
    const float thick = std::max(2.0f, h * 0.045f), headR = h * 0.12f;
    const float bob = pose == StickPose::Float ? sinf(t * 2.2f) * h * 0.03f : 0;
    feet.y += bob;
    Vector2 head{ feet.x, feet.y - h + headR };
    Vector2 neck{ feet.x, head.y + headR };
    Vector2 shoulder{ feet.x, neck.y + h * 0.06f };
    Vector2 hip{ feet.x, feet.y - h * 0.42f };
    const float legLen = h * 0.44f, armLen = h * 0.3f;

    float legL = -0.2f, legR = 0.2f, armL = -0.35f, armR = 0.35f;
    switch (pose) {
        case StickPose::Stand:
            armL = -0.3f - 0.04f * sinf(t * 2); armR = 0.3f + 0.04f * sinf(t * 2);
            break;
        case StickPose::Walk: {
            float s = sinf(t * 7);
            legL = -0.45f * s; legR = 0.45f * s; armL = 0.4f * s - 0.1f; armR = -0.4f * s + 0.1f;
            break;
        }
        case StickPose::Float:
            legL = -0.1f; legR = 0.1f;
            armL = -1.9f + 0.15f * sinf(t * 2); armR = 1.9f - 0.15f * sinf(t * 2);
            break;
        case StickPose::Talk:
            armL = -0.4f; armR = 1.3f + 0.45f * sinf(t * 4.5f); // gesticola
            break;
        case StickPose::Wave:
            armL = -0.35f; armR = 2.6f + 0.4f * sinf(t * 9); // saluta
            break;
    }

    Vector2 pts[5][2] = {
        { neck, hip },
        { hip, Limb(hip, legL, legLen) },
        { hip, Limb(hip, legR, legLen) },
        { shoulder, Limb(shoulder, armL, armLen) },
        { shoulder, Limb(shoulder, armR, armLen) },
    };
    if (glow) { // alone luminoso: e' pur sempre un essere soprannaturale
        DrawCircleV({ feet.x, feet.y - h * 0.5f }, h * 0.55f, Fade({ 255, 250, 220, 255 }, 0.06f * alpha));
        for (auto& p : pts) DrawLineEx(p[0], p[1], thick * 3.2f, Fade(WHITE, 0.12f * alpha));
        DrawCircleV(head, headR * 1.6f, Fade(WHITE, 0.12f * alpha));
    }
    for (auto& p : pts) {
        DrawLineEx(p[0], p[1], thick, Fade(WHITE, alpha));
        DrawCircleV(p[1], thick * 0.5f, Fade(WHITE, alpha));
    }
    DrawCircleV(head, headR, Fade(WHITE, alpha));
    // Occhi e un sorrisetto furbo
    Color ink = Fade({ 30, 30, 40, 255 }, alpha);
    DrawCircleV({ head.x - headR * 0.35f, head.y - headR * 0.1f }, std::max(1.0f, headR * 0.14f), ink);
    DrawCircleV({ head.x + headR * 0.35f, head.y - headR * 0.1f }, std::max(1.0f, headR * 0.14f), ink);
    DrawLineEx({ head.x - headR * 0.35f, head.y + headR * 0.35f }, { head.x + headR * 0.4f, head.y + headR * 0.25f },
               std::max(1.0f, headR * 0.1f), ink);
}

// ---------------------------------------------------------------------------
// La camera da letto (di notte nell'intro, all'alba al risveglio)
// ---------------------------------------------------------------------------

// sit: 0 = sdraiato, 1 = seduto. awake: occhi aperti e spaventati
static Vector2 DrawBedroom(bool night, float sit, bool awake, float t, float headGlow) {
    // Pareti e pavimento
    Color wallTop = night ? Color{ 28, 32, 62, 255 } : Color{ 150, 110, 120, 255 };
    Color wallBottom = night ? Color{ 18, 20, 40, 255 } : Color{ 110, 80, 90, 255 };
    DrawRectangleGradientV(0, 0, 1280, 560, wallTop, wallBottom);
    Color floorCol = night ? Color{ 52, 36, 30, 255 } : Color{ 120, 84, 60, 255 };
    DrawRectangle(0, 560, 1280, 160, floorCol);
    for (int k = 0; k < 6; ++k) DrawLine(0, 580 + k * 26, 1280, 580 + k * 26, Fade(BLACK, 0.2f));

    // Finestra: luna di notte, alba al risveglio
    Rectangle win{ 130, 120, 240, 210 };
    if (night) {
        DrawRectangleGradientV((int)win.x, (int)win.y, (int)win.width, (int)win.height, { 10, 14, 40, 255 }, { 30, 40, 90, 255 });
        DrawCircle(300, 170, 26, { 240, 240, 220, 255 });
        for (int i = 0; i < 12; ++i) DrawPixel(140 + (int)(HashF(i, 1) * 220), 130 + (int)(HashF(i, 2) * 190), WHITE);
    } else {
        DrawRectangleGradientV((int)win.x, (int)win.y, (int)win.width, (int)win.height, { 120, 140, 210, 255 }, { 255, 180, 130, 255 });
        DrawCircle(250, 330, 40, { 255, 220, 150, 255 });
    }
    DrawRectangleLinesEx(win, 8, { 90, 70, 60, 255 });
    DrawRectangle((int)(win.x + win.width / 2 - 3), (int)win.y, 6, (int)win.height, { 90, 70, 60, 255 });
    DrawRectangle((int)win.x, (int)(win.y + win.height / 2 - 3), (int)win.width, 6, { 90, 70, 60, 255 });

    // Comodino con la sveglia: 3:33 di notte, 7:02 al risveglio
    DrawRectangle(190, 470, 110, 90, { 96, 66, 48, 255 });
    DrawRectangle(196, 500, 98, 4, Fade(BLACK, 0.3f));
    DrawRectangle(215, 440, 60, 30, { 30, 30, 34, 255 });
    DrawText(night ? "03:33" : "07:02", 220, 447, 16, night ? Color{ 255, 70, 70, 255 } : Color{ 120, 255, 140, 255 });

    // Letto
    Color wood{ 110, 72, 50, 255 };
    DrawRectangle(360, 470, 560, 52, wood);                 // struttura
    DrawRectangle(370, 522, 20, 38, wood);                  // gambe
    DrawRectangle(890, 522, 20, 38, wood);
    DrawRectangle(338, 370, 34, 190, Shade(wood, 0.85f)); // testiera
    DrawRectangle(905, 430, 30, 130, Shade(wood, 0.85f)); // pediera
    DrawRectangle(372, 452, 534, 26, { 235, 232, 225, 255 }); // materasso
    DrawEllipse(430, 450, 52, 16, { 250, 250, 248, 255 });   // cuscino

    // Il dormiente: si tira su ruotando intorno al bacino
    Vector2 hip{ 540, 452 };
    float angle = Lerp(-PI / 2 + 0.18f, -0.08f, Clamp01(sit)); // da sdraiato (testa a sinistra) a seduto
    Vector2 dir{ sinf(angle), -cosf(angle) };
    Vector2 neck = Vector2Add(hip, Vector2Scale(dir, 82));
    Vector2 head = Vector2Add(neck, Vector2Scale(dir, 24));
    if (awake) { // trema dallo spavento
        float shake = std::max(0.0f, 1.0f - (t - WakeUp::SIT) * 0.8f) * 3;
        head.x += sinf(t * 60) * shake;
        neck.x += sinf(t * 60) * shake * 0.5f;
    }
    Color pajama = night ? Color{ 70, 110, 170, 255 } : Color{ 90, 140, 200, 255 };
    DrawLineEx(hip, neck, 30, pajama);
    if (sit > 0.5f) { // braccia appoggiate sulla coperta
        DrawLineEx(Vector2Add(neck, { 0, 10 }), { neck.x + 50, hip.y - 4 }, 12, pajama);
        DrawLineEx(Vector2Add(neck, { 6, 12 }), { neck.x + 70, hip.y - 2 }, 12, Shade(pajama, 0.85f));
    }
    // Coperta: copre tutto quando dorme, solo le gambe quando e' seduto
    float blanketStart = Lerp(head.x + 20, hip.x - 20, Clamp01(sit));
    Color blanket = night ? Color{ 70, 86, 150, 255 } : Color{ 170, 100, 120, 255 };
    DrawRectangleRounded({ blanketStart, 430, 905 - blanketStart, 48 }, 0.5f, 8, blanket);
    for (int k = 0; k < 4; ++k)
        DrawLine((int)(blanketStart + 40 + k * 90), 438, (int)(blanketStart + 60 + k * 90), 474, Fade(BLACK, 0.15f));

    // Testa
    if (headGlow > 0) DrawCircleV(head, 30 + 30 * headGlow, Fade({ 255, 240, 200, 255 }, 0.35f * headGlow));
    Color skin{ 238, 204, 170, 255 };
    DrawCircleV(head, 24, skin);
    // Capelli: sulla nuca, che da sdraiato e' verso la testiera (a sinistra) e da seduto e' in alto
    float hairDir = atan2f(dir.y, dir.x) * RAD2DEG; // dal centro della testa, dalla parte opposta al collo
    DrawCircleSector(head, 25, hairDir - 70, hairDir + 70, 12, { 50, 36, 30, 255 });
    Vector2 face = Vector2Add(head, Vector2Scale({ dir.y, -dir.x }, -6)); // il viso guarda in avanti/verso l'alto
    if (awake) {
        for (float s : { -8.0f, 8.0f }) {
            DrawCircleV({ face.x + s, face.y - 2 }, 6, WHITE);
            DrawCircleV({ face.x + s, face.y - 2 }, 2.5f, BLACK);
        }
        DrawCircleV({ face.x, face.y + 10 }, 4, { 120, 50, 50, 255 }); // bocca aperta
        // Sudore
        for (int k = 0; k < 2; ++k) {
            float ph = fmodf(t * 1.3f + k * 0.5f, 1.0f);
            Vector2 d{ head.x + 26 + k * 6, head.y - 18 + ph * 30 };
            DrawCircleV(d, 4, Fade({ 140, 200, 255, 255 }, 1 - ph));
            Tri({ d.x - 4, d.y }, { d.x + 4, d.y }, { d.x, d.y - 8 }, Fade({ 140, 200, 255, 255 }, 1 - ph));
        }
    } else {
        DrawLineEx({ face.x - 12, face.y - 4 }, { face.x - 4, face.y - 2 }, 2, { 60, 40, 30, 255 }); // occhi chiusi
        DrawLineEx({ face.x + 4, face.y - 2 }, { face.x + 12, face.y - 4 }, 2, { 60, 40, 30, 255 });
    }
    return head;
}

static void DrawZzz(Vector2 head, float t, float alpha) {
    for (int k = 0; k < 3; ++k) {
        float ph = fmodf(t * 0.5f + k / 3.0f, 1.0f);
        DrawText("z", (int)(head.x + 20 + ph * 60), (int)(head.y - 30 - ph * 90), 18 + (int)(ph * 16), Fade(WHITE, alpha * (1 - ph)));
    }
}

// ---------------------------------------------------------------------------
// Il sogno: il Giappone al tramonto, poi il labirinto che precipita
// ---------------------------------------------------------------------------

struct Slab { float x, w, h, land; }; // un muro del labirinto: posizione, misure, istante in cui tocca terra
static const Slab SLABS[] = {
    { 90, 150, 380, 0.9f }, { 400, 130, 440, 1.25f }, { 660, 180, 360, 1.6f }, { 930, 140, 420, 1.95f }, { 1150, 160, 390, 2.3f },
};

static void DrawDream(float t, float fall) {
    // Scossa quando i muri toccano terra
    float shake = 0;
    for (const Slab& s : SLABS) {
        float since = fall - s.land;
        if (since > 0 && since < 0.6f) shake = std::max(shake, (0.6f - since) * 16);
    }
    rlPushMatrix();
    rlTranslatef(sinf(t * 70) * shake, cosf(t * 53) * shake, 0);

    // Cielo, sole, montagne
    float gloom = Smooth(0.8f, 3.0f, fall); // il labirinto oscura il cielo
    DrawRectangleGradientV(-20, -20, 1320, 600, Mix({ 70, 46, 120, 255 }, { 40, 34, 60, 255 }, gloom),
                           Mix({ 245, 150, 110, 255 }, { 120, 90, 100, 255 }, gloom));
    DrawCircle(900, 330, 90, Fade({ 255, 210, 150, 255 }, 0.3f));
    DrawCircle(900, 330, 62, Mix({ 255, 225, 160, 255 }, { 200, 160, 130, 255 }, gloom));
    Color mountain = Mix({ 110, 70, 120, 255 }, { 60, 50, 70, 255 }, gloom);
    Tri({ -40, 480 }, { 260, 250 }, { 560, 480 }, mountain);
    Tri({ 380, 480 }, { 720, 290 }, { 1060, 480 }, Shade(mountain, 0.9f));
    Tri({ 860, 480 }, { 1140, 270 }, { 1400, 480 }, mountain);

    // Pagoda
    Color sil = Mix({ 60, 36, 60, 255 }, { 30, 26, 36, 255 }, gloom);
    for (int k = 0; k < 5; ++k) {
        float w = 140 - k * 22, y = 560 - k * 42;
        DrawRectangle((int)(300 - w * 0.3f), (int)(y - 30), (int)(w * 0.6f), 30, sil);
        Tri({ 300 - w / 2, y - 28 }, { 300 + w / 2, y - 28 }, { 300, y - 48 }, sil);
    }
    DrawRectangle(298, 290, 4, 50, sil);

    // Case: quelle sotto un muro caduto si schiacciano
    for (int i = 0; i < 9; ++i) {
        float x = 20 + i * 145 + HashF(i, 5) * 30, w = 100 + HashF(i, 6) * 30, h = 70 + HashF(i, 7) * 40;
        float squash = 1;
        for (const Slab& s : SLABS)
            if (fall > s.land && x + w > s.x && x < s.x + s.w) squash = std::min(squash, 0.35f);
        h *= squash;
        Color wall = Mix({ 200, 170, 150, 255 }, { 110, 100, 100, 255 }, gloom);
        DrawRectangle((int)x, (int)(600 - h), (int)w, (int)h, wall);
        Tri({ x - 10, 600 - h }, { x + w + 10, 600 - h }, { x + w / 2, 600 - h - 30 * squash }, Shade(wall, 0.45f));
        if (squash == 1) DrawRectangle((int)(x + w * 0.35f), (int)(600 - h * 0.55f), (int)(w * 0.3f), (int)(h * 0.55f), Shade(wall, 0.6f));
    }

    // Ciliegi in fiore
    for (float cx : { 180.0f, 1180.0f }) {
        DrawRectangle((int)cx - 6, 520, 12, 80, { 80, 50, 40, 255 });
        for (int k = 0; k < 5; ++k)
            DrawCircleV({ cx - 40 + k * 20, 500 - (k % 2) * 20.0f }, 34, Mix({ 250, 180, 200, 255 }, { 150, 110, 130, 255 }, gloom));
    }

    // Strada e torii in primo piano
    DrawRectangle(-20, 600, 1320, 140, Mix({ 90, 72, 82, 255 }, { 50, 46, 52, 255 }, gloom));
    for (int k = 0; k < 8; ++k) DrawRectangle(40 + k * 170, 660, 80, 6, Fade(WHITE, 0.5f));
    Color red = Mix({ 225, 60, 35, 255 }, { 140, 50, 40, 255 }, gloom);
    DrawRectangle(1010, 470, 16, 180, red);
    DrawRectangle(1150, 470, 16, 180, red);
    DrawRectangle(985, 455, 206, 18, red);
    DrawRectangle(975, 446, 226, 10, { 30, 20, 20, 255 });
    DrawRectangle(1000, 500, 176, 12, red);

    // Petali che cadono (finche' non arriva il labirinto)
    for (int i = 0; i < 40; ++i) {
        float ph = fmodf(t * (0.12f + HashF(i, 8) * 0.1f) + HashF(i, 9), 1.0f);
        float x = HashF(i, 10) * 1400 - 60 + ph * 120 + sinf(t * 2 + i) * 15, y = -20 + ph * 680;
        DrawEllipse((int)x, (int)y, 4, 2, Fade({ 255, 200, 215, 255 }, 1 - gloom));
    }

    // I muri del labirinto: cadono accelerando, poi polvere
    for (const Slab& s : SLABS) {
        float start = s.land - 0.8f;
        if (fall < start) continue;
        float k = Clamp01((fall - start) / 0.8f);
        float bottom = Lerp(-40, 610, k * k);
        Rectangle r{ s.x, bottom - s.h, s.w, s.h };
        DrawRectangleRec(r, { 104, 106, 116, 255 });
        for (int row = 0; row * 34 < s.h; ++row) { // blocchi di pietra
            float y = r.y + row * 34;
            DrawLine((int)r.x, (int)y, (int)(r.x + r.width), (int)y, { 70, 72, 80, 255 });
            for (float bx = r.x + (row % 2 ? 30 : 60); bx < r.x + r.width; bx += 60)
                DrawLine((int)bx, (int)y, (int)bx, (int)std::min(y + 34, r.y + r.height), { 70, 72, 80, 255 });
        }
        DrawRectangleLinesEx(r, 3, { 60, 62, 70, 255 });
        float since = fall - s.land;
        if (since > 0 && since < 2.5f) // nuvola di polvere alla base
            for (int p = 0; p < 7; ++p) {
                float spread = since * 90 * (0.6f + HashF(p, (int)s.x) * 0.6f);
                DrawCircleV({ s.x + s.w / 2 + (p - 3) * spread * 0.45f, 610 - HashF(p, 11) * 30 - since * 12 },
                            20 + since * 30, Fade({ 180, 170, 160, 255 }, 0.45f * (1 - since / 2.5f)));
            }
    }
    rlPopMatrix();
}

// ---------------------------------------------------------------------------
// Intro
// ---------------------------------------------------------------------------

static const char* GM_LINES[Intro::LINES] = {
    "Ciao! Sono il GameMaster. Mi annoiavo... e allora eccoci qua!",
    "Questo e' il tuo sogno, ma il labirinto e' mio. Trova il torii per scendere sempre piu' a fondo.",
    "Raccogli artefatti e armi, e combinali bene: lo zaino e' piccolo, dovrai scegliere cosa tenere.",
    "E se cadi... ti svegli. Ma ti aspettero' al prossimo sogno. Divertiti!",
};

float Intro::NextStop(float t) {
    const float stops[] = { ROOM, DREAM, FALL, TALK + 0.8f };
    for (float s : stops) if (t < s - 0.2f) return s;
    // Durante il discorso: alla battuta successiva
    float line = floorf((t - (TALK + 0.8f)) / LINE_TIME) + 1;
    return std::min(END, TALK + 0.8f + line * LINE_TIME);
}

static void DrawSkyScene(float t) {
    DrawRectangleGradientV(0, 0, 1280, 720, { 2, 2, 10, 255 }, { 10, 16, 44, 255 });
    for (int i = 0; i < 160; ++i) {
        float tw = 0.5f + 0.5f * sinf(t * 2 + i);
        DrawCircleV({ HashF(i, 1) * 1280, HashF(i, 2) * 560 }, 1 + HashF(i, 3) * 1.2f, Fade(WHITE, 0.3f + 0.7f * tw * HashF(i, 4)));
    }
    // La Terra
    Vector2 c{ 640, 720 + 760 };
    DrawCircleV(c, 980, Fade({ 120, 180, 255, 255 }, 0.2f)); // atmosfera
    DrawCircleV(c, 950, { 36, 90, 170, 255 });
    for (int i = 0; i < 6; ++i)
        DrawEllipse((int)(200 + i * 190 + HashF(i, 5) * 60), (int)(560 + HashF(i, 6) * 60), 70 + HashF(i, 7) * 50, 22,
                    { 70, 140, 80, 255 });
    // Il GM scende piano, poi si tuffa verso la Terra
    float k = Smooth(0.3f, 4.6f, t);
    Vector2 feet{ 640, Lerp(-40, 470, k) };
    float h = 150 * (1 - Smooth(4.9f, 5.7f, t));
    for (int i = 1; i <= 6; ++i) // scia luminosa
        DrawCircleV({ feet.x, feet.y - h * 0.5f - i * 22 }, 10 - i, Fade({ 255, 240, 200, 255 }, 0.25f * (1 - Smooth(4.9f, 5.7f, t))));
    DrawStickman(feet, h, t, StickPose::Float, 1);
    DrawSparkles({ feet.x, feet.y - h * 0.5f }, 60 + h * 0.2f, t, 0.8f);
    if (t > 5.4f) DrawCircleV({ 640, 480 }, (t - 5.4f) * 400, Fade(WHITE, 1 - Clamp01((t - 5.4f) / 0.6f))); // lampo
    Caption("Il GameMaster si annoiava.", t - 0.6f);
}

static void DrawRoomScene(float u) {
    float glow = Smooth(6.0f, 7.0f, u) * (0.7f + 0.3f * sinf(u * 6));
    Vector2 head = DrawBedroom(true, 0, false, u, glow);
    if (u < 6.2f) DrawZzz(head, u, 1);

    // Il GM compare ai piedi del letto, vola sopra il letto e si infila nella testa
    float appear = Smooth(1.0f, 1.8f, u);
    if (u > 0.8f && u < 6.2f) {
        Vector2 foot{ 990, 560 };
        Vector2 aboveHead{ head.x + 30, head.y - 20 };
        Vector2 pos = foot;
        float h = 150;
        StickPose pose = StickPose::Stand;
        if (u > 2.2f) {
            float k = Smooth(2.2f, 4.6f, u);
            pos = { Lerp(foot.x, aboveHead.x + 40, k), Lerp(foot.y, aboveHead.y - 60, Smooth(2.2f, 3.0f, u)) };
            pose = StickPose::Float;
        }
        if (u > 4.6f) { // si rimpicciolisce e scivola dentro la testa
            float k = Smooth(4.6f, 6.0f, u);
            pos = Vector2Lerp({ aboveHead.x + 40, aboveHead.y - 60 }, { head.x, head.y + 20 }, k);
            h = 150 * (1 - k);
        }
        DrawStickman(pos, h, u, pose, appear);
        DrawSparkles({ pos.x, pos.y - h * 0.5f }, 40 + h * 0.3f, u, appear * (u < 2.2f ? 1.0f : 0.5f));
    }
    // Il sogno inizia: bolle di pensiero e un piccolo torii
    if (u > 6.3f) {
        float k = Smooth(6.3f, 7.3f, u);
        for (int i = 0; i < 3; ++i) DrawCircleV({ head.x + 30 + i * 28, head.y - 40 - i * 34 }, (6 + i * 5) * k, Fade(WHITE, 0.85f));
        Vector2 b{ head.x + 170, head.y - 190 };
        DrawEllipse((int)b.x, (int)b.y, 110 * k, 70 * k, Fade(WHITE, 0.9f));
        if (k > 0.8f) {
            DrawRectangle((int)b.x - 30, (int)b.y - 18, 8, 50, { 225, 60, 35, 255 });
            DrawRectangle((int)b.x + 22, (int)b.y - 18, 8, 50, { 225, 60, 35, 255 });
            DrawRectangle((int)b.x - 44, (int)b.y - 28, 88, 10, { 225, 60, 35, 255 });
        }
    }
    if (u < 0.8f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 1 - u / 0.8f));
    if (u > 7.6f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, Smooth(7.6f, 8.5f, u))); // dentro il sogno
    Caption("Cosi', un giorno, scese sulla terra per insinuarsi nei sogni del povero malcapitato.", u - 0.6f);
}

void DrawIntro(float t) {
    BeginCanvas();
    if (t < Intro::ROOM) {
        DrawSkyScene(t);
    } else if (t < Intro::DREAM) {
        DrawRoomScene(t - Intro::ROOM);
    } else if (t < Intro::TALK) {
        float fall = t - Intro::FALL;
        DrawDream(t, fall);
        if (t < Intro::DREAM + 0.8f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, 1 - (t - Intro::DREAM) / 0.8f));
        if (t < Intro::FALL) Caption("Il malcapitato sognava il Giappone: templi, ciliegi in fiore, il tramonto...", t - Intro::DREAM - 0.6f);
        else Caption("...finche' dal cielo non precipito' un labirinto.", t - Intro::FALL - 0.2f);
    } else {
        // Il GM appare, spiega e sparisce
        DrawDream(t, 99);
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.45f));
        float u = t - Intro::TALK;
        float alpha = Smooth(0, 0.8f, u) * (1 - Smooth(Intro::VANISH - Intro::TALK, Intro::VANISH - Intro::TALK + 0.6f, u));
        Vector2 feet{ 320, 640 };
        float h = 300 * (1 + 0.6f * Smooth(Intro::VANISH - Intro::TALK, Intro::VANISH - Intro::TALK + 0.6f, u));
        DrawStickman(feet, h, t, StickPose::Talk, alpha);
        DrawSparkles({ feet.x, feet.y - 150 }, 150, t, alpha * 0.6f, 14);
        if (u > Intro::VANISH - Intro::TALK) // sparisce in uno sbuffo di scintille
            DrawSparkles({ feet.x, feet.y - 150 }, 60 + (u - (Intro::VANISH - Intro::TALK)) * 400, t, 1 - alpha, 24);

        int line = (int)((u - 0.8f) / Intro::LINE_TIME);
        if (u > 0.8f && line < Intro::LINES) {
            float lt = u - 0.8f - line * Intro::LINE_TIME;
            Rectangle bubble{ 520, 150, 660, 210 };
            DrawRectangleRounded(bubble, 0.2f, 10, Fade(WHITE, 0.95f));
            Tri({ 540, 300 }, { 600, 340 }, { 410, 400 }, Fade(WHITE, 0.95f)); // coda verso il GM
            DrawText("GameMaster", 548, 168, 20, { 200, 140, 30, 255 });
            DrawTextWrapped(GM_LINES[line], { 548, 204, 610, 150 }, 28, { 30, 30, 40, 255 }, (int)(lt * 40));
            for (int i = 0; i < Intro::LINES; ++i) // a che punto e' il discorso
                DrawCircle(1130 + i * 14 - Intro::LINES * 14, 340, 4, i <= line ? Color{ 200, 140, 30, 255 } : LIGHTGRAY);
        }
        if (t > Intro::END - 0.9f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, Smooth(Intro::END - 0.9f, Intro::END, t)));
    }
    DrawText("Invio / A: avanti     Esc / B: salta", 960, 16, 16, Fade(WHITE, 0.5f));
    EndCanvas();
}

// ---------------------------------------------------------------------------
// Risveglio
// ---------------------------------------------------------------------------

void DrawWakeUp(float t) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (t < WakeUp::ROOM) {
        // Il labirinto si tinge di rosso e sparisce nel buio
        float k = Clamp01(t / WakeUp::DARK);
        DrawRectangle(0, 0, sw, sh, Fade(Mix({ 160, 0, 0, 255 }, BLACK, k), 0.4f + 0.6f * k));
        return;
    }
    BeginCanvas();
    float sit = Smooth(WakeUp::SIT, WakeUp::SIT + 0.22f, t);
    sit += 0.08f * sinf(Clamp01((t - WakeUp::SIT) / 0.5f) * PI); // un piccolo rimbalzo nello scatto
    bool awake = t > WakeUp::SIT;
    Vector2 head = DrawBedroom(false, sit, awake, t, 0);
    if (!awake) DrawZzz(head, t, 1);
    if (awake && t < WakeUp::SIT + 1.4f) DrawText("!", (int)head.x + 10, (int)head.y - 90, 64, { 255, 220, 60, 255 });

    // Il GM saluta dalla finestra
    float gm = Smooth(WakeUp::GM, WakeUp::GM + 0.6f, t) * (1 - Smooth(WakeUp::END - 0.8f, WakeUp::END, t));
    if (gm > 0) {
        DrawStickman({ 250, 318 }, 120, t, StickPose::Wave, gm);
        DrawSparkles({ 250, 258 }, 50, t, gm * 0.6f);
    }
    if (t < WakeUp::ROOM + 0.5f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 1 - (t - WakeUp::ROOM) / 0.5f));
    if (t > WakeUp::GM + 0.2f) Caption("...o forse no.", t - WakeUp::GM - 0.2f);
    else Caption("Era solo un incubo...", t - WakeUp::SIT - 0.8f);
    EndCanvas();
}
