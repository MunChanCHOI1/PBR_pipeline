#include "mesh.h"

MeshUPtr Mesh::Create(
    ID3D11Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    auto mesh = MeshUPtr(new Mesh());
    if (!mesh->Init(device, vertices, indices)) return nullptr;
    return std::move(mesh);
}

bool Mesh::Init(
    ID3D11Device* device,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices)
{
    std::vector<Vertex> finalVertices = vertices;
    ComputeTangents(finalVertices, indices);
    m_indexCount = (uint32_t)indices.size();

    // [수정] BufferUPtr로 받아서 SRV만 따로 캐싱
    // CreateWithData가 BufferUPtr을 반환하므로 타입이 일치함

    // 1. Vertex StructuredBuffer
    m_vertexBuffer = Buffer::CreateWithData(
        device,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        finalVertices.data(),
        sizeof(Vertex),
        (uint32_t)finalVertices.size(),
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
    );
    if (!m_vertexBuffer) return false;

    // 2. Index StructuredBuffer
    m_indexBuffer = Buffer::CreateWithData(
        device,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        indices.data(),
        sizeof(uint32_t),
        (uint32_t)indices.size(),
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
    );
    if (!m_indexBuffer) return false;

    // 3. SRV 생성 후 ComPtr에 캐싱 (매 프레임 CreateSRV 호출 방지)
    m_vertexSRV = m_vertexBuffer->CreateSRV(device);
    m_indexSRV  = m_indexBuffer->CreateSRV(device);

    return (m_vertexSRV && m_indexSRV);
}

// --- 기본 도형 생성 함수들 ---
MeshUPtr Mesh::CreatePlane(ID3D11Device* device) {
    std::vector<Vertex> vertices = {
        { glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0,1,0), glm::vec2(0,0), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.0f, -0.5f), glm::vec3(0,1,0), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.0f,  0.5f), glm::vec3(0,1,0), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.0f,  0.5f), glm::vec3(0,1,0), glm::vec2(0,1), glm::vec3(0) },
    };
    std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
    return Create(device, vertices, indices);
}

MeshUPtr Mesh::CreateBox(ID3D11Device* device) {
    std::vector<Vertex> vertices = {
        // Front (+Z)
        { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3( 0.5f,-0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0,0,1), glm::vec2(0,0), glm::vec3(0) },
        // Back (-Z)
        { glm::vec3( 0.5f,-0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f,-0.5f), glm::vec3(0,0,-1), glm::vec2(0,0), glm::vec3(0) },
        // Left (-X)
        { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(-1,0,0), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(-1,0,0), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(-1,0,0), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(-1,0,0), glm::vec2(0,0), glm::vec3(0) },
        // Right (+X)
        { glm::vec3( 0.5f,-0.5f, 0.5f), glm::vec3(1,0,0), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3( 0.5f,-0.5f,-0.5f), glm::vec3(1,0,0), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f,-0.5f), glm::vec3(1,0,0), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f, 0.5f), glm::vec3(1,0,0), glm::vec2(0,0), glm::vec3(0) },
        // Top (+Y)
        { glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0,1,0), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f, 0.5f), glm::vec3(0,1,0), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3( 0.5f, 0.5f,-0.5f), glm::vec3(0,1,0), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3(-0.5f, 0.5f,-0.5f), glm::vec3(0,1,0), glm::vec2(0,0), glm::vec3(0) },
        // Bottom (-Y)
        { glm::vec3(-0.5f,-0.5f,-0.5f), glm::vec3(0,-1,0), glm::vec2(0,1), glm::vec3(0) },
        { glm::vec3( 0.5f,-0.5f,-0.5f), glm::vec3(0,-1,0), glm::vec2(1,1), glm::vec3(0) },
        { glm::vec3( 0.5f,-0.5f, 0.5f), glm::vec3(0,-1,0), glm::vec2(1,0), glm::vec3(0) },
        { glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(0,-1,0), glm::vec2(0,0), glm::vec3(0) },
    };
    std::vector<uint32_t> indices;
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t b = face * 4;
        indices.insert(indices.end(), { b,b+1,b+2, b+2,b+3,b });
    }
    return Create(device, vertices, indices);
}

MeshUPtr Mesh::CreateSphere(ID3D11Device* device, uint32_t lati, uint32_t longi) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t i = 0; i <= lati; ++i) {
        float phi = glm::pi<float>() * i / lati;
        for (uint32_t j = 0; j <= longi; ++j) {
            float theta = 2.0f * glm::pi<float>() * j / longi;
            Vertex v;
            v.position = glm::vec3(
                sinf(phi) * cosf(theta),
                cosf(phi),
                sinf(phi) * sinf(theta)
            );
            v.normal   = v.position;
            v.texCoord = glm::vec2((float)j / longi, (float)i / lati);
            v.tangent  = glm::vec3(0.0f);
            vertices.push_back(v);
        }
    }
    for (uint32_t i = 0; i < lati; ++i) {
        for (uint32_t j = 0; j < longi; ++j) {
            uint32_t a = i * (longi + 1) + j;
            uint32_t b = a + longi + 1;
            indices.insert(indices.end(), { a, b, a+1, b, b+1, a+1 });
        }
    }
    return Create(device, vertices, indices);
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
        float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        if (fabsf(denom) < 1e-6f) continue; // 퇴화 삼각형 skip
        float f = 1.0f / denom;
        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangents[i1] += tangent; tangents[i2] += tangent; tangents[i3] += tangent;
    }
    for (size_t i = 0; i < vertices.size(); i++) {
        float len = glm::length(tangents[i]);
        vertices[i].tangent = (len > 1e-6f) ? tangents[i] / len : glm::vec3(1,0,0);
    }
}