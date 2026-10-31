// Guida verso il torii: non obbliga a cambiare piano, ma se ti sei perso ti indica la strada.
// La freccia compare quando vaghi a lungo senza scoprire niente di nuovo, e segue il percorso vero
// (intorno a muri e oggetti), non la linea d'aria.
#include <algorithm>
#include <cmath>
#include "Game.hpp"
#include "raymath.h"
#include "Ui.hpp"

constexpr float GUIDE_MIN_TIME = 120;   // secondi sul piano prima che la guida possa comparire
constexpr float GUIDE_STUCK_TIME = 45;  // ...e secondi di fila senza scoprire caselle nuove
constexpr float GUIDE_MAX_TIME = 300;   // dopo cinque minuti compare comunque

void Game::UpdateGuide(float dt) {
    floorTime += dt;
    lostTimer += dt;    if (!guideActive) {
        bool lost = (floorTime > GUIDE_MIN_TIME && lostTimer > GUIDE_STUCK_TIME) || floorTime > GUIDE_MAX_TIME;
        if (!lost) return;
        guideActive = true; // una volta comparsa resta fino al prossimo piano: niente frecce che vanno e vengono
        guideRepath = 0;
        ShowToast("Sembri perso: la freccia in alto indica la strada per il torii");
    }

    // Il percorso si ricalcola qualche volta al secondo: costa poco e segue i tuoi spostamenti
    guideRepath -= dt;
    if (guideRepath > 0) return;
    guideRepath = 0.4f;

    Vector2 exit = dungeon.ExitPos();
    std::vector<Vector2> path = nav.FindPath(player.pos, exit);
    guideDistance = (float)path.size();
    // Punta al punto piu' lontano del percorso che si vede gia' da qui (al massimo 8 caselle avanti):
    // cosi' la freccia non trema a ogni casella e non indica dentro un muro
    Vector2 target = exit;
    if (!path.empty()) {
        target = path[0];
        for (int i = 0; i < (int)path.size() && i < 8; ++i)
            if (dungeon.HasLineOfSight(player.pos, path[i])) target = path[i];
    }
    Vector2 d = Vector2Subtract(target, player.pos);
    if (Vector2Length(d) > 1) guideDir = Vector2Normalize(d);
}

void Game::DrawGuide() const {
    if (!guideActive || state != State::Playing) return;
    const District& d = CurrentDistrict();
    const int sw = GetScreenWidth();

    // Direzione rispetto allo sguardo: in alto sullo schermo = davanti a te
    Vector2 right{ -player.aim.y, player.aim.x };
    float side = Vector2DotProduct(guideDir, right), ahead = Vector2DotProduct(guideDir, player.aim);
    float angle = atan2f(side, ahead); // 0 = dritto, positivo = verso destra

    const Vector2 c{ sw / 2.0f, 150 };
    float pulse = 1.0f + 0.08f * sinf((float)GetTime() * 5);
    auto rot = [&](float x, float y) { // punto del disegno ruotato (y negativo = punta in alto)
        float s = sinf(angle), co = cosf(angle);
        return Vector2{ c.x + (x * co - y * s) * pulse, c.y + (x * s + y * co) * pulse };
    };

    DrawCircleV(c, 40, Fade(BLACK, 0.4f));
    DrawCircleLines((int)c.x, (int)c.y, 40, Fade(d.accent, 0.6f));
    // Freccia: punta e coda, con un bordo scuro per leggerla anche sul cielo chiaro
    Vector2 tip = rot(0, -30), l = rot(-18, -4), r = rot(18, -4);
    Vector2 tl = rot(-7, -4), tr = rot(7, -4), bl = rot(-7, 24), br = rot(7, 24);
    for (int pass = 0; pass < 2; ++pass) {
        Color col = pass == 0 ? Fade(BLACK, 0.6f) : d.accent;
        Vector2 o = pass == 0 ? Vector2{ 2, 2 } : Vector2{ 0, 0 };
        auto P = [&](Vector2 p) { return Vector2Add(p, o); };
        // Disegnati nei due versi: con la rotazione l'ordine dei vertici cambia
        DrawTriangle(P(tip), P(l), P(r), col);
        DrawTriangle(P(tip), P(r), P(l), col);
        DrawTriangle(P(tl), P(bl), P(br), col);
        DrawTriangle(P(tl), P(br), P(bl), col);
        DrawTriangle(P(tl), P(br), P(tr), col);
        DrawTriangle(P(tl), P(tr), P(br), col);
    }
    DrawTextCentered("Torii", sw / 2, 196, 18, d.accent);
    DrawTextCentered(TextFormat("circa %d passi", (int)(guideDistance * 3)), sw / 2, 216, 14, Fade(WHITE, 0.7f));
}
