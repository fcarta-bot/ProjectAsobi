#pragma once
#include "raylib.h"

// Input "grezzo" che raylib non vede o non legge bene.
// pad e' lo stesso indice dei joystick di raylib.

// Il touchpad (cliccato) di un controller PlayStation: false se non c'e' o non e' premuto
bool PadTouchpadDown(int pad);

// Dita sullo schermo (iPad, telefoni). Ogni dito ha un id che resta uguale finche' non si stacca;
// pos e' in pixel dello schermo di gioco
struct RawTouch { int id; Vector2 pos; };
int RawTouchCount();
RawTouch RawTouchAt(int i);
