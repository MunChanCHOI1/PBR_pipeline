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
    float t = clamp(direction.y * 0.5f + 0.5f, 0.0f, 1.0f);

    // 야간 하늘: 짙은 남색
    float3 horizon = float3(0.02f, 0.02f, 0.05f);
    float3 zenith  = float3(0.005f, 0.005f, 0.02f);
    float3 sky     = lerp(horizon, zenith, t);

    // 달
    float3 moonDir = normalize(float3(-0.3f, 0.7f, 0.2f));
    float  moonDot = dot(normalize(direction), moonDir);
    if (moonDot > 0.9998f) {
        sky += float3(1.5f, 1.4f, 1.2f);
    } else if (moonDot > 0.990f) {
        float g = (moonDot - 0.990f) / (0.9998f - 0.990f);
        sky += lerp(float3(0,0,0), float3(0.04f, 0.04f, 0.03f), g);
    }

    return sky;
}

#endif
