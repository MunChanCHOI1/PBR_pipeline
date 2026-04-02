#include "context.h"

ContextUPtr Context::Create(ID3D11Device* device, ID3D11DeviceContext* ctx) {
    auto context = ContextUPtr(new Context());
    if (!context->Init(device, ctx)) return nullptr;
    return std::move(context);
}

bool Context::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
    // 1. 패스 트레이서 컴퓨트 셰이더 로드
    auto cs = Shader::CreateFromFile("shader/PathTracer.hlsl", "CSMain", "cs_5_0");
    if (!cs) return false;
    m_pathTracerProgram = ComputeProgram::Create(device, std::move(cs));
    if (!m_pathTracerProgram) return false;

    // 2. 글로벌 상수 버퍼 생성
    m_globalBuffer = Buffer::CreateWithData(
        device,
        D3D11_BIND_CONSTANT_BUFFER,
        D3D11_USAGE_DYNAMIC,
        nullptr,
        sizeof(GlobalUniforms),
        1
    );
    if (!m_globalBuffer) return false;

    // 3. 테스트용 기본 도형 생성 (모델 파일 없을 때)
    m_meshes.push_back(Mesh::CreateSphere(device));
    m_meshes.push_back(Mesh::CreatePlane(device));
    if (m_meshes.empty() || !m_meshes[0]) {
        SPDLOG_ERROR("Failed to create default meshes.");
        return false;
    }

    // 4. 출력용 도화지(UAV) 생성
    OnResize(device, WINDOW_WIDTH, WINDOW_HEIGHT);

    return true;
}

void Context::OnResize(ID3D11Device* device, uint32_t width, uint32_t height) {
    m_outputUAV.Reset();
    m_outputSRV.Reset();
    m_outputTexture.Reset();
    m_accumUAV.Reset();
    m_accumSRV.Reset();
    m_accumTexture.Reset();

    // 표시용 LDR 텍스처 (R8G8B8A8)
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&desc, nullptr, m_outputTexture.ReleaseAndGetAddressOf());
    device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, m_outputUAV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(m_outputTexture.Get(), nullptr, m_outputSRV.ReleaseAndGetAddressOf());

    // 누적용 HDR 버퍼 (R32G32B32A32_FLOAT - 합산용)
    desc.Format    = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&desc, nullptr, m_accumTexture.ReleaseAndGetAddressOf());
    device->CreateUnorderedAccessView(m_accumTexture.Get(), nullptr, m_accumUAV.ReleaseAndGetAddressOf());
    device->CreateShaderResourceView(m_accumTexture.Get(), nullptr, m_accumSRV.ReleaseAndGetAddressOf());

    m_frameCount = 0;
}

void Context::Render(ID3D11DeviceContext* context, uint32_t width, uint32_t height) {
    // 1. 카메라 행렬 계산
    glm::vec3 front;
    front.x = cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));
    front.y = sin(glm::radians(m_pitch));
    front.z = cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
    m_cameraFront = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(m_cameraFront, m_cameraUp));

    GlobalUniforms globalData;
    globalData.cameraPos   = m_cameraPos;
    globalData.fov         = glm::radians(45.0f);
    globalData.cameraFront = m_cameraFront;
    globalData.aspectRatio = (float)width / (float)height;
    globalData.cameraUp    = m_cameraUp;
    globalData.frameCount  = (float)m_frameCount++;
    globalData.cameraRight = right;
    globalData._pad        = 0.0f;

    m_globalBuffer->UpdateData(context, globalData);

    // 2. 상수 버퍼 바인딩 (b0)
    auto gBuf = m_globalBuffer->GetBuffer();
    context->CSSetConstantBuffers(0, 1, &gBuf);

    // 3. 메시 SRV 바인딩
    // [수정] 루프마다 덮어쓰지 않고 모든 SRV를 배열로 한 번에 바인딩
    // (6번 버그 수정 포함)
    const auto& meshes = m_meshes;
    std::vector<ID3D11ShaderResourceView*> vSRVs, iSRVs;
    vSRVs.reserve(meshes.size());
    iSRVs.reserve(meshes.size());

    for (const auto& mesh : meshes) {
        vSRVs.push_back(mesh->GetVertexSRV());
        iSRVs.push_back(mesh->GetIndexSRV());
    }

    // t0~ : 버텍스 버퍼들, t(N)~ : 인덱스 버퍼들
    context->CSSetShaderResources(0,               (UINT)vSRVs.size(), vSRVs.data());
    context->CSSetShaderResources((UINT)vSRVs.size(), (UINT)iSRVs.size(), iSRVs.data());

    // 출력 UAV 바인딩 (u0 = accum, u1 = output LDR은 tonemap에서)
    ID3D11UnorderedAccessView* uavs[2] = {
        m_accumUAV.Get(),
        m_outputUAV.Get()
    };
    context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

    // 5. Dispatch 실행
    // [수정] Use() 호출 제거 → Dispatch() 내부에서 CSSetShader까지 처리
    uint32_t gx = (width  + 7) / 8;
    uint32_t gy = (height + 7) / 8;
    m_pathTracerProgram->Dispatch(context, gx, gy, 1);

    // 6. 리소스 해제 (다음 프레임 간섭 방지)
    ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
    context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);

    std::vector<ID3D11ShaderResourceView*> nullSRVs(vSRVs.size() + iSRVs.size(), nullptr);
    context->CSSetShaderResources(0, (UINT)nullSRVs.size(), nullSRVs.data());
}

void Context::Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv) {
    ID3D11Resource* backBufferRes = nullptr;
    rtv->GetResource(&backBufferRes);
    if (backBufferRes) {
        context->CopyResource(backBufferRes, m_outputTexture.Get());
        backBufferRes->Release();
    }
}

void Context::ProcessMouseMenu(float dx, float dy) {
    m_yaw   += dx * m_mouseSensitivity;
    m_pitch -= dy * m_mouseSensitivity; // y축 반전 (화면 아래 = 양수 dy)

    // Pitch 클램핑 (짐벌락 방지)
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    // 카메라가 움직였으므로 누적 샘플 초기화
    m_frameCount = 0;
}

void Context::ProcessKeyboard(float deltaTime) {
    // 카메라 Right 벡터 = Front × Up의 정규화
    glm::vec3 right = glm::normalize(glm::cross(m_cameraFront, m_cameraUp));
    float speed = m_cameraSpeed * deltaTime;
    bool moved = false;

    if (GetAsyncKeyState('W') & 0x8000) { m_cameraPos += m_cameraFront * speed; moved = true; }
    if (GetAsyncKeyState('S') & 0x8000) { m_cameraPos -= m_cameraFront * speed; moved = true; }
    if (GetAsyncKeyState('A') & 0x8000) { m_cameraPos -= right          * speed; moved = true; }
    if (GetAsyncKeyState('D') & 0x8000) { m_cameraPos += right          * speed; moved = true; }
    if (GetAsyncKeyState('E') & 0x8000) { m_cameraPos += m_cameraUp     * speed; moved = true; } // 위
    if (GetAsyncKeyState('Q') & 0x8000) { m_cameraPos -= m_cameraUp     * speed; moved = true; } // 아래

    // 이동 시 누적 샘플 초기화 (노이즈 방지)
    if (moved) m_frameCount = 0;
}