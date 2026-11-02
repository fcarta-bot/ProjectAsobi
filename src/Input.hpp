#pragma once
#include "raylib.h"
#include "Touch.hpp"

// Riunisce tastiera/mouse, joystick e comandi touch in un'unica interfaccia.
// Un joystick va "collegato" al gioco prima di poterlo usare: premendo START o A
// su di esso, oppure scegliendolo dal menu Controller.
class Input {
public:
    static constexpr int MAX_PADS = 4;

    // Famiglia del joystick: serve solo a mostrare i nomi giusti dei tasti
    enum class PadFamily { Xbox, PlayStation, Nintendo, Generic };

    void Update(); // da chiamare una volta per frame, prima di leggere i comandi

    void Connect(int padIndex);
    void Disconnect();
    int ConnectedPad() const { return pad; }
    bool PadConnected() const { return pad >= 0; }
    bool UsingPad() const { return usingPad; } // l'ultimo dispositivo usato e' il joystick
    bool LostPad() const { return lostPad; }   // vero nel frame in cui il joystick collegato si stacca

    // Touch: l'ultimo dispositivo usato e' lo schermo (iPad). Il gioco dice quali tasti mostrare
    bool UsingTouch() const { return usingTouch; }
    void SetTouchLayout(TouchControls::Layout l) { touch.SetLayout(l); }
    float TouchLook() const { return usingTouch ? touch.LookDelta() : 0.0f; } // pixel trascinati per girarsi
    float PitchAxis() const { return usingTouch ? touch.AimPitch() : 0.0f; }  // -1 giu' ... +1 su (stick di tiro)
    void DrawTouch() const { if (usingTouch) touch.Draw(); }

    // Indice di un joystick non ancora collegato su cui e' appena stato premuto START o A, altrimenti -1
    int NewPadRequest() const;

    // Comandi di gioco
    // Movimento rispetto allo sguardo: x = laterale (+ destra), y = avanti/indietro (- avanti).
    // Lunghezza tra 0 e 1 (lo stick e' analogico)
    Vector2 Move() const;
    float TurnAxis() const; // rotazione dello sguardo da stick destro o frecce sx/dx, tra -1 e 1
    bool FireDown() const;
    bool JumpPressed() const;
    // Scatto. "dir" e' la direzione rispetto allo sguardo (come Move); (0, 0) = nella direzione di marcia
    bool DashPressed(Vector2& dir) const;
    bool InteractPressed() const;
    bool SwapWeaponPressed() const;
    bool GameMenuPressed() const;     // menu di gioco (pausa, comandi, esci): Esc / P, touchpad, Share, tasto PS
    bool MatchMenuPressed() const;    // menu di partita (inventario, armi, mappa, statistiche): Tab / I, Options
    bool PadMenuButtonPressed() const; // uno qualsiasi dei tasti menu del joystick
    bool TouchpadPressed(int padIndex) const; // touchpad PlayStation appena cliccato
    bool TouchpadDown(int padIndex) const;
    bool MenuPressed() const;         // F1: scorciatoia per il menu Controller
    bool FullscreenPressed() const;   // F11 o Alt+Invio: schermo intero / finestra

    // Comandi dei menu
    bool UpPressed() const { return up; }
    bool DownPressed() const { return down; }
    bool LeftPressed() const { return left; }   // anche L1/LB: per cambiare scheda
    bool RightPressed() const { return right; } // anche R1/RB
    bool ConfirmPressed() const;
    bool BackPressed() const;

    // Nomi dei tasti per il joystick collegato (es. "Triangolo" su PlayStation, "Y" su Xbox)
    PadFamily Family() const { return FamilyOf(pad); }
    const char* Btn(int button) const { return ButtonName(Family(), button); }
    static PadFamily FamilyOf(int padIndex);
    static const char* ButtonName(PadFamily family, int button);

private:
    bool PadDown(int button) const;
    bool PadPressed(int button) const;
    bool ScreenDown(TouchButton b) const { return usingTouch && touch.Down(b); }       // tasto a schermo
    bool ScreenPressed(TouchButton b) const { return usingTouch && touch.Pressed(b); }
    Vector2 Stick(int axisX, int axisY) const;

    int pad = -1;
    bool usingPad = false;
    bool usingTouch = false;
    TouchControls touch;
    bool lostPad = false;
    bool up = false, down = false, left = false, right = false;
    bool stickUpHeld = false, stickDownHeld = false, stickLeftHeld = false, stickRightHeld = false;
    // raylib non vede il touchpad: lo leggiamo a parte e ricordiamo il frame prima per capire il "click"
    bool touchDown[MAX_PADS] = {}, touchPrev[MAX_PADS] = {};
};
