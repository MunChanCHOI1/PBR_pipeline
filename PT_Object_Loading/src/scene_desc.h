#ifndef __SCENE_DESC_H__
#define __SCENE_DESC_H__
#include "common.h"
#include <vector>

// -------------------------------------------------------
// GPU StructuredBuffer 레이아웃
// (셰이더의 ShaderMaterial / ShaderMeshInfo 와 일치 필수)
// -------------------------------------------------------
struct GpuMaterial {
    glm::vec3 albedo;
    float     roughness;
    glm::vec3 emissive;
    float     metallic;
};

struct GpuMeshInfo {
    uint32_t vertexOffset;   // 전체 버텍스 배열에서 이 메시의 시작 인덱스
    uint32_t indexOffset;    // 전체 인덱스 배열에서 이 메시의 시작 인덱스
    uint32_t indexCount;     // 이 메시의 인덱스 수
    uint32_t materialIndex;  // 재질 배열에서의 인덱스
};

// -------------------------------------------------------
// 씬 디스크립터 — 순수 데이터, 지오메트리 생성 없음
// -------------------------------------------------------
struct BoxDesc {
    glm::vec3   lo, hi;
    GpuMaterial mat;
};

struct QuadDesc {
    glm::vec3   p0, p1, p2, p3;  // 정점 (CCW 순서)
    glm::vec3   n;                // 법선
    GpuMaterial mat;
};

struct LightDesc {
    glm::vec3 center;
    float     radius;
    glm::vec3 emission;
    float     _pad { 0.f };
};

struct SceneDesc {
    std::vector<BoxDesc>   boxes;
    std::vector<QuadDesc>  quads;
    std::vector<LightDesc> lights;
};

// -------------------------------------------------------
// 씬 팩토리 — scene_desc.cpp 에서 구현
// -------------------------------------------------------
SceneDesc MakeCityScene();

#endif
