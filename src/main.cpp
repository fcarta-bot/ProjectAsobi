#include "raylib.h"
#include "Game.hpp"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

static Game* game = nullptr;

// Un fotogramma: aggiorna e disegna
static void Frame() {
    game->Update(GetFrameTime());
    BeginDrawing();
    game->Draw();
    EndDrawing();
}

int main() {
#if defined(__EMSCRIPTEN__)
    // Nel browser la tela segue la grandezza della pagina (iPad dritto o girato)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#else
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
#endif
    InitWindow(1280, 720, "ProjectAsobi");
    SetExitKey(KEY_NULL); // ESC serve nei menu, non deve chiudere il gioco

    // Sul heap: nel browser main() "finisce" subito e le sue variabili locali non sono piu' valide
    game = new Game();
#if defined(__EMSCRIPTEN__)
    // Il browser chiama Frame a ogni aggiornamento dello schermo: un ciclo while bloccherebbe la pagina
    emscripten_set_main_loop(Frame, 0, true);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose() && !game->WantsQuit()) Frame();
#endif

    delete game;
    CloseWindow();
    return 0;
}
