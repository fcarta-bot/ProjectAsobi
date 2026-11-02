#include "Input.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include "raymath.h"
#include "PadRaw.hpp"

constexpr float DEADZONE = 0.25f; // sotto questa soglia lo stick e' considerato fermo

// Toglie la zona morta e riporta il valore tra 0 e 1, cosi' il movimento parte morbido
static Vector2 ApplyDeadzone(Vector2 v) {
    float len = Vector2Length(v);
    if (len < DEADZONE) return { 0, 0 };
    float scaled = std::min(1.0f, (len - DEADZONE) / (1.0f - DEADZONE));
    return Vector2Scale(v, scaled / len);
}

void Input::Update() {
    for (int i = 0; i < MAX_PADS; ++i) {
        touchPrev[i] = touchDown[i];
        touchDown[i] = IsGamepadAvailable(i) && PadTouchpadDown(i);
    }

    lostPad = false;
    if (pad >= 0 && !IsGamepadAvailable(pad)) {
        pad = -1;
        usingPad = false;
        lostPad = true;
    }

    // Capisce quale dispositivo si sta usando, per la mira e i suggerimenti a schermo
    bool padActivity = false;
    if (pad >= 0) {
        padActivity = Vector2Length(Stick(GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y)) > 0 ||
                      Vector2Length(Stick(GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y)) > 0;
        for (int b = GAMEPAD_BUTTON_LEFT_FACE_UP; b <= GAMEPAD_BUTTON_RIGHT_THUMB && !padActivity; ++b)
            if (IsGamepadButtonPressed(pad, b)) padActivity = true;
        if (TouchpadPressed(pad)) padActivity = true;
    }
    // Col touch raylib muove anche il "mouse": finche' si tocca lo schermo il mouse non conta
    touch.Update();
    const bool touchActivity = touch.Touching();
    const bool keyActivity = GetKeyPressed() != 0;
    bool keyboardActivity = keyActivity ||
        (!touchActivity && !usingTouch && (Vector2Length(GetMouseDelta()) > 2 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)));
    if (padActivity) usingPad = true;
    else if (keyboardActivity || touchActivity) usingPad = false;
    if (touchActivity) usingTouch = true;
    else if (padActivity || keyActivity) usingTouch = false;

    // Nei menu lo stick funziona come le frecce: conta solo il momento in cui lo si spinge
    float ly = pad >= 0 ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y) : 0.0f;
    bool stickUp = ly < -0.6f, stickDown = ly > 0.6f;
    up = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || PadPressed(GAMEPAD_BUTTON_LEFT_FACE_UP) ||
         (stickUp && !stickUpHeld) || ScreenPressed(TB_UP);
    down = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || PadPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN) ||
           (stickDown && !stickDownHeld) || ScreenPressed(TB_DOWN);
    stickUpHeld = stickUp;
    stickDownHeld = stickDown;

    float lx = pad >= 0 ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X) : 0.0f;
    bool stickLeft = lx < -0.6f, stickRight = lx > 0.6f;
    left = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || PadPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT) ||
           PadPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1) || (stickLeft && !stickLeftHeld) || ScreenPressed(TB_LEFT);
    right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || PadPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) ||
            PadPressed(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) || (stickRight && !stickRightHeld) || ScreenPressed(TB_RIGHT);
    stickLeftHeld = stickLeft;
    stickRightHeld = stickRight;
}

void Input::Connect(int padIndex) {
    pad = padIndex;
    usingPad = true;
    // evita un movimento fantasma nei menu subito dopo il collegamento
    stickUpHeld = stickDownHeld = stickLeftHeld = stickRightHeld = true;
}

void Input::Disconnect() {
    pad = -1;
    usingPad = false;
}

int Input::NewPadRequest() const {
    for (int i = 0; i < MAX_PADS; ++i) {
        if (i == pad || !IsGamepadAvailable(i)) continue;
        if (IsGamepadButtonPressed(i, GAMEPAD_BUTTON_MIDDLE_RIGHT) ||
            IsGamepadButtonPressed(i, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || TouchpadPressed(i))
            return i;
    }
    return -1;
}

bool Input::TouchpadDown(int padIndex) const {
    return padIndex >= 0 && padIndex < MAX_PADS && touchDown[padIndex];
}

bool Input::TouchpadPressed(int padIndex) const {
    return TouchpadDown(padIndex) && !touchPrev[padIndex];
}

Vector2 Input::Move() const {
    Vector2 stick = Stick(GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
    if (Vector2Length(stick) > 0) return stick;
    if (usingTouch && Vector2Length(touch.Move()) > 0) return touch.Move();

    // Le frecce del joystick non muovono: servono allo scatto
    Vector2 dir{ 0, 0 };
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))   dir.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) dir.y += 1;
    if (IsKeyDown(KEY_A)) dir.x -= 1; // passo laterale
    if (IsKeyDown(KEY_D)) dir.x += 1;
    return Vector2Normalize(dir);
}

float Input::TurnAxis() const {
    float turn = Stick(GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y).x;
    if (usingTouch) turn += touch.AimTurn(); // stick di tiro a schermo
    if (IsKeyDown(KEY_LEFT)) turn -= 1;
    if (IsKeyDown(KEY_RIGHT)) turn += 1;
    return std::clamp(turn, -1.0f, 1.0f);
}

bool Input::FireDown() const {
    // Col joystick si spara col grilletto destro (lo stick destro serve a girarsi)
    return (!usingTouch && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) || ScreenDown(TB_ATTACK) ||
           PadDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_2) || PadDown(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
}

bool Input::JumpPressed() const {
    // Il tasto in basso: X (croce) su PlayStation, A su Xbox, B su Nintendo
    return IsKeyPressed(KEY_SPACE) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || ScreenPressed(TB_JUMP);
}

bool Input::DashPressed(Vector2& dir) const {
    // Col joystick si scatta con le frecce, nella direzione della freccia premuta
    if (PadPressed(GAMEPAD_BUTTON_LEFT_FACE_UP))    { dir = { 0, -1 }; return true; }
    if (PadPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN))  { dir = { 0, 1 };  return true; }
    if (PadPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT))  { dir = { -1, 0 }; return true; }
    if (PadPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) { dir = { 1, 0 };  return true; }
    dir = { 0, 0 };
    return IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT) || PadPressed(GAMEPAD_BUTTON_LEFT_TRIGGER_1) ||
           ScreenPressed(TB_DASH);
}

bool Input::InteractPressed() const {
    return IsKeyPressed(KEY_E) || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) || ScreenPressed(TB_USE);
}

bool Input::SwapWeaponPressed() const {
    return IsKeyPressed(KEY_Q) || GetMouseWheelMove() != 0 || PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_UP) || ScreenPressed(TB_SWAP);
}

// I due menu hanno tasti diversi: Options/Menu/START per la partita in corso,
// touchpad, Share/Visualizza/Select e tasto PS/Xbox per il gioco (pausa, comandi, esci)
bool Input::GameMenuPressed() const {
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || TouchpadPressed(pad) ||
           PadPressed(GAMEPAD_BUTTON_MIDDLE_LEFT) || PadPressed(GAMEPAD_BUTTON_MIDDLE) || ScreenPressed(TB_PAUSE);
}

bool Input::MatchMenuPressed() const {
    return IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I) || PadPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT) || ScreenPressed(TB_MATCH);
}

bool Input::PadMenuButtonPressed() const {
    return PadPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT) || PadPressed(GAMEPAD_BUTTON_MIDDLE_LEFT) ||
           PadPressed(GAMEPAD_BUTTON_MIDDLE) || TouchpadPressed(pad);
}

bool Input::MenuPressed() const {
    return IsKeyPressed(KEY_F1);
}

bool Input::FullscreenPressed() const {
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    return IsKeyPressed(KEY_F11) || (alt && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)));
}

Input::PadFamily Input::FamilyOf(int padIndex) {
    if (padIndex < 0 || !IsGamepadAvailable(padIndex)) return PadFamily::Generic;
    std::string name = GetGamepadName(padIndex);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
    if (has("ps3") || has("ps4") || has("ps5") || has("playstation") || has("dualshock") || has("dualsense") ||
        has("wireless controller")) // e' il nome con cui Windows vede i DualShock 4
        return PadFamily::PlayStation;
    if (has("nintendo") || has("pro controller") || has("joy-con") || has("switch")) return PadFamily::Nintendo;
    if (has("xbox") || has("xinput") || has("x-box")) return PadFamily::Xbox;
    return PadFamily::Generic;
}

const char* Input::ButtonName(PadFamily f, int button) {
    // Ordine delle colonne: Xbox, PlayStation, Nintendo, generico
    auto pick = [f](const char* xbox, const char* ps, const char* nin, const char* gen) {
        switch (f) {
            case PadFamily::Xbox: return xbox;
            case PadFamily::PlayStation: return ps;
            case PadFamily::Nintendo: return nin;
            default: return gen;
        }
    };
    switch (button) {
        case GAMEPAD_BUTTON_RIGHT_FACE_DOWN:  return pick("A", "X", "B", "A");
        case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return pick("B", "Cerchio", "A", "B");
        case GAMEPAD_BUTTON_RIGHT_FACE_LEFT:  return pick("X", "Quadrato", "Y", "X");
        case GAMEPAD_BUTTON_RIGHT_FACE_UP:    return pick("Y", "Triangolo", "X", "Y");
        case GAMEPAD_BUTTON_MIDDLE_RIGHT:     return pick("Menu", "Options", "+", "Start");
        case GAMEPAD_BUTTON_MIDDLE_LEFT:      return pick("Visualizza", "Share", "-", "Select");
        case GAMEPAD_BUTTON_MIDDLE:           return pick("Xbox", "PS", "Home", "Guida");
        case GAMEPAD_BUTTON_LEFT_TRIGGER_1:   return pick("LB", "L1", "L", "LB");
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_1:  return pick("RB", "R1", "R", "RB");
        case GAMEPAD_BUTTON_LEFT_TRIGGER_2:   return pick("LT", "L2", "ZL", "LT");
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_2:  return pick("RT", "R2", "ZR", "RT");
        case GAMEPAD_BUTTON_LEFT_FACE_UP:     return "Su";
        case GAMEPAD_BUTTON_LEFT_FACE_DOWN:   return "Giu";
        case GAMEPAD_BUTTON_LEFT_FACE_LEFT:   return "Sx";
        case GAMEPAD_BUTTON_LEFT_FACE_RIGHT:  return "Dx";
        default: return "?";
    }
}

bool Input::ConfirmPressed() const {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE) ||
           PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || ScreenPressed(TB_OK) || (usingTouch && touch.Tapped());
}

bool Input::BackPressed() const {
    return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_BACKSPACE) ||
           PadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || ScreenPressed(TB_BACK) || ScreenPressed(TB_SKIP);
}

bool Input::PadDown(int button) const {
    return pad >= 0 && IsGamepadButtonDown(pad, button);
}

bool Input::PadPressed(int button) const {
    return pad >= 0 && IsGamepadButtonPressed(pad, button);
}

Vector2 Input::Stick(int axisX, int axisY) const {
    if (pad < 0) return { 0, 0 };
    return ApplyDeadzone({ GetGamepadAxisMovement(pad, axisX), GetGamepadAxisMovement(pad, axisY) });
}
