// Il giro del mondo del GameMaster: 16 citta' in 7 paesi. Ogni piano ne pesca una a caso.
#include "Dungeon.hpp"
#include <algorithm>
#include <random>

using D = DistrictId;
using C = Country;
using F = Facade;
using T = TreeStyle;

const District& GetDistrict(int index) {
    //   id, paese, citta', luogo,
    //   cielo in alto, orizzonte (= foschia), asfalto, marciapiede, terreno libero,
    //   pietra del labirinto, chiome, colore d'accento (torii e dettagli),
    //   notte, facciate, % facciate, alberi, citta' fitta, sole, neve
    static const District districts[DISTRICT_COUNT] = {
        // --- Giappone ---
        { D::Kyoto, C::Japan, "Kyoto", "Fushimi Inari al tramonto",
          { 70, 50, 110, 255 }, { 232, 140, 100, 255 }, { 62, 60, 64, 255 }, { 150, 140, 126, 255 }, { 172, 160, 136, 255 },
          { 92, 88, 94, 255 }, { 196, 64, 40, 255 }, { 230, 70, 40, 255 },
          false, F::Machiya, 12, T::Round, false, 4.0f, false },
        { D::Nara, C::Japan, "Nara", "Parco dei cervi",
          { 92, 150, 220, 255 }, { 205, 222, 232, 255 }, { 72, 72, 76, 255 }, { 160, 154, 140, 255 }, { 98, 142, 72, 255 },
          { 100, 104, 96, 255 }, { 72, 132, 62, 255 }, { 240, 200, 120, 255 },
          false, F::Temple, 8, T::Round, false, -1, false },
        { D::Osaka, C::Japan, "Osaka", "Dotonbori di sera",
          { 30, 20, 62, 255 }, { 150, 72, 112, 255 }, { 50, 48, 56, 255 }, { 112, 106, 112, 255 }, { 126, 112, 102, 255 },
          { 84, 80, 92, 255 }, { 64, 116, 72, 255 }, { 255, 200, 40, 255 },
          true, F::Neon, 26, T::Round, true, -1, false },
        { D::Tokyo, C::Japan, "Tokyo", "Shibuya di notte",
          { 8, 10, 26, 255 }, { 40, 46, 82, 255 }, { 38, 38, 44, 255 }, { 86, 86, 96, 255 }, { 96, 96, 106, 255 },
          { 72, 74, 86, 255 }, { 214, 182, 60, 255 }, { 60, 230, 255, 255 },
          true, F::Neon, 28, T::Round, true, -1, false },
        // --- Stati Uniti ---
        { D::NewYork, C::USA, "New York", "Times Square di notte",
          { 12, 12, 30, 255 }, { 78, 56, 92, 255 }, { 40, 40, 46, 255 }, { 112, 108, 112, 255 }, { 100, 100, 104, 255 },
          { 86, 84, 92, 255 }, { 70, 110, 60, 255 }, { 255, 210, 40, 255 },
          true, F::Manhattan, 30, T::Round, true, -1, false },
        { D::NewOrleans, C::USA, "New Orleans", "French Quarter al crepuscolo",
          { 60, 40, 90, 255 }, { 196, 118, 112, 255 }, { 58, 56, 58, 255 }, { 150, 106, 88, 255 }, { 74, 98, 62, 255 },
          { 96, 98, 90, 255 }, { 62, 98, 52, 255 }, { 170, 80, 210, 255 },
          false, F::IronBalcony, 22, T::Moss, false, 3.2f, false },
        { D::Chicago, C::USA, "Chicago", "Lincoln Park in autunno",
          { 118, 138, 168, 255 }, { 200, 204, 210, 255 }, { 64, 64, 68, 255 }, { 150, 150, 150, 255 }, { 128, 116, 62, 255 },
          { 100, 98, 104, 255 }, { 212, 112, 40, 255 }, { 40, 120, 200, 255 },
          false, F::Brick, 16, T::Round, false, -1, false },
        { D::LasVegas, C::USA, "Las Vegas", "La Strip di notte",
          { 20, 10, 40, 255 }, { 120, 52, 104, 255 }, { 44, 42, 48, 255 }, { 150, 130, 110, 255 }, { 190, 156, 108, 255 },
          { 110, 96, 90, 255 }, { 62, 128, 62, 255 }, { 255, 60, 180, 255 },
          true, F::Casino, 30, T::Palm, true, -1, false },
        // --- Italia ---
        { D::Rome, C::Italy, "Roma", "Fori Imperiali al tramonto",
          { 90, 70, 130, 255 }, { 248, 168, 110, 255 }, { 78, 72, 68, 255 }, { 186, 176, 156, 255 }, { 150, 140, 92, 255 },
          { 120, 112, 100, 255 }, { 52, 84, 44, 255 }, { 230, 180, 60, 255 },
          false, F::Travertine, 18, T::UmbrellaPine, false, 1.2f, false },
        { D::Venice, C::Italy, "Venezia", "Campo San Polo nella nebbia",
          { 168, 176, 196, 255 }, { 218, 214, 208, 255 }, { 118, 114, 110, 255 }, { 200, 196, 186, 255 }, { 150, 146, 138, 255 },
          { 116, 110, 104, 255 }, { 82, 110, 70, 255 }, { 210, 170, 60, 255 },
          false, F::Venetian, 26, T::Round, false, -1, false },
        { D::Naples, C::Italy, "Napoli", "Spaccanapoli a mezzogiorno",
          { 78, 140, 230, 255 }, { 222, 222, 204, 255 }, { 66, 64, 64, 255 }, { 160, 150, 130, 255 }, { 170, 150, 112, 255 },
          { 108, 100, 96, 255 }, { 70, 120, 60, 255 }, { 50, 120, 220, 255 },
          false, F::Neapolitan, 34, T::Round, true, -1, false },
        { D::Florence, C::Italy, "Firenze", "Piazza della Signoria al tramonto",
          { 100, 80, 140, 255 }, { 240, 160, 100, 255 }, { 90, 86, 80, 255 }, { 170, 150, 120, 255 }, { 156, 124, 92, 255 },
          { 112, 106, 98, 255 }, { 54, 90, 50, 255 }, { 200, 40, 50, 255 },
          false, F::Florentine, 22, T::Cypress, false, 5.0f, false },
        // --- Francia, Germania, Inghilterra, Spagna ---
        { D::Paris, C::France, "Parigi", "Montmartre di sera",
          { 30, 30, 72, 255 }, { 150, 112, 140, 255 }, { 60, 60, 66, 255 }, { 140, 136, 130, 255 }, { 110, 128, 90, 255 },
          { 104, 102, 108, 255 }, { 80, 122, 60, 255 }, { 70, 100, 210, 255 },
          true, F::Haussmann, 26, T::Round, true, -1, false },
        { D::Munich, C::Germany, "Monaco di Baviera", "Marienplatz d'inverno",
          { 140, 160, 190, 255 }, { 226, 230, 236, 255 }, { 80, 80, 84, 255 }, { 170, 170, 172, 255 }, { 232, 234, 238, 255 },
          { 104, 104, 110, 255 }, { 40, 80, 52, 255 }, { 60, 120, 220, 255 },
          false, F::Fachwerk, 22, T::Conifer, false, -1, true },
        { D::London, C::England, "Londra", "Westminster nella nebbia",
          { 30, 34, 44, 255 }, { 108, 114, 118, 255 }, { 46, 46, 50, 255 }, { 120, 118, 116, 255 }, { 70, 100, 60, 255 },
          { 90, 90, 96, 255 }, { 60, 90, 50, 255 }, { 220, 30, 40, 255 },
          true, F::LondonBrick, 24, T::Round, true, -1, false },
        { D::Seville, C::Spain, "Siviglia", "Barrio de Santa Cruz di notte",
          { 16, 16, 46, 255 }, { 110, 62, 82, 255 }, { 80, 70, 64, 255 }, { 200, 190, 170, 255 }, { 178, 148, 108, 255 },
          { 120, 108, 96, 255 }, { 60, 110, 50, 255 }, { 240, 170, 30, 255 },
          true, F::Andalusian, 26, T::Orange, false, -1, false },
    };
    return districts[std::clamp(index, 0, DISTRICT_COUNT - 1)];
}

const char* CountryName(Country c) {
    static const char* names[COUNTRY_COUNT] = { "Giappone", "Stati Uniti", "Italia", "Francia", "Germania", "Inghilterra", "Spagna" };
    return names[(int)c];
}

const char* CountryAdjective(Country c) {
    static const char* names[COUNTRY_COUNT] = { "giapponese", "americano", "italiano", "francese", "tedesco", "inglese", "spagnolo" };
    return names[(int)c];
}

std::vector<int> MakeWorldTour(unsigned seed, int floors) {
    std::mt19937 rng(seed);
    std::vector<int> tour;
    while ((int)tour.size() < floors) {
        std::vector<int> round(DISTRICT_COUNT);
        for (int i = 0; i < DISTRICT_COUNT; ++i) round[i] = i;
        std::shuffle(round.begin(), round.end(), rng);
        // Niente stessa citta' due volte di fila tra un giro e l'altro
        if (!tour.empty() && round[0] == tour.back()) std::swap(round[0], round[DISTRICT_COUNT - 1]);
        tour.insert(tour.end(), round.begin(), round.end());
    }
    tour.resize(floors);
    return tour;
}
