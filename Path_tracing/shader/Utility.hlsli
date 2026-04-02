#ifndef __UTILITY_HLSLI__
#define __UTILITY_HLSLI__

// -------------------------------------------------------
// 난수 생성 (PCG Hash 기반 - GPU에서 흔히 쓰는 방식)
// -------------------------------------------------------

// 32비트 PCG Hash
uint PCGHash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// [0, 1) 범위 float 난수
float UintToFloat01(uint h) {
    return (float)(h & 0x00FFFFFFu) / (float)0x01000000u;
}

// 픽셀 좌표 + 바운스 + 프레임 카운트를 시드로 사용
// b0의 frameCount와 조합하여 매 프레임 다른 시퀀스를 생성
float GetRandomFloat(uint2 pixelCoord, uint bounce, uint frameCount) {
    uint seed = pixelCoord.x * 1973u + pixelCoord.y * 9277u 
              + bounce * 26699u + frameCount * 36271u;
    return UintToFloat01(PCGHash(seed));
}

// 독립적인 두 난수 (xi.x, xi.y) 반환 - 중요도 샘플링용
float2 GetRandomSamples(uint2 pixelCoord, uint bounce, uint frameCount) {
    uint seed0 = pixelCoord.x * 1973u + pixelCoord.y * 9277u
               + bounce * 26699u + frameCount * 36271u;
    uint seed1 = pixelCoord.x * 2699u + pixelCoord.y * 5003u
               + bounce * 31337u + frameCount * 12763u;
    return float2(UintToFloat01(PCGHash(seed0)),
                  UintToFloat01(PCGHash(seed1)));
}

// -------------------------------------------------------
// 스카이 컬러 (그라디언트 HDR 하늘)
// -------------------------------------------------------
float3 GetSkyColor(float3 direction) {
    // 방향 벡터의 y 성분으로 지평선~하늘 그라디언트 결정
    float t = clamp(direction.y * 0.5f + 0.5f, 0.0f, 1.0f);

    // 지평선: 흰빛, 하늘: 연한 파랑
    float3 horizon = float3(1.0f, 1.0f, 1.0f);
    float3 zenith  = float3(0.4f, 0.6f, 1.0f);
    float3 sky     = lerp(horizon, zenith, t) * 3.0f;

    // 태양 디스크
    float3 sunDir = normalize(float3(0.4f, 0.8f, 0.3f));
    float  sunDot = dot(normalize(direction), sunDir);
    if (sunDot > 0.9995f) {
        sky += float3(20.0f, 18.0f, 14.0f); // 강한 태양
    } else if (sunDot > 0.998f) {
        float glow = (sunDot - 0.998f) / (0.9995f - 0.998f);
        sky += lerp(float3(0,0,0), float3(6.0f, 5.0f, 3.0f), glow);
    }

    return sky;
}

#endif
