#include "Common.hlsli"
#include "Utility.hlsli"
#include "Scene.hlsli"
#include "BRDF.hlsli"

cbuffer GlobalUB : register(b0) {
    float3 g_cameraPos;
    float  g_fov;
    float3 g_cameraFront;
    float  g_aspectRatio;
    float3 g_cameraUp;
    float  g_frameCount;
    float3 g_cameraRight;
    float  g_pad;
};

RWTexture2D<float4>       g_accum  : register(u0);
RWTexture2D<unorm float4> g_output : register(u1);

static const int MAX_BOUNCES     = 3;
static const int SAMPLES_PER_PIXEL = 1;

Ray GenerateCameraRay(uint2 pixelCoord, uint2 screenSize, uint frameCount) {
    float2 jitter = GetRandomSamples(pixelCoord, 0, frameCount) - 0.5f;
    float2 uv  = ((float2)pixelCoord + 0.5f + jitter) / (float2)screenSize;
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float  halfH = tan(g_fov * 0.5f);
    float  halfW = halfH * g_aspectRatio;
    float3 dir = normalize(
        g_cameraFront +
        g_cameraRight * (ndc.x * halfW) +
        g_cameraUp    * (ndc.y * halfH)
    );
    Ray ray;
    ray.origin    = g_cameraPos;
    ray.direction = dir;
    return ray;
}

float3 TracePath(Ray ray, uint2 pixelCoord, uint frameCount) {
    float3 totalRadiance = float3(0, 0, 0);
    float3 throughput    = float3(1, 1, 1);

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
        SurfaceHit hit;
        if (!SceneIntersect(ray, hit)) {
            totalRadiance += GetSkyColor(ray.direction) * throughput;
            break;
        }

        if (IsEmitter(hit.material)) {
            if (bounce == 0)
                totalRadiance += hit.material.emissive * throughput;
            break;
        }

        float3 N = hit.normal;
        float3 V = -ray.direction;

        // NEE
        for (int li = 0; li < NUM_LIGHTS; ++li) {
            float2 xiNEE = GetRandomSamples(pixelCoord, (uint)(bounce * 10 + li + 50), frameCount);
            float3 direct = SampleDirectLight(hit.p, N, V, hit.material, xiNEE, li);
            totalRadiance += clamp(direct * throughput, 0.0f, 50.0f);
        }

        // 간접광
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), hit.material.albedo, hit.material.metallic);
        float2 xi = GetRandomSamples(pixelCoord, (uint)bounce, frameCount);
        float3 H  = ImportanceSampleGGX(xi, N, hit.material.roughness);
        float3 L  = reflect(-V, H);

        float NdotL = dot(N, L);
        if (NdotL <= 0.0f) break;

        float  NdotV  = max(dot(N, V), 0.0f);
        float3 F      = FresnelSchlick(max(dot(H, V), 0.0f), F0);
        float  G      = GeometrySmith(N, V, L, hit.material.roughness);
        float  NDF    = DistributionGGX(N, H, hit.material.roughness);
        float  pdf    = ComputePDF(N, V, L, H, hit.material.roughness);

        float3 specular = (NDF * G * F) / (4.0f * NdotV * NdotL + 0.0001f);
        float3 kD       = (1.0f - F) * (1.0f - hit.material.metallic);
        float3 diffuse  = kD * hit.material.albedo / PI;

        float3 brdf = (diffuse + specular) * NdotL;
        throughput *= brdf / (pdf + 0.0001f);
        throughput  = min(throughput, float3(10.0f, 10.0f, 10.0f));

        if (bounce > 1) {
            float p = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.05f, 0.95f);
            if (GetRandomFloat(pixelCoord, (uint)bounce + 100u, frameCount) > p) break;
            throughput /= p;
        }

        ray.origin    = hit.p + N * 0.01f;
        ray.direction = L;
    }

    return totalRadiance;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID) {
    uint2 pixelCoord = dispatchThreadID.xy;
    uint screenW, screenH;
    g_output.GetDimensions(screenW, screenH);
    if (pixelCoord.x >= screenW || pixelCoord.y >= screenH) return;

    uint frameCount = (uint)g_frameCount;
    Ray ray = GenerateCameraRay(pixelCoord, uint2(screenW, screenH), frameCount);
    float3 newSample = TracePath(ray, pixelCoord, frameCount);

    if (any(isnan(newSample)) || any(isinf(newSample)))
        newSample = float3(0, 0, 0);
    newSample = clamp(newSample, 0.0f, 100.0f);

    if (frameCount == 0u) {
        g_accum[pixelCoord] = float4(newSample, 1.0f);
    } else {
        g_accum[pixelCoord] += float4(newSample, 0.0f);
    }

    float3 hdr = g_accum[pixelCoord].rgb / (float)(frameCount + 1u);
    float3 ldr = hdr / (hdr + 1.0f);
    ldr = pow(saturate(ldr), 1.0f / 2.2f);
    g_output[pixelCoord] = float4(ldr, 1.0f);
}