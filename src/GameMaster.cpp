#include "GameMaster.hpp"
#include <algorithm>
#include <cmath>
#include "Game.hpp"
#include "raymath.h"

static float Rand01() { return GetRandomValue(0, 10000) / 10000.0f; }
static Vector2 Dir(float angle) { return { cosf(angle), sinf(angle) }; }

// ---------------------------------------------------------------------------
// Preparazione del piano
// ---------------------------------------------------------------------------

void GameMaster::PopulateFloor(Game& g) {
    const auto& rooms = g.dungeon.Rooms();
    int n = (int)rooms.size();

    // L'oni fa la guardia alla stanza del torii d'uscita
    int exitRoom = n - 1;
    for (int i = 0; i < n; ++i)
        if (Vector2Distance(rooms[i].Center(), g.dungeon.ExitPos()) < TILE) exitRoom = i;

    // Le altre stanze (non quella di partenza) in ordine casuale: una per gruppo finche' bastano
    std::vector<int> free;
    for (int i = 1; i < n; ++i)
        if (i != exitRoom) free.push_back(i);
    for (int i = (int)free.size() - 1; i > 0; --i) std::swap(free[i], free[GetRandomValue(0, i)]);
    size_t next = 0;
    auto takeRoom = [&]() { return free.empty() ? exitRoom : free[next++ % free.size()]; };

    int f = g.floor;
    int goblinSwarms = 1 + f / 3;
    int robotSquads = 1 + (f - 1) / 3;
    int tengu = 2 + f / 2;
    for (int k = 0; k < goblinSwarms; ++k)
        SpawnSwarm(g, EnemyType::Goblin, takeRoom(), GetRandomValue(GOBLIN_SWARM_MIN, std::min(GOBLIN_SWARM_MAX, 10 + f * 2)));
    for (int k = 0; k < robotSquads; ++k)
        SpawnSwarm(g, EnemyType::Robot, takeRoom(), GetRandomValue(ROBOT_SQUAD_MIN, std::min(ROBOT_SQUAD_MAX, 3 + f)));
    for (int k = 0; k < tengu; ++k) g.SpawnEnemy(EnemyType::Tengu, g.FreeSpotInRoom(rooms[takeRoom()], 14));

    Enemy& oni = g.SpawnEnemy(EnemyType::Oni, g.FreeSpotInRoom(rooms[exitRoom], 24));
    oni.home = Vector2Add(rooms[exitRoom].Center(), { TILE * 1.5f, 0 }); // accanto al torii, non sopra
}

void GameMaster::SpawnSwarm(Game& g, EnemyType type, int roomIndex, int size) {
    const Room& room = g.dungeon.Rooms()[roomIndex];
    Swarm s;
    s.type = type;
    s.home = s.wander = room.Center();
    s.patrolRoom = roomIndex;
    s.waitTimer = 2 + Rand01() * 3;
    int index = (int)g.swarms.size();
    g.swarms.push_back(s);

    for (int k = 0; k < size; ++k) {
        Enemy& e = g.SpawnEnemy(type, g.FreeSpotInRoom(room, 12));
        e.swarm = index;
        e.home = room.Center();
        e.slotAngle = k * 2 * PI / size + Rand01() * 0.3f;
        // Le squadre di robot sono miste: due d'assalto ogni artigliere
        if (type == EnemyType::Robot) e.role = k % 3 == 2 ? RobotRole::Gunner : RobotRole::Assault;
    }
}

// ---------------------------------------------------------------------------
// Regia delle squadre di robot
// ---------------------------------------------------------------------------

void GameMaster::Update(Game& g, float dt) {
    for (int i = 0; i < (int)g.swarms.size(); ++i)
        if (g.swarms[i].alive && g.swarms[i].type == EnemyType::Robot && g.swarms[i].size > 0) DirectSquad(g, i, dt);
}

void GameMaster::DirectSquad(Game& g, int squad, float dt) {
    Swarm& s = g.swarms[squad];
    bool aggro = false, sees = false;
    for (const auto& e : g.enemies) {
        if (!e.alive || e.swarm != squad) continue;
        aggro |= e.aggro;
        sees |= e.seesPlayer;
    }
    int tactic = !aggro ? 0 : sees ? 1 : 2;
    s.orderTimer -= dt;
    s.waitTimer -= dt;
    if (tactic != s.tactic) s.orderTimer = 0; // la situazione e' cambiata: nuovi ordini subito
    s.tactic = tactic;
    if (s.orderTimer > 0) return;

    if (tactic == 0) Patrol(g, squad);
    else if (tactic == 1) Hunt(g, squad);
    else Search(g, squad);
}

// Pattuglia: la squadra va in formazione da una stanza all'altra e si ferma un po' a controllare
void GameMaster::Patrol(Game& g, int squad) {
    Swarm& s = g.swarms[squad];
    const auto& rooms = g.dungeon.Rooms();
    s.orderTimer = 0.5f;

    bool anyOrder = false;
    float spread = 0;
    for (const auto& e : g.enemies) {
        if (!e.alive || e.swarm != squad || !e.hasOrder) continue;
        anyOrder = true;
        spread = std::max(spread, Vector2Distance(e.pos, e.order));
    }
    if (anyOrder) {
        if (spread > 40) { // ancora in cammino: la sosta parte quando sono arrivati tutti
            s.waitTimer = 2 + Rand01() * 3;
            return;
        }
        if (s.waitTimer > 0) return; // sosta: si guardano intorno
    }

    // Prossima stanza: una vicina, mai quella di partenza del giocatore
    std::vector<int> options;
    Vector2 here = rooms[s.patrolRoom].Center();
    for (int i = 1; i < (int)rooms.size(); ++i)
        if (i != s.patrolRoom && Vector2Distance(rooms[i].Center(), here) < TILE * 22) options.push_back(i);
    if (options.empty())
        for (int i = 1; i < (int)rooms.size(); ++i)
            if (i != s.patrolRoom) options.push_back(i);
    if (options.empty()) return;
    s.patrolRoom = options[GetRandomValue(0, (int)options.size() - 1)];
    s.home = rooms[s.patrolRoom].Center();

    int k = 0;
    for (auto& e : g.enemies) {
        if (!e.alive || e.swarm != squad) continue;
        Vector2 spot = k == 0 ? s.home : Vector2Add(s.home, Vector2Scale(Dir(k * 2.1f), 30));
        k++;
        e.order = g.nav.NearestWalkable(spot);
        e.home = e.order;
        e.hasOrder = true;
    }
}

// Caccia: gli assaltatori ti circondano arrivando da lati diversi, gli artiglieri si piazzano
// a distanza in punti da cui ti vedono, ognuno in un posto diverso
void GameMaster::Hunt(Game& g, int squad) {
    Swarm& s = g.swarms[squad];
    s.orderTimer = 0.8f;
    Vector2 p = g.player.pos;
    Vector2 from = Vector2Subtract(s.center, p);
    float base = Vector2Length(from) > 1 ? atan2f(from.y, from.x) : 0;

    static const float flank[] = { 0, 1.4f, -1.4f, 2.6f, -2.6f, 0.7f, -0.7f };
    int assault = 0;
    std::vector<Vector2> taken;
    for (auto& e : g.enemies) {
        if (!e.alive || e.swarm != squad) continue;
        e.hasOrder = true;
        if (e.role == RobotRole::Assault) {
            Vector2 spot = Vector2Add(p, Vector2Scale(Dir(base + flank[assault++ % 7]), 70));
            e.order = g.nav.NearestWalkable(spot);
            continue;
        }
        // Artigliere: tra 12 punti intorno a te, il piu' vicino a lui da cui ti si vede e non gia' preso
        float bestDist = 1e9f;
        Vector2 best = p;
        for (int a = 0; a < 12; ++a) {
            Vector2 spot = Vector2Add(p, Vector2Scale(Dir(a * PI / 6), 220));
            if (!g.nav.WalkableAt(spot) || !g.dungeon.HasLineOfSight(spot, p)) continue;
            bool crowded = std::any_of(taken.begin(), taken.end(), [&](Vector2 t) { return Vector2Distance(t, spot) < 90; });
            if (crowded) continue;
            float d = Vector2Distance(spot, e.pos);
            if (d < bestDist) { bestDist = d; best = spot; }
        }
        taken.push_back(best);
        e.order = best;
    }
}

// Ricerca: ti hanno perso di vista. Uno va dove ti hanno visto l'ultima volta,
// gli altri si dividono le stanze piu' vicine a quel punto
void GameMaster::Search(Game& g, int squad) {
    Swarm& s = g.swarms[squad];
    s.orderTimer = 4.0f;
    Vector2 last{};
    for (const auto& e : g.enemies)
        if (e.alive && e.swarm == squad) { last = e.lastSeen; break; }

    const auto& rooms = g.dungeon.Rooms();
    std::vector<int> nearest;
    for (int i = 0; i < (int)rooms.size(); ++i) nearest.push_back(i);
    std::sort(nearest.begin(), nearest.end(), [&](int a, int b) {
        return Vector2Distance(rooms[a].Center(), last) < Vector2Distance(rooms[b].Center(), last);
    });
    int count = std::min(3, (int)nearest.size());

    int k = 0;
    for (auto& e : g.enemies) {
        if (!e.alive || e.swarm != squad) continue;
        e.hasOrder = true;
        e.order = k == 0 ? g.nav.NearestWalkable(last) : rooms[nearest[(k - 1) % count]].Center();
        k++;
    }
}
