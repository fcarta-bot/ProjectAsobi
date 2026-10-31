#pragma once
#include <vector>
#include "raylib.h"

// Tag elementali: ogni artefatto ne ha uno o piu'.
// Accumulare 3 artefatti con lo stesso tag sblocca una sinergia.
enum Tag : unsigned {
    TAG_FUOCO   = 1u << 0,
    TAG_VENTO   = 1u << 1,
    TAG_SPIRITO = 1u << 2,
    TAG_ACCIAIO = 1u << 3,
    TAG_FORTUNA = 1u << 4,
};
constexpr int TAG_COUNT = 5;

const char* TagName(int index); // index da 0 a TAG_COUNT-1
Color TagColor(int index);
int FirstTagIndex(unsigned tags);

// Statistiche del giocatore: si ricalcolano da zero ogni volta che l'inventario cambia.
struct Stats {
    float maxHp        = 100.0f;
    float damage       = 10.0f;
    float damageMult   = 1.0f;
    float fireRate     = 3.0f;   // colpi al secondo
    float fireRateMult = 1.0f;
    float moveSpeed    = 220.0f;
    float projSpeed    = 560.0f;
    float critChance   = 0.05f;
    float critMult     = 2.0f;
    float lifesteal    = 0.0f;   // frazione del danno inflitto che torna come vita
    float burnDps      = 0.0f;   // danno al secondo della bruciatura
    float dashCooldown = 1.2f;
    float dropChance   = 0.12f;  // probabilita' che un nemico lasci un artefatto
    int   projectiles  = 1;
    int   pierce       = 0;      // quanti nemici un colpo puo' attraversare
};

// L'ordine deve corrispondere alla lista in AllArtifacts()
enum ArtifactId {
    A_OMAMORI, A_MANEKI_NEKO, A_DARUMA, A_LANTERNA_GION, A_SENSU, A_FURIN,
    A_TAMAHAGANE, A_SHURIKEN, A_KITSUNE_BI, A_TAKOYAKI, A_NEON, A_ORIZURU,
    A_SPECCHIO_YATA, A_KOBAN, A_SHIKA_SENBEI,
    A_COUNT
};

struct Artifact {
    const char* name;
    const char* origin; // il luogo a cui rende omaggio
    const char* desc;
    unsigned tags;
    void (*apply)(Stats&);
};

struct Synergy {
    const char* name;
    const char* desc;
    bool (*isActive)(const std::vector<int>& inventory);
    void (*apply)(Stats&);
};

const std::vector<Artifact>& AllArtifacts();
const std::vector<Synergy>& AllSynergies();

int CountTag(const std::vector<int>& inventory, unsigned tag);
std::vector<int> ActiveSynergies(const std::vector<int>& inventory);
Stats ComputeStats(const std::vector<int>& inventory);
