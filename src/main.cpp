// CPU path tracer with a multithreaded tile renderer and BVH acceleration.
//   --accel list --threads 1   brute-force baseline
//   --accel list --threads N   multithreaded
//   --accel bvh  --threads N   BVH + multithreaded
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "accel.h"
#include "camera.h"
#include "png.h"
#include "renderer.h"

struct Scene {
    std::vector<Material> mats;
    std::vector<Sphere> spheres;
};

// Classic "lots of small random spheres + 3 big ones" scene.
// gridHalf = 11 gives ~480 spheres; raise it to stress the BVH.
static Scene makeScene(int gridHalf) {
    Scene sc;
    Rng rng(1337);  // fixed seed => identical scene every run

    auto addMat = [&](Material m) { sc.mats.push_back(m); return static_cast<int>(sc.mats.size()) - 1; };
    auto addSphere = [&](Vec3 c, float r, int m) { sc.spheres.push_back({c, r, m}); };

    int ground = addMat({MatType::Lambertian, {0.5f, 0.5f, 0.5f}, 0, 0});
    addSphere({0, -1000, 0}, 1000, ground);

    for (int a = -gridHalf; a < gridHalf; ++a) {
        for (int b = -gridHalf; b < gridHalf; ++b) {
            float choose = rng.uniform();
            Vec3 c{a + 0.9f * rng.uniform(), 0.2f, b + 0.9f * rng.uniform()};
            if (length(c - Vec3{4, 0.2f, 0}) <= 0.9f) continue;

            if (choose < 0.75f) {
                Vec3 albedo = Vec3{rng.uniform(), rng.uniform(), rng.uniform()} *
                              Vec3{rng.uniform(), rng.uniform(), rng.uniform()};
                addSphere(c, 0.2f, addMat({MatType::Lambertian, albedo, 0, 0}));
            } else if (choose < 0.92f) {
                Vec3 albedo{rng.uniform(0.5f, 1), rng.uniform(0.5f, 1), rng.uniform(0.5f, 1)};
                addSphere(c, 0.2f, addMat({MatType::Metal, albedo, rng.uniform(0, 0.5f), 0}));
            } else {
                addSphere(c, 0.2f, addMat({MatType::Dielectric, {1, 1, 1}, 0, 1.5f}));
            }
        }
    }

    addSphere({0, 1, 0}, 1.0f, addMat({MatType::Dielectric, {1, 1, 1}, 0, 1.5f}));
    addSphere({-4, 1, 0}, 1.0f, addMat({MatType::Lambertian, {0.4f, 0.2f, 0.1f}, 0, 0}));
    addSphere({4, 1, 0}, 1.0f, addMat({MatType::Metal, {0.7f, 0.6f, 0.5f}, 0.0f, 0}));
    return sc;
}

static void usage(const char* prog) {
    std::printf(
        "Usage: %s [options]\n"
        "  --width N      image width (default 800; height is 16:9)\n"
        "  --spp N        samples per pixel (default 50)\n"
        "  --depth N      max ray bounces (default 20)\n"
        "  --threads N    worker threads, 0 = all cores (default 0)\n"
        "  --accel MODE   list | bvh (default bvh)\n"
        "  --grid N       scene size: ~(2N)^2 small spheres (default 11)\n"
        "  --out FILE     output PNG path (default out.png)\n"
        "  --no-output    skip writing the image (pure benchmarking)\n",
        prog);
}

int main(int argc, char** argv) {
    RenderConfig cfg;
    std::string accel = "bvh";
    std::string outPath = "out.png";
    int grid = 11;
    bool writeOutput = true;

    for (int i = 1; i < argc; ++i) {
        auto arg = [&](const char* name) { return std::strcmp(argv[i], name) == 0; };
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) { usage(argv[0]); std::exit(1); }
            return argv[++i];
        };
        if (arg("--width")) cfg.width = std::atoi(next());
        else if (arg("--spp")) cfg.spp = std::atoi(next());
        else if (arg("--depth")) cfg.maxDepth = std::atoi(next());
        else if (arg("--threads")) cfg.threads = std::atoi(next());
        else if (arg("--accel")) accel = next();
        else if (arg("--grid")) grid = std::atoi(next());
        else if (arg("--out")) outPath = next();
        else if (arg("--no-output")) writeOutput = false;
        else { usage(argv[0]); return arg("--help") ? 0 : 1; }
    }
    cfg.height = static_cast<int>(cfg.width * 9 / 16);

    Scene sc = makeScene(grid);
    Camera cam({13, 2, 3}, {0, 0, 0}, {0, 1, 0}, 20.0f,
               static_cast<float>(cfg.width) / cfg.height, 0.1f, 10.0f);

    int threads = cfg.threads > 0 ? cfg.threads : static_cast<int>(std::thread::hardware_concurrency());
    std::printf("config: %dx%d  spp=%d  depth=%d  threads=%d  accel=%s  spheres=%zu\n", cfg.width,
                cfg.height, cfg.spp, cfg.maxDepth, threads, accel.c_str(), sc.spheres.size());

    std::vector<Vec3> fb;
    RenderStats stats;
    if (accel == "list") {
        LinearList world{sc.spheres};
        stats = render(world, sc.mats, cam, cfg, fb);
    } else if (accel == "bvh") {
        BVH world(sc.spheres);
        stats = render(world, sc.mats, cam, cfg, fb);
    } else {
        usage(argv[0]);
        return 1;
    }

    std::printf("result: time=%.3fs  rays=%.1fM  speed=%.2f Mrays/s\n", stats.seconds,
                stats.rays / 1e6, stats.rays / 1e6 / stats.seconds);

    if (writeOutput) {
        std::vector<uint8_t> rgb(static_cast<size_t>(cfg.width) * cfg.height * 3);
        for (size_t i = 0; i < fb.size(); ++i) {
            // gamma 2.0 correction, clamp, quantize
            auto q = [](float c) {
                c = std::sqrt(std::max(0.0f, c));
                return static_cast<uint8_t>(255.999f * std::min(c, 0.999f));
            };
            rgb[i * 3 + 0] = q(fb[i].x);
            rgb[i * 3 + 1] = q(fb[i].y);
            rgb[i * 3 + 2] = q(fb[i].z);
        }
        if (!writePNG(outPath, cfg.width, cfg.height, rgb)) {
            std::fprintf(stderr, "failed to write %s\n", outPath.c_str());
            return 1;
        }
        std::printf("wrote %s\n", outPath.c_str());
    }
    return 0;
}
