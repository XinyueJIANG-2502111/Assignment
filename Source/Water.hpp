#pragma once

#include "Vector.hpp"
#include <vector>

// 前方宣言
class Camera;

// エフェクト（着地用）
struct SplashParticle {
    Vector2 pos;
    Vector2 vel;
    float alpha;
    float size;
    float lifetime;
};

// エフェクト（ゲームオーバー用）
struct BloodParticle {
    Vector2 pos;
    Vector2 vel;
    float size;
    float alpha;
    float maxLifetime;
    float currentLifetime;
    float wavePhase;
};


class Water {
public:
    struct WaterPoint {
        float y;          // 縦軸座標
        float targetY;    // 平衡状態の時の縦軸座標
        float speed;      // 振動の度合い
    };

    std::vector<WaterPoint> points;
    std::vector<SplashParticle> particles;
    std::vector<BloodParticle> bloodParticles;

    // 物理演算関連
    const float k = 2.5f;          // ばね定数
    const float damping = 0.05f;   // 抵抗
    const float spread = 0.25f;    // 波の速度

    const float surfaceY = 550.0f; // 水面
    const int numPoints = 81;      // 頂点数
    const float spacing = 10.0f;   // 頂点間隔

public:
    // ctor
    Water();

    // エフェクト（着地用）
    void Splash(float playerWorldX, float velocityY, float cameraLeft);

    // エフェクト（ゲームオーバー用）
    void EmitBlood(Vector2 worldPos, int count);

    // 描画
    void Draw(const Camera& camera);

    // 更新
    void Update(float dt);
};