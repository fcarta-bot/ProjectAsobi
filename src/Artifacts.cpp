#include "Artifacts.hpp"
#include <algorithm>

const char* TagName(int i) {
    static const char* names[TAG_COUNT] = { "Fuoco", "Vento", "Spirito", "Acciaio", "Fortuna" };
    return names[i];
}

Color TagColor(int i) {
    static const Color colors[TAG_COUNT] = {
        {255, 110, 60, 255},  // Fuoco
        {120, 220, 200, 255}, // Vento
        {190, 150, 255, 255}, // Spirito
        {180, 190, 205, 255}, // Acciaio
        {255, 210, 70, 255},  // Fortuna
    };
    return colors[i];
}

int FirstTagIndex(unsigned tags) {
    for (int i = 0; i < TAG_COUNT; ++i)
        if (tags & (1u << i)) return i;
    return 0;
}

static bool Has(const std::vector<int>& inv, ArtifactId id) {
    return std::find(inv.begin(), inv.end(), (int)id) != inv.end();
}

const std::vector<Artifact>& AllArtifacts() {
    static const std::vector<Artifact> list = {
        { "Omamori",         "Kyoto",     "+25 vita massima",
          TAG_SPIRITO,               [](Stats& s) { s.maxHp += 25; } },
        { "Maneki-neko",     "Tokyo",     "+10% probabilita' di critico",
          TAG_FORTUNA,               [](Stats& s) { s.critChance += 0.10f; } },
        { "Daruma",          "Takasaki",  "+15 vita, +5% critico",
          TAG_SPIRITO | TAG_FORTUNA, [](Stats& s) { s.maxHp += 15; s.critChance += 0.05f; } },
        { "Lanterna di Gion", "Kyoto",    "I colpi bruciano: 3 danni/s",
          TAG_FUOCO,                 [](Stats& s) { s.burnDps += 3; } },
        { "Ventaglio Sensu", "Kyoto",     "+40 velocita' di movimento",
          TAG_VENTO,                 [](Stats& s) { s.moveSpeed += 40; } },
        { "Furin",           "Edo",       "Lo scatto si ricarica 0.3s prima",
          TAG_VENTO | TAG_SPIRITO,   [](Stats& s) { s.dashCooldown -= 0.3f; } },
        { "Lama Tamahagane", "Seki",      "+6 danno",
          TAG_ACCIAIO,               [](Stats& s) { s.damage += 6; } },
        { "Shuriken di Iga", "Iga",       "+1 proiettile, -15% danno",
          TAG_ACCIAIO | TAG_VENTO,   [](Stats& s) { s.projectiles += 1; s.damageMult *= 0.85f; } },
        { "Kitsune-bi",      "Fushimi",   "+2 bruciatura, 3% rubavita",
          TAG_FUOCO | TAG_SPIRITO,   [](Stats& s) { s.burnDps += 2; s.lifesteal += 0.03f; } },
        { "Takoyaki",        "Osaka",     "+20 vita, +10% cadenza",
          TAG_FUOCO,                 [](Stats& s) { s.maxHp += 20; s.fireRateMult *= 1.10f; } },
        { "Insegna al neon", "Shibuya",   "+30% cadenza di fuoco",
          TAG_FUOCO | TAG_VENTO,     [](Stats& s) { s.fireRateMult *= 1.30f; } },
        { "Orizuru",         "Hiroshima", "+5% rubavita",
          TAG_SPIRITO,               [](Stats& s) { s.lifesteal += 0.05f; } },
        { "Specchio Yata",   "Ise",       "I colpi trapassano 1 nemico in piu'",
          TAG_ACCIAIO | TAG_SPIRITO, [](Stats& s) { s.pierce += 1; } },
        { "Koban d'oro",     "Edo",       "+8% critico, +3 danno",
          TAG_FORTUNA | TAG_ACCIAIO, [](Stats& s) { s.critChance += 0.08f; s.damage += 3; } },
        { "Shika senbei",    "Nara",      "+30 velocita', +10 vita",
          TAG_FORTUNA | TAG_VENTO,   [](Stats& s) { s.moveSpeed += 30; s.maxHp += 10; } },
    };
    return list;
}

const std::vector<Synergy>& AllSynergies() {
    static const std::vector<Synergy> list = {
        // Sinergie di tag: 3 artefatti con lo stesso elemento
        { "Hanabi", "Fuoco x3: bruciatura raddoppiata",
          [](const std::vector<int>& inv) { return CountTag(inv, TAG_FUOCO) >= 3; },
          [](Stats& s) { s.burnDps = s.burnDps * 2 + 2; } },
        { "Kamikaze", "Vento x3: +60 velocita', scatto dimezzato",
          [](const std::vector<int>& inv) { return CountTag(inv, TAG_VENTO) >= 3; },
          [](Stats& s) { s.moveSpeed += 60; s.dashCooldown *= 0.5f; } },
        { "Kami protettore", "Spirito x3: +30 vita, +5% rubavita",
          [](const std::vector<int>& inv) { return CountTag(inv, TAG_SPIRITO) >= 3; },
          [](Stats& s) { s.maxHp += 30; s.lifesteal += 0.05f; } },
        { "Via del Samurai", "Acciaio x3: +25% danno, +1 trapasso",
          [](const std::vector<int>& inv) { return CountTag(inv, TAG_ACCIAIO) >= 3; },
          [](Stats& s) { s.damageMult *= 1.25f; s.pierce += 1; } },
        { "Shichifukujin", "Fortuna x3: +15% critico, critici x3",
          [](const std::vector<int>& inv) { return CountTag(inv, TAG_FORTUNA) >= 3; },
          [](Stats& s) { s.critChance += 0.15f; s.critMult = 3.0f; } },

        // Combo specifiche tra due artefatti
        { "Tempesta di Iga", "Shuriken + Ventaglio: +2 proiettili",
          [](const std::vector<int>& inv) { return Has(inv, A_SHURIKEN) && Has(inv, A_SENSU); },
          [](Stats& s) { s.projectiles += 2; } },
        { "Notte di Obon", "Kitsune-bi + Lanterna: +4 bruciatura, +20% cadenza",
          [](const std::vector<int>& inv) { return Has(inv, A_KITSUNE_BI) && Has(inv, A_LANTERNA_GION); },
          [](Stats& s) { s.burnDps += 4; s.fireRateMult *= 1.2f; } },
        { "Prosperita' di Edo", "Maneki-neko + Koban: +15% drop artefatti",
          [](const std::vector<int>& inv) { return Has(inv, A_MANEKI_NEKO) && Has(inv, A_KOBAN); },
          [](Stats& s) { s.dropChance += 0.15f; } },
    };
    return list;
}

int CountTag(const std::vector<int>& inv, unsigned tag) {
    int n = 0;
    for (int id : inv)
        if (AllArtifacts()[id].tags & tag) ++n;
    return n;
}

std::vector<int> ActiveSynergies(const std::vector<int>& inv) {
    std::vector<int> active;
    const auto& all = AllSynergies();
    for (int i = 0; i < (int)all.size(); ++i)
        if (all[i].isActive(inv)) active.push_back(i);
    return active;
}

Stats ComputeStats(const std::vector<int>& inv) {
    Stats s;
    for (int id : inv) AllArtifacts()[id].apply(s);
    for (int i : ActiveSynergies(inv)) AllSynergies()[i].apply(s);

    // I moltiplicatori si applicano alla fine, cosi' l'ordine degli artefatti non conta
    s.damage *= s.damageMult;
    s.fireRate *= s.fireRateMult;
    s.dashCooldown = std::max(0.25f, s.dashCooldown);
    s.critChance = std::min(s.critChance, 0.9f);
    return s;
}
