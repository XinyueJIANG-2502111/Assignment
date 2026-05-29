#pragma once

#include "Vector.hpp"

#include <vector>
#include <set>
#include <cmath>
#include <cstdlib>


// 前方宣言
class Camera;

// ブロック（AABB）
struct Obstacle {
    float x, y;          // 左上座標
    float width, height;
    unsigned int color;

    float Left() const { return x; }
    float Right() const { return x + width; }
    float Top() const { return y; }
    float Bottom() const { return y + height; }
};

// 当たり判定用
struct RaycastResult {
    bool hit;           // 当たっているかどうか
    Vector2 hitPoint;   // 当たった場所の座標
    float t;            // 時間
};

// 当たった時の情報
struct CollisionResult {
    bool collided;
    Vector2 pushVector; // プレイヤーを押し出す
    Vector2 normal;     // 法線ベクトル
};

// アイテム
struct CoreItem {
    float x, y;          // 中心点座標
    float radius;        // 半径
    unsigned int color;
    bool isCollected;
    float pulseTimer;    // アニメーション用
};

// エフェクト用
struct EffectParticle {
    Vector2 pos;       // 座標
    Vector2 velocity;  // 移動方向
    float alpha;       // 不透明度
    unsigned int color;
};

// 当たり判定（ワイヤーがブロックに当たっているかどうか）
inline RaycastResult RaycastToObstacle(const Vector2& rayStart, const Vector2& rayDir, float maxDist, const Obstacle& obs) {
    RaycastResult result = { false, {0,0}, maxDist };

    float tXMin = (obs.Left() - rayStart.x) / rayDir.x;
    float tXMax = (obs.Right() - rayStart.x) / rayDir.x;
    if (tXMin > tXMax) std::swap(tXMin, tXMax);

    float tYMin = (obs.Top() - rayStart.y) / rayDir.y;
    float tYMax = (obs.Bottom() - rayStart.y) / rayDir.y;
    if (tYMin > tYMax) std::swap(tYMin, tYMax);

    if (tXMin > tYMax || tYMin > tXMax) return result;

    float tNear = (std::max)(tXMin, tYMin);
    float tFar = (std::min)(tXMax, tYMax);

    if (tFar < 0.0f) return result;

    if (tNear > maxDist || tNear < 0.0f) return result;

    result.hit = true;
    result.t = tNear;
    result.hitPoint = rayStart + rayDir * tNear;
    return result;
}

// プレイヤーの当たり判定
inline CollisionResult CheckPlayerCollision(const Vector2& playerPos, float radius, const Obstacle& obs) {
    CollisionResult result = { false, {0,0}, {0,0} };

    float closestX = (std::max)(obs.Left(), (std::min)(playerPos.x, obs.Right()));
    float closestY = (std::max)(obs.Top(), (std::min)(playerPos.y, obs.Bottom()));

    Vector2 diff = playerPos - Vector2{ closestX, closestY };
    float distance = diff.Length();

    if (distance < radius && distance > 0.001f) {
        result.collided = true;
        float overlap = radius - distance;
        result.normal = diff.Normalized();
        result.pushVector = result.normal * overlap;
    }
    else if (distance == 0.0f) {
        result.collided = true;
        float dl = playerPos.x - obs.Left();
        float dr = obs.Right() - playerPos.x;
        float dt = playerPos.y - obs.Top();
        float db = obs.Bottom() - playerPos.y;
        float minVal = (std::min)({ dl, dr, dt, db });

        if (minVal == dl) { result.pushVector = { -(radius + dl), 0 }; result.normal = { -1, 0 }; }
        else if (minVal == dr) { result.pushVector = { radius + dr, 0 };  result.normal = { 1, 0 }; }
        else if (minVal == dt) { result.pushVector = { 0, -(radius + dt) }; result.normal = { 0, -1 }; }
        else { result.pushVector = { 0, radius + db };  result.normal = { 0, 1 }; }
    }

    return result;
}



// マップを管理するクラス
class TerrainManager {
public:
    const float CHUNK_SIZE = 400.0f; // ゾーン範囲

    // ロード済のゾーン
    std::set<std::pair<int, int>> loadedChunks;

public:
    std::vector<Obstacle> obstacles;
    std::vector<CoreItem> cores;

    float lastGeneratedX = 300.0f;
    const float CHUNK_WIDTH = 400.0f;

    // エフェクト用
    std::vector<EffectParticle> particles;

    // エフェクトを生成
    void SpawnExplosion(Vector2 spawnPos, unsigned int coreColor);

    // 初期化
    void Init();

    // 周りのマップをロードする
    void UpdateAllDirections(Vector2 cameraPos);

private:
    // ブロック生成
    void GenerateChunk(int cx, int cy);

public:
    // 描画
    void Draw(const Camera& camera);
};