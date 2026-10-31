#pragma once

// Le sfide del GameMaster. Due tipi:
// - sfide "premio": valgono per tutta la run e ognuna regala un pezzo d'equipaggiamento raro;
// - sfide "vittoria": a ogni run il GM ne propone 3. Completarne una lo diverte abbastanza da far
//   vincere la run, e sblocca un titolo che resta per sempre.
constexpr int REWARD_COUNT = 10;
constexpr int VICTORY_COUNT = 10;
constexpr int VICTORY_PICKS = 3;

enum RewardId {
    RC_KILLS100, RC_BOSSES3, RC_CLEAN_FLOOR, RC_DAHU, RC_COMBO,
    RC_FLOOR10, RC_EXPLORER, RC_MELEE_BOSS, RC_PASSPORT, RC_SYNERGY3,
};
enum VictoryId {
    VC_KILLS500, VC_EMPOWERED, VC_GHOST, VC_GLOBETROTTER, VC_LEGENDS,
    VC_SPEED, VC_UNTOUCHABLE, VC_ARMORY, VC_FEARLESS, VC_ALCHEMIST,
};

struct Challenge {
    const char* name;
    const char* desc;
    int target;          // a quanto deve arrivare il contatore
    const char* prize;   // il premio (nome dell'oggetto o del titolo)
};

const Challenge& RewardChallenge(int id);
const Challenge& VictoryChallenge(int id);
// Cosa regala una sfida premio: un'arma (>= 0) oppure un artefatto
void RewardItem(int id, int& weaponId, int& artifactId);
