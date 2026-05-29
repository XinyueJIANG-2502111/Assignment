#pragma once
#include "Vector.hpp"

// カメラクラス
class Camera {
public:
    Vector2 position{ 400.0f, 300.0f }; // カメラ位置

    // 初期化
    void Init() {
        position = Vector2{ 400.0f, 300.0f };
    }

    // 座標変換
    Vector2 WorldToScreen(const Vector2& worldPos) const {
        return {
            worldPos.x - position.x + 400.0f,
            worldPos.y - position.y + 300.0f
        };
    }

    // プレイヤーを追従する（線形補間使用）
    void Follow(const Vector2& playerWorldPos, float dt) {
        // すこし上にずれる
        float targetX = playerWorldPos.x;
        float targetY = playerWorldPos.y - 80.0f;

        position.x += (targetX - position.x) * 5.0f * dt;
        position.y += (targetY - position.y) * 5.0f * dt;
    }
};