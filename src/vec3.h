#pragma once
#include <algorithm>
#include <cmath>

// 3-component single-precision vector. Floats keep memory traffic low and
// map cleanly onto SIMD lanes.
struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator-() const { return {-x, -y, -z}; }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator*=(float t) { x *= t; y *= t; z *= t; return *this; }
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3 operator*(const Vec3& a, float t) { return {a.x * t, a.y * t, a.z * t}; }
inline Vec3 operator*(float t, const Vec3& a) { return a * t; }
inline Vec3 operator/(const Vec3& a, float t) { return a * (1.0f / t); }

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length_sq(const Vec3& a) { return dot(a, a); }
inline float length(const Vec3& a) { return std::sqrt(length_sq(a)); }
inline Vec3 unit(const Vec3& a) { return a / length(a); }

inline bool near_zero(const Vec3& a) {
    const float s = 1e-6f;
    return std::fabs(a.x) < s && std::fabs(a.y) < s && std::fabs(a.z) < s;
}

inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - 2.0f * dot(v, n) * n; }

inline Vec3 refract(const Vec3& uv, const Vec3& n, float etaiOverEtat) {
    float cosTheta = std::min(dot(-uv, n), 1.0f);
    Vec3 perp = etaiOverEtat * (uv + cosTheta * n);
    Vec3 parallel = -std::sqrt(std::fabs(1.0f - length_sq(perp))) * n;
    return perp + parallel;
}
