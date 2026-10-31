#pragma once
#include <vector>
#include "raylib.h"

// Armi: si trovano nel labirinto e se ne portano due. Gli artefatti potenziano qualunque arma
// (danno, cadenza, proiettili in piu', bruciatura...), l'arma decide COME si attacca.
enum WeaponId { W_OFUDA, W_KATANA, W_YUMI, W_PACHINKO, W_TAMAYA, W_COUNT };

struct Weapon {
    const char* name;
    const char* origin;   // il luogo a cui rende omaggio
    const char* desc;
    float damageMult;
    float rateMult;
    float speedMult;
    float spread;         // radianti tra un colpo e l'altro quando se ne sparano piu' di uno
    float jitter;         // imprecisione casuale (radianti)
    float life;           // durata di un colpo in secondi
    float radius;         // raggio del colpo in pixel
    int extraPierce;      // nemici in piu' che un colpo attraversa
    int bounces;          // rimbalzi sui muri
    bool explosive;       // esplode all'impatto (e incendia cio' che e' infiammabile)
    bool melee;           // corpo a corpo: colpisce subito tutto cio' che e' davanti
    float meleeRange;     // pixel
    float meleeArc;       // mezzo angolo del fendente, in radianti
    Color color;
};

const std::vector<Weapon>& AllWeapons();

// Modellino 3D centrato nell'origine (per le armi a terra)
void DrawWeapon3D(int id);

// L'arma in mano, in basso a destra. recoil va da 1 (appena sparato) a 0, swing da 1 a 0 durante il fendente
void DrawWeaponInHand(int id, float bobX, float bobY, float recoil, float swing, bool ready, Color accent);
