#pragma once
#include "Vector.hpp"
#include "Camera.hpp"
#include "DxLib.h"
#include <random>

// 水花粒子结构体
struct SplashParticle {
    Vector2 pos;
    Vector2 vel;
    float alpha;
    float size;
    float lifetime;
};

// 血液效果
struct BloodParticle {
    Vector2 pos;
    Vector2 vel;
    float size;
    float alpha;
    float maxLifetime;
    float currentLifetime;
    float wavePhase;
};


// ==========================================
// 弹性流体水面管理类
// ==========================================
class Water {
public:
    struct WaterPoint {
        float y;          // 当前Y坐标
        float targetY;    // 目标平衡位置Y坐标
        float speed;      // 垂直振动速度
    };

    std::vector<WaterPoint> points;
    std::vector<SplashParticle> particles;
    std::vector<BloodParticle> bloodParticles;

    // 物理参数调校
    const float k = 2.5f;          // 弹簧刚度 (数值越大，波浪恢复越快，水越“硬”)
    const float damping = 0.05f;   // 阻尼 (数值越大，水波消散越快)
    const float spread = 0.25f;    // 波的传导系数 (数值越大，水波向两边扩散得越快)

    const float surfaceY = 550.0f; // 水面平衡线
    const int numPoints = 81;      // 水面顶点数量 (800像素宽，每10像素一个点)
    const float spacing = 10.0f;   // 顶点间距

    Water() {
        points.resize(numPoints);
        for (int i = 0; i < numPoints; i++) {
            points[i].y = surfaceY;
            points[i].targetY = surfaceY;
            points[i].speed = 0.0f;
        }
    }

    // 外部触发：在指定 X 坐标处砸出一个大水坑
    // ==========================================
    // Water 类内部需要修改的两个函数
    // ==========================================

    // A. 修改 Splash 函数：新增接收 cameraLeft 参数
    void Splash(float playerWorldX, float velocityY, float cameraLeft) {
        // 核心转变：计算玩家相对于当前相机视口左侧的 相对X坐标 
        float relativeX = playerWorldX - cameraLeft;
        int index = static_cast<int>(relativeX / spacing);

        if (index >= 0 && index < numPoints) {
            float force = velocityY * 0.4f;
            points[index].speed = force > 300.0f ? 300.0f : force;
        }

        // 粒子依然在绝对世界坐标下诞生，这部分保持原样
        int particleCount = static_cast<int>(velocityY * 0.05f);
        for (int i = 0; i < particleCount; i++) {
            SplashParticle p;
            p.pos = { playerWorldX, surfaceY };
            float angle = ((rand() % 100) / 100.0f) * 3.14159f * 0.6f + 3.14159f * 0.2f;
            float speed = ((rand() % 100) / 100.0f) * (velocityY * 0.6f) + 100.0f;
            p.vel = { cosf(angle) * speed, -sinf(angle) * speed };
            p.alpha = 1.0f; p.size = 3.0f + (rand() % 4); p.lifetime = 0.5f + ((rand() % 50) / 100.0f);
            particles.push_back(p);
        }
    }

    void EmitBlood(Vector2 worldPos, int count) {
        // 适当增加粒子数，让细碎的粒子呈现丝线状
        int actualCount = count * 6;

        for (int i = 0; i < actualCount; i++) {
            BloodParticle p;

            // 诞生在伤口周围的一个极小随机圈内
            float offset = ((rand() % 100) / 100.0f) * 8.0f;
            float angle = ((rand() % 100) / 100.0f) * 2.0f * 3.14159f;
            p.pos.x = worldPos.x + cosf(angle) * offset;
            p.pos.y = worldPos.y + sinf(angle) * offset;

            //【上浮速度配置】：横向初速度几乎为0，纵向给予一个向上的初速度 (-40 到 -80)
            p.vel.x = ((rand() % 20) - 10.0f) * 0.5f;
            p.vel.y = -(((rand() % 40) + 40.0f));

            p.size = 1.0f + ((rand() % 120) / 100.0f);   // 保持 1~2 像素的极细碎颗粒
            p.alpha = 0.15f + ((rand() % 15) / 100.0f);  // 稍稍提高单颗初始亮度，防止太淡看不清
            p.maxLifetime = 2.0f + ((rand() % 100) / 100.0f) * 1.5f; // 寿命 2.0s ~ 3.5s
            p.currentLifetime = p.maxLifetime;

            p.wavePhase = ((rand() % 100) / 100.0f) * 6.28f; // 随机初始化摆动起点

            bloodParticles.push_back(p);
        }
    }

    // B. 修改 Draw 函数：让网格顶点的世界 X 坐标随相机平移
    void Draw(const Camera& camera) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 130);
        int waterColor = GetColor(134, 214, 217);
        int surfaceColor = GetColor(0, 230, 194);

        // 获取当前相机视口在世界中的左边缘 X 坐标
        float worldLeft = camera.position.x - 400.0f;

        for (int i = 0; i < numPoints - 1; i++) {
            // 💡 核心转变：水体顶点的世界 X 坐标不再是固定的，而是基于相机左边缘累加
            float wx1 = worldLeft + static_cast<float>(i * spacing);
            float wx2 = worldLeft + static_cast<float>((i + 1) * spacing);

            float wy1 = points[i].y; // 依旧是波动的绝对世界 Y 坐标
            float wy2 = points[i + 1].y;

            // 转换为屏幕坐标
            Vector2 sW1 = camera.WorldToScreen({ wx1, wy1 });
            Vector2 sW2 = camera.WorldToScreen({ wx2, wy2 });

            // 将水底延伸至大世界深处（例如 Y = 2000），防止相机向上飞时露出白色视口底部
            Vector2 sB1 = camera.WorldToScreen({ wx1, 2500.0f });
            Vector2 sB2 = camera.WorldToScreen({ wx2, 2500.0f });

            DrawQuadrangle(
                static_cast<int>(sW1.x), static_cast<int>(sW1.y),
                static_cast<int>(sW2.x), static_cast<int>(sW2.y),
                static_cast<int>(sB2.x), static_cast<int>(sB2.y),
                static_cast<int>(sB1.x), static_cast<int>(sB1.y),
                waterColor, TRUE
            );

            DrawLine(static_cast<int>(sW1.x), static_cast<int>(sW1.y), static_cast<int>(sW2.x), static_cast<int>(sW2.y), surfaceColor, 2);
        }

        // 2. 绘制晕染血雾 (采用 DX_BLENDMODE_ALPHA)
        for (const auto& bp : bloodParticles) {
            Vector2 sPos = camera.WorldToScreen(bp.pos);
            if (sPos.x < -10 || sPos.x > 810 || sPos.y < -10 || sPos.y > 610) continue;

            int alphaVal = static_cast<int>(bp.alpha * 255);
            if (alphaVal <= 0) continue;

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
            // 使用纯粹高饱和的鲜红色 (R=200, G=0, B=20)
            DrawCircle(static_cast<int>(sPos.x), static_cast<int>(sPos.y),
                static_cast<int>(bp.size), GetColor(200, 0, 20), TRUE);
        }
        //SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // 绘制水花粒子（粒子本身存的就是世界坐标，直接转屏幕即可，无需改动）
        for (const auto& p : particles) {
            Vector2 sParticlePos = camera.WorldToScreen(p.pos);
            int a = static_cast<int>(p.alpha * 200);
            if (a < 0) a = 0;
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, a);
            DrawCircle(static_cast<int>(sParticlePos.x), static_cast<int>(sParticlePos.y), static_cast<int>(p.size), GetColor(100, 220, 255), TRUE);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void Update(float dt) {
        // 1. 独立弹簧振动更新
        for (int i = 0; i < numPoints; i++) {
            float diff = points[i].y - points[i].targetY;
            float acceleration = -k * diff - damping * points[i].speed;
            points[i].speed += acceleration * dt * 60.0f; // 归一化到时间步长
            points[i].y += points[i].speed * dt;
        }

        // 2. 波传导扩散的核心算法 (进行多次迭代让传导更平滑)
        std::vector<float> leftDeltas(numPoints, 0.0f);
        std::vector<float> rightDeltas(numPoints, 0.0f);

        for (int j = 0; j < 8; j++) { // 迭代8次提高传导效率
            for (int i = 0; i < numPoints; i++) {
                if (i > 0) {
                    leftDeltas[i] = spread * (points[i].y - points[i - 1].y);
                    points[i - 1].speed += leftDeltas[i] * dt * 60.0f;
                }
                if (i < numPoints - 1) {
                    rightDeltas[i] = spread * (points[i].y - points[i + 1].y);
                    points[i + 1].speed += rightDeltas[i] * dt * 60.0f;
                }
            }
            // 将高度误差应用回位置修正
            for (int i = 0; i < numPoints; i++) {
                if (i > 0) points[i - 1].y += leftDeltas[i] * dt;
                if (i < numPoints - 1) points[i + 1].y += rightDeltas[i] * dt;
            }
        }

        // 3. 水花粒子物理更新 (受重力影响)
        for (size_t i = 0; i < particles.size(); ) {
            particles[i].vel.y += 800.0f * dt; // 粒子重力
            particles[i].pos += particles[i].vel * dt;
            particles[i].lifetime -= dt;
            particles[i].alpha = particles[i].lifetime / 1.0f; // 渐隐

            if (particles[i].lifetime <= 0.0f || particles[i].alpha <= 0.0f) {
                particles.erase(particles.begin() + i);
            }
            else {
                i++;
            }
        }

        // 4. 更新血雾粒子
        const float WATER_LINE_Y = 550.0f; // 你的水面高度

        for (auto it = bloodParticles.begin(); it != bloodParticles.end(); ) {
            it->currentLifetime -= dt;

            // 寿命耗尽，或者粒子已经高过水面，直接抹除
            if (it->currentLifetime <= 0.0f || it->pos.y < WATER_LINE_Y) {
                it = bloodParticles.erase(it);
            }
            else {
                // 1. 恒定的上浮感：给纵向施加一个柔和且持续的向上浮力加速度
                it->vel.y -= 30.0f * dt;
                // 纵向速度封顶，防止上冲太快
                if (it->vel.y < -90.0f) it->vel.y = -90.0f;

                // 2. 💡【真实上浮晃动】：推进每个粒子的摆动相位
                it->wavePhase += dt * 5.0f; // 摇摆频率
                // 在原有横向速度基础上，叠加一个微弱的正弦波右左晃动
                float swing = sinf(it->wavePhase) * 1.2f;

                // 3. 位移应用
                it->pos.y += it->vel.y * dt;
                it->pos.x += (it->vel.x + swing) * dt;

                // 4. 💡【水面渐变淡出机制】：
                // 当粒子距离水面小于 60 像素时，开始提前产生额外的线性淡出
                float distanceToSurface = it->pos.y - WATER_LINE_Y;
                float surfaceAlphaScale = 1.0f;
                if (distanceToSurface < 60.0f) {
                    surfaceAlphaScale = distanceToSurface / 60.0f; // 越接近 0，比例越趋近于 0
                }

                // 结合寿命衰减和水面逼近，算出最终的不透明度
                float lifeRatio = it->currentLifetime / it->maxLifetime;
                it->alpha = (lifeRatio * 0.25f) * surfaceAlphaScale;

                // 粒子本身不需要膨胀得太大，保持碎屑感
                it->size += dt * 0.5f;

                it++;
            }
        }
    }
};