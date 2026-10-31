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
        case StickPose::Laugh: // si tiene la pancia dal ridere e saltella
            feet.y -= fabsf(sinf(t * 10)) * h * 0.04f;
            armL = -1.0f + 0.2f * sinf(t * 10); armR = 1.0f - 0.2f * sinf(t * 10);
            break;
        case StickPose::Crouch: // accucciato sul davanzale, a spiare
            legL = -1.1f; legR = 1.1f; armL = 0.9f; armR = 0.7f + 0.1f * sinf(t * 2);
            break;
    }
    if (pose == StickPose::Laugh || pose == StickPose::Crouch) { // ricalcola lo scheletro con le nuove pose
        head = { feet.x, feet.y - h + headR + (pose == StickPose::Crouch ? h * 0.25f : 0) };
        neck = { feet.x, head.y + headR };
        shoulder = { feet.x, neck.y + h * 0.06f };
        hip = { feet.x, pose == StickPose::Crouch ? feet.y - h * 0.22f : feet.y - h * 0.42f };
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
// La camera da letto (di notte nel menu e nell'intro, all'alba al risveglio)
// ---------------------------------------------------------------------------

enum Face { FACE_ASLEEP, FACE_SCARED, FACE_CALM, FACE_HAPPY };

// Il GM appostato fuori dalla finestra: si disegna dietro il vetro, prima della cornice
struct OutsideGM { float alpha = 0; float t = 0; };
static OutsideGM gOutside;

static constexpr Rectangle WINDOW_RECT{ 130, 120, 240, 210 };

// sit: 0 = sdraiato, 1 = seduto. lamp: abat-jour accesa (0..1). phone: sta guardando il telefono
static Vector2 DrawBedroom(bool night, float sit, Face face, float t, float headGlow, float lamp = 0, bool phone = false) {
    // Pareti e pavimento
    Color wallTop = night ? Color{ 28, 32, 62, 255 } : Color{ 150, 110, 120, 255 };
    Color wallBottom = night ? Color{ 18, 20, 40, 255 } : Color{ 110, 80, 90, 255 };
    DrawRectangleGradientV(0, 0, 1280, 560, wallTop, wallBottom);
    Color floorCol = night ? Color{ 52, 36, 30, 255 } : Color{ 120, 84, 60, 255 };
    DrawRectangle(0, 560, 1280, 160, floorCol);
    for (int k = 0; k < 6; ++k) DrawLine(0, 580 + k * 26, 1280, 580 + k * 26, Fade(BLACK, 0.2f));
    // Un poster e una mensola: e' la stanza di qualcuno
    DrawRectangle(1010, 150, 150, 200, night ? Color{ 50, 40, 70, 255 } : Color{ 200, 170, 150, 255 });
    DrawRectangleLinesEx({ 1010, 150, 150, 200 }, 4, Fade(BLACK, 0.3f));
    DrawCircle(1085, 230, 40, night ? Color{ 120, 60, 60, 255 } : Color{ 230, 90, 80, 255 });
    DrawRectangle(560, 210, 220, 10, { 90, 64, 48, 255 });
    for (int k = 0; k < 6; ++k) DrawRectangle(575 + k * 30, 160 + (k % 2) * 10, 20, 50 - (k % 2) * 10, Color{ (unsigned char)(80 + k * 25), 60, (unsigned char)(140 - k * 15), 255 });

    // Finestra: luna di notte, alba al risveglio
    const Rectangle win = WINDOW_RECT;
    if (night) {
        DrawRectangleGradientV((int)win.x, (int)win.y, (int)win.width, (int)win.height, { 10, 14, 40, 255 }, { 30, 40, 90, 255 });
        DrawCircle(300, 170, 26, { 240, 240, 220, 255 });
        for (int i = 0; i < 12; ++i) DrawPixel(140 + (int)(HashF(i, 1) * 220), 130 + (int)(HashF(i, 2) * 190), WHITE);
    } else {
        DrawRectangleGradientV((int)win.x, (int)win.y, (int)win.width, (int)win.height, { 120, 140, 210, 255 }, { 255, 180, 130, 255 });
        DrawCircle(250, 330, 40, { 255, 220, 150, 255 });
    }
    if (gOutside.alpha > 0) { // accucciato sul davanzale, sbircia dentro
        DrawStickman({ 300, win.y + win.height - 4 }, 120, gOutside.t, StickPose::Crouch, gOutside.alpha);
        DrawSparkles({ 300, win.y + win.height - 60 }, 40, gOutside.t, gOutside.alpha * 0.4f, 6);
    }
    DrawRectangleLinesEx(win, 8, { 90, 70, 60, 255 });
    DrawRectangle((int)(win.x + win.width / 2 - 3), (int)win.y, 6, (int)win.height, { 90, 70, 60, 255 });
    DrawRectangle((int)win.x, (int)(win.y + win.height / 2 - 3), (int)win.width, 6, { 90, 70, 60, 255 });

    // Comodino con la sveglia (3:33 di notte, 7:02 al risveglio) e l'abat-jour
    DrawRectangle(190, 470, 110, 90, { 96, 66, 48, 255 });
    DrawRectangle(196, 500, 98, 4, Fade(BLACK, 0.3f));
    DrawRectangle(205, 440, 50, 30, { 30, 30, 34, 255 });
    DrawText(night ? "03:33" : "07:02", 208, 447, 15, night ? Color{ 255, 70, 70, 255 } : Color{ 120, 255, 140, 255 });
    DrawRectangle(272, 420, 6, 50, { 70, 60, 50, 255 });
    Tri({ 255, 422 }, { 295, 422 }, { 284, 396 }, Mix({ 120, 100, 80, 255 }, { 255, 220, 150, 255 }, lamp));
    Tri({ 255, 422 }, { 284, 396 }, { 266, 396 }, Mix({ 120, 100, 80, 255 }, { 255, 220, 150, 255 }, lamp));

    // Letto
    Color wood{ 110, 72, 50, 255 };
    DrawRectangle(360, 470, 560, 52, wood);
    DrawRectangle(370, 522, 20, 38, wood);
    DrawRectangle(890, 522, 20, 38, wood);
    DrawRectangle(338, 370, 34, 190, Shade(wood, 0.85f)); // testiera
    DrawRectangle(905, 430, 30, 130, Shade(wood, 0.85f)); // pediera
    DrawRectangle(372, 452, 534, 26, { 235, 232, 225, 255 });
    DrawEllipse(430, 450, 52, 16, { 250, 250, 248, 255 });

    // Il malcapitato: si tira su ruotando intorno al bacino
    Vector2 hip{ 540, 452 };
    float angle = Lerp(-PI / 2 + 0.18f, -0.08f, Clamp01(sit)); // da sdraiato (testa a sinistra) a seduto
    Vector2 dir{ sinf(angle), -cosf(angle) };
    Vector2 neck = Vector2Add(hip, Vector2Scale(dir, 82));
    Vector2 head = Vector2Add(neck, Vector2Scale(dir, 24));
    if (face == FACE_SCARED) { // trema dallo spavento
        float shake = std::max(0.0f, 1.0f - (t - WakeUp::SIT) * 0.8f) * 3;
        head.x += sinf(t * 60) * shake;
        neck.x += sinf(t * 60) * shake * 0.5f;
    }
    Color pajama = night ? Color{ 70, 110, 170, 255 } : Color{ 90, 140, 200, 255 };
    DrawLineEx(hip, neck, 30, pajama);
    if (sit > 0.5f) { // braccia appoggiate sulla coperta, o il telefono in mano
        Vector2 hand{ neck.x + 60, phone ? hip.y - 40 : hip.y - 4 };
        DrawLineEx(Vector2Add(neck, { 0, 10 }), hand, 12, pajama);
        DrawLineEx(Vector2Add(neck, { 6, 12 }), { hand.x + 12, hand.y + 4 }, 12, Shade(pajama, 0.85f));
        if (phone) {
            DrawRectangleRounded({ hand.x - 4, hand.y - 30, 22, 36 }, 0.3f, 4, { 30, 30, 36, 255 });
            DrawRectangle((int)hand.x - 1, (int)hand.y - 27, 16, 28, { 150, 200, 255, 255 });
            DrawCircleV({ hand.x + 7, hand.y - 12 }, 60, Fade({ 150, 200, 255, 255 }, 0.08f)); // luce sul viso
        }
    }
    float blanketStart = Lerp(head.x + 20, hip.x - 20, Clamp01(sit));
    Color blanket = night ? Color{ 70, 86, 150, 255 } : Color{ 170, 100, 120, 255 };
    DrawRectangleRounded({ blanketStart, 430, 905 - blanketStart, 48 }, 0.5f, 8, blanket);
    for (int k = 0; k < 4; ++k)
        DrawLine((int)(blanketStart + 40 + k * 90), 438, (int)(blanketStart + 60 + k * 90), 474, Fade(BLACK, 0.15f));

    // Testa
    if (headGlow > 0) DrawCircleV(head, 30 + 30 * headGlow, Fade({ 255, 240, 200, 255 }, 0.35f * headGlow));
    Color skin{ 238, 204, 170, 255 };
    DrawCircleV(head, 24, skin);
    float hairDir = atan2f(dir.y, dir.x) * RAD2DEG; // capelli sulla nuca, dalla parte opposta al collo
    DrawCircleSector(head, 25, hairDir - 70, hairDir + 70, 12, { 50, 36, 30, 255 });
    Vector2 fc = Vector2Add(head, Vector2Scale({ dir.y, -dir.x }, -6)); // il viso guarda in avanti/verso l'alto
    const Color ink{ 60, 40, 30, 255 };
    switch (face) {
        case FACE_SCARED:
            for (float s : { -8.0f, 8.0f }) {
                DrawCircleV({ fc.x + s, fc.y - 2 }, 6, WHITE);
                DrawCircleV({ fc.x + s, fc.y - 2 }, 2.5f, BLACK);
            }
            DrawCircleV({ fc.x, fc.y + 10 }, 4, { 120, 50, 50, 255 });
            for (int k = 0; k < 2; ++k) { // sudore
                float ph = fmodf(t * 1.3f + k * 0.5f, 1.0f);
                Vector2 d{ head.x + 26 + k * 6, head.y - 18 + ph * 30 };
                DrawCircleV(d, 4, Fade({ 140, 200, 255, 255 }, 1 - ph));
                Tri({ d.x - 4, d.y }, { d.x + 4, d.y }, { d.x, d.y - 8 }, Fade({ 140, 200, 255, 255 }, 1 - ph));
            }
            break;
        case FACE_CALM: {
            bool blinkNow = fmodf(t, 3.2f) < 0.12f;
            for (float s : { -8.0f, 8.0f }) {
                if (blinkNow) DrawLineEx({ fc.x + s - 4, fc.y - 2 }, { fc.x + s + 4, fc.y - 2 }, 2, ink);
                else DrawCircleV({ fc.x + s, fc.y - 2 }, 2.5f, ink);
            }
            DrawLineEx({ fc.x - 5, fc.y + 9 }, { fc.x + 5, fc.y + 9 }, 2, ink);
            break;
        }
        case FACE_HAPPY:
            for (float s : { -8.0f, 8.0f }) DrawCircleSector({ fc.x + s, fc.y }, 4, 180, 360, 6, ink); // occhi sorridenti
            DrawCircleSector({ fc.x, fc.y + 6 }, 7, 0, 180, 8, { 150, 60, 60, 255 });
            break;
        default:
            DrawLineEx({ fc.x - 12, fc.y - 4 }, { fc.x - 4, fc.y - 2 }, 2, ink); // occhi chiusi
            DrawLineEx({ fc.x + 4, fc.y - 2 }, { fc.x + 12, fc.y - 4 }, 2, ink);
            break;
    }
    // Di notte la stanza e' buia, tranne il cerchio di luce dell'abat-jour
    if (night) {
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.45f * (1 - lamp)));
        if (lamp > 0) DrawCircleGradient(278, 420, 220, Fade({ 255, 210, 140, 255 }, 0.18f * lamp), BLANK);
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
// Il sogno: un giro del mondo in cartolina, poi il labirinto che precipita
// ---------------------------------------------------------------------------

struct Slab { float x, w, h, land; }; // un muro del labirinto: posizione, misure, istante in cui tocca terra
static const Slab SLABS[] = {
    { 90, 150, 380, 0.9f }, { 400, 130, 440, 1.25f }, { 660, 180, 360, 1.6f }, { 930, 140, 420, 1.95f }, { 1150, 160, 390, 2.3f },
};

// I monumenti del sogno, in fila sull'orizzonte: torii, Colosseo, Tour Eiffel, Big Ben, Liberta', Sagrada...
static void DrawLandmarks(float gloom, float t) {
    Color sil = Mix({ 70, 46, 80, 255 }, { 30, 26, 36, 255 }, gloom);
    // Pagoda (Giappone)
    for (int k = 0; k < 5; ++k) {
        float w = 120 - k * 18, y = 560 - k * 38;
        DrawRectangle((int)(130 - w * 0.3f), (int)(y - 28), (int)(w * 0.6f), 28, sil);
        Tri({ 130 - w / 2, y - 26 }, { 130 + w / 2, y - 26 }, { 130, y - 44 }, sil);
    }
    // Colosseo (Roma)
    for (int k = 0; k < 3; ++k) {
        float y0 = 560 - (k + 1) * 34.0f, w = k == 2 ? 120.0f : 200.0f;
        DrawRectangle(250, (int)y0, (int)w, 34, sil);
        for (int a = 0; a < (int)(w / 20); ++a) DrawRectangle(256 + a * 20, (int)y0 + 8, 10, 20, Mix(sil, { 200, 140, 120, 255 }, 0.35f * (1 - gloom)));
    }
    // Tour Eiffel (Parigi)
    Tri({ 500, 560 }, { 560, 330 }, { 530, 560 }, sil);
    Tri({ 620, 560 }, { 560, 330 }, { 590, 560 }, sil);
    DrawRectangle(515, 470, 90, 8, sil);
    Tri({ 545, 340 }, { 575, 340 }, { 560, 230 }, sil);
    // Big Ben (Londra)
    DrawRectangle(690, 340, 44, 220, sil);
    DrawCircleV({ 712, 370 }, 15, Mix({ 250, 230, 170, 255 }, sil, gloom));
    Tri({ 690, 340 }, { 734, 340 }, { 712, 280 }, sil);
    // Statua della Liberta' (New York)
    DrawRectangle(800, 470, 60, 90, sil);
    DrawRectangle(816, 390, 28, 80, sil);
    DrawRectangle(836, 350, 7, 45, sil);
    DrawCircleV({ 840, 348 }, 6, Mix({ 255, 220, 120, 255 }, sil, gloom));
    // Frauenkirche (Monaco) e Giralda (Siviglia)
    for (float x : { 920.0f, 970.0f }) { DrawRectangle((int)x, 400, 34, 160, sil); DrawCircleV({ x + 17, 395 }, 19, sil); }
    DrawRectangle(1070, 330, 40, 230, sil);
    Tri({ 1070, 330 }, { 1110, 330 }, { 1090, 290 }, sil);
    // Vesuvio (Napoli) sullo sfondo, con un filo di fumo
    Tri({ 1120, 560 }, { 1300, 560 }, { 1230, 420 }, Mix(sil, { 120, 90, 110, 255 }, 0.4f));
    for (int k = 0; k < 3; ++k)
        DrawCircleV({ 1230 + sinf(t + k) * 8 + k * 8, 410 - k * 18.0f }, 8 + k * 5.0f, Fade(Mix(sil, WHITE, 0.4f), 0.35f));
}

static void DrawDream(float t, float fall) {
    float shake = 0; // scossa quando i muri toccano terra
    for (const Slab& s : SLABS) {
        float since = fall - s.land;
        if (since > 0 && since < 0.6f) shake = std::max(shake, (0.6f - since) * 16);
    }
    rlPushMatrix();
    rlTranslatef(sinf(t * 70) * shake, cosf(t * 53) * shake, 0);

    float gloom = Smooth(0.8f, 3.0f, fall); // il labirinto oscura il cielo
    DrawRectangleGradientV(-20, -20, 1320, 640, Mix({ 70, 46, 120, 255 }, { 40, 34, 60, 255 }, gloom),
                           Mix({ 245, 150, 110, 255 }, { 120, 90, 100, 255 }, gloom));
    DrawCircle(640, 300, 90, Fade({ 255, 210, 150, 255 }, 0.3f));
    DrawCircle(640, 300, 62, Mix({ 255, 225, 160, 255 }, { 200, 160, 130, 255 }, gloom));
    DrawLandmarks(gloom, t);

    // Case in primo piano: quelle sotto un muro caduto si schiacciano
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
    DrawRectangle(-20, 600, 1320, 140, Mix({ 90, 72, 82, 255 }, { 50, 46, 52, 255 }, gloom));
    for (int k = 0; k < 8; ++k) DrawRectangle(40 + k * 170, 660, 80, 6, Fade(WHITE, 0.5f));

    // Coriandoli di cartoline che volano (finche' non arriva il labirinto)
    static const Color paper[] = { { 255, 200, 215, 255 }, { 200, 230, 255, 255 }, { 255, 240, 180, 255 } };
    for (int i = 0; i < 40; ++i) {
        float ph = fmodf(t * (0.12f + HashF(i, 8) * 0.1f) + HashF(i, 9), 1.0f);
        float x = HashF(i, 10) * 1400 - 60 + ph * 120 + sinf(t * 2 + i) * 15, y = -20 + ph * 680;
        DrawEllipse((int)x, (int)y, 4, 2, Fade(paper[i % 3], 1 - gloom));
    }

    for (const Slab& s : SLABS) { // i muri del labirinto: cadono accelerando, poi polvere
        float start = s.land - 0.8f;
        if (fall < start) continue;
        float k = Clamp01((fall - start) / 0.8f);
        float bottom = Lerp(-40, 610, k * k);
        Rectangle r{ s.x, bottom - s.h, s.w, s.h };
        DrawRectangleRec(r, { 104, 106, 116, 255 });
        for (int row = 0; row * 34 < s.h; ++row) {
            float y = r.y + row * 34;
            DrawLine((int)r.x, (int)y, (int)(r.x + r.width), (int)y, { 70, 72, 80, 255 });
            for (float bx = r.x + (row % 2 ? 30 : 60); bx < r.x + r.width; bx += 60)
                DrawLine((int)bx, (int)y, (int)bx, (int)std::min(y + 34, r.y + r.height), { 70, 72, 80, 255 });
        }
        DrawRectangleLinesEx(r, 3, { 60, 62, 70, 255 });
        float since = fall - s.land;
        if (since > 0 && since < 2.5f)
            for (int p = 0; p < 7; ++p) {
                float spread = since * 90 * (0.6f + HashF(p, (int)s.x) * 0.6f);
                DrawCircleV({ s.x + s.w / 2 + (p - 3) * spread * 0.45f, 610 - HashF(p, 11) * 30 - since * 12 },
                            20 + since * 30, Fade({ 180, 170, 160, 255 }, 0.45f * (1 - since / 2.5f)));
            }
    }
    rlPopMatrix();
}

// ---------------------------------------------------------------------------
// Menu iniziale: dal cielo alla finestra
// ---------------------------------------------------------------------------

static void DrawSpace(float t, float k) {
    DrawRectangleGradientV(0, 0, 1280, 720, { 2, 2, 10, 255 }, { 10, 16, 44, 255 });
    for (int i = 0; i < 160; ++i) {
        float tw = 0.5f + 0.5f * sinf(t * 2 + i);
        DrawCircleV({ HashF(i, 1) * 1280, HashF(i, 2) * 560 }, 1 + HashF(i, 3) * 1.2f, Fade(WHITE, 0.3f + 0.7f * tw * HashF(i, 4)));
    }
    Vector2 c{ 640, 720 + 760 - k * 200 }; // la Terra si avvicina
    DrawCircleV(c, 980, Fade({ 120, 180, 255, 255 }, 0.2f));
    DrawCircleV(c, 950, { 36, 90, 170, 255 });
    for (int i = 0; i < 6; ++i)
        DrawEllipse((int)(200 + i * 190 + HashF(i, 5) * 60), (int)(c.y - 920 + HashF(i, 6) * 60), 70 + HashF(i, 7) * 50, 22, { 70, 140, 80, 255 });
}

// La via di notte, la casa con una finestra accesa: il GM scende e si posa sul davanzale
static void DrawStreet(float u) {
    DrawRectangleGradientV(0, 0, 1280, 720, { 8, 10, 30, 255 }, { 30, 34, 70, 255 });
    for (int i = 0; i < 60; ++i) DrawPixel((int)(HashF(i, 21) * 1280), (int)(HashF(i, 22) * 300), Fade(WHITE, 0.6f));
    DrawCircle(1100, 110, 30, { 240, 240, 220, 255 });
    for (int i = 0; i < 6; ++i) { // le altre case, tutte addormentate
        float x = i * 230 - 40.0f, h = 220 + HashF(i, 23) * 80;
        DrawRectangle((int)x, (int)(620 - h), 200, (int)h, { 30, 30, 50, 255 });
        Tri({ x - 10, 620 - h }, { x + 210, 620 - h }, { x + 100, 560 - h }, { 24, 24, 40, 255 });
    }
    // La casa del malcapitato, con la finestra accesa al primo piano
    DrawRectangle(460, 220, 360, 400, { 70, 60, 70, 255 });
    Tri({ 440, 220 }, { 840, 220 }, { 640, 120 }, { 90, 50, 50, 255 });
    DrawRectangle(560, 290, 160, 130, { 255, 214, 140, 255 });
    DrawRectangleLinesEx({ 560, 290, 160, 130 }, 6, { 50, 40, 40, 255 });
    DrawRectangle(637, 290, 6, 130, { 50, 40, 40, 255 });
    DrawRectangle(545, 420, 190, 12, { 60, 50, 50, 255 }); // davanzale
    DrawRectangle(600, 500, 80, 120, { 60, 40, 30, 255 });  // porta
    DrawRectangle(-20, 620, 1320, 120, { 26, 26, 32, 255 });
    // Il GM scende dall'alto e si accuccia sul davanzale
    float k = Smooth(0.0f, 2.6f, u);
    Vector2 feet{ Lerp(900, 600, k), Lerp(-60, 420, k) };
    bool landed = u > 2.6f;
    DrawStickman(feet, landed ? 70 : 90, u, landed ? StickPose::Crouch : StickPose::Float, 1);
    DrawSparkles({ feet.x, feet.y - 40 }, 40, u, landed ? 0.3f : 0.8f);
}

void DrawTitleScene(float t) {
    BeginCanvas();
    if (t < TitleScene::STREET) {
        float k = Smooth(0.3f, TitleScene::STREET, t);
        DrawSpace(t, k);
        Vector2 feet{ 640, Lerp(-40, 520, k) };
        float h = 150 * (1 - k * 0.7f);
        for (int i = 1; i <= 6; ++i) DrawCircleV({ feet.x, feet.y - h * 0.5f - i * 22 }, 10 - i, Fade({ 255, 240, 200, 255 }, 0.25f));
        DrawStickman(feet, h, t, StickPose::Float, 1);
        DrawSparkles({ feet.x, feet.y - h * 0.5f }, 60 + h * 0.2f, t, 0.8f);
        Caption("Lassu', il GameMaster si annoiava da morire.", t - 0.6f);
        if (t > TitleScene::STREET - 0.5f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, Smooth(TitleScene::STREET - 0.5f, TitleScene::STREET, t)));
    } else if (t < TitleScene::MENU - 0.6f) {
        float u = t - TitleScene::STREET;
        DrawStreet(u);
        Caption("Cosi', una notte, scese sulla terra in cerca di qualcuno con cui giocare.", u - 0.3f);
        if (u < 0.5f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 1 - u / 0.5f));
    } else {
        // Dentro la stanza: il malcapitato e' ancora sveglio col telefono, il GM lo spia dalla finestra
        gOutside = { 1, t };
        DrawBedroom(true, 1, FACE_CALM, t, 0, 1, true);
        gOutside.alpha = 0;
        float u = t - (TitleScene::MENU - 0.6f);
        if (u < 0.6f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 1 - u / 0.6f));
        // Il titolo del gioco
        DrawText("Project", 640, 60, 40, Fade(WHITE, 0.8f));
        DrawText("ASOBI", 640, 96, 84, { 255, 214, 100, 255 });
        DrawText("il gioco del GameMaster", 646, 182, 20, Fade(WHITE, 0.6f));
    }
    if (t < TitleScene::MENU) DrawText("Invio / A: salta", 1100, 16, 16, Fade(WHITE, 0.5f));
    EndCanvas();
}

Rectangle TitleMenuArea() {
    // In coordinate di schermo: la parte destra della tela virtuale
    float s = std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
    float ox = (GetScreenWidth() - 1280 * s) / 2, oy = (GetScreenHeight() - 720 * s) / 2;
    return { ox + 640 * s, oy + 230 * s, 560 * s, 470 * s };
}

// ---------------------------------------------------------------------------
// Intro: si addormenta, il GM entra nel sogno, il giro del mondo, il labirinto, il discorso
// ---------------------------------------------------------------------------

static const char* GM_LINES[Intro::LINES] = {
    "Ciao! Io sono il GameMaster. Mi annoiavo, e tu avevi l'aria di uno che fa dei bei sogni.",
    "Ho fatto cadere il mio labirinto sulle tue citta'. Per passare al piano dopo, cerca il torii.",
    "Per strada troverai armi e artefatti. Lo zaino pero' e' piccolo: scegli bene cosa tenerti.",
    "E ho qualche sfida per te. Se riesci a farmi divertire, ti lascio svegliare da vincitore.",
    "Se cadi, ti svegli e si ricomincia da capo. Pronto? Allora si va!",
};

float Intro::NextStop(float t) {
    const float stops[] = { ENTER, DREAM, FALL, TALK + 0.8f };
    for (float s : stops) if (t < s - 0.2f) return s;
    float line = floorf((t - (TALK + 0.8f)) / LINE_TIME) + 1; // durante il discorso: alla battuta successiva
    return std::min(END, TALK + 0.8f + line * LINE_TIME);
}

// Sbadiglia, spegne la luce, si sdraia; il GM passa attraverso il vetro e si infila nella sua testa
static void DrawSleepScene(float t) {
    float lamp = 1 - Smooth(1.6f, 1.9f, t);
    float sit = 1 - Smooth(2.0f, 3.2f, t);
    float glow = Smooth(Intro::ENTER + 4.2f, Intro::ENTER + 5.0f, t) * (0.7f + 0.3f * sinf(t * 6));
    Face face = t < 1.4f ? FACE_CALM : FACE_ASLEEP;
    gOutside = { t < Intro::ENTER ? 1.0f : 0.0f, t };
    Vector2 head = DrawBedroom(true, sit, face, t, glow, lamp, t < 1.2f);
    gOutside.alpha = 0;
    if (t > 3.2f && t < Intro::ENTER + 4.4f) DrawZzz(head, t, 1);

    if (t >= Intro::ENTER && t < Intro::ENTER + 4.6f) {
        float u = t - Intro::ENTER;
        Vector2 window{ 300, WINDOW_RECT.y + WINDOW_RECT.height - 4 };
        Vector2 above{ head.x + 70, head.y - 80 };
        Vector2 pos = Vector2Lerp(window, above, Smooth(0.2f, 2.4f, u));
        float h = 140;
        if (u > 2.6f) { // si rimpicciolisce e scivola dentro la testa
            float k = Smooth(2.6f, 4.2f, u);
            pos = Vector2Lerp(above, { head.x, head.y + 20 }, k);
            h = 140 * (1 - k);
        }
        DrawStickman(pos, h, t, StickPose::Float, 1);
        DrawSparkles({ pos.x, pos.y - h * 0.5f }, 40 + h * 0.3f, t, 0.6f);
    }
    if (t > Intro::DREAM - 0.9f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, Smooth(Intro::DREAM - 0.9f, Intro::DREAM, t)));
    if (t < 1.4f) Caption("Si era fatto tardi. Il povero malcapitato sbadiglio' e spense la luce...", t);
    else if (t > Intro::ENTER + 0.2f) Caption("...e il GameMaster ne approfitto' per infilarsi nei suoi sogni.", t - Intro::ENTER - 0.2f);
}

void DrawIntro(float t, bool newRun) {
    BeginCanvas();
    if (t < Intro::DREAM) {
        DrawSleepScene(t);
    } else if (!newRun) {
        // Si riprende un salvataggio: il labirinto e' gia' li' che aspetta
        DrawDream(t, 99);
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.45f));
        float u = t - Intro::DREAM;
        if (u < 0.8f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, 1 - u / 0.8f));
        DrawStickman({ 320, 640 }, 260, t, StickPose::Wave, Smooth(0, 0.6f, u) * (1 - Smooth(2.2f, 2.8f, u)));
        Caption("\"Bentornato! Ti ho tenuto il posto. Si riparte dall'inizio del piano.\"", u - 0.3f);
        if (u > 2.2f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, Smooth(2.2f, 3.0f, u)));
    } else if (t < Intro::TALK) {
        float fall = t - Intro::FALL;
        DrawDream(t, fall);
        if (t < Intro::DREAM + 0.8f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, 1 - (t - Intro::DREAM) / 0.8f));
        if (t < Intro::FALL) Caption("Sognava di girare il mondo: Kyoto, Roma, Parigi, Londra, New York...", t - Intro::DREAM - 0.6f);
        else Caption("...finche' dal cielo non gli piombo' addosso un labirinto.", t - Intro::FALL - 0.2f);
    } else {
        // Il GM appare, spiega e sparisce
        DrawDream(t, 99);
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.45f));
        float u = t - Intro::TALK;
        float vanish = Intro::VANISH - Intro::TALK;
        float alpha = Smooth(0, 0.8f, u) * (1 - Smooth(vanish, vanish + 0.6f, u));
        Vector2 feet{ 320, 640 };
        float h = 300 * (1 + 0.6f * Smooth(vanish, vanish + 0.6f, u));
        DrawStickman(feet, h, t, StickPose::Talk, alpha);
        DrawSparkles({ feet.x, feet.y - 150 }, 150, t, alpha * 0.6f, 14);
        if (u > vanish) DrawSparkles({ feet.x, feet.y - 150 }, 60 + (u - vanish) * 400, t, 1 - alpha, 24);

        int line = (int)((u - 0.8f) / Intro::LINE_TIME);
        if (u > 0.8f && line < Intro::LINES) {
            float lt = u - 0.8f - line * Intro::LINE_TIME;
            Rectangle bubble{ 520, 150, 660, 210 };
            DrawRectangleRounded(bubble, 0.2f, 10, Fade(WHITE, 0.95f));
            Tri({ 540, 300 }, { 600, 340 }, { 410, 400 }, Fade(WHITE, 0.95f)); // coda verso il GM
            DrawText("GameMaster", 548, 168, 20, { 200, 140, 30, 255 });
            DrawTextWrapped(GM_LINES[line], { 548, 204, 610, 150 }, 28, { 30, 30, 40, 255 }, (int)(lt * 40));
            for (int i = 0; i < Intro::LINES; ++i)
                DrawCircle(1130 + i * 14 - Intro::LINES * 14, 340, 4, i <= line ? Color{ 200, 140, 30, 255 } : LIGHTGRAY);
        }
        if (t > Intro::END - 0.9f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, Smooth(Intro::END - 0.9f, Intro::END, t)));
    }
    DrawText("Invio / A: avanti     Esc / B: salta", 960, 16, 16, Fade(WHITE, 0.5f));
    EndCanvas();
}

// ---------------------------------------------------------------------------
// Risveglio (morte) e vittoria
// ---------------------------------------------------------------------------

void DrawWakeUp(float t) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (t < WakeUp::ROOM) { // il labirinto si tinge di rosso e sparisce nel buio
        float k = Clamp01(t / WakeUp::DARK);
        DrawRectangle(0, 0, sw, sh, Fade(Mix({ 160, 0, 0, 255 }, BLACK, k), 0.4f + 0.6f * k));
        return;
    }
    BeginCanvas();
    float sit = Smooth(WakeUp::SIT, WakeUp::SIT + 0.22f, t);
    sit += 0.08f * sinf(Clamp01((t - WakeUp::SIT) / 0.5f) * PI); // un piccolo rimbalzo nello scatto
    bool awake = t > WakeUp::SIT;
    Vector2 head = DrawBedroom(false, sit, awake ? FACE_SCARED : FACE_ASLEEP, t, 0);
    if (!awake) DrawZzz(head, t, 1);
    if (awake && t < WakeUp::SIT + 1.4f) DrawText("!", (int)head.x + 10, (int)head.y - 90, 64, { 255, 220, 60, 255 });

    float gm = Smooth(WakeUp::GM, WakeUp::GM + 0.6f, t) * (1 - Smooth(WakeUp::END - 0.8f, WakeUp::END, t));
    if (gm > 0) { // il GM saluta dalla finestra
        DrawStickman({ 250, 318 }, 120, t, StickPose::Wave, gm);
        DrawSparkles({ 250, 258 }, 50, t, gm * 0.6f);
    }
    if (t < WakeUp::ROOM + 0.5f) DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 1 - (t - WakeUp::ROOM) / 0.5f));
    if (t > WakeUp::GM + 0.2f) Caption("\"A stanotte!\"", t - WakeUp::GM - 0.2f);
    else Caption("Uff... era solo un brutto sogno.", t - WakeUp::SIT - 0.8f);
    EndCanvas();
}

void DrawVictoryScene(float t, const char* title) {
    BeginCanvas();
    if (t < VictoryScene::WAKE) {
        // Nel sogno: il labirinto si sbriciola e il GM ride a crepapelle
        DrawDream(t, 99);
        DrawRectangle(0, 0, 1280, 720, Fade(BLACK, 0.35f));
        static const Color confetti[] = { { 255, 90, 90, 255 }, { 255, 220, 80, 255 }, { 120, 220, 255, 255 }, { 140, 255, 140, 255 } };
        for (int i = 0; i < 80; ++i) {
            float ph = fmodf(t * (0.2f + HashF(i, 31) * 0.2f) + HashF(i, 32), 1.0f);
            DrawRectangle((int)(HashF(i, 33) * 1280 + sinf(t * 3 + i) * 20), (int)(-20 + ph * 760), 8, 4, confetti[i % 4]);
        }
        DrawStickman({ 640, 620 }, 300, t, StickPose::Laugh, Smooth(0, 0.6f, t));
        DrawSparkles({ 640, 470 }, 180, t, 0.7f, 16);
        if (t < 3) Caption("\"Ahahah! Era da secoli che non mi divertivo cosi'!\"", t - 0.3f);
        else Caption(TextFormat("\"Te lo sei meritato: d'ora in poi ti chiamero' %s.\"", title), t - 3.0f);
        if (t > VictoryScene::WAKE - 0.8f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, Smooth(VictoryScene::WAKE - 0.8f, VictoryScene::WAKE, t)));
    } else {
        // Al mattino si sveglia col sorriso, e il GM lo saluta dalla finestra
        float u = t - VictoryScene::WAKE;
        Vector2 head = DrawBedroom(false, Smooth(0.6f, 1.2f, u), u > 0.6f ? FACE_HAPPY : FACE_ASLEEP, t, 0);
        if (u < 0.6f) DrawZzz(head, t, 1);
        DrawStickman({ 250, 318 }, 120, t, StickPose::Wave, Smooth(1.2f, 1.8f, u));
        if (u < 0.6f) DrawRectangle(0, 0, 1280, 720, Fade(WHITE, 1 - u / 0.6f));
        Caption("Che sogno strano... ma che bello.", u - 0.8f);
    }
    EndCanvas();
}
