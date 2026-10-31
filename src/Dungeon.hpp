#pragma once
#include <cstdint>
#include <vector>
#include "raylib.h"

constexpr int TILE = 32; // dimensione di una casella in pixel

// Nel mondo 3D una casella misura 1 unita': la logica resta sulla mappa in pixel,
// e queste costanti servono solo per disegnarla in prima persona
constexpr float WALL_HEIGHT = 3.0f; // i muri del labirinto sono enormi: non si vede oltre
constexpr float VIEW_DIST = 18.0f;  // oltre questa distanza tutto sparisce nella foschia

// Converte un punto della mappa (pixel) in un punto 3D all'altezza indicata
inline Vector3 To3D(Vector2 p, float height) { return { p.x / TILE, height, p.y / TILE }; }

// Sfuma un colore verso quello della foschia in base alla distanza (in caselle)
Color ApplyFog(Color c, Color fog, float dist);
Color Mix(Color a, Color b, float t);
Color Shade(Color c, float k);

// Numero pseudo-casuale stabile per una casella: serve a decorare sempre allo stesso modo
unsigned TileHash(int x, int y, unsigned seed);

enum class Tile : uint8_t { Wall, Floor, Exit };

// Cosa c'era sotto il labirinto prima che cadesse sulla citta'
enum class Ground : uint8_t {
    Road,      // asfalto
    LineX,     // asfalto con la linea di mezzeria (strada che corre lungo x)
    LineZ,     // idem, strada lungo z
    CrossX,    // strisce pedonali su una strada lungo x
    CrossZ,
    Sidewalk,  // marciapiede
    Open,      // ghiaia, prato o piazza, a seconda del distretto
};

struct Room {
    int x, y, w, h; // in caselle
    int CenterX() const { return x + w / 2; }
    int CenterY() const { return y + h / 2; }
    Vector2 Center() const { return { (CenterX() + 0.5f) * TILE, (CenterY() + 0.5f) * TILE }; }
};

enum class DistrictId { Kyoto, Nara, Osaka, Tokyo };
enum class Skyline { Temples, Hills, Downtown, Skyscrapers };

// Ogni piano e' una zona di una citta' giapponese su cui e' precipitato il labirinto.
struct District {
    DistrictId id;
    const char* city;
    const char* place;
    Color skyTop, horizon;   // cielo in alto e all'orizzonte: l'orizzonte e' anche il colore della foschia
    Color road, sidewalk, open;
    Color stone;             // pietra del labirinto
    Color foliage;           // chiome degli alberi
    Color accent;            // torii d'uscita e dettagli
    bool night;              // di notte finestre e insegne sono accese
    Skyline skyline;
    int facadeChance;        // % di facce di muro che hanno inglobato un pezzo di edificio
};
const District& GetDistrict(int floor);

class Dungeon {
public:
    void Generate(int width, int height, unsigned seed);

    Tile At(int tx, int ty) const;
    bool IsSolid(int tx, int ty) const { return At(tx, ty) == Tile::Wall; }
    bool IsSolidAt(Vector2 worldPos) const;
    bool HasLineOfSight(Vector2 a, Vector2 b) const; // nessun muro tra i due punti
    bool InRoom(int tx, int ty) const;               // false nei corridoi
    Ground GroundAt(int tx, int ty) const;

    int Width() const { return width; }
    int Height() const { return height; }
    unsigned Seed() const { return seed; }
    const std::vector<Room>& Rooms() const { return rooms; }
    Vector2 ExitPos() const { return exitPos; }

    // Disegna in 3D il suolo della citta', i muri del labirinto e il torii d'uscita (dentro BeginMode3D).
    // "forward" e' la direzione dello sguardo sulla mappa, serve a saltare cio' che sta alle spalle
    void Draw3D(const District& d, Vector3 eye, Vector2 forward) const;

private:
    void Carve(int x, int y, int w, int h, Tile t = Tile::Floor);
    void Corridor(const Room& a, const Room& b, bool horizontalFirst);
    void DrawGround(const District& d, int tx, int tz, float dist) const;
    void DrawWall(const District& d, int tx, int tz, int nx, int nz, float dist) const;

    int width = 0, height = 0;
    unsigned seed = 0;
    int roadOffsetX = 0, roadOffsetY = 0; // spostano il reticolo delle strade a ogni piano
    std::vector<Tile> tiles;
    std::vector<Room> rooms;
    Vector2 exitPos{};
};
