// HDR 누적 버퍼 → LDR 백버퍼 변환
Texture2D<float4>           g_hdrInput  : register(t10);
RWTexture2D<unorm float4>   g_ldrOutput : register(u1);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    uint w, h;
    g_ldrOutput.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float3 hdr = g_hdrInput[id.xy].rgb;

    // Reinhard 톤맵핑
    float3 ldr = hdr / (hdr + 1.0f);

    // 감마 보정
    ldr = pow(saturate(ldr), 1.0f / 2.2f);

    g_ldrOutput[id.xy] = float4(ldr, 1.0f);
}
