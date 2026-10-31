// Disegno 3D dei nemici: forme semplici (sfere, cilindri, cubi) colorate e girate dove guardano
#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include "raymath.h"
#include "rlgl.h"

using E = EnemyType;

float EnemyBodyY(const Enemy& e) {
    float r = e.radius / TILE;
    switch (e.type) {
        case E::Tengu:      return 0.55f + 0.05f * sinf((float)GetTime() * 3 + e.id);
        case E::Oni:        return r * 1.05f;
        case E::Robot:      return r * 1.2f;
        case E::Goblin:     return e.windup > 0 ? r * 1.35f : r; // si alza sulle zampe mentre carica il morso
        case E::Mothman:    return 0.5f + e.z;
        case E::Grossmann:  return 0.9f;
        case E::SpringJack: return r * 1.6f + e.z;
        case E::Wendigo:    return r * 1.4f + e.z;
        case E::Compana:    return r * 1.4f;
        default:            return r + e.z;
    }
}

void Game::DrawEnemy(const Enemy& e) const {
    const District& d = CurrentDistrict();
    float dist = Vector2Distance(e.pos, player.pos) / TILE;
    if (dist > VIEW_DIST) return;
    auto fog = [&](Color c) { return e.hitFlash > 0 ? WHITE : ApplyFog(c, d.horizon, dist); };

    float t = (float)GetTime();
    float r = e.radius / TILE;
    Vector2 dir = e.facing; // guarda dove sta guardando davvero: alle spalle si puo' passare inosservati
    Vector3 fwd{ dir.x, 0, dir.y }, side{ -dir.y, 0, dir.x }, up{ 0, 1, 0 };
    Vector3 center = To3D(e.pos, EnemyBodyY(e));
    if (e.doom > 0) center.x += sinf(t * 60) * 0.02f; // il goblin condannato trema
    Vector3 top = Vector3Add(center, Vector3Scale(up, r * 0.8f));
    bool blink = e.windup > 0 && fmodf(t, 0.16f) < 0.08f;
    Color eyes = BLACK;

    // Guardiano potenziato dal GameMaster: un alone dorato
    if (e.empowered) DrawCircle3D(To3D(e.pos, 0.02f), r * 1.6f + 0.05f * sinf(t * 5), { 1, 0, 0 }, 90, GOLD);

    if (e.type > E::Oni) {
        DrawLegend(e, center, r, fwd, side, dist);
        return;
    }

    // Preavvisi dell'oni sul pavimento: dove carichera' e fin dove arriva lo schianto
    if (e.type == E::Oni && e.windup > 0) {
        Vector3 ground = To3D(e.pos, 0.03f);
        if (e.attack == EnemyAttack::Charge) {
            Vector3 end = Vector3Add(ground, { e.attackDir.x * 8.0f, 0, e.attackDir.y * 8.0f });
            DrawCylinderEx(ground, end, 0.12f, 0.12f, 4, Fade(RED, 0.55f));
        } else if (e.attack == EnemyAttack::Slam) {
            for (float k : { 1.0f, 0.97f, 0.94f })
                DrawCircle3D(ground, ONI_SLAM_RADIUS / TILE * k, { 1, 0, 0 }, 90, Fade(RED, blink ? 0.9f : 0.5f));
        }
    }

    Color body{};
    switch (e.type) {
        case E::Goblin: {
            body = e.doom > 0 ? Color{ 70, 40, 80, 255 } : Color{ 96, 142, 58, 255 };
            if (e.power > 1.01f) body = Mix(body, { 150, 60, 40, 255 }, std::min(1.0f, (e.power - 1) * 0.5f));
            for (float s : { -1.0f, 1.0f }) { // orecchie a punta
                Vector3 base = Vector3Add(center, Vector3Add(Vector3Scale(side, s * r * 0.75f), Vector3Scale(up, r * 0.35f)));
                Vector3 tip = Vector3Add(base, Vector3Add(Vector3Scale(side, s * r * 0.9f), Vector3Scale(up, r * 0.45f)));
                DrawCylinderEx(base, tip, r * 0.22f, 0.01f, 5, fog({ 120, 160, 80, 255 }));
            }
            eyes = e.windup > 0 ? Color{ 255, 230, 60, 255 } : Color{ 220, 200, 40, 255 };
            break;
        }
        case E::Robot: {
            // Corpo squadrato, girato dove guarda: si disegna da solo e non usa la sfera
            Color stripe = e.role == RobotRole::Gunner ? Color{ 60, 200, 230, 255 } : Color{ 235, 135, 40, 255 };
            Color visor = e.windup > 0 ? (blink ? WHITE : RED) : Color{ 255, 70, 60, 255 };
            rlPushMatrix();
            rlTranslatef(center.x, center.y, center.z);
            rlRotatef(atan2f(-dir.y, dir.x) * RAD2DEG, 0, 1, 0);
            DrawCube({ 0, 0, 0 }, r * 1.5f, r * 1.6f, r * 1.4f, fog({ 140, 146, 158, 255 }));
            DrawCubeWires({ 0, 0, 0 }, r * 1.5f, r * 1.6f, r * 1.4f, fog({ 70, 74, 84, 255 }));
            DrawCube({ 0, -r * 0.2f, 0 }, r * 1.55f, r * 0.25f, r * 1.45f, fog(stripe));
            DrawCube({ 0, r * 1.05f, 0 }, r * 0.9f, r * 0.55f, r * 0.9f, fog({ 120, 126, 138, 255 }));
            DrawCube({ r * 0.46f, r * 1.05f, 0 }, r * 0.08f, r * 0.18f, r * 0.7f, e.hitFlash > 0 ? WHITE : visor);
            DrawCylinderEx({ 0, r * 1.3f, 0 }, { 0, r * 1.9f, 0 }, 0.012f, 0.012f, 3, fog(DARKGRAY));
            DrawSphereEx({ 0, r * 1.95f, 0 }, 0.03f, 4, 4, stripe);
            if (e.role == RobotRole::Gunner)
                DrawCylinderEx({ r * 0.2f, 0, r * 0.85f }, { r * 1.1f, 0, r * 0.85f }, r * 0.16f, r * 0.16f, 6, fog(DARKGRAY));
            else
                for (float s : { -1.0f, 1.0f })
                    DrawCube({ r * 0.8f, -r * 0.1f, s * r * 0.8f }, r * 0.9f, r * 0.08f, r * 0.12f, fog(LIGHTGRAY));
            rlPopMatrix();
            if (e.burnTimer > 0)
                DrawSphereEx(Vector3Add(top, { 0, 0.25f, 0 }), 0.05f + 0.02f * sinf(t * 20), 4, 4, ORANGE);
            return;
        }
        case E::Tengu:
            body = { 200, 50, 50, 255 };
            DrawCylinderEx(Vector3Add(center, Vector3Scale(fwd, r * 0.8f)), Vector3Add(center, Vector3Scale(fwd, r + 0.25f)),
                           r * 0.18f, 0.01f, 6, fog({ 230, 90, 80, 255 })); // il naso lungo
            break;
        case E::Oni:
            body = e.enraged ? Color{ 170, 50, 60, 255 } : Color{ 110, 70, 160, 255 };
            for (float s : { -1.0f, 1.0f }) {
                Vector3 baseHorn = Vector3Add(top, Vector3Scale(side, s * r * 0.45f));
                DrawCylinderEx(baseHorn, Vector3Add(baseHorn, { side.x * s * 0.06f, 0.28f, side.z * s * 0.06f }),
                               r * 0.16f, 0.01f, 6, fog({ 240, 230, 210, 255 }));
            }
            {
                Vector3 hand = Vector3Add(center, Vector3Add(Vector3Scale(side, r * 1.05f), Vector3Scale(fwd, r * 0.3f)));
                Vector3 club = e.windup > 0 && e.attack == EnemyAttack::Swing
                                   ? Vector3Add(hand, { 0, 0.55f, 0 })
                                   : Vector3Add(hand, Vector3Add(Vector3Scale(fwd, 0.45f), { 0, 0.1f, 0 }));
                DrawCylinderEx(hand, club, 0.04f, 0.09f, 6, fog({ 90, 70, 50, 255 }));
            }
            eyes = e.enraged ? Color{ 255, 220, 60, 255 } : BLACK;
            if (e.stun > 0)
                for (int k = 0; k < 3; ++k) {
                    float a = t * 5 + k * 2.1f;
                    DrawSphereEx(Vector3Add(top, { cosf(a) * 0.3f, 0.3f, sinf(a) * 0.3f }), 0.04f, 4, 4, YELLOW);
                }
            break;
        default: break;
    }
    if (blink && e.type != E::Goblin) body = Mix(body, RED, 0.6f);
    DrawSphereEx(center, r, 10, 10, fog(body));
    for (float s : { -1.0f, 1.0f }) { // occhi: guardano dove guarda lui
        Vector3 eye = Vector3Add(Vector3Add(center, Vector3Scale(fwd, r * 0.88f)),
                                 Vector3Add(Vector3Scale(side, s * r * 0.32f), Vector3Scale(up, r * 0.2f)));
        DrawSphereEx(eye, r * 0.12f, 4, 4, eyes);
    }
    if (e.burnTimer > 0) DrawSphereEx(Vector3Add(top, { 0, 0.15f, 0 }), 0.05f + 0.02f * sinf(t * 20), 4, 4, ORANGE);
}

// Le creature leggendarie
void Game::DrawLegend(const Enemy& e, Vector3 c, float r, Vector3 fwd, Vector3 side, float dist) const {
    const District& d = CurrentDistrict();
    const float t = (float)GetTime();
    const float a = e.alpha;
    auto fog = [&](Color col) { return Fade(e.hitFlash > 0 ? WHITE : ApplyFog(col, d.horizon, dist), a); };
    auto glow = [&](Color col) { return e.hitFlash > 0 ? WHITE : ApplyFog(col, d.horizon, dist * 0.4f); }; // occhi luminosi
    auto at = [&](float f, float s, float u) { // punto relativo al corpo: avanti, lato, su
        return Vector3Add(c, Vector3Add(Vector3Scale(fwd, f), Vector3Add(Vector3Scale(side, s), { 0, u, 0 })));
    };
    auto eyesPair = [&](float f, float s, float u, float size, Color col) {
        for (float k : { -1.0f, 1.0f }) DrawSphereEx(at(f, s * k, u), size, 4, 4, col);
    };
    auto legs = [&](float spread, float len, float thick, Color col, float shortLeft = 1) {
        for (float fs : { -1.0f, 1.0f })
            for (float ss : { -1.0f, 1.0f }) {
                Vector3 hip = at(fs * spread, ss * spread * 0.7f, -r * 0.4f);
                float l = len * (ss < 0 ? shortLeft : 1);
                DrawCylinderEx(hip, { hip.x, std::max(0.0f, hip.y - l), hip.z }, thick, thick * 0.8f, 5, col);
            }
    };
    auto wing = [&](float s, float span, float flap, Color col) {
        Vector3 root = at(-r * 0.2f, s * r * 0.4f, r * 0.2f);
        Vector3 tip = at(-r * 0.5f, s * span, r * 0.2f + flap);
        Vector3 back = at(-r * 1.2f, s * span * 0.6f, -r * 0.3f + flap * 0.5f);
        DrawTriangle3D(root, tip, back, col);
        DrawTriangle3D(root, back, tip, col);
    };
    bool blink = e.windup > 0 && fmodf(t, 0.16f) < 0.08f;

    switch (e.type) {
        case E::Wendigo: { // scheletrico e pallido, teschio di cervo con le corna
            Color bone = blink ? Color{ 255, 120, 120, 255 } : Color{ 200, 196, 184, 255 };
            DrawCylinderEx(To3D(e.pos, 0), c, r * 0.35f, r * 0.55f, 6, fog({ 90, 84, 78, 255 }));
            DrawSphereEx(c, r * 0.7f, 8, 8, fog({ 120, 110, 100, 255 }));
            Vector3 head = at(r * 0.3f, 0, r * 0.9f);
            DrawSphereEx(head, r * 0.42f, 7, 7, fog(bone));
            for (float s : { -1.0f, 1.0f }) {
                Vector3 b = at(r * 0.2f, s * r * 0.3f, r * 1.2f), m = at(r * 0.1f, s * r * 0.8f, r * 1.9f);
                DrawCylinderEx(b, m, 0.025f, 0.015f, 4, fog(bone));
                DrawCylinderEx(m, at(r * 0.4f, s * r * 1.1f, r * 2.3f), 0.015f, 0.005f, 4, fog(bone));
                DrawCylinderEx(m, at(-r * 0.2f, s * r * 1.2f, r * 2.1f), 0.015f, 0.005f, 4, fog(bone));
                DrawCylinderEx(at(0, s * r * 0.6f, r * 0.4f), at(r * 0.6f, s * r * 0.9f, -r * 0.8f), 0.03f, 0.02f, 4, fog({ 110, 100, 90, 255 }));
            }
            eyesPair(r * 0.68f, r * 0.15f, r * 0.95f, r * 0.08f, glow({ 220, 240, 255, 255 }));
            if (e.special > 0) DrawSphereEx(at(r * 0.9f, 0, -r * 0.6f), 0.08f, 5, 5, { 150, 20, 20, 255 }); // sta mangiando
            break;
        }
        case E::Mothman: { // corpo scuro, grandi ali, occhi rossi che si vedono sempre
            float flap = sinf(t * 7 + e.id) * 0.18f;
            Color dark = fog({ 50, 40, 36, 255 });
            DrawCylinderEx(at(0, 0, -r * 0.9f), at(0, 0, r * 0.6f), r * 0.35f, r * 0.5f, 6, dark);
            wing(1, 0.8f, flap, dark);
            wing(-1, 0.8f, flap, dark);
            eyesPair(r * 0.4f, r * 0.22f, r * 0.55f, r * 0.14f, glow({ 255, 30, 20, 255 }));
            break;
        }
        case E::JerseyDevil: { // testa di cavallo, ali da pipistrello, zoccoli
            Color fur = fog(blink ? Color{ 200, 80, 60, 255 } : Color{ 100, 52, 40, 255 });
            DrawSphereEx(c, r * 0.75f, 8, 8, fur);
            DrawCylinderEx(at(r * 0.3f, 0, r * 0.5f), at(r * 1.0f, 0, r * 0.4f), r * 0.25f, r * 0.18f, 6, fur); // muso
            for (float s : { -1.0f, 1.0f }) DrawCylinderEx(at(r * 0.4f, s * r * 0.2f, r * 0.75f), at(r * 0.3f, s * r * 0.3f, r * 1.2f), 0.03f, 0.0f, 4, fog({ 230, 220, 200, 255 }));
            float flap = sinf(t * 6) * 0.15f;
            wing(1, 0.6f, flap, fog({ 70, 30, 30, 255 }));
            wing(-1, 0.6f, flap, fog({ 70, 30, 30, 255 }));
            legs(r * 0.3f, r * 0.9f + e.z, 0.03f, fog({ 40, 30, 26, 255 }));
            eyesPair(r * 0.85f, r * 0.12f, r * 0.55f, r * 0.08f, glow({ 255, 200, 40, 255 }));
            break;
        }
        case E::Gnefro: {
            if (e.hidden) { // sotto le macerie: si intravedono appena gli occhi
                DrawSphereEx(To3D(e.pos, 0.02f), r * 1.1f, 6, 6, fog(Shade(d.stone, 0.8f)));
                if (dist < 6) eyesPair(r * 0.9f, r * 0.25f, -r * 0.6f, 0.012f, { 255, 220, 80, 255 });
                break;
            }
            DrawSphereEx(c, r, 8, 8, fog({ 120, 90, 60, 255 }));
            DrawCylinder(Vector3Add(c, { 0, r * 0.6f, 0 }), r * 0.7f, 0.0f, r * 1.6f, 6, fog({ 190, 40, 40, 255 })); // cappello a punta
            for (float s : { -1.0f, 1.0f }) DrawCylinderEx(at(0, s * r * 0.8f, r * 0.2f), at(-r * 0.2f, s * r * 1.6f, r * 0.5f), r * 0.2f, 0.01f, 4, fog({ 150, 110, 80, 255 }));
            eyesPair(r * 0.85f, r * 0.3f, r * 0.15f, r * 0.15f, { 30, 20, 10, 255 });
            break;
        }
        case E::Babau: { // ombra senza volto: un velo nero che ondeggia
            Color shadow = Fade({ 18, 12, 28, 255 }, a);
            DrawCylinderEx(To3D(e.pos, 0), Vector3Add(c, { 0, r * 0.5f, 0 }), r * 0.9f, r * 0.45f, 8, shadow);
            DrawSphereEx(Vector3Add(c, { 0, r * 0.9f, 0 }), r * 0.5f, 8, 8, shadow);
            for (int k = 0; k < 4; ++k) // filamenti d'ombra
                DrawSphereEx(Vector3Add(c, { sinf(t * 2 + k) * r, -r * 0.2f + k * 0.08f, cosf(t * 2 + k) * r }), r * 0.25f, 4, 4, Fade(shadow, a * 0.5f));
            if (!e.hidden) eyesPair(r * 0.45f, r * 0.15f, r * 0.95f, r * 0.07f, { 160, 80, 255, 255 });
            break;
        }
        case E::Badalisc: { // un sacco peloso con le corna e due occhioni
            float roll = e.attack == EnemyAttack::Roll && e.windup <= 0 ? t * 12 : 0;
            Color fur = fog(blink ? Color{ 200, 120, 80, 255 } : Color{ 120, 90, 60, 255 });
            DrawSphereEx(c, r, 10, 10, fur);
            DrawSphereEx(Vector3Add(c, { 0, -r * 0.3f, 0 }), r * 1.05f, 8, 8, fog({ 100, 74, 50, 255 }));
            for (float s : { -1.0f, 1.0f }) {
                DrawCylinderEx(at(0, s * r * 0.5f, r * 0.7f), at(-r * 0.2f, s * r * 0.9f, r * 1.4f), r * 0.14f, 0.01f, 5, fog({ 230, 220, 190, 255 }));
                Vector3 eye = at(r * 0.8f, s * r * 0.35f, r * 0.3f + sinf(roll) * 0.05f);
                DrawSphereEx(eye, r * 0.28f, 7, 7, fog(WHITE));
                DrawSphereEx(Vector3Add(eye, Vector3Scale(fwd, r * 0.18f)), r * 0.13f, 5, 5, fog(BLACK));
            }
            break;
        }
        case E::Grossmann: { // altissimo e magro, vestito di nero, faccia bianca senza tratti
            Color suit = fog({ 16, 16, 20, 255 });
            for (float s : { -1.0f, 1.0f }) DrawCylinderEx(To3D(Vector2Add(e.pos, { side.x * s * 4, side.z * s * 4 }), 0), at(0, s * r * 0.2f, 0), 0.035f, 0.04f, 5, suit);
            DrawCylinderEx(c, at(0, 0, 1.1f), 0.12f, 0.1f, 6, suit);
            DrawSphereEx(at(0, 0, 1.3f), 0.12f, 8, 8, fog({ 236, 236, 232, 255 }));
            float reach = e.windup > 0 ? (1 - e.windup / 0.55f) * 3.5f : 0.6f; // le braccia si allungano
            for (float s : { -1.0f, 1.0f })
                DrawCylinderEx(at(0, s * 0.14f, 1.0f), at(reach, s * 0.25f, 0.6f), 0.02f, 0.015f, 4, suit);
            break;
        }
        case E::Nachzehrer: { // morto nel sudario, ingobbito
            DrawSphereEx(c, r, 8, 8, fog({ 120, 130, 110, 255 }));
            DrawCylinderEx(To3D(e.pos, 0), c, r * 0.9f, r * 0.8f, 7, fog({ 210, 206, 196, 255 }));
            DrawSphereEx(at(r * 0.5f, 0, r * 0.6f), r * 0.45f, 7, 7, fog({ 140, 150, 120, 255 }));
            eyesPair(r * 0.9f, r * 0.15f, r * 0.7f, r * 0.08f, glow({ 255, 60, 40, 255 }));
            if (e.aggro && Vector2Distance(e.pos, player.pos) < 85) // il risucchio della vita
                DrawCylinderEx(c, To3D(player.pos, player.z + 0.4f), 0.02f, 0.01f, 4, Fade({ 140, 255, 140, 255 }, 0.5f));
            break;
        }
        case E::Wolpertinger: { // lepre con corna e alette
            DrawSphereEx(c, r, 8, 8, fog({ 150, 110, 70, 255 }));
            DrawSphereEx(at(r * 0.8f, 0, r * 0.3f), r * 0.55f, 6, 6, fog({ 160, 120, 80, 255 }));
            for (float s : { -1.0f, 1.0f }) {
                DrawCylinderEx(at(r * 0.7f, s * r * 0.25f, r * 0.7f), at(r * 0.5f, s * r * 0.35f, r * 1.8f), r * 0.12f, r * 0.05f, 4, fog({ 170, 130, 90, 255 }));
                DrawCylinderEx(at(r * 0.9f, s * r * 0.2f, r * 0.7f), at(r * 1.1f, s * r * 0.5f, r * 1.4f), 0.01f, 0.004f, 3, fog({ 230, 220, 200, 255 }));
                wing(s, r * 1.6f, sinf(t * 20) * 0.05f, fog({ 200, 190, 170, 255 }));
            }
            eyesPair(r * 1.2f, r * 0.2f, r * 0.4f, r * 0.1f, BLACK);
            break;
        }
        case E::Gevaudan: { // lupo enorme, rossiccio, con la criniera scura
            Color fur = fog(blink ? Color{ 220, 90, 60, 255 } : Color{ 140, 80, 50, 255 });
            DrawSphereEx(at(-r * 0.4f, 0, 0), r * 0.8f, 8, 8, fur);
            DrawSphereEx(at(r * 0.4f, 0, r * 0.1f), r * 0.85f, 8, 8, fog({ 70, 44, 30, 255 }));
            DrawCylinderEx(at(r * 0.9f, 0, r * 0.3f), at(r * 1.6f, 0, r * 0.15f), r * 0.32f, r * 0.18f, 6, fur);
            for (float s : { -1.0f, 1.0f }) DrawCylinderEx(at(r * 0.9f, s * r * 0.25f, r * 0.6f), at(r * 0.8f, s * r * 0.3f, r * 1.0f), r * 0.1f, 0.0f, 4, fur);
            DrawCylinderEx(at(-r * 1.1f, 0, 0), at(-r * 1.8f, 0, -r * 0.2f), r * 0.15f, 0.02f, 5, fur);
            legs(r * 0.5f, r * 0.9f + e.z, 0.05f, fur);
            eyesPair(r * 1.3f, r * 0.15f, r * 0.45f, r * 0.07f, glow({ 255, 200, 60, 255 }));
            break;
        }
        case E::Dahu: { // capretta con le zampe di sinistra piu' corte: cammina storta
            rlPushMatrix();
            rlTranslatef(c.x, 0, c.z);
            rlRotatef(10, fwd.x, 0, fwd.z);
            rlTranslatef(-c.x, 0, -c.z);
            DrawSphereEx(c, r, 8, 8, fog({ 230, 220, 200, 255 }));
            DrawSphereEx(at(r * 0.9f, 0, r * 0.4f), r * 0.45f, 6, 6, fog({ 220, 210, 190, 255 }));
            for (float s : { -1.0f, 1.0f }) DrawCylinderEx(at(r * 0.9f, s * r * 0.2f, r * 0.8f), at(r * 0.5f, s * r * 0.4f, r * 1.3f), r * 0.08f, 0.01f, 4, fog({ 120, 100, 80, 255 }));
            legs(r * 0.5f, r, 0.03f, fog({ 120, 100, 80, 255 }), 0.55f);
            rlPopMatrix();
            break;
        }
        case E::Matagot: { // gatto nero con gli occhi verdi
            DrawSphereEx(c, r, 8, 8, fog({ 20, 18, 24, 255 }));
            DrawSphereEx(at(r * 0.7f, 0, r * 0.6f), r * 0.55f, 7, 7, fog({ 24, 22, 28, 255 }));
            for (float s : { -1.0f, 1.0f }) DrawCylinder(at(r * 0.7f, s * r * 0.3f, r * 1.0f), r * 0.18f, 0.0f, r * 0.4f, 4, fog({ 30, 26, 34, 255 }));
            DrawCylinderEx(at(-r * 0.8f, 0, 0), at(-r * 1.3f, 0, r * 1.2f + sinf(t * 3) * 0.1f), r * 0.12f, r * 0.08f, 5, fog({ 20, 18, 24, 255 }));
            eyesPair(r * 1.15f, r * 0.2f, r * 0.7f, r * 0.11f, glow({ 120, 255, 120, 255 }));
            break;
        }
        case E::BlackShuck: case E::Dip: { // cani neri: Black Shuck enorme con gli occhi di fuoco, Dip magro e zoppo
            bool shuck = e.type == E::BlackShuck;
            Color fur = fog(blink ? Color{ 90, 40, 40, 255 } : Color{ 18, 18, 20, 255 });
            DrawSphereEx(at(-r * 0.4f, 0, 0), r * (shuck ? 0.85f : 0.6f), 8, 8, fur);
            DrawSphereEx(at(r * 0.35f, 0, r * 0.1f), r * (shuck ? 0.8f : 0.6f), 8, 8, fur);
            DrawCylinderEx(at(r * 0.8f, 0, r * 0.35f), at(r * 1.5f, 0, r * 0.2f), r * 0.3f, r * 0.16f, 6, fur);
            for (float s : { -1.0f, 1.0f }) DrawCylinder(at(r * 0.8f, s * r * 0.25f, r * 0.65f), r * 0.1f, 0.0f, r * 0.35f, 4, fur);
            legs(r * 0.45f, r * 0.9f + e.z, shuck ? 0.05f : 0.03f, fur, shuck ? 1.0f : 0.7f);
            eyesPair(r * 1.25f, r * 0.15f, r * 0.45f, r * (shuck ? 0.14f : 0.08f), glow(shuck ? Color{ 255, 120, 20, 255 } : Color{ 255, 40, 40, 255 }));
            break;
        }
        case E::SpringJack: { // mantello nero, elmo, occhi rossi; fiamme blu quando soffia
            Color cape = fog({ 20, 20, 30, 255 });
            DrawCylinderEx(To3D(e.pos, e.z), c, r * 0.5f, r * 0.7f, 6, cape);
            DrawCylinder(Vector3Add(c, { 0, -r * 0.4f, 0 }), r * 0.6f, r * 1.2f, r * 1.6f, 8, Fade(cape, a * 0.8f)); // mantello aperto
            DrawSphereEx(at(0, 0, r * 0.9f), r * 0.45f, 7, 7, fog({ 200, 200, 210, 255 }));
            eyesPair(r * 0.42f, r * 0.15f, r * 0.95f, r * 0.09f, glow({ 255, 50, 30, 255 }));
            if (e.attack == EnemyAttack::Breath && e.windup > 0)
                DrawSphereEx(at(r * 0.7f, 0, r * 0.8f), 0.08f + 0.04f * sinf(t * 30), 5, 5, { 90, 150, 255, 255 });
            for (float s : { -1.0f, 1.0f }) // le molle sotto i tacchi
                DrawCylinderEx(To3D(Vector2Add(e.pos, { side.x * s * 4, side.z * s * 4 }), e.z), To3D(Vector2Add(e.pos, { side.x * s * 4, side.z * s * 4 }), e.z + 0.15f), 0.03f, 0.03f, 4, fog({ 150, 150, 160, 255 }));
            break;
        }
        case E::Redcap: { // goblin grigio col berretto che diventa sempre piu' rosso
            DrawSphereEx(c, r, 8, 8, fog({ 110, 120, 100, 255 }));
            float k = std::min(1.0f, e.stacks / 5.0f);
            DrawCylinder(Vector3Add(c, { 0, r * 0.55f, 0 }), r * 0.8f, r * 0.2f, r * (0.7f + k * 0.6f), 7, fog(Mix({ 140, 40, 40, 255 }, { 255, 20, 20, 255 }, k)));
            eyesPair(r * 0.88f, r * 0.3f, r * 0.15f, r * 0.13f, glow({ 255, 200, 60, 255 }));
            DrawCylinderEx(at(r * 0.5f, r * 0.9f, -r * 0.1f), at(r * 1.2f, r * 1.0f, r * 0.6f), 0.015f, 0.015f, 4, fog({ 170, 170, 180, 255 })); // la picca
            break;
        }
        case E::Cuco: {
            if (e.hidden) { // travestito da sacco abbandonato
                DrawSphereEx(To3D(e.pos, r * 0.7f), r * 0.8f, 8, 8, fog({ 140, 116, 80, 255 }));
                DrawCylinder(To3D(e.pos, r * 1.4f), r * 0.15f, r * 0.3f, r * 0.3f, 6, fog({ 120, 96, 64, 255 }));
                break;
            }
            Color shadow = fog({ 30, 20, 26, 255 });
            DrawSphereEx(c, r, 8, 8, shadow);
            DrawSphereEx(at(0, 0, r * 0.9f), r * 0.6f, 8, 8, shadow);
            for (int k = 0; k < 6; ++k) // peli ispidi
                DrawCylinderEx(at(0, 0, r * 0.5f), at(cosf(k * 1.05f) * r * 1.4f, sinf(k * 1.05f) * r * 1.4f, r * (0.2f + 0.2f * sinf(t * 4 + k))), 0.02f, 0.0f, 3, shadow);
            eyesPair(r * 0.55f, r * 0.2f, r * 1.0f, r * 0.12f, glow({ 255, 230, 60, 255 }));
            DrawCube(at(r * 0.6f, 0, r * 0.6f), r * 0.1f, r * 0.12f, r * 0.6f, glow({ 160, 20, 20, 255 })); // la bocca
            break;
        }
        case E::Compana: { // anime incappucciate con la candela
            Color robe = Fade(ApplyFog({ 230, 230, 240, 255 }, d.horizon, dist), 0.7f);
            DrawCylinder(To3D(e.pos, 0), r * 0.9f, r * 0.35f, r * 2.2f, 8, robe);
            DrawSphereEx(To3D(e.pos, r * 2.3f), r * 0.45f, 7, 7, robe);
            DrawSphereEx(at(r * 0.4f, 0, r * 1.3f), r * 0.2f, 4, 4, BLACK); // il cappuccio vuoto
            Vector3 candle = at(r * 0.9f, r * 0.4f, r * 0.3f);
            DrawCylinder(candle, 0.015f, 0.015f, 0.12f, 4, { 240, 230, 200, 255 });
            DrawSphereEx(Vector3Add(candle, { 0, 0.15f + 0.01f * sinf(t * 15 + e.id), 0 }), 0.025f, 4, 4, { 255, 210, 100, 255 });
            break;
        }
        default: break;
    }
    if (e.burnTimer > 0) DrawSphereEx(Vector3Add(c, { 0, r + 0.15f, 0 }), 0.05f + 0.02f * sinf(t * 20), 4, 4, ORANGE);
    if (e.stun > 0)
        for (int k = 0; k < 3; ++k) {
            float ang = t * 5 + k * 2.1f;
            DrawSphereEx(Vector3Add(c, { cosf(ang) * 0.3f, r + 0.3f, sinf(ang) * 0.3f }), 0.04f, 4, 4, YELLOW);
        }
}
