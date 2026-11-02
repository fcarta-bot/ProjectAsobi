// Menu di gioco (pausa): si apre con Esc / P, col touchpad o Share del joystick, o col tasto pausa a schermo.
#include <algorithm>
#include <string>
#include "Game.hpp"
#include "Ui.hpp"

enum { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_TOUCH, PAUSE_FULLSCREEN, PAUSE_NEW_RUN, PAUSE_SAVE_QUIT, PAUSE_QUIT, PAUSE_COUNT };

// Nel browser (iPad) schermo intero e uscita li gestisce la pagina web: quelle voci non ci sono
#if defined(__EMSCRIPTEN__)
static const int PAUSE_ITEMS[] = { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_TOUCH, PAUSE_NEW_RUN, PAUSE_SAVE_QUIT };
#else
static const int PAUSE_ITEMS[] = { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_FULLSCREEN, PAUSE_NEW_RUN, PAUSE_SAVE_QUIT, PAUSE_QUIT };
#endif
constexpr int PAUSE_SHOWN = sizeof(PAUSE_ITEMS) / sizeof(PAUSE_ITEMS[0]);

// Schermo intero "senza bordi": una finestra grande quanto il monitor. Non cambia la risoluzione
// dello schermo, quindi passare ad altre finestre (Alt+Tab) e' istantaneo
void Game::ToggleFullscreenMode() {
    ToggleBorderlessWindowed();
    // Il mouse catturato va ricatturato nella nuova finestra: lo rilascia qui, lo riprende UpdateCursorLock
    if (cursorLocked) {
        EnableCursor();
        cursorLocked = false;
    }
    ShowToast(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "Schermo intero (F11 per tornare in finestra)" : "Finestra");
}

void Game::OpenPause() {
    pauseReturnState = state;
    state = State::Paused;
    pauseIndex = 0;
    pauseShowControls = false;
}

void Game::UpdatePause() {
    if (pauseShowControls) {
        if (input.BackPressed() || input.ConfirmPressed() || input.GameMenuPressed()) pauseShowControls = false;
        return;
    }
    if (input.GameMenuPressed() || input.BackPressed()) {
        state = pauseReturnState;
        return;
    }
    if (input.MatchMenuPressed() && pauseReturnState == State::Playing) { // passa direttamente all'altro menu
        state = State::Playing;
        OpenMatchMenu();
        return;
    }
    if (input.UpPressed()) pauseIndex = (pauseIndex + PAUSE_SHOWN - 1) % PAUSE_SHOWN;
    if (input.DownPressed()) pauseIndex = (pauseIndex + 1) % PAUSE_SHOWN;
    if (!input.ConfirmPressed()) return;

    switch (PAUSE_ITEMS[pauseIndex]) {
        case PAUSE_RESUME:     state = pauseReturnState; break;
        case PAUSE_CONTROLLER: OpenControllerMenu(""); break; // chiudendolo si torna qui
        case PAUSE_CONTROLS:   pauseShowControls = true; break;
        case PAUSE_TOUCH:      input.Touch().StartEditing(); break; // con "Fatto" si torna qui
        case PAUSE_FULLSCREEN: ToggleFullscreenMode(); break;
        case PAUSE_NEW_RUN:    NewRun(); break;
        case PAUSE_SAVE_QUIT:  if (pauseReturnState == State::GameOver) OpenTitle(); else SaveAndQuitToTitle(); break;
        case PAUSE_QUIT:       if (pauseReturnState != State::GameOver) SaveRun(); quit = true; break;
    }
}

// La pagina "Comandi": tastiera, joystick e touch (usata anche dalle impostazioni del menu iniziale)
void Game::DrawControlsPage() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.85f));
    DrawTextCentered("Comandi", sw / 2, 30, 40, WHITE);
    const bool pad = input.PadConnected();
    DrawTextCentered(pad ? TextFormat("Joystick: %s", GetGamepadName(input.ConnectedPad()))
                         : "Nessun joystick collegato: i nomi dei tasti sono quelli di un controller Xbox",
                     sw / 2, 78, 18, LIGHTGRAY);
    auto b = [&](int button) { return pad ? input.Btn(button) : Input::ButtonName(Input::PadFamily::Xbox, button); };

    struct Row { const char* action; std::string keyboard; std::string joystick; std::string touch; };
    const Row rows[] = {
        { "Muoviti", "WASD / frecce su e giu'", "Stick sinistro", "Trascina a sinistra" },
        { "Girati", "Mouse / frecce sx e dx", "Stick destro", "Trascina fuori dai tasti" },
        { "Attacca", "Click sinistro", std::string(b(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) + " o " + b(GAMEPAD_BUTTON_RIGHT_TRIGGER_1), "Stick ATTACCA (tienilo)" },
        { "Salta", "Spazio", b(GAMEPAD_BUTTON_RIGHT_FACE_DOWN), "SALTA" },
        { "Scatto", "Shift", std::string("Frecce o ") + b(GAMEPAD_BUTTON_LEFT_TRIGGER_1), "SCATTO" },
        { "Raccogli / torii", "E", b(GAMEPAD_BUTTON_RIGHT_FACE_LEFT), "PRENDI" },
        { "Cambia arma", "Q / rotellina", b(GAMEPAD_BUTTON_RIGHT_FACE_UP), "ARMA" },
        { "Menu di partita", "Tab / I", b(GAMEPAD_BUTTON_MIDDLE_RIGHT), "BORSA" },
        { "Menu di gioco", "Esc / P",
          pad && input.Family() == Input::PadFamily::PlayStation ? std::string("Touchpad o Share")
                                                                 : std::string(b(GAMEPAD_BUTTON_MIDDLE_LEFT)),
          "Tasto pausa" },
        { "Schermo intero", "F11 / Alt+Invio", "dal menu", "dal browser" },
    };
    const int x0 = std::max(20, sw / 2 - 560), cw = std::min(280, (sw - 40) / 4);
    int y = 120;
    const char* heads[] = { "Azione", "Tastiera e mouse", "Joystick", "Touch (iPad)" };
    for (int c = 0; c < 4; ++c) DrawText(heads[c], x0 + c * cw, y, 20, GOLD);
    y += 34;
    for (const Row& r : rows) {
        DrawRectangle(x0 - 10, y - 6, cw * 4, 32, Fade({ 40, 40, 52, 255 }, 0.7f));
        DrawText(r.action, x0, y, 17, WHITE);
        DrawText(r.keyboard.c_str(), x0 + cw, y, 17, LIGHTGRAY);
        DrawText(r.joystick.c_str(), x0 + cw * 2, y, 17, LIGHTGRAY);
        DrawText(r.touch.c_str(), x0 + cw * 3, y, 17, LIGHTGRAY);
        y += 37;
    }
    DrawTextCentered("Invio, Esc o B per tornare", sw / 2, sh - 40, 18, Fade(WHITE, 0.7f));
}

void Game::DrawPause() const {
    const District& d = CurrentDistrict();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (pauseShowControls) { DrawControlsPage(); return; }
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));

    DrawTextCentered("Pausa", sw / 2, 50, 48, WHITE);
    DrawTextCentered(TextFormat("Piano %d - %s: %s", floor, d.city, d.place), sw / 2, 104, 20, d.accent);

    struct Item { const char* label; const char* desc; };
    const Item items[PAUSE_COUNT] = {
        { "Riprendi", nullptr },
        { "Controller e joystick", "Collega un joystick (Bluetooth, cavo, pennetta) e prova i tasti" },
        { "Comandi", "Tutti i tasti: tastiera, joystick e touch" },
        { "Modifica comandi touch", "Sposta e ingrandisci i tasti, sensibilita', stick fissi o mobili" },
        { "Schermo intero", "In qualsiasi momento anche con F11 o Alt+Invio" },
        { "Ricomincia la run", "Si riparte dal primo piano, con un nuovo giro del mondo" },
        { "Salva e torna al menu", nullptr },
        { "Esci dal gioco", nullptr },
    };
    const bool full = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    const float w = 560, x = (sw - w) / 2;
    float y = 140;
    for (int k = 0; k < PAUSE_SHOWN; ++k) {
        const int i = PAUSE_ITEMS[k];
        float h = items[i].desc ? 66.0f : 48.0f;
        const char* status = i == PAUSE_FULLSCREEN ? (full ? "SI" : "NO") : "";
        DrawMenuItem({ x, y, w, h }, items[i].label, k == pauseIndex, true, status, full ? GREEN : GRAY, items[i].desc);
        y += h + 7;
    }
    if (!input.UsingTouch())
        DrawTextCentered("Su/Giu scegli  |  Invio o A conferma  |  Esc, B o touchpad riprende",
                         sw / 2, sh - 34, 18, Fade(WHITE, 0.7f));
}
