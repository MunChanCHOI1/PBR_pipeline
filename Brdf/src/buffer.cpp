#include "buffer.h"

// stride(개당 크기)와 count(개수)를 인자로 받도록 변경
BufferUPtr Buffer::CreateWithData(ID3D11Device* device, uint32_t bindFlags, 
                                  D3D11_USAGE usage, const void* data, 
                                  uint32_t stride, uint32_t count) {
    auto buffer = BufferUPtr(new Buffer());
    if (!buffer->Init(device, bindFlags, usage, data, stride, count)) {
        return nullptr;
    }
    return std::move(buffer);
}

Buffer::~Buffer() {}

bool Buffer::Init(ID3D11Device* device, uint32_t bindFlags, D3D11_USAGE usage, 
                  const void* data, uint32_t stride, uint32_t count) {
    m_bindFlags = bindFlags;
    m_stride = (uint32_t)stride;
    m_count = (uint32_t)count;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = usage;
    // [수정] 내부에서 전체 사이즈 계산 (stride * count)
    bd.ByteWidth = (uint32_t)(m_stride * m_count); 
    bd.BindFlags = bindFlags;
    bd.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;

    // 상수 버퍼(Constant Buffer)인 경우 16바이트 정렬 체크 
    if (bindFlags & D3D11_BIND_CONSTANT_BUFFER) {
        if (bd.ByteWidth % 16 != 0) {
            SPDLOG_ERROR("Constant Buffer size must be a multiple of 16 bytes. Current size: {}", bd.ByteWidth);
            return false;
        }
    }

    D3D11_SUBRESOURCE_DATA sd = {};
    if (data) {
        sd.pSysMem = data;
    }

    HRESULT hr = device->CreateBuffer(&bd, data ? &sd : nullptr, m_buffer.GetAddressOf());
    if (FAILED(hr)) {
        SPDLOG_ERROR("Failed to create DX11 Buffer (HR: 0x{:x})", (uint32_t)hr);
        return false;
    }
    return true;
}

void Buffer::Bind(ID3D11DeviceContext* context, uint32_t slot) const {
    uint32_t offset = 0;
    if (m_bindFlags & D3D11_BIND_VERTEX_BUFFER) {
        context->IASetVertexBuffers(slot, 1, m_buffer.GetAddressOf(), &m_stride, &offset);
    }
    else if (m_bindFlags & D3D11_BIND_INDEX_BUFFER) {
        // 인덱스 버퍼 바인딩 (보통 32비트 uint 사용)
        context->IASetIndexBuffer(m_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
    else if (m_bindFlags & D3D11_BIND_CONSTANT_BUFFER) {
        // 상수 버퍼 바인딩 (VS와 PS 둘 다 걸어주는 것이 일반적)
        context->VSSetConstantBuffers(slot, 1, m_buffer.GetAddressOf());
        context->PSSetConstantBuffers(slot, 1, m_buffer.GetAddressOf());
    }
}