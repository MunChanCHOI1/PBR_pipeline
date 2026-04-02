#include "program.h"

ProgramUPtr Program::Create(ID3D11Device* device, ShaderPtr vs, ShaderPtr ps) {
    auto program = ProgramUPtr(new Program());
    if (!program->Init(device, vs, ps)) {
        return nullptr;
    }
    return std::move(program);
}

Program::~Program() {}

bool Program::Init(ID3D11Device* device, ShaderPtr vs, ShaderPtr ps) {

    if (!vs || !ps) return false;

    // Blob 데이터를 이용해 실제 GPU 셰이더 객체 생성
    HRESULT hrVS = device->CreateVertexShader(
        vs->GetBuffer()->GetBufferPointer(), 
        vs->GetBuffer()->GetBufferSize(), 
        nullptr, &m_vs
    );
    if (FAILED(hrVS)) {
        SPDLOG_ERROR("Failed to create Vertex Shader");
        return false;
    }

    HRESULT hrPS = device->CreatePixelShader(
        ps->GetBuffer()->GetBufferPointer(), 
        ps->GetBuffer()->GetBufferSize(), 
        nullptr, &m_ps
    );
    if (FAILED(hrPS)) {
        SPDLOG_ERROR("Failed to create Pixel Shader");
        return false;
    }

    if (!CreateDefaultSampler(device)) return false;

    return true;
}

void Program::Use(ID3D11DeviceContext* context) const {
    context->VSSetShader(m_vs.Get(), nullptr, 0);
    context->PSSetShader(m_ps.Get(), nullptr, 0);
    if (m_defaultSampler) {
        context->PSSetSamplers(0, 1, m_defaultSampler.GetAddressOf());
    }
}

bool Program::CreateDefaultSampler(ID3D11Device* device){

    // texture sampler desc 생성
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_ANISOTROPIC;
    desc.MaxAnisotropy = 16;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = device->CreateSamplerState(&desc, m_defaultSampler.GetAddressOf());

    return SUCCEEDED(hr);


}