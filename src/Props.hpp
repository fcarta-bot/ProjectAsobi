#pragma once
#include <vector>
#include "Dungeon.hpp"

// Oggetti della citta' rimasti nel labirinto: auto schiacciate o tagliate dai muri, alberi,
// cestini, distributori... Alcuni bloccano il passaggio, quelli bassi si saltano o ci si sale sopra.
enum class PropType {
    Car,            // auto intera, a volte schiacciata
    CarHalf,        // mezza auto: l'altra meta' e' finita dentro il muro
    Tree,
    FallenTree,     // tronco a terra: si scavalca col salto
    Bin,            // cestino (a volte rovesciato)
    Vending,        // distributore automatico (a volte a terra)
    Cone,
    Lamp,           // lampione, spesso piegato
    Bench,
    Rubble,         // macerie cadute dal labirinto
    StoneLantern,   // toro di pietra (Kyoto, Nara)
    Torii,          // i mille torii di Fushimi Inari, a cavallo dei corridoi
    Deer,           // i cervi di Nara: sono sopravvissuti
    Yatai,          // bancarella di cibo (in Giappone con le lanterne, altrove carretto o chiosco)
    // Giro del mondo
    Hydrant,        // idrante (Stati Uniti)
    NewsBox,        // distributore di giornali (Stati Uniti)
    Scooter,        // motorino (Italia, Spagna)
    CafeTable,      // tavolino di un caffe' con due sedie (Italia, Parigi, Venezia)
    PhoneBox,       // cabina telefonica rossa (Londra)
    PostBox,        // cassetta delle lettere rossa (Londra)
    Column,         // colonna romana spezzata, a terra (Roma)
    Fountain,       // fontanella (il "nasone" di Roma) o vera da pozzo
    Statue,         // statua su piedistallo (Firenze)
    MorrisColumn,   // colonna delle affissioni (Parigi)
    Snowman,        // pupazzo di neve (Monaco d'inverno)
};

struct Prop {
    PropType type;
    Vector2 pos;          // centro, in pixel sulla mappa
    Vector2 half;         // mezze dimensioni del riquadro di collisione, in pixel
    float height = 0;     // altezza in caselle: sotto questa quota l'oggetto blocca
    bool solid = true;
    bool alongX = true;   // orientamento (auto, panchine, tronchi...)
    bool flipped = false; // verso (es. muso dell'auto, lato del muro)
    bool knocked = false; // rovesciato, schiacciato o piegato
    bool flammable = false;
    bool burning = false;
    bool charred = false; // bruciato e spento
    float fireTime = -1;  // secondi di fuoco rimasti (-1 = brucia per sempre)
    Color color{};
    unsigned seed = 0;
};

class CityProps {
public:
    void Generate(const Dungeon& dungeon, const District& d, Vector2 start);
    void Update(float dt);

    // Collisioni. z e' la quota dei piedi di chi si muove: gli oggetti piu' bassi si scavalcano
    bool BlocksCircle(Vector2 p, float r, float z) const;
    float GroundHeight(Vector2 p, float r, float z) const; // quota su cui si poggia (0 = suolo)
    bool BlocksShot(Vector2 p, float height) const;
    bool Overlaps(Vector2 p, float r) const; // per non far nascere nemici o oggetti dentro le cose

    // Fuoco: solo gli oggetti infiammabili possono bruciare
    int Ignite(Vector2 p, float radius);               // restituisce quanti ne ha incendiati
    bool NearFire(Vector2 p, float r, float z) const;  // abbastanza vicino da scottarsi

    void Draw(const District& d, Vector3 eye, Vector2 forward) const;
    void DrawFires(Vector3 eye) const; // dopo tutto il resto: fumo e fiamme sono trasparenti

private:
    std::vector<Prop> props;
};
