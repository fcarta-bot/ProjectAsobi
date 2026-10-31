#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include "raymath.h"

// IA dei nemici: percezione (ti vede? quanto gli interessi?), movimento senza sbattere nei muri,
// regole degli sciami di goblin e comportamento di ogni tipo. Le squadre di robot le dirige il GameMaster.

static float Rand01() { return GetRandomValue(0, 10000) / 10000.0f; }
static bool IsZero(Vector2 v) { return v.x == 0 && v.y == 0; }
static Vector2 Dir(float angle) { return { cosf(angle), sinf(angle) }; }

constexpr float ENEMY_SHOT_H = 0.5f;
constexpr float THINK_TIME = 0.2f;   // ogni quanto si ricontrollano vista e linee libere
constexpr float VIEW_COS = 0.34f;    // campo visivo di circa 140 gradi
constexpr float NOTICE_NEAR = 70;    // cosi' vicino si accorge di te anche alle spalle
constexpr float CHARGE_SPEED = 430;  // carica dell'oni

// Quanto lontano vede, quanti secondi ci mette a dimenticarti e quanto lo scoraggia la tua velocita'
static float SightRange(EnemyType t) {
    switch (t) {
        case EnemyType::Goblin: return 300;
        case EnemyType::Robot:  return 420;
        case EnemyType::Tengu:  return 450;
        case EnemyType::Oni:    return 480;
    }
    return 300;
}
static float MemoryTime(EnemyType t) {
    switch (t) {
        case EnemyType::Goblin: return 5;
        case EnemyType::Robot:  return 9;  // la squadra si coordina: ti cerca a lungo
        case EnemyType::Tengu:  return 6;
        case EnemyType::Oni:    return 14; // il boss non molla facilmente
    }
    return 5;
}
static float SpeedBoredom(EnemyType t) {
    switch (t) {
        case EnemyType::Goblin: return 0.5f;
        case EnemyType::Robot:  return 0.25f;
        case EnemyType::Tengu:  return 0.4f;
        case EnemyType::Oni:    return 0.15f;
    }
    return 0.5f;
}

// ---------------------------------------------------------------------------
// Ciclo principale
// ---------------------------------------------------------------------------

void Game::UpdateEnemies(float dt) {
    // Velocita' del giocatore: serve per anticiparlo e per capire se sta seminando i nemici
    if (dt > 0) playerVel = Vector2Scale(Vector2Subtract(player.pos, prevPlayerPos), 1.0f / dt);
    if (Vector2Length(playerVel) > 1200) playerVel = { 0, 0 }; // teletrasporto (nuovo piano)
    prevPlayerPos = player.pos;
    nav.UpdateFlow(player.pos);

    for (auto& e : enemies) {
        if (!e.alive) continue;
        e.hitFlash -= dt;
        e.attackTimer -= dt;
        e.avoidDirect -= dt;
        e.thinkTimer -= dt;
        e.thinkNow = e.thinkTimer <= 0;
        if (e.thinkNow) e.thinkTimer = THINK_TIME + Rand01() * 0.05f;

        // Chi passa vicino a un oggetto in fiamme si brucia
        if (props.NearFire(e.pos, e.radius, 0)) {
            e.burnTimer = 2.0f;
            e.burnDps = std::max(e.burnDps, 6.0f);
        }
        if (e.burnTimer > 0) {
            e.burnTimer -= dt;
            e.hp -= e.burnDps * dt;
            if (e.hp <= 0) { KillEnemy(e); continue; }
        }
        if (e.doom <= 0) Perceive(e, dt);
    }
    ShareSwarmAwareness();
    UpdateSwarms(dt);
    gm.Update(*this, dt);

    for (auto& e : enemies) {
        if (!e.alive) continue;
        if (e.doom > 0) {
            // Goblin di troppo: resta fermo mentre i compagni lo eliminano
            e.doom -= dt;
            if (e.doom <= 0) AbsorbDoomed(e);
            continue;
        }
        switch (e.type) {
            case EnemyType::Goblin: UpdateGoblin(e, dt); break;
            case EnemyType::Robot:  UpdateRobot(e, dt); break;
            case EnemyType::Tengu:  UpdateTengu(e, dt); break;
            case EnemyType::Oni:    UpdateOni(e, dt); break;
        }
    }

    // Separazione: i nemici non si sovrappongono tutti nello stesso punto
    for (size_t i = 0; i < enemies.size(); ++i) {
        for (size_t j = i + 1; j < enemies.size(); ++j) {
            Enemy& a = enemies[i];
            Enemy& b = enemies[j];
            if (!a.alive || !b.alive) continue;
            Vector2 d = Vector2Subtract(b.pos, a.pos);
            float len = Vector2Length(d);
            float minDist = a.radius + b.radius;
            if (len > 0 && len < minDist) {
                Vector2 push = Vector2Scale(d, (minDist - len) / len * 0.5f);
                MoveWithCollision(a.pos, a.radius, Vector2Negate(push));
                MoveWithCollision(b.pos, b.radius, push);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Percezione: vista e interesse
// ---------------------------------------------------------------------------

void Game::Perceive(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = std::max(1.0f, Vector2Length(to));
    float sight = SightRange(e.type);

    if (e.thinkNow) {
        // Ti vede solo se sei nel suo campo visivo (o molto vicino) e non c'e' un muro in mezzo.
        // Quando ti sta gia' dando la caccia ti segue con lo sguardo anche se gli giri intorno
        bool inView = dist < NOTICE_NEAR || Vector2DotProduct(e.facing, Vector2Scale(to, 1 / dist)) > VIEW_COS ||
                      (e.aggro && dist < 200);
        e.seesPlayer = dist < sight && inView && dungeon.HasLineOfSight(e.pos, player.pos);
    }

    // > 0 se vai piu' veloce di lui: piu' sei veloce, prima si stufa di inseguirti
    float faster = std::max(0.0f, Vector2Length(playerVel) / e.speed - 1.0f);
    float boredom = faster * SpeedBoredom(e.type);

    if (e.seesPlayer) {
        e.lastSeen = player.pos;
        if (!e.aggro) {
            // Si accorge di te tanto piu' in fretta quanto sei vicino
            e.interest += dt * (0.8f + 2.5f * (1 - dist / sight));
            if (e.interest >= 1) {
                e.aggro = true;
                e.interest = 1;
                AddText(e.pos, "!", YELLOW);
            }
        } else {
            // Ti vede, ma stai scappando piu' veloce di lui: l'interesse cala lo stesso
            bool escaping = Vector2DotProduct(playerVel, to) > 0 && dist > 220;
            if (escaping && boredom > 0) e.interest -= dt * boredom * 0.6f;
            else e.interest = std::min(1.0f, e.interest + dt * 2);
        }
    } else if (e.aggro) {
        e.interest -= dt * (1.0f / MemoryTime(e.type) + boredom);
    } else {
        e.interest = std::max(0.0f, e.interest - dt * 0.3f); // un'occhiata di sfuggita: se ne dimentica
    }

    if (e.aggro && e.interest <= 0) {
        e.aggro = false;
        e.interest = 0;
        e.attack = EnemyAttack::None;
        e.windup = 0;
        e.burstLeft = 0;
        e.hasOrder = false;
        e.path.clear();
        if (e.swarm < 0) AddText(e.pos, "?", LIGHTGRAY); // per gli sciami sarebbero venti punti di domanda
    }
}

// I membri di uno sciame (o di una squadra) si passano le informazioni: se uno ti vede, lo sanno tutti
void Game::ShareSwarmAwareness() {
    std::vector<float> best(swarms.size(), 0.0f);
    std::vector<int> source(swarms.size(), -1);
    for (int i = 0; i < (int)enemies.size(); ++i) {
        const Enemy& e = enemies[i];
        if (!e.alive || e.swarm < 0 || !e.aggro) continue;
        float score = e.interest + (e.seesPlayer ? 1 : 0); // chi ti vede adesso ha l'informazione migliore
        if (score > best[e.swarm]) { best[e.swarm] = score; source[e.swarm] = i; }
    }
    for (auto& e : enemies) {
        if (!e.alive || e.swarm < 0 || source[e.swarm] < 0 || e.doom > 0) continue;
        const Enemy& s = enemies[source[e.swarm]];
        e.aggro = true;
        e.interest = std::max(e.interest, s.interest);
        if (!e.seesPlayer) e.lastSeen = s.lastSeen;
    }
}

// ---------------------------------------------------------------------------
// Sciami di goblin: troppo pochi scappano e si uniscono ad altri, troppi si sbranano
// ---------------------------------------------------------------------------

void Game::UpdateSwarms(float dt) {
    for (auto& s : swarms) {
        s.size = 0;
        s.center = { 0, 0 };
    }
    std::vector<int> doomed(swarms.size(), 0);
    for (const auto& e : enemies) {
        if (!e.alive || e.swarm < 0) continue;
        if (e.doom > 0) { doomed[e.swarm]++; continue; }
        swarms[e.swarm].size++;
        swarms[e.swarm].center = Vector2Add(swarms[e.swarm].center, e.pos);
    }

    for (int i = 0; i < (int)swarms.size(); ++i) {
        Swarm& s = swarms[i];
        if (s.size == 0) {
            if (doomed[i] == 0) s.alive = false;
            continue;
        }
        s.center = Vector2Scale(s.center, 1.0f / s.size);

        // Quando e' tranquillo lo sciame gironzola per la sua stanza
        s.wanderTimer -= dt;
        if (s.wanderTimer <= 0) {
            s.wanderTimer = 3 + Rand01() * 4;
            Vector2 p = Vector2Add(s.home, { (Rand01() * 2 - 1) * TILE * 2.5f, (Rand01() * 2 - 1) * TILE * 2 });
            s.wander = nav.WalkableAt(p) ? p : s.home;
        }
        if (s.type != EnemyType::Goblin) continue;

        // Troppi goblin: i piu' deboli vengono eliminati dai compagni, che si spartiscono la loro forza
        if (s.size > GOBLIN_SWARM_MAX && doomed[i] == 0) {
            std::vector<Enemy*> members;
            for (auto& e : enemies)
                if (e.alive && e.swarm == i && e.doom <= 0) members.push_back(&e);
            std::sort(members.begin(), members.end(), [](const Enemy* a, const Enemy* b) { return a->hp < b->hp; });
            for (int k = 0; k < s.size - GOBLIN_SWARM_MAX; ++k) members[k]->doom = 0.9f;
            AddText(s.center, "Lo sciame elimina i piu' deboli!", { 200, 120, 255, 255 });
        }

        // Troppo pochi: lo sciame si sbanda e cerca un altro gruppo a cui unirsi.
        // Meglio uno sciame "vero"; tra due sciami piccoli si muove quello piu' piccolo, l'altro aspetta
        s.joinTarget = -1;
        if (s.size < GOBLIN_SWARM_MIN) {
            float bestDist = 1e9f;
            bool bestIsFull = false;
            for (int j = 0; j < (int)swarms.size(); ++j) {
                const Swarm& o = swarms[j];
                if (j == i || !o.alive || o.type != EnemyType::Goblin || o.size == 0) continue;
                bool full = o.size >= GOBLIN_SWARM_MIN;
                if (!full && (o.size < s.size || (o.size == s.size && j > i))) continue;
                float d = Vector2Distance(s.center, o.center);
                if ((full && !bestIsFull) || (full == bestIsFull && d < bestDist)) {
                    bestDist = d;
                    bestIsFull = full;
                    s.joinTarget = j;
                }
            }
        }
    }
}

// Il goblin eliminato dai compagni muore e la sua forza si divide tra chi resta
void Game::AbsorbDoomed(Enemy& victim) {
    victim.alive = false; // non conta come uccisione tua e non lascia niente
    AddText(victim.pos, "divorato", { 200, 120, 255, 255 });
    if (victim.swarm < 0) return;
    std::vector<Enemy*> survivors;
    for (auto& e : enemies)
        if (e.alive && e.swarm == victim.swarm && e.doom <= 0) survivors.push_back(&e);
    if (survivors.empty()) return;
    float n = (float)survivors.size();
    for (Enemy* e : survivors) {
        e->maxHp += victim.maxHp / n;
        e->hp += victim.maxHp / n;
        e->damage += victim.damage / n;
        e->power += victim.power / n;
        e->speed = std::min(e->speed + victim.speed * 0.05f / n, 220.0f);
        e->radius = e->baseRadius * std::min(1.6f, 1 + 0.3f * (e->power - 1)); // si vede che sono cresciuti
    }
}

// ---------------------------------------------------------------------------
// Movimento
// ---------------------------------------------------------------------------

// Un corpo di raggio r puo' andare da a a b in linea retta senza urtare muri o oggetti?
bool Game::WalkLine(Vector2 a, Vector2 b, float r) const {
    float len = Vector2Distance(a, b);
    int steps = (int)ceilf(len / 10);
    for (int i = 1; i <= steps; ++i)
        if (Blocked(Vector2Lerp(a, b, (float)i / steps), r * 0.9f, 0)) return false;
    return true;
}

// Direzione per raggiungere un punto qualsiasi: dritto se si puo', altrimenti col percorso A*
Vector2 Game::DirToPoint(Enemy& e, Vector2 goal, float dt) {
    Vector2 to = Vector2Subtract(goal, e.pos);
    if (Vector2Length(to) < 2) return { 0, 0 };
    if (e.thinkNow || Vector2DistanceSqr(goal, e.moveGoal) > TILE * TILE) {
        e.directClear = WalkLine(e.pos, goal, e.radius);
        e.moveGoal = goal;
    }
    if (e.directClear && e.avoidDirect <= 0) return Vector2Normalize(to);

    e.repathTimer -= dt;
    if (e.repathTimer <= 0 || Vector2DistanceSqr(goal, e.pathGoal) > (TILE * 1.5f) * (TILE * 1.5f)) {
        e.path = nav.FindPath(e.pos, goal);
        e.pathIndex = 0;
        e.pathGoal = goal;
        e.repathTimer = 1.0f + Rand01() * 0.5f;
    }
    int n = (int)e.path.size();
    while (e.pathIndex < n && Vector2Distance(e.pos, e.path[e.pathIndex]) < TILE * 0.45f) e.pathIndex++;
    // Scorciatoia: se il punto successivo si raggiunge dritto, salta quello attuale (niente zig-zag)
    if (e.thinkNow && e.pathIndex + 1 < n && WalkLine(e.pos, e.path[e.pathIndex + 1], e.radius)) e.pathIndex++;
    if (e.pathIndex >= n) return Vector2Normalize(to);
    return Vector2Normalize(Vector2Subtract(e.path[e.pathIndex], e.pos));
}

// Verso il giocatore: dritto se la strada e' libera, altrimenti con la mappa delle distanze
Vector2 Game::DirToPlayer(Enemy& e) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    if (e.thinkNow || Vector2DistanceSqr(player.pos, e.moveGoal) > TILE * TILE) {
        e.directClear = WalkLine(e.pos, player.pos, e.radius);
        e.moveGoal = player.pos;
    }
    if (e.directClear && e.avoidDirect <= 0) return Vector2Normalize(to);
    Vector2 d = nav.TowardTarget(e.pos);
    return IsZero(d) ? Vector2Normalize(to) : d;
}

// Via dal giocatore lungo la mappa: niente fughe contro il muro. (0,0) = messo all'angolo
Vector2 Game::DirAwayFromPlayer(Enemy& e) {
    return nav.AwayFromTarget(e.pos);
}

// Muove il nemico e si accorge se si e' incastrato (contro un angolo, un'auto, altri nemici)
void Game::Steer(Enemy& e, Vector2 dir, float speed, float dt, bool face) {
    if (IsZero(dir)) return;
    if (face) Face(e, dir, dt);
    Vector2 before = e.pos;
    MoveWithCollision(e.pos, e.radius, Vector2Scale(dir, speed * dt));
    float wanted = speed * dt, moved = Vector2Distance(before, e.pos);
    if (moved < wanted * 0.25f) e.stuckTimer += dt;
    else e.stuckTimer = std::max(0.0f, e.stuckTimer - dt);
    if (e.stuckTimer > 0.4f) {
        // Incastrato: per un po' segue la mappa invece delle linee dritte e si sposta di lato
        e.stuckTimer = 0;
        e.avoidDirect = 1.0f;
        e.repathTimer = 0;
        Vector2 side{ -dir.y, dir.x };
        MoveWithCollision(e.pos, e.radius, Vector2Scale(side, (GetRandomValue(0, 1) ? 1 : -1) * e.radius * 0.5f));
    }
}

// Si gira gradualmente verso una direzione
void Game::Face(Enemy& e, Vector2 dir, float dt) {
    if (IsZero(dir)) return;
    float cur = atan2f(e.facing.y, e.facing.x), want = atan2f(dir.y, dir.x);
    float diff = remainderf(want - cur, 2 * PI);
    float step = 8.0f * dt;
    cur += std::clamp(diff, -step, step);
    e.facing = Dir(cur);
}

Vector2 Game::PredictPlayer(float seconds) const {
    Vector2 v = playerVel;
    if (Vector2Length(v) > 400) v = Vector2Scale(Vector2Normalize(v), 400); // lo scatto non si puo' prevedere
    return Vector2Add(player.pos, Vector2Scale(v, seconds));
}

void Game::EnemyShoot(const Enemy& e, Vector2 dir, float speed, float radius, float damage, EnemyShot look) {
    Projectile p;
    p.pos = Vector2Add(e.pos, Vector2Scale(dir, e.radius * 0.8f));
    p.vel = Vector2Scale(dir, speed);
    p.height = std::max(ENEMY_SHOT_H, player.z + 0.4f); // mira all'altezza a cui sei ora
    p.radius = radius;
    p.damage = damage;
    p.life = 3;
    p.fromPlayer = false;
    p.look = look;
    projectiles.push_back(p);
}

const Enemy* Game::AwakeBoss() const {
    for (const auto& e : enemies)
        if (e.alive && e.type == EnemyType::Oni && e.aggro) return &e;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Comportamenti
// ---------------------------------------------------------------------------

// Nessuno l'ha disturbato: gironzola (goblin), pattuglia (robot) o fa la guardia, e si guarda intorno
void Game::UpdateCalm(Enemy& e, float dt) {
    Vector2 goal = e.home;
    float speed = e.speed * 0.4f;
    if (e.type == EnemyType::Goblin && e.swarm >= 0)
        goal = Vector2Add(swarms[e.swarm].wander, Vector2Scale(Dir(e.slotAngle), 18 + e.id % 3 * 10.0f));
    if (e.type == EnemyType::Robot && e.hasOrder) {
        goal = e.order;
        speed = e.speed * 0.6f;
    }
    if (Vector2Distance(e.pos, goal) > 20) {
        Steer(e, DirToPoint(e, goal, dt), speed, dt);
        return;
    }
    // Fermo: ogni tanto si gira a guardare da un'altra parte (attackDir qui e' solo dove guarda)
    e.strafeTimer -= dt;
    if (e.strafeTimer <= 0) {
        e.strafeTimer = 1.5f + Rand01() * 2.5f;
        e.attackDir = Dir(Rand01() * 2 * PI);
    }
    Face(e, e.attackDir, dt * 0.3f);
}

// Morso del goblin o lama del robot: si ferma, carica (si vede) e colpisce. true = e' impegnato
bool Game::UpdateMeleeAttack(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    float reach = e.radius + player.radius + 10;
    if (e.windup > 0) {
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0) {
            if (dist < reach + 8 && player.z < 0.5f) DamagePlayer(e.damage);
            MoveWithCollision(e.pos, e.radius, Vector2Scale(Vector2Normalize(to), 6)); // lo scatto in avanti
            e.attack = EnemyAttack::None;
            e.attackTimer = 0.8f + Rand01() * 0.6f;
        }
        return true;
    }
    if (dist < reach && e.attackTimer <= 0) {
        e.attack = EnemyAttack::Bite;
        e.windup = e.type == EnemyType::Goblin ? 0.32f : 0.4f;
        return true;
    }
    return false;
}

void Game::UpdateGoblin(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    Swarm* s = e.swarm >= 0 ? &swarms[e.swarm] : nullptr;

    // Sciame troppo piccolo: scappa e raggiunge un altro sciame. Morde solo se messo alle strette
    if (s && s->size < GOBLIN_SWARM_MIN) {
        bool cornered = e.aggro && dist < e.radius + player.radius + 30;
        if ((cornered || e.windup > 0) && UpdateMeleeAttack(e, dt)) return;
        if (s->joinTarget >= 0) {
            const Swarm& target = swarms[s->joinTarget];
            if (Vector2Distance(e.pos, target.center) < 50 + 5.0f * target.size) {
                e.swarm = s->joinTarget; // arrivato: ora fa parte dell'altro sciame
                e.home = target.home;
                e.slotAngle = Rand01() * 2 * PI;
                return;
            }
            Vector2 d = DirToPoint(e, target.center, dt);
            if (e.aggro && dist < 160) {
                Vector2 away = DirAwayFromPlayer(e); // gli passa alla larga
                if (!IsZero(away)) d = Vector2Normalize(Vector2Add(d, Vector2Scale(away, 0.8f)));
            }
            Steer(e, d, e.speed * 1.1f, dt);
            return;
        }
        if (e.aggro) {
            // Nessuno a cui unirsi: tiene le distanze
            Vector2 away = DirAwayFromPlayer(e);
            if (dist < 260 && !IsZero(away)) Steer(e, away, e.speed, dt);
            else if (IsZero(away) && dist < 200) Steer(e, DirToPlayer(e), e.speed, dt); // all'angolo: attacca
            else Face(e, to, dt);
            return;
        }
        UpdateCalm(e, dt);
        return;
    }

    if (!e.aggro) { UpdateCalm(e, dt); return; }
    if (UpdateMeleeAttack(e, dt)) return;
    if (dist < e.radius + player.radius + 4) { Face(e, to, dt); return; } // non entra nella telecamera

    // Ti ha perso: va dove ti ha visto l'ultima volta
    if (Vector2Distance(e.lastSeen, player.pos) > TILE) {
        if (Vector2Distance(e.pos, e.lastSeen) > 24) Steer(e, DirToPoint(e, e.lastSeen, dt), e.speed * 0.8f, dt);
        else UpdateCalm(e, dt);
        return;
    }
    // Da vicino lo sciame ti circonda: ognuno punta al suo posto intorno a te
    if (dist < 140) {
        Vector2 slot = Vector2Add(player.pos, Vector2Scale(Dir(e.slotAngle), player.radius + e.radius + 6));
        Steer(e, DirToPoint(e, slot, dt), e.speed, dt);
        Face(e, to, dt);
        return;
    }
    Steer(e, DirToPlayer(e), e.speed, dt);
}

void Game::UpdateRobot(Enemy& e, float dt) {
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    Vector2 goal = e.hasOrder ? e.order : e.lastSeen;

    if (e.role == RobotRole::Assault) {
        if (UpdateMeleeAttack(e, dt)) return;
        if (dist < e.radius + player.radius + 4) { Face(e, to, dt); return; }
        // Arrivato vicino dal lato assegnato: si butta su di te
        if (e.seesPlayer && dist < 110) {
            Steer(e, DirToPlayer(e), e.speed * 1.1f, dt);
            return;
        }
        if (Vector2Distance(e.pos, goal) > 12) Steer(e, DirToPoint(e, goal, dt), e.speed, dt);
        else Face(e, to, dt);
        return;
    }

    // Artigliere: si piazza dove il GameMaster gli dice, carica e spara una raffica di tre colpi
    if (e.burstLeft > 0) {
        e.actionTimer -= dt;
        Face(e, to, dt);
        if (e.actionTimer <= 0) {
            Vector2 aim = Vector2Normalize(Vector2Subtract(PredictPlayer(dist / 330), e.pos));
            EnemyShoot(e, aim, 330, 4, e.damage * 0.6f, EnemyShot::Laser);
            e.burstLeft--;
            e.actionTimer = 0.12f;
        }
        return;
    }
    if (e.windup > 0) {
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0) {
            e.burstLeft = 3;
            e.actionTimer = 0;
        }
        return;
    }
    if (e.seesPlayer && dist < 460 && e.attackTimer <= 0) {
        e.attack = EnemyAttack::Burst;
        e.windup = 0.45f;
        e.attackTimer = 2.4f + Rand01() * 0.6f;
        return;
    }
    if (dist < 120) {
        Steer(e, DirAwayFromPlayer(e), e.speed, dt, false); // troppo vicino: indietreggia
    } else if (Vector2Distance(e.pos, goal) > 12) {
        Steer(e, DirToPoint(e, goal, dt), e.speed, dt, !e.seesPlayer);
    }
    if (e.seesPlayer) Face(e, to, dt);
}

void Game::UpdateTengu(Enemy& e, float dt) {
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    e.shootTimer -= dt;
    bool canShoot = e.seesPlayer && dist < 480 && e.shootTimer <= 0;
    auto shoot = [&](float angleOffset) {
        Vector2 aim = Vector2Subtract(PredictPlayer(dist / 260 * 0.8f), e.pos);
        float a = atan2f(aim.y, aim.x) + angleOffset;
        EnemyShoot(e, Dir(a), 260, 6, e.damage, EnemyShot::Wind);
    };

    // Ti sei avvicinato troppo: scappa (seguendo la mappa, non contro il muro)
    if (dist < 150) {
        Vector2 away = DirAwayFromPlayer(e);
        if (IsZero(away)) {
            // Messo all'angolo: raffica a ventaglio
            Face(e, to, dt);
            if (canShoot) {
                for (float k : { -0.3f, 0.0f, 0.3f }) shoot(k);
                e.shootTimer = 1.0f;
            }
            return;
        }
        Steer(e, away, e.speed * 1.15f, dt);
        if (canShoot) { shoot(0); e.shootTimer = 2.2f; } // scappando spara meno
        return;
    }
    if (!e.seesPlayer) {
        if (Vector2Distance(e.pos, e.lastSeen) > 24) Steer(e, DirToPoint(e, e.lastSeen, dt), e.speed, dt);
        else UpdateCalm(e, dt);
        return;
    }
    if (dist > 300) {
        Steer(e, DirToPlayer(e), e.speed, dt, false);
    } else {
        // Alla distanza giusta si sposta di lato: non e' un bersaglio facile
        e.strafeTimer -= dt;
        if (e.strafeTimer <= 0) {
            e.strafeTimer = 1.2f + Rand01();
            e.strafeSign = -e.strafeSign;
        }
        Vector2 side = Vector2Scale(Vector2Normalize({ -to.y, to.x }), e.strafeSign);
        if (e.thinkNow && !WalkLine(e.pos, Vector2Add(e.pos, Vector2Scale(side, 30)), e.radius)) e.strafeSign = -e.strafeSign;
        Steer(e, side, e.speed * 0.6f, dt, false);
    }
    Face(e, to, dt);
    if (canShoot) {
        shoot(0);
        e.shootTimer = 1.7f;
    }
}

// L'oni e' il boss: legge le tue mosse, schiva i colpi, carica, e se ti arrampichi ti tira i massi
void Game::UpdateOni(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = std::max(1.0f, Vector2Length(to));
    if (!e.enraged && e.hp < e.maxHp * 0.5f) {
        e.enraged = true;
        e.speed *= 1.3f;
        AddText(e.pos, "L'oni e' furioso!", RED);
    }
    float rage = e.enraged ? 0.7f : 1.0f;
    e.chargeCd -= dt;
    e.slamCd -= dt;
    e.dodgeCd -= dt;
    e.elevatedTimer = player.z > 0.4f ? e.elevatedTimer + dt : 0;

    if (e.stun > 0) { e.stun -= dt; return; }

    // Carica in corso: dritto nella direzione scelta, se sbatte contro qualcosa resta stordito
    if (e.attack == EnemyAttack::Charge && e.windup <= 0 && e.actionTimer > 0) {
        e.actionTimer -= dt;
        Vector2 before = e.pos;
        MoveWithCollision(e.pos, e.radius, Vector2Scale(e.attackDir, CHARGE_SPEED * dt));
        if (!e.attackHit && dist < e.radius + player.radius + 6 && player.z < 0.5f) {
            DamagePlayer(e.damage * 1.2f);
            e.attackHit = true;
        }
        if (Vector2Distance(before, e.pos) < CHARGE_SPEED * dt * 0.3f) {
            e.stun = 1.3f;
            e.actionTimer = 0;
            AddText(e.pos, "Stordito!", YELLOW);
        }
        if (e.actionTimer <= 0) {
            e.attack = EnemyAttack::None;
            e.attackTimer = 0.6f * rage;
        }
        return;
    }
    if (e.dodgeTime > 0) {
        e.dodgeTime -= dt;
        MoveWithCollision(e.pos, e.radius, Vector2Scale(e.dodgeVel, dt));
        return;
    }

    // Preavviso dell'attacco: e' il momento di scappare, saltare o scattare
    if (e.windup > 0) {
        e.windup -= dt;
        if (e.attack != EnemyAttack::Charge) Face(e, to, dt); // la carica ha la direzione gia' decisa
        if (e.windup > 0) return;
        switch (e.attack) {
            case EnemyAttack::Swing:
                if (dist < e.radius + player.radius + 40 && Vector2DotProduct(e.facing, Vector2Scale(to, 1 / dist)) > 0.2f &&
                    player.z < 0.6f)
                    DamagePlayer(e.damage);
                e.attackTimer = 1.1f * rage;
                break;
            case EnemyAttack::Slam:
                if (dist < ONI_SLAM_RADIUS + player.radius && player.z < 0.25f) DamagePlayer(e.damage * 1.3f);
                AddText(e.pos, "Schianto!", ORANGE);
                e.slamCd = 5 * rage;
                e.attackTimer = 1.0f * rage;
                break;
            case EnemyAttack::Throw: {
                Vector2 aim = Vector2Normalize(Vector2Subtract(PredictPlayer(dist / 300), e.pos));
                EnemyShoot(e, aim, 300, 10, e.damage * 0.8f, EnemyShot::Rock);
                e.attackTimer = 1.4f * rage;
                break;
            }
            case EnemyAttack::Charge:
                e.actionTimer = 0.85f;
                e.attackHit = false;
                return; // resta in modalita' carica
            default:
                break;
        }
        e.attack = EnemyAttack::None;
        return;
    }

    if (!e.aggro) { UpdateCalm(e, dt); return; }

    // Schiva i colpi che stanno per prenderlo (non sempre: ha bisogno di riprendere fiato)
    if (e.dodgeCd <= 0) {
        for (const auto& p : projectiles) {
            if (!p.alive || !p.fromPlayer) continue;
            Vector2 rel = Vector2Subtract(e.pos, p.pos);
            if (Vector2Length(rel) > 140) continue;
            Vector2 v = Vector2Normalize(p.vel);
            if (Vector2DotProduct(rel, v) <= 0) continue; // si sta allontanando
            float miss = fabsf(v.x * rel.y - v.y * rel.x); // distanza dalla traiettoria
            if (miss > e.radius + p.radius + 4) continue;
            Vector2 side{ -v.y, v.x };
            if (Vector2DotProduct(side, rel) < 0) side = Vector2Negate(side);
            if (!WalkLine(e.pos, Vector2Add(e.pos, Vector2Scale(side, 50)), e.radius)) side = Vector2Negate(side);
            e.dodgeVel = Vector2Scale(side, 260);
            e.dodgeTime = 0.2f;
            e.dodgeCd = e.enraged ? 1.6f : 2.6f;
            return;
        }
    }

    // Sceglie l'attacco in base a dove sei e a cosa stai facendo
    float reach = e.radius + player.radius + 34;
    if (e.attackTimer <= 0 && e.seesPlayer) {
        if (e.enraged && e.slamCd <= 0 && dist < ONI_SLAM_RADIUS * 0.9f) {
            e.attack = EnemyAttack::Slam;
            e.windup = 0.75f;
            return;
        }
        if (dist < reach) {
            e.attack = EnemyAttack::Swing;
            e.windup = std::max(0.38f, 0.55f * rage);
            return;
        }
        if (e.elevatedTimer > 1.0f && dist < 420) { // sei salito su qualcosa per non farti prendere
            e.attack = EnemyAttack::Throw;
            e.windup = 0.5f;
            return;
        }
        if (e.chargeCd <= 0 && dist > 150 && dist < 380 && WalkLine(e.pos, player.pos, e.radius)) {
            e.attack = EnemyAttack::Charge;
            e.windup = 0.7f;
            e.attackDir = Vector2Normalize(Vector2Subtract(PredictPlayer(0.5f), e.pos)); // mira dove starai
            e.chargeCd = 4.5f * rage;
            return;
        }
    }

    if (dist < reach * 0.8f || (player.z > 0.4f && dist < reach)) { Face(e, to, dt); return; }
    if (e.seesPlayer) {
        // Ti taglia la strada: punta a dove starai, non a dove sei
        Vector2 intercept = PredictPlayer(std::min(0.6f, dist / 400));
        Vector2 d = WalkLine(e.pos, intercept, e.radius) ? Vector2Normalize(Vector2Subtract(intercept, e.pos)) : DirToPlayer(e);
        Steer(e, d, e.speed, dt);
        return;
    }
    if (Vector2Distance(e.pos, e.lastSeen) > 24) Steer(e, DirToPoint(e, e.lastSeen, dt), e.speed, dt);
    else UpdateCalm(e, dt);
}
