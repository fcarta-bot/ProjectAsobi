#include "Weapons.hpp"
#include <cmath>
#include "raymath.h"

using S = WeaponShape;
using L = ShotLook;
using C = Country;

const std::vector<Weapon>& AllWeapons() {
    // Solo i campi che cambiano rispetto ai valori normali (vedi Weapons.hpp), nell'ordine in cui sono dichiarati
    static const std::vector<Weapon> list = {
        // --- Giappone ---
        { .name = "Ofuda", .origin = "Santuari shinto", .desc = "Talismani di carta: ne' veloci ne' lenti, vanno bene per tutto",
          .country = C::Japan, .shape = S::Talisman, .color = { 240, 230, 205, 255 }, .color2 = { 190, 40, 40, 255 } },
        { .name = "Katana", .origin = "Seki", .desc = "Fendente ravvicinato fortissimo, che ferma anche i colpi nemici",
          .country = C::Japan, .shape = S::Blade, .damageMult = 2.6f, .rateMult = 0.75f, .melee = true, .meleeRange = 74, .meleeArc = 0.75f,
          .color = { 200, 210, 220, 255 }, .color2 = { 30, 30, 42, 255 } },
        { .name = "Yumi", .origin = "Sanjusangen-do, Kyoto", .desc = "Arco lungo: lento da tendere, ma ogni freccia passa da parte a parte",
          .country = C::Japan, .shape = S::Bow, .shot = L::Arrow, .damageMult = 2.4f, .rateMult = 0.45f, .speedMult = 1.9f, .spread = 0.06f,
          .life = 1.4f, .radius = 3.5f, .extraPierce = 2, .color = { 160, 110, 60, 255 } },
        { .name = "Fionda pachinko", .origin = "Sale pachinko di Osaka", .desc = "Raffica di biglie d'acciaio che rimbalzano sui muri",
          .country = C::Japan, .shape = S::Sling, .shot = L::Ball, .damageMult = 0.42f, .rateMult = 2.8f, .speedMult = 0.95f, .spread = 0.1f,
          .jitter = 0.07f, .life = 1.6f, .radius = 3.0f, .bounces = 2, .color = { 190, 195, 205, 255 }, .color2 = { 130, 86, 52, 255 } },
        { .name = "Razzi Tamaya", .origin = "Fuochi di Sumida, Tokyo", .desc = "Razzi che scoppiano in fuochi d'artificio e danno fuoco a cio' che brucia",
          .country = C::Japan, .shape = S::Rocket, .shot = L::Rocket, .damageMult = 1.5f, .rateMult = 0.5f, .speedMult = 0.7f, .spread = 0.2f,
          .life = 1.8f, .radius = 6.0f, .explosive = true, .color = { 210, 50, 50, 255 }, .color2 = { 230, 190, 60, 255 } },
        // --- Stati Uniti ---
        { .name = "Revolver Colt", .origin = "Il Far West", .desc = "Colpi rapidi e precisi che trapassano un nemico",
          .country = C::USA, .shape = S::Pistol, .shot = L::Bullet, .damageMult = 1.5f, .rateMult = 1.4f, .speedMult = 2.2f, .spread = 0.05f,
          .life = 0.9f, .radius = 3.0f, .extraPierce = 1, .color = { 150, 150, 160, 255 }, .color2 = { 120, 80, 50, 255 } },
        { .name = "Fucile a pompa", .origin = "Texas", .desc = "Sei pallettoni a ventaglio: devastante da vicino, inutile da lontano",
          .country = C::USA, .shape = S::LongGun, .shot = L::Bullet, .damageMult = 0.55f, .rateMult = 0.55f, .speedMult = 1.6f, .spread = 0.09f,
          .jitter = 0.05f, .life = 0.45f, .radius = 3.0f, .pellets = 5, .color = { 70, 70, 76, 255 }, .color2 = { 120, 80, 50, 255 } },
        { .name = "Mazza da baseball", .origin = "Yankee Stadium, New York", .desc = "Colpo largo e pesante: spedisce lontano i nemici",
          .country = C::USA, .shape = S::Club, .damageMult = 2.0f, .rateMult = 0.85f, .melee = true, .meleeRange = 66, .meleeArc = 1.0f,
          .knockback = 46, .stun = 0.25f, .color = { 200, 170, 120, 255 }, .color2 = { 60, 40, 30, 255 } },
        { .name = "Tomahawk", .origin = "Grandi Pianure", .desc = "Ascia da lancio che rimbalza una volta e trapassa",
          .country = C::USA, .shape = S::Axe, .shot = L::Axe, .damageMult = 1.6f, .rateMult = 0.8f, .speedMult = 1.1f, .life = 1.2f,
          .radius = 6.0f, .extraPierce = 1, .bounces = 1, .color = { 160, 160, 165, 255 }, .color2 = { 120, 80, 50, 255 } },
        // --- Italia ---
        { .name = "Gladio", .origin = "Roma", .desc = "La spada corta dei legionari: colpi rapidi e precisi",
          .country = C::Italy, .shape = S::Blade, .damageMult = 1.4f, .rateMult = 1.5f, .melee = true, .meleeRange = 56, .meleeArc = 0.55f,
          .color = { 205, 205, 200, 255 }, .color2 = { 170, 120, 50, 255 } },
        { .name = "Balestra genovese", .origin = "Genova", .desc = "Lenta da ricaricare, ma i dardi trapassano tre nemici",
          .country = C::Italy, .shape = S::Crossbow, .shot = L::Arrow, .damageMult = 3.0f, .rateMult = 0.38f, .speedMult = 2.3f, .spread = 0.04f,
          .life = 1.4f, .radius = 3.5f, .extraPierce = 3, .color = { 130, 90, 50, 255 }, .color2 = { 80, 80, 90, 255 } },
        { .name = "Lancia della Giostra", .origin = "Giostra del Saracino, Arezzo", .desc = "Affondo lunghissimo: tiene i nemici a distanza",
          .country = C::Italy, .shape = S::Spear, .damageMult = 2.0f, .rateMult = 0.9f, .melee = true, .meleeRange = 112, .meleeArc = 0.32f,
          .knockback = 30, .color = { 200, 40, 40, 255 }, .color2 = { 230, 200, 80, 255 } },
        { .name = "Fuochi di Piedigrotta", .origin = "Napoli", .desc = "Tre fuochi d'artificio a ventaglio che scoppiano e incendiano",
          .country = C::Italy, .shape = S::Rocket, .shot = L::Spark, .damageMult = 0.8f, .rateMult = 0.55f, .speedMult = 0.8f, .spread = 0.25f,
          .life = 1.1f, .radius = 5.0f, .pellets = 2, .explosive = true, .color = { 40, 110, 220, 255 }, .color2 = { 240, 240, 240, 255 } },
        // --- Francia ---
        { .name = "Fioretto", .origin = "Parigi", .desc = "Lama sottilissima: affondi lunghi e rapidissimi",
          .country = C::France, .shape = S::Blade, .damageMult = 1.0f, .rateMult = 2.2f, .melee = true, .meleeRange = 84, .meleeArc = 0.3f,
          .knockback = 10, .color = { 220, 225, 235, 255 }, .color2 = { 200, 170, 60, 255 } },
        { .name = "Fucile di Chastel", .origin = "Gevaudan, 1767", .desc = "Pallottole d'argento: le bestie leggendarie le temono",
          .country = C::France, .shape = S::LongGun, .shot = L::Bullet, .damageMult = 2.6f, .rateMult = 0.42f, .speedMult = 2.4f, .life = 1.0f,
          .radius = 3.5f, .extraPierce = 1, .silver = true, .color = { 210, 215, 225, 255 }, .color2 = { 110, 70, 40, 255 } },
        { .name = "Boule de petanque", .origin = "Marsiglia", .desc = "Bocce d'acciaio pesanti che rimbalzano tre volte",
          .country = C::France, .shape = S::Ball, .shot = L::Ball, .damageMult = 1.4f, .rateMult = 0.9f, .speedMult = 0.9f, .life = 1.8f,
          .radius = 6.0f, .bounces = 3, .color = { 170, 175, 185, 255 } },
        { .name = "Tappo di Champagne", .origin = "Reims", .desc = "Bottiglia agitata: il tappo rimbalza e poi esplode in schiuma",
          .country = C::France, .shape = S::Bottle, .shot = L::Cork, .damageMult = 1.3f, .rateMult = 0.6f, .speedMult = 1.3f, .life = 1.2f,
          .bounces = 1, .explosive = true, .incendiary = false, .color = { 40, 90, 50, 255 }, .color2 = { 230, 200, 90, 255 } },
        // --- Germania ---
        { .name = "Zweihander", .origin = "Solingen", .desc = "Spadone a due mani: lento, ma taglia tutto quello che ha davanti",
          .country = C::Germany, .shape = S::Blade, .damageMult = 3.4f, .rateMult = 0.5f, .melee = true, .meleeRange = 92, .meleeArc = 1.2f,
          .knockback = 34, .color = { 190, 195, 205, 255 }, .color2 = { 60, 40, 30, 255 } },
        { .name = "Morgenstern", .origin = "Norimberga", .desc = "Mazza ferrata: ogni colpo stordisce chi lo prende",
          .country = C::Germany, .shape = S::Club, .damageMult = 2.4f, .rateMult = 0.75f, .melee = true, .meleeRange = 64, .meleeArc = 0.8f,
          .stun = 0.6f, .color = { 120, 120, 128, 255 }, .color2 = { 80, 55, 35, 255 } },
        { .name = "Boccale dell'Oktoberfest", .origin = "Monaco di Baviera", .desc = "Boccale lanciato che esplode in schiuma (niente fuoco)",
          .country = C::Germany, .shape = S::Mug, .shot = L::Mug, .damageMult = 1.6f, .rateMult = 0.7f, .speedMult = 0.85f, .life = 1.0f,
          .radius = 6.0f, .explosive = true, .incendiary = false, .color = { 230, 180, 60, 255 }, .color2 = { 245, 245, 240, 255 } },
        { .name = "Boller di Capodanno", .origin = "Berlino", .desc = "Petardi a raffica che scoppiano dove cadono",
          .country = C::Germany, .shape = S::Firecracker, .shot = L::Spark, .damageMult = 0.55f, .rateMult = 2.2f, .speedMult = 0.9f,
          .jitter = 0.12f, .life = 0.8f, .radius = 4.0f, .explosive = true, .color = { 200, 40, 40, 255 }, .color2 = { 240, 220, 120, 255 } },
        // --- Inghilterra ---
        { .name = "Arco lungo inglese", .origin = "Agincourt, 1415", .desc = "Frecce velocissime che attraversano tre nemici",
          .country = C::England, .shape = S::Bow, .shot = L::Arrow, .damageMult = 2.2f, .rateMult = 0.6f, .speedMult = 2.4f, .spread = 0.05f,
          .life = 1.5f, .radius = 3.5f, .extraPierce = 3, .color = { 130, 90, 50, 255 } },
        { .name = "Ombrello di Londra", .origin = "Londra", .desc = "Robusto, rapido, e ferma i proiettili: piove sempre, del resto",
          .country = C::England, .shape = S::Umbrella, .damageMult = 1.3f, .rateMult = 1.5f, .melee = true, .meleeRange = 62, .meleeArc = 0.7f,
          .color = { 30, 30, 34, 255 }, .color2 = { 140, 90, 50, 255 } },
        { .name = "Freccette da pub", .origin = "Pub inglesi", .desc = "Tre freccette a ventaglio, rapidissime",
          .country = C::England, .shape = S::Darts, .shot = L::Dart, .damageMult = 0.5f, .rateMult = 2.0f, .speedMult = 1.6f, .spread = 0.08f,
          .life = 0.9f, .radius = 2.5f, .pellets = 2, .color = { 200, 40, 40, 255 }, .color2 = { 240, 240, 240, 255 } },
        { .name = "Mazza da cricket", .origin = "Lord's, Londra", .desc = "Rispedisce al mittente i colpi nemici",
          .country = C::England, .shape = S::Club, .damageMult = 1.8f, .rateMult = 0.9f, .melee = true, .meleeRange = 64, .meleeArc = 0.9f,
          .knockback = 30, .reflect = true, .color = { 220, 200, 150, 255 }, .color2 = { 40, 60, 120, 255 } },
        // --- Spagna ---
        { .name = "Espada toledana", .origin = "Toledo", .desc = "Acciaio di Toledo: equilibrata e affilata",
          .country = C::Spain, .shape = S::Blade, .damageMult = 2.2f, .rateMult = 1.0f, .melee = true, .meleeRange = 72, .meleeArc = 0.7f,
          .color = { 215, 220, 230, 255 }, .color2 = { 180, 140, 40, 255 } },
        { .name = "Navaja", .origin = "Albacete", .desc = "Coltello a serramanico: cortissimo, ma velocissimo",
          .country = C::Spain, .shape = S::Dagger, .damageMult = 0.9f, .rateMult = 3.0f, .melee = true, .meleeRange = 46, .meleeArc = 0.6f,
          .knockback = 6, .color = { 200, 205, 210, 255 }, .color2 = { 90, 60, 40, 255 } },
        { .name = "Banderillas", .origin = "Siviglia", .desc = "Due aste colorate che trapassano i nemici",
          .country = C::Spain, .shape = S::Banderilla, .shot = L::Arrow, .damageMult = 1.2f, .rateMult = 0.8f, .speedMult = 1.3f, .spread = 0.12f,
          .life = 1.0f, .radius = 4.0f, .pellets = 1, .extraPierce = 1, .color = { 240, 200, 40, 255 }, .color2 = { 200, 40, 40, 255 } },
        { .name = "Petardi delle Fallas", .origin = "Valencia", .desc = "Rimbalzano una volta e poi esplodono in fiamme",
          .country = C::Spain, .shape = S::Firecracker, .shot = L::Spark, .damageMult = 1.2f, .rateMult = 0.65f, .speedMult = 0.9f, .life = 1.3f,
          .radius = 5.0f, .bounces = 1, .explosive = true, .color = { 240, 140, 30, 255 }, .color2 = { 250, 230, 120, 255 } },
        // --- Leggendarie: le regala il GameMaster a chi completa le sue sfide ---
        { .name = "Kusanagi", .origin = "Santuario di Atsuta", .desc = "La spada sacra del Giappone: ogni fendente lancia un'onda di vento",
          .country = C::Japan, .shape = S::Blade, .shot = L::Wave, .damageMult = 3.0f, .rateMult = 1.0f, .melee = true, .meleeRange = 84,
          .meleeArc = 0.9f, .legendary = true, .color = { 150, 230, 255, 255 }, .color2 = { 40, 40, 60, 255 } },
        { .name = "Excalibur", .origin = "Camelot", .desc = "La spada di Re Artu': d'argento, e ogni nemico colpito ti cura",
          .country = C::England, .shape = S::Blade, .damageMult = 3.2f, .rateMult = 0.9f, .melee = true, .meleeRange = 80, .meleeArc = 0.9f,
          .silver = true, .legendary = true, .color = { 230, 240, 255, 255 }, .color2 = { 220, 180, 60, 255 } },
        { .name = "Durlindana", .origin = "Roncisvalle", .desc = "La spada di Orlando: d'argento, e rispedisce indietro i colpi",
          .country = C::France, .shape = S::Blade, .damageMult = 3.0f, .rateMult = 1.0f, .melee = true, .meleeRange = 80, .meleeArc = 0.9f,
          .reflect = true, .silver = true, .legendary = true, .color = { 240, 230, 200, 255 }, .color2 = { 140, 30, 40, 255 } },
        { .name = "Tizona", .origin = "Burgos (El Cid)", .desc = "La spada del Cid: chi viene colpito resta paralizzato dalla paura",
          .country = C::Spain, .shape = S::Blade, .damageMult = 2.8f, .rateMult = 1.1f, .melee = true, .meleeRange = 76, .meleeArc = 0.8f,
          .stun = 0.9f, .legendary = true, .color = { 250, 200, 120, 255 }, .color2 = { 120, 20, 30, 255 } },
        { .name = "Balmung", .origin = "Saga dei Nibelunghi", .desc = "La spada di Sigfrido: colpisce tutto cio' che ha intorno, muri o non muri",
          .country = C::Germany, .shape = S::Blade, .damageMult = 3.2f, .rateMult = 0.8f, .melee = true, .meleeRange = 96, .meleeArc = 1.4f,
          .legendary = true, .color = { 180, 220, 160, 255 }, .color2 = { 60, 40, 30, 255 } },
        { .name = "Martello di John Henry", .origin = "Virginia Occidentale", .desc = "Ogni colpo fa tremare la terra intorno a te",
          .country = C::USA, .shape = S::Hammer, .damageMult = 3.6f, .rateMult = 0.6f, .melee = true, .meleeRange = 70, .meleeArc = 1.0f,
          .knockback = 40, .legendary = true, .color = { 150, 150, 156, 255 }, .color2 = { 140, 100, 60, 255 } },
    };
    return list;
}

// ---------------------------------------------------------------------------
// Modellini 3D (centrati nell'origine, lunghi circa mezza casella)
// ---------------------------------------------------------------------------

void DrawWeapon3D(int id) {
    const Weapon& w = AllWeapons()[id];
    const Color c = w.color, c2 = w.color2;
    switch (w.shape) {
        case S::Talisman:
            DrawCube({ 0, 0, 0 }, 0.02f, 0.36f, 0.16f, c);
            DrawCube({ 0.012f, 0.12f, 0 }, 0.005f, 0.05f, 0.16f, c2);
            break;
        case S::Blade: case S::Dagger: {
            float len = w.shape == S::Dagger ? 0.24f : 0.3f + w.meleeRange / 300.0f;
            DrawCube({ 0, len / 2 - 0.04f, 0 }, 0.02f, len, 0.035f, c);
            DrawCube({ 0, -0.06f, 0 }, 0.08f, 0.02f, 0.1f, Shade(c2, 1.2f));
            DrawCube({ 0, -0.16f, 0 }, 0.035f, 0.18f, 0.035f, c2);
            if (w.legendary) DrawSphereEx({ 0, len * 0.5f, 0 }, 0.12f, 6, 6, Fade(c, 0.25f));
            break;
        }
        case S::Club: case S::Hammer:
            DrawCylinderEx({ 0, -0.25f, 0 }, { 0, 0.2f, 0 }, 0.025f, 0.05f, 6, c2);
            if (w.shape == S::Hammer) DrawCube({ 0, 0.24f, 0 }, 0.08f, 0.1f, 0.24f, c);
            else DrawCylinderEx({ 0, 0.05f, 0 }, { 0, 0.3f, 0 }, 0.05f, 0.065f, 7, c);
            break;
        case S::Spear: case S::Banderilla:
            DrawCylinderEx({ 0, -0.35f, 0 }, { 0, 0.3f, 0 }, 0.015f, 0.015f, 5, c);
            DrawCylinderEx({ 0, 0.3f, 0 }, { 0, 0.4f, 0 }, 0.03f, 0.0f, 5, { 200, 205, 210, 255 });
            for (int k = 0; k < 3; ++k) DrawCylinderEx({ 0, -0.2f + k * 0.2f, 0 }, { 0, -0.15f + k * 0.2f, 0 }, 0.018f, 0.018f, 5, c2);
            break;
        case S::Bow:
            for (int k = 0; k < 6; ++k) {
                float a0 = -0.9f + k * 0.3f, a1 = a0 + 0.3f;
                DrawCylinderEx({ 0, std::sin(a0) * 0.4f, std::cos(a0) * 0.12f - 0.08f },
                               { 0, std::sin(a1) * 0.4f, std::cos(a1) * 0.12f - 0.08f }, 0.015f, 0.015f, 5, c);
            }
            DrawCylinderEx({ 0, -0.31f, 0 }, { 0, 0.31f, 0 }, 0.004f, 0.004f, 3, { 230, 230, 220, 255 });
            break;
        case S::Crossbow:
            DrawCube({ 0, 0, 0 }, 0.05f, 0.4f, 0.05f, c);
            DrawCylinderEx({ 0, 0.15f, -0.2f }, { 0, 0.15f, 0.2f }, 0.012f, 0.012f, 4, c2);
            break;
        case S::Sling:
            DrawCube({ 0, -0.12f, 0 }, 0.04f, 0.2f, 0.04f, c2);
            DrawCube({ 0, 0.04f, -0.07f }, 0.04f, 0.16f, 0.04f, c2);
            DrawCube({ 0, 0.04f, 0.07f }, 0.04f, 0.16f, 0.04f, c2);
            DrawSphereEx({ 0, 0.08f, 0 }, 0.05f, 6, 6, c);
            break;
        case S::Rocket: case S::Firecracker:
            DrawCylinderEx({ 0, -0.2f, 0 }, { 0, 0.22f, 0 }, 0.07f, 0.07f, 8, c);
            DrawCylinderEx({ 0, 0.06f, 0 }, { 0, 0.1f, 0 }, 0.075f, 0.075f, 8, c2);
            DrawCylinderEx({ 0, 0.22f, 0 }, { 0, 0.34f, 0 }, 0.07f, 0.0f, 8, c2);
            break;
        case S::Pistol:
            DrawCube({ 0, 0.06f, 0 }, 0.04f, 0.06f, 0.3f, c);
            DrawCube({ 0, -0.04f, -0.1f }, 0.04f, 0.16f, 0.06f, c2);
            break;
        case S::LongGun:
            DrawCube({ 0, 0, 0 }, 0.03f, 0.03f, 0.6f, c);
            DrawCube({ 0, -0.02f, -0.22f }, 0.05f, 0.08f, 0.22f, c2);
            break;
        case S::Axe:
            DrawCylinderEx({ 0, -0.22f, 0 }, { 0, 0.18f, 0 }, 0.02f, 0.02f, 5, c2);
            DrawCube({ 0, 0.14f, 0.06f }, 0.02f, 0.12f, 0.14f, c);
            break;
        case S::Ball:
            DrawSphereEx({ 0, 0, 0 }, 0.1f, 8, 8, c);
            break;
        case S::Bottle:
            DrawCylinder({ 0, -0.2f, 0 }, 0.07f, 0.07f, 0.26f, 8, c);
            DrawCylinder({ 0, 0.06f, 0 }, 0.07f, 0.025f, 0.08f, 8, c);
            DrawCylinder({ 0, 0.14f, 0 }, 0.03f, 0.03f, 0.06f, 6, c2);
            break;
        case S::Mug:
            DrawCylinder({ 0, -0.12f, 0 }, 0.09f, 0.09f, 0.22f, 8, c);
            DrawCylinder({ 0, 0.1f, 0 }, 0.095f, 0.095f, 0.05f, 8, c2);
            DrawCube({ 0, 0, 0.11f }, 0.02f, 0.12f, 0.04f, c);
            break;
        case S::Darts:
            for (int k = -1; k <= 1; ++k) DrawCylinderEx({ 0, -0.12f, k * 0.05f }, { 0, 0.14f, k * 0.05f }, 0.012f, 0.0f, 4, c);
            break;
        case S::Umbrella:
            DrawCylinderEx({ 0, -0.3f, 0 }, { 0, 0.32f, 0 }, 0.012f, 0.012f, 4, c2);
            DrawCylinder({ 0, -0.05f, 0 }, 0.05f, 0.03f, 0.3f, 8, c);
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

static void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

void DrawWeaponInHand(int id, float bobX, float bobY, float recoil, float swing, bool ready, Color accent) {
    const Weapon& w = AllWeapons()[id];
    const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    const Color shadow = Fade(BLACK, 0.35f);
    const Color c = w.color, c2 = w.color2;

    switch (w.shape) {
        case S::Talisman: {
            float hx = sw - 290 + bobX, hy = sh - 300 + bobY + recoil * 26;
            float angle = 14 - recoil * 6;
            DrawRectanglePro({ hx + 4, hy + 4, 62, 140 }, { 31, 70 }, angle, shadow);
            DrawRectanglePro({ hx, hy, 62, 140 }, { 31, 70 }, angle, c);
            DrawRectanglePro({ hx, hy - 52, 62, 14 }, { 31, 70 }, angle, c2);
            for (int k = 0; k < 4; ++k)
                DrawRectanglePro({ hx, hy - 22 + k * 22.0f, 26, 5 }, { 13, 2.5f }, angle, { 30, 25, 25, 255 });
            if (recoil > 0.8f) DrawCircle((int)hx - 30, (int)hy - 80, 16, Fade(accent, 0.6f));
            break;
        }
        case S::Blade: case S::Dagger: case S::Club: case S::Hammer: case S::Umbrella: {
            // A riposo l'arma punta in alto a sinistra; il fendente la fa passare da destra a sinistra
            float angle = swing > 0 ? -150 + (1 - swing) * 120 : -125.0f;
            Vector2 pivot{ sw - 230 + bobX, sh - 150 + bobY };
            float len = w.shape == S::Dagger ? 110 : w.shape == S::Blade ? 160 + w.meleeRange * 1.4f : 220;
            float thick = w.shape == S::Blade ? (w.meleeRange > 88 ? 20.0f : w.meleeRange > 80 && w.rateMult > 1.5f ? 7.0f : 14.0f) : 14.0f;
            Bar(pivot, 40, 0, 90, 18, angle, c2); // impugnatura
            if (w.shape == S::Blade || w.shape == S::Dagger) {
                for (int k = 0; k < 4; ++k) Bar(pivot, 8 + k * 20.0f, 0, 6, 18, angle, Shade(c2, 1.6f));
                Bar(pivot, -8, 0, 10, 46, angle, Shade(c2, 1.3f));                 // guardia
                Bar(pivot, -len / 2 - 10, 0, len, thick, angle, c);                 // lama
                Bar(pivot, -len / 2 - 10, thick * 0.3f, len, thick * 0.3f, angle, Shade(c, 1.15f)); // filo
                if (w.legendary) Bar(pivot, -len / 2 - 10, 0, len + 16, thick + 10, angle, Fade(c, 0.25f));
            } else if (w.shape == S::Umbrella) {
                Bar(pivot, -100, 0, 200, 8, angle, { 60, 60, 64, 255 });
                Bar(pivot, -110, 0, 170, 34, angle, c); // l'ombrello chiuso
                Bar(pivot, -205, 0, 20, 6, angle, { 60, 60, 64, 255 });
            } else { // mazza, martello
                Bar(pivot, -60, 0, 140, 16, angle, c2);
                if (w.shape == S::Hammer) Bar(pivot, -150, 0, 50, 90, angle, c);
                else Bar(pivot, -150, 0, 120, 30, angle, c);
                if (id == W_MORGENSTERN)
                    for (int k = 0; k < 4; ++k) Bar(pivot, -110 - k * 24.0f, 20, 8, 14, angle, { 200, 200, 210, 255 }); // punte
            }
            if (swing > 0.3f) // scia del fendente
                for (int k = 0; k < 3; ++k) Bar(pivot, -len / 2 - 20, 0, len, 10, angle - 12 - k * 10, Fade(WHITE, 0.15f * (3 - k)));
            break;
        }
        case S::Spear: { // affondo in avanti invece del fendente
            float push = swing > 0 ? sinf(swing * PI) * 120 : 0;
            Vector2 pivot{ sw - 300 + bobX - push * 0.5f, sh - 120 + bobY - push * 0.4f };
            float angle = -140;
            Bar(pivot, -150, 0, 420, 14, angle, c);
            for (int k = 0; k < 6; ++k) Bar(pivot, -40 - k * 56.0f, 0, 22, 15, angle, c2); // strisce colorate
            Bar(pivot, -380, 0, 50, 24, angle, { 200, 205, 210, 255 });
            break;
        }
        case S::Bow: {
            float cx = sw - 260 + bobX, cy = sh - 300 + bobY;
            Vector2 prev{};
            for (int k = 0; k <= 12; ++k) { // l'arco, una curva fatta di segmenti
                float a = -1.0f + k * (2.0f / 12);
                Vector2 p{ cx + std::cos(a) * 90 - 90 + recoil * 8, cy + std::sin(a) * 230 };
                if (k) DrawLineEx(prev, p, 9, c);
                prev = p;
            }
            Vector2 top{ cx + std::cos(-1.0f) * 90 - 90, cy - std::sin(1.0f) * 230 };
            Vector2 bottom{ top.x, cy + std::sin(1.0f) * 230 };
            float pull = ready ? 40.0f : 0.0f;
            Vector2 nock{ top.x + pull, cy };
            DrawLineEx(top, nock, 2, { 235, 235, 225, 255 });
            DrawLineEx(nock, bottom, 2, { 235, 235, 225, 255 });
            if (ready) {
                DrawLineEx(nock, { nock.x - 200, cy - 40 }, 4, { 120, 80, 50, 255 });
                Tri({ nock.x - 214, cy - 43 }, { nock.x - 198, cy - 33 }, { nock.x - 196, cy - 46 }, { 180, 185, 195, 255 });
            }
            break;
        }
        case S::Crossbow: {
            Vector2 pivot{ sw - 260 + bobX, sh - 170 + bobY + recoil * 20 };
            Bar(pivot, 0, 20, 60, 170, -12, c);                 // fusto
            Bar(pivot, 0, -50, 260, 14, -12, c2);               // arco d'acciaio
            DrawLineEx({ pivot.x - 120, pivot.y - 30 }, { pivot.x + 6, pivot.y + (ready ? 0.0f : -36.0f) }, 2, LIGHTGRAY);
            DrawLineEx({ pivot.x + 130, pivot.y - 86 }, { pivot.x + 6, pivot.y + (ready ? 0.0f : -36.0f) }, 2, LIGHTGRAY);
            if (ready) Bar(pivot, -10, -40, 8, 120, -12, { 120, 80, 50, 255 });
            break;
        }
        case S::Sling: {
            Vector2 pivot{ sw - 250 + bobX, sh - 190 + bobY + recoil * 14 };
            Bar(pivot, 0, 60, 22, 110, 0, c2);
            Bar(pivot, -26, -20, 16, 80, -18, c2);
            Bar(pivot, 26, -20, 16, 80, 18, c2);
            float back = ready ? 30.0f : 6.0f;
            Vector2 l{ pivot.x - 40, pivot.y - 56 }, r{ pivot.x + 40, pivot.y - 56 }, pouch{ pivot.x, pivot.y - 30 + back };
            DrawLineEx(l, pouch, 5, { 90, 60, 40, 255 });
            DrawLineEx(r, pouch, 5, { 90, 60, 40, 255 });
            if (ready) DrawCircleV(pouch, 11, c);
            break;
        }
        case S::Rocket: {
            Vector2 pivot{ sw - 250 + bobX + recoil * 10, sh - 200 + bobY + recoil * 30 };
            float angle = -60;
            Bar(pivot, 0, 6, 250, 64, angle, shadow);
            Bar(pivot, 0, 0, 250, 64, angle, c);
            for (int k = 0; k < 3; ++k) Bar(pivot, -90 + k * 70.0f, 0, 12, 66, angle, c2);
            Bar(pivot, -128, 0, 10, 52, angle, { 40, 30, 30, 255 });
            if (recoil > 0.7f) DrawCircle((int)(pivot.x - 70), (int)(pivot.y - 120), 30 * recoil, Fade({ 255, 200, 80, 255 }, 0.7f));
            break;
        }
        case S::Pistol: case S::LongGun: {
            bool rifle = w.shape == S::LongGun;
            Vector2 pivot{ sw - 300 + bobX, sh - 150 + bobY + recoil * 26 };
            float angle = -8 - recoil * 10;
            if (rifle) {
                Bar(pivot, -40, 0, 380, 22, angle, c);       // canna
                Bar(pivot, 110, 16, 170, 44, angle + 8, c2); // calcio
                if (id == W_CHASTEL) Bar(pivot, -200, 0, 30, 24, angle, { 230, 235, 245, 255 }); // argento
            } else {
                Bar(pivot, -60, 0, 170, 26, angle, c);
                Bar(pivot, 10, 0, 50, 40, angle, Shade(c, 0.8f));  // tamburo
                Bar(pivot, 50, 46, 34, 80, angle + 16, c2);        // calcio
            }
            if (recoil > 0.7f) DrawCircle((int)(pivot.x - (rifle ? 240 : 150)), (int)(pivot.y - 30), 22 * recoil, Fade({ 255, 220, 120, 255 }, 0.8f));
            break;
        }
        default: { // oggetti da lanciare: li tieni in mano finche' non parte il colpo
            Vector2 hand{ sw - 260 + bobX, sh - 170 + bobY + recoil * 60 };
            if (!ready && recoil > 0.3f) break; // appena lanciato: la mano e' vuota
            DrawCircleV({ hand.x + 30, hand.y + 40 }, 46, { 220, 180, 150, 255 }); // la mano
            switch (w.shape) {
                case S::Axe:
                    Bar(hand, 0, -40, 18, 170, -20, c2);
                    Bar(hand, 30, -120, 70, 50, -20, c);
                    break;
                case S::Ball: DrawCircleV({ hand.x, hand.y - 30 }, 44, c); DrawCircleLines((int)hand.x, (int)hand.y - 30, 44, Shade(c, 0.6f)); break;
                case S::Bottle:
                    Bar(hand, 0, -40, 60, 150, -10, c);
                    Bar(hand, 12, -130, 26, 50, -10, c2);
                    break;
                case S::Mug:
                    Bar(hand, 0, -30, 90, 120, 0, c);
                    Bar(hand, 0, -100, 96, 26, 0, c2);
                    Bar(hand, 56, -30, 22, 70, 0, c);
                    break;
                case S::Darts:
                    for (int k = -1; k <= 1; ++k) Bar(hand, k * 26.0f, -70, 8, 110, -10 + k * 8.0f, c);
                    break;
                case S::Banderilla:
                    for (int k = -1; k <= 1; k += 2) {
                        Bar(hand, k * 30.0f, -90, 12, 220, k * 6.0f, c);
                        for (int s = 0; s < 4; ++s) Bar(hand, k * 30.0f, -150 + s * 40.0f, 14, 14, k * 6.0f, c2);
                    }
                    break;
                case S::Firecracker:
                    Bar(hand, 0, -40, 40, 110, -15, c);
                    Bar(hand, 16, -110, 4, 30, -15, c2);
                    if ((int)(GetTime() * 10) % 2) DrawCircleV({ hand.x + 22, hand.y - 130 }, 6, { 255, 220, 120, 255 }); // miccia
                    break;
                default: break;
            }
            break;
        }
    }
}
