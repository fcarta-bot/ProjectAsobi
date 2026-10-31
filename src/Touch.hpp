#pragma once
#include <vector>
#include "raylib.h"

// Comandi a schermo per iPad e telefoni: stick virtuale a sinistra, trascinare a destra per girarsi,
// tasti rotondi per le azioni. La disposizione dei tasti sta tutta nella tabella in Touch.cpp.
enum TouchButton {
    TB_ATTACK, TB_JUMP, TB_DASH, TB_SWAP, TB_USE, TB_MATCH, TB_PAUSE, // in partita
    TB_UP, TB_DOWN, TB_LEFT, TB_RIGHT, TB_OK, TB_BACK,                // nei menu
    TB_SKIP,                                                          // nelle animazioni
    TB_COUNT
};

class TouchControls {
public:
    // Game = in partita, Menu = frecce + OK/Indietro, Tap = animazioni (tocca ovunque per andare avanti)
    enum class Layout { Game, Menu, Tap };

    void SetLayout(Layout l);
    void Update(); // una volta per frame
    void Draw() const;

    bool Touching() const { return !fingers.empty(); } // almeno un dito sullo schermo
    bool Down(TouchButton b) const { return down[b]; }
    bool Pressed(TouchButton b) const { return down[b] && !prev[b]; }
    bool Tapped() const { return tapped; }     // dito appoggiato fuori dai tasti (layout Tap)
    Vector2 Move() const { return move; }      // come Input::Move: x laterale, y avanti (-) / indietro (+)
    float LookDelta() const { return lookDx; } // pixel trascinati in orizzontale in questo frame

private:
    enum class Role { None, Button, Stick, Look };
    struct Finger { int id; Role role; int button; Vector2 start, last, pos; };

    Layout layout = Layout::Game;
    std::vector<Finger> fingers;
    bool down[TB_COUNT] = {}, prev[TB_COUNT] = {};
    bool tapped = false;
    Vector2 move{ 0, 0 };
    float lookDx = 0;
};
