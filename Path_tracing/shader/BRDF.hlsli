#ifndef __BRDF_HLSLI__
#define __BRDF_HLSLI__

#include "Common.hlsli"

// [수정] PI 중복 정의 방지
// pbr.hlsl에도 static const float PI가 있으므로 매크로로 통일
#ifndef PI
#define PI 3.14159265359f
#endif

// -------------------------------------------------------
// 1. NDF: GGX 분포
// -------------------------------------------------------
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0f) + 1.0f);
    return a2 / max(PI * denom * denom, 0.000001f);
}

// -------------------------------------------------------
// 2. Geometry: Smith + Schlick-GGX
// -------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

// -------------------------------------------------------
// 3. Fresnel: Schlick
// -------------------------------------------------------
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// -------------------------------------------------------
// 4. GGX 중요도 샘플링
// 난수 xi로 하프벡터 H를 GGX 분포에 따라 샘플링
// -------------------------------------------------------
float3 ImportanceSampleGGX(float2 xi, float3 N, float roughness) {
    float a   = roughness * roughness;
    float phi = 2.0f * PI * xi.x;
    float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // 구면 → 직교
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // 접선 공간 → 월드 공간
    float3 up       = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangent  = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// -------------------------------------------------------
// 5. PDF 계산
// GGX 분포 기반 확률밀도함수
// -------------------------------------------------------
float ComputePDF(float3 N, float3 V, float3 L, float3 H, float roughness) {
    float NDF    = DistributionGGX(N, H, roughness);
    float NdotH  = max(dot(N, H), 0.0f);
    float VdotH  = max(dot(V, H), 0.0f);
    return (NDF * NdotH) / (4.0f * VdotH + 0.0001f);
}

#endif
