// Tutorial del GameMaster: niente lezioni all'inizio. Il GM compare in persona solo quando il giocatore
// sembra in difficolta' con un comando (non si muove, non attacca, resta bloccato contro un ostacolo...),
// spiega cosa fare, e appena il giocatore ci riesce si complimenta e sparisce. Il gioco non si ferma.
#include <algorithm>
#include <cmath>
#include "Cutscene.hpp"
#include "Game.hpp"
#include "raymath.h"
#include "Ui.hpp"

constexpr float TIP_DURATION = 10.0f;  // se il giocatore non ci prova, il GM se ne va comunque
constexpr float TIP_PRAISE = 1.8f;     // durata dei complimenti
constexpr float TIP_COOLDOWN = 15.0f;  // pausa minima tra un consiglio e l'altro
constexpr float TIP_RETRY = 60.0f;     // un consiglio ignorato puo' tornare dopo un minuto (una volta sola)

bool Game::TipDone(int tip) const {
    switch (tip) {
        case TIP_MOVE:   return usedMove;
        case TIP_LOOK:   return usedTurn;
        case TIP_PICKUP: return usedPickup;
        case TIP_ATTACK: return usedAttack;
        case TIP_JUMP:   return usedJump;
        case TIP_DASH:   return usedDash;
        case TIP_SWAP:   return usedSwap;
        case TIP_MENU:   return usedMatchMenu;
        default:         return false; // "ti sei perso" non ha niente da imparare
    }
}

void Game::StartTip(int tip) {
    activeTip = tip;
    tipTime = 0;
    tipPraise = 0;
    tipShown[tip]++;
    tipAgainAt[tip] = runTime + TIP_RETRY;
}

void Game::UpdateTutorials(float dt) {
    // Sensori: cosa sta succedendo al giocatore
    bool threatened = false;
    for (const auto& e : enemies)
        if (e.alive && e.aggro && e.seesPlayer && Vector2Distance(e.pos, player.pos) < 350) { threatened = true; break; }
    threatTime = threatened ? threatTime + dt : std::max(0.0f, threatTime - dt);
    bool nearItem = NearestPickup(48, true) >= 0 || NearestPickup(48, false) >= 0;
    pickupLinger = nearItem ? pickupLinger + dt : 0;
    twoWeaponsTime = player.weapons[1] >= 0 ? twoWeaponsTime + dt : 0;
    bool guideJustAppeared = guideActive && !guideWasActive;
    guideWasActive = guideActive;

    // Un consiglio e' gia' in corso
    if (activeTip >= 0) {
        tipTime += dt;
        if (tipPraise > 0) {
            tipPraise -= dt;
            if (tipPraise <= 0) { activeTip = -1; tipCooldown = TIP_COOLDOWN; }
            return;
        }
        if (TipDone(activeTip)) { tipPraise = TIP_PRAISE; return; } // ce l'ha fatta!
        if (tipTime > TIP_DURATION) { activeTip = -1; tipCooldown = TIP_COOLDOWN; }
        return;
    }
    tipCooldown -= dt;
    if (guideJustAppeared && tipShown[TIP_LOST] == 0) { StartTip(TIP_LOST); return; } // va detto subito
    if (tipCooldown > 0 || floorTime < 2) return;

    // Il GM interviene solo se serve: condizione di "difficolta'" per ogni comando non ancora usato
    auto offer = [&](int tip, bool struggling) {
        if (!struggling || TipDone(tip) || tipShown[tip] >= 2) return false;
        if (tipShown[tip] == 1 && runTime < tipAgainAt[tip]) return false;
        StartTip(tip);
        return true;
    };
    if (offer(TIP_MOVE, floorTime > 7)) return;
    if (offer(TIP_LOOK, floorTime > 14)) return;
    if (offer(TIP_ATTACK, threatTime > 4)) return;
    if (offer(TIP_JUMP, lowObstacleTime > 1.2f)) return;
    if (offer(TIP_PICKUP, pickupLinger > 4)) return;
    if (offer(TIP_DASH, hitsWithoutDash >= 4)) return;
    if (offer(TIP_SWAP, twoWeaponsTime > 45)) return;
    offer(TIP_MENU, runTime > 150);
}

void Game::DrawTutorial() const {
    if (activeTip < 0 || state != State::Playing) return;
    const bool pad = input.UsingPad();
    std::string text;
    switch (activeTip) {
        case TIP_MOVE:
            text = pad ? "Ehi, non stare li' impalato! Muoviti con lo stick sinistro." : "Ehi, non stare li' impalato! Muoviti con W A S D.";
            break;
        case TIP_LOOK:
            text = pad ? "Guardati intorno con lo stick destro: il labirinto e' grande." : "Guardati intorno col mouse: il labirinto e' grande.";
            break;
        case TIP_ATTACK:
            text = pad ? TextFormat("Ti stanno attaccando! Colpisci con %s.", input.Btn(GAMEPAD_BUTTON_RIGHT_TRIGGER_2))
                       : "Ti stanno attaccando! Colpisci col tasto sinistro del mouse.";
            break;
        case TIP_JUMP:
            text = pad ? TextFormat("E' basso: saltalo! Premi %s.", input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN))
                       : "E' basso: saltalo! Premi Spazio.";
            break;
        case TIP_PICKUP:
            text = pad ? TextFormat("Quello e' per te: prendilo con %s.", input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_LEFT))
                       : "Quello e' per te: prendilo con E.";
            break;
        case TIP_DASH:
            text = pad ? "Ti colpiscono troppo! Scatta con le frecce: per un attimo nessuno ti tocca."
                       : "Ti colpiscono troppo! Scatta con Shift: per un attimo nessuno ti tocca.";
            break;
        case TIP_SWAP:
            text = pad ? TextFormat("Hai due armi, sai? Cambiale con %s.", input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_UP))
                       : "Hai due armi, sai? Cambiale con Q.";
            break;
        case TIP_MENU:
            text = pad ? TextFormat("Premi %s: mappa, inventario e statistiche.", input.Btn(GAMEPAD_BUTTON_MIDDLE_RIGHT))
                       : "Premi Tab: mappa, inventario e statistiche.";
            break;
        default:
            text = "Ti sei perso? Ti ho messo una freccia in alto. Non dirlo a nessuno.";
            break;
    }
    static const char* praise[] = { "Ecco, bravo!", "Visto? Facile.", "Ora si ragiona!", "Perfetto. Mi stai divertendo." };
    if (tipPraise > 0) text = praise[activeTip % 4];

    // Entra con uno sbuffo di scintille, esce rimpicciolendosi
    float in = std::min(1.0f, tipTime / 0.35f);
    float out = tipPraise > 0 ? std::min(1.0f, tipPraise / 0.4f) : std::min(1.0f, (TIP_DURATION - tipTime) / 0.4f);
    float k = std::max(0.0f, std::min(in, out));
    float t = (float)GetTime();

    Rectangle panel{ 20, 186, 470, 120 };
    DrawRectangleRounded(panel, 0.2f, 8, Fade({ 12, 12, 20, 255 }, 0.78f * k));
    DrawRectangleRoundedLinesEx(panel, 0.2f, 8, 1.5f, Fade(WHITE, 0.25f * k));
    Vector2 feet{ 72, panel.y + panel.height - 10 };
    DrawStickman(feet, 92 * k, t, tipPraise > 0 ? StickPose::Wave : StickPose::Talk, k);
    if (tipTime < 0.6f) DrawSparkles({ feet.x, feet.y - 46 }, 40 + tipTime * 60, t, 1 - tipTime / 0.6f);
    DrawText("GameMaster", 130, (int)panel.y + 12, 16, Fade(GOLD, k));
    int chars = tipPraise > 0 ? -1 : (int)(tipTime * 45);
    DrawTextWrapped(text.c_str(), { 130, panel.y + 36, 340, 80 }, 20, Fade(WHITE, k), chars);
}
