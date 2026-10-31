#include "raylib.h"
#include "Game.hpp"

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "ProjectAsobi");
    SetExitKey(KEY_NULL); // ESC serve nei menu, non deve chiudere il gioco
    SetTargetFPS(60);

    Game game;
    while (!WindowShouldClose() && !game.WantsQuit()) {
        game.Update(GetFrameTime());
        BeginDrawing();
        game.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
