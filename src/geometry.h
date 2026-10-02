#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include "vec3.h"

constexpr float kInf = std::numeric_limits<float>::infinity();

struct Ray {
    Vec3 origin, dir;
    Vec3 at(float t) const { return origin + t * dir; }
};

enum class MatType : uint8_t { Lambertian, Metal, Dielectric };

struct Material {
    MatType type;
    Vec3 albedo;
    float fuzz;  // metal roughness
    float ior;   // dielectric index of refraction
};

struct Sphere {
    Vec3 center;
    float radius;
    int mat;  // index into the material table
};

struct HitRecord {
    float t = 0;
    Vec3 point, normal;
    bool frontFace = true;
    int mat = 0;
};

// Ray-sphere intersection (the hot path of the renderer).
inline bool hitSphere(const Sphere& s, const Ray& r, float tMin, float tMax, HitRecord& rec) {
    Vec3 oc = r.origin - s.center;
    float a = length_sq(r.dir);
    float halfB = dot(oc, r.dir);
    float c = length_sq(oc) - s.radius * s.radius;
    float disc = halfB * halfB - a * c;
    if (disc < 0) return false;

    float sq = std::sqrt(disc);
    float root = (-halfB - sq) / a;
    if (root < tMin || root > tMax) {
        root = (-halfB + sq) / a;
        if (root < tMin || root > tMax) return false;
    }

    rec.t = root;
    rec.point = r.at(root);
    Vec3 outward = (rec.point - s.center) / s.radius;
    rec.frontFace = dot(r.dir, outward) < 0;
    rec.normal = rec.frontFace ? outward : -outward;
    rec.mat = s.mat;
    return true;
}

// Axis-aligned bounding box.
struct AABB {
    Vec3 lo{kInf, kInf, kInf};
    Vec3 hi{-kInf, -kInf, -kInf};

    void expand(const AABB& o) {
        lo = {std::fmin(lo.x, o.lo.x), std::fmin(lo.y, o.lo.y), std::fmin(lo.z, o.lo.z)};
        hi = {std::fmax(hi.x, o.hi.x), std::fmax(hi.y, o.hi.y), std::fmax(hi.z, o.hi.z)};
    }

    static AABB ofSphere(const Sphere& s) {
        Vec3 r{s.radius, s.radius, s.radius};
        return {s.center - r, s.center + r};
    }

    // Slab test. invD = 1/direction, precomputed once per ray.
    bool hit(const Vec3& o, const Vec3& invD, float tMin, float tMax) const {
        for (int a = 0; a < 3; ++a) {
            float t0 = (lo[a] - o[a]) * invD[a];
            float t1 = (hi[a] - o[a]) * invD[a];
            if (invD[a] < 0) std::swap(t0, t1);
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMax <= tMin) return false;
        }
        return true;
    }
};
