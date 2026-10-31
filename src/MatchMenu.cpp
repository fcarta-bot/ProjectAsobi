// Menu di partita (Options / Tab): inventario e sinergie, armi, mappa esplorata, statistiche della run.
// Il menu di gioco (pausa, comandi, esci) e' un'altra cosa: sta in PauseMenu.cpp.
#include <algorithm>
#include <cmath>
#include "Game.hpp"
#include "raymath.h"
#include "Ui.hpp"

static const Color PANEL{ 22, 21, 30, 240 };
static const Color BORDER{ 90, 90, 100, 255 };

static int DrawChips(unsigned tags, int x, int y) {
    for (int i = 0; i < TAG_COUNT; ++i) {
        if (!(tags & (1u << i))) continue;
        int w = MeasureText(TagName(i), 14) + 10;
        DrawRectangleRounded({ (float)x, (float)y, (float)w, 18 }, 0.5f, 6, TagColor(i));
        DrawText(TagName(i), x + 5, y + 2, 14, BLACK);
        x += w + 6;
    }
    return x;
}

// Riga "etichetta ........ valore"
static void StatRow(int x, int y, int w, const char* label, const char* value, Color valueColor = WHITE) {
    DrawText(label, x, y, 18, LIGHTGRAY);
    DrawText(value, x + w - MeasureText(value, 18), y, 18, valueColor);
    DrawLine(x, y + 24, x + w, y + 24, Fade(BORDER, 0.5f));
}

// ---------------------------------------------------------------------------
// Apertura e comandi
// ---------------------------------------------------------------------------

void Game::OpenMatchMenu() {
    state = State::MatchMenu;
    usedMatchMenu = true;
}

void Game::UpdateMatchMenu() {
    if (input.MatchMenuPressed() || input.BackPressed()) { // Options, Tab, Esc o B: si torna a giocare
        state = State::Playing;
        return;
    }
    if (input.GameMenuPressed()) { // touchpad (o Share / PS): passa direttamente al menu di gioco
        state = State::Playing;
        OpenPause();
        return;
    }
    if (input.LeftPressed()) matchTab = (matchTab + TAB_COUNT - 1) % TAB_COUNT;
    if (input.RightPressed()) matchTab = (matchTab + 1) % TAB_COUNT;
}

// Segna come viste le caselle intorno a te che non sono nascoste da un muro
void Game::UpdateExploration(float dt) {
    exploreTimer -= dt;
    if (exploreTimer > 0 || explored.empty()) return;
    exploreTimer = 0.15f;

    const int R = 9; // caselle: piu' o meno quanto si distingue nella foschia
    const int W = dungeon.Width(), H = dungeon.Height();
    const int px = (int)floorf(player.pos.x / TILE), py = (int)floorf(player.pos.y / TILE);
    for (int ty = std::max(0, py - R); ty <= std::min(H - 1, py + R); ++ty) {
        for (int tx = std::max(0, px - R); tx <= std::min(W - 1, px + R); ++tx) {
            int i = ty * W + tx;
            if (explored[i] || dungeon.IsSolid(tx, ty)) continue;
            if ((tx - px) * (tx - px) + (ty - py) * (ty - py) > R * R) continue;
            Vector2 c{ (tx + 0.5f) * TILE, (ty + 0.5f) * TILE };
            if (!dungeon.HasLineOfSight(player.pos, c)) continue;
            explored[i] = 1;
            lostTimer = 0; // hai scoperto qualcosa: non sei perso
            // I muri intorno a una casella vista si vedono anche loro
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = tx + dx, ny = ty + dy;
                    if (nx >= 0 && ny >= 0 && nx < W && ny < H && dungeon.IsSolid(nx, ny)) explored[ny * W + nx] = 1;
                }
        }
    }
}

// ---------------------------------------------------------------------------
// Disegno
// ---------------------------------------------------------------------------

void Game::DrawMatchMenu() const {
    const District& d = GetDistrict(floor);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade({ 8, 8, 12, 255 }, 0.94f));

    DrawText("Partita", 60, 28, 40, WHITE);
    DrawText(TextFormat("Piano %d - %s: %s", floor, d.city, d.place), 60, 74, 20, d.accent);

    // Schede
    static const char* names[TAB_COUNT] = { "Inventario", "Armi", "Mappa", "Statistiche" };
    const float tabW = 200, tabH = 40;
    float tx = sw - 60 - (float)TAB_COUNT * (tabW + 8);
    for (int t = 0; t < TAB_COUNT; ++t) {
        Rectangle r{ tx + t * (tabW + 8), 34, tabW, tabH };
        bool sel = t == matchTab;
        DrawRectangleRec(r, sel ? Color{ 50, 44, 66, 255 } : PANEL);
        DrawRectangleLinesEx(r, sel ? 3.0f : 1.0f, sel ? GOLD : BORDER);
        DrawTextCentered(names[t], (int)(r.x + tabW / 2), (int)r.y + 10, 20, sel ? WHITE : GRAY);
    }

    Rectangle area{ 60, 110, sw - 120.0f, sh - 170.0f };
    switch (matchTab) {
        case TAB_INVENTORY: DrawMatchInventory(area); break;
        case TAB_WEAPONS:   DrawMatchWeapons(area); break;
        case TAB_MAP:       DrawMatchMap(area); break;
        default:            DrawMatchStats(area); break;
    }

    const char* l = input.PadConnected() ? input.Btn(GAMEPAD_BUTTON_LEFT_TRIGGER_1) : "Frecce";
    const char* r = input.PadConnected() ? input.Btn(GAMEPAD_BUTTON_RIGHT_TRIGGER_1) : "";
    const char* close = input.PadConnected() ? input.Btn(GAMEPAD_BUTTON_MIDDLE_RIGHT) : "Tab";
    std::string hint = input.PadConnected()
        ? TextFormat("%s / %s o frecce: cambia scheda  |  %s o %s: torna a giocare", l, r, close, input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT))
        : TextFormat("%s sx/dx: cambia scheda  |  %s o Esc: torna a giocare", l, close);
    DrawTextCentered(hint.c_str(), sw / 2, sh - 44, 18, Fade(WHITE, 0.7f));
}

void Game::DrawMatchInventory(Rectangle area) const {
    // Sinistra: i cinque posti dell'inventario
    const int x = (int)area.x, w = 640, rowH = 78;
    DrawText(TextFormat("Artefatti (%d/%d)", (int)inventory.size(), INVENTORY_SIZE), x, (int)area.y, 22, WHITE);
    int y = (int)area.y + 34;
    for (int k = 0; k < INVENTORY_SIZE; ++k, y += rowH + 8) {
        Rectangle r{ (float)x, (float)y, (float)w, (float)rowH };
        DrawRectangleRec(r, PANEL);
        DrawRectangleLinesEx(r, 1, BORDER);
        DrawText(TextFormat("%d", k + 1), x + w - 18, y + 6, 14, GRAY);
        if (k >= (int)inventory.size()) {
            DrawText("vuoto", x + 14, y + 28, 18, DARKGRAY);
            continue;
        }
        const Artifact& a = AllArtifacts()[inventory[k]];
        DrawText(a.name, x + 14, y + 8, 22, WHITE);
        DrawText(TextFormat("Tributo: %s", a.origin), x + 14 + MeasureText(a.name, 22) + 14, y + 13, 14, GRAY);
        DrawText(a.desc, x + 14, y + 34, 16, LIGHTGRAY);
        DrawChips(a.tags, x + 14, y + rowH - 22);
    }

    // Destra: elementi e tutte le sinergie (quelle attive in oro)
    const int rx = x + w + 30, rw = (int)(area.x + area.width) - rx;
    int ry = (int)area.y;
    DrawText("Elementi", rx, ry, 22, WHITE);
    ry += 34;
    for (int i = 0; i < TAG_COUNT; ++i, ry += 24) {
        int c = CountTag(inventory, 1u << i);
        DrawCircle(rx + 8, ry + 9, 7, TagColor(i));
        DrawText(TagName(i), rx + 24, ry, 18, c >= 3 ? TagColor(i) : LIGHTGRAY);
        for (int p = 0; p < 3; ++p) // tre pallini: quanti ne mancano alla sinergia
            DrawRectangle(rx + 130 + p * 22, ry + 3, 16, 12, p < c ? TagColor(i) : Color{ 50, 50, 60, 255 });
        if (c > 3) DrawText(TextFormat("+%d", c - 3), rx + 200, ry, 16, TagColor(i));
    }

    ry += 10;
    DrawText("Sinergie", rx, ry, 22, WHITE);
    ry += 30;
    std::vector<int> active = ActiveSynergies(inventory);
    for (int s = 0; s < (int)AllSynergies().size(); ++s, ry += 36) {
        bool on = std::find(active.begin(), active.end(), s) != active.end();
        DrawText(AllSynergies()[s].name, rx, ry, 18, on ? GOLD : GRAY);
        if (on) DrawText("ATTIVA", rx + rw - MeasureText("ATTIVA", 14), ry + 2, 14, GOLD);
        DrawText(AllSynergies()[s].desc, rx, ry + 19, 14, on ? LIGHTGRAY : DARKGRAY);
    }
}

void Game::DrawMatchWeapons(Rectangle area) const {
    const float gap = 30, w = (area.width - gap) / 2, h = 380;
    for (int s = 0; s < WEAPON_SLOTS; ++s) {
        Rectangle r{ area.x + s * (w + gap), area.y + 10, w, h };
        int id = player.weapons[s];
        bool inHand = s == player.activeSlot;
        DrawRectangleRec(r, PANEL);
        DrawRectangleLinesEx(r, inHand ? 3.0f : 1.0f, inHand && id >= 0 ? AllWeapons()[id].color : BORDER);
        int x = (int)r.x + 20, y = (int)r.y + 18;
        if (id < 0) {
            DrawText("Slot vuoto", x, y, 26, GRAY);
            DrawText("Trova un'arma nel labirinto: ne puoi portare due", x, y + 40, 18, DARKGRAY);
            continue;
        }
        const Weapon& wp = AllWeapons()[id];
        DrawText(wp.name, x, y, 30, WHITE);
        if (inHand) DrawText("IN MANO", (int)(r.x + r.width) - 20 - MeasureText("IN MANO", 18), y + 6, 18, GOLD);
        DrawText(TextFormat("Tributo: %s", wp.origin), x, y + 38, 16, GRAY);
        DrawText(wp.desc, x, y + 62, 16, LIGHTGRAY);

        // Valori reali, artefatti compresi
        const int rw = (int)r.width - 40;
        int ry = y + 104;
        float rate = stats.fireRate * wp.rateMult;
        float dmg = stats.damage * wp.damageMult;
        if (wp.melee) {
            float arc = (wp.meleeArc + 0.12f * (stats.projectiles - 1)) * 2 * RAD2DEG;
            StatRow(x, ry, rw, "Danno per fendente", TextFormat("%.0f", dmg)); ry += 34;
            StatRow(x, ry, rw, "Fendenti al secondo", TextFormat("%.1f", rate)); ry += 34;
            StatRow(x, ry, rw, "Portata", TextFormat("%.1f caselle", wp.meleeRange / TILE)); ry += 34;
            StatRow(x, ry, rw, "Ampiezza", TextFormat("%.0f gradi", arc)); ry += 34;
            StatRow(x, ry, rw, "Speciale", "respinge i colpi nemici", SKYBLUE); ry += 34;
        } else {
            StatRow(x, ry, rw, "Danno per colpo", TextFormat("%.0f", dmg)); ry += 34;
            StatRow(x, ry, rw, "Colpi al secondo", TextFormat("%.1f", rate)); ry += 34;
            StatRow(x, ry, rw, "Proiettili per colpo", TextFormat("%d", stats.projectiles)); ry += 34;
            StatRow(x, ry, rw, "Trapassa", TextFormat("%d nemici", stats.pierce + wp.extraPierce)); ry += 34;
            if (wp.bounces > 0) { StatRow(x, ry, rw, "Rimbalzi sui muri", TextFormat("%d", wp.bounces), SKYBLUE); ry += 34; }
            if (wp.explosive) { StatRow(x, ry, rw, "Speciale", "esplode e incendia", ORANGE); ry += 34; }
        }
        StatRow(x, ry, rw, "Critico", TextFormat("%d%%  (x%.1f)", (int)(stats.critChance * 100), stats.critMult)); ry += 34;
        if (stats.burnDps > 0) StatRow(x, ry, rw, "Bruciatura", TextFormat("%.0f danni/s", stats.burnDps), ORANGE);
    }
    const char* swap = input.PadConnected() ? input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_UP) : "Q";
    DrawText(TextFormat("In partita cambi arma con %s. Gli artefatti potenziano entrambe le armi.", swap),
             (int)area.x, (int)(area.y + 410), 18, GRAY);
}

// La mappa mostra SOLO cio' che hai visto: niente cornice grande quanto il piano, niente bordi,
// niente percentuali. Si inquadra sulla parte esplorata e si allarga man mano che scopri: il resto e' ignoto
void Game::DrawMatchMap(Rectangle area) const {
    const District& d = GetDistrict(floor);
    const int W = dungeon.Width(), H = dungeon.Height();
    const float legendW = 300;
    const Rectangle view{ area.x, area.y, area.width - legendW - 30, area.height };

    // Riquadro delle caselle viste (piu' un po' di margine), e la tua posizione dentro
    int minX = W, minY = H, maxX = -1, maxY = -1;
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
            if (!explored.empty() && explored[ty * W + tx]) {
                minX = std::min(minX, tx); maxX = std::max(maxX, tx);
                minY = std::min(minY, ty); maxY = std::max(maxY, ty);
            }
    int px = (int)floorf(player.pos.x / TILE), py = (int)floorf(player.pos.y / TILE);
    minX = std::min(minX, px); maxX = std::max(maxX, px);
    minY = std::min(minY, py); maxY = std::max(maxY, py);
    float spanX = (float)(maxX - minX + 1) + 4, spanY = (float)(maxY - minY + 1) + 4;

    // Zoom: tutto il visto deve stare nel riquadro, ma all'inizio non serve ingrandire troppo
    const float cell = std::clamp(std::floor(std::min(view.width / spanX, view.height / spanY)), 5.0f, 22.0f);
    const float midX = (minX + maxX + 1) * 0.5f, midY = (minY + maxY + 1) * 0.5f;
    const float ox = view.x + view.width / 2 - midX * cell, oy = view.y + view.height / 2 - midY * cell;
    auto toMap = [&](Vector2 p) { return Vector2{ ox + p.x / TILE * cell, oy + p.y / TILE * cell }; };
    auto seenAt = [&](Vector2 p) {
        int tx = (int)floorf(p.x / TILE), ty = (int)floorf(p.y / TILE);
        return !explored.empty() && tx >= 0 && ty >= 0 && tx < W && ty < H && explored[ty * W + tx];
    };

    // Il fondo e' il riquadro dello schermo, non la sagoma del piano: fuori dal visto c'e' solo buio
    DrawRectangleRec(view, { 10, 10, 14, 255 });
    DrawRectangleLinesEx(view, 1, BORDER);
    BeginScissorMode((int)view.x + 1, (int)view.y + 1, (int)view.width - 2, (int)view.height - 2);
    for (int ty = 0; ty < H; ++ty) {
        for (int tx = 0; tx < W; ++tx) {
            if (explored.empty() || !explored[ty * W + tx]) continue;
            Color c;
            if (dungeon.IsSolid(tx, ty)) {
                c = Shade(d.stone, 0.7f);
            } else {
                Ground g = dungeon.GroundAt(tx, ty);
                c = g == Ground::Open ? d.open : g == Ground::Sidewalk ? d.sidewalk : Shade(d.road, 1.6f);
                c = Mix(c, { 120, 120, 120, 255 }, 0.35f);
            }
            DrawRectangle((int)(ox + tx * cell), (int)(oy + ty * cell), (int)cell, (int)cell, c);
        }
    }

    // Torii d'uscita, se l'hai gia' visto
    if (seenAt(dungeon.ExitPos())) {
        Vector2 e = toMap(dungeon.ExitPos());
        float s = cell * 1.2f;
        DrawRectangle((int)(e.x - s), (int)(e.y - s), (int)(s * 2), (int)(s * 0.35f), d.accent);
        DrawRectangle((int)(e.x - s * 0.7f), (int)(e.y - s), (int)(s * 0.3f), (int)(s * 2), d.accent);
        DrawRectangle((int)(e.x + s * 0.4f), (int)(e.y - s), (int)(s * 0.3f), (int)(s * 2), d.accent);
    }
    // Oggetti visti a terra
    for (const auto& p : pickups) {
        if (p.taken || !seenAt(p.pos)) continue;
        Vector2 c = toMap(p.pos);
        float s = std::max(3.0f, cell * 0.4f);
        if (p.weaponId >= 0) DrawRectangle((int)(c.x - s), (int)(c.y - s), (int)(s * 2), (int)(s * 2), WHITE);
        else if (p.artifactId >= 0) DrawPoly(c, 4, s * 1.4f, 45, TagColor(FirstTagIndex(AllArtifacts()[p.artifactId].tags)));
        else DrawCircleV(c, s, { 230, 60, 80, 255 });
    }
    // Tu: una freccia che indica dove guardi
    Vector2 me = toMap(player.pos);
    float a = std::max(7.0f, cell * 0.8f);
    Vector2 f = player.aim, side{ -f.y, f.x };
    Vector2 tip = Vector2Add(me, Vector2Scale(f, a * 1.3f));
    Vector2 l = Vector2Add(Vector2Add(me, Vector2Scale(f, -a * 0.8f)), Vector2Scale(side, -a * 0.8f));
    Vector2 r = Vector2Add(Vector2Add(me, Vector2Scale(f, -a * 0.8f)), Vector2Scale(side, a * 0.8f));
    DrawTriangle(tip, l, r, GOLD);
    DrawTriangle(tip, r, l, GOLD); // in un verso o nell'altro: uno dei due si vede sempre
    EndScissorMode();

    // Legenda
    const int lx = (int)(area.x + area.width - legendW), ly = (int)area.y + 10;
    DrawText("Mappa esplorata", lx, ly, 22, WHITE);
    int y = ly + 44;
    auto entry = [&](const char* label, auto drawIcon) {
        drawIcon(Vector2{ (float)lx + 8, (float)y + 9 });
        DrawText(label, lx + 26, y, 18, LIGHTGRAY);
        y += 30;
    };
    entry("Tu (e dove guardi)", [&](Vector2 c) { DrawTriangle({ c.x + 8, c.y }, { c.x - 6, c.y - 6 }, { c.x - 6, c.y + 6 }, GOLD);
                                                   DrawTriangle({ c.x + 8, c.y }, { c.x - 6, c.y + 6 }, { c.x - 6, c.y - 6 }, GOLD); });
    entry("Torii d'uscita", [&](Vector2 c) { DrawRectangle((int)c.x - 7, (int)c.y - 7, 14, 4, d.accent);
                                               DrawRectangle((int)c.x - 5, (int)c.y - 7, 3, 14, d.accent);
                                               DrawRectangle((int)c.x + 3, (int)c.y - 7, 3, 14, d.accent); });
    entry("Artefatto", [&](Vector2 c) { DrawPoly(c, 4, 6, 45, GOLD); });
    entry("Arma", [&](Vector2 c) { DrawRectangle((int)c.x - 4, (int)c.y - 4, 8, 8, WHITE); });
    entry("Cuore", [&](Vector2 c) { DrawCircleV(c, 4, { 230, 60, 80, 255 }); });
    y += 10;
    DrawText("Si segna solo cio' che hai visto:", lx, y, 16, GRAY);
    DrawText("il resto del labirinto e' ignoto.", lx, y + 22, 16, GRAY);
    DrawText("I nemici non compaiono.", lx, y + 44, 16, GRAY);
    if (guideActive) DrawText("La freccia in alto ti guida al torii.", lx, y + 78, 16, d.accent);
}

void Game::DrawMatchStats(Rectangle area) const {
    const int colW = (int)(area.width - 60) / 2;
    int x = (int)area.x, y = (int)area.y;

    // Sinistra: la run
    DrawText("La run", x, y, 24, WHITE);
    int ry = y + 40;
    int minutes = (int)runTime / 60, seconds = (int)runTime % 60;
    StatRow(x, ry, colW, "Piano", TextFormat("%d (%s)", floor, GetDistrict(floor).city)); ry += 34;
    StatRow(x, ry, colW, "Tempo", TextFormat("%d:%02d", minutes, seconds)); ry += 34;
    StatRow(x, ry, colW, "Nemici sconfitti", TextFormat("%d", kills), GOLD); ry += 34;
    static const char* enemyNames[4] = { "  Goblin", "  Robot", "  Tengu", "  Oni" };
    for (int t = 0; t < 4; ++t) { StatRow(x, ry, colW, enemyNames[t], TextFormat("%d", killsByType[t])); ry += 34; }
    StatRow(x, ry, colW, "Danno inflitto", TextFormat("%.0f", damageDealt)); ry += 34;
    StatRow(x, ry, colW, "Danno subito", TextFormat("%.0f", damageTaken), { 230, 110, 110, 255 }); ry += 34;
    StatRow(x, ry, colW, "Artefatti raccolti", TextFormat("%d", artifactsFound)); ry += 34;

    // Destra: il personaggio, con gli artefatti che ha addosso
    x += colW + 60;
    DrawText("Il personaggio", x, y, 24, WHITE);
    ry = y + 40;
    StatRow(x, ry, colW, "Vita", TextFormat("%d / %d", (int)player.hp, (int)stats.maxHp), { 230, 110, 110, 255 }); ry += 34;
    StatRow(x, ry, colW, "Danno base", TextFormat("%.1f", stats.damage)); ry += 34;
    StatRow(x, ry, colW, "Cadenza base", TextFormat("%.1f colpi/s", stats.fireRate)); ry += 34;
    StatRow(x, ry, colW, "Critico", TextFormat("%d%%  (x%.1f)", (int)(stats.critChance * 100), stats.critMult)); ry += 34;
    StatRow(x, ry, colW, "Proiettili / trapasso", TextFormat("%d / %d", stats.projectiles, stats.pierce)); ry += 34;
    StatRow(x, ry, colW, "Velocita'", TextFormat("%.0f", stats.moveSpeed)); ry += 34;
    StatRow(x, ry, colW, "Ricarica scatto", TextFormat("%.1f s", stats.dashCooldown)); ry += 34;
    StatRow(x, ry, colW, "Rubavita", TextFormat("%d%%", (int)(stats.lifesteal * 100))); ry += 34;
    StatRow(x, ry, colW, "Bruciatura", TextFormat("%.0f danni/s", stats.burnDps)); ry += 34;
    StatRow(x, ry, colW, "Fortuna (bottino)", TextFormat("%d%%", (int)(stats.dropChance * 100))); ry += 34;
}
