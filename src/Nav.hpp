#pragma once
#include <cstdint>
#include <vector>
#include "raylib.h"
#include "Dungeon.hpp"
#include "Props.hpp"

// Navigazione dei nemici sulla griglia delle caselle: sanno girare intorno a muri e oggetti.
// Due strumenti:
// - la "mappa delle distanze" dal giocatore (flow field): una sola per tutti quelli che lo inseguono
//   o che scappano da lui, si ricalcola solo quando il giocatore cambia casella;
// - i percorsi A* tra due punti qualsiasi, per chi va altrove (ultimo punto in cui ti ha visto,
//   ritorno a casa, raggiungere lo sciame, posizioni ordinate dal GameMaster).
class Nav {
public:
    void Build(const Dungeon& dungeon, const CityProps& props);

    bool Walkable(int tx, int ty) const;
    bool WalkableAt(Vector2 p) const;
    Vector2 NearestWalkable(Vector2 p) const; // il centro della casella libera piu' vicina

    void UpdateFlow(Vector2 target);
    float FlowDistance(Vector2 from) const;     // in pixel lungo il percorso; < 0 se irraggiungibile
    Vector2 TowardTarget(Vector2 from) const;   // direzione per avvicinarsi; (0,0) se non si puo'
    Vector2 AwayFromTarget(Vector2 from) const; // direzione per allontanarsi; (0,0) se messo all'angolo

    // Centri delle caselle da attraversare (senza quella di partenza); vuoto se non c'e' strada
    std::vector<Vector2> FindPath(Vector2 from, Vector2 to) const;

private:
    int Index(int tx, int ty) const { return ty * width + tx; }
    float StepCost(int from, int to, bool diagonal) const;
    int BestNeighbor(Vector2 from, bool closer) const;

    int width = 0, height = 0;
    std::vector<uint8_t> walk;
    std::vector<float> cost; // le caselle accanto ai muri costano di piu': cosi' si cammina in mezzo
    std::vector<float> flow; // distanza dal giocatore in caselle (enorme = irraggiungibile)
    int flowTile = -1;
};
