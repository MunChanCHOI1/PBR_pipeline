#include "context.h"

// --- 생성 및 초기화 ---
ContextUPtr Context::Create(ID3D11Device* device, ID3D11DeviceContext* CTX) {
    auto context = ContextUPtr(new Context());
    if (!context->Init(device, CTX)) return nullptr;
    return std::move(context);
}

bool Context::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
    // 1. 셰이더 로드 (Shader::CreateFromFile 사용)
    // PBR용 BRDF 셰이더와 기본 텍스처/단색 셰이더를 모두 로드합니다.
    auto brdfVs = Shader::CreateFromFile("shader/brdf.hlsl", "VS", "vs_5_0");
    auto brdfPs = Shader::CreateFromFile("shader/brdf.hlsl", "PS", "ps_5_0");
    auto texVs = Shader::CreateFromFile("shader/texture.hlsl", "VS", "vs_5_0");
    auto texPs = Shader::CreateFromFile("shader/texture.hlsl", "PS", "ps_5_0");
    auto simVs = Shader::CreateFromFile("shader/simple.hlsl", "VS", "vs_5_0");
    auto simPs = Shader::CreateFromFile("shader/simple.hlsl", "PS", "ps_5_0");

    // 하나라도 로드 실패 시 즉시 종료 (nullptr 참조 방지)
    if (!brdfVs || !brdfPs || !texVs || !texPs || !simVs || !simPs) {
        return false;
    }

    // 2. Constant Buffers (UBO) 생성
    // sizeof(구조체)가 16의 배수(32, 64 등)인지 헤더에서 확인 완료했습니다.
    m_transformBuffer = Buffer::CreateWithData(device, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, nullptr, sizeof(TransformUBO), 1);
    m_sceneBuffer = Buffer::CreateWithData(device, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, nullptr, sizeof(SceneUBO), 1);
    m_materialBuffer = Buffer::CreateWithData(device, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, nullptr, sizeof(MaterialUBO), 1);

    if (!m_transformBuffer || !m_sceneBuffer || !m_materialBuffer) return false;

    // 3. 메시 생성 (TANGENT 레이아웃이 포함된 brdfVs의 Blob 사용)
    auto vsBlob = brdfVs->GetBuffer();
    m_sphere = Mesh::CreateSphere(device, vsBlob);
    m_plane = Mesh::CreatePlane(device, vsBlob);
    
    if (!m_sphere || !m_plane) return false;

    // 4. 프로그램 생성 (std::move로 소유권 이전)
    m_brdfProgram = Program::Create(device, std::move(brdfVs), std::move(brdfPs));
    m_textureProgram = Program::Create(device, std::move(texVs), std::move(texPs));
    m_simpleProgram = Program::Create(device, std::move(simVs), std::move(simPs));

    if (!m_brdfProgram || !m_textureProgram || !m_simpleProgram) return false;

    // 5. 초기 씬 조명 데이터 설정 (PBR 광원)
    m_sceneData.lights[0] = { glm::vec3(10.0f, 10.0f, 10.0f), 0.0f, glm::vec3(300.0f, 300.0f, 300.0f), 0.0f };
    m_sceneData.lights[1] = { glm::vec3(-10.0f, 10.0f, 10.0f), 0.0f, glm::vec3(150.0f, 150.0f, 150.0f), 0.0f };
    m_sceneData.lights[2] = { glm::vec3(10.0f, -10.0f, 10.0f), 0.0f, glm::vec3(100.0f, 100.0f, 200.0f), 0.0f };
    m_sceneData.lights[3] = { glm::vec3(0.0f, 0.0f, -10.0f), 0.0f, glm::vec3(50.0f, 50.0f, 50.0f), 0.0f };

    return true;
}

void Context::Render(ID3D11DeviceContext* context, uint32_t width, uint32_t height) {
    // 1. 카메라 방향 벡터 계산
    glm::vec3 front;
    front.x = cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));
    front.y = sin(glm::radians(m_pitch));
    front.z = cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
    m_cameraFront = glm::normalize(front);

    // 2. 공통 행렬(View, Projection) 준비
    auto view = glm::lookAtLH(m_cameraPos, m_cameraPos + m_cameraFront, m_cameraUp);
    auto projection = glm::perspectiveLH_ZO(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

    // 3. SceneUBO 업데이트 (b1 슬롯: 모든 물체 공통 조명/카메라 데이터)
    m_sceneData.viewPos = m_cameraPos;
    m_sceneBuffer->UpdateData(context, m_sceneData);
    auto sBuf = m_sceneBuffer->GetBuffer();
    context->PSSetConstantBuffers(1, 1, &sBuf); // register(b1)과 매칭

    // 4. 장면 그리기 함수 호출
    DrawScene(context, view, projection);
}

void Context::DrawScene(ID3D11DeviceContext* context, const glm::mat4& view, const glm::mat4& projection) {
    // PBR 프로그램 사용
    auto program = m_brdfProgram.get();
    if (!program) return;
    program->Use(context);
    
    const int count = 7;
    const float spacing = 1.3f;

    TransformUBO tUbo;
    tUbo.view = view;
    tUbo.projection = projection;

    MaterialUBO mUbo;
    mUbo.albedo = glm::vec3(1.0f, 0.05f, 0.05f); 
    mUbo.ao = 1.0f;
    mUbo.pad[0] = 0.0f; mUbo.pad[1] = 0.0f;

    for (int j = 0; j < count; j++) {
        // 1. Roughness (y축/세로): j=0(상단)일 때 0.05, j=6(하단)일 때 1.0
        mUbo.roughness = glm::clamp((float)j / (float)(count - 1), 0.05f, 1.0f);

        for (int i = 0; i < count; i++) {
            // 2. Metallic (x축/가로): i=0(왼쪽)일 때 1.0, i=6(오른쪽)일 때 0.0
            mUbo.metallic = 1.0f - ((float)i / (float)(count - 1));

            // 3. 좌표 계산 (중요!)
            // x: i=0일 때 왼쪽(-), i=6일 때 오른쪽(+)
            float x = ((float)i - (float)(count - 1) * 0.5f) * spacing;
            // y: j=0일 때 위(+), j=6일 때 아래(-) -> 그래서 -를 붙여야 합니다.
            float y = -((float)j - (float)(count - 1) * 0.5f) * spacing;

            // 4. Transform 업데이트 (b0)
            tUbo.model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
            m_transformBuffer->UpdateData(context, tUbo);
            auto tBuf = m_transformBuffer->GetBuffer();
            context->VSSetConstantBuffers(0, 1, &tBuf);

            // 5. Material 업데이트 (b2)
            m_materialBuffer->UpdateData(context, mUbo);
            auto mBuf = m_materialBuffer->GetBuffer();
            context->PSSetConstantBuffers(2, 1, &mBuf);

            m_sphere->Draw(context, program);
        }
    }
}

// ProcessMouseMenu와 ProcessKeyboard는 기존과 동일하여 생략하지 않고 포함합니다.
void Context::ProcessMouseMenu(float dx, float dy){
    float sensitivity = 0.1f;
    m_yaw += dx * sensitivity;
    m_pitch -= dy * sensitivity;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;
}

void Context::ProcessKeyboard(float deltaTime) {
    float speed = 5.0f * deltaTime;
    glm::vec3 f = m_cameraFront; 
    glm::vec3 r = glm::normalize(glm::cross(glm::vec3(0, 1, 0), f)); 
    glm::vec3 u = glm::normalize(glm::cross(f, r));

    if (GetAsyncKeyState('W') & 0x8000) m_cameraPos += f * speed;
    if (GetAsyncKeyState('S') & 0x8000) m_cameraPos -= f * speed;
    if (GetAsyncKeyState('D') & 0x8000) m_cameraPos += r * speed;
    if (GetAsyncKeyState('A') & 0x8000) m_cameraPos -= r * speed;
    if (GetAsyncKeyState('Q') & 0x8000) m_cameraPos -= u * speed;
    if (GetAsyncKeyState('E') & 0x8000) m_cameraPos += u * speed;
}