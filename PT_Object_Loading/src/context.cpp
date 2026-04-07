#include "context.h"
#include <spdlog/spdlog.h>

// -------------------------------------------------------
// 지오메트리 생성 헬퍼 (파일 내부 전용)
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
    AppendQuad({lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},{0,1,0},v,i);
    AppendQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},{0,-1,0},v,i);
    AppendQuad({lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z},{0,0,1},v,i);
    AppendQuad({hi.x,lo.y,lo.z},{lo.x,lo.y,lo.z},{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{0,0,-1},v,i);
    AppendQuad({hi.x,lo.y,hi.z},{hi.x,lo.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{1,0,0},v,i);
    AppendQuad({lo.x,lo.y,lo.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,hi.z},{lo.x,hi.y,lo.z},{-1,0,0},v,i);
}

// SceneDesc → GPU 버퍼용 배열로 변환
void FlattenScene(
    const SceneDesc&          desc,
    std::vector<Vertex>&      verts,
    std::vector<uint32_t>&    inds,
    std::vector<GpuMeshInfo>& meshInfos,
    std::vector<GpuMaterial>& materials)
{
    for (const auto& b : desc.boxes) {
        GpuMeshInfo info{};
        info.vertexOffset  = (uint32_t)verts.size();
        info.indexOffset   = (uint32_t)inds.size();
        info.materialIndex = (uint32_t)materials.size();
        AppendBox(b.lo, b.hi, verts, inds);
        info.indexCount = (uint32_t)inds.size() - info.indexOffset;
        meshInfos.push_back(info);
        materials.push_back(b.mat);
    }

    for (const auto& q : desc.quads) {
        GpuMeshInfo info{};
        info.vertexOffset  = (uint32_t)verts.size();
        info.indexOffset   = (uint32_t)inds.size();
        info.materialIndex = (uint32_t)materials.size();
        AppendQuad(q.p0, q.p1, q.p2, q.p3, q.n, verts, inds);
        info.indexCount = (uint32_t)inds.size() - info.indexOffset;
        meshInfos.push_back(info);
        materials.push_back(q.mat);
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
  SceneDesc desc;
  if (meshInfos.empty()) {
    SPDLOG_INFO("No model found. Building procedural city scene.");
    desc = MakeCityScene();
    FlattenScene(desc, allVertices, allIndices, meshInfos, materials);
  }

  m_meshCount = (uint32_t)meshInfos.size();

  // BVH 빌드
  {
    uint32_t totalTris = (uint32_t)(allIndices.size() / 3);
    std::vector<uint32_t> triMesh(totalTris);
    uint32_t globalTri = 0;
    for (uint32_t mi = 0; mi < (uint32_t)meshInfos.size(); ++mi) {
      uint32_t triCount = meshInfos[mi].indexCount / 3;
      for (uint32_t t = 0; t < triCount; ++t)
        triMesh[globalTri++] = mi;
    }

    Bvh bvh;
    bvh.Build(allVertices, allIndices, triMesh);

    m_bvhNodeBuffer = Buffer::CreateWithData(
        device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
        bvh.Nodes().data(), sizeof(BvhNode), (uint32_t)bvh.Nodes().size(),
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
    if (!m_bvhNodeBuffer) return false;
    m_bvhNodeSRV = m_bvhNodeBuffer->CreateSRV(device);

    m_bvhPrimBuffer = Buffer::CreateWithData(
        device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
        bvh.Prims().data(), sizeof(BvhPrim), (uint32_t)bvh.Prims().size(),
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
    if (!m_bvhPrimBuffer) return false;
    m_bvhPrimSRV = m_bvhPrimBuffer->CreateSRV(device);

    SPDLOG_INFO("BVH built: {} nodes, {} prims", bvh.Nodes().size(), bvh.Prims().size());
  }

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

  // 광원 버퍼 (t6)
  m_lightCount = (uint32_t)desc.lights.size();
  if (m_lightCount > 0) {
    m_lightBuffer = Buffer::CreateWithData(
        device, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT,
        desc.lights.data(), sizeof(LightDesc), m_lightCount,
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED);
    if (!m_lightBuffer) return false;
    m_lightSRV = m_lightBuffer->CreateSRV(device);
  }

  SPDLOG_INFO("Scene built: {} meshes, {} vertices, {} indices, {} lights",
              m_meshCount, allVertices.size(), allIndices.size(), m_lightCount);
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
  globalData.lightCount  = m_lightCount;
  m_globalBuffer->UpdateData(context, globalData);

  // b0: 상수 버퍼
  auto gBuf = m_globalBuffer->GetBuffer();
  context->CSSetConstantBuffers(0, 1, &gBuf);

  // t0~t6: 씬 데이터 SRV
  ID3D11ShaderResourceView *srvs[7] = {
      m_vertexSRV.Get(),   // t0: 버텍스
      m_indexSRV.Get(),    // t1: 인덱스
      m_meshInfoSRV.Get(), // t2: 메시 정보
      m_materialSRV.Get(), // t3: 재질
      m_bvhNodeSRV.Get(),  // t4: BVH 노드
      m_bvhPrimSRV.Get(),  // t5: BVH 프리미티브
      m_lightSRV.Get(),    // t6: NEE 광원
  };
  context->CSSetShaderResources(0, 7, srvs);

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
  ID3D11ShaderResourceView *nullSRVs[7] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  context->CSSetShaderResources(0, 7, nullSRVs);
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