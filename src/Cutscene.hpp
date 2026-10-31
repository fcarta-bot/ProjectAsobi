#pragma once
#include "raylib.h"

// Animazioni della storia: "Il GameMaster si annoiava, cosi' un giorno scese sulla terra
// per insinuarsi nei sogni del povero malcapitato."
// Tutto e' disegnato in 2D su una tela virtuale di 1280x720, adattata allo schermo.

// Il GameMaster: uno stickman bianco e luminoso
enum class StickPose { Stand, Walk, Float, Talk, Wave, Laugh, Crouch };
void DrawStickman(Vector2 feet, float height, float t, StickPose pose, float alpha, bool glow = true);
void DrawSparkles(Vector2 center, float radius, float t, float alpha, int count = 10);
// Testo che va a capo da solo dentro un riquadro; maxChars < 0 = tutto (serve all'effetto macchina da scrivere)
void DrawTextWrapped(const char* text, Rectangle box, int size, Color color, int maxChars = -1);

// Menu iniziale: il GM scende dal cielo e si apposta fuori dalla finestra del malcapitato, che e' ancora sveglio
namespace TitleScene {
constexpr float STREET = 5.5f;  // dal cielo alla strada davanti a casa
constexpr float MENU = 9.5f;    // il GM e' appostato alla finestra: compare il menu
}
void DrawTitleScene(float t);
// Dove disegnare il menu iniziale (a destra, sopra la camera da letto)
Rectangle TitleMenuArea();

// Dopo "Nuova partita" o "Scegli partita": il malcapitato si addormenta, il GM entra nel sogno,
// il sogno del giro del mondo, il labirinto che cade, il discorso del GM
namespace Intro {
constexpr float ENTER = 4.5f;    // spenta la luce, il GM entra dalla finestra e si infila nella testa
constexpr float DREAM = 10.5f;   // il sogno: le citta' del mondo
constexpr float FALL = 16.5f;    // il labirinto precipita
constexpr float TALK = 21.5f;    // il GM spiega la situazione
constexpr int LINES = 5;
constexpr float LINE_TIME = 4.4f;
constexpr float VANISH = TALK + 0.8f + LINES * LINE_TIME; // il GM sparisce
constexpr float END = VANISH + 1.8f;
constexpr float LOAD_END = DREAM + 3.0f; // ripresa di un salvataggio: niente discorso, solo un "bentornato"
float NextStop(float t); // dove saltare premendo "avanti"
}
void DrawIntro(float t, bool newRun);

// Morte: il giocatore si sveglia di soprassalto dall'incubo
namespace WakeUp {
constexpr float DARK = 1.4f;  // il labirinto sfuma nel rosso e poi nel nero
constexpr float ROOM = 2.0f;  // la camera all'alba
constexpr float SIT = 2.6f;   // si tira su di scatto
constexpr float GM = 6.2f;    // il GM saluta dalla finestra
constexpr float END = 9.0f;
}
void DrawWakeUp(float t); // va disegnata sopra il mondo: lo copre da sola

// Vittoria: il GM ride e applaude, poi il malcapitato si sveglia contento
namespace VictoryScene {
constexpr float WAKE = 6.0f;
constexpr float END = 10.0f;
}
void DrawVictoryScene(float t, const char* title);
