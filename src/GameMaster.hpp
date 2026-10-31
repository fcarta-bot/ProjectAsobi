#pragma once

class Game;
enum class EnemyType;

// Il GameMaster e' il "regista" invisibile della partita.
// Per ora prepara i nemici di ogni piano (sciami di goblin, squadre di robot, tengu e l'oni
// che fa la guardia al torii) e comanda le squadre di robot: pattuglie, accerchiamenti, ricerche.
// Piu' avanti osservera' come giochi, se lo ricordera' tra una sessione e l'altra
// e adattera' la partita (piu' facile se ti blocchi sempre nello stesso punto, piu' dura se vinci troppo).
class GameMaster {
public:
    void PopulateFloor(Game& g);
    void Update(Game& g, float dt);

private:
    void SpawnSwarm(Game& g, EnemyType type, int roomIndex, int size);
    void DirectSquad(Game& g, int squad, float dt);
    void Patrol(Game& g, int squad);
    void Hunt(Game& g, int squad);
    void Search(Game& g, int squad);
};
