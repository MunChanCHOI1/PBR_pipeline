#include "Common.hlsli"
#include "Intersection.hlsli"
#include "BRDF.hlsli"

// 최대 반사 횟수 (M3 Max라면 4~8회도 쾌적합니다)
static const int MAX_BOUNCES = 5;

float3 TracePath(Ray ray, uint2 pixelCoord) {
    float3 totalRadiance = float3(0, 0, 0);
    float3 throughput = float3(1, 1, 1); // 빛의 감쇄율 (에너지 보존)

    for (int bounce = 0; bounce < MAX_BOUNCES; ++bounce) {
        HitRecord rec;
        // 1. 가장 가까운 물체와 충돌 검사 (Intersection.hlsli 사용)
        if (!SceneIntersect(ray, rec)) {
            // 아무것도 안 부딪히면 배경색(Sky) 반환 후 종료
            totalRadiance += GetSkyColor(ray.direction) * throughput;
            break;
        }

        // 2. 부딪힌 물체가 광원(Emitter)인 경우
        if (IsEmitter(rec.material)) {
            totalRadiance += rec.material.emissive * throughput;
            break;
        }

        // 3. 다음 레이 방향 결정 (Importance Sampling - BRDF.hlsli 사용)
        float2 xi = GetRandomSamples(pixelCoord, bounce); // 난수 생성
        float3 V = -ray.direction;
        float3 N = rec.normal;
        
        // 중요도 샘플링으로 다음 레이 방향 L 결정
        float3 L = ImportanceSampleGGX(xi, N, rec.material.roughness);
        float3 H = normalize(V + L);

        // 4. 에너지 계산 및 가중치 보정 (PBRT 이론 적용)
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), rec.material.F0);
        float G = GeometrySmith(N, V, L, rec.material.roughness);
        float pdf = ComputePDF(N, V, L, H, rec.material.roughness);

        // [핵심] f * cos / pdf 로 throughput 갱신
        // Cook-Torrance Specular + Diffuse 조합
        float3 specular = (DistributionGGX(N, H, rec.material.roughness) * G * F) / 
                          (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001);
        float3 diffuse = (1.0 - F) * (1.0 - rec.material.metallic) * rec.material.albedo / PI;
        
        float3 f = (diffuse + specular) * max(dot(N, L), 0.0);
        throughput *= f / (pdf + 0.0001);

        // 5. 러시아 룰렛 (가중치가 너무 낮아지면 확률적으로 중단)
        float p = max(throughput.r, max(throughput.g, throughput.b));
        if (bounce > 2) {
            if (GetRandomFloat(pixelCoord) > p) break;
            throughput /= p; // 에너지 보존을 위한 증폭
        }

        // 다음 레이 설정
        ray.origin = rec.p + N * 0.001f; // Shadow Acne 방지 (노트 8p 오차 보정)
        ray.direction = L;
    }

    return totalRadiance;
}