#include "common.hlsli"
#include "BRDF.hlsli" // PBR 함수는 BRDF.hlsli로 통합, PI도 여기서 정의됨

// [수정] static const float PI 제거 → BRDF.hlsli의 #define PI 사용
// static const float PI = 3.14159265359; ← 삭제

struct VS_IN {
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD;
    float3 tangent : TANGENT;
};

struct PS_IN {
    float4 pos      : SV_POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

PS_IN VS(VS_IN input) {
    PS_IN output;
    float4 worldPos  = mul(float4(input.pos, 1.0f), model);
    output.worldPos  = worldPos.xyz;
    output.pos       = mul(mul(worldPos, view), projection);
    output.normal    = normalize(mul(input.normal, (float3x3)model));
    output.uv        = input.uv;
    return output;
}

float4 PS(PS_IN input) : SV_Target {
    float3 N  = normalize(input.normal);
    float3 V  = normalize(g_viewPos - input.worldPos);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), m_albedo, m_metallic);

    float3 Lo = float3(0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        float3 L         = normalize(g_lights[i].position - input.worldPos);
        float3 H         = normalize(V + L);
        float  dist      = length(g_lights[i].position - input.worldPos);
        float  atten     = 1.0f / (dist * dist);
        float3 radiance  = g_lights[i].color * atten;

        float  NDF = DistributionGGX(N, H, m_roughness);
        float  G   = GeometrySmith(N, V, L, m_roughness);
        float3 F   = FresnelSchlick(max(dot(H, V), 0.0f), F0);

        float3 kD  = (1.0f - F) * (1.0f - m_metallic);
        float3 num = NDF * G * F;
        float  den = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;

        float3 specular = num / den;
        Lo += (kD * m_albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
    }

    float3 ambient = float3(0.03f, 0.03f, 0.03f) * m_albedo * m_ao;
    float3 color   = ambient + Lo;

    // HDR 톤맵핑 & 감마 교정
    color = color / (color + 1.0f);
    color = pow(color, 1.0f / 2.2f);

    return float4(color, 1.0f);
}
