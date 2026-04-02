#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "common.h"
#include "shader.h"
#include "program.h"
#include "buffer.h"
#include "vertex_layout.h"
#include "image.h"
#include "texture.h"
#include "mesh.h"
#include <vector>
#include <unordered_map>

// [Slot 0] Transform: VS에서 사용하는 행렬 데이터
struct TransformUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

// [Slot 1] Scene: PS에서 사용하는 조명 및 카메라 위치
struct Light {
    glm::vec3 position;
    float padding1;      // HLSL float3 뒤의 4바이트 정렬용
    glm::vec3 color;
    float padding2;      // HLSL float3 뒤의 4바이트 정렬용
};

struct SceneUBO {
    Light lights[4];
    glm::vec3 viewPos;
    float scenePadding;  // 16바이트 정렬용
};

// [Slot 2] Material: PBR 재질 파라미터
struct MaterialUBO {
    glm::vec3 albedo;    // 12
    float metallic;      // 4
    float roughness;     // 4
    float ao;            // 4
    float pad[2];        // 8 (반드시 추가!)
};

CLASS_PTR(Context)
class Context {
public:
    static ContextUPtr Create(ID3D11Device* device, ID3D11DeviceContext* context);
    void Render(ID3D11DeviceContext* context, uint32_t width, uint32_t height);
    void ProcessMouseMenu(float dx, float dy);
    void ProcessKeyboard(float deltatime);
    
    // DrawScene 함수 (내부 루프용)
    void DrawScene(ID3D11DeviceContext* context, const glm::mat4& view, const glm::mat4& projection);

private:
    Context() {}
    bool Init(ID3D11Device* device, ID3D11DeviceContext* context);

    // --- Resources ---
    // 프로그램 관리는 map으로 하거나 개별 변수로 관리 (여기선 직관적으로 변수 사용)
    ProgramUPtr m_brdfProgram;
    ProgramUPtr m_textureProgram;
    ProgramUPtr m_simpleProgram;

    // Mesh
    MeshUPtr m_box;
    MeshUPtr m_sphere;
    MeshUPtr m_plane;

    // Texture
    TexturePtr m_texture;
    TexturePtr m_texture_marble;

    // --- Constant Buffers (UBO) ---
    BufferUPtr m_transformBuffer; // b0
    BufferUPtr m_sceneBuffer;     // b1
    BufferUPtr m_materialBuffer;  // b2

    // --- Camera ---
    glm::vec3 m_cameraPos { 0.0f, 0.0f, 8.0f };
    glm::vec3 m_cameraFront { 0.0f, 0.0f, -1.0f };
    glm::vec3 m_cameraUp { 0.0f, 1.0f, 0.0f };

    float m_yaw = -90.0f; // 초기 정면 응시를 위해 -90도 설정
    float m_pitch = 0.0f;

    // --- Data for Update ---
    SceneUBO m_sceneData;
    glm::vec4 m_clearColor { 0.1f, 0.1f, 0.1f, 1.0f };
};

#endif