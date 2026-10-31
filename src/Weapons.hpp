#pragma once
#include <vector>
#include "raylib.h"
#include "Dungeon.hpp" // Country

// Armi: si trovano nel labirinto e se ne portano due. Gli artefatti potenziano qualunque arma
// (danno, cadenza, proiettili in piu', bruciatura...), l'arma decide COME si attacca.
// Ogni paese ha le sue armi tipiche; le leggendarie si ottengono solo con le sfide del GameMaster.
// L'ordine deve corrispondere alla lista in AllWeapons()
enum WeaponId {
    W_OFUDA, W_KATANA, W_YUMI, W_PACHINKO, W_TAMAYA,         // Giappone
    W_REVOLVER, W_SHOTGUN, W_BASEBALL, W_TOMAHAWK,           // Stati Uniti
    W_GLADIO, W_BALESTRA, W_LANCIA, W_PIEDIGROTTA,           // Italia
    W_FIORETTO, W_CHASTEL, W_BOULE, W_CHAMPAGNE,             // Francia
    W_ZWEIHANDER, W_MORGENSTERN, W_BOCCALE, W_BOLLER,        // Germania
    W_LONGBOW, W_OMBRELLO, W_FRECCETTE, W_CRICKET,           // Inghilterra
    W_TOLEDO, W_NAVAJA, W_BANDERILLAS, W_FALLAS,             // Spagna
    W_KUSANAGI, W_EXCALIBUR, W_DURLINDANA, W_TIZONA, W_BALMUNG, W_JOHN_HENRY, // leggendarie
    W_COUNT
};
constexpr int W_FIRST_LEGENDARY = W_KUSANAGI;

// Come si disegna (in mano e a terra)
enum class WeaponShape {
    Talisman, Blade, Bow, Sling, Rocket, Pistol, LongGun, Club, Spear, Crossbow,
    Axe, Ball, Bottle, Mug, Darts, Umbrella, Dagger, Banderilla, Firecracker, Hammer,
};
// Come si disegna il colpo in volo
enum class ShotLook { Paper, Arrow, Ball, Rocket, Bullet, Axe, Cork, Mug, Dart, Spark, Wave };

struct Weapon {
    const char* name;
    const char* origin;   // il luogo a cui rende omaggio
    const char* desc;
    Country country;
    WeaponShape shape;
    ShotLook shot = ShotLook::Paper;
    float damageMult = 1;
    float rateMult = 1;
    float speedMult = 1;
    float spread = 0.16f; // radianti tra un colpo e l'altro quando se ne sparano piu' di uno
    float jitter = 0;     // imprecisione casuale (radianti)
    float life = 1.2f;    // durata di un colpo in secondi
    float radius = 4.5f;  // raggio del colpo in pixel
    int pellets = 0;      // colpi in piu' a ogni sparo (fucili a pallettoni, banderillas...)
    int extraPierce = 0;  // nemici in piu' che un colpo attraversa
    int bounces = 0;      // rimbalzi sui muri
    bool explosive = false;
    bool incendiary = true; // l'esplosione incendia cio' che e' infiammabile
    bool melee = false;     // corpo a corpo: colpisce subito tutto cio' che e' davanti
    float meleeRange = 0;   // pixel
    float meleeArc = 0;     // mezzo angolo del fendente, in radianti
    float knockback = 18;   // spinta del fendente
    float stun = 0;         // secondi di stordimento a chi viene colpito
    bool reflect = false;   // il fendente rimanda indietro i colpi nemici (invece di fermarli)
    bool silver = false;    // d'argento: ferisce anche le bestie che temono solo l'argento
    bool legendary = false;
    Color color{ 200, 200, 200, 255 };
    Color color2{ 60, 50, 40, 255 }; // impugnatura, dettagli
};

const std::vector<Weapon>& AllWeapons();

// Modellino 3D centrato nell'origine (per le armi a terra)
void DrawWeapon3D(int id);

// L'arma in mano, in basso a destra. recoil va da 1 (appena sparato) a 0, swing da 1 a 0 durante il fendente
void DrawWeaponInHand(int id, float bobX, float bobY, float recoil, float swing, bool ready, Color accent);
