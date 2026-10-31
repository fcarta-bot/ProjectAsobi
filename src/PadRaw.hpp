#pragma once

// Tasti "grezzi" del joystick, oltre ai 15 standard che raylib conosce.
// Serve per il touchpad dei controller PlayStation, che raylib non vede.
// pad e' lo stesso indice dei joystick di raylib.

// Il touchpad (cliccato) di un controller PlayStation: false se non c'e' o non e' premuto
bool PadTouchpadDown(int pad);
