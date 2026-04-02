#ifndef __SCENE_HLSLI__
#define __SCENE_HLSLI__

#include "Intersection.hlsli"

struct ShaderVertex {
    float3 position;
    float3 normal;
    float2 texCoord;
    float3 tangent;
    float  _pad;
};

struct ShaderMaterial {
    float3 albedo;
    float  roughness;
    float3 emissive;
    float  metallic;
};

StructuredBuffer<ShaderVertex> g_vertices : register(t0);
StructuredBuffer<uint>         g_indices  : register(t1);

struct SurfaceHit {
    float          t;
    float3         p;
    float3         normal;
    float3         bary;
    ShaderMaterial material;
    bool           frontFace;
};

void SetFaceNormal(Ray ray, float3 outwardNormal, inout SurfaceHit hit) {
    hit.frontFace = dot(ray.direction, outwardNormal) < 0.0f;
    hit.normal    = hit.frontFace ? outwardNormal : -outwardNormal;
}

struct SphereLight {
    float3 center;
    float  radius;
    float3 emission;
    float  _pad;
};

static const int NUM_LIGHTS = 1;

SphereLight GetLight(int idx) {
    SphereLight l;
    l.center   = float3(2, 2, 3);
    l.radius   = 1.0f;
    l.emission = float3(100.0f, 95.0f, 80.0f);
    l._pad     = 0;
    return l;
}

bool SceneIntersect(Ray ray, out SurfaceHit hit) {
    hit = (SurfaceHit)0;
    float tClosest = 1e30f;
    bool  hitAny   = false;

    // 금속 구
    HitRecord recSphere;
    if (IntersectSphere(ray, float3(0, 0, 2), 1.0f, recSphere)) {
        if (recSphere.t > 0.0001f && recSphere.t < tClosest) {
            tClosest              = recSphere.t;
            hitAny                = true;
            hit.t                 = recSphere.t;
            hit.p                 = recSphere.p;
            hit.normal            = recSphere.normal;
            hit.frontFace         = dot(ray.direction, recSphere.normal) < 0.0f;
            if (!hit.frontFace) hit.normal = -hit.normal;
            hit.material.albedo    = float3(0.9f, 0.8f, 0.6f);
            hit.material.roughness = 0.2f;
            hit.material.metallic  = 1.0f;
            hit.material.emissive  = float3(0, 0, 0);
        }
    }

    // 바닥 평면 (y = -1)
    float3 planeN    = float3(0, 1, 0);
    float  planeDist = 1.0f;
    float  denom     = dot(ray.direction, planeN);
    if (abs(denom) > 0.0001f) {
        float tPlane = -(dot(ray.origin, planeN) + planeDist) / denom;
        if (tPlane > 0.0001f && tPlane < tClosest) {
            tClosest              = tPlane;
            hitAny                = true;
            hit.t                 = tPlane;
            hit.p                 = ray.origin + tPlane * ray.direction;
            hit.normal            = planeN;
            hit.frontFace         = true;
            float2 uv             = hit.p.xz;
            float  checker        = (fmod(abs(floor(uv.x) + floor(uv.y)), 2.0f) < 1.0f) ? 1.0f : 0.0f;
            float3 col1           = float3(0.9f, 0.9f, 0.9f);
            float3 col2           = float3(0.3f, 0.3f, 0.3f);
            hit.material.albedo    = lerp(col2, col1, checker);
            hit.material.roughness = 0.8f;
            hit.material.metallic  = 0.0f;
            hit.material.emissive  = float3(0, 0, 0);
        }
    }

    // 광원 구
    for (int li = 0; li < NUM_LIGHTS; ++li) {
        SphereLight sl = GetLight(li);
        HitRecord recL;
        if (IntersectSphere(ray, sl.center, sl.radius, recL)) {
            if (recL.t > 0.0001f && recL.t < tClosest) {
                tClosest              = recL.t;
                hitAny                = true;
                hit.t                 = recL.t;
                hit.p                 = recL.p;
                hit.normal            = recL.normal;
                hit.frontFace         = true;
                hit.material.albedo    = float3(1, 1, 1);
                hit.material.roughness = 1.0f;
                hit.material.metallic  = 0.0f;
                hit.material.emissive  = sl.emission;
            }
        }
    }

    // 메시 삼각형
    uint triCount, dummy;
    g_indices.GetDimensions(triCount, dummy);
    triCount /= 3;
    for (uint i = 0; i < triCount; ++i) {
        uint i0 = g_indices[i * 3 + 0];
        uint i1 = g_indices[i * 3 + 1];
        uint i2 = g_indices[i * 3 + 2];
        HitRecord rec;
        if (IntersectTriangle(ray,
            g_vertices[i0].position,
            g_vertices[i1].position,
            g_vertices[i2].position, rec))
        {
            if (rec.t > 0.0001f && rec.t < tClosest) {
                tClosest = rec.t;
                hitAny   = true;
                hit.t    = rec.t;
                hit.p    = rec.p;
                hit.bary = rec.bary;
                float3 N = normalize(
                    g_vertices[i0].normal * rec.bary.x +
                    g_vertices[i1].normal * rec.bary.y +
                    g_vertices[i2].normal * rec.bary.z
                );
                SetFaceNormal(ray, N, hit);
                hit.material.albedo    = float3(0.8f, 0.8f, 0.8f);
                hit.material.roughness = 0.5f;
                hit.material.metallic  = 0.0f;
                hit.material.emissive  = float3(0, 0, 0);
            }
        }
    }
    return hitAny;
}

bool IsOccluded(float3 origin, float3 target) {
    float3 toTarget = target - origin;
    float  dist     = length(toTarget);
    Ray    sr;
    sr.origin    = origin;
    sr.direction = toTarget / dist;

    // 금속 구 차폐
    HitRecord rec;
    if (IntersectSphere(sr, float3(0, 0, 2), 1.0f, rec))
        if (rec.t > 0.01f && rec.t < dist - 0.01f) return true;

    // 바닥 평면 차폐
    float planeDenom = dot(sr.direction, float3(0, 1, 0));
    if (abs(planeDenom) > 0.0001f) {
        float tPlane = -(dot(sr.origin, float3(0, 1, 0)) + 1.0f) / planeDenom;
        if (tPlane > 0.001f && tPlane < dist - 0.001f) return true;
    }

    // 메시 차폐
    uint triCount, dummy;
    g_indices.GetDimensions(triCount, dummy);
    triCount /= 3;
    for (uint i = 0; i < triCount; ++i) {
        HitRecord tr;
        if (IntersectTriangle(sr,
            g_vertices[g_indices[i*3+0]].position,
            g_vertices[g_indices[i*3+1]].position,
            g_vertices[g_indices[i*3+2]].position, tr))
            if (tr.t > 0.001f && tr.t < dist - 0.001f) return true;
    }
    return false;
}

float3 SampleDirectLight(
    float3 hitP, float3 hitN, float3 V,
    ShaderMaterial mat, float2 xi, int lightIdx)
{
    SphereLight light = GetLight(lightIdx);
    if (light.radius <= 0.0f) return float3(0, 0, 0);

    float  phi  = 2.0f * 3.14159265f * xi.x;
    float  cosT = 1.0f - 2.0f * xi.y;
    float  sinT = sqrt(max(0.0f, 1.0f - cosT * cosT));
    float3 lN   = float3(sinT * cos(phi), sinT * sin(phi), cosT);
    float3 lPos = light.center + light.radius * lN;

    float3 toLight = lPos - hitP;
    float  distSq  = dot(toLight, toLight);
    float  dist    = sqrt(distSq);
    float3 L       = toLight / dist;

    float NdotL = dot(hitN, L);
    if (NdotL <= 0.0f) return float3(0, 0, 0);

    if (IsOccluded(hitP + hitN * 0.001f, lPos)) return float3(0, 0, 0);

    float lightArea  = 4.0f * 3.14159265f * light.radius * light.radius;
    float lightNdotL = abs(dot(lN, -L));
    float pdf        = distSq / max(lightArea * lightNdotL, 0.0001f);

    float3 H     = normalize(V + L);
    float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), mat.albedo, mat.metallic);
    float3 F     = F0 + (1.0f - F0) * pow(clamp(1.0f - max(dot(H, V), 0.0f), 0.0f, 1.0f), 5.0f);
    float  a     = mat.roughness * mat.roughness;
    float  a2    = a * a;
    float  NdotH = max(dot(hitN, H), 0.0f);
    float  NdotV = max(dot(hitN, V), 0.0f);
    float  d     = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
    float  NDF   = a2 / max(3.14159265f * d * d, 0.000001f);
    float  k     = ((mat.roughness + 1.0f) * (mat.roughness + 1.0f)) / 8.0f;
    float  G     = (NdotV / (NdotV * (1.0f - k) + k)) *
                   (NdotL / (NdotL * (1.0f - k) + k));

    float3 specular = (NDF * G * F) / (4.0f * NdotV * NdotL + 0.0001f);
    float3 kD       = (1.0f - F) * (1.0f - mat.metallic);
    float3 brdf     = (kD * mat.albedo / 3.14159265f + specular) * NdotL;

    return brdf * light.emission / (pdf + 0.0001f);
}

bool IsEmitter(ShaderMaterial mat) {
    return dot(mat.emissive, float3(1, 1, 1)) > 0.001f;
}

#endif