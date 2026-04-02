#ifndef __MODEL_H__
#define __MODEL_H__
#include "common.h"
#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

CLASS_PTR(Model)
class Model {
public:
    static ModelUPtr Load(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::string& filepath,
        ID3DBlob* vsBlob
    );

    void Draw(ID3D11DeviceContext* context, const Program* program) const;
    const std::vector<MeshUPtr>& GetMeshes() const { return m_meshes; }

private:
    Model() {}
    bool LoadModel(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::string& filepath,
        ID3DBlob* vsBlob
    );
    void ProcessNode(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        aiNode* node,
        const aiScene* scene,
        ID3DBlob* vsBlob
    );
    MeshUPtr ProcessMesh(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        aiMesh* mesh,
        const aiScene* scene,
        ID3DBlob* vsBlob
    );
    TexturePtr LoadTexture(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        aiMaterial* material,
        aiTextureType type
    );

    std::string m_directory;
    std::vector<MeshUPtr> m_meshes;
};
#endif