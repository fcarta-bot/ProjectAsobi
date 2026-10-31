// Le sfide del GameMaster: premi rari durante la run e sfide "vittoria" per divertirlo e vincere.
// Qui ci sono anche le scritte di paura e maledizioni nell'HUD.
#include "Challenges.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include "Cutscene.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "raymath.h"

const Challenge& RewardChallenge(int id) {
    static const Challenge list[REWARD_COUNT] = {
        { "Cento teste",        "Sconfiggi 100 nemici",                                100, "Martello di John Henry" },
        { "Cacciatore di guardiani", "Sconfiggi 3 guardiani del torii",                3, "Excalibur" },
        { "Senza un graffio",   "Finisci un piano senza farti colpire",                1, "Orologio di Greenwich" },
        { "Il Dahu esiste",     "Acchiappa un Dahu (in Francia gira in tondo...)",     1, "Cornicello napoletano" },
        { "Raffica",            "Sconfiggi 10 nemici in 10 secondi",                   10, "Kusanagi" },
        { "Giu' nel profondo",  "Arriva al piano 10",                                  10, "Durlindana" },
        { "Esploratore",        "Esplora l'80% di un piano prima di uscire",           1, "Acchiappasogni" },
        { "Lama contro gigante", "Finisci un guardiano con un'arma corpo a corpo",     1, "Tizona" },
        { "Passaporto pieno",   "Visita tutti e 7 i paesi",                            7, "Balmung" },
        { "Alchimista",         "Tieni attive 3 sinergie insieme",                     3, "Giglio d'oro" },
    };
    return list[id];
}

const Challenge& VictoryChallenge(int id) {
    static const Challenge list[VICTORY_COUNT] = {
        { "Sterminatore",       "Sconfiggi 500 mostri",                                500, "Lo Sterminatore" },
        { "Ammazzagiganti",     "Sconfiggi un guardiano potenziato dal GM",            1, "Ammazzagiganti" },
        { "Fantasma",           "Arriva al piano 100 senza subire ne' infliggere danni", 100, "Il Fantasma" },
        { "Giramondo",          "Visita tutte e 16 le citta'",                         16, "Giramondo" },
        { "Cacciatore di leggende", "Sconfiggi il guardiano di ognuno dei 7 paesi",    7, "Cacciatore di leggende" },
        { "Fulmine",            "Arriva al piano 15 in meno di 25 minuti",             15, "Il Fulmine" },
        { "Intoccabile",        "Finisci 8 piani di fila senza farti colpire",         8, "L'Intoccabile" },
        { "Armaiolo",           "Impugna 20 armi diverse",                             20, "Maestro d'armi" },
        { "Coraggioso",         "Sconfiggi l'Uomo Nero o el Cuco con la paura al massimo", 1, "Senza paura" },
        { "Gran alchimista",    "Tieni attive 4 sinergie insieme",                     4, "Gran alchimista" },
    };
    return list[id];
}

void RewardItem(int id, int& weaponId, int& artifactId) {
    static const int weapons[REWARD_COUNT] = { W_JOHN_HENRY, W_EXCALIBUR, -1, -1, W_KUSANAGI, W_DURLINDANA, -1, W_TIZONA, W_BALMUNG, -1 };
    static const int artifacts[REWARD_COUNT] = { -1, -1, A_OROLOGIO, A_CORNICELLO, -1, -1, A_ACCHIAPPASOGNI, -1, -1, A_GIGLIO };
    weaponId = weapons[id];
    artifactId = artifacts[id];
}

// ---------------------------------------------------------------------------
// Contatori
// ---------------------------------------------------------------------------

void Game::ResetChallenges() {
    std::fill(std::begin(rewardDone), std::end(rewardDone), false);
    bossesKilled = 0;
    meleeBossKill = empoweredKilled = fearlessKill = dahuCaught = false;
    empoweredSpawned = false;
    cleanFloorDone = explorerDone = false;
    noDamageStreak = bestNoDamageStreak = 0;
    districtsSeen = countriesSeen = bossCountries = 0;
    weaponsHeld = 1ull << W_OFUDA;
    bestSynergies = 0;
    recentKills.clear();
    wonChallenge = -1;
    bannerTimer = 0;
}

// Il GM sceglie tre sfide a caso tra le dieci: completane una per vincere la run
void Game::PickVictoryChallenges() {
    int all[VICTORY_COUNT];
    for (int i = 0; i < VICTORY_COUNT; ++i) all[i] = i;
    for (int i = VICTORY_COUNT - 1; i > 0; --i) std::swap(all[i], all[GetRandomValue(0, i)]);
    for (int k = 0; k < VICTORY_PICKS; ++k) victoryPicks[k] = all[k];
}

void Game::OnEnemyKilled(const Enemy& e) {
    recentKills.push_back(runTime);
    if (fearFloor) fear = std::max(0.0f, fear - 0.04f);
    if (InfoOf(e.type).boss) {
        bossesKilled++;
        bossCountries |= 1u << (int)InfoOf(e.type).country;
        if (e.lastHitMelee) meleeBossKill = true;
        if (e.empowered) empoweredKilled = true;
    }
    if ((e.type == EnemyType::Babau || e.type == EnemyType::Cuco) && fear >= 0.95f) fearlessKill = true;
}

void Game::OnFloorLeft() {
    bool clean = damageTaken <= floorDamageStart + 0.01f;
    if (clean) cleanFloorDone = true;
    noDamageStreak = clean ? noDamageStreak + 1 : 0;
    bestNoDamageStreak = std::max(bestNoDamageStreak, noDamageStreak);
    if (ExploredFraction() >= 0.8f) explorerDone = true;
}

void Game::OnWeaponTaken(int id) { weaponsHeld |= 1ull << id; }

int Game::RewardProgress(int id) const {
    switch (id) {
        case RC_KILLS100:    return kills;
        case RC_BOSSES3:     return bossesKilled;
        case RC_CLEAN_FLOOR: return cleanFloorDone;
        case RC_DAHU:        return dahuCaught;
        case RC_COMBO:       return (int)recentKills.size();
        case RC_FLOOR10:     return floor;
        case RC_EXPLORER:    return explorerDone;
        case RC_MELEE_BOSS:  return meleeBossKill;
        case RC_PASSPORT:    return std::popcount(countriesSeen);
        case RC_SYNERGY3:    return bestSynergies;
    }
    return 0;
}

// -1 = sfida fallita (non si puo' piu' completare in questa run)
int Game::VictoryProgress(int id) const {
    switch (id) {
        case VC_KILLS500:     return kills;
        case VC_EMPOWERED:    return empoweredKilled;
        case VC_GHOST:        return damageDealt > 0 || damageTaken > 0 ? -1 : floor;
        case VC_GLOBETROTTER: return std::popcount(districtsSeen);
        case VC_LEGENDS:      return std::popcount(bossCountries);
        case VC_SPEED:        return runTime > 25 * 60 && floor < 15 ? -1 : floor;
        case VC_UNTOUCHABLE:  return bestNoDamageStreak;
        case VC_ARMORY:       return std::popcount(weaponsHeld);
        case VC_FEARLESS:     return fearlessKill;
        case VC_ALCHEMIST:    return bestSynergies;
    }
    return 0;
}

void Game::UpdateChallenges(float dt) {
    (void)dt;
    // La raffica conta solo le uccisioni degli ultimi 10 secondi
    recentKills.erase(std::remove_if(recentKills.begin(), recentKills.end(), [&](float t) { return runTime - t > 10; }), recentKills.end());

    for (int i = 0; i < REWARD_COUNT; ++i)
        if (!rewardDone[i] && RewardProgress(i) >= RewardChallenge(i).target) CompleteReward(i);
    for (int v : victoryPicks)
        if (VictoryProgress(v) >= VictoryChallenge(v).target) { WinRun(v); return; }
}

void Game::CompleteReward(int id) {
    rewardDone[id] = true;
    int weapon, artifact;
    RewardItem(id, weapon, artifact);
    Vector2 spot = nav.NearestWalkable(Vector2Add(player.pos, Vector2Scale(player.aim, 40)));
    if (weapon >= 0) pickups.push_back({ spot, -1, weapon });
    else pickups.push_back({ spot, artifact });
    bannerText = TextFormat("Sfida completata: %s! Il GameMaster ti lascia: %s", RewardChallenge(id).name, RewardChallenge(id).prize);
    bannerTimer = 5;
}

void Game::WinRun(int victoryId) {
    wonChallenge = victoryId;
    state = State::Victory;
    victoryTime = 0;
    activeTip = -1;
    profile.titles |= 1u << victoryId;
    profile.victories++;
    SaveProfile();
    DeleteRun(slot); // la run e' finita: il sogno si chiude bene, stavolta
}

void Game::UpdateVictory(float dt) {
    victoryTime += dt;
    if (victoryTime < VictoryScene::END) {
        if (input.ConfirmPressed() || input.BackPressed()) victoryTime = VictoryScene::END;
        return;
    }
    if (input.ConfirmPressed()) OpenTitle();
}

void Game::DrawVictory() const {
    DrawVictoryScene(victoryTime, VictoryChallenge(std::max(0, wonChallenge)).prize);
    if (victoryTime < VictoryScene::END) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle panel{ sw / 2.0f - 260, sh * 0.5f - 120, 520, 250 };
    DrawRectangleRounded(panel, 0.08f, 8, Fade({ 14, 14, 22, 255 }, 0.9f));
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2, GOLD);
    DrawTextCentered("Hai vinto la run!", sw / 2, (int)panel.y + 20, 32, GOLD);
    DrawTextCentered(TextFormat("Sfida: %s", VictoryChallenge(wonChallenge).name), sw / 2, (int)panel.y + 66, 20, WHITE);
    DrawTextCentered(TextFormat("Nuovo titolo: \"%s\"", VictoryChallenge(wonChallenge).prize), sw / 2, (int)panel.y + 96, 22, { 255, 220, 120, 255 });
    DrawTextCentered(TextFormat("Piano %d   Nemici %d   Tempo %d:%02d", floor, kills, (int)runTime / 60, (int)runTime % 60),
                     sw / 2, (int)panel.y + 140, 18, LIGHTGRAY);
    DrawTextCentered(input.UsingTouch() ? "Tocca per tornare al menu" : "Invio / A per tornare al menu", sw / 2, (int)panel.y + 200, 18, Fade(WHITE, 0.7f));
}

// Scritta in alto quando si completa una sfida premio
void Game::DrawChallengeBanner() const {
    if (bannerTimer <= 0 || state != State::Playing) return;
    int sw = GetScreenWidth();
    float a = std::min(1.0f, bannerTimer);
    int w = MeasureText(bannerText.c_str(), 20) + 40;
    DrawRectangleRounded({ (sw - w) / 2.0f, 74, (float)w, 40 }, 0.4f, 8, Fade({ 40, 30, 10, 255 }, 0.85f * a));
    DrawRectangleRoundedLinesEx({ (sw - w) / 2.0f, 74, (float)w, 40 }, 0.4f, 8, 2, Fade(GOLD, a));
    DrawTextCentered(bannerText.c_str(), sw / 2, 84, 20, Fade({ 255, 230, 150, 255 }, a));
}

// Paura e maledizioni nell'HUD, piu' il "disturbo" dell'Uomo Alto quando e' vicino
void Game::DrawStatusEffects() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (grossmannNear > 0.05f) {
        for (int k = 0; k < (int)(grossmannNear * 140); ++k) {
            unsigned h = TileHash(k, (int)(GetTime() * 30), 77);
            DrawRectangle((int)(h % sw), (int)((h >> 12) % sh), 2 + (int)(h >> 24) % 30, 1, Fade(WHITE, 0.25f * grossmannNear));
        }
        DrawRectangleGradientH(0, 0, sw / 4, sh, Fade(BLACK, 0.5f * grossmannNear), BLANK);
        DrawRectangleGradientH(sw * 3 / 4, 0, sw / 4, sh, BLANK, Fade(BLACK, 0.5f * grossmannNear));
    }
    if (fearFloor || fear > 0.05f) {
        DrawText("Paura", 20, 172, 14, { 200, 160, 255, 255 });
        DrawRectangle(70, 175, 210, 8, { 20, 20, 20, 200 });
        Color col = fear >= 0.6f ? Color{ 200, 60, 255, 255 } : Color{ 140, 110, 200, 255 };
        DrawRectangle(70, 175, (int)(210 * fear), 8, col);
        DrawRectangle(70 + (int)(210 * 0.6f), 173, 2, 12, WHITE); // oltre questa soglia l'Uomo Nero si materializza
    }
    int x = 365, y = 130;
    for (const Curse& c : curses) {
        const char* name = c.type == CurseType::Omen ? "Presagio: danni subiti x1.5"
                         : c.type == CurseType::Compana ? "Maledizione della Santa Compana"
                                                        : "Sanguini";
        Color col = c.type == CurseType::Omen ? Color{ 255, 90, 80, 255 } : c.type == CurseType::Compana ? Color{ 200, 200, 255, 255 } : RED;
        DrawRectangle(x - 6, y - 3, MeasureText(name, 16) + 60, 22, Fade(BLACK, 0.5f));
        DrawText(TextFormat("%s  %ds", name, (int)ceilf(c.time)), x, y, 16, col);
        y += 24;
    }
}
