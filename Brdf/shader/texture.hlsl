#ifndef TEXTURE_HLSL
#define TEXTURE_HLSL

#include "common.hlsli"

Texture2D t_diffuse : register(t0);
SamplerState s_diffuse : register(s0);

struct VS_IN {
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD;
    float3 tangent: TANGENT;
};

struct PS_IN {
    float4 pos      : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

PS_IN VS(VS_IN input) {
    PS_IN output;
    
    // 2. 행렬 곱셈 순서를 (vector * matrix)로 통일합니다. (DX 표준)
    float4 worldPos = mul(float4(input.pos, 1.0f), model);
    output.worldPos = worldPos.xyz;
    
    // View와 Projection도 순서대로 곱해줍니다.
    float4 viewPos = mul(worldPos, view);
    output.pos = mul(viewPos, projection);
    
    // 노멀 계산 (스케일이 없다면 이대로 충분합니다)
    output.normal = mul(input.normal, (float3x3)model);
    output.uv = input.uv;
    
    return output;
}

float4 PS(PS_IN input) : SV_Target {
    // 3. 일단 텍스처가 나오는지 확인하기 위해 텍스처를 샘플링해봅시다.
    // 만약 샘플링이 안된다면 다시 빨간색으로 나오도록 아래 주석을 푸세요.
    float4 texColor = t_diffuse.Sample(s_diffuse, input.uv);
    return texColor; 
    // return float4(1.0f, 0.0f, 0.0f, 1.0f); 
}

#endif