#ifndef __PROGRAM_H__
#define __PROGRAM_H__

#include "common.h"
#include "shader.h"

CLASS_PTR(Program)
class Program {
public:
    // 명확하게 VS와 PS를 따로 받도록 수정했습니다.
    static ProgramUPtr Create(ID3D11Device* device, ShaderPtr vs, ShaderPtr ps);
    ~Program();
    
    void Use(ID3D11DeviceContext* context) const;


private:
    Program() {}
    bool Init(ID3D11Device* device, ShaderPtr vs, ShaderPtr ps);
    bool CreateDefaultSampler(ID3D11Device* device);

    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader> m_ps;
    ComPtr<ID3D11SamplerState> m_defaultSampler;
};

#endif // __PROGRAM_H__