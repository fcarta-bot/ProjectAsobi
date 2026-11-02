// Salvataggi: tre partite e il profilo (titoli e impostazioni).
// Su PC sono file di testo nella cartella "salvataggi" accanto al gioco; nel browser (iPad)
// stanno nella memoria del sito (localStorage), cosi' restano anche chiudendo la pagina.
// Si salva all'inizio di ogni piano: ricaricando, si riparte dall'inizio di quel piano.
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include "Game.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

// I dati sono solo lettere, numeri e pochi simboli: si possono passare a JavaScript tra apici
static void StorageWrite(const std::string& key, const std::string& data) {
    std::string flat = data;
    for (char& c : flat) if (c == '\n') c = '|';
    emscripten_run_script(("try{localStorage.setItem('asobi_" + key + "','" + flat + "')}catch(e){}").c_str());
}
static std::string StorageRead(const std::string& key) {
    std::string s = emscripten_run_script_string(("(function(){try{return localStorage.getItem('asobi_" + key + "')||''}catch(e){return ''}})()").c_str());
    for (char& c : s) if (c == '|') c = '\n';
    return s;
}
static void StorageDelete(const std::string& key) {
    emscripten_run_script(("try{localStorage.removeItem('asobi_" + key + "')}catch(e){}").c_str());
}

#else
#include <filesystem>
#include <fstream>

static std::filesystem::path StoragePath(const std::string& key) {
    std::filesystem::path dir = std::filesystem::path(GetApplicationDirectory()) / "salvataggi";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / (key + ".txt");
}
static void StorageWrite(const std::string& key, const std::string& data) {
    std::ofstream f(StoragePath(key), std::ios::trunc);
    f << data;
}
static std::string StorageRead(const std::string& key) {
    std::ifstream f(StoragePath(key));
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static void StorageDelete(const std::string& key) {
    std::error_code ec;
    std::filesystem::remove(StoragePath(key), ec);
}
#endif

// Righe "chiave=valore"
static std::map<std::string, std::string> Parse(const std::string& text) {
    std::map<std::string, std::string> m;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) m[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return m;
}
static double Num(const std::map<std::string, std::string>& m, const char* key, double fallback = 0) {
    auto it = m.find(key);
    return it == m.end() || it->second.empty() ? fallback : std::strtod(it->second.c_str(), nullptr);
}
static std::vector<int> List(const std::map<std::string, std::string>& m, const char* key) {
    std::vector<int> out;
    auto it = m.find(key);
    if (it == m.end()) return out;
    std::istringstream in(it->second);
    std::string item;
    while (std::getline(in, item, ',')) if (!item.empty()) out.push_back(std::atoi(item.c_str()));
    return out;
}
template <typename T> static std::string Join(const T* v, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += (i ? "," : "") + std::to_string(v[i]);
    return s;
}

static std::string SlotKey(int slot) { return "partita" + std::to_string(slot + 1); }

// ---------------------------------------------------------------------------
// Profilo
// ---------------------------------------------------------------------------

void Game::LoadProfile() {
    auto m = Parse(StorageRead("profilo"));
    profile.titles = (unsigned)Num(m, "titoli");
    profile.lookSensitivity = (float)Num(m, "sensibilita", 1);
    profile.tips = Num(m, "consigli", 1) != 0;
    profile.bestFloor = (int)Num(m, "piano_record");
    profile.victories = (int)Num(m, "vittorie");
    if (m.count("touch")) input.Touch().LoadSetup(m["touch"]);
    for (int i = 0; i < SAVE_SLOTS; ++i) slotInfo[i] = ReadSlotInfo(i);
}

void Game::SaveProfile() const {
    std::ostringstream o;
    o << "titoli=" << profile.titles << "\n"
      << "sensibilita=" << profile.lookSensitivity << "\n"
      << "consigli=" << (profile.tips ? 1 : 0) << "\n"
      << "piano_record=" << profile.bestFloor << "\n"
      << "vittorie=" << profile.victories << "\n"
      << "touch=" << input.Touch().SaveSetup() << "\n";
    StorageWrite("profilo", o.str());
}

// ---------------------------------------------------------------------------
// Partite
// ---------------------------------------------------------------------------

SlotInfo Game::ReadSlotInfo(int s) const {
    auto m = Parse(StorageRead(SlotKey(s)));
    SlotInfo info;
    info.used = Num(m, "piano") > 0;
    info.floor = (int)Num(m, "piano");
    info.runTime = (float)Num(m, "tempo");
    info.kills = (int)Num(m, "uccisioni");
    if (info.used) info.district = MakeWorldTour((unsigned)Num(m, "giro"), MAX_FLOORS)[(info.floor - 1) % MAX_FLOORS];
    return info;
}

void Game::SaveRun() const {
    if (floor <= 0 || !runActive) return; // nel menu iniziale la run "di sfondo" non si salva
    unsigned flags = (meleeBossKill ? 1 : 0) | (empoweredKilled ? 2 : 0) | (fearlessKill ? 4 : 0) | (dahuCaught ? 8 : 0) |
                     (empoweredSpawned ? 16 : 0) | (cleanFloorDone ? 32 : 0) | (explorerDone ? 64 : 0);
    unsigned rewards = 0;
    for (int i = 0; i < REWARD_COUNT; ++i) rewards |= (rewardDone[i] ? 1u : 0u) << i;
    std::ostringstream o;
    o << "piano=" << floor << "\n"
      << "seme=" << floorSeed << "\n"
      << "giro=" << tourSeed << "\n"
      << "vita=" << player.hp << "\n"
      << "armi=" << player.weapons[0] << "," << player.weapons[1] << "\n"
      << "arma_in_mano=" << player.activeSlot << "\n"
      << "inventario=" << Join(inventory.data(), (int)inventory.size()) << "\n"
      << "uccisioni=" << kills << "\n"
      << "per_tipo=" << Join(killsByType, ENEMY_TYPE_COUNT) << "\n"
      << "tempo=" << runTime << "\n"
      << "danno_fatto=" << damageDealt << "\n"
      << "danno_subito=" << damageTaken << "\n"
      << "artefatti=" << artifactsFound << "\n"
      << "premi=" << rewards << "\n"
      << "sfide=" << Join(victoryPicks, VICTORY_PICKS) << "\n"
      << "guardiani=" << bossesKilled << "\n"
      << "segni=" << flags << "\n"
      << "serie=" << noDamageStreak << "," << bestNoDamageStreak << "\n"
      << "citta=" << districtsSeen << "\n"
      << "paesi=" << countriesSeen << "\n"
      << "paesi_guardiani=" << bossCountries << "\n"
      << "armi_usate=" << weaponsHeld << "\n"
      << "sinergie=" << bestSynergies << "\n";
    StorageWrite(SlotKey(slot), o.str());
}

bool Game::LoadRun(int s) {
    auto m = Parse(StorageRead(SlotKey(s)));
    if (Num(m, "piano") <= 0) return false;
    slot = s;
    runActive = false;
    NewRun(); // azzera tutto (e genera un piano che poi viene sostituito)
    runActive = true;
    floor = (int)Num(m, "piano");
    tourSeed = (unsigned)Num(m, "giro");
    tour = MakeWorldTour(tourSeed, MAX_FLOORS);
    std::vector<int> w = List(m, "armi");
    player.weapons[0] = w.size() > 0 ? w[0] : W_OFUDA;
    player.weapons[1] = w.size() > 1 ? w[1] : -1;
    player.activeSlot = (int)Num(m, "arma_in_mano");
    if (player.weapons[player.activeSlot] < 0) player.activeSlot = 0;
    inventory = List(m, "inventario");
    kills = (int)Num(m, "uccisioni");
    std::vector<int> byType = List(m, "per_tipo");
    for (int i = 0; i < ENEMY_TYPE_COUNT && i < (int)byType.size(); ++i) killsByType[i] = byType[i];
    runTime = (float)Num(m, "tempo");
    damageDealt = (float)Num(m, "danno_fatto");
    damageTaken = (float)Num(m, "danno_subito");
    artifactsFound = (int)Num(m, "artefatti");
    unsigned rewards = (unsigned)Num(m, "premi");
    for (int i = 0; i < REWARD_COUNT; ++i) rewardDone[i] = (rewards >> i) & 1;
    std::vector<int> picks = List(m, "sfide");
    for (int i = 0; i < VICTORY_PICKS && i < (int)picks.size(); ++i) victoryPicks[i] = picks[i];
    bossesKilled = (int)Num(m, "guardiani");
    unsigned flags = (unsigned)Num(m, "segni");
    meleeBossKill = flags & 1; empoweredKilled = flags & 2; fearlessKill = flags & 4; dahuCaught = flags & 8;
    empoweredSpawned = flags & 16; cleanFloorDone = flags & 32; explorerDone = flags & 64;
    std::vector<int> streak = List(m, "serie");
    noDamageStreak = streak.size() > 0 ? streak[0] : 0;
    bestNoDamageStreak = streak.size() > 1 ? streak[1] : 0;
    districtsSeen = (unsigned)Num(m, "citta");
    countriesSeen = (unsigned)Num(m, "paesi");
    bossCountries = (unsigned)Num(m, "paesi_guardiani");
    weaponsHeld = (uint64_t)Num(m, "armi_usate", 1);
    bestSynergies = (int)Num(m, "sinergie");
    stats = ComputeStats(inventory);
    player.hp = std::min(stats.maxHp, std::max(1.0f, (float)Num(m, "vita", stats.maxHp)));
    BuildFloor((unsigned)Num(m, "seme"));
    return true;
}

// Dal menu di pausa: salva e torna al menu iniziale
void Game::SaveAndQuitToTitle() {
    SaveRun();
    slotInfo[slot] = ReadSlotInfo(slot);
    OpenTitle();
}

void Game::DeleteRun(int s) {
    StorageDelete(SlotKey(s));
    slotInfo[s] = SlotInfo{};
}
