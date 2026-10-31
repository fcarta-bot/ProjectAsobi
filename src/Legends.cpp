// Le creature leggendarie del giro del mondo: criptidi americani, folletti italiani, mostri delle foreste
// tedesche, bestie francesi, segugi inglesi e ombre spagnole. Ognuna ha la sua regola speciale.
#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include "raymath.h"

static float Rand01() { return GetRandomValue(0, 10000) / 10000.0f; }
static bool IsZero(Vector2 v) { return v.x == 0 && v.y == 0; }
static Vector2 Dir(float angle) { return { cosf(angle), sinf(angle) }; }

using E = EnemyType;
using C = Country;

const EnemyInfo& InfoOf(EnemyType t) {
    //   nome, paese, raggio, vita, velocita', danno, vista, memoria, noia, guardiano, titolo
    static const EnemyInfo info[ENEMY_TYPE_COUNT] = {
        { "Goblin",        C::Japan,   9,  16, 165,  6, 300,  5, 0.5f,  false, "" },
        { "Robot",         C::Japan,  12,  45, 120, 12, 420,  9, 0.25f, false, "" },
        { "Tengu",         C::Japan,  13,  30, 125, 12, 450,  6, 0.4f,  false, "" },
        { "Oni",           C::Japan,  22, 420,  90, 24, 480, 14, 0.15f, true,  "ONI - Guardiano del torii" },
        { "Wendigo",       C::USA,    18, 380, 120, 20, 520, 16, 0.1f,  true,  "WENDIGO - Guardiano del torii" },
        { "Mothman",       C::USA,    14,  60, 150, 12, 520, 10, 0.2f,  false, "" },
        { "Jersey Devil",  C::USA,    17, 200, 130, 16, 460, 10, 0.2f,  false, "" },
        { "Gnefro",        C::Italy,   7,  10, 175,  5, 260,  6, 0.5f,  false, "" },
        { "Uomo Nero",     C::Italy,  15, 160, 110, 14, 600, 99, 0.0f,  false, "" },
        { "Badalisc",      C::Italy,  22, 380,  70, 18, 400, 12, 0.1f,  true,  "BADALISC - Guardiano del torii" },
        { "Der Grossmann", C::Germany, 14, 360, 190, 22, 700, 99, 0.0f, true,  "DER GROSSMANN - Guardiano del torii" },
        { "Nachzehrer",    C::Germany, 14, 120,  70, 14, 360, 12, 0.4f, false, "" },
        { "Wolpertinger",  C::Germany,  8,  14, 230,  6, 320,  5, 0.5f, false, "" },
        { "Bestia del Gevaudan", C::France, 20, 400, 170, 22, 520, 16, 0.1f, true, "LA BESTIA DEL GEVAUDAN - Guardiano del torii" },
        { "Dahu",          C::France, 10,   1, 150,  0, 260,  3, 1.0f,  false, "" },
        { "Matagot",       C::France, 10,  60, 140, 11, 460,  8, 0.3f,  false, "" },
        { "Black Shuck",   C::England, 18, 360, 210, 20, 560, 99, 0.0f, true,  "BLACK SHUCK - Guardiano del torii" },
        { "Spring-heeled Jack", C::England, 12, 90, 160, 13, 520, 10, 0.2f, false, "" },
        { "Berretto Rosso", C::England, 9, 30, 160,  7, 320,  8, 0.3f,  false, "" },
        { "El Cuco",       C::Spain,  17, 380, 140, 20, 500, 10, 0.2f,  true,  "EL CUCO - Guardiano del torii" },
        { "Dip",           C::Spain,  12,  40, 130,  9, 380,  7, 0.3f,  false, "" },
        { "Santa Compana", C::Spain,  11,   1,  40,  0,   0,  0, 0.0f,  false, "" },
    };
    return info[(int)t];
}

// ---------------------------------------------------------------------------
// Smistamento e strumenti comuni
// ---------------------------------------------------------------------------

void Game::UpdateLegend(Enemy& e, float dt) {
    switch (e.type) {
        case E::Wendigo:      UpdateWendigo(e, dt); break;
        case E::Mothman:      UpdateMothman(e, dt); break;
        case E::JerseyDevil:  UpdateJerseyDevil(e, dt); break;
        case E::Gnefro:       UpdateGnefro(e, dt); break;
        case E::Babau:        UpdateBabau(e, dt); break;
        case E::Badalisc:     UpdateBadalisc(e, dt); break;
        case E::Grossmann:    UpdateGrossmann(e, dt); break;
        case E::Nachzehrer:   UpdateNachzehrer(e, dt); break;
        case E::Wolpertinger: UpdateWolpertinger(e, dt); break;
        case E::Gevaudan:     UpdateGevaudan(e, dt); break;
        case E::Dahu:         UpdateDahu(e, dt); break;
        case E::Matagot:      UpdateMatagot(e, dt); break;
        case E::BlackShuck:   UpdateBlackShuck(e, dt); break;
        case E::SpringJack:   UpdateSpringJack(e, dt); break;
        case E::Redcap:       UpdateRedcap(e, dt); break;
        case E::Cuco:         UpdateCuco(e, dt); break;
        case E::Dip:          UpdateDip(e, dt); break;
        case E::Compana:      UpdateCompana(e, dt); break;
        default: break;
    }
}

// Inseguimento di base: morde da vicino, va dove ti ha visto l'ultima volta. false = e' tranquillo
bool Game::Chase(Enemy& e, float dt, float speedScale) {
    if (!e.aggro) { UpdateCalm(e, dt); return false; }
    if (UpdateMeleeAttack(e, dt)) return true;
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    if (dist < e.radius + player.radius + 4) { Face(e, to, dt); return true; } // non entra nella telecamera
    if (Vector2Distance(e.lastSeen, player.pos) > TILE) {
        if (Vector2Distance(e.pos, e.lastSeen) > 24) Steer(e, DirToPoint(e, e.lastSeen, dt), e.speed * 0.8f * speedScale, dt);
        else UpdateCalm(e, dt);
        return true;
    }
    Steer(e, DirToPlayer(e), e.speed * speedScale, dt);
    return true;
}

// Balzo: parte da dove si trova e atterra nel punto scelto. Atterrando colpisce chi e' a terra li' vicino
void Game::StartLeap(Enemy& e, Vector2 to, float duration) {
    e.attack = EnemyAttack::Leap;
    e.leapFrom = e.pos;
    e.leapTo = nav.NearestWalkable(to);
    e.actionTimer = duration;
    e.dodgeTime = duration; // durata totale (l'oni usa questo campo per le schivate, gli altri no)
    e.attackHit = false;
}

bool Game::UpdateLeap(Enemy& e, float dt, float height, float landRadius, float damage) {
    if (e.attack != EnemyAttack::Leap || e.actionTimer <= 0) return false;
    e.actionTimer -= dt;
    float k = 1 - std::max(0.0f, e.actionTimer) / e.dodgeTime;
    Vector2 want = Vector2Lerp(e.leapFrom, e.leapTo, k);
    MoveWithCollision(e.pos, e.radius, Vector2Subtract(want, e.pos), 99); // in volo passa sopra gli oggetti
    e.z = height * 4 * k * (1 - k);
    if (e.actionTimer <= 0) {
        e.z = 0;
        e.attack = EnemyAttack::None;
        if (landRadius > 0 && Vector2Distance(e.pos, player.pos) < landRadius + player.radius && player.z < 0.3f) {
            if (DamagePlayer(damage)) HurtByLegend(e, damage);
        }
        if (landRadius > 0) AddText(e.pos, "Tonf!", ORANGE);
    }
    return true;
}

bool Game::Damageable(const Enemy& e) const {
    if (e.type == E::Compana || e.type == E::Dahu) return false;
    if (e.type == E::Babau && e.hidden) return false; // senza paura l'Uomo Nero e' solo un'ombra
    return true;
}

// Il nemico ti ha appena colpito
void Game::HurtByLegend(Enemy& e, float dmg) {
    switch (e.type) {
        case E::Wendigo: e.hp = std::min(e.maxHp, e.hp + dmg * 0.5f); break; // si nutre
        case E::Redcap:
            e.stacks++;
            e.damage *= 1.25f;
            e.radius = std::min(e.baseRadius * 1.7f, e.radius + 1.0f);
            AddText(e.pos, "Il berretto si tinge...", { 220, 40, 40, 255 });
            break;
        case E::Dip:
            AddCurse(CurseType::Bleed, 5);
            e.hp = std::min(e.maxHp, e.hp + 10);
            break;
        case E::Wolpertinger: e.special = 1.2f; break; // morde e scappa
        case E::Nachzehrer: e.hp = std::min(e.maxHp, e.hp + dmg); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Paura e maledizioni
// ---------------------------------------------------------------------------

void Game::UpdateFear(float dt) {
    if (!fearFloor) {
        fear = std::max(0.0f, fear - dt * 0.2f);
        return;
    }
    float change = -0.025f;                                   // da sola passa piano piano...
    if (player.hp < stats.maxHp * 0.4f) change += 0.05f;      // ...ma col fiato corto sale
    if (CurrentDistrict().night) change += 0.01f;
    for (const auto& e : enemies) {
        if (!e.alive || (e.type != E::Babau && e.type != E::Cuco)) continue;
        float d = Vector2Distance(e.pos, player.pos);
        if (d < 350) change += e.hidden ? 0.06f : 0.04f;      // qualcosa ti osserva dal buio
    }
    fear = std::clamp(fear + change * dt, 0.0f, 1.0f);
}

void Game::AddCurse(CurseType type, float seconds) {
    for (Curse& c : curses)
        if (c.type == type) { c.time = std::max(c.time, seconds); return; }
    curses.push_back({ type, seconds });
}

bool Game::HasCurse(CurseType type) const {
    return std::any_of(curses.begin(), curses.end(), [type](const Curse& c) { return c.type == type; });
}

void Game::UpdateCurses(float dt) {
    for (Curse& c : curses) {
        c.time -= dt;
        if (c.type == CurseType::Compana) DrainPlayer(3 * dt);
        if (c.type == CurseType::Bleed) DrainPlayer(2 * dt);
    }
    curses.erase(std::remove_if(curses.begin(), curses.end(), [](const Curse& c) { return c.time <= 0; }), curses.end());
}

// ---------------------------------------------------------------------------
// Stati Uniti
// ---------------------------------------------------------------------------

// Wendigo: lo spirito affamato delle foreste del nord. Scatta come un fulmine e, se e' ferito,
// corre a divorare i caduti per rimettersi in forze
void Game::UpdateWendigo(Enemy& e, float dt) {
    if (!e.enraged && e.hp < e.maxHp * 0.35f) {
        e.enraged = true;
        e.speed *= 1.3f;
        AddText(e.pos, "Il Wendigo ha fame!", RED);
    }
    e.chargeCd -= dt;
    e.specialCd -= dt;
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);

    // Sta mangiando: fermo, e si cura in fretta (e' il momento di colpirlo!)
    if (e.special > 0) {
        e.special -= dt;
        e.hp = std::min(e.maxHp, e.hp + e.maxHp * 0.08f * dt);
        if (e.special <= 0 && !corpses.empty()) {
            auto it = std::min_element(corpses.begin(), corpses.end(), [&](const Corpse& a, const Corpse& b) {
                return Vector2Distance(a.pos, e.pos) < Vector2Distance(b.pos, e.pos);
            });
            corpses.erase(it);
        }
        return;
    }
    // Scatto in corso
    if (e.attack == EnemyAttack::Charge && e.windup <= 0 && e.actionTimer > 0) {
        e.actionTimer -= dt;
        MoveWithCollision(e.pos, e.radius, Vector2Scale(e.attackDir, 470 * dt));
        if (!e.attackHit && dist < e.radius + player.radius + 8 && player.z < 0.5f) {
            if (DamagePlayer(e.damage * 1.2f)) HurtByLegend(e, e.damage);
            e.attackHit = true;
        }
        if (e.actionTimer <= 0) { e.attack = EnemyAttack::None; e.attackTimer = 0.6f; }
        return;
    }
    if (e.windup > 0 && e.attack == EnemyAttack::Charge) {
        e.windup -= dt;
        if (e.windup <= 0) { e.actionTimer = 0.6f; e.attackHit = false; }
        return;
    }
    if (!e.aggro) { UpdateCalm(e, dt); return; }

    // Ferito: cerca un cadavere da divorare
    if (e.hp < e.maxHp * 0.5f && e.specialCd <= 0 && !corpses.empty()) {
        const Corpse* best = nullptr;
        for (const Corpse& c : corpses)
            if (Vector2Distance(c.pos, e.pos) < 700 && (!best || Vector2Distance(c.pos, e.pos) < Vector2Distance(best->pos, e.pos))) best = &c;
        if (best) {
            if (Vector2Distance(best->pos, e.pos) < 22) {
                e.special = 2.5f;
                e.specialCd = 12;
                AddText(e.pos, "Il Wendigo divora un caduto!", { 200, 60, 60, 255 });
            } else {
                Steer(e, DirToPoint(e, best->pos, dt), e.speed * 1.3f, dt);
            }
            return;
        }
    }
    if (e.seesPlayer && e.chargeCd <= 0 && dist > 120 && dist < 420 && WalkLine(e.pos, player.pos, e.radius)) {
        e.attack = EnemyAttack::Charge;
        e.windup = 0.5f;
        e.attackDir = Vector2Normalize(Vector2Subtract(PredictPlayer(0.4f), e.pos));
        e.chargeCd = e.enraged ? 3.0f : 4.5f;
        return;
    }
    Chase(e, dt);
}

// Mothman: vola alto in cerchio, quasi invisibile tranne gli occhi rossi. Se ti fissa troppo a lungo
// porta sfortuna (subisci piu' danni). Ogni tanto si butta in picchiata: e' il momento di colpirlo
void Game::UpdateMothman(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    e.specialCd -= dt;
    bool diving = e.attack == EnemyAttack::Charge;
    e.alpha = diving || dist < 170 ? 1.0f : 0.2f;
    e.z = diving && e.windup <= 0 ? std::max(0.0f, e.z - dt * 3) : std::min(1.2f, e.z + dt * 1.5f);

    if (diving && e.windup <= 0 && e.actionTimer > 0) {
        e.actionTimer -= dt;
        MoveWithCollision(e.pos, e.radius, Vector2Scale(e.attackDir, 400 * dt), 99);
        if (!e.attackHit && dist < e.radius + player.radius + 8) {
            DamagePlayer(e.damage);
            e.attackHit = true;
        }
        if (e.actionTimer <= 0) { e.attack = EnemyAttack::None; e.attackTimer = 3.5f; }
        return;
    }
    if (diving && e.windup > 0) {
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0) {
            e.attackDir = Vector2Normalize(Vector2Subtract(PredictPlayer(0.3f), e.pos));
            e.actionTimer = 0.75f;
            e.attackHit = false;
        }
        return;
    }
    if (!e.aggro) { UpdateCalm(e, dt); return; }

    // Il presagio
    if (e.seesPlayer) e.watched += dt;
    if (e.watched > 3 && e.specialCd <= 0) {
        AddCurse(CurseType::Omen, 10);
        AddPlayerText("Presagio di sventura!", { 255, 80, 80, 255 });
        e.specialCd = 16;
        e.watched = 0;
    }
    if (e.seesPlayer && e.attackTimer <= 0 && dist < 330) {
        e.attack = EnemyAttack::Charge;
        e.windup = 0.6f;
        AddText(e.pos, "Iiiih!", { 255, 90, 90, 255 });
        return;
    }
    // Gira intorno al giocatore
    e.slotAngle += dt * 0.8f;
    Vector2 orbit = nav.NearestWalkable(Vector2Add(player.pos, Vector2Scale(Dir(e.slotAngle), 210)));
    Steer(e, DirToPoint(e, orbit, dt), e.speed, dt, false);
    Face(e, to, dt);
}

// Jersey Devil: zoccoli e ali da pipistrello. Balza addosso e lo schianto si evita saltando
void Game::UpdateJerseyDevil(Enemy& e, float dt) {
    if (UpdateLeap(e, dt, 1.8f, 70, e.damage * 1.2f)) return;
    e.specialCd -= dt;
    if (!e.enraged && e.hp < e.maxHp * 0.5f) {
        e.enraged = true;
        fear = std::min(1.0f, fear + 0.2f);
        AddText(e.pos, "Un urlo agghiacciante!", ORANGE);
        for (auto& o : enemies) // l'urlo sveglia tutti quelli vicini
            if (o.alive && o.hostile && Vector2Distance(o.pos, e.pos) < 500) { o.aggro = true; o.interest = 1; o.lastSeen = player.pos; }
    }
    float dist = Vector2Distance(e.pos, player.pos);
    if (e.windup > 0 && e.attack == EnemyAttack::Leap) {
        e.windup -= dt;
        Face(e, Vector2Subtract(player.pos, e.pos), dt);
        if (e.windup <= 0) StartLeap(e, PredictPlayer(0.6f), 0.8f);
        return;
    }
    if (e.aggro && e.seesPlayer && e.specialCd <= 0 && dist > 100 && dist < 380) {
        e.attack = EnemyAttack::Leap;
        e.windup = 0.45f;
        e.actionTimer = 0;
        e.specialCd = e.enraged ? 2.5f : 3.5f;
        return;
    }
    Chase(e, dt);
}

// ---------------------------------------------------------------------------
// Italia
// ---------------------------------------------------------------------------

// Gnefri: folletti maliziosi. Restano nascosti sotto le macerie e saltano fuori tutti insieme
void Game::UpdateGnefro(Enemy& e, float dt) {
    float dist = Vector2Distance(e.pos, player.pos);
    if (e.hidden) {
        bool trigger = (dist < 100 && dungeon.HasLineOfSight(e.pos, player.pos)) || e.aggro;
        if (!trigger) return;
        int revealed = 0;
        for (auto& o : enemies) {
            if (!o.alive || o.type != E::Gnefro || o.swarm != e.swarm || !o.hidden) continue;
            o.hidden = false;
            o.aggro = true;
            o.interest = 1;
            o.lastSeen = player.pos;
            o.attackTimer = 0.3f + Rand01() * 0.4f;
            revealed++;
        }
        if (revealed > 1) AddText(e.pos, "Imboscata!", { 255, 200, 80, 255 });
        return;
    }
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    if (UpdateMeleeAttack(e, dt)) return;
    if (dist < 130) { // ti saltellano intorno
        Vector2 slot = Vector2Add(player.pos, Vector2Scale(Dir(e.slotAngle + (float)GetTime() * 0.7f), player.radius + e.radius + 10));
        Steer(e, DirToPoint(e, slot, dt), e.speed, dt);
        Face(e, Vector2Subtract(player.pos, e.pos), dt);
        return;
    }
    Chase(e, dt);
}

// L'Uomo Nero: un'ombra senza volto che ti segue alle spalle. Si materializza solo quando hai molta
// paura, e piu' paura hai piu' e' forte. Se ti calmi torna nell'ombra
void Game::UpdateBabau(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    if (e.hidden) {
        e.alpha = 0.12f;
        e.radius = e.baseRadius;
        // Ti sta dietro, fuori dal tuo sguardo
        Vector2 behind = nav.NearestWalkable(Vector2Subtract(player.pos, Vector2Scale(player.aim, 240)));
        if (Vector2Distance(e.pos, behind) > 30) Steer(e, DirToPoint(e, behind, dt), e.speed * 1.2f, dt);
        else Face(e, to, dt);
        if (fear >= 0.6f) {
            e.hidden = false;
            e.aggro = true;
            e.interest = 1;
            e.lastSeen = player.pos;
            AddText(e.pos, "L'Uomo Nero si materializza!", { 180, 120, 255, 255 });
        }
        return;
    }
    e.alpha = 0.6f + 0.4f * fear;
    e.radius = e.baseRadius * (1 + 0.5f * fear);
    e.damage = e.power * (1 + fear);
    if (fear < 0.4f) {
        e.hidden = true;
        e.attack = EnemyAttack::None;
        e.windup = 0;
        AddText(e.pos, "L'Uomo Nero torna nel buio...", { 180, 120, 255, 255 });
        return;
    }
    e.aggro = true;
    e.interest = 1;
    e.lastSeen = player.pos; // sente la tua paura: sa sempre dove sei
    Chase(e, dt, 0.8f + fear * 0.6f);
}

// Badalisc: la creatura a forma di sacco della Val Camonica. Se ne sta tranquillo vicino al torii
// a spettegolare: se lo attacchi si offende, rotola come una botte e ti sputa addosso i pettegolezzi
void Game::UpdateBadalisc(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    e.specialCd -= dt;
    e.shootTimer -= dt;
    e.chargeCd -= dt;
    if (!e.hostile) {
        UpdateCalm(e, dt);
        if (dist < 160) Face(e, to, dt);
        if (dist < 160 && e.specialCd <= 0) {
            static const char* gossip[] = {
                "\"Lo sai che il GameMaster parla da solo?\"",
                "\"Io non dico niente... ma il tengu ha un debole per l'oni.\"",
                "\"Passa pure, ma non toccarmi. Sono delicato.\"",
                "\"Hai una faccia da sonnambulo, sai?\"",
            };
            AddText(e.pos, gossip[GetRandomValue(0, 3)], { 230, 220, 160, 255 });
            e.specialCd = 7;
        }
        return;
    }
    // Rotola: rimbalza sui muri finche' non si ferma, poi gli gira la testa
    if (e.attack == EnemyAttack::Roll && e.windup <= 0 && e.actionTimer > 0) {
        e.actionTimer -= dt;
        e.slamCd -= dt;
        Vector2 step = Vector2Scale(e.attackDir, 360 * dt);
        Vector2 before = e.pos;
        MoveWithCollision(e.pos, e.radius, step);
        if (fabsf(e.pos.x - before.x) < fabsf(step.x) * 0.5f) e.attackDir.x = -e.attackDir.x;
        if (fabsf(e.pos.y - before.y) < fabsf(step.y) * 0.5f) e.attackDir.y = -e.attackDir.y;
        if (e.slamCd <= 0 && dist < e.radius + player.radius + 6 && player.z < 0.6f) {
            DamagePlayer(e.damage);
            e.slamCd = 0.6f;
        }
        if (e.actionTimer <= 0) {
            e.attack = EnemyAttack::None;
            e.stun = 0.9f;
            AddText(e.pos, "Gli gira la testa", YELLOW);
        }
        return;
    }
    if (e.windup > 0) {
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0 && e.attack == EnemyAttack::Roll) {
            e.attackDir = Vector2Normalize(to);
            e.actionTimer = 2.4f;
            e.slamCd = 0;
        }
        return;
    }
    if (!e.aggro) { e.aggro = true; e.interest = 1; e.lastSeen = player.pos; }
    if (e.chargeCd <= 0 && dist > 90 && e.seesPlayer) {
        e.attack = EnemyAttack::Roll;
        e.windup = 0.7f;
        e.chargeCd = 6;
        return;
    }
    if (e.shootTimer <= 0 && e.seesPlayer && dist < 420) { // cinque pettegolezzi a ventaglio
        float a = atan2f(to.y, to.x);
        for (int k = -2; k <= 2; ++k) EnemyShoot(e, Dir(a + k * 0.22f), 230, 6, e.damage * 0.5f, EnemyShot::Gossip);
        e.shootTimer = 3;
        return;
    }
    Chase(e, dt);
}

// ---------------------------------------------------------------------------
// Germania
// ---------------------------------------------------------------------------

// Der Grossmann, l'Uomo Alto della Foresta Nera: se lo guardi si ferma, e dopo un po' sparisce
// per ricomparirti alle spalle. Quando non lo guardi si avvicina in fretta e allunga le braccia
void Game::UpdateGrossmann(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    grossmannNear = std::max(grossmannNear, std::clamp(1 - dist / 420, 0.0f, 1.0f) * (e.aggro ? 1.0f : 0.4f));
    Vector2 fromPlayer = Vector2Scale(to, -1 / std::max(1.0f, dist));
    bool looked = Vector2DotProduct(fromPlayer, player.aim) > 0.82f && dist < VIEW_DIST * TILE &&
                  dungeon.HasLineOfSight(player.pos, e.pos);

    if (e.windup > 0) { // braccia che si allungano
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0) {
            if (dist < 140 && player.z < 1.0f) DamagePlayer(e.damage);
            e.attack = EnemyAttack::None;
            e.attackTimer = 1.4f;
        }
        return;
    }
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    e.lastSeen = player.pos;

    if (looked) {
        e.watched += dt;
        Face(e, to, dt);
        if (e.watched > 1.6f) {
            // Sparisce e ricompare alle tue spalle
            for (int tries = 0; tries < 24; ++tries) {
                float a = atan2f(-player.aim.y, -player.aim.x) + (Rand01() - 0.5f) * 1.6f;
                Vector2 spot = Vector2Add(player.pos, Vector2Scale(Dir(a), TILE * (5 + Rand01() * 4)));
                if (!nav.WalkableAt(spot)) continue;
                e.pos = nav.NearestWalkable(spot);
                e.path.clear();
                break;
            }
            e.watched = 0;
            fear = std::min(1.0f, fear + 0.1f);
        }
        return;
    }
    e.watched = std::max(0.0f, e.watched - dt);
    if (dist < 125 && e.attackTimer <= 0) {
        e.attack = EnemyAttack::Reach;
        e.windup = 0.55f;
        return;
    }
    if (dist > 60) Steer(e, DirToPlayer(e), e.speed, dt);
    else Face(e, to, dt);
}

// Nachzehrer: il morto famelico. Lento, ma da vicino ti prosciuga la vita e la prende per se'
void Game::UpdateNachzehrer(Enemy& e, float dt) {
    float dist = Vector2Distance(e.pos, player.pos);
    if (e.aggro && dist < 85 && dungeon.HasLineOfSight(e.pos, player.pos)) {
        DrainPlayer(5 * dt);
        e.hp = std::min(e.maxHp, e.hp + 5 * dt);
    }
    Chase(e, dt);
}

// Wolpertinger: la lepre cornuta e alata della Baviera. Morde e scappa, poi torna
void Game::UpdateWolpertinger(Enemy& e, float dt) {
    e.z = fabsf(sinf((float)GetTime() * 9 + e.id)) * 0.25f; // saltella
    if (e.special > 0) {
        e.special -= dt;
        Vector2 away = DirAwayFromPlayer(e);
        if (!IsZero(away)) Steer(e, away, e.speed * 1.1f, dt);
        return;
    }
    Chase(e, dt);
}

// ---------------------------------------------------------------------------
// Francia
// ---------------------------------------------------------------------------

// La Bestia del Gevaudan: lupo enorme che balza addosso. Le armi normali non la scalfiscono:
// servono l'argento o il fuoco (il controllo e' in DamageEnemy)
void Game::UpdateGevaudan(Enemy& e, float dt) {
    if (UpdateLeap(e, dt, 1.2f, 50, e.damage)) return;
    if (!e.enraged && e.hp < e.maxHp * 0.5f) {
        e.enraged = true;
        e.speed *= 1.25f;
        fear = std::min(1.0f, fear + 0.25f);
        AddText(e.pos, "La Bestia ulula!", RED);
    }
    e.specialCd -= dt;
    float dist = Vector2Distance(e.pos, player.pos);
    if (e.windup > 0 && e.attack == EnemyAttack::Leap) {
        e.windup -= dt;
        Face(e, Vector2Subtract(player.pos, e.pos), dt);
        if (e.windup <= 0) StartLeap(e, PredictPlayer(0.45f), 0.55f);
        return;
    }
    if (e.aggro && e.seesPlayer && e.specialCd <= 0 && dist > 90 && dist < 300) {
        e.attack = EnemyAttack::Leap;
        e.windup = 0.4f;
        e.actionTimer = 0;
        e.specialCd = e.enraged ? 2.0f : 3.2f;
        return;
    }
    Chase(e, dt);
}

// Dahu: l'animale di montagna con le zampe di un lato piu' corte. Innocuo, ma sa girare solo
// a sinistra: gira in tondo. Chi riesce a toccarlo ha in regalo un artefatto
void Game::UpdateDahu(Enemy& e, float dt) {
    float dist = Vector2Distance(e.pos, player.pos);
    if (dist < e.radius + player.radius + 6) {
        e.alive = false;
        dahuCaught = true;
        AddPlayerText("Hai preso il Dahu!", GOLD);
        pickups.push_back({ e.pos, RandomArtifact() });
        return;
    }
    bool scared = dist < 180;
    float heading = atan2f(e.facing.y, e.facing.x) + dt * (scared ? 2.4f : 1.2f); // sempre a sinistra
    e.facing = Dir(heading);
    Vector2 before = e.pos;
    MoveWithCollision(e.pos, e.radius, Vector2Scale(e.facing, (scared ? e.speed : e.speed * 0.45f) * dt));
    if (Vector2Distance(before, e.pos) < 0.3f) e.facing = Dir(heading + 1.2f); // contro un muro: gira di piu'
}

// Matagot: lo spirito-gatto nero. Sparisce e ricompare intorno a te lanciando sfere spiritiche.
// Porta ricchezza a chi lo sconfigge... ma il patto si paga col sangue (vedi KillEnemy)
void Game::UpdateMatagot(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    e.specialCd -= dt;
    e.shootTimer -= dt;
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    if (e.specialCd <= 0) { // salta in un altro punto intorno a te
        for (int tries = 0; tries < 16; ++tries) {
            Vector2 spot = Vector2Add(player.pos, Vector2Scale(Dir(Rand01() * 2 * PI), 110 + Rand01() * 80));
            if (!nav.WalkableAt(spot) || !dungeon.HasLineOfSight(spot, player.pos)) continue;
            AddText(e.pos, "puf", { 180, 120, 255, 255 });
            e.pos = nav.NearestWalkable(spot);
            e.path.clear();
            break;
        }
        e.specialCd = 2.6f + Rand01();
    }
    Face(e, to, dt);
    if (e.seesPlayer && e.shootTimer <= 0 && dist < 460) {
        float a = atan2f(to.y, to.x);
        for (float k : { -0.15f, 0.15f }) EnemyShoot(e, Dir(a + k), 240, 6, e.damage, EnemyShot::Spirit);
        e.shootTimer = 1.8f;
    }
    if (dist < 140) {
        Vector2 away = DirAwayFromPlayer(e);
        if (!IsZero(away)) Steer(e, away, e.speed, dt, false);
    }
}

// ---------------------------------------------------------------------------
// Inghilterra
// ---------------------------------------------------------------------------

// Black Shuck: il cane nero dagli occhi di fuoco. Sorveglia la sua brughiera (la stanza del torii):
// chi ci entra viene inseguito senza tregua, ovunque vada
void Game::UpdateBlackShuck(Enemy& e, float dt) {
    if (UpdateLeap(e, dt, 0.6f, 40, e.damage)) return;
    float dist = Vector2Distance(e.pos, player.pos);
    if (e.special <= 0 && Vector2Distance(player.pos, e.anchor) < e.territory) {
        e.special = 1;
        e.hostile = true;
        AddText(e.pos, "Un ululato dalla brughiera...", ORANGE);
        fear = std::min(1.0f, fear + 0.15f);
    }
    if (e.special > 0) { // ti ha marchiato: sa sempre dove sei
        e.aggro = true;
        e.interest = 1;
        e.lastSeen = player.pos;
    }
    e.specialCd -= dt;
    if (e.windup > 0 && e.attack == EnemyAttack::Leap) {
        e.windup -= dt;
        if (e.windup <= 0) StartLeap(e, PredictPlayer(0.35f), 0.4f);
        return;
    }
    if (e.aggro && e.specialCd <= 0 && dist > 80 && dist < 220 && dungeon.HasLineOfSight(e.pos, player.pos)) {
        e.attack = EnemyAttack::Leap;
        e.windup = 0.3f;
        e.actionTimer = 0;
        e.specialCd = 2.2f;
        return;
    }
    if (!e.aggro) { // pattuglia la sua brughiera
        if (Vector2Distance(e.home, e.anchor) > e.territory) e.home = e.anchor;
        UpdateCalm(e, dt);
        return;
    }
    Chase(e, dt);
}

// Spring-heeled Jack: balzi enormi (anche sopra le auto) e fiamme blu dalla bocca.
// In aria i colpi bassi non lo prendono: aspetta che atterri, o salta anche tu
void Game::UpdateSpringJack(Enemy& e, float dt) {
    if (UpdateLeap(e, dt, 2.2f, 0, 0)) return;
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    e.specialCd -= dt;
    if (e.windup > 0) {
        e.windup -= dt;
        Face(e, to, dt);
        if (e.windup <= 0 && e.attack == EnemyAttack::Breath) {
            float a = atan2f(to.y, to.x);
            for (int k = -2; k <= 2; ++k) EnemyShoot(e, Dir(a + k * 0.17f), 270, 5, e.damage * 0.7f, EnemyShot::BlueFire);
            e.attack = EnemyAttack::None;
            e.attackTimer = 2.2f;
        }
        return;
    }
    if (!e.aggro) { UpdateCalm(e, dt); return; }
    if (e.seesPlayer && dist < 230 && e.attackTimer <= 0) {
        e.attack = EnemyAttack::Breath;
        e.windup = 0.4f;
        return;
    }
    if (e.specialCd <= 0) {
        Vector2 spot = Vector2Add(player.pos, Vector2Scale(Dir(Rand01() * 2 * PI), 70 + Rand01() * 70));
        StartLeap(e, spot, 0.9f);
        e.specialCd = 1.6f + Rand01();
        if (Rand01() < 0.3f) AddText(e.pos, "Ah ah ah!", { 120, 170, 255, 255 });
        return;
    }
    Chase(e, dt, 0.7f);
}

// Berretto Rosso: goblin feroce. Ogni volta che ti ferisce si rafforza (vedi HurtByLegend)
void Game::UpdateRedcap(Enemy& e, float dt) {
    Chase(e, dt, 1 + 0.08f * e.stacks);
}

// ---------------------------------------------------------------------------
// Spagna
// ---------------------------------------------------------------------------

// El Cuco: l'ombra che si traveste. Fa la guardia al torii fingendosi un sacco abbandonato.
// Ti sente: se stai fermo e non attacchi, perde le tue tracce (nascondersi funziona!)
void Game::UpdateCuco(Enemy& e, float dt) {
    Vector2 to = Vector2Subtract(player.pos, e.pos);
    float dist = Vector2Length(to);
    if (e.hidden) {
        if (dist < 100 || e.aggro) {
            e.hidden = false;
            e.aggro = true;
            e.interest = 1;
            e.lastSeen = player.pos;
            fear = std::min(1.0f, fear + 0.3f);
            AddText(e.pos, "EL CUCO!", { 200, 120, 255, 255 });
        }
        return;
    }
    e.alpha = dist > 220 ? 0.3f : 1.0f;
    e.specialCd -= dt;
    bool hiding = quietTime > 1.2f && dist > 110;
    if (!hiding && e.aggro) e.lastSeen = player.pos; // ti sente muovere
    if (hiding && e.aggro) e.interest -= dt * 0.5f;
    // Mutaforma: ogni tanto riappare dove ti ha sentito l'ultima volta, alle tue spalle
    if (e.aggro && !hiding && e.specialCd <= 0 && dist > 170) {
        Vector2 spot = nav.NearestWalkable(Vector2Subtract(player.pos, Vector2Scale(player.aim, 90)));
        if (nav.WalkableAt(spot)) {
            AddText(e.pos, "...", { 160, 120, 200, 255 });
            e.pos = spot;
            e.path.clear();
        }
        e.specialCd = 8;
    }
    Chase(e, dt);
}

// Dip: il cane nero e zoppo del diavolo. Arranca, ma il suo morso dissangua
void Game::UpdateDip(Enemy& e, float dt) {
    float limp = 0.6f + 0.4f * fabsf(sinf((float)GetTime() * 3 + e.id));
    Chase(e, dt, limp);
}

// Santa Compana: una processione di anime incappucciate con le candele. Non si combatte: si evita.
// Chi ci finisce in mezzo prende una maledizione che lo prosciuga
void Game::UpdateCompana(Enemy& e, float dt) {
    if (e.swarm < 0) return;
    Swarm& s = swarms[e.swarm];
    float dist = Vector2Distance(e.pos, player.pos);
    if (dist < e.radius + player.radius + 4 && !HasCurse(CurseType::Compana)) {
        AddCurse(CurseType::Compana, 25);
        fear = std::min(1.0f, fear + 0.4f);
        AddPlayerText("La Santa Compana ti ha preso con se'!", { 220, 220, 255, 255 });
    }
    // Chi e' davanti nella fila? (slotAngle e' il posto nella fila)
    const Enemy* ahead = nullptr;
    for (const auto& o : enemies)
        if (o.alive && o.swarm == e.swarm && (int)o.slotAngle == (int)e.slotAngle - 1) ahead = &o;
    if (ahead) {
        if (Vector2Distance(e.pos, ahead->pos) > 30) Steer(e, DirToPoint(e, ahead->pos, dt), e.speed * 1.15f, dt);
        return;
    }
    // In testa: va di stanza in stanza
    const auto& rooms = dungeon.Rooms();
    Vector2 goal = rooms[std::clamp(s.patrolRoom, 0, (int)rooms.size() - 1)].Center();
    if (Vector2Distance(e.pos, goal) < 24) {
        int next = GetRandomValue(1, (int)rooms.size() - 1);
        s.patrolRoom = next == s.patrolRoom ? (next % ((int)rooms.size() - 1)) + 1 : next;
        return;
    }
    Steer(e, DirToPoint(e, goal, dt), e.speed, dt);
}
