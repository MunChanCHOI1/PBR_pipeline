#include "common.hlsli"

struct VS_IN {
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD;
    float3 tangent : TANGENT; // 레이아웃 일치
};

struct PS_IN {
    float4 pos      : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

PS_IN VS(VS_IN input) {
    PS_IN output;
    float4 worldPos = mul(float4(input.pos, 1.0f), model);
    output.worldPos = worldPos.xyz;
    output.pos = mul(mul(worldPos, view), projection);
    output.normal = normalize(mul(input.normal, (float3x3)model));
    output.uv = input.uv;
    return output;
}

float4 PS(PS_IN input) : SV_Target {
    // MaterialUBO에 정의된 m_albedo 사용
    return float4(m_albedo, 1.0f);
}