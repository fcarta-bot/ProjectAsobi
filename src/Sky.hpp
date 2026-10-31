#pragma once
#include "Dungeon.hpp"

// Cielo e profilo della citta' all'orizzonte (pagode, colline, grattacieli...).
// Va disegnato prima del mondo 3D: i muri del labirinto lo coprono dove sono piu' vicini.
// yaw e' la direzione dello sguardo, fovy il campo visivo verticale in gradi.
void DrawSky(const District& d, float yaw, float fovy);
