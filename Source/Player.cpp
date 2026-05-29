#include "Player.hpp"
#include "DxLib.h"
#include "Vector.hpp"
#include "Water.hpp"
#include "Camera.hpp"
#include "Terrain.hpp"

#include <cmath>
#include <vector>

bool Player::IsInWater() const {
    return position.y >= waterLineY;
}

void Player::LaunchGrapple(Vector2 mouseWorldPos, const TerrainManager& terrain) {
    if (grappleState != GrappleState::None) return; // 重複発射防止

    Vector2 toMouse = mouseWorldPos - position;
    float distToMouse = toMouse.Length();
    if (distToMouse < 10.0f) return;

    Vector2 rayDir = toMouse.Normalized();

    grappleState = GrappleState::Flying;
    isGrappling = false;

    hookCurrentPos = position; // 開始位置
    hookVelocity = rayDir * hookFlySpeed;
    currentFlyDist = 0.0f; // 距離リセット
    anchorPoint = hookCurrentPos;

    // 移動軌跡（エフェクト）
    trail.push_back({ position, 1.5f });
}

void Player::ReleaseGrapple() {
    isGrappling = false;
    grappleState = GrappleState::None;
}

void Player::Jump() {
    if (isGrappling || isGrounded) {
        ReleaseGrapple();
        velocity.y = -grappleJumpBonus;
        trail.push_back({ position, 1.5f });
    }
    else if (IsInWater()) {
        velocity.y = -jumpForce;
    }
}

void Player::Update(float dt, int horizontalInput, int verticalInput, bool jumpInput, bool reelInput, Water& water, float cameraLeft, const TerrainManager& terrain) {
    // ゲームオーバー
    if (isDead) {
        // 完全に沈む
        const float deathGravity = 400.0f;
        velocity.y += deathGravity * dt;
        position += velocity * dt;
        const float SEA_BOTTOM_Y = 2000.0f;

        // 境界条件
        if (position.y + 8.f > SEA_BOTTOM_Y) {
            position.y = SEA_BOTTOM_Y - 8.f;
        }

        // エフェクト
        water.EmitBlood(position, 2);
        return;
    }

    // ========================================================
    // 　Hook Point
    // ========================================================
    if (grappleState == GrappleState::Flying) {
        // 直前の位置
        Vector2 prevHookPos = hookCurrentPos;

        // 発射
        Vector2 stepMove = hookVelocity * dt;
        hookCurrentPos += stepMove;
        currentFlyDist += stepMove.Length();

        // 当たり判定
        bool hasHit = false;
        RaycastResult closestResult;
        closestResult.hit = false;
        closestResult.t = 1.0f;
        closestResult.hitPoint = hookCurrentPos;

        Vector2 stepDir = hookVelocity.Normalized();
        float stepLen = stepMove.Length();

        for (const auto& obs : terrain.obstacles) {
            // 当たり判定
            RaycastResult res = RaycastToObstacle(prevHookPos, stepDir, stepLen, obs);
            if (res.hit) {
                float tFraction = (stepLen > 0.0001f) ? (res.t / stepLen) : 0.0f;
                if (tFraction < closestResult.t) {
                    closestResult = res;
                    closestResult.t = tFraction;
                    hasHit = true;
                }
            }

            // ブロック内部であるかどうか
            bool pointInside = (hookCurrentPos.x >= obs.Left() && hookCurrentPos.x <= obs.Right() &&
                hookCurrentPos.y >= obs.Top() && hookCurrentPos.y <= obs.Bottom());
            if (pointInside) {
                Vector2 snappedPos = hookCurrentPos;
                if (prevHookPos.x <= obs.Left() && hookCurrentPos.x >= obs.Left()) {
                    snappedPos.x = obs.Left();
                }
                else if (prevHookPos.x >= obs.Right() && hookCurrentPos.x <= obs.Right()) {
                    snappedPos.x = obs.Right();
                }
                else if (prevHookPos.y <= obs.Top() && hookCurrentPos.y >= obs.Top()) {
                    snappedPos.y = obs.Top();
                }
                else if (prevHookPos.y >= obs.Bottom() && hookCurrentPos.y <= obs.Bottom()) {
                    snappedPos.y = obs.Bottom();
                }

                if (!hasHit || 1.0f < closestResult.t) {
                    closestResult.hit = true;
                    closestResult.hitPoint = snappedPos;
                    closestResult.t = 1.0f;
                    hasHit = true;
                }
            }
        }

        
        if (hasHit) {
            // 目標位置にくっつく
            grappleState = GrappleState::Pinned;
            isGrappling = true;
            hookCurrentPos = closestResult.hitPoint;
            anchorPoint = hookCurrentPos;

            Vector2 toAnchor = anchorPoint - position;
            float actualDistance = toAnchor.Length();
            Vector2 pullDir = toAnchor.Normalized();

            if (IsInWater() || position.y >= waterLineY - 30.0f) {
                float speed = velocity.Length();
                if (speed > 600.0f) speed = 600.0f;
                velocity = pullDir * speed;
            }
            else {
                if (velocity.y > 0.0f) {
                    velocity.y *= 0.15f;
                }
            }
            velocity += pullDir * launchPullForce * 1.6f;

            ropeLength = actualDistance;
            targetRopeLength = actualDistance;
        }
        else if (currentFlyDist >= MAX_GRAPPLE_RANGE) {
            // 最大距離を超えた
            grappleState = GrappleState::None;
            isGrappling = false;
        }
    }

    // ===========================================================================================
    velocity.y += gravity * dt;
    isGrounded = false;

    bool hasInput = (horizontalInput != 0) || (verticalInput != 0) || jumpInput || reelInput || isGrappling;
    bool currentInWater = IsInWater();

    // プレイヤーの状態
    if (isGrappling) {
        if (ropeLength > targetRopeLength) {
            ropeLength -= autoPullSpeed * dt;
            if (ropeLength < targetRopeLength) ropeLength = targetRopeLength;
        }
        if (reelInput) {
            targetRopeLength -= reelSpeed * dt;
            if (targetRopeLength < 40.0f) targetRopeLength = 40.0f;
            ropeLength = targetRopeLength;
        }
        if (verticalInput == 1) {
            targetRopeLength += reelSpeed * dt;

            if (targetRopeLength > MAX_GRAPPLE_RANGE) {
                targetRopeLength = MAX_GRAPPLE_RANGE;
            }
        }

        if (ropeLength < targetRopeLength) {
            ropeLength += autoPullSpeed * dt;
            if (ropeLength > targetRopeLength) ropeLength = targetRopeLength;
        }
        else if (ropeLength > targetRopeLength) {
            ropeLength -= autoPullSpeed * dt;
            if (ropeLength < targetRopeLength) ropeLength = targetRopeLength;
        }

        if (ropeLength > MAX_GRAPPLE_RANGE) {
            ropeLength = MAX_GRAPPLE_RANGE;
        }

        Vector2 toPlayer = position - anchorPoint;
        float currentDist = toPlayer.Length();
        Vector2 ropeDir = toPlayer.Normalized();

        Vector2 tangentDir = { ropeDir.y, -ropeDir.x };
        velocity += tangentDir * (static_cast<float>(horizontalInput) * swingForce * dt);

        if (currentDist >= ropeLength) {
            float vRope = velocity.Dot(ropeDir);
            if (vRope > 0.0f) {
                velocity = velocity - ropeDir * vRope;
            }
            position = anchorPoint + ropeDir * ropeLength;
        }
        if (currentInWater) {
            velocity *= (1.0f - waterDensity * dt);
        }
        else {
            velocity *= (1.0f - drag * dt);
        }

    }
    else {
        if (currentInWater) {
            if (!wasInWater && velocity.y > 0.0f) {
                water.Splash(position.x, velocity.y, cameraLeft);
            }

            // 水面にいる時
            // 着地瞬間（エフェクトを出す）
            if (!wasInWater && velocity.y > 0.0f) {
                water.Splash(position.x, velocity.y, cameraLeft);
            }

            // 長い間（１０秒間）に入力がないとゲームオーバー
            if (!hasInput) {
                noInputWaterTimer += dt;

                if (noInputWaterTimer < 5.f) {
                    // 五秒以内：普段通り
                    buoyancyScale = 1.0f;
                }
                else if (noInputWaterTimer >= 5.f && noInputWaterTimer < 10.f) {
                    // 十秒以内：すこし沈む
                    float progress = (noInputWaterTimer - 0.5f) / 1.0f;
                    buoyancyScale = 1.0f - progress + 0.0001f;

                    // エフェクト（小）
                    if (rand() % 10 == 0) {
                        water.EmitBlood(position, 1);
                    }
                }
                else if (noInputWaterTimer >= 10.f) {
                    // 十秒超えた：完全に沈む、ゲームオーバー
                    isDead = true;
                    velocity = { 0.0f, 60.0f };
                    water.EmitBlood(position, 80); // エフェクト（大）
                    return;
                }
            }
            else {
                // リセット
                noInputWaterTimer = 0.0f;
            }

            // 流体シミュレーション
            velocity.x -= velocity.x * std::abs(velocity.x) * 0.002f * waterDensity * dt;
            velocity.y -= velocity.y * std::abs(velocity.y) * 0.005f * waterDensity * dt;

            velocity *= (1.0f - 2.0f * dt);

            float depth = position.y - waterLineY;
            if (depth < 0.0f) depth = 0.0f;

            float currentBuoyancy = (depth / 30.0f) * buoyancyEngine * buoyancyScale;
            if (currentBuoyancy > buoyancyEngine) currentBuoyancy = buoyancyEngine;

            velocity.y -= currentBuoyancy * dt;

            // 移動
            if (horizontalInput != 0) {
                velocity.x += static_cast<float>(horizontalInput) * groundAcc * dt;
            }
        }
        else {
            if (horizontalInput != 0) {
                velocity.x += static_cast<float>(horizontalInput) * airControlForce * dt;
            }
            velocity.x *= (1.0f - drag * dt);
            velocity.y *= (1.0f - drag * dt);
        }
    }

    if (!isDead) {
        const float PLAYER_RADIUS = 16.0f; // 半径

        // 当たり判定
        // 二回ループ：より安定的な結果
        for (int iteration = 0; iteration < 2; iteration++) {
            for (const auto& obs : terrain.obstacles) {
                CollisionResult col = CheckPlayerCollision(position, PLAYER_RADIUS, obs);

                if (col.collided) {
                    // 位置変更（ブロック内部にいないように）
                    position += col.pushVector;

                    // ブロックの上にいる
                    if (col.normal.y < -0.7f) {
                        isGrounded = true;

                        if (velocity.y > 0.0f) {
                            velocity.y = 0.0f;
                        }
                    }

                    // 衝突後の速度再計算
                    float velAlongNormal = velocity.x * col.normal.x + velocity.y * col.normal.y;

                    if (velAlongNormal < 0.0f) {
                        const float bounce = 0.15f;

                        velocity -= col.normal * (1.0f + bounce) * velAlongNormal;

                        if (col.normal.y != 0.0f) velocity.x *= 0.98f;
                        if (col.normal.x != 0.0f) velocity.y *= 0.95f;
                    }
                }
            }
        }
    }

    // 位置更新
    position += velocity * dt;

    // 境界条件
    if (position.y > 650.0f) {
        position.y = 650.0f;
        if (velocity.y > 0.0f) velocity.y = 0.0f;
    }
    //if (position.x < 20.0f) { position.x = 20.0f;  velocity.x = -velocity.x * 0.5f; }
    //if (position.x > 780.0f) { position.x = 780.0f; velocity.x = -velocity.x * 0.5f; }

    wasInWater = currentInWater;

    // 移動の時のエフェクト
    if (velocity.Length() > 150.0f) {
        trail.push_back({ position, 1.0f });
    }
    for (size_t i = 0; i < trail.size(); ) {
        trail[i].alpha -= 3.0f * dt;
        if (trail[i].alpha <= 0.0f) {
            trail.erase(trail.begin() + i);
        }
        else {
            i++;
        }
    }
}

void Player::Draw(const Camera& camera, const Vector2& mouseWorldPos, const TerrainManager& terrain) {
    // ========================================================
    // ワイヤー発射前のガイド：発射方向を示す
    // ========================================================
    if (!isDead && !isGrappling) {
        Vector2 toMouse = mouseWorldPos - position;
        float distToMouse = toMouse.Length();

        if (distToMouse > 10.0f) {
            Vector2 rayDir = toMouse.Normalized();
            const float MAX_GRAPPLE_RANGE = 600.0f; // 最大距離と一致

            bool foundClosestHit = false;
            RaycastResult closestResult;
            closestResult.t = MAX_GRAPPLE_RANGE;

            // 周りのブロックを探す
            for (const auto& obs : terrain.obstacles) {
                RaycastResult res = RaycastToObstacle(position, rayDir, MAX_GRAPPLE_RANGE, obs);
                if (res.hit && res.t < closestResult.t) {
                    closestResult = res;
                    foundClosestHit = true;
                }
            }

            Vector2 lineStartWorld = position;
            Vector2 lineEndWorld = foundClosestHit ? closestResult.hitPoint : (position + rayDir * MAX_GRAPPLE_RANGE);

            Vector2 sStart = camera.WorldToScreen(lineStartWorld);
            Vector2 sEnd = camera.WorldToScreen(lineEndWorld);

            Vector2 screenDelta = sEnd - sStart;
            float screenDist = screenDelta.Length();
            Vector2 screenDir = screenDelta.Normalized();

            const float dotStep = 15.0f;

            // その方向にブロックがあるかどうか
            // ある：青色の点線 | ない（または範囲外）：灰色の点線
            unsigned int lineColor = foundClosestHit ? GetColor(0, 255, 200) : GetColor(120, 120, 120);

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, foundClosestHit ? 180 : 80);

            for (float d = 0.0f; d < screenDist; d += dotStep) {
                Vector2 dotPos = sStart + screenDir * d;
                int radius = foundClosestHit ? static_cast<int>(3.0f * (1.0f - (d / screenDist * 0.5f))) : 2;
                if (radius < 1) radius = 1;

                DrawCircle(static_cast<int>(dotPos.x), static_cast<int>(dotPos.y), radius, lineColor, TRUE);
            }

            if (foundClosestHit) {
                int targetX = static_cast<int>(sEnd.x);
                int targetY = static_cast<int>(sEnd.y);
                DrawCircle(targetX, targetY, 6, lineColor, FALSE);
                DrawCircle(targetX, targetY, 2, GetColor(255, 255, 255), TRUE);
            }

            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    // ========================================================
    // // 移動の時のエフェクト
    // ========================================================
    for (const auto& t : trail) {
        Vector2 sTrailPos = camera.WorldToScreen(t.pos);

        int alphaVal = static_cast<int>(t.alpha * 120);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
        DrawCircle(static_cast<int>(sTrailPos.x), static_cast<int>(sTrailPos.y), 12, GetColor(0, 255, 255), TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // ========================================================
    // ワイヤーの描画
    // ========================================================
    if (grappleState == GrappleState::Flying || grappleState == GrappleState::Pinned) {
        Vector2 sPlayerPos = camera.WorldToScreen(position);

        Vector2 targetTip = (grappleState == GrappleState::Flying) ? hookCurrentPos : anchorPoint;
        Vector2 sAnchorPos = camera.WorldToScreen(targetTip);

        int pX = static_cast<int>(sPlayerPos.x);
        int pY = static_cast<int>(sPlayerPos.y);
        int aX = static_cast<int>(sAnchorPos.x);
        int aY = static_cast<int>(sAnchorPos.y);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, (grappleState == GrappleState::Flying) ? 160 : 100);
        unsigned int ropeColor = (grappleState == GrappleState::Flying) ? GetColor(0, 180, 255) : GetColor(0, 255, 128);
        DrawLine(aX, aY, pX, pY, ropeColor, 5);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        DrawLine(aX, aY, pX, pY, GetColor(255, 255, 255), 2);

        // 飛行中のヘッド
        if (grappleState == GrappleState::Flying) {
            // Flying hook head: distinctive neon glow effect (overlapping translucent circles)
            unsigned int glowColor = GetColor(0, 230, 255); // Ice blue glow
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 50);
            DrawCircle(aX, aY, 14, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
            DrawCircle(aX, aY, 8, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawCircle(aX, aY, 4, glowColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawCircle(aX, aY, 2, GetColor(255, 255, 255), TRUE); // Central bright white dot
        }
        else {
            // Pinned hook head: red/neon green anchor point
            unsigned int pinColor = GetColor(255, 50, 50); // Red core
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 100);
            DrawCircle(aX, aY, 10, pinColor, TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
            DrawCircle(aX, aY, 5, pinColor, TRUE);
            DrawCircle(aX, aY, 2, GetColor(255, 255, 255), TRUE);
        }
    }

    // ========================================================
    // プレイヤーを描画
    // ========================================================
    Vector2 sPlayerPos = camera.WorldToScreen(position);
    int pX = static_cast<int>(sPlayerPos.x);
    int pY = static_cast<int>(sPlayerPos.y);

    float speed = velocity.Length();
    Vector2 moveDir = velocity.Normalized();

    DrawCircle(pX, pY, 16, GetColor(200, 200, 200), TRUE);
    DrawCircle(pX, pY, 14, GetColor(255, 255, 255), TRUE);
    DrawCircle(pX, pY, 10, GetColor(0, 160, 255), TRUE);
    DrawCircle(pX, pY, 7, GetColor(190, 255, 255), TRUE);
    DrawCircle(pX, pY, 3, GetColor(70, 75, 80), TRUE);
    DrawPixel(pX, pY, GetColor(255, 255, 255));
}