#ifndef __SCENE_HLSLI__
#define __SCENE_HLSLI__

#include "Intersection.hlsli"

// -------------------------------------------------------
// GPU 버퍼 구조체 (C++의 GpuMeshInfo, GpuMaterial과 일치)
// -------------------------------------------------------
struct ShaderVertex {
    float3 position; float pad0;
    float3 normal;   float pad1;
    float2 texCoord; float2 pad2;
    float3 tangent;  float pad3;
};

struct ShaderMaterial {
    float3 albedo;
    float  roughness;
    float3 emissive;
    float  metallic;
};

struct ShaderMeshInfo {
    uint vertexOffset;   // 전체 버텍스 배열에서 이 메시의 시작
    uint indexOffset;    // 전체 인덱스 배열에서 이 메시의 시작
    uint indexCount;     // 이 메시의 인덱스 수
    uint materialIndex;  // 재질 배열 인덱스
};

// t0~t5: context.cpp에서 바인딩
StructuredBuffer<ShaderVertex>   g_vertices  : register(t0);
StructuredBuffer<uint>           g_indices   : register(t1);
StructuredBuffer<ShaderMeshInfo> g_meshInfos : register(t2);
StructuredBuffer<ShaderMaterial> g_materials : register(t3);

struct ShaderBvhNode {
    float3 aabbMin;
    uint   leftFirst; // primCount==0: 왼쪽 자식 인덱스, else: 첫 prim 인덱스
    float3 aabbMax;
    uint   primCount; // 0=내부 노드, >0=리프
};
struct ShaderBvhPrim {
    uint triOffset; // g_indices 내 삼각형 시작 (triIdx*3)
    uint meshIdx;
    uint pad0, pad1;
};
StructuredBuffer<ShaderBvhNode> g_bvhNodes : register(t4);
StructuredBuffer<ShaderBvhPrim> g_bvhPrims : register(t5);

// t6: NEE 광원 (C++의 LightDesc 와 메모리 레이아웃 일치)
struct ShaderLight {
    float3 center;
    float  radius;
    float3 emission;
    float  _pad;
};
StructuredBuffer<ShaderLight> g_lights : register(t6);

// -------------------------------------------------------
// SurfaceHit
// -------------------------------------------------------
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

// -------------------------------------------------------
// 씬 교차 검사
// -------------------------------------------------------
bool SceneIntersect(Ray ray, out SurfaceHit hit) {
    hit = (SurfaceHit)0;
    float tClosest = 1e30f;
    bool  hitAny   = false;

    // 바닥 평면 (y = 0) — 메시로 덮이지 않은 영역용 폴백
    float3 planeN    = float3(0, 1, 0);
    float  planeDist = 0.0f;
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
            hit.material.albedo    = lerp(float3(0.3f,0.3f,0.3f), float3(0.9f,0.9f,0.9f), checker);
            hit.material.roughness = 0.8f;
            hit.material.metallic  = 0.0f;
            hit.material.emissive  = float3(0, 0, 0);
        }
    }

    // 광원 구 (GPU 버퍼에서 동적으로 읽기)
    for (int li = 0; li < (int)g_lightCount; ++li) {
        ShaderLight sl = g_lights[li];
        HitRecord recL;
        if (IntersectSphere(ray, sl.center, sl.radius, recL)) {
            if (recL.t > 0.0001f && recL.t < tClosest) {
                tClosest               = recL.t;
                hitAny                 = true;
                hit.t                  = recL.t;
                hit.p                  = recL.p;
                hit.normal             = recL.normal;
                hit.frontFace          = true;
                hit.material.albedo    = float3(1, 1, 1);
                hit.material.roughness = 1.0f;
                hit.material.metallic  = 0.0f;
                hit.material.emissive  = sl.emission;
            }
        }
    }

    // -------------------------------------------------------
    // BVH 순회 (스택 기반 반복, O(log N))
    // -------------------------------------------------------
    uint bvhStack[64];
    int  bvhTop = 0;
    bvhStack[bvhTop++] = 0u; // 루트

    while (bvhTop > 0) {
        ShaderBvhNode node = g_bvhNodes[bvhStack[--bvhTop]];

        // AABB 교차 테스트
        float3 invD = 1.0f / ray.direction;
        float3 t0s  = (node.aabbMin - ray.origin) * invD;
        float3 t1s  = (node.aabbMax - ray.origin) * invD;
        float3 tMn  = min(t0s, t1s);
        float3 tMx  = max(t0s, t1s);
        float  tEnter = max(max(tMn.x, tMn.y), tMn.z);
        float  tExit  = min(min(tMx.x, tMx.y), tMx.z);
        if (tExit < 0.0001f || tEnter > tExit || tEnter > tClosest)
            continue;

        if (node.primCount > 0u) {
            // 리프: 삼각형 테스트
            for (uint p = 0u; p < node.primCount; ++p) {
                ShaderBvhPrim prim = g_bvhPrims[node.leftFirst + p];
                uint i0 = g_indices[prim.triOffset + 0];
                uint i1 = g_indices[prim.triOffset + 1];
                uint i2 = g_indices[prim.triOffset + 2];

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
                            g_vertices[i2].normal * rec.bary.z);
                        SetFaceNormal(ray, N, hit);
                        hit.material = g_materials[prim.meshIdx];
                    }
                }
            }
        } else {
            // 내부 노드: 자식 push
            if (bvhTop + 1 < 64) {
                bvhStack[bvhTop++] = node.leftFirst;
                bvhStack[bvhTop++] = node.leftFirst + 1u;
            }
        }
    }

    return hitAny;
}

// -------------------------------------------------------
// 섀도우 레이 (모든 오브젝트 차폐 검사)
// -------------------------------------------------------
bool IsOccluded(float3 origin, float3 target) {
    float3 toTarget = target - origin;
    float  dist     = length(toTarget);
    Ray    sr;
    sr.origin    = origin;
    sr.direction = toTarget / dist;

    // 바닥 평면 (y = 0)
    float pd = dot(sr.direction, float3(0,1,0));
    if (abs(pd) > 0.0001f) {
        float tp = -(dot(sr.origin, float3(0,1,0)) + 0.0f) / pd;
        if (tp > 0.001f && tp < dist - 0.001f) return true;
    }

    // BVH 섀도우 순회
    uint sStack[64];
    int  sTop = 0;
    sStack[sTop++] = 0u;

    while (sTop > 0) {
        ShaderBvhNode node = g_bvhNodes[sStack[--sTop]];

        float3 invD = 1.0f / sr.direction;
        float3 t0s  = (node.aabbMin - sr.origin) * invD;
        float3 t1s  = (node.aabbMax - sr.origin) * invD;
        float3 tMn  = min(t0s, t1s);
        float3 tMx  = max(t0s, t1s);
        float  tEnter = max(max(tMn.x, tMn.y), tMn.z);
        float  tExit  = min(min(tMx.x, tMx.y), tMx.z);
        if (tExit < 0.001f || tEnter > tExit || tEnter > dist - 0.001f)
            continue;

        if (node.primCount > 0u) {
            for (uint p = 0u; p < node.primCount; ++p) {
                ShaderBvhPrim prim = g_bvhPrims[node.leftFirst + p];
                HitRecord tr;
                if (IntersectTriangle(sr,
                    g_vertices[g_indices[prim.triOffset + 0]].position,
                    g_vertices[g_indices[prim.triOffset + 1]].position,
                    g_vertices[g_indices[prim.triOffset + 2]].position, tr))
                    if (tr.t > 0.001f && tr.t < dist - 0.001f) return true;
            }
        } else {
            if (sTop + 1 < 64) {
                sStack[sTop++] = node.leftFirst;
                sStack[sTop++] = node.leftFirst + 1u;
            }
        }
    }
    return false;
}

// -------------------------------------------------------
// NEE: 구형 광원 직접 샘플링
// -------------------------------------------------------
float3 SampleDirectLight(
    float3 hitP, float3 hitN, float3 V,
    ShaderMaterial mat, float2 xi, int lightIdx)
{
    ShaderLight light = g_lights[lightIdx];
    if (light.radius <= 0.0f) return float3(0, 0, 0);

    float  phi  = 2.0f * 3.14159265f * xi.x;
    float  cosT = 1.0f - 2.0f * xi.y;
    float  sinT = sqrt(max(0.0f, 1.0f - cosT * cosT));
    float3 lN   = float3(sinT*cos(phi), sinT*sin(phi), cosT);
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
    float3 F0    = lerp(float3(0.04f,0.04f,0.04f), mat.albedo, mat.metallic);
    float3 F     = F0 + (1.0f-F0) * pow(clamp(1.0f-max(dot(H,V),0.0f),0.0f,1.0f),5.0f);
    float  a     = mat.roughness * mat.roughness;
    float  a2    = a * a;
    float  NdotH = max(dot(hitN, H), 0.0f);
    float  NdotV = max(dot(hitN, V), 0.0f);
    float  d     = (NdotH*NdotH*(a2-1.0f)+1.0f);
    float  NDF   = a2 / max(3.14159265f*d*d, 0.000001f);
    float  k     = ((mat.roughness+1.0f)*(mat.roughness+1.0f)) / 8.0f;
    float  G     = (NdotV/(NdotV*(1.0f-k)+k)) * (NdotL/(NdotL*(1.0f-k)+k));

    float3 specular = (NDF*G*F) / (4.0f*NdotV*NdotL + 0.0001f);
    float3 kD       = (1.0f-F) * (1.0f-mat.metallic);
    float3 brdf     = (kD*mat.albedo/3.14159265f + specular) * NdotL;

    return brdf * light.emission / (pdf + 0.0001f);
}

bool IsEmitter(ShaderMaterial mat) {
    return dot(mat.emissive, float3(1,1,1)) > 0.001f;
}

#endif
