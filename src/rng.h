#pragma once
#include <cstdint>
#include "vec3.h"

// Small xorshift RNG. Each pixel gets its own seeded instance, so output is
// bit-identical regardless of thread count (easy to regression-test).
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}

    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }  // [0,1)
    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }
};

inline uint32_t wangHash(uint32_t x) {
    x = (x ^ 61u) ^ (x >> 16);
    x *= 9u;
    x ^= x >> 4;
    x *= 0x27d4eb2du;
    x ^= x >> 15;
    return x;
}

inline Vec3 randomInUnitSphere(Rng& rng) {
    while (true) {
        Vec3 p{rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)};
        if (length_sq(p) < 1.0f) return p;
    }
}
inline Vec3 randomUnitVector(Rng& rng) { return unit(randomInUnitSphere(rng)); }
inline Vec3 randomInUnitDisk(Rng& rng) {
    while (true) {
        Vec3 p{rng.uniform(-1, 1), rng.uniform(-1, 1), 0};
        if (length_sq(p) < 1.0f) return p;
    }
}
