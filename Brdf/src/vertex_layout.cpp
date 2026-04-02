#include "vertex_layout.h"

VertexLayoutUPtr VertexLayout::Create(ID3D11Device* device, const std::vector<D3D11_INPUT_ELEMENT_DESC>& descs, ID3DBlob* vsBlob) {
    auto layout = VertexLayoutUPtr(new VertexLayout());
    if (!layout->Init(device, descs, vsBlob)) {
        return nullptr;
    }
    return std::move(layout);
}

VertexLayout::~VertexLayout() {}

bool VertexLayout::Init(ID3D11Device* device, const std::vector<D3D11_INPUT_ELEMENT_DESC>& descs, ID3DBlob* vsBlob) {
    HRESULT hr = device->CreateInputLayout(
        descs.data(), 
        (UINT)descs.size(), 
        vsBlob->GetBufferPointer(), 
        vsBlob->GetBufferSize(), 
        &m_layout
    );

    if (FAILED(hr)) {
        SPDLOG_ERROR("Failed to create Input Layout");
        return false;
    }
    return true;
}

void VertexLayout::Bind(ID3D11DeviceContext* context) const {
    context->IASetInputLayout(m_layout.Get());
}