#pragma once
#pragma once

#include "Vector.hpp"
#include "Camera.hpp"
#include "DxLib.h"
#include <vector>
#include <set>
#include <cmath>
#include <cstdlib>

// 2D 轴对齐矩形地形 (AABB)
struct Obstacle {
    float x, y;          // 矩形左上角的世界坐标
    float width, height; // 矩形的宽高
    unsigned int color;  // 渲染颜色

    // 便捷获取矩形的四个边界，用于后续的碰撞和射线检测
    float Left() const { return x; }
    float Right() const { return x + width; }
    float Top() const { return y; }
    float Bottom() const { return y + height; }
};

// 射线检测结果结构体
struct RaycastResult {
    bool hit;           // 是否碰到了地形
    Vector2 hitPoint;   // 碰撞点的精确世界坐标
    float t;            // 射线碰撞时间（距离比例）
};

// 结构体：记录碰撞的详细信息
struct CollisionResult {
    bool collided;
    Vector2 pushVector; // 把玩家推出墙壁的最小位移向量
    Vector2 normal;     // 碰撞面的法线方向（上、下、左、右）
};

// 核心收集物结构体
struct CoreItem {
    float x, y;          // 世界坐标中心点
    float radius;        // 碰撞半径（比如 12.0f）
    unsigned int color;  // 核心的霓虹色彩
    bool isCollected;    // 是否已被吃掉
    float pulseTimer;    // 用于控制呼吸灯发光特效的计时器
};
struct EffectParticle {
    Vector2 pos;       // 当前世界坐标
    Vector2 velocity;  // 移动速度向量
    float alpha;       // 剩余生命值（不透明度 1.0 -> 0.0）
    unsigned int color;// 粒子颜色
};

// 检测单条射线是否穿过某个特定的矩形障碍物
inline RaycastResult RaycastToObstacle(const Vector2& rayStart, const Vector2& rayDir, float maxDist, const Obstacle& obs) {
    RaycastResult result = { false, {0,0}, maxDist };

    // 计算射线到矩形 X 轴边界的进入和离开时间
    float tXMin = (obs.Left() - rayStart.x) / rayDir.x;
    float tXMax = (obs.Right() - rayStart.x) / rayDir.x;
    if (tXMin > tXMax) std::swap(tXMin, tXMax);

    // 计算射线到矩形 Y 轴边界的进入和离开时间
    float tYMin = (obs.Top() - rayStart.y) / rayDir.y;
    float tYMax = (obs.Bottom() - rayStart.y) / rayDir.y;
    if (tYMin > tYMax) std::swap(tYMin, tYMax);

    // 如果两个轴的时间区间没有交集，说明射线完美错过了矩形
    if (tXMin > tYMax || tYMin > tXMax) return result;

    // 真正的进入时间是两个轴进入时间的最大值
    float tNear = (std::max)(tXMin, tYMin);
    // 真正的离开时间是两个轴离开时间的最小值
    float tFar = (std::min)(tXMax, tYMax);

    // 如果离开时间小于0，说明矩形在射线背后的反方向
    if (tFar < 0.0f) return result;

    // 如果进入时间超出了钩爪的最大射程，也算没抓到
    if (tNear > maxDist || tNear < 0.0f) return result;

    // 命中成功！
    result.hit = true;
    result.t = tNear;
    result.hitPoint = rayStart + rayDir * tNear; // 根据时间算出交点的世界坐标
    return result;
}

// 检测并计算玩家（圆形）与单个矩形障碍物的碰撞
inline CollisionResult CheckPlayerCollision(const Vector2& playerPos, float radius, const Obstacle& obs) {
    CollisionResult result = { false, {0,0}, {0,0} };

    // 1. 在矩形边界上找到离玩家中心最近的点 (Clamping)
    float closestX = (std::max)(obs.Left(), (std::min)(playerPos.x, obs.Right()));
    float closestY = (std::max)(obs.Top(), (std::min)(playerPos.y, obs.Bottom()));

    // 2. 计算玩家中心到这个最近点的距离和方向
    Vector2 diff = playerPos - Vector2{ closestX, closestY };
    float distance = diff.Length();

    // 如果最近点在玩家身体内部（或者完全重合），说明发生了碰撞
    // 特殊情况处理：如果玩家中心刚好完全在矩形正中央，diff.Length() 可能为 0
    if (distance < radius && distance > 0.001f) {
        result.collided = true;
        float overlap = radius - distance; // 穿透深度
        result.normal = diff.Normalized();
        result.pushVector = result.normal * overlap; // 推出向量
    }
    else if (distance == 0.0f) {
        // 极端防穿墙保险：如果完全重叠，判断离哪面墙最近，强制往外推
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

// 地形管理器
class TerrainManager {
public:
    const float CHUNK_SIZE = 400.0f; // 每个正方形区块的大小

    // 记录当前已经加载（实例化）的区块坐标，防止重复生成
    // 使用 std::pair<int, int> 存储 (CX, CY)
    std::set<std::pair<int, int>> loadedChunks;

public:
    std::vector<Obstacle> obstacles;
    std::vector<CoreItem> cores;

    float lastGeneratedX = 300.0f; // 记录上一次生成障碍物的最右侧边缘
    const float CHUNK_WIDTH = 400.0f; // 每个虚拟生成触发区间的宽度

    //【全新新增】：存储当前世界里正在飞散的特效粒子
    std::vector<EffectParticle> particles;

    //【全新新增】：触发粒子爆炸的公共接口
    void SpawnExplosion(Vector2 spawnPos, unsigned int coreColor) {
        // 瞬间向四周随机发射 16 个高能粒子
        const int particleCount = 16;
        for (int i = 0; i < particleCount; i++) {
            // 算出 360 度随机发射角度
            float angle = static_cast<float>(rand() % 360) * (3.1415926f / 180.0f);
            // 随机射出速度（每秒 150 到 400 像素位移）
            float speed = 150.0f + static_cast<float>(rand() % 250);

            Vector2 pVel = { std::cos(angle) * speed, std::sin(angle) * speed };

            // 注入粒子池（生命值 alpha 设为 1.0f）
            particles.push_back({ spawnPos, pVel, 1.0f, coreColor });
        }
    }

    void Init() {
        lastGeneratedX = 300.0f;

        obstacles.clear();
        cores.clear();
        particles.clear();
        loadedChunks.clear();

        // 出生点安全基石：不管怎么随机，确保 (0,0) 区块中心有个稳固的落脚点
        obstacles.push_back({ 200.0f, 500.0f, 400.0f, 40.0f, GetColor(110, 110, 120) });
        //loadedChunks.insert({ 0, 1 }); // 标记该区域已处理
    }

    //【全方位核心算法】：根据相机当前的中心点，加载周围的区块
    void UpdateAllDirections(Vector2 cameraPos) {
        // 1. 计算相机当前处于哪一个区块
        int currentCX = static_cast<int>(std::floor(cameraPos.x / CHUNK_SIZE));
        int currentCY = static_cast<int>(std::floor(cameraPos.y / CHUNK_SIZE));

        // 2. 扫描相机周围 5x5 的区块范围（上下左右各外延 2 个区块，确保视口外有缓冲）
        const int radius = 2;
        for (int cx = currentCX - radius; cx <= currentCX + radius; ++cx) {
            for (int cy = currentCY - radius; cy <= currentCY + radius; ++cy) {

                // 如果这个区块还没有被生成过
                if (loadedChunks.find({ cx, cy }) == loadedChunks.end()) {
                    GenerateChunk(cx, cy);
                    loadedChunks.insert({ cx, cy }); // 标记为已生成
                }
            }
        }

        // 3.【动态内存回收】：删除离相机太远（比如超过 4 个区块距离）的地形方块
        float maxActiveDistance = CHUNK_SIZE * 4.0f;
        for (auto it = obstacles.begin(); it != obstacles.end(); ) {
            // 计算方块中心到相机的距离
            float centerX = it->x + it->width * 0.5f;
            float centerY = it->y + it->height * 0.5f;
            float distToCamera = std::sqrt(std::pow(centerX - cameraPos.x, 2) + std::pow(centerY - cameraPos.y, 2));

            if (distToCamera > maxActiveDistance) {
                // 如果太远了，从物理世界隐去
                // 注意：由于我们把对应的 (CX, CY) 保留在 loadedChunks 中，所以玩家往回走时，这里暂时不会重复刷，
                // 如果想要实现完全的卸载与重新确定性读取，可以在此处同时 erase loadedChunks，但现在这样对性能最稳定。
                it = obstacles.erase(it);
            }
            else {
                it++;
            }
        }

        // 2.【全新新增】：动态回收太远的核心，或者已经吃掉的核心
        for (auto it = cores.begin(); it != cores.end(); ) {
            float distToCamera = std::sqrt(std::pow(it->x - cameraPos.x, 2) + std::pow(it->y - cameraPos.y, 2));
            if (distToCamera > maxActiveDistance || it->isCollected) {
                it = cores.erase(it);
            }
            else {
                it++;
            }
        }

        //【全新新增】：驱动特效粒子飞散并自然消亡
        const float dt = 0.008f;
        for (auto it = particles.begin(); it != particles.end(); ) {
            // 1. 位置累加速度
            it->pos += it->velocity * dt;

            // 2. 空气轻微阻力（让爆炸有一个从快到慢的顿挫感）
            it->velocity *= (1.0f - 2.5f * dt);

            // 3. 生命值淡出
            it->alpha -= 2.5f * dt;

            // 如果粒子完全透明了，从内存中抹去
            if (it->alpha <= 0.0f) {
                it = particles.erase(it);
            }
            else {
                it++;
            }
        }
    }

private:
    //【网格群落生成器】：修改为仅在水面上方（Y < 550f）生成地形
    void GenerateChunk(int cx, int cy) {
        // 计算该区块的世界坐标 Y 轴起点
        float chunkStartY = cy * CHUNK_SIZE;
        const float WATER_SURFACE_Y = 550.0f; // 你的水面绝对高度

        //【核心过滤 1】：如果整个区块的顶部都已经在水面以下了，直接清空返回
        if (chunkStartY >= WATER_SURFACE_Y) {
            return;
        }

        // 利用区块坐标作为唯一种子，保持确定性随机
        unsigned int seed = std::abs(cx * 73856093 ^ cy * 19349663);
        srand(seed);

        // 计算该区块的世界坐标 X 轴起点
        float chunkStartX = cx * CHUNK_SIZE;

        // 30% 的概率留白，让天空更开阔
        if (rand() % 100 < 30) return;

        //【全新精简群落】：只保留水面以上的“悬空浮岛”与“低空高架”
        int count = 1 + (rand() % 2); // 每个区块随机 1~2 个方块

        for (int i = 0; i < count; i++) {
            // 随机出方块的尺寸
            float w = 80.0f + (rand() % 140);
            float h = 30.0f + (rand() % 50);

            // 在当前区块网格内随机放置
            float rx = chunkStartX + (rand() % static_cast<int>(CHUNK_SIZE - w));
            float ry = chunkStartY + (rand() % static_cast<int>(CHUNK_SIZE - h));

            //【核心过滤 2】：双重保险，如果方块的底部越过了水面，强行削减其高度
            if (ry + h > WATER_SURFACE_Y) {
                // 方法 A：可以直接一刀切不生成这个尴尬的边缘方块
                // continue; 

                // 方法 B：对它进行修剪，让它刚好贴在水面上方
                h = WATER_SURFACE_Y - ry - 2.0f; // 留出 2 像素的微小空气间隙
                if (h < 15.0f) continue; // 如果修剪完太薄了，就直接扔掉
            }

            // 根据高度随机给点好看的黄灰色/遗迹青灰色
            unsigned int blockColor = (cy < 0) ? GetColor(140, 150, 160) : GetColor(110, 115, 100);

            // 塞入物理世界
            obstacles.push_back({ rx, ry, w, h, blockColor });

            // ========================================================
            // 【全新新增】：在生成的方块周围，概率生成“赛博核心”
            // ========================================================
            if (rand() % 100 < 60) { // 60% 的概率在这个方块附近刷出一个核心
                float coreX = rx + w * 0.5f; // 居中于方块 X 轴
                float coreY = 0.0f;

                if (rand() % 2 == 0) {
                    // 悬挂型：刷在方块下方 60 像素的空中
                    coreY = ry + h + 60.0f;
                }
                else {
                    // 顶托型：刷在方块上方 50 像素的空中
                    coreY = ry - 50.0f;
                }

                // 再次确保核心绝对不会掉进水里
                if (coreY < WATER_SURFACE_Y - 20.0f) {
                    // 核心颜色：调配一抹极其亮眼的亮粉色/高能橙，与蓝绿地形形成鲜明对比
                    unsigned int coreColor = GetColor(250, 0, 0);
                    cores.push_back({ coreX, coreY, 12.0f, coreColor, false, static_cast<float>(rand() % 100) * 0.01f });
                }
            }
        }
    }

public:
    // 绘制所有地形
    void Draw(const Camera& camera) {
        for (const auto& obs : obstacles) {
            // 1. 将世界坐标转换为屏幕坐标
            Vector2 topLeft = camera.WorldToScreen({ obs.x, obs.y });
            Vector2 bottomRight = camera.WorldToScreen({ obs.x + obs.width, obs.y + obs.height });

            int x1 = static_cast<int>(topLeft.x);
            int y1 = static_cast<int>(topLeft.y);
            int x2 = static_cast<int>(bottomRight.x);
            int y2 = static_cast<int>(bottomRight.y);

            // ========================================================
            // 【核心美化】：多重半透明叠加 —— 调制蓝绿色霓虹发光外圈
            // ========================================================

            // 调配标准的赛博朋克蓝绿霓虹色 (Cyan / Teal)
            unsigned int glowColor = GetColor(0, 240, 210);

            // 层的厚度和透明度经过严密的手感和视觉调校：
            // 外部光晕层 1：最外层，极粗（单边拓宽 6 像素），极其微弱
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 35);
            DrawBox(x1 - 6, y1 - 6, x2 + 6, y2 + 6, glowColor, FALSE);
            DrawBox(x1 - 5, y1 - 5, x2 + 5, y2 + 5, glowColor, FALSE);

            // 外部光晕层 2：过渡层，中等粗细（单边拓宽 3 像素），中等透明
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 75);
            DrawBox(x1 - 3, y1 - 3, x2 + 3, y2 + 3, glowColor, FALSE);
            DrawBox(x1 - 2, y1 - 2, x2 + 2, y2 + 2, glowColor, FALSE);

            // 外部光晕层 3：紧贴实体层，标准粗细（单边拓宽 1 像素），较亮
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
            DrawBox(x1 - 1, y1 - 1, x2 + 1, y2 + 1, glowColor, FALSE);

            // ========================================================
            // 【核心美化】：绘制最中心的“高亮灯丝”核心线
            // ========================================================
            // 核心线采用极浅的亮青白，能让发光处产生视觉中心的灼烧感
            unsigned int coreColor = GetColor(220, 255, 250);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255); // 100% 不透明实线
            DrawBox(x1, y1, x2, y2, coreColor, FALSE);

            // 【核心修改】：内部完全不填色 (去除了原本的 Fill Box 逻辑)
            // 这样整个地块中间就是完全透明的，背景的深海和发光拖尾能从方块内部完美透过来。
        }

        // 绘制完毕后，务必将 DxLib 的混合模式重置，防止影响后续的渲染
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        // ========================================================
        // 【全新新增】：绘制具有呼吸灯特效的赛博核心
        // ========================================================
        // 在每帧绘制时，由于我们需要动态改变核心的脉冲计时器，我们需要临时给 dt 做个简单的累加
        // 假定正常每帧 dt 约为 1/60s，我们直接让它动态呼吸
        for (auto& core : cores) {
            if (core.isCollected) continue;

            // 更新核心的自定义发光计时器
            core.pulseTimer += 0.05f;
            // 算出呼吸振幅 (在 0.8 到 1.3 之间循环)
            float pulseScale = 1.0f + std::sin(core.pulseTimer) * 0.25f;

            Vector2 sCorePos = camera.WorldToScreen({ core.x, core.y });
            int cx = static_cast<int>(sCorePos.x);
            int cy = static_cast<int>(sCorePos.y);
            int baseR = static_cast<int>(core.radius);

            // 1. 绘制外层扩散发光圈（半透明）
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
            DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale * 1.8f), core.color, TRUE);

            // 2. 绘制中层能量体
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 180);
            DrawCircle(cx, cy, static_cast<int>(baseR * pulseScale), core.color, FALSE);

            // 3. 绘制最中心的灼烧白点
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
            DrawCircle(cx, cy, 4, GetColor(255, 205, 205), TRUE);
            DrawCircle(cx, cy, 1, GetColor(255, 255, 255), TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0); // 恢复

        // ========================================================
        // 【全新新增】：绘制核心破碎的流光粒子特效
        // ========================================================
        for (const auto& p : particles) {
            // 将粒子的世界坐标转换为屏幕坐标
            Vector2 sPartPos = camera.WorldToScreen(p.pos);
            int px = static_cast<int>(sPartPos.x);
            int py = static_cast<int>(sPartPos.y);

            // 映射不透明度 (0 ~ 255)
            int alphaVal = static_cast<int>(p.alpha * 255);
            if (alphaVal < 0) alphaVal = 0;

            // 1. 绘制粒子外围的大柔光晕
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal / 3); // 较淡
            DrawCircle(px, py, 8, p.color, TRUE);

            // 2. 绘制粒子中心的不透明高亮骨架
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, alphaVal);
            DrawCircle(px, py, 3, GetColor(255, 255, 255), TRUE); // 纯白灯丝核心
            DrawCircle(px, py, 4, p.color, FALSE);                // 霓虹外圈
        }
    }
};