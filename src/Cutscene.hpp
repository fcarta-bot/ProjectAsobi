#pragma once
#include "raylib.h"

// Animazioni della storia: "Il GameMaster si annoiava, cosi' un giorno scese sulla terra
// per insinuarsi nei sogni del povero malcapitato."
// Tutto e' disegnato in 2D su una tela virtuale di 1280x720, adattata allo schermo.

// Il GameMaster: uno stickman bianco e luminoso
enum class StickPose { Stand, Walk, Float, Talk, Wave };
void DrawStickman(Vector2 feet, float height, float t, StickPose pose, float alpha, bool glow = true);
void DrawSparkles(Vector2 center, float radius, float t, float alpha, int count = 10);
// Testo che va a capo da solo dentro un riquadro; maxChars < 0 = tutto (serve all'effetto macchina da scrivere)
void DrawTextWrapped(const char* text, Rectangle box, int size, Color color, int maxChars = -1);

// Introduzione all'avvio: discesa del GM, la camera da letto, il sogno, il labirinto che cade, il discorso
namespace Intro {
constexpr float ROOM = 6.0f;     // il GM arriva ai piedi del letto ed entra nella testa
constexpr float DREAM = 14.5f;   // il sogno: il Giappone
constexpr float FALL = 19.5f;    // il labirinto precipita sulla citta'
constexpr float TALK = 25.0f;    // il GM spiega la situazione
constexpr int LINES = 4;
constexpr float LINE_TIME = 4.2f;
constexpr float VANISH = TALK + 0.8f + LINES * LINE_TIME; // il GM sparisce
constexpr float END = VANISH + 1.8f;
float NextStop(float t); // dove saltare premendo "avanti"
}
void DrawIntro(float t);

// Morte: il giocatore si sveglia di soprassalto dall'incubo
namespace WakeUp {
constexpr float DARK = 1.4f;  // il labirinto sfuma nel rosso e poi nel nero
constexpr float ROOM = 2.0f;  // la camera all'alba
constexpr float SIT = 2.6f;   // si tira su di scatto
constexpr float GM = 6.2f;    // il GM saluta dalla finestra
constexpr float END = 9.0f;
}
void DrawWakeUp(float t); // va disegnata sopra il mondo: lo copre da sola
