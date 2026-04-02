#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "common.h"

CLASS_PTR(Buffer)
class Buffer {
public:
    // dataSize 대신 stride(개당 크기)와 count(개수)를 받도록 변경
    static BufferUPtr CreateWithData(ID3D11Device* device, UINT bindFlags, 
                                     D3D11_USAGE usage, const void* data, 
                                     uint32_t stride, uint32_t count);
    ~Buffer();
    
    void Bind(ID3D11DeviceContext* context, UINT slot = 0) const;
    
    size_t GetStride() const { return m_stride; }
    size_t GetCount() const { return m_count; }
    size_t GetTotalSize() const { return m_stride * m_count; } // 전체 크기 계산 편의 메서드
    ID3D11Buffer* GetBuffer() const { return m_buffer.Get(); }

    template<typename T>
    void UpdateData(ID3D11DeviceContext* context, const T& data) {
        D3D11_MAPPED_SUBRESOURCE mapped {};
        context->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &data, sizeof(T));
        context->Unmap(m_buffer.Get(), 0);
    }



private:
    Buffer() = default;
    bool Init(ID3D11Device* device, UINT bindFlags, D3D11_USAGE usage, 
              const void* data, uint32_t stride, uint32_t count);

    ComPtr<ID3D11Buffer> m_buffer;
    UINT m_bindFlags{ 0 };
    uint32_t m_stride{ 0 };
    uint32_t m_count { 0 };
};



#endif