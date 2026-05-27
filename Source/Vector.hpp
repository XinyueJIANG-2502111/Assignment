#pragma once
#include <cmath>

struct Vector2 {
    float x, y;

    Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
    Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
    Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
    Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }

    float Dot(const Vector2& o) const { return x * o.x + y * o.y; }
    float Length() const { return std::sqrt(x * x + y * y); }
    Vector2 Normalized() const {
        float l = Length();
        return l > 0.0f ? Vector2{ x / l, y / l } : Vector2{ 0.0f, 0.0f };
    }
};

struct TrailPoint {
    Vector2 pos;
    float alpha;
};