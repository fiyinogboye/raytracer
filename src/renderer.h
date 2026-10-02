#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#include "camera.h"
#include "geometry.h"
#include "rng.h"

struct RenderConfig {
    int width = 800;
    int height = 450;
    int spp = 50;       // samples per pixel
    int maxDepth = 20;  // max bounces
    int threads = 0;    // 0 => hardware_concurrency
};

struct RenderStats {
    double seconds = 0;
    uint64_t rays = 0;
};

// Schlick's approximation for glass reflectance.
inline float schlick(float cosine, float ior) {
    float r0 = (1 - ior) / (1 + ior);
    r0 *= r0;
    return r0 + (1 - r0) * std::pow(1 - cosine, 5.0f);
}

// Follow one ray through the scene (iteratively -- no recursion).
// `World` is any type with: bool hit(const Ray&, float, float, HitRecord&) const.
// Templating on it lets the compiler inline the accel structure's hit().
template <class World>
Vec3 rayColor(Ray r, const World& world, const std::vector<Material>& mats, Rng& rng,
              int maxDepth, uint64_t& rayCount) {
    Vec3 throughput{1, 1, 1};

    for (int depth = 0; depth < maxDepth; ++depth) {
        ++rayCount;
        HitRecord rec;
        if (!world.hit(r, 0.001f, kInf, rec)) {
            Vec3 d = unit(r.dir);
            float t = 0.5f * (d.y + 1.0f);
            Vec3 sky = (1.0f - t) * Vec3{1, 1, 1} + t * Vec3{0.5f, 0.7f, 1.0f};
            return throughput * sky;
        }

        const Material& m = mats[rec.mat];
        switch (m.type) {
            case MatType::Lambertian: {
                Vec3 dir = rec.normal + randomUnitVector(rng);
                if (near_zero(dir)) dir = rec.normal;
                r = {rec.point, dir};
                throughput = throughput * m.albedo;
                break;
            }
            case MatType::Metal: {
                Vec3 refl = reflect(unit(r.dir), rec.normal);
                Vec3 dir = refl + m.fuzz * randomInUnitSphere(rng);
                if (dot(dir, rec.normal) <= 0) return {0, 0, 0};  // absorbed
                r = {rec.point, dir};
                throughput = throughput * m.albedo;
                break;
            }
            case MatType::Dielectric: {
                float ratio = rec.frontFace ? (1.0f / m.ior) : m.ior;
                Vec3 ud = unit(r.dir);
                float cosT = std::min(dot(-ud, rec.normal), 1.0f);
                float sinT = std::sqrt(1.0f - cosT * cosT);
                bool cannotRefract = ratio * sinT > 1.0f;
                Vec3 dir = (cannotRefract || schlick(cosT, ratio) > rng.uniform())
                               ? reflect(ud, rec.normal)
                               : refract(ud, rec.normal, ratio);
                r = {rec.point, dir};
                break;
            }
        }
    }
    return {0, 0, 0};
}

// Tile-based multithreaded renderer.
// Threads grab 32x32 tiles from an atomic counter (dynamic load balancing:
// sky tiles are cheap, glass-sphere tiles are expensive, so static row
// splitting would leave threads idle).
template <class World>
RenderStats render(const World& world, const std::vector<Material>& mats, const Camera& cam,
                   const RenderConfig& cfg, std::vector<Vec3>& framebuffer) {
    framebuffer.assign(static_cast<size_t>(cfg.width) * cfg.height, Vec3{});

    constexpr int kTile = 32;
    const int tilesX = (cfg.width + kTile - 1) / kTile;
    const int tilesY = (cfg.height + kTile - 1) / kTile;
    const int totalTiles = tilesX * tilesY;

    std::atomic<int> nextTile{0};
    std::atomic<uint64_t> totalRays{0};

    auto worker = [&]() {
        uint64_t rays = 0;
        while (true) {
            const int tile = nextTile.fetch_add(1);
            if (tile >= totalTiles) break;

            const int x0 = (tile % tilesX) * kTile;
            const int y0 = (tile / tilesX) * kTile;
            const int x1 = std::min(x0 + kTile, cfg.width);
            const int y1 = std::min(y0 + kTile, cfg.height);

            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    const int pixel = y * cfg.width + x;
                    Rng rng(wangHash(static_cast<uint32_t>(pixel) + 1u));

                    Vec3 sum{0, 0, 0};
                    for (int s = 0; s < cfg.spp; ++s) {
                        float u = (x + rng.uniform()) / cfg.width;
                        float v = (cfg.height - 1 - y + rng.uniform()) / cfg.height;  // flip Y
                        Ray r = cam.getRay(u, v, rng);
                        sum += rayColor(r, world, mats, rng, cfg.maxDepth, rays);
                    }
                    framebuffer[pixel] = sum / static_cast<float>(cfg.spp);
                }
            }
        }
        totalRays += rays;
    };

    int nThreads = cfg.threads > 0 ? cfg.threads : static_cast<int>(std::thread::hardware_concurrency());
    nThreads = std::max(1, nThreads);

    auto t0 = std::chrono::steady_clock::now();
    if (nThreads == 1) {
        worker();  // run inline: a clean single-thread baseline
    } else {
        std::vector<std::thread> pool;
        for (int i = 0; i < nThreads; ++i) pool.emplace_back(worker);
        for (auto& t : pool) t.join();
    }
    auto t1 = std::chrono::steady_clock::now();

    return {std::chrono::duration<double>(t1 - t0).count(), totalRays.load()};
}
