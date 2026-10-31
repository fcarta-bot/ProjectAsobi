#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "Artifacts.hpp"
#include "Dungeon.hpp"
#include "GameMaster.hpp"
#include "Input.hpp"
#include "Nav.hpp"
#include "Props.hpp"
#include "Weapons.hpp"
#include "link/ControllerLink.hpp"

constexpr int INVENTORY_SIZE = 5;
constexpr int WEAPON_SLOTS = 2;
constexpr float ONI_SLAM_RADIUS = 110; // pixel: lo schianto a terra dell'oni si evita saltando

struct Player {
    Vector2 pos{};
    float z = 0, vz = 0;   // quota dei piedi (in caselle) e velocita' verticale: per i salti
    bool onGround = true;
    float yaw = 0;         // direzione dello sguardo in radianti (0 = verso destra sulla mappa)
    Vector2 aim{ 1, 0 };   // la stessa direzione come vettore, ricalcolata da yaw
    Vector2 dashDir{};
    float radius = 12;
    float hp = 100;
    float shootTimer = 0;
    float dashTimer = 0;   // > 0 mentre lo scatto e' in corso
    float dashCdTimer = 0; // > 0 mentre lo scatto si ricarica
    float invuln = 0;      // secondi di invulnerabilita' rimasti
    int weapons[WEAPON_SLOTS] = { W_OFUDA, -1 }; // -1 = slot vuoto
    int activeSlot = 0;
    int Weapon() const { return weapons[activeSlot]; }
};

// Goblin: sciami da 7 a 20, corpo a corpo. Robot: squadre da 3 a 7 guidate dal GameMaster.
// Tengu: da solo, spara e scappa se ti avvicini. Oni: il boss che fa la guardia al torii.
enum class EnemyType { Goblin, Robot, Tengu, Oni };
enum class RobotRole { Assault, Gunner }; // le squadre di robot sono miste
enum class EnemyAttack { None, Bite, Swing, Charge, Slam, Burst, Throw };
constexpr int GOBLIN_SWARM_MIN = 7, GOBLIN_SWARM_MAX = 20; // sotto il minimo scappano, sopra il massimo si sbranano
constexpr int ROBOT_SQUAD_MIN = 3, ROBOT_SQUAD_MAX = 7;

struct Enemy {
    int id = 0;
    EnemyType type = EnemyType::Goblin;
    RobotRole role = RobotRole::Assault;
    Vector2 pos{};
    Vector2 facing{ 1, 0 }; // dove guarda: si accorge di te solo se sei nel suo campo visivo
    float radius = 12, hp = 1, maxHp = 1, speed = 100, damage = 10;
    float baseRadius = 12;  // i goblin crescono quando assorbono la forza dei compagni
    float shootTimer = 0, hitFlash = 0;
    float burnTimer = 0, burnDps = 0;
    bool alive = true;

    // Percezione: l'interesse sale quando ti vede, scende quando ti perde (e piu' in fretta se sei veloce)
    bool aggro = false;
    bool seesPlayer = false;
    float interest = 0;     // da 0 a 1
    Vector2 lastSeen{};     // dove ti ha visto l'ultima volta
    Vector2 home{};         // dove torna quando si stufa
    bool thinkNow = false;
    float thinkTimer = 0;   // vista e linee libere si ricontrollano qualche volta al secondo, non a ogni frame

    // Movimento
    std::vector<Vector2> path;
    int pathIndex = 0;
    Vector2 pathGoal{};
    float repathTimer = 0;
    bool directClear = false; // linea dritta e libera verso il punto in cui vuole andare
    Vector2 moveGoal{};       // il punto per cui e' stata calcolata directClear
    Vector2 lastPos{};
    float stuckTimer = 0;
    float avoidDirect = 0;    // dopo essersi incastrato segue la mappa per un po', niente linee dritte

    // Gruppo
    int swarm = -1;            // indice in Game::swarms; -1 = da solo
    float slotAngle = 0;       // posto intorno al giocatore quando lo sciame lo circonda
    Vector2 order{};           // posizione assegnata dal GameMaster (robot)
    bool hasOrder = false;
    float doom = 0;            // > 0: goblin di troppo, i compagni lo stanno eliminando
    float power = 1;           // forza assorbita dai compagni eliminati

    // Combattimento: ogni attacco ha un preavviso visibile, cosi' lo si puo' schivare
    EnemyAttack attack = EnemyAttack::None;
    float windup = 0;          // > 0: sta caricando l'attacco
    float attackTimer = 0;     // ricarica tra un attacco e l'altro
    Vector2 attackDir{};
    float actionTimer = 0;     // durata della carica dell'oni / raffica dei robot
    int burstLeft = 0;
    bool attackHit = false;
    float stun = 0;
    float strafeTimer = 0;
    float strafeSign = 1;

    // Solo per l'oni
    bool enraged = false;
    float chargeCd = 0, slamCd = 0, dodgeCd = 0, elevatedTimer = 0;
    Vector2 dodgeVel{};
    float dodgeTime = 0;
};

// Sciame di goblin o squadra di robot. I membri si riconoscono da Enemy::swarm
struct Swarm {
    EnemyType type = EnemyType::Goblin;
    bool alive = true;
    int size = 0;          // membri vivi (senza quelli condannati)
    Vector2 center{};
    Vector2 home{};        // stanza di partenza: ci gira intorno quando e' tranquillo
    Vector2 wander{};
    float wanderTimer = 0;
    int joinTarget = -1;   // goblin: sciame troppo piccolo che cerca di unirsi a un altro
    // Squadre di robot (GameMaster)
    float orderTimer = 0;
    int patrolRoom = -1;
    float waitTimer = 0;
    int tactic = -1;       // 0 pattuglia, 1 caccia, 2 ricerca
};

enum class EnemyShot { Wind, Laser, Rock }; // tengu, robot, oni

struct Projectile {
    Vector2 pos{}, vel{};
    float height = 0.45f; // quota del colpo: serve per i salti e per gli oggetti bassi
    float radius = 4, damage = 10, life = 1;
    bool fromPlayer = true;
    bool crit = false;
    bool alive = true;
    int pierceLeft = 0;
    int bouncesLeft = 0;
    bool explosive = false;
    int weapon = W_OFUDA; // come disegnarlo
    EnemyShot look = EnemyShot::Wind; // colpi nemici: come disegnarli
    std::vector<int> hitIds; // nemici gia' colpiti (per i colpi che trapassano)
};

struct Pickup {
    Vector2 pos{};
    int artifactId = -1; // -1 = cuore che cura (se weaponId e' -1)
    int weaponId = -1;   // >= 0: un'arma
    bool taken = false;
};

struct FloatingText {
    Vector2 pos;
    std::string text;
    Color color;
    float life;
    float height = 0.9f;  // altezza nel mondo 3D, sale piano piano
    bool onScreen = false; // testi del giocatore: in prima persona vanno sotto il mirino
};

// Esplosione dei razzi: solo un effetto visivo, il danno e' immediato
struct Explosion {
    Vector3 pos;
    float t = 0;
    unsigned seed = 0;
};

enum class State { Intro, Playing, InventoryFull, GameOver, Paused, MatchMenu, ControllerMenu };

// Consigli del GameMaster: compaiono solo se il giocatore sembra in difficolta' con un comando
enum Tip { TIP_MOVE, TIP_LOOK, TIP_PICKUP, TIP_ATTACK, TIP_JUMP, TIP_DASH, TIP_SWAP, TIP_MENU, TIP_LOST, TIP_COUNT };

// Schede del menu di partita
enum MatchTab { TAB_INVENTORY, TAB_WEAPONS, TAB_MAP, TAB_STATS, TAB_COUNT };

// Pagine del menu Controller
enum class MenuPage {
    Devices,       // elenco dei controlli: tastiera, joystick rilevati, "collega un nuovo joystick"
    NewController, // scelta del metodo: Bluetooth, cavo USB, pennetta/ricevitore
    Bluetooth,     // ricerca e abbinamento Bluetooth
    Wired,         // attesa di un joystick via cavo o pennetta
};

class Game {
    friend class GameMaster; // il regista vede e muove tutto
public:
    Game();
    void Update(float dt);
    void Draw();
    bool WantsQuit() const { return quit; }

private:
    void NewRun();
    void NextFloor();
    void SpawnFloorContent();
    Enemy& SpawnEnemy(EnemyType type, Vector2 pos);
    Vector2 FreeSpotInRoom(const Room& r, float radius) const;
    int RandomArtifact() const;
    int RandomWeapon() const;
    void RecomputeStats();

    void UpdatePlayer(float dt);
    void UpdateProjectiles(float dt);

    // IA dei nemici (EnemyAI.cpp)
    void UpdateEnemies(float dt);
    void Perceive(Enemy& e, float dt);
    void ShareSwarmAwareness();
    void UpdateSwarms(float dt);
    void UpdateGoblin(Enemy& e, float dt);
    void UpdateRobot(Enemy& e, float dt);
    void UpdateTengu(Enemy& e, float dt);
    void UpdateOni(Enemy& e, float dt);
    void UpdateCalm(Enemy& e, float dt);
    bool UpdateMeleeAttack(Enemy& e, float dt);
    void AbsorbDoomed(Enemy& victim);
    Vector2 DirToPoint(Enemy& e, Vector2 goal, float dt);
    Vector2 DirToPlayer(Enemy& e);
    Vector2 DirAwayFromPlayer(Enemy& e);
    void Steer(Enemy& e, Vector2 dir, float speed, float dt, bool face = true);
    void Face(Enemy& e, Vector2 dir, float dt);
    bool WalkLine(Vector2 a, Vector2 b, float r) const; // un corpo di raggio r passa in linea retta?
    Vector2 PredictPlayer(float seconds) const;
    void EnemyShoot(const Enemy& e, Vector2 dir, float speed, float radius, float damage, EnemyShot look);
    const Enemy* AwakeBoss() const;
    void DrawBossBar() const;
    void UpdatePickups();
    void Attack();
    void MeleeSwing(const struct Weapon& w);
    void Explode(Vector2 pos, float height, float damage);
    void UpdateInventoryChoice();
    void ResolveInventoryChoice(int row);

    // Menu di pausa (PauseMenu.cpp)
    void OpenPause();
    void UpdatePause();
    void DrawPause() const;
    void ToggleFullscreenMode();

    // Menu di partita: inventario, armi, mappa esplorata, statistiche (MatchMenu.cpp)
    void OpenMatchMenu();
    void UpdateMatchMenu();
    void UpdateExploration(float dt);
    void DrawMatchMenu() const;
    void DrawMatchInventory(Rectangle area) const;
    void DrawMatchWeapons(Rectangle area) const;
    void DrawMatchMap(Rectangle area) const;
    void DrawMatchStats(Rectangle area) const;

    // Menu Controller (ControllerMenu.cpp)
    void UpdateControllerMenu();
    void OpenControllerMenu(const std::string& message);
    void CloseControllerMenu();
    void GoToPage(MenuPage page);
    void UpdateDevicesPage();
    void UpdateNewControllerPage();
    void UpdateBluetoothPage();
    void UpdateWiredPage();
    void SnapshotPads();
    int NewlyAvailablePad() const;
    void ConnectNewPad(int pad);
    void ShowToast(const std::string& text);

    int NearestPickup(float maxDist, bool weapons) const;
    void TryPickup(int index);
    void TakeWeapon(int index);
    void DamageEnemy(Enemy& e, float dmg, bool crit);
    void KillEnemy(Enemy& e);
    void DamagePlayer(float dmg);
    bool CircleHitsWall(Vector2 p, float r) const;
    bool Blocked(Vector2 p, float r, float z) const; // muri e oggetti
    void MoveWithCollision(Vector2& pos, float r, Vector2 delta, float z = 0);
    void AddText(Vector2 pos, const std::string& text, Color color);
    void AddPlayerText(const std::string& text, Color color);
    void UpdateCursorLock();
    void SyncCamera();

    void DrawWorld() const;
    void DrawEnemy(const Enemy& e) const;
    void DrawExplosions() const;
    void DrawWorldOverlay() const; // barre della vita e testi sopra le cose visibili
    void DrawFirstPerson() const;  // mirino, arma in mano, lampo rosso quando si e' colpiti
    void DrawHud() const;
    void DrawIntroTitle(const District& d) const;
    void DrawInventoryChoice() const;
    void DrawGameOver() const;
    void DrawControllerMenu() const;
    void DrawDevicesPage() const;
    void DrawNewControllerPage() const;
    void DrawBluetoothPage() const;
    void DrawWiredPage() const;
    void DrawToast() const;
    std::string ControlsHint() const;

    Dungeon dungeon;
    CityProps props;
    State state = State::Playing;
    int floor = 0;
    int kills = 0;
    int nextEnemyId = 0;
    bool quit = false;

    Player player;
    Stats stats;
    std::vector<int> inventory; // id degli artefatti posseduti

    std::vector<Enemy> enemies;
    std::vector<Swarm> swarms;
    Nav nav;
    GameMaster gm;
    Vector2 playerVel{};     // i nemici la usano per anticiparti e per capire se stai scappando
    Vector2 prevPlayerPos{};
    std::vector<Projectile> projectiles;
    std::vector<Pickup> pickups;
    std::vector<FloatingText> texts;
    std::vector<Explosion> explosions;

    int pendingPickup = -1; // artefatto in attesa quando l'inventario e' pieno
    int choiceIndex = 0;    // riga selezionata nella schermata "inventario pieno"
    float introTimer = 0;

    Input input;
    int pauseIndex = 0;
    bool pauseShowControls = false; // pagina "Comandi" del menu di pausa
    State pauseReturnState = State::Playing;
    int matchTab = TAB_INVENTORY;

    // Mappa: le caselle che hai gia' visto (solo quelle compaiono nel menu di partita)
    std::vector<uint8_t> explored;
    float exploreTimer = 0;

    // Storia: introduzione all'avvio e risveglio quando si muore (Cutscene.cpp)
    void UpdateIntro(float dt);
    float cutsceneTime = 0;
    float deathTime = 0;
    float fadeFromBlack = 0; // dissolvenza all'inizio della partita, dopo l'intro

    // Tutorial del GameMaster (Tutorial.cpp). Cosa il giocatore ha gia' imparato vale per tutta la sessione
    void UpdateTutorials(float dt);
    void DrawTutorial() const;
    void StartTip(int tip);
    bool TipDone(int tip) const;
    int activeTip = -1;
    float tipTime = 0, tipPraise = 0, tipCooldown = 0;
    int tipShown[TIP_COUNT] = {};
    float tipAgainAt[TIP_COUNT] = {}; // un consiglio ignorato puo' tornare, ma non subito
    bool usedMove = false, usedTurn = false, usedAttack = false, usedJump = false, usedDash = false;
    bool usedSwap = false, usedPickup = false, usedMatchMenu = false;
    float threatTime = 0;      // secondi sotto attacco senza mai attaccare
    float pickupLinger = 0;    // secondi accanto a un oggetto senza raccoglierlo
    float lowObstacleTime = 0; // secondi bloccato contro qualcosa di basso, che si potrebbe saltare
    float twoWeaponsTime = 0;
    int hitsWithoutDash = 0;
    bool guideWasActive = false;

    // Guida: se vaghi a lungo senza scoprire niente, una freccia indica la strada per il torii (Guide.cpp)
    void UpdateGuide(float dt);
    void DrawGuide() const;
    float floorTime = 0;  // secondi passati su questo piano
    float lostTimer = 0;  // secondi senza scoprire caselle nuove
    bool guideActive = false;
    float guideRepath = 0;
    Vector2 guideDir{};   // direzione sulla mappa verso il prossimo tratto di strada
    float guideDistance = 0; // caselle di strada che mancano

    // Statistiche della run
    float runTime = 0;
    int killsByType[4] = {}; // nell'ordine di EnemyType
    float damageDealt = 0, damageTaken = 0;
    int artifactsFound = 0;
    State menuReturnState = State::Playing; // dove tornare quando si chiude il menu Controller
    int menuIndex = 0;
    std::string menuMessage;
    MenuPage menuPage = MenuPage::Devices;
    int pageIndex = 0;          // voce selezionata nelle pagine secondarie
    bool wiredIsDongle = false; // pagina Wired: pennetta/ricevitore invece del cavo
    bool padsBefore[Input::MAX_PADS] = {}; // joystick gia' presenti, per riconoscere quello nuovo
    std::unique_ptr<ControllerLink> link;  // ricerca e abbinamento (dipende dalla piattaforma)
    std::string toast; // messaggio temporaneo in alto (es. "Joystick collegato")
    float toastTimer = 0;

    Camera3D camera{};
    bool cursorLocked = false; // in partita il mouse e' "catturato" e serve a girarsi
    bool skipMouseDelta = false;
    float walkCycle = 0;  // oscillazione della vista mentre si cammina
    float recoil = 0;     // rinculo dell'arma quando si spara
    float swingTimer = 0; // fendente della katana in corso
    float hurtTimer = 0;  // lampo rosso ai bordi quando si viene colpiti
    float fireTick = 0;   // il fuoco scotta a intervalli, non a ogni frame
};
