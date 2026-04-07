#ifndef __CONTEXT_H__
#define __CONTEXT_H__
#include "common.h"
#include "shader.h"
#include "compute_program.h"
#include "buffer.h"
#include "texture.h"
#include "mesh.h"
#include "model.h"
#include "bvh.h"
#include "scene_desc.h"  // GpuMaterial, GpuMeshInfo, SceneDesc

// -------------------------------------------------------
// GPU 상수 버퍼 - 카메라 파라미터
// -------------------------------------------------------
struct GlobalUniforms {
    glm::vec3 cameraPos;
    float     fov;
    glm::vec3 cameraFront;
    float     aspectRatio;
    glm::vec3 cameraUp;
    float     frameCount;
    glm::vec3 cameraRight;
    uint32_t  lightCount;  // 셰이더의 g_lightCount 와 일치
};

CLASS_PTR(Context)
class Context {
public:
    static ContextUPtr Create(ID3D11Device* device, ID3D11DeviceContext* context);

    void Render(ID3D11DeviceContext* context, uint32_t width, uint32_t height);
    void OnResize(ID3D11Device* device, uint32_t width, uint32_t height);
    void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);
    void ProcessMouseMenu(float dx, float dy);
    void ProcessKeyboard(float deltaTime);

private:
    Context() {}
    bool Init(ID3D11Device* device, ID3D11DeviceContext* context);

    // 씬 데이터를 통합 버퍼로 빌드
    bool BuildSceneBuffers(ID3D11Device* device);

    // --- Compute Resources ---
    ComputeProgramUPtr m_pathTracerProgram;

    // 출력 텍스처 (LDR 표시용 u1)
    ComPtr<ID3D11Texture2D>           m_outputTexture;
    ComPtr<ID3D11UnorderedAccessView> m_outputUAV;
    ComPtr<ID3D11ShaderResourceView>  m_outputSRV;

    // 누적 버퍼 (HDR 합산용 u0)
    ComPtr<ID3D11Texture2D>           m_accumTexture;
    ComPtr<ID3D11UnorderedAccessView> m_accumUAV;
    ComPtr<ID3D11ShaderResourceView>  m_accumSRV;

    // --- Scene Data ---
    ModelUPtr m_model; // Assimp 로딩 모델

    // 통합 GPU 버퍼 (모든 메시를 하나로 합친 것)
    BufferUPtr m_vertexBuffer;   // t0: 전체 버텍스
    BufferUPtr m_indexBuffer;    // t1: 전체 인덱스
    BufferUPtr m_meshInfoBuffer; // t2: 메시별 오프셋/카운트
    BufferUPtr m_materialBuffer; // t3: 메시별 재질
    BufferUPtr m_bvhNodeBuffer;  // t4: BVH 노드
    BufferUPtr m_bvhPrimBuffer;  // t5: BVH 프리미티브
    BufferUPtr m_lightBuffer;    // t6: NEE 광원

    // SRV 캐싱
    ComPtr<ID3D11ShaderResourceView> m_vertexSRV;
    ComPtr<ID3D11ShaderResourceView> m_indexSRV;
    ComPtr<ID3D11ShaderResourceView> m_meshInfoSRV;
    ComPtr<ID3D11ShaderResourceView> m_materialSRV;
    ComPtr<ID3D11ShaderResourceView> m_bvhNodeSRV;
    ComPtr<ID3D11ShaderResourceView> m_bvhPrimSRV;
    ComPtr<ID3D11ShaderResourceView> m_lightSRV;

    uint32_t m_meshCount  { 0 };
    uint32_t m_lightCount { 0 };

    // 상수 버퍼
    BufferUPtr m_globalBuffer;

    // --- Camera State ---
    glm::vec3 m_cameraPos   { 0.0f, 2.5f, -6.0f };  // 거리 끝에서 +Z 방향으로
    glm::vec3 m_cameraFront { 0.0f, 0.0f,  1.0f };
    glm::vec3 m_cameraUp    { 0.0f, 1.0f,  0.0f };
    float m_yaw   = 90.0f;   // +Z 방향 (거리 안쪽)
    float m_pitch =  0.0f;

    float m_cameraSpeed      = 5.0f;
    float m_mouseSensitivity = 0.1f;

    uint32_t m_frameCount = 0;
};
#endif