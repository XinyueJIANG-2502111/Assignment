#pragma once

#include "DxLib.h"
#include "Vector.hpp"
#include "Water.hpp"
#include "Camera.hpp"
#include "Terrain.hpp"

#include <cmath>
#include <vector>

// ==========================================
// 2. 玩家与钩爪物理核心类（纯物理流体浮力版）
// ==========================================
class Player {
public:
    Vector2 position{ 400.0f, 500.0f };
    Vector2 velocity{ 0.0f, 0.0f };

    // ==========================================
    // 物理微调参数
    // ==========================================
    const float gravity = 750.0f;       // 基础重力 (向下)
    const float drag = 0.3f;            // 空气阻力
    const float swingForce = 600.0f;
    const float reelSpeed = 300.0f;

    const float groundAcc = 1200.0f;      // 水中划水推进力
    const float airControlForce = 400.0f;

    bool isGrounded = false;
    const float jumpForce = 500.0f;       // 破水而出的跳跃力

    const float grappleJumpBonus = 350.0f;
    const float launchPullForce = 350.0f;
    const float ropeLengthRate = 0.9f;
    const float autoPullSpeed = 500.0f;
    float targetRopeLength = 0.0f;

    // ---- [新流体物理核心参数] ----
    const float waterLineY = 550.0f;      // 水面绝对高度
    const float waterDensity = 4.5f;      // 水的阻力系数 (数值越大，在水里减速越快)
    const float buoyancyEngine = 1800.0f; // 纯浮力加速度 (必须大于重力，小人才能浮起来)

    bool isGrappling = false;
    Vector2 anchorPoint{ 0.0f, 0.0f };
    float ropeLength = 0.0f;

    std::vector<TrailPoint> trail;
    bool wasInWater = false;

    float noInputWaterTimer = 0.0f;
    bool isDead = false;
    float buoyancyScale = 1.0f;

    // ==========================================
    // 钩爪飞出物理状态机
    // ==========================================
    enum class GrappleState {
        None,       // 闲置状态（没开钩）
        Flying,     // 钩头正在天空中飞向目标
        Pinned      // 钩头已咬住墙体，正在荡跃（对应原本的 isGrappling = true）
    };
    GrappleState grappleState = GrappleState::None;

    Vector2 hookCurrentPos{ 0.0f, 0.0f };   // 钩头当前在世界中的绝对位置
    Vector2 hookVelocity{ 0.0f, 0.0f };     // 钩头的飞行速度向量
    const float hookFlySpeed = 2200.0f;     // 钩头飞出速度（2200像素/秒）
    float currentFlyDist = 0.0f;            // 当前飞出的累计距离，用来判断是否超出射程
    const float MAX_GRAPPLE_RANGE = 600.0f; // 限制钩爪的最远射程

    // 判断玩家中心是否在水面以下
    bool IsInWater() const {
        return position.y >= waterLineY;
    }

    void LaunchGrapple(Vector2 mouseWorldPos, const TerrainManager& terrain) {
        if (grappleState != GrappleState::None) return; // 如果已经发射了，不能重复发射

        Vector2 toMouse = mouseWorldPos - position;
        float distToMouse = toMouse.Length();
        if (distToMouse < 10.0f) return;

        Vector2 rayDir = toMouse.Normalized();

        // 💡【核心改动】：不直接定死锚点，而是初始化钩头的“飞行物理”
        grappleState = GrappleState::Flying;
        isGrappling = false; // 此时还没挂住，不能产生身体拉力

        hookCurrentPos = position; // 钩头从玩家当前的身体中心出发
        hookVelocity = rayDir * hookFlySpeed; // 赋予高速动能
        currentFlyDist = 0.0f; // 重置飞行距离
        anchorPoint = hookCurrentPos; // 临时记录

        // 记录拖尾
        trail.push_back({ position, 1.5f });
    }

    void ReleaseGrapple() {
        isGrappling = false;
        grappleState = GrappleState::None; // 彻底恢复闲置
    }

    void Jump() {
        if (isGrappling || isGrounded) {
            ReleaseGrapple();
            velocity.y = -grappleJumpBonus;
            trail.push_back({ position, 1.5f });
        }
        else if (IsInWater()) {
            // 破水起跳，顺便带起一波小水花
            velocity.y = -jumpForce;
        }
    }

    void Update(float dt, int horizontalInput, int verticalInput, bool jumpInput, bool reelInput, Water& water, float cameraLeft, const TerrainManager& terrain) {
        if (isDead) {
            // 死亡物理：像秤砣一样绝望地下沉，不受任何浮力控制
            const float deathGravity = 400.0f;
            velocity.y += deathGravity * dt;
            position += velocity * dt;
            const float SEA_BOTTOM_Y = 2000.0f;

            // 如果玩家的双脚（或身体底部）穿过了海底
            if (position.y + 8.f > SEA_BOTTOM_Y) {
                position.y = SEA_BOTTOM_Y - 8.f; // 强行把玩家推回海底表面（位置修正
            }

            // 持续在玩家伤口处喷射血雾
            water.EmitBlood(position, 2);
            return;
        }

        // ========================================================
        // 每一帧驱动飞行的钩头（Hook Point）
        // ========================================================
        if (grappleState == GrappleState::Flying) {
            // 1. 记录前一帧位置，用于做这段短位移的线段碰撞（防止穿模）
            Vector2 prevHookPos = hookCurrentPos;

            // 2. 钩头向前飞行
            Vector2 stepMove = hookVelocity * dt;
            hookCurrentPos += stepMove;
            currentFlyDist += stepMove.Length();

            // 3. 实时地形边缘检测（AABB点在盒内检测 + 射线/线段步进检测）
            bool hasHit = false;
            RaycastResult closestResult;
            closestResult.hit = false;
            closestResult.t = 1.0f; // 步长内的比例分量 (0.0 至 1.0)
            closestResult.hitPoint = hookCurrentPos;

            Vector2 stepDir = hookVelocity.Normalized();
            float stepLen = stepMove.Length();

            for (const auto& obs : terrain.obstacles) {
                // 连续扫掠检测 (射线检测)
                RaycastResult res = RaycastToObstacle(prevHookPos, stepDir, stepLen, obs);
                if (res.hit) {
                    float tFraction = (stepLen > 0.0001f) ? (res.t / stepLen) : 0.0f;
                    if (tFraction < closestResult.t) {
                        closestResult = res;
                        closestResult.t = tFraction;
                        hasHit = true;
                    }
                }

                // AABB 点在盒内检测 (防穿模边缘贴合回退机制)
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
                // 钉入成功！抓住了建筑物表面
                grappleState = GrappleState::Pinned;
                isGrappling = true; // 激活摆荡物理
                hookCurrentPos = closestResult.hitPoint; // 锁死精确挂钩点
                anchorPoint = hookCurrentPos;

                // ------ 触发原本完美的“入钩动能红利重定向” ------
                Vector2 toAnchor = anchorPoint - position;
                float actualDistance = toAnchor.Length();
                Vector2 pullDir = toAnchor.Normalized();

                if (IsInWater() || position.y >= waterLineY - 30.0f) {
                    // 净化速度向量：过滤重力、浮力或拍水产生的杂乱垂直力，将动能平滑重定向至拉绳方向
                    float speed = velocity.Length();
                    if (speed > 600.0f) speed = 600.0f; // 限制极端的动能异常
                    velocity = pullDir * speed;
                } else {
                    if (velocity.y > 0.0f) {
                        velocity.y *= 0.15f;
                    }
                }
                velocity += pullDir * launchPullForce * 1.6f;

                // 强制让绳长与目标绳长在碰撞帧完全贴合实际测得的物理距离，防止闪现
                ropeLength = actualDistance;
                targetRopeLength = actualDistance;
            }
            else if (currentFlyDist >= MAX_GRAPPLE_RANGE) {
                // 超出最远射程，宣告空钩，缩回绳索
                grappleState = GrappleState::None;
                isGrappling = false;
            }
        }

        // ===========================================================================================
        // 1. 任何时候，基础重力都在作用
        velocity.y += gravity * dt;
        isGrounded = false;

        bool hasInput = (horizontalInput != 0) || (verticalInput != 0) || jumpInput || reelInput || isGrappling;
        bool currentInWater = IsInWater();

        if (isGrappling) {
            // ==========================================
            // 【钩爪悬挂物理分支】
            // ==========================================
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
                //【新增情况 B】：玩家按住 S 键 —— 主动放绳
                targetRopeLength += reelSpeed * dt;

                // 放绳时，目标绳长不能超过最大射程
                if (targetRopeLength > MAX_GRAPPLE_RANGE) {
                    targetRopeLength = MAX_GRAPPLE_RANGE;
                }
            }

            // 让实际绳长（ropeLength）平滑地向目标绳长（targetRopeLength）过渡
            // 为了让放绳子的时候动作更顺滑、不卡顿，可以使用常速趋近
            if (ropeLength < targetRopeLength) {
                ropeLength += autoPullSpeed * dt;
                if (ropeLength > targetRopeLength) ropeLength = targetRopeLength;
            }
            else if (ropeLength > targetRopeLength) {
                ropeLength -= autoPullSpeed * dt;
                if (ropeLength < targetRopeLength) ropeLength = targetRopeLength;
            }

            // 再次确保实际绳长受到最远射程安全兜底
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
            } else {
                velocity *= (1.0f - drag * dt);
            }

        }
        else {
            // ==========================================
            // 【非悬挂常规物理分支】
            // ==========================================
            if (currentInWater) {
                if (!wasInWater && velocity.y > 0.0f) {
                    //【修改点】：在这里调用 splash 时，把相机的左边缘传进去
                    water.Splash(position.x, velocity.y, cameraLeft);
                }

                // ==========================================
                // 【纯物理流体：浸泡状态】
                // ==========================================

                // 1. 边缘触发：只有刚砸进水面的那一帧触发飞溅，绝不修改速度
                if (!wasInWater && velocity.y > 0.0f) {
                    water.Splash(position.x, velocity.y, cameraLeft);
                }

                //【核心改动】：如果泡在水里且没有操作
                if (!hasInput) {
                    noInputWaterTimer += dt;

                    if (noInputWaterTimer < 5.f) {
                        // 阶段 1：前 2 秒，完全正常的浮力状态
                        buoyancyScale = 1.0f;
                    }
                    else if (noInputWaterTimer >= 5.f && noInputWaterTimer < 10.f) {
                        // 阶段 2：2s 到 10s 之间，浮力线性衰减 (从 1.0 递减到 0.0)
                        // 计算当前阶段的进度百分比 (0.0 到 1.0)
                        float progress = (noInputWaterTimer - 0.5f) / 1.0f;
                        buoyancyScale = 1.0f - progress + 0.0001f; // 浮力越来越小

                        // 【视觉反馈】：既然开始溺水下沉，可以在嘴边开始冒出少量微弱的血丝或气泡
                        if (rand() % 10 == 0) {
                            water.EmitBlood(position, 1);
                        }
                    }
                    else if (noInputWaterTimer >= 10.f) {
                        // 阶段 3：超过 10 秒，彻底判定死亡
                        isDead = true;
                        velocity = { 0.0f, 60.0f };   // 瞬间获得一个向下的沉重速度
                        water.EmitBlood(position, 80); // 死亡瞬间大血雾爆发
                        return;
                    }
                }
                else {
                    // 只要玩家动了（比如按了 A/D 游动或开钩），计时器立刻清零，脱离危险
                    noInputWaterTimer = 0.0f;
                }

                // 2. 模拟流体阻力 (Fluid Drag Force)
                // 阻力 = 速度 * |速度| * 水的密度。这种二次方阻力能完美表现高空入水被瞬间“闷住”的顿挫感
                velocity.x -= velocity.x * std::abs(velocity.x) * 0.002f * waterDensity * dt;
                velocity.y -= velocity.y * std::abs(velocity.y) * 0.005f * waterDensity * dt;

                // 水中基础线性层阻尼 (处理极低速静止时的稳定)
                velocity *= (1.0f - 2.0f * dt);

                // 3. 模拟阿基米德浮力 (Buoyancy Force)
                // 侵入水下越深，浮力越强。小人浸入水下的深度 = position.y - waterLineY
                float depth = position.y - waterLineY;
                if (depth < 0.0f) depth = 0.0f;

                // 浮力公式：给予一个向上的加速度
                float currentBuoyancy = (depth / 30.0f) * buoyancyEngine * buoyancyScale;
                if (currentBuoyancy > buoyancyEngine) currentBuoyancy = buoyancyEngine; // 浮力上限限制

                velocity.y -= currentBuoyancy * dt;

                // 4. 水中水平操作 (划水游泳)
                if (horizontalInput != 0) {
                    velocity.x += static_cast<float>(horizontalInput) * groundAcc * dt;
                }
            }
            else {
                // ==========================================
                // 【空气中自由落体分支】
                // ==========================================
                if (horizontalInput != 0) {
                    velocity.x += static_cast<float>(horizontalInput) * airControlForce * dt;
                }
                velocity.x *= (1.0f - drag * dt);
                velocity.y *= (1.0f - drag * dt);
            }
        }

        
        // ========================================================
        // 所有矩形障碍物的物理碰撞与丝滑滑落响应
        // ========================================================
        if (!isDead) { // 活着的时候才进行精细滑落解算
            const float PLAYER_RADIUS = 16.0f; // 玩家碰撞半径

            // 考虑到可能同时卡在多个方块边缘，我们进行 2 次迭代解算以达到完美的稳定性
            for (int iteration = 0; iteration < 2; iteration++) {
                for (const auto& obs : terrain.obstacles) {
                    CollisionResult col = CheckPlayerCollision(position, PLAYER_RADIUS, obs);

                    if (col.collided) {
                        // Step A: 位置修正（安全推出墙体，防止卡死）
                        position += col.pushVector;

                        // 判断是否踩在方块顶部
                        // 当法线的 Y 小于 0（例如 -1.0），说明墙面把玩家往上推，玩家正踩在方块顶部
                        if (col.normal.y < -0.7f) {
                            isGrounded = true;

                            // 既然双脚着地，如果此时还在往下掉，就清除向下的速度，防止重力无限累加
                            if (velocity.y > 0.0f) {
                                velocity.y = 0.0f;
                            }
                        }

                        // Step B: 速度重定向（冲量解算）
                        // 计算当前速度在墙面法线方向上的投影
                        float velAlongNormal = velocity.x * col.normal.x + velocity.y * col.normal.y;

                        // 如果玩家正朝着墙壁运动（内冲速度），则消除法向速度
                        if (velAlongNormal < 0.0f) {
                            // 弹性系数 (Restitution)：0.0f 表示贴墙丝滑滑落，0.2f 表示带有一点点轻微反弹
                            const float bounce = 0.15f;

                            // 减去撞墙方向的速度，并叠加反弹速度
                            velocity -= col.normal * (1.0f + bounce) * velAlongNormal;

                            // 墙面摩擦力切向衰减：撞墙时让平行于墙面的速度稍微减速，更有刮擦质感
                            // 如果是撞到天花板或侧墙，让切向运动稍微滞涩一点
                            if (col.normal.y != 0.0f) velocity.x *= 0.98f; // 擦着天花板/地板滑行
                            if (col.normal.x != 0.0f) velocity.y *= 0.95f; // 擦着左右墙壁滑行
                        }

                        // 如果撞墙了，钩爪绳索应该怎么办？
                        // 如果玩家在荡跃过程中身体狠狠撞在了障碍物上，为了防止绳子穿过方块拉扯发生诡异摆荡，
                        // 业界通用的爽快做法是：撞墙瞬间“自动切断绳索”，给玩家一个重置操作的自由落体反馈。
                        //if (isGrappling) {
                        //    // 如果撞击面法线方向背离勾点，说明身体撞在了障碍物外侧，强制断绳
                        //    Vector2 toAnchor = anchorPoint - position;
                        //    if (toAnchor.x * col.normal.x + toAnchor.y * col.normal.y < 0.0f) {
                        //        ReleaseGrapple(); // 断开钩爪，防止绳子穿模抽搐
                        //    }
                        //}
                    }
                }
            }
        }

        // 3. 位移整合
        position += velocity * dt;

        // 4. 边界碰撞 
        // 核心细节：由于是纯物理浮力控制，我们把下潜底线放得极深（甚至取消），让物理公式自己去决定它沉多深
        if (position.y > 650.0f) {
            position.y = 650.0f;
            if (velocity.y > 0.0f) velocity.y = 0.0f;
        }
        //if (position.x < 20.0f) { position.x = 20.0f;  velocity.x = -velocity.x * 0.5f; }
        //if (position.x > 780.0f) { position.x = 780.0f; velocity.x = -velocity.x * 0.5f; }

        wasInWater = currentInWater;

        // 5. 拖尾逻辑
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

    void Draw(const Camera& camera, const Vector2& mouseWorldPos, const TerrainManager& terrain) {
        // ========================================================
        // 绘制发射前的辅助观察预瞄虚线（Raycast 预览）
        // ========================================================
        if (!isDead && !isGrappling) { // 只有在活着且当前没有挂绳子时，才显示预瞄线
            Vector2 toMouse = mouseWorldPos - position;
            float distToMouse = toMouse.Length();

            if (distToMouse > 10.0f) {
                Vector2 rayDir = toMouse.Normalized();
                const float MAX_GRAPPLE_RANGE = 600.0f; // 与你发射钩爪的最大射程完全一致

                bool foundClosestHit = false;
                RaycastResult closestResult;
                closestResult.t = MAX_GRAPPLE_RANGE; // 初始化为最远射程

                // 实时扫描当前周边的所有障碍物，找出最近的潜在落点
                for (const auto& obs : terrain.obstacles) {
                    RaycastResult res = RaycastToObstacle(position, rayDir, MAX_GRAPPLE_RANGE, obs);
                    if (res.hit && res.t < closestResult.t) {
                        closestResult = res;
                        foundClosestHit = true;
                    }
                }

                // 确定辅助线的起点和终点（世界坐标）
                Vector2 lineStartWorld = position;
                Vector2 lineEndWorld = foundClosestHit ? closestResult.hitPoint : (position + rayDir * MAX_GRAPPLE_RANGE);

                // 转换为屏幕坐标用于绘制
                Vector2 sStart = camera.WorldToScreen(lineStartWorld);
                Vector2 sEnd = camera.WorldToScreen(lineEndWorld);

                //【核心绘制算法】：将屏幕视切线转化为“虚线点阵”
                Vector2 screenDelta = sEnd - sStart;
                float screenDist = screenDelta.Length();
                Vector2 screenDir = screenDelta.Normalized();

                // 每一个虚线点的间距（每 15 像素画一个点）
                const float dotStep = 15.0f;

                // 依据是否瞄准命中，给预瞄线赋予不同的反馈颜色
                // 瞄准到了：亮青色（代表可以抓取） | 没瞄准到：淡淡的灰色/暗红色（代表超长或空钩）
                unsigned int lineColor = foundClosestHit ? GetColor(0, 255, 200) : GetColor(120, 120, 120);

                // 开启半透明混合，让虚线显得非常精致、不刺眼
                SetDrawBlendMode(DX_BLENDMODE_ALPHA, foundClosestHit ? 180 : 80);

                // 步进式绘制虚线小圆点
                for (float d = 0.0f; d < screenDist; d += dotStep) {
                    Vector2 dotPos = sStart + screenDir * d;
                    // 如果瞄准中了，越靠近落点的小圆点画得越小，形成视觉汇聚感
                    int radius = foundClosestHit ? static_cast<int>(3.0f * (1.0f - (d / screenDist * 0.5f))) : 2;
                    if (radius < 1) radius = 1;

                    DrawCircle(static_cast<int>(dotPos.x), static_cast<int>(dotPos.y), radius, lineColor, TRUE);
                }

                // 如果精准瞄中了建筑物表面，在交点处绘制一个小交叉或亮圈作为“锁定框”
                if (foundClosestHit) {
                    int targetX = static_cast<int>(sEnd.x);
                    int targetY = static_cast<int>(sEnd.y);
                    // 画一个空心小靶心
                    DrawCircle(targetX, targetY, 6, lineColor, FALSE);
                    DrawCircle(targetX, targetY, 2, GetColor(255, 255, 255), TRUE);
                }

                SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0); // 恢复混合模式
            }
        }

        // ========================================================
        // 1. 绘制发光拖尾（需要将每个拖尾点转换为屏幕坐标）
        // ========================================================
        for (const auto& t : trail) {
            // 【修改点】：将拖尾的世界坐标转换为屏幕坐标
            Vector2 sTrailPos = camera.WorldToScreen(t.pos);

            int alphaVal = static_cast<int>(t.alpha * 120);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
            DrawCircle(static_cast<int>(sTrailPos.x), static_cast<int>(sTrailPos.y), 12, GetColor(0, 255, 255), TRUE);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // ========================================================
        // 2. 绘制钩爪绳索与锚点
        // ========================================================
        if (grappleState == GrappleState::Flying || grappleState == GrappleState::Pinned) {
            // 1. 绳索起点：始终是玩家本体的世界坐标
            Vector2 sPlayerPos = camera.WorldToScreen(position);

            // 2. 绳索终点（动态抉择）：
            //    如果钩子还在天上飞，终点就是不断向前推进的【hookCurrentPos】
            //    如果已经钉住了，终点才是锁死在墙面上的【anchorPoint】
            Vector2 targetTip = (grappleState == GrappleState::Flying) ? hookCurrentPos : anchorPoint;
            Vector2 sAnchorPos = camera.WorldToScreen(targetTip);

            int pX = static_cast<int>(sPlayerPos.x);
            int pY = static_cast<int>(sPlayerPos.y);
            int aX = static_cast<int>(sAnchorPos.x);
            int aY = static_cast<int>(sAnchorPos.y);

            // 3. 绘制外围发光霓虹粗线
            //    飞行中采用高科技感的冰蓝色（0, 180, 255），钉入后变成高能青绿色（0, 255, 128）
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, (grappleState == GrappleState::Flying) ? 160 : 100);
            unsigned int ropeColor = (grappleState == GrappleState::Flying) ? GetColor(0, 180, 255) : GetColor(0, 255, 128);
            DrawLine(aX, aY, pX, pY, ropeColor, 5);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

            // 4. 绘制内层高亮白细线（灯丝）
            DrawLine(aX, aY, pX, pY, GetColor(255, 255, 255), 2);

            // 5. 绘制正在破空飞行的“钩头”高亮粒子及霓虹发光特效
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
            } else {
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
        // 3. 绘制主角自身（圆形 / 拉伸椭圆）
        // ========================================================
        // 【修改点】：将玩家当前世界坐标转换为屏幕坐标
        Vector2 sPlayerPos = camera.WorldToScreen(position);
        int pX = static_cast<int>(sPlayerPos.x);
        int pY = static_cast<int>(sPlayerPos.y);

        float speed = velocity.Length();
        Vector2 moveDir = velocity.Normalized();

        // 物理细节提示：
        // 速度（velocity）和移动方向（moveDir）是纯粹的“物理矢量”，
        // 它们代表每秒移动的像素位移，与相机位置无关，因此【绝对不需要】进行坐标转换。    
        DrawCircle(pX, pY, 16, GetColor(200, 200, 200), TRUE);
        DrawCircle(pX, pY, 14, GetColor(255, 255, 255), TRUE);
        DrawCircle(pX, pY, 10, GetColor(0, 160, 255), TRUE);
        DrawCircle(pX, pY, 7, GetColor(190, 255, 255), TRUE);
        DrawCircle(pX, pY, 3, GetColor(70, 75, 80), TRUE);
        DrawPixel(pX, pY, GetColor(255, 255, 255));
    }
};