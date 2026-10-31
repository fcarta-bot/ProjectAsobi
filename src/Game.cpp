#include "Game.hpp"
#include <algorithm>
#include <cmath>
#include "raymath.h"
#include "rlgl.h"
#include "Cutscene.hpp"
#include "Sky.hpp"
#include "Ui.hpp"

static float Rand01() { return GetRandomValue(0, 10000) / 10000.0f; }

constexpr float EYE_HEIGHT = 0.6f;     // altezza degli occhi sopra i piedi, in caselle
constexpr float BASE_FOV = 75.0f;      // campo visivo verticale in gradi
constexpr float TURN_SPEED = 3.0f;     // radianti al secondo con stick o frecce
constexpr float MOUSE_TURN = 0.0025f;  // radianti per pixel di mouse
constexpr float TOUCH_TURN = 0.005f;   // radianti per pixel trascinato sullo schermo touch
constexpr float GRAVITY = 22.0f;       // caselle al secondo quadrato
constexpr float JUMP_SPEED = 7.0f;     // basta per saltare su un'auto (circa 1.1 caselle)
constexpr float PLAYER_SHOT_H = 0.45f; // altezza dei colpi sopra i piedi
constexpr float EXPLOSION_RADIUS = 56; // pixel
constexpr float SWING_TIME = 0.22f;

static Vector2 RandomPointInRoom(const Room& r) {
    return { GetRandomValue(r.x + 1, r.x + r.w - 2) * (float)TILE + TILE / 2.0f,
             GetRandomValue(r.y + 1, r.y + r.h - 2) * (float)TILE + TILE / 2.0f };
}

// Disegna le "etichette" colorate dei tag; restituisce la x finale
static int DrawTagChips(unsigned tags, int x, int y) {
    for (int i = 0; i < TAG_COUNT; ++i) {
        if (!(tags & (1u << i))) continue;
        int w = MeasureText(TagName(i), 14) + 10;
        DrawRectangleRounded({ (float)x, (float)y, (float)w, 18 }, 0.5f, 6, TagColor(i));
        DrawText(TagName(i), x + 5, y + 2, 14, BLACK);
        x += w + 6;
    }
    return x;
}

static void DrawArtifactCard(const Artifact& a, Rectangle r, Color border) {
    DrawRectangleRec(r, { 20, 18, 26, 235 });
    DrawRectangleLinesEx(r, 2, border);
    DrawText(a.name, (int)r.x + 12, (int)r.y + 8, 22, WHITE);
    DrawText(TextFormat("Tributo: %s", a.origin), (int)r.x + 12, (int)r.y + 32, 14, GRAY);
    DrawText(a.desc, (int)r.x + 12, (int)r.y + 50, 16, LIGHTGRAY);
    DrawTagChips(a.tags, (int)r.x + 12, (int)r.y + (int)r.height - 26);
}

static void DrawWeaponCard(const Weapon& w, Rectangle r, const char* footer) {
    DrawRectangleRec(r, { 20, 18, 26, 235 });
    DrawRectangleLinesEx(r, 2, w.color);
    DrawText(TextFormat("Arma: %s", w.name), (int)r.x + 12, (int)r.y + 8, 22, WHITE);
    DrawText(TextFormat("Tributo: %s", w.origin), (int)r.x + 12, (int)r.y + 32, 14, GRAY);
    DrawText(w.desc, (int)r.x + 12, (int)r.y + 50, 14, LIGHTGRAY);
    if (*footer) DrawText(footer, (int)r.x + 12, (int)r.y + (int)r.height - 24, 14, ORANGE);
}

// ---------------------------------------------------------------------------
// Gestione della partita
// ---------------------------------------------------------------------------

Game::Game() {
    camera.up = { 0, 1, 0 };
    camera.fovy = BASE_FOV;
    camera.projection = CAMERA_PERSPECTIVE;
    link = ControllerLink::Create();
    LoadProfile();
    // All'avvio: il menu iniziale, col GameMaster che scende sulla terra e si apposta alla finestra
    NewRun();
    OpenTitle();
}

void Game::UpdateIntro(float dt) {
    cutsceneTime += dt;
    const float end = introNewRun ? Intro::END : Intro::LOAD_END;
    if (input.BackPressed() || input.GameMenuPressed()) cutsceneTime = end;                 // salta tutto
    else if (input.ConfirmPressed()) cutsceneTime = std::min(end, Intro::NextStop(cutsceneTime)); // avanti
    if (cutsceneTime >= end) {
        state = State::Playing;
        introTimer = 3.5f; // il nome del distretto compare ora, non sotto l'intro
        floorTime = lostTimer = 0;
        fadeFromBlack = 1.0f;
    }
}

void Game::NewRun() {
    floor = 0;
    kills = 0;
    runTime = 0;
    std::fill(std::begin(killsByType), std::end(killsByType), 0);
    damageDealt = damageTaken = 0;
    artifactsFound = 0;
    inventory.clear();
    stats = ComputeStats(inventory);
    player = Player{};
    player.hp = stats.maxHp;
    curses.clear();
    fear = 0;
    tourSeed = (unsigned)GetRandomValue(0, 1 << 30);
    tour = MakeWorldTour(tourSeed, MAX_FLOORS);
    ResetChallenges();
    PickVictoryChallenges();
    state = State::Playing;
    NextFloor();
}

void Game::NextFloor() {
    if (floor > 0) {
        OnFloorLeft();
        player.hp = std::min(stats.maxHp, player.hp + stats.maxHp * 0.25f); // un po' di riposo tra un piano e l'altro
    }
    floor++;
    BuildFloor((unsigned)GetRandomValue(0, 1000000));
    SaveRun(); // salvataggio automatico all'inizio di ogni piano
}

void Game::BuildFloor(unsigned seed) {
    floorSeed = seed;
    dungeon.Generate(64, 48, seed);
    enemies.clear();
    swarms.clear();
    corpses.clear();
    projectiles.clear();
    pickups.clear();
    texts.clear();
    explosions.clear();
    floorDamageStart = damageTaken;
    districtsSeen |= 1u << (int)CurrentDistrict().id;
    countriesSeen |= 1u << (int)CurrentDistrict().country;
    if (runActive) profile.bestFloor = std::max(profile.bestFloor, floor); // la run "di sfondo" del menu non conta

    player.pos = dungeon.Rooms()[0].Center();
    player.z = player.vz = 0;
    player.onGround = true;
    player.yaw = 0; // si parte guardando gli altari accanto alla partenza
    player.aim = { 1, 0 };
    props.Generate(dungeon, CurrentDistrict(), player.pos);
    nav.Build(dungeon, props);
    prevPlayerPos = player.pos;
    playerVel = { 0, 0 };
    introTimer = 3.5f;
    explored.assign(dungeon.Width() * dungeon.Height(), 0); // piano nuovo: mappa da scoprire
    exploreTimer = 0;
    floorTime = lostTimer = 0;
    guideActive = false;
    SpawnFloorContent();
    SyncCamera();
}

// La telecamera sta negli occhi del giocatore e guarda dove guarda lui
void Game::SyncCamera() {
    float bob = player.onGround ? sinf(walkCycle) * 0.03f : 0.0f;
    camera.position = To3D(player.pos, EYE_HEIGHT + player.z + bob);
    camera.target = Vector3Add(camera.position, { player.aim.x, 0, player.aim.y });
}

// In partita il mouse sparisce e gira la visuale; nei menu torna il puntatore normale
void Game::UpdateCursorLock() {
    bool want = state == State::Playing && IsWindowFocused() && !input.UsingTouch(); // col touch non serve
    if (want == cursorLocked) return;
    if (want) DisableCursor();
    else EnableCursor();
    cursorLocked = want;
    skipMouseDelta = true; // il primo spostamento dopo la cattura e' un salto finto
}

// Un punto della stanza dove non c'e' gia' un'auto, un albero...
Vector2 Game::FreeSpotInRoom(const Room& r, float radius) const {
    Vector2 p = r.Center();
    for (int tries = 0; tries < 30; ++tries) {
        p = RandomPointInRoom(r);
        if (!props.Overlaps(p, radius + 2)) break;
    }
    return p;
}

void Game::SpawnFloorContent() {
    const auto& rooms = dungeon.Rooms();

    // I nemici li sceglie e li piazza il GameMaster
    gm.PopulateFloor(*this);

    // Altare con artefatto accanto alla partenza; al primo piano anche un'arma da provare
    pickups.push_back({ Vector2Add(rooms[0].Center(), { TILE * 2.0f, -TILE * 0.8f }), RandomArtifact() });
    if (floor == 1) pickups.push_back({ Vector2Add(rooms[0].Center(), { TILE * 2.0f, TILE * 0.8f }), -1, RandomWeapon() });
    for (int k = 0; k < 2; ++k) {
        const Room& r = rooms[GetRandomValue(1, (int)rooms.size() - 1)];
        pickups.push_back({ FreeSpotInRoom(r, 12), RandomArtifact() });
    }
    // Un'arma nascosta in una stanza a caso
    const Room& r = rooms[GetRandomValue(1, (int)rooms.size() - 1)];
    pickups.push_back({ FreeSpotInRoom(r, 12), -1, RandomWeapon() });

    // Contro la Bestia del Gevaudan serve l'argento: se non ce l'hai, il GM te ne lascia uno in giro
    bool bestia = std::any_of(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.type == EnemyType::Gevaudan; });
    bool silver = false;
    for (int w : player.weapons) silver |= w >= 0 && AllWeapons()[w].silver;
    if (bestia && !silver) {
        const Room& sr = rooms[GetRandomValue(1, (int)rooms.size() - 1)];
        pickups.push_back({ FreeSpotInRoom(sr, 12), -1, W_CHASTEL });
    }
}

// Il riferimento restituito vale solo finche' non si aggiunge un altro nemico
Enemy& Game::SpawnEnemy(EnemyType type, Vector2 pos) {
    Enemy e;
    e.id = nextEnemyId++;
    e.type = type;
    e.pos = e.home = e.lastPos = pos;
    float a = Rand01() * 2 * PI;
    e.facing = { cosf(a), sinf(a) };
    const EnemyInfo& info = InfoOf(type);
    e.radius = info.radius;
    e.maxHp = info.hp;
    e.speed = info.speed;
    e.damage = info.damage;
    if (type == EnemyType::Mothman) e.z = 1.2f; // vola alto
    // I piani successivi sono piu' difficili
    e.maxHp *= 1.0f + 0.3f * (floor - 1);
    e.damage *= 1.0f + 0.15f * (floor - 1);
    e.hp = e.maxHp;
    e.baseRadius = e.radius;
    if (type == EnemyType::Babau) { e.power = e.damage; e.hidden = true; } // power = danno di base, cresce con la paura
    e.shootTimer = 1.0f + Rand01();
    e.thinkTimer = Rand01() * 0.2f; // non pensano tutti nello stesso frame
    e.strafeSign = GetRandomValue(0, 1) ? 1.0f : -1.0f;
    enemies.push_back(e);
    return enemies.back();
}

// Preferisce artefatti che il giocatore non ha e che non sono gia' a terra
int Game::RandomArtifact() const {
    int id = 0;
    for (int tries = 0; tries < 10; ++tries) {
        id = GetRandomValue(0, A_FIRST_RARE - 1); // i rari solo come premio delle sfide
        bool owned = std::find(inventory.begin(), inventory.end(), id) != inventory.end();
        bool onGround = std::any_of(pickups.begin(), pickups.end(),
                                    [id](const Pickup& p) { return p.artifactId == id && p.weaponId < 0 && !p.taken; });
        if (!owned && !onGround) break;
    }
    return id;
}

// Un'arma diversa da quelle che hai e da quelle gia' a terra (gli ofuda li hai gia' dall'inizio).
// Di solito e' un'arma tipica del paese in cui ti trovi; le leggendarie solo come premio delle sfide
int Game::RandomWeapon() const {
    int id = W_KATANA;
    const Country here = CurrentDistrict().country;
    for (int tries = 0; tries < 16; ++tries) {
        id = GetRandomValue(W_KATANA, W_FIRST_LEGENDARY - 1);
        if (tries < 10 && AllWeapons()[id].country != here && GetRandomValue(0, 99) < 70) continue;
        bool owned = player.weapons[0] == id || player.weapons[1] == id;
        bool onGround = std::any_of(pickups.begin(), pickups.end(),
                                    [id](const Pickup& p) { return p.weaponId == id && !p.taken; });
        if (!owned && !onGround) break;
    }
    return id;
}

void Game::RecomputeStats() {
    float oldMax = stats.maxHp;
    stats = ComputeStats(inventory);
    bestSynergies = std::max(bestSynergies, (int)ActiveSynergies(inventory).size());
    if (stats.maxHp > oldMax) player.hp += stats.maxHp - oldMax;
    player.hp = std::min(player.hp, stats.maxHp);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::Update(float dt) {
    dt = std::min(dt, 0.05f); // evita salti enormi se il gioco si blocca un attimo
    // Quali tasti a schermo mostrare (touch): in partita, nelle animazioni o nei menu
    input.SetTouchLayout(state == State::Playing ? TouchControls::Layout::Game
                         : state == State::Intro || state == State::GameOver || state == State::Victory ||
                           (state == State::Title && titleTime < TitleScene::MENU) ? TouchControls::Layout::Tap
                                                                            : TouchControls::Layout::Menu);
    input.Update();
    link->Update();
    toastTimer -= dt;
    bannerTimer -= dt;
    hurtTimer -= dt;
    swingTimer -= dt;
    recoil = std::max(0.0f, recoil - dt * 6);
    fadeFromBlack -= dt;
    // Schermo intero: funziona ovunque, anche nei menu e nelle animazioni.
    // Si esce subito, cosi' l'Invio di Alt+Invio non conferma anche una voce di menu
    if (input.FullscreenPressed()) {
        ToggleFullscreenMode();
        return;
    }
    UpdateCursorLock();

    // Collegamento al volo: START, A o il touchpad su un joystick non ancora collegato.
    // Se era un tasto menu, si apre anche il menu: e' quello che ci si aspetta premendolo
    int request = input.NewPadRequest();
    if (request >= 0) {
        input.Connect(request);
        ShowToast(TextFormat("Joystick collegato: %s", GetGamepadName(request)));
        if (state == State::Playing && input.MatchMenuPressed()) OpenMatchMenu();
        else if (state == State::Playing && input.GameMenuPressed()) OpenPause();
        return;
    }
    if (input.LostPad()) {
        OpenControllerMenu("Il joystick si e' scollegato: ricollegalo o scegli un altro controllo");
        return;
    }
    if (state == State::ControllerMenu) {
        UpdateControllerMenu();
        return;
    }
    if (state == State::Title) {
        UpdateTitle(dt);
        return;
    }
    if (state == State::Victory) {
        UpdateVictory(dt);
        return;
    }
    if (state == State::Intro) {
        UpdateIntro(dt);
        return;
    }
    if (state == State::Paused) {
        UpdatePause();
        return;
    }
    if (state == State::MatchMenu) {
        UpdateMatchMenu();
        return;
    }
    if (input.MenuPressed()) {
        OpenControllerMenu("");
        return;
    }
    if (input.GameMenuPressed() && (state == State::Playing || state == State::GameOver)) {
        OpenPause();
        return;
    }
    if (input.MatchMenuPressed() && state == State::Playing) {
        OpenMatchMenu();
        return;
    }
    if (state == State::GameOver) {
        // Prima l'animazione del risveglio (si puo' saltare), poi si ricomincia a sognare
        deathTime += dt;
        if (deathTime < WakeUp::END) {
            if (input.ConfirmPressed() || input.BackPressed()) deathTime = WakeUp::END;
        } else if (IsKeyPressed(KEY_R)) { // subito un'altra run, nello stesso salvataggio
            NewRun();
            fadeFromBlack = 1.0f;
        } else if (input.ConfirmPressed()) {
            OpenTitle();
        }
        return;
    }
    if (state == State::InventoryFull) {
        UpdateInventoryChoice();
        return;
    }

    // Rimuove gli oggetti "morti" del frame precedente
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive; }), enemies.end());
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.alive; }), projectiles.end());
    pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [](const Pickup& p) { return p.taken; }), pickups.end());
    texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatingText& t) { return t.life <= 0; }), texts.end());
    explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& x) { return x.t > 0.8f; }), explosions.end());

    introTimer -= dt;
    runTime += dt;
    props.Update(dt);
    UpdatePlayer(dt);
    UpdateExploration(dt);
    UpdateGuide(dt);
    if (state != State::Playing) return;
    UpdateEnemies(dt);
    UpdateProjectiles(dt);
    UpdatePickups();
    UpdateFear(dt);
    UpdateCurses(dt);
    for (auto& c : corpses) c.life -= dt;
    corpses.erase(std::remove_if(corpses.begin(), corpses.end(), [](const Corpse& c) { return c.life <= 0; }), corpses.end());
    if (state != State::Playing) return;
    UpdateChallenges(dt);
    if (profile.tips) UpdateTutorials(dt);
    for (auto& t : texts) {
        t.life -= dt;
        t.height += 0.6f * dt;
    }
    for (auto& x : explosions) x.t += dt;
}

void Game::UpdatePlayer(float dt) {
    // Girarsi: mouse (catturato) oppure stick destro / frecce
    float turn = input.TurnAxis() * TURN_SPEED * dt;
    if (cursorLocked && !skipMouseDelta) turn += GetMouseDelta().x * MOUSE_TURN * profile.lookSensitivity;
    turn += input.TouchLook() * TOUCH_TURN * profile.lookSensitivity;
    skipMouseDelta = false;
    player.yaw = fmodf(player.yaw + turn, 2 * PI);
    player.aim = { cosf(player.yaw), sinf(player.yaw) };

    // Il movimento e' relativo allo sguardo: avanti, indietro e passi laterali
    const Vector2 right{ -player.aim.y, player.aim.x };
    auto toWorld = [&](Vector2 local) {
        return Vector2Add(Vector2Scale(player.aim, -local.y), Vector2Scale(right, local.x));
    };
    Vector2 dir = toWorld(input.Move());
    if (Vector2Length(dir) > 0) usedMove = true;   // per i tutorial: cosa ha gia' imparato
    if (fabsf(turn) > 0.0005f) usedTurn = true;

    player.shootTimer -= dt;
    player.dashCdTimer -= dt;
    player.invuln -= dt;

    // Scatto: breve, velocissimo e rende invulnerabili. Col joystick va nella direzione della freccia
    bool moving = Vector2Length(dir) > 0;
    Vector2 dashLocal;
    if (input.DashPressed(dashLocal) && player.dashCdTimer <= 0) {
        Vector2 d = Vector2Length(dashLocal) > 0 ? toWorld(dashLocal) : moving ? dir : player.aim;
        player.dashTimer = 0.16f;
        player.dashDir = Vector2Normalize(d);
        player.dashCdTimer = stats.dashCooldown;
        player.invuln = std::max(player.invuln, 0.22f);
        usedDash = true;
    }

    // Salto: serve a scavalcare tronchi e cestini, a salire sulle auto e a schivare i colpi
    if (input.JumpPressed() && player.onGround) {
        player.vz = JUMP_SPEED;
        player.onGround = false;
        usedJump = true;
    }

    Vector2 vel;
    if (player.dashTimer > 0) {
        player.dashTimer -= dt;
        vel = Vector2Scale(player.dashDir, stats.moveSpeed * 3.2f);
    } else {
        vel = Vector2Scale(dir, stats.moveSpeed);
    }
    Vector2 before = player.pos;
    MoveWithCollision(player.pos, player.radius, Vector2Scale(vel, dt), player.z);

    // Spinge contro qualcosa di basso senza saltare? (serve al tutorial del salto)
    Vector2 ahead = Vector2Add(player.pos, Vector2Scale(dir, player.radius + 6));
    bool stuckLow = moving && player.onGround && Vector2Distance(before, player.pos) < stats.moveSpeed * dt * 0.3f &&
                    props.BlocksCircle(ahead, 6, player.z) && !props.BlocksCircle(ahead, 6, player.z + 1.0f);
    lowObstacleTime = stuckLow ? lowObstacleTime + dt : std::max(0.0f, lowObstacleTime - dt);

    // Gravita': si atterra sul suolo o sopra l'oggetto che si ha sotto i piedi
    float ground = props.GroundHeight(player.pos, player.radius, player.z);
    if (player.z > ground + 0.001f || player.vz > 0) {
        player.vz -= GRAVITY * dt;
        player.z += player.vz * dt;
        player.onGround = false;
        if (player.z <= ground) {
            player.z = ground;
            player.vz = 0;
            player.onGround = true;
        }
    } else {
        player.z = ground;
        player.onGround = true;
    }
    if (moving && player.onGround) walkCycle += dt * 9 * Vector2Length(dir);

    // Lo scatto allarga un attimo il campo visivo, per dare la sensazione di velocita'
    float fovTarget = player.dashTimer > 0 ? BASE_FOV + 10 : BASE_FOV;
    camera.fovy += (fovTarget - camera.fovy) * std::min(1.0f, 12 * dt);

    // Cambio arma (se se ne hanno due)
    int other = 1 - player.activeSlot;
    if (input.SwapWeaponPressed() && player.weapons[other] >= 0) {
        player.activeSlot = other;
        player.shootTimer = std::max(player.shootTimer, 0.2f);
        usedSwap = true;
        AddPlayerText(AllWeapons()[player.Weapon()].name, WHITE);
    }

    // Fermo e zitto? Per el Cuco sei invisibile (ti trova solo dal rumore)
    bool noisy = moving || player.dashTimer > 0 || input.FireDown() || !player.onGround;
    quietTime = noisy ? 0 : quietTime + dt;

    if (input.FireDown() && player.shootTimer <= 0) Attack();

    // E / Quadrato: prendi l'arma o l'artefatto vicino, oppure attraversa il torii
    if (input.InteractPressed()) {
        int weapon = NearestPickup(48, true);
        int artifact = NearestPickup(48, false);
        if (weapon >= 0) TakeWeapon(weapon);
        else if (artifact >= 0) TryPickup(artifact);
        else if (Vector2Distance(player.pos, dungeon.ExitPos()) < 48) NextFloor();
    }

    // Il fuoco scotta: meglio non passarci attraverso
    fireTick -= dt;
    if (fireTick <= 0 && props.NearFire(player.pos, player.radius, player.z)) {
        fireTick = 0.5f;
        DamagePlayer(6);
    }
    SyncCamera();
}

void Game::Attack() {
    const Weapon& w = AllWeapons()[player.Weapon()];
    player.shootTimer = 1.0f / (stats.fireRate * w.rateMult);
    recoil = 1.0f;
    usedAttack = true;
    threatTime = 0;
    if (w.melee) {
        MeleeSwing(w);
        if (w.shot != ShotLook::Wave) return;
        // La Kusanagi lancia anche un'onda di vento che passa attraverso tutti i nemici
        Projectile p;
        p.pos = Vector2Add(player.pos, Vector2Scale(player.aim, player.radius));
        p.vel = Vector2Scale(player.aim, 520);
        p.height = player.z + PLAYER_SHOT_H;
        p.damage = stats.damage * w.damageMult * 0.6f;
        p.radius = 12;
        p.life = 0.7f;
        p.pierceLeft = 99;
        p.melee = true;
        p.weapon = player.Weapon();
        projectiles.push_back(p);
        return;
    }
    // Si spara dove si guarda (il mirino e' al centro dello schermo)
    const int shots = stats.projectiles + w.pellets;
    for (int i = 0; i < shots; ++i) {
        float a = player.yaw + (i - (shots - 1) / 2.0f) * w.spread + (Rand01() * 2 - 1) * w.jitter;
        Projectile p;
        p.pos = Vector2Add(player.pos, Vector2Scale(player.aim, player.radius * 0.5f));
        p.vel = { cosf(a) * stats.projSpeed * w.speedMult, sinf(a) * stats.projSpeed * w.speedMult };
        p.height = player.z + PLAYER_SHOT_H;
        p.crit = Rand01() < stats.critChance;
        p.damage = stats.damage * w.damageMult * (p.crit ? stats.critMult : 1.0f);
        p.radius = w.radius * (p.crit ? 1.3f : 1.0f);
        p.life = w.life;
        p.pierceLeft = stats.pierce + w.extraPierce;
        p.bouncesLeft = w.bounces;
        p.explosive = w.explosive;
        p.incendiary = w.incendiary;
        p.silver = w.silver;
        p.weapon = player.Weapon();
        projectiles.push_back(p);
    }
}

// Fendente: colpisce subito tutti i nemici davanti a portata e respinge i colpi nemici
void Game::MeleeSwing(const Weapon& w) {
    swingTimer = SWING_TIME;
    float arc = w.meleeArc + 0.12f * (stats.projectiles - 1); // i proiettili in piu' allargano il fendente
    float cosArc = cosf(arc);
    auto inFront = [&](Vector2 pos, float extra) {
        Vector2 to = Vector2Subtract(pos, player.pos);
        float len = Vector2Length(to);
        if (len - extra > w.meleeRange) return false;
        return len < player.radius + extra || Vector2DotProduct(to, player.aim) / len >= cosArc;
    };

    const int id = player.Weapon();
    int hits = 0;
    for (auto& e : enemies) {
        if (!e.alive || !Damageable(e) || !inFront(e.pos, e.radius)) continue;
        if (EnemyBodyY(e) - player.z > 1.4f) continue;                         // vola troppo in alto
        if (id != W_BALMUNG && !dungeon.HasLineOfSight(player.pos, e.pos)) continue; // Balmung taglia anche i muri
        bool crit = Rand01() < stats.critChance;
        DamageEnemy(e, stats.damage * w.damageMult * (crit ? stats.critMult : 1.0f), crit, w.silver, false, true);
        Vector2 push = Vector2Scale(Vector2Normalize(Vector2Subtract(e.pos, player.pos)), w.knockback);
        MoveWithCollision(e.pos, e.radius, push);
        if (w.stun > 0 && !InfoOf(e.type).boss) e.stun = std::max(e.stun, w.stun);
        else if (w.stun > 0) e.stun = std::max(e.stun, w.stun * 0.3f); // i guardiani resistono
        hits++;
    }
    if (id == W_EXCALIBUR && hits > 0) player.hp = std::min(stats.maxHp, player.hp + 2.0f * hits);
    if (id == W_JOHN_HENRY) // il martello fa tremare la terra davanti a te
        Explode(Vector2Add(player.pos, Vector2Scale(player.aim, 50)), 0.1f, stats.damage * w.damageMult * 0.5f, false);
    int parried = 0;
    for (auto& p : projectiles) {
        if (!p.alive || p.fromPlayer || !inFront(p.pos, p.radius)) continue;
        ++parried;
        if (w.reflect) { // la mazza da cricket (e Durlindana) rimandano il colpo da dove e' venuto
            p.fromPlayer = true;
            p.vel = Vector2Scale(p.vel, -1.3f);
            p.damage *= 2;
            p.life = 2;
            p.weapon = W_PACHINKO;
        } else {
            p.alive = false;
        }
    }
    if (parried) AddPlayerText(w.reflect ? "Rispedito!" : "Parato!", SKYBLUE);
}

// Esplosione: danno ad area. Quella dei fuochi incendia cio' che puo' bruciare (solo quello),
// quella della schiuma (birra, champagne) no
void Game::Explode(Vector2 pos, float height, float damage, bool incendiary) {
    explosions.push_back({ To3D(pos, height), 0, (unsigned)GetRandomValue(0, 1 << 30), !incendiary });
    for (auto& e : enemies) {
        if (e.alive && Damageable(e) && Vector2Distance(e.pos, pos) < EXPLOSION_RADIUS + e.radius)
            DamageEnemy(e, damage * 0.8f, false, false, incendiary);
    }
    if (incendiary && props.Ignite(pos, 40) > 0) AddText(pos, "Prende fuoco!", ORANGE);
}

void Game::UpdateProjectiles(float dt) {
    auto solid = [&](Vector2 p, float h) { return dungeon.IsSolidAt(p) || props.BlocksShot(p, h); };
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        p.life -= dt;
        Vector2 next = Vector2Add(p.pos, Vector2Scale(p.vel, dt));

        if (solid(next, p.height)) {
            if (p.bouncesLeft > 0) {
                // Rimbalzo: si inverte la componente che ha urtato
                bool hitX = solid({ next.x, p.pos.y }, p.height), hitY = solid({ p.pos.x, next.y }, p.height);
                if (hitX) p.vel.x = -p.vel.x;
                if (hitY) p.vel.y = -p.vel.y;
                if (!hitX && !hitY) p.vel = Vector2Negate(p.vel);
                p.bouncesLeft--;
                continue;
            }
            if (p.explosive) Explode(p.pos, p.height, p.damage, p.incendiary);
            p.alive = false;
            continue;
        }
        p.pos = next;
        if (p.life <= 0) {
            if (p.explosive) Explode(p.pos, p.height, p.damage, p.incendiary);
            p.alive = false;
            continue;
        }

        if (p.fromPlayer) {
            for (auto& e : enemies) {
                if (!e.alive || !Damageable(e)) continue;
                if (std::find(p.hitIds.begin(), p.hitIds.end(), e.id) != p.hitIds.end()) continue;
                if (!CheckCollisionCircles(p.pos, p.radius, e.pos, e.radius)) continue;
                // Chi vola o salta alto non lo prendi con un colpo basso (salta anche tu!)
                if (fabsf(p.height - EnemyBodyY(e)) > e.radius / TILE + 0.55f) continue;
                if (p.explosive) {
                    DamageEnemy(e, p.damage, p.crit, p.silver, p.incendiary);
                    Explode(p.pos, p.height, p.damage, p.incendiary);
                    p.alive = false;
                    break;
                }
                DamageEnemy(e, p.damage, p.crit, p.silver, false, p.melee);
                p.hitIds.push_back(e.id);
                if (--p.pierceLeft < 0) { p.alive = false; break; }
            }
        } else if (CheckCollisionCircles(p.pos, p.radius, player.pos, player.radius) &&
                   fabsf(p.height - (player.z + 0.4f)) < 0.45f) { // saltando lo si schiva
            DamagePlayer(p.damage);
            p.alive = false;
        }
    }
}

void Game::UpdatePickups() {
    for (auto& p : pickups) {
        if (p.taken || p.artifactId >= 0 || p.weaponId >= 0) continue;
        if (Vector2Distance(p.pos, player.pos) < player.radius + 10 && player.z < 0.6f) {
            player.hp = std::min(stats.maxHp, player.hp + 20);
            fear = std::max(0.0f, fear - 0.15f); // un po' di coraggio
            AddPlayerText("+20", GREEN);
            p.taken = true;
        }
    }
}

// weapons = true: solo armi, false: solo artefatti
int Game::NearestPickup(float maxDist, bool weapons) const {
    int best = -1;
    float bestDist = maxDist;
    for (int i = 0; i < (int)pickups.size(); ++i) {
        const Pickup& p = pickups[i];
        if (p.taken) continue;
        if (weapons ? p.weaponId < 0 : (p.weaponId >= 0 || p.artifactId < 0)) continue;
        float d = Vector2Distance(p.pos, player.pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void Game::TryPickup(int index) {
    Pickup& p = pickups[index];
    usedPickup = true;
    if ((int)inventory.size() < INVENTORY_SIZE) {
        inventory.push_back(p.artifactId);
        p.taken = true;
        artifactsFound++;
        RecomputeStats();
        AddPlayerText(std::string("Ottenuto: ") + AllArtifacts()[p.artifactId].name, GOLD);
    } else {
        // Inventario pieno: il gioco si mette in pausa e chiede cosa scartare
        pendingPickup = index;
        choiceIndex = 0;
        state = State::InventoryFull;
    }
}

// Si portano due armi: se le mani sono piene, quella in uso resta a terra al posto della nuova
void Game::TakeWeapon(int index) {
    Pickup& p = pickups[index];
    int id = p.weaponId;
    usedPickup = true;
    if (player.weapons[1] < 0) {
        player.weapons[1] = id;
        player.activeSlot = 1;
        p.taken = true;
    } else {
        int old = player.Weapon();
        player.weapons[player.activeSlot] = id;
        p.weaponId = old;
        AddPlayerText(std::string("Lasciato: ") + AllWeapons()[old].name, GRAY);
    }
    player.shootTimer = std::max(player.shootTimer, 0.3f);
    AddPlayerText(std::string("Arma: ") + AllWeapons()[id].name, GOLD);
    OnWeaponTaken(id);
}

// Righe: 0..inventario-1 = scarta quell'artefatto, ultima = lascia a terra il nuovo
void Game::UpdateInventoryChoice() {
    int rows = (int)inventory.size() + 1;
    if (input.UpPressed()) choiceIndex = (choiceIndex + rows - 1) % rows;
    if (input.DownPressed()) choiceIndex = (choiceIndex + 1) % rows;

    for (int k = 0; k < rows - 1; ++k) {
        if (IsKeyPressed(KEY_ONE + k)) { ResolveInventoryChoice(k); return; }
    }
    if (IsKeyPressed(KEY_SIX) || input.BackPressed()) { ResolveInventoryChoice(rows - 1); return; }
    if (input.ConfirmPressed()) ResolveInventoryChoice(choiceIndex);
}

void Game::ResolveInventoryChoice(int row) {
    if (row < (int)inventory.size()) {
        AddPlayerText(std::string("Scartato: ") + AllArtifacts()[inventory[row]].name, GRAY);
        inventory[row] = pickups[pendingPickup].artifactId; // l'artefatto scartato e' perso per sempre
        pickups[pendingPickup].taken = true;
        artifactsFound++;
        RecomputeStats();
    }
    pendingPickup = -1;
    state = State::Playing;
}

void Game::ShowToast(const std::string& text) {
    toast = text;
    toastTimer = 3.0f;
}

void Game::DamageEnemy(Enemy& e, float dmg, bool crit, bool silver, bool fire, bool melee) {
    if (!e.alive || !Damageable(e)) return;
    // Chi era nascosto, neutrale o travestito si arrabbia (o salta fuori)
    e.hostile = true;
    if (e.type == EnemyType::BlackShuck) e.special = 1;
    if (e.hidden && e.type != EnemyType::Babau) e.hidden = false;
    if (e.type == EnemyType::Badalisc && !e.aggro) AddText(e.pos, "Il Badalisc si offende!", ORANGE);
    // La Bestia del Gevaudan: solo l'argento e il fuoco la feriscono
    if (e.type == EnemyType::Gevaudan && !silver && !fire) {
        e.aggro = true;
        e.lastSeen = player.pos;
        if (stats.burnDps > 0) { // i colpi infuocati invece la bruciano
            e.burnTimer = 3.0f;
            e.burnDps = std::max(e.burnDps, stats.burnDps);
        }
        if (e.hitFlash < -1) {
            AddText(e.pos, "Immune!", LIGHTGRAY);
            e.hitFlash = 0;
        }
        static bool hinted = false;
        if (!hinted) {
            ShowToast("Le armi normali non la scalfiscono: serve l'argento... o il fuoco");
            hinted = true;
        }
        return;
    }
    e.lastHitMelee = melee;
    damageDealt += std::min(dmg, std::max(0.0f, e.hp)); // conta solo il danno che toglie davvero vita
    e.hp -= dmg;
    e.hitFlash = 0.08f;
    // Chi viene colpito sa subito dove sei, anche se non ti aveva visto
    e.aggro = true;
    e.interest = 1;
    e.lastSeen = player.pos;
    if (stats.burnDps > 0) {
        e.burnTimer = 3.0f;
        e.burnDps = std::max(e.burnDps, stats.burnDps);
    }
    if (stats.lifesteal > 0) player.hp = std::min(stats.maxHp, player.hp + dmg * stats.lifesteal);
    AddText(e.pos, TextFormat(crit ? "%d!" : "%d", (int)dmg), crit ? YELLOW : WHITE);
    if (e.hp <= 0) KillEnemy(e);
}

void Game::KillEnemy(Enemy& e) {
    if (!e.alive) return;
    e.alive = false;
    kills++;
    killsByType[(int)e.type]++;
    corpses.push_back({ e.pos, 30 }); // il wendigo se ne nutre
    OnEnemyKilled(e);
    if (e.type == EnemyType::Matagot) {
        // La ricchezza del gatto-spirito... e il prezzo del patto di sangue
        pickups.push_back({ e.pos, RandomArtifact() });
        pickups.push_back({ Vector2Add(e.pos, { 14, 8 }), RandomArtifact() });
        DrainPlayer(std::min(15.0f, player.hp - 1));
        AddPlayerText("Patto di sangue: -15 vita", { 200, 30, 50, 255 });
        return;
    }
    if (InfoOf(e.type).boss) {
        // Il guardiano lascia sempre un artefatto, spesso un secondo artefatto e un'arma, e qualche cuore
        AddText(e.pos, std::string(InfoOf(e.type).name) + " sconfitto!", GOLD);
        pickups.push_back({ e.pos, RandomArtifact() });
        if (Rand01() < 0.25f + stats.dropChance) pickups.push_back({ Vector2Add(e.pos, { 16, 10 }), RandomArtifact() });
        if (Rand01() < 0.6f) pickups.push_back({ Vector2Add(e.pos, { -16, 10 }), -1, RandomWeapon() });
        pickups.push_back({ Vector2Add(e.pos, { -10, -16 }), -1 });
        pickups.push_back({ Vector2Add(e.pos, { 10, -16 }), -1 });
        return;
    }
    // I goblin sono tanti: ognuno lascia qualcosa piu' di rado
    float luck = e.type == EnemyType::Goblin ? 0.35f : 1.0f;
    if (Rand01() < stats.dropChance * luck) pickups.push_back({ e.pos, RandomArtifact() });
    else if (Rand01() < 0.05f * luck) pickups.push_back({ e.pos, -1, RandomWeapon() });
    else if (Rand01() < 0.12f * luck) pickups.push_back({ e.pos, -1 });
}

bool Game::DamagePlayer(float dmg) {
    if (player.invuln > 0 || state != State::Playing) return false;
    if (HasCurse(CurseType::Omen)) dmg *= 1.5f; // il presagio del Mothman
    damageTaken += std::min(dmg, player.hp);
    if (!usedDash) hitsWithoutDash++;
    player.hp -= dmg;
    player.invuln = 0.6f;
    hurtTimer = 0.4f;
    if (fearFloor) fear = std::min(1.0f, fear + 0.07f);
    AddPlayerText(TextFormat("-%d", (int)dmg), RED);
    if (player.hp <= 0) Die();
    return true;
}

void Game::DrainPlayer(float amount) {
    if (state != State::Playing || amount <= 0) return;
    damageTaken += std::min(amount, player.hp);
    player.hp -= amount;
    if (player.hp <= 0) Die();
}

// Si muore nel sogno: ci si sveglia. La run e' finita, il salvataggio si cancella
void Game::Die() {
    player.hp = 0;
    state = State::GameOver;
    deathTime = 0; // parte l'animazione del risveglio
    activeTip = -1;
    curses.clear();
    DeleteRun(slot);
    SaveProfile();
}

bool Game::CircleHitsWall(Vector2 p, float r) const {
    int x0 = (int)floorf((p.x - r) / TILE), x1 = (int)floorf((p.x + r) / TILE);
    int y0 = (int)floorf((p.y - r) / TILE), y1 = (int)floorf((p.y + r) / TILE);
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (dungeon.IsSolid(tx, ty) &&
                CheckCollisionCircleRec(p, r, { (float)tx * TILE, (float)ty * TILE, (float)TILE, (float)TILE }))
                return true;
    return false;
}

bool Game::Blocked(Vector2 p, float r, float z) const {
    return CircleHitsWall(p, r) || props.BlocksCircle(p, r, z);
}

// Muove separatamente su X e su Y: cosi' si "scivola" lungo muri e oggetti invece di bloccarsi
void Game::MoveWithCollision(Vector2& pos, float r, Vector2 delta, float z) {
    Vector2 nx{ pos.x + delta.x, pos.y };
    if (!Blocked(nx, r, z)) pos = nx;
    Vector2 ny{ pos.x, pos.y + delta.y };
    if (!Blocked(ny, r, z)) pos = ny;
}

void Game::AddText(Vector2 pos, const std::string& text, Color color) {
    texts.push_back({ pos, text, color, 0.9f });
}

void Game::AddPlayerText(const std::string& text, Color color) {
    FloatingText t{ player.pos, text, color, 1.2f };
    t.onScreen = true;
    texts.push_back(t);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void Game::Draw() {
    bool overTitle = state == State::ControllerMenu && menuReturnState == State::Title;
    if (state == State::Title || overTitle) {
        ClearBackground(BLACK);
        DrawTitle();
        if (overTitle) DrawControllerMenu();
        input.DrawTouch();
        DrawToast();
        return;
    }
    if (state == State::Victory) {
        ClearBackground(BLACK);
        DrawVictory();
        input.DrawTouch();
        return;
    }
    if (state == State::Intro || (state == State::ControllerMenu && menuReturnState == State::Intro)) {
        ClearBackground(BLACK);
        DrawIntro(cutsceneTime, introNewRun);
        if (state == State::ControllerMenu) DrawControllerMenu();
        input.DrawTouch();
        DrawToast();
        return;
    }
    const District& d = CurrentDistrict();
    ClearBackground(d.horizon);
    DrawSky(d, player.yaw, camera.fovy);

    BeginMode3D(camera);
    DrawWorld();
    EndMode3D();

    DrawWorldOverlay();
    DrawFirstPerson();
    DrawHud();
    DrawGuide();
    DrawTutorial();
    if (fadeFromBlack > 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, fadeFromBlack));
    DrawBossBar();
    DrawChallengeBanner();
    if (state == State::InventoryFull) DrawInventoryChoice();
    if (state == State::GameOver) DrawGameOver();
    if (state == State::Paused) DrawPause();
    if (state == State::MatchMenu) DrawMatchMenu();
    if (state == State::ControllerMenu) DrawControllerMenu();
    input.DrawTouch(); // tasti a schermo (solo se si sta usando il touch)
    DrawToast();
}

void Game::DrawWorld() const {
    const District& d = CurrentDistrict();
    dungeon.Draw3D(d, camera.position, player.aim);
    props.Draw(d, camera.position, player.aim);

    float t = (float)GetTime();
    for (const auto& p : pickups) {
        if (p.taken) continue;
        float dist = Vector2Distance(p.pos, player.pos) / TILE;
        if (dist > VIEW_DIST) continue;
        Vector3 base = To3D(p.pos, 0);
        if (p.weaponId >= 0) {
            // Arma: gira a mezz'aria sopra un cerchio del suo colore
            const Weapon& w = AllWeapons()[p.weaponId];
            DrawCircle3D({ base.x, 0.01f, base.z }, 0.35f + 0.04f * sinf(t * 3), { 1, 0, 0 }, 90, w.color);
            rlPushMatrix();
            rlTranslatef(base.x, 0.55f + 0.06f * sinf(t * 2 + p.pos.x), base.z);
            rlRotatef(t * 70, 0, 1, 0);
            rlRotatef(20, 1, 0, 0);
            DrawWeapon3D(p.weaponId);
            rlPopMatrix();
            continue;
        }
        if (p.artifactId < 0) {
            // Cuore: una sfera rossa che saltella
            DrawSphereEx({ base.x, 0.22f + 0.05f * sinf(t * 4 + p.pos.x), base.z }, 0.14f, 8, 8,
                         ApplyFog({ 220, 50, 70, 255 }, d.horizon, dist));
            continue;
        }
        // Artefatto: un cristallo che gira sopra un piccolo altare di pietra
        Color col = TagColor(FirstTagIndex(AllArtifacts()[p.artifactId].tags));
        DrawCube({ base.x, 0.2f, base.z }, 0.45f, 0.4f, 0.45f, ApplyFog({ 95, 90, 85, 255 }, d.horizon, dist));
        DrawCubeWires({ base.x, 0.2f, base.z }, 0.45f, 0.4f, 0.45f, ApplyFog({ 60, 55, 50, 255 }, d.horizon, dist));
        rlPushMatrix();
        rlTranslatef(base.x, 0.68f + 0.06f * sinf(t * 2 + p.pos.x), base.z);
        rlRotatef(t * 90, 0, 1, 0);
        rlRotatef(45, 1, 0, 1);
        DrawCube({ 0, 0, 0 }, 0.2f, 0.2f, 0.2f, col);
        DrawCubeWires({ 0, 0, 0 }, 0.2f, 0.2f, 0.2f, WHITE);
        rlPopMatrix();
    }

    for (const auto& e : enemies)
        if (e.alive) DrawEnemy(e);

    for (const auto& p : projectiles) {
        if (!p.alive) continue;
        if (Vector2Distance(p.pos, player.pos) / TILE > VIEW_DIST) continue;
        Vector3 c = To3D(p.pos, p.height);
        Vector3 dir = Vector3Normalize({ p.vel.x, 0, p.vel.y });
        if (!p.fromPlayer) {
            switch (p.look) {
                case EnemyShot::Laser: // dardo rosso dei robot
                    DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.22f)), c, 0.035f, 0.035f, 5, { 255, 60, 50, 255 });
                    DrawSphereEx(c, 0.05f, 4, 4, { 255, 200, 190, 255 });
                    break;
                case EnemyShot::Rock: // masso lanciato dall'oni
                    DrawSphereEx(c, p.radius / TILE, 7, 7, { 120, 105, 95, 255 });
                    break;
                case EnemyShot::Spirit: // sfera spiritica del matagot
                    DrawSphereEx(c, p.radius / TILE, 6, 6, { 120, 255, 140, 255 });
                    DrawSphereEx(c, p.radius / TILE * 0.5f, 4, 4, WHITE);
                    break;
                case EnemyShot::BlueFire: // fiamma blu di Spring-heeled Jack
                    DrawSphereEx(c, p.radius / TILE * (0.9f + 0.2f * sinf(t * 30)), 5, 5, { 80, 140, 255, 255 });
                    DrawSphereEx(c, p.radius / TILE * 0.5f, 4, 4, { 200, 230, 255, 255 });
                    break;
                case EnemyShot::Gossip: // i pettegolezzi del Badalisc: nuvolette di chiacchiere
                    DrawSphereEx(c, p.radius / TILE, 5, 5, { 240, 230, 180, 255 });
                    DrawSphereEx(Vector3Add(c, { 0.05f, 0.04f, 0 }), p.radius / TILE * 0.6f, 4, 4, { 250, 245, 220, 255 });
                    break;
                default: // sfera di vento del tengu
                    DrawSphereEx(c, p.radius / TILE, 6, 6, { 170, 80, 255, 255 });
                    DrawSphereEx(c, p.radius / TILE * 0.55f, 4, 4, { 230, 200, 255, 255 });
                    break;
            }
            continue;
        }
        const Weapon& w = AllWeapons()[p.weapon];
        switch (w.shot) {
            case ShotLook::Arrow: // freccia o dardo: asta e punta
                DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.35f)), c, 0.012f, 0.012f, 4, w.shape == WeaponShape::Banderilla ? w.color : Color{ 140, 96, 56, 255 });
                DrawCylinderEx(c, Vector3Add(c, Vector3Scale(dir, 0.08f)), 0.03f, 0.0f, 4, { 190, 195, 205, 255 });
                break;
            case ShotLook::Ball:
                DrawSphereEx(c, p.radius / TILE, 6, 6, p.crit ? GOLD : w.color);
                break;
            case ShotLook::Bullet:
                DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.12f)), c, 0.012f, 0.02f, 4, p.silver ? Color{ 230, 235, 250, 255 } : Color{ 255, 220, 140, 255 });
                break;
            case ShotLook::Axe: { // tomahawk che gira su se stesso
                float spin = t * 20;
                Vector3 s{ cosf(spin) * 0.15f, sinf(spin) * 0.15f, 0 };
                DrawCylinderEx(Vector3Subtract(c, s), Vector3Add(c, s), 0.015f, 0.015f, 4, w.color2);
                DrawSphereEx(Vector3Add(c, s), 0.05f, 4, 4, w.color);
                break;
            }
            case ShotLook::Cork:
                DrawCylinderEx(c, Vector3Add(c, Vector3Scale(dir, 0.08f)), 0.035f, 0.03f, 6, { 200, 170, 110, 255 });
                break;
            case ShotLook::Mug:
                DrawCube(c, 0.14f, 0.18f, 0.14f, w.color);
                DrawCube(Vector3Add(c, { 0, 0.1f, 0 }), 0.15f, 0.04f, 0.15f, w.color2);
                break;
            case ShotLook::Dart:
                DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.12f)), c, 0.01f, 0.0f, 4, w.color);
                break;
            case ShotLook::Spark: // petardo con la miccia accesa
                DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.06f)), Vector3Add(c, Vector3Scale(dir, 0.06f)), 0.035f, 0.035f, 6, w.color);
                DrawSphereEx(Vector3Add(c, { 0, 0.05f, 0 }), 0.02f + 0.01f * sinf(t * 40), 3, 3, { 255, 230, 120, 255 });
                break;
            case ShotLook::Wave: // l'onda di vento della Kusanagi
                DrawCylinderEx(Vector3Add(c, { -dir.z * 0.4f, 0, dir.x * 0.4f }), Vector3Add(c, { dir.z * 0.4f, 0, -dir.x * 0.4f }),
                               0.05f, 0.05f, 5, Fade({ 170, 240, 255, 255 }, 0.6f));
                break;
            case ShotLook::Rocket: // razzo con la scia di scintille
                DrawCylinderEx(Vector3Subtract(c, Vector3Scale(dir, 0.15f)), Vector3Add(c, Vector3Scale(dir, 0.1f)),
                               0.05f, 0.05f, 6, { 200, 45, 45, 255 });
                DrawCylinderEx(Vector3Add(c, Vector3Scale(dir, 0.1f)), Vector3Add(c, Vector3Scale(dir, 0.18f)),
                               0.05f, 0.0f, 6, { 230, 190, 60, 255 });
                for (int k = 1; k <= 3; ++k)
                    DrawSphereEx(Vector3Subtract(c, Vector3Scale(dir, 0.15f + k * 0.12f)), 0.035f - k * 0.008f, 3, 3,
                                 k % 2 ? Color{ 255, 200, 80, 255 } : Color{ 255, 120, 40, 255 });
                break;
            default: // ofuda: un foglietto di carta
                DrawCube(c, 0.08f, 0.13f, 0.08f, p.crit ? GOLD : Color{ 245, 238, 220, 255 });
                break;
        }
    }

    // Per ultime le cose trasparenti: fiamme, fumo e fuochi d'artificio
    props.DrawFires(camera.position);
    DrawExplosions();
}

// Fuochi d'artificio dei razzi Tamaya: scintille colorate che si allargano e svaniscono
void Game::DrawExplosions() const {
    if (explosions.empty()) return;
    static const Color colors[] = { { 255, 80, 80, 255 },  { 255, 210, 80, 255 },  { 120, 255, 140, 255 },
                                    { 90, 220, 255, 255 }, { 255, 120, 220, 255 }, { 255, 255, 255, 255 } };
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const auto& x : explosions) {
        float k = x.t / 0.8f, grow = 1 - (1 - k) * (1 - k);
        if (x.t < 0.25f) DrawSphereEx(x.pos, 0.5f * (1 - x.t / 0.25f) + 0.1f, 8, 8, Fade({ 255, 230, 160, 255 }, 0.6f));
        for (int i = 0; i < 28; ++i) {
            unsigned h = TileHash((int)x.seed, i, 31);
            float yaw = (h % 628) / 100.0f, pitch = ((int)(h / 628 % 300) - 150) / 100.0f;
            Vector3 dir{ cosf(yaw) * cosf(pitch), sinf(pitch) * 0.8f + 0.3f, sinf(yaw) * cosf(pitch) };
            Vector3 p = Vector3Add(x.pos, Vector3Scale(dir, grow * 1.7f));
            p.y -= k * k * 0.4f; // le scintille ricadono un po'
            Color col = x.foam ? Color{ 250, 245, 225, 255 } : colors[(h >> 5) % 6]; // schiuma: bianca
            DrawSphereEx(p, x.foam ? 0.08f : 0.05f, 3, 3, Fade(col, 1 - k));
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

// Barra del guardiano in alto al centro, finche' ti sta dando la caccia
void Game::DrawBossBar() const {
    const Enemy* boss = AwakeBoss();
    if (!boss || state != State::Playing) return;
    int sw = GetScreenWidth(), w = std::min(560, sw - 760);
    if (w < 200) w = 200;
    int x = (sw - w) / 2, y = 24;
    std::string title = InfoOf(boss->type).bossTitle;
    if (boss->empowered) title = "POTENZIATO - " + title;
    if (boss->enraged) title += " (furioso)";
    DrawTextCentered(title.c_str(), sw / 2, y, 20, boss->enraged ? Color{ 255, 120, 100, 255 } : GOLD);
    DrawRectangle(x, y + 24, w, 12, { 20, 20, 20, 210 });
    DrawRectangle(x, y + 24, (int)(w * std::max(0.0f, boss->hp / boss->maxHp)), 12, { 190, 40, 60, 255 });
    DrawRectangleLines(x, y + 24, w, 12, WHITE);
}

// Testi e barre in 2D sopra le cose del mondo, solo se sono davanti a te e non dietro un muro
void Game::DrawWorldOverlay() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float focal = sh * 0.5f / tanf(camera.fovy * DEG2RAD * 0.5f); // pixel per unita' a distanza 1

    auto visible = [&](Vector2 pos) {
        Vector2 to = Vector2Subtract(pos, player.pos);
        float dist = Vector2Length(to) / TILE;
        return dist < VIEW_DIST && Vector2DotProduct(to, player.aim) > 4 && dungeon.HasLineOfSight(player.pos, pos);
    };

    for (const auto& e : enemies) {
        if (!e.alive || e.hp >= e.maxHp || e.hidden || !Damageable(e) || !visible(e.pos)) continue;
        float r = e.radius / TILE;
        float dist = Vector2Distance(e.pos, player.pos) / TILE;
        Vector2 s = GetWorldToScreen(To3D(e.pos, EnemyBodyY(e) + r + 0.18f), camera);
        int w = std::clamp((int)(2 * r * focal / dist), 16, 140);
        int x = (int)s.x - w / 2, y = (int)s.y;
        DrawRectangle(x, y, w, 5, { 20, 20, 20, 200 });
        DrawRectangle(x, y, (int)(w * std::max(0.0f, e.hp / e.maxHp)), 5, { 220, 60, 60, 255 });
    }

    for (const auto& ft : texts) {
        Color col = Fade(ft.color, std::min(1.0f, ft.life * 2));
        if (ft.onScreen) {
            int y = sh / 2 + 60 - (int)((1.2f - ft.life) * 40);
            DrawTextCentered(ft.text.c_str(), sw / 2, y, 22, col);
            continue;
        }
        if (!visible(ft.pos)) continue;
        Vector2 s = GetWorldToScreen(To3D(ft.pos, ft.height), camera);
        DrawTextCentered(ft.text.c_str(), (int)s.x, (int)s.y, 20, col);
    }

    // Suggerimenti contestuali vicino ad armi, artefatti e uscita, sopra il mirino
    if (state != State::Playing) return;
    const char* key = input.UsingTouch() ? "PRENDI" : input.UsingPad() ? input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) : "E";
    int weapon = NearestPickup(48, true), artifact = NearestPickup(48, false);
    Rectangle card{ sw / 2.0f - 190, sh / 2.0f - 200, 380, 104 };
    if (weapon >= 0) {
        const Weapon& w = AllWeapons()[pickups[weapon].weaponId];
        std::string footer;
        if (player.weapons[1] >= 0) footer = std::string("Lascerai a terra: ") + AllWeapons()[player.Weapon()].name;
        DrawWeaponCard(w, card, footer.c_str());
        DrawTextCentered(TextFormat("[%s] prendi", key), sw / 2, sh / 2 - 86, 18, WHITE);
    } else if (artifact >= 0) {
        DrawArtifactCard(AllArtifacts()[pickups[artifact].artifactId], card, WHITE);
        DrawTextCentered(TextFormat("[%s] raccogli", key), sw / 2, sh / 2 - 86, 18, WHITE);
    } else if (Vector2Distance(player.pos, dungeon.ExitPos()) < 48) {
        DrawTextCentered(TextFormat("[%s] attraversa il torii", key), sw / 2, sh / 2 - 86, 22, WHITE);
    }
}

void Game::DrawFirstPerson() const {
    const District& d = CurrentDistrict();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // Mirino
    int cx = sw / 2, cy = sh / 2;
    DrawRectangle(cx - 9, cy - 1, 6, 2, Fade(WHITE, 0.85f));
    DrawRectangle(cx + 3, cy - 1, 6, 2, Fade(WHITE, 0.85f));
    DrawRectangle(cx - 1, cy - 9, 2, 6, Fade(WHITE, 0.85f));
    DrawRectangle(cx - 1, cy + 3, 2, 6, Fade(WHITE, 0.85f));
    DrawCircle(cx, cy, 1.5f, d.accent);

    // L'arma in mano: ondeggia camminando, sobbalza nei salti, rincula quando attacchi
    float bobX = cosf(walkCycle * 0.5f) * 6, bobY = fabsf(sinf(walkCycle)) * 6 - player.vz * 3;
    DrawWeaponInHand(player.Weapon(), bobX, bobY, recoil, std::max(0.0f, swingTimer / SWING_TIME),
                     player.shootTimer <= 0, d.accent);

    // Bordi rossi quando vieni colpito
    if (hurtTimer > 0) {
        Color c = Fade(RED, std::min(1.0f, hurtTimer * 1.5f) * 0.5f);
        DrawRectangleGradientV(0, 0, sw, 90, c, BLANK);
        DrawRectangleGradientV(0, sh - 90, sw, 90, BLANK, c);
        DrawRectangleGradientH(0, 0, 90, sh, c, BLANK);
        DrawRectangleGradientH(sw - 90, 0, 90, sh, BLANK, c);
    }
}

// Riga di aiuto in basso: col joystick usa i nomi veri dei suoi tasti
std::string Game::ControlsHint() const {
    if (!input.UsingPad())
        return "WASD muovi | Mouse girati | Click attacca | Spazio salta | Shift scatto | E raccogli | Q arma | Tab partita | Esc menu";
    const char* gameMenu = input.Family() == Input::PadFamily::PlayStation ? "Touchpad" : input.Btn(GAMEPAD_BUTTON_MIDDLE_LEFT);
    return TextFormat("Stick sx muovi | Stick dx girati | %s attacca | %s salta | Frecce scatto | %s raccogli | %s arma | %s partita | %s menu",
                      input.Btn(GAMEPAD_BUTTON_RIGHT_TRIGGER_2), input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_DOWN),
                      input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_LEFT), input.Btn(GAMEPAD_BUTTON_RIGHT_FACE_UP),
                      input.Btn(GAMEPAD_BUTTON_MIDDLE_RIGHT), gameMenu);
}

void Game::DrawHud() const {
    const District& d = CurrentDistrict();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // Vita
    DrawRectangle(20, 20, 260, 22, { 20, 20, 20, 200 });
    DrawRectangle(20, 20, (int)(260 * player.hp / stats.maxHp), 22, { 200, 50, 50, 255 });
    DrawRectangleLines(20, 20, 260, 22, WHITE);
    DrawText(TextFormat("%d / %d", (int)player.hp, (int)stats.maxHp), 28, 23, 18, WHITE);

    // Ricarica dello scatto
    float dashReady = 1.0f - std::max(0.0f, player.dashCdTimer) / stats.dashCooldown;
    DrawRectangle(20, 46, 260, 6, { 20, 20, 20, 200 });
    DrawRectangle(20, 46, (int)(260 * dashReady), 6, dashReady >= 1 ? SKYBLUE : GRAY);

    // Le scritte stanno su una fascia scura: il cielo puo' essere chiaro
    DrawRectangle(14, 56, 420, 66, Fade(BLACK, 0.35f));
    DrawText(TextFormat("Piano %d - %s: %s", floor, d.city, d.place), 20, 60, 20, d.accent);
    DrawText(TextFormat("Nemici sconfitti: %d", kills), 20, 84, 16, LIGHTGRAY);
    DrawText(TextFormat("Danno %.1f  Cadenza %.1f/s  Critico %d%%  Proiettili %d",
                        stats.damage, stats.fireRate, (int)(stats.critChance * 100), stats.projectiles),
             20, 102, 16, LIGHTGRAY);

    // Le due armi: quella in uso e' evidenziata
    for (int s = 0; s < WEAPON_SLOTS; ++s) {
        Rectangle r{ 20.0f + s * 170, 128, 160, 40 };
        bool active = s == player.activeSlot;
        DrawRectangleRec(r, { 15, 15, 20, 210 });
        int id = player.weapons[s];
        DrawRectangleLinesEx(r, active ? 2.0f : 1.0f, active ? (id >= 0 ? AllWeapons()[id].color : GOLD) : Color{ 90, 90, 100, 255 });
        if (id >= 0) DrawText(AllWeapons()[id].name, (int)r.x + 10, (int)r.y + 11, 18, active ? WHITE : GRAY);
        else DrawText("nessuna arma", (int)r.x + 10, (int)r.y + 12, 16, DARKGRAY);
    }

    DrawStatusEffects();

    // Pannello destro: elementi e sinergie attive
    int px = sw - 270, py = input.UsingTouch() ? 92 : 20; // col touch in alto a destra ci sono i tasti menu
    DrawRectangle(px - 10, py - 8, 260, 150 + 40 * (int)ActiveSynergies(inventory).size(), { 0, 0, 0, 140 });
    DrawText("Elementi", px, py, 20, WHITE);
    py += 26;
    for (int i = 0; i < TAG_COUNT; ++i) {
        int c = CountTag(inventory, 1u << i);
        Color col = c >= 3 ? TagColor(i) : Fade(TagColor(i), 0.6f);
        DrawCircle(px + 6, py + 7, 6, col);
        DrawText(TextFormat("%s  %d/3", TagName(i), c), px + 18, py, 16, col);
        py += 20;
    }
    py += 6;
    DrawText("Sinergie", px, py, 20, WHITE);
    py += 24;
    for (int s : ActiveSynergies(inventory)) {
        DrawText(AllSynergies()[s].name, px, py, 16, GOLD);
        DrawText(AllSynergies()[s].desc, px, py + 17, 12, LIGHTGRAY);
        py += 40;
    }

    // Inventario in basso. Col touch li' ci sono stick e tasti: l'inventario si vede nel menu BORSA
    if (input.UsingTouch()) {
        DrawIntroTitle(d);
        return;
    }
    const int slotW = 220, slotH = 64, gap = 10;
    int total = INVENTORY_SIZE * slotW + (INVENTORY_SIZE - 1) * gap;
    int x0 = (sw - total) / 2, y = sh - slotH - 16;
    std::string hint = ControlsHint();
    int hintSize = MeasureText(hint.c_str(), 16) > total ? 14 : 16;
    DrawRectangle(x0 - 6, y - 25, MeasureText(hint.c_str(), hintSize) + 12, 21, Fade(BLACK, 0.35f));
    DrawText(hint.c_str(), x0, y - 22, hintSize, Fade(WHITE, 0.75f));
    for (int k = 0; k < INVENTORY_SIZE; ++k) {
        Rectangle r{ (float)(x0 + k * (slotW + gap)), (float)y, (float)slotW, (float)slotH };
        DrawRectangleRec(r, { 15, 15, 20, 210 });
        DrawRectangleLinesEx(r, 2, { 90, 90, 100, 255 });
        DrawText(TextFormat("%d", k + 1), (int)r.x + slotW - 16, (int)r.y + 6, 14, GRAY);
        if (k < (int)inventory.size()) {
            const Artifact& a = AllArtifacts()[inventory[k]];
            DrawText(a.name, (int)r.x + 10, (int)r.y + 8, 18, WHITE);
            DrawTagChips(a.tags, (int)r.x + 10, (int)r.y + 36);
        } else {
            DrawText("vuoto", (int)r.x + 10, (int)r.y + 22, 16, DARKGRAY);
        }
    }
    DrawIntroTitle(d);
}

// Titolo del distretto all'ingresso di ogni piano
void Game::DrawIntroTitle(const District& d) const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (introTimer > 0) {
        float alpha = std::min(1.0f, introTimer);
        DrawTextCentered(d.city, sw / 2 + 3, sh / 3 + 3, 72, Fade(BLACK, alpha * 0.6f));
        DrawTextCentered(d.city, sw / 2, sh / 3, 72, Fade(d.accent, alpha));
        DrawTextCentered(d.place, sw / 2, sh / 3 + 80, 28, Fade(WHITE, alpha));
        DrawTextCentered("Il labirinto e' caduto anche qui. Trova il torii per andare avanti",
                         sw / 2, sh / 3 + 118, 18, Fade(LIGHTGRAY, alpha));
    }
}

void Game::DrawInventoryChoice() const {
    int sw = GetScreenWidth();
    DrawRectangle(0, 0, sw, GetScreenHeight(), Fade(BLACK, 0.8f));

    int newId = pickups[pendingPickup].artifactId;
    DrawTextCentered("Inventario pieno! Cosa vuoi scartare?", sw / 2, 30, 32, WHITE);
    DrawArtifactCard(AllArtifacts()[newId], { sw / 2.0f - 200, 74, 400, 104 }, GOLD);

    std::vector<int> current = ActiveSynergies(inventory);
    const int rowW = 940, rowH = 64;
    int x = (sw - rowW) / 2, y = 196;

    // Per ogni scelta mostriamo come cambierebbero le sinergie: verde = guadagnata, rosso = persa
    for (int k = 0; k <= (int)inventory.size(); ++k) {
        bool keep = k == (int)inventory.size(); // ultima riga: lascia a terra
        std::vector<int> inv = inventory;
        if (!keep) inv[k] = newId;
        std::vector<int> after = ActiveSynergies(inv);

        DrawRectangle(x, y, rowW, rowH, { 25, 25, 32, 255 });
        if (k == choiceIndex) DrawRectangleLinesEx({ (float)x, (float)y, (float)rowW, (float)rowH }, 3, GOLD);
        else DrawRectangleLines(x, y, rowW, rowH, { 90, 90, 100, 255 });
        if (keep) {
            DrawText("[6 / Q]  Lascia a terra il nuovo artefatto", x + 12, y + 10, 20, LIGHTGRAY);
        } else {
            const Artifact& a = AllArtifacts()[inventory[k]];
            DrawText(TextFormat("[%d]  Scarta %s", k + 1, a.name), x + 12, y + 8, 20, WHITE);
            DrawTagChips(a.tags, x + 12, y + 36);
            DrawText(a.desc, x + 200, y + 38, 14, GRAY);
        }

        int sx = x + 520;
        DrawText("Sinergie:", sx, y + 10, 16, GRAY);
        sx += MeasureText("Sinergie:", 16) + 8;
        bool any = false;
        for (int s = 0; s < (int)AllSynergies().size(); ++s) {
            bool before = std::find(current.begin(), current.end(), s) != current.end();
            bool now = std::find(after.begin(), after.end(), s) != after.end();
            if (!before && !now) continue;
            any = true;
            const char* label = !before ? TextFormat("+%s", AllSynergies()[s].name)
                              : !now    ? TextFormat("-%s", AllSynergies()[s].name)
                                        : AllSynergies()[s].name;
            Color col = !before ? GREEN : !now ? RED : WHITE;
            DrawText(label, sx, y + 10, 16, col);
            sx += MeasureText(label, 16) + 12;
        }
        if (!any) DrawText("nessuna", sx, y + 10, 16, DARKGRAY);
        y += rowH + 8;
    }
    DrawTextCentered("Su/Giu per scegliere, Invio o A per confermare, Esc o B per lasciarlo a terra",
                     sw / 2, y + 8, 16, Fade(WHITE, 0.6f));
}

void Game::DrawGameOver() const {
    // Il giocatore si sveglia di soprassalto dall'incubo...
    DrawWakeUp(deathTime);
    if (deathTime < WakeUp::END - 0.6f) {
        if (!input.UsingTouch()) DrawText("Invio / A: salta", GetScreenWidth() - 170, 16, 16, Fade(WHITE, 0.5f));
        return;
    }
    // ...e poi il resoconto del sogno
    const District& d = CurrentDistrict();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float a = std::min(1.0f, (deathTime - (WakeUp::END - 0.6f)) / 0.6f);
    Rectangle panel{ sw - 520.0f, sh * 0.5f - 210, 470, 330 };
    DrawRectangleRounded(panel, 0.08f, 8, Fade({ 14, 14, 22, 255 }, 0.88f * a));
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2, Fade(d.accent, a));
    int x = (int)panel.x + 28, y = (int)panel.y + 22;
    DrawText(TextFormat("Sei caduto a %s", d.city), x, y, 34, Fade(d.accent, a));
    y += 56;
    int minutes = (int)runTime / 60, seconds = (int)runTime % 60;
    DrawText(TextFormat("Piano raggiunto: %d", floor), x, y, 22, Fade(WHITE, a));
    DrawText(TextFormat("Nemici sconfitti: %d", kills), x, y + 36, 22, Fade(WHITE, a));
    DrawText(TextFormat("Artefatti raccolti: %d", artifactsFound), x, y + 72, 22, Fade(WHITE, a));
    DrawText(TextFormat("Tempo di sogno: %d:%02d", minutes, seconds), x, y + 108, 22, Fade(WHITE, a));
    DrawText("Invio, A o R: sogna ancora", x, (int)(panel.y + panel.height) - 46, 22, Fade(GOLD, a * (0.6f + 0.4f * sinf((float)GetTime() * 4))));
}

void Game::DrawToast() const {
    if (toastTimer <= 0) return;
    int sw = GetScreenWidth();
    int w = MeasureText(toast.c_str(), 20) + 32;
    float alpha = std::min(1.0f, toastTimer);
    // Agganciato al bordo in basso: resta al suo posto anche a schermo intero. Nel menu va sotto le voci
    int y = GetScreenHeight() - (state == State::ControllerMenu ? 104 : 154);
    DrawRectangle(sw / 2 - w / 2, y, w, 36, Fade({ 20, 60, 40, 255 }, 0.9f * alpha));
    DrawTextCentered(toast.c_str(), sw / 2, y + 8, 20, Fade(WHITE, alpha));
}
