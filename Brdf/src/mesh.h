#ifndef __MESH_H__
#define __MESH_H__
#include "common.h"
#include "buffer.h"
#include "vertex_layout.h"
#include "texture.h"
#include "program.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent; // [추가] 노멀 맵 연산용
};

CLASS_PTR(Material);
class Material {
public:
    static MaterialUPtr Create() {
        return MaterialUPtr(new Material());
    }
    TexturePtr diffuse;
    TexturePtr specular;
    float shininess { 32.0f };
    void SetToProgram(ID3D11DeviceContext* context) const;
private:
    Material() {}
};

CLASS_PTR(Mesh);
class Mesh {
public:
    static MeshUPtr Create(
        ID3D11Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        ID3DBlob* vsBlob,
        D3D11_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    static MeshUPtr CreateBox(ID3D11Device* device, ID3DBlob* vsBlob);
    static MeshUPtr CreateSphere(ID3D11Device* device, ID3DBlob* vsBlob,
        uint32_t latiSegmentCount = 16, uint32_t longiSegmentCount = 32);
    static MeshUPtr CreatePlane(ID3D11Device* device, ID3DBlob* vsBlob);


    void ComputeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

    const VertexLayout* GetVertexLayout() const { return m_vertexLayout.get(); }
    const Buffer* GetVertexBuffer() const { return m_vertexBuffer.get(); }
    const Buffer* GetIndexBuffer() const { return m_indexBuffer.get(); }
    void SetMaterial(MaterialPtr material) { m_material = material; }
    MaterialPtr GetMaterial() const { return m_material; }
    void Draw(ID3D11DeviceContext* context, const Program* program) const;

private:
    Mesh() {}
    void Init(
        ID3D11Device* device,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        ID3DBlob* vsBlob,
        D3D11_PRIMITIVE_TOPOLOGY primitiveType
    );

    MaterialPtr m_material;
    D3D11_PRIMITIVE_TOPOLOGY m_primitiveType { D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
    VertexLayoutUPtr m_vertexLayout;
    BufferUPtr m_vertexBuffer;
    BufferUPtr m_indexBuffer;
};
#endif