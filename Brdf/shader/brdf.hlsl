#include "common.hlsli"

// 정점 데이터 구조
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

// --- PBR 수학 함수들 (Cook-Torrance) ---
static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return nom / max(PI * denom * denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// --- Shader Functions ---

PS_IN VS(VS_IN input) {
    PS_IN output;
    // common.hlsli에 정의된 b0(TransformUBO) 사용
    float4 worldPos = mul(float4(input.pos, 1.0f), model);
    output.worldPos = worldPos.xyz;
    output.pos = mul(mul(worldPos, view), projection);
    output.normal = normalize(mul(input.normal, (float3x3)model));
    output.uv = input.uv;
    return output;
}

float4 PS(PS_IN input) : SV_Target {
    float3 N = normalize(input.normal);
    float3 V = normalize(g_viewPos - input.worldPos); // b1(SceneUBO) 사용

    // 반사율 설정 (b2 MaterialUBO 사용)
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, m_albedo, m_metallic);

    float3 Lo = float3(0.0, 0.0, 0.0);
    for(int i = 0; i < 4; ++i) {
        float3 L = normalize(g_lights[i].position - input.worldPos);
        float3 H = normalize(V + L);
        float dist = length(g_lights[i].position - input.worldPos);
        float attenuation = 1.0 / (dist * dist);
        float3 radiance = g_lights[i].color * attenuation;

        float NDF = DistributionGGX(N, H, m_roughness);
        float G   = GeometrySmith(N, V, L, m_roughness);
        float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

        float3 kS = F;
        float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - m_metallic);

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3 specular = numerator / denominator;

        Lo += (kD * m_albedo / PI + specular) * radiance * max(dot(N, L), 0.0);
    }

    float3 ambient = float3(0.03, 0.03, 0.03) * m_albedo * m_ao;
    float3 color = ambient + Lo;

    // HDR 톤맵핑 & 감마 교정
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    return float4(color, 1.0);
}