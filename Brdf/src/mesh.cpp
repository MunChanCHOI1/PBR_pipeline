#include "mesh.h"

MeshUPtr Mesh::Create(
    ID3D11Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    ID3DBlob* vsBlob,
    D3D11_PRIMITIVE_TOPOLOGY primitiveType)
{
    auto mesh = MeshUPtr(new Mesh());
    mesh->Init(device, vertices, indices, vsBlob, primitiveType);
    return std::move(mesh);
}

void Mesh::Init(
    ID3D11Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    ID3DBlob* vsBlob,
    D3D11_PRIMITIVE_TOPOLOGY primitiveType)
{
    m_primitiveType = primitiveType;

    std::vector<Vertex> finalVertices = vertices;
    if (m_primitiveType == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
        ComputeTangents(finalVertices, indices);
    }

    m_vertexBuffer = Buffer::CreateWithData(
        device, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DEFAULT,
        finalVertices.data(), sizeof(Vertex), (uint32_t)finalVertices.size()
    );
    
    m_indexBuffer = Buffer::CreateWithData(
        device, D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_DEFAULT,
        indices.data(), sizeof(uint32_t), (uint32_t)indices.size()
    );

    std::vector<D3D11_INPUT_ELEMENT_DESC> layoutDesc = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, texCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    m_vertexLayout = VertexLayout::Create(device, layoutDesc, vsBlob);
}

void Mesh::Draw(ID3D11DeviceContext* context, const Program* program) const {
    m_vertexBuffer->Bind(context, 0);
    m_indexBuffer->Bind(context, 0);
    if (m_vertexLayout) m_vertexLayout->Bind(context);
    if (m_material) m_material->SetToProgram(context);
    context->IASetPrimitiveTopology(m_primitiveType);
    context->DrawIndexed((uint32_t)m_indexBuffer->GetCount(), 0, 0);
}

MeshUPtr Mesh::CreatePlane(ID3D11Device* device, ID3DBlob* vsBlob) {
    std::vector<Vertex> vertices = {
        { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 
        2, 3, 0,
    };

    return Create(device, vertices, indices, vsBlob, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

MeshUPtr Mesh::CreateSphere(ID3D11Device* device, ID3DBlob* vsBlob,
    uint32_t latiSegmentCount, uint32_t longiSegmentCount)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    uint32_t circleVertCount = longiSegmentCount + 1;
    vertices.resize((latiSegmentCount + 1) * circleVertCount);

    for (uint32_t i = 0; i <= latiSegmentCount; i++) {
        float v = (float)i / (float)latiSegmentCount;
        float phi = (v - 0.5f) * glm::pi<float>();
        auto cosPhi = cosf(phi);
        auto sinPhi = sinf(phi);
        for (uint32_t j = 0; j <= longiSegmentCount; j++) {
            float u = (float)j / (float)longiSegmentCount;
            float theta = u * glm::pi<float>() * 2.0f;
            auto cosTheta = cosf(theta);
            auto sinTheta = sinf(theta);
            auto point = glm::vec3(cosPhi * cosTheta, sinPhi, -cosPhi * sinTheta);

            vertices[i * circleVertCount + j] = Vertex {
                point * 0.5f,
                point,
                glm::vec2(u, v),
                glm::vec3(0.0f)
            };
        }
    }

    indices.resize(latiSegmentCount * longiSegmentCount * 6);
    for (uint32_t i = 0; i < latiSegmentCount; i++) {
        for (uint32_t j = 0; j < longiSegmentCount; j++) {
            uint32_t vertexOffset = i * circleVertCount + j;
            uint32_t indexOffset = (i * longiSegmentCount + j) * 6;
            indices[indexOffset    ] = vertexOffset;
            indices[indexOffset + 1] = vertexOffset + 1;
            indices[indexOffset + 2] = vertexOffset + 1 + circleVertCount;
            indices[indexOffset + 3] = vertexOffset;
            indices[indexOffset + 4] = vertexOffset + 1 + circleVertCount;
            indices[indexOffset + 5] = vertexOffset + circleVertCount;
        }
    }

    return Create(device, vertices, indices, vsBlob, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

MeshUPtr Mesh::CreateBox(ID3D11Device* device, ID3DBlob* vsBlob) {
    std::vector<Vertex> vertices = {
        // 앞면 (normal: 0, 0, -1)
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },

        // 뒷면 (normal: 0, 0, 1)
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },

        // 왼쪽면 (normal: -1, 0, 0)
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },

        // 오른쪽면 (normal: 1, 0, 0)
        { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3( 1.0f, 0.0f,  0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3( 1.0f, 0.0f,  0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3( 1.0f, 0.0f,  0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3( 1.0f, 0.0f,  0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },

        // 아랫면 (normal: 0, -1, 0)
        { glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f, -0.5f,  0.5f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },

        // 윗면 (normal: 0, 1, 0)
        { glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f, -0.5f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f) },
        { glm::vec3( 0.5f,  0.5f,  0.5f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f) },
        { glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f) },
    };

    std::vector<uint32_t> indices = {
         0,  2,  1,  2,  0,  3,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 14, 13, 14, 12, 15,
        16, 17, 18, 18, 19, 16,
        20, 22, 21, 22, 20, 23,
    };

    return Create(device, vertices, indices, vsBlob, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Mesh::ComputeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i1 = indices[i], i2 = indices[i+1], i3 = indices[i+2];
        Vertex& v1 = vertices[i1]; Vertex& v2 = vertices[i2]; Vertex& v3 = vertices[i3];

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;
        glm::vec2 deltaUV1 = v2.texCoord - v1.texCoord;
        glm::vec2 deltaUV2 = v3.texCoord - v1.texCoord;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        tangents[i1] += tangent; tangents[i2] += tangent; tangents[i3] += tangent;
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].tangent = glm::normalize(tangents[i]);
    }
}

void Material::SetToProgram(ID3D11DeviceContext* context) const {
    if (diffuse) diffuse->Bind(context, 0);
    if (specular) specular->Bind(context, 1);
    //if (normal)   normal->Bind(context, 2);
}