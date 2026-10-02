#pragma once
#include <algorithm>
#include <vector>
#include "geometry.h"

// ---------------------------------------------------------------------------
// Acceleration structure #1: brute force. Test every sphere for every ray.
// O(N) per ray; used as the baseline for benchmarks.
// ---------------------------------------------------------------------------
struct LinearList {
    std::vector<Sphere> spheres;

    bool hit(const Ray& r, float tMin, float tMax, HitRecord& rec) const {
        HitRecord tmp;
        bool hitAny = false;
        float closest = tMax;
        for (const Sphere& s : spheres) {
            if (hitSphere(s, r, tMin, closest, tmp)) {
                hitAny = true;
                closest = tmp.t;
                rec = tmp;
            }
        }
        return hitAny;
    }
};

// ---------------------------------------------------------------------------
// Acceleration structure #2: Bounding Volume Hierarchy.
// A binary tree of boxes. If a ray misses a box, it skips everything inside.
// O(log N) per ray on average.
//
// Layout notes:
//  * Nodes are stored in one flat array in depth-first order, so a node's
//    left child is always at (index + 1). Only the right child needs an index.
//  * Spheres are reordered at build time so every leaf owns a contiguous
//    range [first, first + count) in `prims`.
// ---------------------------------------------------------------------------
struct BVH {
    struct Node {
        AABB box;
        int right;  // interior node: index of right child
        int first;  // leaf: first primitive index
        int count;  // leaf: number of primitives (0 => interior node)
    };

    std::vector<Node> nodes;
    std::vector<Sphere> prims;

    explicit BVH(std::vector<Sphere> spheres) : prims(std::move(spheres)) {
        nodes.reserve(prims.size() * 2);
        build(0, static_cast<int>(prims.size()));
    }

    bool hit(const Ray& r, float tMin, float tMax, HitRecord& rec) const {
        const Vec3 invD{1.0f / r.dir.x, 1.0f / r.dir.y, 1.0f / r.dir.z};

        int stack[64];  // explicit stack: no recursion, no heap allocation
        int sp = 0;
        stack[sp++] = 0;

        HitRecord tmp;
        bool hitAny = false;
        float closest = tMax;

        while (sp > 0) {
            const int idx = stack[--sp];
            const Node& n = nodes[idx];
            if (!n.box.hit(r.origin, invD, tMin, closest)) continue;

            if (n.count > 0) {
                for (int i = n.first; i < n.first + n.count; ++i) {
                    if (hitSphere(prims[i], r, tMin, closest, tmp)) {
                        hitAny = true;
                        closest = tmp.t;
                        rec = tmp;
                    }
                }
            } else {
                stack[sp++] = n.right;
                stack[sp++] = idx + 1;  // left child
            }
        }
        return hitAny;
    }

private:
    // Median split along the longest axis of the centroid bounds.
    int build(int start, int end) {
        const int idx = static_cast<int>(nodes.size());
        nodes.emplace_back();

        AABB box;
        AABB centroidBox;
        for (int i = start; i < end; ++i) {
            box.expand(AABB::ofSphere(prims[i]));
            centroidBox.expand({prims[i].center, prims[i].center});
        }

        const int n = end - start;
        if (n <= 2) {
            nodes[idx] = {box, 0, start, n};
            return idx;
        }

        Vec3 ext = centroidBox.hi - centroidBox.lo;
        int axis = (ext.x > ext.y && ext.x > ext.z) ? 0 : (ext.y > ext.z ? 1 : 2);

        const int mid = (start + end) / 2;
        std::nth_element(prims.begin() + start, prims.begin() + mid, prims.begin() + end,
                         [axis](const Sphere& a, const Sphere& b) {
                             return a.center[axis] < b.center[axis];
                         });

        build(start, mid);               // left child lands at idx + 1
        const int right = build(mid, end);
        nodes[idx] = {box, right, 0, 0};
        return idx;
    }
};
