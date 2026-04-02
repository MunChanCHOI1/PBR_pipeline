#ifndef __MESH_H__
#define __MESH_H__
#include "common.h"
#include "buffer.h"

// 셰이더의 StructuredBuffer<Vertex>와 1:1 대응
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
};

// 재질 데이터 (컴퓨트 셰이더에서 광선 반사 계산 시 사용)
struct MaterialData {
    glm::vec3 albedo;
    float roughness;
    glm::vec3 emissive;
    float metallic;
};

CLASS_PTR(Mesh);
class Mesh {
public:
    static MeshUPtr Create(
        ID3D11Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    );

    static MeshUPtr CreateBox(ID3D11Device* device);
    static MeshUPtr CreateSphere(ID3D11Device* device, uint32_t lati = 16, uint32_t longi = 32);
    static MeshUPtr CreatePlane(ID3D11Device* device);

    static void ComputeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    // SRV는 Buffer가 소유하므로 Buffer를 통해 접근
    ID3D11ShaderResourceView* GetVertexSRV() const { return m_vertexSRV.Get(); }
    ID3D11ShaderResourceView* GetIndexSRV()  const { return m_indexSRV.Get(); }

    uint32_t GetIndexCount() const { return m_indexCount; }
    void SetMaterial(const MaterialData& material) { m_materialData = material; }
    const MaterialData& GetMaterial() const { return m_materialData; }

private:
    Mesh() {}
    bool Init(
        ID3D11Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices
    );

    // [수정] ComPtr<ID3D11Buffer> → BufferUPtr
    // Buffer 객체가 수명을 관리하고, SRV는 별도 ComPtr에 캐싱
    BufferUPtr m_vertexBuffer;
    BufferUPtr m_indexBuffer;

    ComPtr<ID3D11ShaderResourceView> m_vertexSRV;
    ComPtr<ID3D11ShaderResourceView> m_indexSRV;

    MaterialData m_materialData;
    uint32_t m_indexCount { 0 };
};
#endif