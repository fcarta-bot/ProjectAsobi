#pragma once
#include "raylib.h"

// Piccole funzioni di disegno condivise tra le schermate

inline void DrawTextCentered(const char* text, int cx, int y, int size, Color color) {
    DrawText(text, cx - MeasureText(text, size) / 2, y, size, color);
}

// Voce di menu: riquadro con etichetta, stato a destra (facoltativo) e descrizione sotto (facoltativa)
inline void DrawMenuItem(Rectangle r, const char* label, bool selected, bool enabled,
                         const char* status = "", Color statusColor = GRAY, const char* desc = nullptr) {
    const Color panel{ 25, 25, 32, 255 }, panelSelected{ 45, 40, 60, 255 }, border{ 90, 90, 100, 255 };
    DrawRectangleRec(r, selected ? panelSelected : panel);
    DrawRectangleLinesEx(r, selected ? 3.0f : 1.0f, selected ? GOLD : border);
    int textY = desc ? (int)r.y + 10 : (int)r.y + (int)r.height / 2 - 10;
    DrawText(label, (int)r.x + 16, textY, 20, enabled ? WHITE : GRAY);
    if (desc) DrawText(desc, (int)r.x + 16, (int)r.y + 38, 16, LIGHTGRAY);
    if (*status) DrawText(status, (int)(r.x + r.width) - 16 - MeasureText(status, 18), textY + 1, 18, statusColor);
}
