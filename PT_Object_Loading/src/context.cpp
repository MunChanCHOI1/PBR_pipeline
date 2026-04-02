#include "context.h"
#include <spdlog/spdlog.h>

// -------------------------------------------------------
// 절차적 씬 생성 헬퍼 (파일 내부 전용)
// -------------------------------------------------------
namespace {

void AppendQuad(
    const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3,
    const glm::vec3& normal,
    std::vector<Vertex>& verts, std::vector<uint32_t>& inds)
{
    uint32_t b = (uint32_t)verts.size();
    glm::vec3 tang = (p1 - p0 == glm::vec3(0.f))
        ? glm::vec3(1,0,0)
        : glm::normalize(p1 - p0);
    auto push = [&](const glm::vec3& pos) {
        Vertex v{};
        v.position = pos;
        v.normal   = normal;
        v.tangent  = tang;
        verts.push_back(v);
    };
    push(p0); push(p1); push(p2); push(p3);
    inds.push_back(b);   inds.push_back(b+1); inds.push_back(b+2);
    inds.push_back(b);   inds.push_back(b+2); inds.push_back(b+3);
}

void AppendBox(
    const glm::vec3& lo, const glm::vec3& hi,
    std::vector<Vertex>& v, std::vector<uint32_t>& i)
{
    // +Y
    AppendQuad({lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},{0,1,0},v,i);
    // -Y
    AppendQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},{0,-1,0},v,i);
    // +Z
    AppendQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},{0,0,1},v,i);
    // -Z
    AppendQuad({hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{0,0,-1},v,i);
    // +X
    AppendQuad({hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{1,0,0},v,i);
    // -X
    AppendQuad({lo.x,lo.y,lo.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{lo.x,hi.y,lo.z},{-1,0,0},v,i);
}

void BuildCityGeometry(
    std::vector<Vertex>&    allVerts,
    std::vector<uint32_t>&  allInds,
    std::vector<GpuMeshInfo>& meshInfos,
    std::vector<GpuMaterial>& materials)
{
    // 박스 메시 추가 헬퍼
    auto addBox = [&](const glm::vec3& lo, const glm::vec3& hi, const GpuMaterial& mat) {
        GpuMeshInfo info{};
        info.vertexOffset  = (uint32_t)allVerts.size();
        info.indexOffset   = (uint32_t)allInds.size();
        info.materialIndex = (uint32_t)materials.size();
        AppendBox(lo, hi, allVerts, allInds);
        info.indexCount    = (uint32_t)allInds.size() - info.indexOffset;
        meshInfos.push_back(info);
        materials.push_back(mat);
    };

    // 창문 쿼드 추가 헬퍼
    auto addWin = [&](
        const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3,
        const glm::vec3& n,  const GpuMaterial& mat)
    {
        GpuMeshInfo info{};
        info.vertexOffset  = (uint32_t)allVerts.size();
        info.indexOffset   = (uint32_t)allInds.size();
        info.materialIndex = (uint32_t)materials.size();
        AppendQuad(p0, p1, p2, p3, n, allVerts, allInds);
        info.indexCount    = (uint32_t)allInds.size() - info.indexOffset;
        meshInfos.push_back(info);
        materials.push_back(mat);
    };

    // ---- 재질 정의 ----
    GpuMaterial matBrick{};
    matBrick.albedo = glm::vec3(0.65f, 0.30f, 0.20f); matBrick.roughness = 0.85f;

    GpuMaterial matConcrete{};
    matConcrete.albedo = glm::vec3(0.55f, 0.55f, 0.55f); matConcrete.roughness = 0.90f;

    GpuMaterial matDarkBld{};
    matDarkBld.albedo = glm::vec3(0.10f, 0.10f, 0.12f); matDarkBld.roughness = 0.70f;

    GpuMaterial matBeige{};
    matBeige.albedo = glm::vec3(0.88f, 0.80f, 0.62f); matBeige.roughness = 0.88f;

    // 아스팔트: 더 어둡고 거친 표면, 약간의 갈색 기운
    GpuMaterial matRoad{};
    matRoad.albedo    = glm::vec3(0.12f, 0.11f, 0.10f);
    matRoad.roughness = 0.97f;

    GpuMaterial matSide{};
    matSide.albedo = glm::vec3(0.65f, 0.62f, 0.58f); matSide.roughness = 0.90f;

    // 창문 켜짐: warm Ke 2.0
    GpuMaterial matWinWarm{};
    matWinWarm.albedo   = glm::vec3(1.0f, 0.9f, 0.6f);
    matWinWarm.roughness = 1.0f;
    matWinWarm.emissive = glm::vec3(2.0f, 1.5f, 0.6f);

    // 창문 켜짐: cool Ke 1.2
    GpuMaterial matWinCool1{};
    matWinCool1.albedo   = glm::vec3(0.7f, 0.85f, 1.0f);
    matWinCool1.roughness = 1.0f;
    matWinCool1.emissive = glm::vec3(0.8f, 1.0f, 1.8f);

    // 창문 켜짐: cool Ke 2.0
    GpuMaterial matWinCool2{};
    matWinCool2.albedo   = glm::vec3(0.8f, 0.9f, 1.0f);
    matWinCool2.roughness = 1.0f;
    matWinCool2.emissive = glm::vec3(1.2f, 1.5f, 2.0f);

    // 창문 꺼짐: 어두운 유리
    GpuMaterial matWinDark{};
    matWinDark.albedo   = glm::vec3(0.04f, 0.04f, 0.05f);
    matWinDark.roughness = 0.05f;
    matWinDark.metallic  = 0.9f;

    // 가로등 기둥
    GpuMaterial matPole{};
    matPole.albedo   = glm::vec3(0.18f, 0.18f, 0.20f);
    matPole.roughness = 0.50f;
    matPole.metallic  = 0.80f;

    // 가로등 헤드 Ke 3.5
    GpuMaterial matHead{};
    matHead.albedo   = glm::vec3(1.0f, 0.95f, 0.70f);
    matHead.roughness = 1.0f;
    matHead.emissive = glm::vec3(3.5f, 2.7f, 1.3f);

    // 가로등 유리 Ke 2.5
    GpuMaterial matGlass{};
    matGlass.albedo   = glm::vec3(0.95f, 0.90f, 0.75f);
    matGlass.roughness = 0.20f;
    matGlass.emissive = glm::vec3(2.5f, 2.0f, 1.0f);

    // 물웅덩이: 매우 낮은 roughness로 거울반사에 가깝게, 약한 청색 틴트
    GpuMaterial matPuddle{};
    matPuddle.albedo    = glm::vec3(0.03f, 0.04f, 0.06f);
    matPuddle.roughness = 0.02f;
    matPuddle.metallic  = 0.0f;

    // ---- 도로 & 보도 ----
    addBox({-4.0f,-0.05f,-10.0f}, { 4.0f,  0.00f, 35.0f}, matRoad);
    addBox({-6.0f,-0.05f,-10.0f}, {-4.0f,  0.02f, 35.0f}, matSide);
    addBox({ 4.0f,-0.05f,-10.0f}, { 6.0f,  0.02f, 35.0f}, matSide);

    // ---- 물웅덩이 (도로 중앙, z=5~9, 도로면보다 0.5mm 위) ----
    addWin(
        {-1.5f, 0.0005f,  5.0f},
        { 1.5f, 0.0005f,  5.0f},
        { 1.5f, 0.0005f,  9.0f},
        {-1.5f, 0.0005f,  9.0f},
        {0.0f, 1.0f, 0.0f},
        matPuddle
    );

    // ---- 건물 4채 ----
    // A: 벽돌, 왼쪽, 7층 (21m)
    addBox({-13.0f, 0.0f,  0.0f}, {-6.0f, 21.0f, 12.0f}, matBrick);
    // B: 콘크리트, 왼쪽, 8층 (24m)
    addBox({-13.0f, 0.0f, 14.0f}, {-6.0f, 24.0f, 26.0f}, matConcrete);
    // C: 다크, 오른쪽, 5층 (15m)
    addBox({  6.0f, 0.0f,  0.0f}, {13.0f, 15.0f, 12.0f}, matDarkBld);
    // D: 베이지, 오른쪽, 9층 (27m)
    addBox({  6.0f, 0.0f, 14.0f}, {13.0f, 27.0f, 26.0f}, matBeige);

    // ---- 창문 ----
    struct BWin { float faceX, nX, zS, zE; int floors; };
    BWin bwins[] = {
        {-6.0f,  1.0f,  0.0f, 12.0f, 7},  // A
        {-6.0f,  1.0f, 14.0f, 26.0f, 8},  // B
        { 6.0f, -1.0f,  0.0f, 12.0f, 5},  // C
        { 6.0f, -1.0f, 14.0f, 26.0f, 9},  // D
    };
    // 패턴: 0=warm, 1=cool1, 2=cool2, 3=dark
    const int PAT[] = {0,0,3, 0,2,0, 3,0,1, 0,1,0, 3,0,0, 0,2,3, 1,0,0, 0,3,1, 0,0,2};
    int pi = 0;
    const int   WPF = 3;          // 층당 창문 수
    const float WH  = 1.4f;       // 창 높이
    const float WZ  = 1.5f;       // 창 폭(Z)
    const float FH  = 3.0f;       // 층 높이

    for (auto& bw : bwins) {
        float zStep = (bw.zE - bw.zS) / WPF;
        float fx = bw.faceX + bw.nX * 0.01f;  // 건물 표면에서 1cm 앞
        glm::vec3 n(bw.nX, 0.0f, 0.0f);

        for (int fl = 0; fl < bw.floors; fl++) {
            float yB = fl * FH + 0.8f, yT = yB + WH;
            for (int w = 0; w < WPF; w++) {
                float zC = bw.zS + (w + 0.5f) * zStep;
                float z0 = zC - WZ * 0.5f, z1 = zC + WZ * 0.5f;

                const GpuMaterial* mat;
                switch (PAT[pi++ % 9]) {
                    case 0:  mat = &matWinWarm;  break;
                    case 1:  mat = &matWinCool1; break;
                    case 2:  mat = &matWinCool2; break;
                    default: mat = &matWinDark;  break;
                }
                addWin({fx,yB,z0},{fx,yB,z1},{fx,yT,z1},{fx,yT,z0}, n, *mat);
            }
        }
    }

    // ---- 가로등 10개 (5쌍) ----
    const float SL_Z[] = {2.0f, 8.0f, 14.0f, 20.0f, 26.0f};
    const float sides[] = {-1.0f, 1.0f};
    for (float z : SL_Z) {
        for (float s : sides) {
            float px  = s * 5.0f;
            float arm = px - s * 1.8f;   // 팔 끝 (도로 방향)
            float ax0 = glm::min(px, arm), ax1 = glm::max(px, arm);

            // 기둥
            addBox({px-0.10f, 0.0f, z-0.10f}, {px+0.10f, 3.5f, z+0.10f}, matPole);
            // 수평 팔
            addBox({ax0, 3.40f, z-0.05f}, {ax1, 3.55f, z+0.05f}, matPole);
            // 헤드 (Ke 3.5)
            addBox({arm-0.30f, 3.30f, z-0.18f}, {arm+0.30f, 3.50f, z+0.18f}, matHead);
            // 유리 (Ke 2.5)
            addBox({arm-0.25f, 3.10f, z-0.15f}, {arm+0.25f, 3.30f, z+0.15f}, matGlass);
        }
    }
}

} // namespace

ContextUPtr Context::Create(ID3D11Device *device, ID3D11DeviceContext *ctx) {
  auto context = ContextUPtr(new Context());
  if (!context->Init(device, ctx))
    return nullptr;
  return std::move(context);
}

bool Context::Init(ID3D11Device *device, ID3D11DeviceContext *context) {
  // 1. 패스 트레이서 컴퓨트 셰이더 로드
  auto cs =
      Shader::CreateFromFile("shader/PathTracer.hlsl", "CSMain", "cs_5_0");
  if (!cs)
    return false;
  m_pathTracerProgram = ComputeProgram::Create(device, std::move(cs));
  if (!m_pathTracerProgram)
    return false;

  // 2. 글로벌 상수 버퍼
  m_globalBuffer = Buffer::CreateWithData(device, D3D11_BIND_CONSTANT_BUFFER,
                                          D3D11_USAGE_DYNAMIC, nullptr,
                                          sizeof(GlobalUniforms), 1);
  if (!m_globalBuffer)
    return false;

  // 3. 절차적 도시 씬 사용 (OBJ는 Z-up 좌표계로 렌더러와 불일치)
  m_model = nullptr;

  // 4. 씬 버퍼 빌드 (모든 메시를 하나의 큰 버퍼로 합치기)
  if (!BuildSceneBuffers(device))
    return false;

  // 5. 출력 텍스처 생성
  OnResize(device, WINDOW_WIDTH, WINDOW_HEIGHT);
  return true;
}

bool Context::BuildSceneBuffers(ID3D11Device *device) {
  std::vector<Vertex> allVertices;
  std::vector<uint32_t> allIndices;
  std::vector<GpuMeshInfo> meshInfos;
  std::vector<GpuMaterial> materials;

  // 모델이 있으면 메시 데이터 수집
  if (m_model) {
    // 총 크기를 미리 계산하여 재할당 방지
    size_t totalVerts = 0, totalInds = 0;
    for (const auto& m : m_model->GetMeshes()) {
      totalVerts += m->GetVertexCount();
      totalInds  += m->GetIndexCount();
    }
    allVertices.reserve(totalVerts);
    allIndices.reserve(totalInds);
    meshInfos.reserve(m_model->GetMeshes().size());
    materials.reserve(m_model->GetMeshes().size());

    for (const auto &mesh : m_model->GetMeshes()) {
      // 이 메시의 오프셋 기록
      GpuMeshInfo info;
      info.vertexOffset  = (uint32_t)allVertices.size();
      info.indexOffset   = (uint32_t)allIndices.size();
      info.indexCount    = mesh->GetIndexCount();
      info.materialIndex = (uint32_t)materials.size();
      meshInfos.push_back(info);

      // 재질 추가
      const auto &mat = mesh->GetMaterial();
      GpuMaterial gpuMat;
      gpuMat.albedo    = mat.albedo;
      gpuMat.roughness = mat.roughness;
      gpuMat.emissive  = mat.emissive;
      gpuMat.metallic  = mat.metallic;
      materials.push_back(gpuMat);

      // 버텍스/인덱스 수집 (CPU 사본에서 읽기)
      uint32_t vOffset = info.vertexOffset;
      const auto& verts = mesh->GetVertices();
      allVertices.insert(allVertices.end(), verts.begin(), verts.end());

      for (uint32_t idx : mesh->GetIndices())
        allIndices.push_back(vOffset + idx); // 전역 인덱스로 보정
    }
  }

  // 모델이 없으면 절차적 도시 씬 생성
  if (meshInfos.empty()) {
    SPDLOG_INFO("No model found. Building procedural city scene.");
    BuildCityGeometry(allVertices, allIndices, meshInfos, materials);
  }

  m_meshCount = (uint32_t)meshInfos.size();

  // GPU 버퍼 생성
  m_vertexBuffer = Buffer::CreateWithData(
      device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
      allVertices.data(), sizeof(Vertex), (uint32_t)allVertices.size(),
      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
  if (!m_vertexBuffer)
    return false;
  m_vertexSRV = m_vertexBuffer->CreateSRV(device);

  m_indexBuffer = Buffer::CreateWithData(
      device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
      allIndices.data(), sizeof(uint32_t), (uint32_t)allIndices.size(),
      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
  if (!m_indexBuffer)
    return false;
  m_indexSRV = m_indexBuffer->CreateSRV(device);

  m_meshInfoBuffer = Buffer::CreateWithData(
      device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, meshInfos.data(),
      sizeof(GpuMeshInfo), (uint32_t)meshInfos.size(),
      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
  if (!m_meshInfoBuffer)
    return false;
  m_meshInfoSRV = m_meshInfoBuffer->CreateSRV(device);

  m_materialBuffer = Buffer::CreateWithData(
      device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, materials.data(),
      sizeof(GpuMaterial), (uint32_t)materials.size(),
      D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
  if (!m_materialBuffer)
    return false;
  m_materialSRV = m_materialBuffer->CreateSRV(device);

  SPDLOG_INFO("Scene built: {} meshes, {} vertices, {} indices", m_meshCount,
              allVertices.size(), allIndices.size());
  return true;
}

void Context::OnResize(ID3D11Device *device, uint32_t width, uint32_t height) {
  m_outputUAV.Reset();
  m_outputSRV.Reset();
  m_outputTexture.Reset();
  m_accumUAV.Reset();
  m_accumSRV.Reset();
  m_accumTexture.Reset();

  // LDR 출력 텍스처 (u1)
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width            = width;
  desc.Height           = height;
  desc.MipLevels        = 1;
  desc.ArraySize        = 1;
  desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage            = D3D11_USAGE_DEFAULT;
  desc.BindFlags        = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

  HRESULT hr = device->CreateTexture2D(&desc, nullptr, m_outputTexture.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create output texture. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  hr = device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, m_outputUAV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create output UAV. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  hr = device->CreateShaderResourceView(m_outputTexture.Get(), nullptr, m_outputSRV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create output SRV. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  // HDR 누적 버퍼 (u0)
  desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

  hr = device->CreateTexture2D(&desc, nullptr, m_accumTexture.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create accum texture. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  hr = device->CreateUnorderedAccessView(m_accumTexture.Get(), nullptr, m_accumUAV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create accum UAV. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  hr = device->CreateShaderResourceView(m_accumTexture.Get(), nullptr, m_accumSRV.ReleaseAndGetAddressOf());
  if (FAILED(hr)) { SPDLOG_ERROR("Failed to create accum SRV. HRESULT: 0x{:08x}", (uint32_t)hr); return; }

  m_frameCount = 0;
}

void Context::Render(ID3D11DeviceContext *context, uint32_t width,
                     uint32_t height) {
  // 카메라 벡터 계산
  glm::vec3 front;
  front.x = cos(glm::radians(m_pitch)) * cos(glm::radians(m_yaw));
  front.y = sin(glm::radians(m_pitch));
  front.z = cos(glm::radians(m_pitch)) * sin(glm::radians(m_yaw));
  m_cameraFront = glm::normalize(front);
  glm::vec3 right = glm::normalize(glm::cross(m_cameraFront, m_cameraUp));

  // 상수 버퍼 업데이트
  GlobalUniforms globalData;
  globalData.cameraPos = m_cameraPos;
  globalData.fov = glm::radians(45.0f);
  globalData.cameraFront = m_cameraFront;
  globalData.aspectRatio = (float)width / (float)height;
  globalData.cameraUp = m_cameraUp;
  globalData.frameCount = (float)m_frameCount++;
  globalData.cameraRight = right;
  globalData._pad = 0.0f;
  m_globalBuffer->UpdateData(context, globalData);

  // b0: 상수 버퍼
  auto gBuf = m_globalBuffer->GetBuffer();
  context->CSSetConstantBuffers(0, 1, &gBuf);

  // t0~t3: 씬 데이터 SRV
  ID3D11ShaderResourceView *srvs[4] = {
      m_vertexSRV.Get(),   // t0: 버텍스
      m_indexSRV.Get(),    // t1: 인덱스
      m_meshInfoSRV.Get(), // t2: 메시 정보
      m_materialSRV.Get(), // t3: 재질
  };
  context->CSSetShaderResources(0, 4, srvs);

  // u0: HDR 누적, u1: LDR 출력
  ID3D11UnorderedAccessView *uavs[2] = {
      m_accumUAV.Get(),
      m_outputUAV.Get(),
  };
  context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);

  // Dispatch
  uint32_t gx = (width + 7) / 8;
  uint32_t gy = (height + 7) / 8;
  m_pathTracerProgram->Dispatch(context, gx, gy, 1);

  // 리소스 해제
  ID3D11UnorderedAccessView *nullUAVs[2] = {nullptr, nullptr};
  context->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
  ID3D11ShaderResourceView *nullSRVs[4] = {nullptr, nullptr, nullptr, nullptr};
  context->CSSetShaderResources(0, 4, nullSRVs);
}

void Context::Present(ID3D11DeviceContext *context,
                      ID3D11RenderTargetView *rtv) {
  ID3D11Resource *backBufferRes = nullptr;
  rtv->GetResource(&backBufferRes);
  if (backBufferRes) {
    context->CopyResource(backBufferRes, m_outputTexture.Get());
    backBufferRes->Release();
  }
}

void Context::ProcessMouseMenu(float dx, float dy) {
  m_yaw += dx * m_mouseSensitivity;
  m_pitch -= dy * m_mouseSensitivity;
  m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
  m_frameCount = 0;
}

void Context::ProcessKeyboard(float deltaTime) {
  glm::vec3 right = glm::normalize(glm::cross(m_cameraFront, m_cameraUp));
  
  float currentSpeed = m_cameraSpeed;
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
      currentSpeed *= 5.0f; // Shift 누를 시 5배 가속
  }
  float speed = currentSpeed * deltaTime;
  
  bool moved = false;

  if (GetAsyncKeyState('W') & 0x8000) {
    m_cameraPos += m_cameraFront * speed;
    moved = true;
  }
  if (GetAsyncKeyState('S') & 0x8000) {
    m_cameraPos -= m_cameraFront * speed;
    moved = true;
  }
  if (GetAsyncKeyState('A') & 0x8000) {
    m_cameraPos -= right * speed;
    moved = true;
  }
  if (GetAsyncKeyState('D') & 0x8000) {
    m_cameraPos += right * speed;
    moved = true;
  }
  if (GetAsyncKeyState('E') & 0x8000) {
    m_cameraPos += m_cameraUp * speed;
    moved = true;
  }
  if (GetAsyncKeyState('Q') & 0x8000) {
    m_cameraPos -= m_cameraUp * speed;
    moved = true;
  }

  if (moved)
    m_frameCount = 0;
}