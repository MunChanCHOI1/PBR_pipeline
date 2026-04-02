#ifndef __CONTEXT_H__
#define __CONTEXT_H__
#include "common.h"
#include "shader.h"
#include "compute_program.h"
#include "buffer.h"
#include "texture.h"
#include "mesh.h"

// 패스 트레이싱용 상수 버퍼
struct GlobalUniforms {
    glm::vec3 cameraPos;
    float     fov;           // 라디안
    glm::vec3 cameraFront;
    float     aspectRatio;
    glm::vec3 cameraUp;
    float     frameCount;
    glm::vec3 cameraRight;
    float     _pad;
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
    void CreateOutputTexture(ID3D11Device* device, uint32_t width, uint32_t height);

    // --- Compute Resources ---
    ComputeProgramUPtr m_pathTracerProgram;
    ComputeProgramUPtr m_tonemapProgram;   // HDR→LDR 톤맵핑

    // 출력용 도화지 (UAV)
    ComPtr<ID3D11Texture2D>           m_outputTexture;  // 표시용 LDR
    ComPtr<ID3D11UnorderedAccessView> m_outputUAV;
    ComPtr<ID3D11ShaderResourceView>  m_outputSRV;

    // 누적용 HDR 버퍼 (합산)
    ComPtr<ID3D11Texture2D>           m_accumTexture;
    ComPtr<ID3D11UnorderedAccessView> m_accumUAV;
    ComPtr<ID3D11ShaderResourceView>  m_accumSRV;

    // --- Scene Data ---
    // [수정] Model 대신 Mesh 직접 보유 (테스트용 기본 도형)
    std::vector<MeshUPtr> m_meshes;
    BufferUPtr m_globalBuffer;

    // --- Camera State ---
    glm::vec3 m_cameraPos   { 0.0f, 5.0f, 10.0f };
    glm::vec3 m_cameraFront { 0.0f, 0.0f, -1.0f };
    glm::vec3 m_cameraUp    { 0.0f, 1.0f,  0.0f }; // [추가] lookAtLH에서 필요
    float m_yaw   = -90.0f;
    float m_pitch =  -45.0f;

    float m_cameraSpeed     = 5.0f;   // 키보드 이동 속도 (units/sec)
    float m_mouseSensitivity = 0.1f;  // 마우스 감도

    uint32_t m_frameCount = 0;
};
#endif