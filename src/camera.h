#pragma once
#include <cmath>
#include "geometry.h"
#include "rng.h"

// Thin-lens camera with depth of field.
struct Camera {
    Vec3 origin, lowerLeft, horizontal, vertical, u, v, w;
    float lensRadius;

    Camera(Vec3 lookFrom, Vec3 lookAt, Vec3 up, float vfovDeg, float aspect, float aperture,
           float focusDist) {
        float theta = vfovDeg * 3.14159265358979f / 180.0f;
        float h = std::tan(theta / 2);
        float viewH = 2.0f * h;
        float viewW = aspect * viewH;

        w = unit(lookFrom - lookAt);
        u = unit(cross(up, w));
        v = cross(w, u);

        origin = lookFrom;
        horizontal = focusDist * viewW * u;
        vertical = focusDist * viewH * v;
        lowerLeft = origin - horizontal / 2 - vertical / 2 - focusDist * w;
        lensRadius = aperture / 2;
    }

    Ray getRay(float s, float t, Rng& rng) const {
        Vec3 rd = lensRadius * randomInUnitDisk(rng);
        Vec3 offset = u * rd.x + v * rd.y;
        return {origin + offset, lowerLeft + s * horizontal + t * vertical - origin - offset};
    }
};
