#pragma once
#include <string>
#include <vector>
#include "raylib.h"

// Comandi a schermo per iPad e telefoni. In partita: stick per muoversi, stick di tiro (spara e sposta
// la visuale col mirino al centro), tasti rotondi per le azioni e, dove non ci sono comandi, trascinare
// il dito sposta la visuale senza sparare. Posizione, grandezza, sensibilita' e stick fissi o mobili
// si scelgono nell'editor ("Modifica comandi touch") e si salvano nel profilo.
enum TouchButton {
    TB_ATTACK, TB_JUMP, TB_DASH, TB_SWAP, TB_USE, TB_MATCH, TB_PAUSE, // in partita
    TB_UP, TB_DOWN, TB_LEFT, TB_RIGHT, TB_OK, TB_BACK,                // nei menu
    TB_SKIP,                                                          // nelle animazioni
    TB_COUNT
};

// I comandi della partita che si possono spostare (l'ordine conta: e' quello del salvataggio)
enum TouchControl { TC_MOVE, TC_AIM, TC_JUMP, TC_DASH, TC_SWAP, TC_USE, TC_MATCH, TC_PAUSE, TC_COUNT };

// Come il giocatore ha sistemato i comandi
struct TouchSetup {
    struct Ctl {
        float x = 0, y = 0;    // centro, in frazione dello schermo (0..1)
        float size = 1;        // grandezza rispetto a quella normale
        float sens = 1;        // sensibilita' (solo stick)
        bool floating = false; // stick mobile: parte da dove si appoggia il dito, nella sua meta' di schermo
    };
    bool valid = false;        // false = non ancora calcolata (serve la grandezza dello schermo)
    Ctl ctl[TC_COUNT];
    float lookSens = 1;        // sensibilita' della visuale trascinando dove non ci sono comandi
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
    Vector2 LookDelta() const { return look; } // pixel trascinati sulla visuale libera (gia' per la sensibilita')
    float AimTurn() const { return aimTurn; }   // stick di tiro: rotazione, negativo = a sinistra
    float AimPitch() const { return aimPitch; } // stick di tiro: -1 guarda giu' ... +1 guarda su

    // Editor dei comandi: mentre e' aperto Update e Draw servono solo all'editor
    void StartEditing();
    bool Editing() const { return editing; }
    bool TakeEditDone() { bool d = editDone; editDone = false; return d; } // ha appena premuto "Fatto"
    void DrawEditor() const;

    std::string SaveSetup() const;          // per il profilo
    void LoadSetup(const std::string& text);

private:
    enum class Role { None, Button, Stick, Aim, Look };
    struct Finger { int id; Role role; int button; Vector2 start, last, pos; };

    void EnsureSetup() const;
    void UpdateGame(std::vector<Finger>& now, Finger& f);
    void UpdateEditor();
    Vector2 CtlCenter(int c) const;
    float CtlRadius(int c) const;
    void DrawControls(bool editor) const;

    Layout layout = Layout::Game;
    std::vector<Finger> fingers;
    bool down[TB_COUNT] = {}, prev[TB_COUNT] = {};
    bool tapped = false;
    Vector2 move{ 0, 0 };
    Vector2 look{ 0, 0 };
    float aimTurn = 0, aimPitch = 0;
    Vector2 moveVec{ 0, 0 }, aimVec{ 0, 0 }; // spinta degli stick (per disegnarli)
    mutable TouchSetup setup;

    // Editor
    bool editing = false, editDone = false;
    int selected = TC_AIM;
    int editFinger = -1;  // id del dito che sta trascinando qualcosa
    int dragCtl = -1;     // comando trascinato
    int dragSlider = -1;  // barra trascinata (vedi Touch.cpp)
    Vector2 dragOffset{ 0, 0 };
};
