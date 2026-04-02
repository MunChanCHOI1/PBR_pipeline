#include "model.h"

ModelUPtr Model::Load(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& filepath,
    ID3DBlob* vsBlob)
{
    auto model = ModelUPtr(new Model());
    if (!model->LoadModel(device, context, filepath, vsBlob))
        return nullptr;
    return std::move(model);
}

bool Model::LoadModel(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::string& filepath,
    ID3DBlob* vsBlob)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate       |  // 삼각형으로 변환
        aiProcess_CalcTangentSpace  |  // tangent 자동 계산
        aiProcess_FlipUVs           |  // DX UV 좌표계 (위아래 반전)
        aiProcess_GenNormals        |  // 노말 없으면 자동 생성
        aiProcess_MakeLeftHanded    |  // DX 왼손 좌표계로 변환
        aiProcess_FlipWindingOrder     // DX 시계방향 와인딩
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        SPDLOG_ERROR("Assimp error: {}", importer.GetErrorString());
        return false;
    }

    m_directory = filepath.substr(0, filepath.find_last_of('/'));
    ProcessNode(device, context, scene->mRootNode, scene, vsBlob);
    return true;
}

void Model::ProcessNode(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    aiNode* node,
    const aiScene* scene,
    ID3DBlob* vsBlob)
{
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(ProcessMesh(device, context, mesh, scene, vsBlob));
    }
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessNode(device, context, node->mChildren[i], scene, vsBlob);
    }
}

MeshUPtr Model::ProcessMesh(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    aiMesh* mesh,
    const aiScene* scene,
    ID3DBlob* vsBlob)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // 버텍스 추출
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        Vertex v;
        v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        v.normal   = mesh->HasNormals()
            ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        v.texCoord = mesh->mTextureCoords[0]
            ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
            : glm::vec2(0.0f);
        v.tangent  = mesh->HasTangentsAndBitangents()
            ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z)
            : glm::vec3(0.0f);
        vertices.push_back(v);
    }

    // 인덱스 추출
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // 메시 생성
    auto newMesh = Mesh::Create(device, vertices, indices, vsBlob);

    // 텍스처 로드
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        auto mat = Material::Create();
        mat->diffuse  = LoadTexture(device, context, material, aiTextureType_DIFFUSE);
        mat->specular = LoadTexture(device, context, material, aiTextureType_SPECULAR);
        newMesh->SetMaterial(std::move(mat));
    }

    return newMesh;
}

TexturePtr Model::LoadTexture(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    aiMaterial* material,
    aiTextureType type)
{
    if (material->GetTextureCount(type) == 0)
        return nullptr;

    aiString str;
    material->GetTexture(type, 0, &str);
    std::string path = m_directory + "/" + str.C_Str();

    auto image = Image::Load(path);
    if (!image) {
        SPDLOG_ERROR("Failed to load texture: {}", path);
        return nullptr;
    }
    return Texture::CreateFromImage(device, context, image.get());
}

void Model::Draw(ID3D11DeviceContext* context, const Program* program) const {
    for (const auto& mesh : m_meshes)
        mesh->Draw(context, program);
}