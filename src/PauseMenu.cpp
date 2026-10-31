// Menu di pausa: si apre con Esc (o P/Tab) e col tasto menu del joystick (START / Options / Menu).
#include <string>
#include "Game.hpp"
#include "Ui.hpp"

enum { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_FULLSCREEN, PAUSE_NEW_RUN, PAUSE_QUIT, PAUSE_COUNT };

// Nel browser (iPad) schermo intero e uscita li gestisce la pagina web: quelle voci non ci sono
#if defined(__EMSCRIPTEN__)
static const int PAUSE_ITEMS[] = { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_NEW_RUN };
#else
static const int PAUSE_ITEMS[] = { PAUSE_RESUME, PAUSE_CONTROLLER, PAUSE_CONTROLS, PAUSE_FULLSCREEN, PAUSE_NEW_RUN, PAUSE_QUIT };
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
        case PAUSE_FULLSCREEN: ToggleFullscreenMode(); break;
        case PAUSE_NEW_RUN:    NewRun(); break;
        case PAUSE_QUIT:       quit = true; break;
    }
}

void Game::DrawPause() const {
    const District& d = GetDistrict(floor);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));

    if (pauseShowControls) {
        DrawTextCentered("Comandi", sw / 2, 36, 40, WHITE);
        const bool pad = input.PadConnected();
        DrawTextCentered(pad ? TextFormat("Joystick: %s", GetGamepadName(input.ConnectedPad()))
                             : "Nessun joystick collegato: i nomi dei tasti sono quelli di un controller Xbox",
                         sw / 2, 86, 18, LIGHTGRAY);
        auto b = [&](int button) { return pad ? input.Btn(button) : Input::ButtonName(Input::PadFamily::Xbox, button); };

        struct Row { const char* action; std::string keyboard; std::string joystick; };
        const Row rows[] = {
            { "Muoviti", "WASD / frecce su e giu'", "Stick sinistro" },
            { "Girati", "Mouse / frecce sx e dx", "Stick destro" },
            { "Attacca", "Click sinistro", std::string(b(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) + " o " + b(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) },
            { "Salta", "Spazio", b(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) },
            { "Scatto", "Shift (dove ti muovi)", std::string("Frecce (nella loro direzione) o ") + b(GAMEPAD_BUTTON_LEFT_TRIGGER_1) },
            { "Raccogli / attraversa il torii", "E", b(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) },
            { "Cambia arma", "Q / rotellina", b(GAMEPAD_BUTTON_RIGHT_FACE_UP) },
            { "Menu di partita (inventario, mappa...)", "Tab / I", b(GAMEPAD_BUTTON_MIDDLE_RIGHT) },
            { "Menu di gioco (questo)", "Esc / P",
              pad && input.Family() == Input::PadFamily::PlayStation
                  ? std::string("Touchpad, Share o PS")
                  : std::string(b(GAMEPAD_BUTTON_MIDDLE_LEFT)) + " o " + b(GAMEPAD_BUTTON_MIDDLE) },
            { "Menu controller", "F1", "dal menu di gioco" },
            { "Schermo intero / finestra", "F11 / Alt+Invio", "dal menu di gioco" },
        };
        const int x0 = 140, x1 = 520, x2 = 820;
        int y = 140;
        DrawText("Azione", x0, y, 20, GOLD);
        DrawText("Tastiera e mouse", x1, y, 20, GOLD);
        DrawText("Joystick", x2, y, 20, GOLD);
        y += 36;
        for (const Row& r : rows) {
            DrawRectangle(x0 - 12, y - 6, x2 + 330 - x0, 34, Fade({ 40, 40, 52, 255 }, 0.7f));
            DrawText(r.action, x0, y, 18, WHITE);
            DrawText(r.keyboard.c_str(), x1, y, 18, LIGHTGRAY);
            DrawText(r.joystick.c_str(), x2, y, 18, LIGHTGRAY);
            y += 40;
        }
        DrawTextCentered("Invio, Esc o B per tornare", sw / 2, sh - 50, 18, Fade(WHITE, 0.7f));
        return;
    }

    DrawTextCentered("Pausa", sw / 2, 60, 48, WHITE);
    DrawTextCentered(TextFormat("Piano %d - %s: %s", floor, d.city, d.place), sw / 2, 118, 20, d.accent);

    struct Item { const char* label; const char* desc; };
    const Item items[PAUSE_COUNT] = {
        { "Riprendi", nullptr },
        { "Controller e joystick", "Collega un joystick (Bluetooth, cavo, pennetta) e prova i tasti" },
        { "Comandi", "Tutti i tasti, con i nomi del tuo joystick" },
        { "Schermo intero", "In qualsiasi momento anche con F11 o Alt+Invio" },
        { "Nuova run", "Ricomincia dal primo piano" },
        { "Esci dal gioco", nullptr },
    };
    const bool full = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    const float w = 560, x = (sw - w) / 2;
    float y = 160;
    for (int k = 0; k < PAUSE_SHOWN; ++k) {
        const int i = PAUSE_ITEMS[k];
        float h = items[i].desc ? 70.0f : 52.0f;
        const char* status = i == PAUSE_FULLSCREEN ? (full ? "SI" : "NO") : "";
        DrawMenuItem({ x, y, w, h }, items[i].label, k == pauseIndex, true, status, full ? GREEN : GRAY, items[i].desc);
        y += h + 8;
    }
    if (!input.UsingTouch())
        DrawTextCentered("Su/Giu scegli  |  Invio o A conferma  |  Esc, B o touchpad riprende",
                         sw / 2, sh - 50, 18, Fade(WHITE, 0.7f));
}
