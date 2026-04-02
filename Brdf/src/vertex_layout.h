#ifndef __VERTEX_LAYOUT_H__
#define __VERTEX_LAYOUT_H__

#include "common.h"

CLASS_PTR(VertexLayout)
class VertexLayout {
public:
    static VertexLayoutUPtr Create(ID3D11Device* device, const std::vector<D3D11_INPUT_ELEMENT_DESC>& descs, ID3DBlob* vsBlob);
    ~VertexLayout();
    
    void Bind(ID3D11DeviceContext* context) const;

private:
    VertexLayout() {}
    bool Init(ID3D11Device* device, const std::vector<D3D11_INPUT_ELEMENT_DESC>& descs, ID3DBlob* vsBlob);

    ComPtr<ID3D11InputLayout> m_layout;
};

#endif // __VERTEX_LAYOUT_H__