#include "Weapons.hpp"
#include <cmath>
#include "raymath.h"

const std::vector<Weapon>& AllWeapons() {
    //                    danno  cadenza  velocita' ventaglio impreciso durata raggio trap. rimb. esplode melee portata arco
    static const std::vector<Weapon> list = {
        { "Ofuda", "Santuari shinto", "Talismani di carta: equilibrati, ne' veloci ne' lenti",
          1.0f, 1.0f, 1.0f, 0.16f, 0.0f, 1.2f, 4.5f, 0, 0, false, false, 0, 0, { 240, 230, 205, 255 } },
        { "Katana", "Seki", "Fendente ravvicinato fortissimo: respinge anche i colpi nemici",
          2.6f, 0.75f, 0, 0, 0, 0, 0, 0, 0, false, true, 74, 0.75f, { 200, 210, 220, 255 } },
        { "Yumi", "Sanjusangen-do, Kyoto", "Arco lungo: frecce lente da tirare ma potenti, trapassano 2 nemici",
          2.4f, 0.45f, 1.9f, 0.06f, 0.0f, 1.4f, 3.5f, 2, 0, false, false, 0, 0, { 160, 110, 60, 255 } },
        { "Fionda pachinko", "Sale pachinko di Osaka", "Raffica di biglie d'acciaio che rimbalzano sui muri",
          0.42f, 2.8f, 0.95f, 0.1f, 0.07f, 1.6f, 3.0f, 0, 2, false, false, 0, 0, { 190, 195, 205, 255 } },
        { "Razzi Tamaya", "Fuochi di Sumida, Tokyo", "Razzi che esplodono in fuochi d'artificio e incendiano",
          1.5f, 0.5f, 0.7f, 0.2f, 0.0f, 1.8f, 6.0f, 0, 0, true, false, 0, 0, { 210, 50, 50, 255 } },
    };
    return list;
}

// ---------------------------------------------------------------------------
// Modellini 3D (centrati nell'origine, lunghi circa mezza casella)
// ---------------------------------------------------------------------------

void DrawWeapon3D(int id) {
    switch (id) {
        case W_OFUDA:
            DrawCube({ 0, 0, 0 }, 0.02f, 0.36f, 0.16f, { 240, 230, 205, 255 });
            DrawCube({ 0.012f, 0.12f, 0 }, 0.005f, 0.05f, 0.16f, { 190, 40, 40, 255 });
            break;
        case W_KATANA:
            DrawCube({ 0, 0.12f, 0 }, 0.02f, 0.5f, 0.035f, { 210, 215, 225, 255 });
            DrawCube({ 0, -0.14f, 0 }, 0.08f, 0.02f, 0.08f, { 60, 50, 30, 255 });   // tsuba
            DrawCube({ 0, -0.24f, 0 }, 0.035f, 0.18f, 0.035f, { 30, 30, 40, 255 }); // impugnatura
            break;
        case W_YUMI:
            for (int k = 0; k < 6; ++k) { // arco fatto a segmenti
                float a0 = -0.9f + k * 0.3f, a1 = a0 + 0.3f;
                DrawCylinderEx({ 0, std::sin(a0) * 0.4f, std::cos(a0) * 0.12f - 0.08f },
                               { 0, std::sin(a1) * 0.4f, std::cos(a1) * 0.12f - 0.08f }, 0.015f, 0.015f, 5, { 150, 100, 55, 255 });
            }
            DrawCylinderEx({ 0, -0.31f, -0.0f }, { 0, 0.31f, -0.0f }, 0.004f, 0.004f, 3, { 230, 230, 220, 255 });
            break;
        case W_PACHINKO:
            DrawCube({ 0, -0.12f, 0 }, 0.04f, 0.2f, 0.04f, { 120, 80, 50, 255 });
            DrawCube({ 0, 0.04f, -0.07f }, 0.04f, 0.16f, 0.04f, { 120, 80, 50, 255 });
            DrawCube({ 0, 0.04f, 0.07f }, 0.04f, 0.16f, 0.04f, { 120, 80, 50, 255 });
            DrawSphereEx({ 0, 0.08f, 0 }, 0.05f, 6, 6, { 200, 205, 215, 255 });
            break;
        case W_TAMAYA:
            DrawCylinderEx({ 0, -0.2f, 0 }, { 0, 0.22f, 0 }, 0.07f, 0.07f, 8, { 200, 45, 45, 255 });
            DrawCylinderEx({ 0, 0.06f, 0 }, { 0, 0.1f, 0 }, 0.075f, 0.075f, 8, { 230, 190, 60, 255 });
            DrawCylinderEx({ 0, 0.22f, 0 }, { 0, 0.34f, 0 }, 0.07f, 0.0f, 8, { 230, 190, 60, 255 });
            break;
    }
}

// ---------------------------------------------------------------------------
// Armi in mano (2D, sopra la scena)
// ---------------------------------------------------------------------------

static void Bar(Vector2 pivot, float x, float y, float w, float h, float angle, Color c) {
    // Rettangolo che ruota attorno a "pivot"; (x, y) e' lo spostamento del suo centro nel sistema ruotato
    float r = angle * DEG2RAD;
    Vector2 center{ pivot.x + x * std::cos(r) - y * std::sin(r), pivot.y + x * std::sin(r) + y * std::cos(r) };
    DrawRectanglePro({ center.x, center.y, w, h }, { w / 2, h / 2 }, angle, c);
}

void DrawWeaponInHand(int id, float bobX, float bobY, float recoil, float swing, bool ready, Color accent) {
    const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    const Color shadow = Fade(BLACK, 0.35f);

    switch (id) {
        case W_OFUDA: {
            float hx = sw - 290 + bobX, hy = sh - 300 + bobY + recoil * 26;
            float angle = 14 - recoil * 6;
            DrawRectanglePro({ hx + 4, hy + 4, 62, 140 }, { 31, 70 }, angle, shadow);
            DrawRectanglePro({ hx, hy, 62, 140 }, { 31, 70 }, angle, { 240, 230, 205, 255 });
            DrawRectanglePro({ hx, hy - 52, 62, 14 }, { 31, 70 }, angle, { 190, 40, 40, 255 });
            for (int k = 0; k < 4; ++k)
                DrawRectanglePro({ hx, hy - 22 + k * 22.0f, 26, 5 }, { 13, 2.5f }, angle, { 30, 25, 25, 255 });
            if (recoil > 0.8f) DrawCircle((int)hx - 30, (int)hy - 80, 16, Fade(accent, 0.6f));
            break;
        }
        case W_KATANA: {
            // A riposo la lama punta in alto a sinistra; il fendente la fa passare da destra a sinistra
            float angle = swing > 0 ? -150 + (1 - swing) * 120 : -125.0f;
            Vector2 pivot{ sw - 230 + bobX, sh - 150 + bobY };
            Bar(pivot, 40, 0, 90, 18, angle, { 30, 30, 42, 255 });       // impugnatura
            for (int k = 0; k < 4; ++k) Bar(pivot, 8 + k * 20.0f, 0, 6, 18, angle, { 200, 190, 160, 255 }); // intreccio
            Bar(pivot, -8, 0, 10, 46, angle, { 70, 56, 30, 255 });        // tsuba
            Bar(pivot, -150, 0, 270, 14, angle, { 205, 212, 222, 255 });  // lama
            Bar(pivot, -150, 4, 270, 4, angle, { 240, 245, 250, 255 });   // filo
            if (swing > 0.3f) { // scia del fendente
                for (int k = 0; k < 3; ++k)
                    Bar(pivot, -160, 0, 260, 10, angle - 12 - k * 10, Fade(WHITE, 0.15f * (3 - k)));
            }
            break;
        }
        case W_YUMI: {
            float cx = sw - 260 + bobX, cy = sh - 300 + bobY;
            Vector2 prev{};
            for (int k = 0; k <= 12; ++k) { // l'arco, una curva fatta di segmenti
                float a = -1.0f + k * (2.0f / 12);
                Vector2 p{ cx + std::cos(a) * 90 - 90 + recoil * 8, cy + std::sin(a) * 230 };
                if (k) DrawLineEx(prev, p, 9, { 150, 100, 55, 255 });
                prev = p;
            }
            Vector2 top{ cx + std::cos(-1.0f) * 90 - 90, cy - std::sin(1.0f) * 230 };
            Vector2 bottom{ top.x, cy + std::sin(1.0f) * 230 };
            float pull = ready ? 40.0f : 0.0f;
            Vector2 nock{ top.x + pull, cy };
            DrawLineEx(top, nock, 2, { 235, 235, 225, 255 });
            DrawLineEx(nock, bottom, 2, { 235, 235, 225, 255 });
            if (ready) { // freccia incoccata
                DrawLineEx(nock, { nock.x - 200, cy - 40 }, 4, { 120, 80, 50, 255 });
                DrawTriangle({ nock.x - 214, cy - 43 }, { nock.x - 198, cy - 33 }, { nock.x - 196, cy - 46 }, { 180, 185, 195, 255 });
            }
            break;
        }
        case W_PACHINKO: {
            Vector2 pivot{ sw - 250 + bobX, sh - 190 + bobY + recoil * 14 };
            Color wood{ 130, 86, 52, 255 };
            Bar(pivot, 0, 60, 22, 110, 0, wood);       // manico
            Bar(pivot, -26, -20, 16, 80, -18, wood);   // rebbi
            Bar(pivot, 26, -20, 16, 80, 18, wood);
            float back = ready ? 30.0f : 6.0f;
            Vector2 l{ pivot.x - 40, pivot.y - 56 }, r{ pivot.x + 40, pivot.y - 56 }, pouch{ pivot.x, pivot.y - 30 + back };
            DrawLineEx(l, pouch, 5, { 90, 60, 40, 255 });
            DrawLineEx(r, pouch, 5, { 90, 60, 40, 255 });
            if (ready) DrawCircleV(pouch, 11, { 200, 205, 215, 255 });
            break;
        }
        case W_TAMAYA: {
            Vector2 pivot{ sw - 250 + bobX + recoil * 10, sh - 200 + bobY + recoil * 30 };
            float angle = -60;
            Bar(pivot, 0, 6, 250, 64, angle, shadow);
            Bar(pivot, 0, 0, 250, 64, angle, { 196, 44, 44, 255 });
            for (int k = 0; k < 3; ++k) Bar(pivot, -90 + k * 70.0f, 0, 12, 66, angle, { 230, 190, 60, 255 });
            Bar(pivot, -128, 0, 10, 52, angle, { 40, 30, 30, 255 }); // bocca
            if (recoil > 0.7f) DrawCircle((int)(pivot.x - 70), (int)(pivot.y - 120), 30 * recoil, Fade({ 255, 200, 80, 255 }, 0.7f));
            break;
        }
    }
}
