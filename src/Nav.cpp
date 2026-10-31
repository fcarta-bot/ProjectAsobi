#include "Nav.hpp"
#include <algorithm>
#include <cmath>
#include <queue>
#include "raymath.h"

constexpr float UNREACHABLE = 1e9f;
static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
static const int DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

static Vector2 TileCenter(int tx, int ty) { return { (tx + 0.5f) * TILE, (ty + 0.5f) * TILE }; }
static int TileOf(float v) { return (int)std::floor(v / TILE); }

void Nav::Build(const Dungeon& dungeon, const CityProps& props) {
    width = dungeon.Width();
    height = dungeon.Height();
    walk.assign(width * height, 0);
    cost.assign(width * height, 1.0f);
    flow.assign(width * height, UNREACHABLE);
    flowTile = -1;

    // Una casella e' percorribile se non e' muro e il suo centro non e' dentro un'auto, un albero...
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
            walk[Index(tx, ty)] = !dungeon.IsSolid(tx, ty) && !props.BlocksCircle(TileCenter(tx, ty), 6, 0);

    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx) {
            if (!walk[Index(tx, ty)]) continue;
            for (int k = 0; k < 8; ++k)
                if (!Walkable(tx + DX[k], ty + DY[k])) { cost[Index(tx, ty)] = 1.6f; break; }
        }
}

bool Nav::Walkable(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return false;
    return walk[Index(tx, ty)];
}

bool Nav::WalkableAt(Vector2 p) const { return Walkable(TileOf(p.x), TileOf(p.y)); }

Vector2 Nav::NearestWalkable(Vector2 p) const {
    int cx = TileOf(p.x), cy = TileOf(p.y);
    if (Walkable(cx, cy)) return TileCenter(cx, cy);
    Vector2 best = p;
    float bestDist = 1e9f;
    for (int ring = 1; ring <= 5 && bestDist >= 1e9f; ++ring)
        for (int ty = cy - ring; ty <= cy + ring; ++ty)
            for (int tx = cx - ring; tx <= cx + ring; ++tx) {
                if (!Walkable(tx, ty)) continue;
                float d = Vector2DistanceSqr(TileCenter(tx, ty), p);
                if (d < bestDist) { bestDist = d; best = TileCenter(tx, ty); }
            }
    return best;
}

float Nav::StepCost(int from, int to, bool diagonal) const {
    return (diagonal ? 1.4142f : 1.0f) * (cost[from] + cost[to]) * 0.5f;
}

// Dijkstra dalla casella del giocatore verso tutta la mappa
void Nav::UpdateFlow(Vector2 target) {
    Vector2 t = NearestWalkable(target); // se sei sopra un'auto, conta la casella libera piu' vicina
    int start = Index(TileOf(t.x), TileOf(t.y));
    if (start == flowTile || walk.empty()) return;
    flowTile = start;
    std::fill(flow.begin(), flow.end(), UNREACHABLE);

    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    flow[start] = 0;
    open.push({ 0, start });
    while (!open.empty()) {
        auto [d, i] = open.top();
        open.pop();
        if (d > flow[i]) continue;
        int x = i % width, y = i / width;
        for (int k = 0; k < 8; ++k) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!Walkable(nx, ny)) continue;
            bool diagonal = k >= 4;
            if (diagonal && (!Walkable(nx, y) || !Walkable(x, ny))) continue; // niente angoli tagliati
            int n = Index(nx, ny);
            float nd = d + StepCost(i, n, diagonal);
            if (nd < flow[n]) {
                flow[n] = nd;
                open.push({ nd, n });
            }
        }
    }
}

float Nav::FlowDistance(Vector2 from) const {
    int tx = TileOf(from.x), ty = TileOf(from.y);
    float best = Walkable(tx, ty) ? flow[Index(tx, ty)] : UNREACHABLE;
    if (best >= UNREACHABLE) {
        for (int k = 0; k < 8; ++k)
            if (Walkable(tx + DX[k], ty + DY[k])) best = std::min(best, flow[Index(tx + DX[k], ty + DY[k])] + 1);
    }
    return best >= UNREACHABLE ? -1.0f : best * TILE;
}

// La casella vicina piu' vicina al giocatore (closer) o piu' lontana da lui
int Nav::BestNeighbor(Vector2 from, bool closer) const {
    int x = TileOf(from.x), y = TileOf(from.y);
    bool here = Walkable(x, y);
    float current = here ? flow[Index(x, y)] : (closer ? UNREACHABLE : -1.0f);
    int best = -1;
    float bestValue = current;
    for (int k = 0; k < 8; ++k) {
        int nx = x + DX[k], ny = y + DY[k];
        if (!Walkable(nx, ny)) continue;
        if (here && k >= 4 && (!Walkable(nx, y) || !Walkable(x, ny))) continue;
        float v = flow[Index(nx, ny)];
        if (v >= UNREACHABLE) continue;
        if (closer ? v < bestValue : v > bestValue) {
            bestValue = v;
            best = Index(nx, ny);
        }
    }
    return best;
}

Vector2 Nav::TowardTarget(Vector2 from) const {
    int n = BestNeighbor(from, true);
    if (n < 0) return { 0, 0 };
    return Vector2Normalize(Vector2Subtract(TileCenter(n % width, n / width), from));
}

Vector2 Nav::AwayFromTarget(Vector2 from) const {
    int n = BestNeighbor(from, false);
    if (n < 0) return { 0, 0 };
    return Vector2Normalize(Vector2Subtract(TileCenter(n % width, n / width), from));
}

// A* classico sulla griglia, con gli stessi costi della mappa delle distanze
std::vector<Vector2> Nav::FindPath(Vector2 from, Vector2 to) const {
    std::vector<Vector2> path;
    if (walk.empty()) return path;
    int sx = TileOf(from.x), sy = TileOf(from.y);
    if (sx < 0 || sy < 0 || sx >= width || sy >= height) return path;
    Vector2 g = NearestWalkable(to);
    int gx = TileOf(g.x), gy = TileOf(g.y);
    if (!Walkable(gx, gy)) return path;
    int start = Index(sx, sy), goal = Index(gx, gy);
    if (start == goal) return path;

    auto heuristic = [&](int i) {
        float dx = fabsf((float)(i % width - gx)), dy = fabsf((float)(i / width - gy));
        return std::max(dx, dy) + 0.4142f * std::min(dx, dy);
    };
    std::vector<float> gCost(width * height, UNREACHABLE);
    std::vector<int> parent(width * height, -1);
    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    gCost[start] = 0;
    open.push({ heuristic(start), start });

    while (!open.empty()) {
        auto [f, i] = open.top();
        open.pop();
        if (i == goal) break;
        if (f - heuristic(i) > gCost[i] + 0.001f) continue; // voce vecchia
        int x = i % width, y = i / width;
        bool here = walk[i];
        for (int k = 0; k < 8; ++k) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!Walkable(nx, ny)) continue;
            bool diagonal = k >= 4;
            if (diagonal && (!Walkable(nx, y) || !Walkable(x, ny))) continue;
            int n = Index(nx, ny);
            float ng = gCost[i] + (here ? StepCost(i, n, diagonal) : (diagonal ? 1.4142f : 1.0f));
            if (ng < gCost[n]) {
                gCost[n] = ng;
                parent[n] = i;
                open.push({ ng + heuristic(n), n });
            }
        }
    }
    if (parent[goal] < 0) return path;
    for (int i = goal; i != start; i = parent[i]) path.push_back(TileCenter(i % width, i / width));
    std::reverse(path.begin(), path.end());
    return path;
}
