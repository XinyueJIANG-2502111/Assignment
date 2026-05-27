#pragma once
#include "Vector.hpp"
// ==========================================
// 3. 2D 相机系统
// ==========================================
class Camera {
public:
    Vector2 position{ 400.0f, 300.0f }; // 相机在世界中的中心点坐标

    void Init() {
        position = Vector2{ 400.0f, 300.0f };
    }

    // 核心数学公式：世界坐标转屏幕坐标
    Vector2 WorldToScreen(const Vector2& worldPos) const {
        return {
            worldPos.x - position.x + 400.0f, // 400 是屏幕宽度的二分之一
            worldPos.y - position.y + 300.0f  // 300 是屏幕高度的二分之一
        };
    }

    // 平滑跟随逻辑 (使用 Lerp 插值法)
    void Follow(const Vector2& playerWorldPos, float dt) {
        // 目标位置：我们希望相机的中心对准玩家，但在 Y 轴上稍微往上提 80 像素
        // 这样可以给上方留出更多视野，方便玩家往高处发射钩爪
        float targetX = playerWorldPos.x;
        float targetY = playerWorldPos.y - 80.0f;

        // 这里的 5.0f 是平滑系数。数值越大，镜头跟得越紧；数值越小，镜头越有电影般的滞后拉扯感
        position.x += (targetX - position.x) * 5.0f * dt;
        position.y += (targetY - position.y) * 5.0f * dt;
    }
};