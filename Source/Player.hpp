#pragma once

#include "Vector.hpp"

#include <vector>

// 前方宣言
class TerrainManager;
class Water;
class Camera;

// エフェクト用（移動時）
struct TrailPoint {
    Vector2 pos;
    float alpha;
};

// プレイヤークラス
class Player {
public:
    // 位置と速度
    Vector2 position{ 400.0f, 500.0f };
    Vector2 velocity{ 0.0f, 0.0f };

    // 物理演算関連
    const float gravity = 750.0f;
    const float drag = 0.3f;
    const float swingForce = 600.0f;
    const float reelSpeed = 300.0f;

    const float groundAcc = 1200.0f;
    const float airControlForce = 400.0f;

    bool isGrounded = false;
    const float jumpForce = 500.0f;

    const float grappleJumpBonus = 350.0f;
    const float launchPullForce = 350.0f;
    const float ropeLengthRate = 0.9f;
    const float autoPullSpeed = 500.0f;
    float targetRopeLength = 0.0f;

    // 水
    const float waterLineY = 550.0f;      // 水面高さ
    const float waterDensity = 4.5f;      // 密度（抵抗係数）
    const float buoyancyEngine = 1800.0f;

    bool isGrappling = false;
    Vector2 anchorPoint{ 0.0f, 0.0f };
    float ropeLength = 0.0f;

    // エフェクト用
    std::vector<TrailPoint> trail;
    bool wasInWater = false;

    // ゲームオーバー判定用
    float noInputWaterTimer = 0.0f;
    bool isDead = false;
    float buoyancyScale = 1.0f;

    // ワイヤー関連
    // 状態
    enum class GrappleState {
        None,       // なにもくっついていない（発射していない）
        Flying,     // 発射途中
        Pinned      // ブロックにくっついている
    };

    // 初期化
    GrappleState grappleState = GrappleState::None;

    Vector2 hookCurrentPos{ 0.0f, 0.0f };   // ワイヤーヘッドの位置
    Vector2 hookVelocity{ 0.0f, 0.0f };     // 速度（方向）
    const float hookFlySpeed = 2200.0f;     // 速度（大きさ）
    float currentFlyDist = 0.0f;            // 飛行した距離
    const float MAX_GRAPPLE_RANGE = 600.0f; // 最大飛行距離


public:
    // プレイヤーが水にいるかどうか
    bool IsInWater() const;

    // ワイヤーを発射する
    void LaunchGrapple(Vector2 mouseWorldPos, const TerrainManager& terrain);

    // ワイヤーを切る
    void ReleaseGrapple();

    // ジャンプ
    void Jump();

    // 更新
    void Update(float dt, 
        int horizontalInput, int verticalInput, bool jumpInput, bool reelInput, 
        Water& water, float cameraLeft, const TerrainManager& terrain);

    // 描画
    void Draw(const Camera& camera, const Vector2& mouseWorldPos, const TerrainManager& terrain);
};