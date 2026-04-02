# CLAUDE.md — PT_Object_Loading 리팩토링 & glTF 도입 가이드

## 📁 현재 레포 구조

```
PT_Object_Loading/
├── src/
│   ├── common.h           # CLASS_PTR 매크로, ComPtr alias
│   ├── buffer.h/cpp       # DX11 Buffer 래퍼, CreateWithData / CreateSRV
│   ├── mesh.h/cpp         # Vertex, MaterialData, CPU 사본(m_cpuVertices/Indices)
│   ├── model.h/cpp        # Assimp OBJ 로더 → Mesh 배열
│   ├── context.h/cpp      # Init / BuildSceneBuffers / Render / 카메라
│   ├── compute_program.h/cpp
│   ├── shader.h/cpp
│   ├── texture.h/cpp
│   └── image.h/cpp
├── shader/
│   ├── PathTracer.hlsl    # CS 진입점, TracePath (NEE + GGX 간접광)
│   ├── Scene.hlsli        # SceneIntersect — 하드코딩 폴백 + 메시 순회
│   ├── BRDF.hlsli         # GGX NDF/G/F, ImportanceSampleGGX, ComputePDF
│   ├── Intersection.hlsli # IntersectTriangle / IntersectSphere
│   ├── Utility.hlsli      # GetRandomSamples, GetSkyColor
│   └── Common.hlsli       # Ray 구조체
├── model/                 # OBJ/MTL 에셋 (빌드 시 build/model/ 로 자동 복사)
├── Dependency.cmake       # ExternalProject: spdlog, stb, glm, assimp
└── CMakeLists.txt
```

## 🔑 핵심 데이터 흐름 (현재)

```
OBJ 파일
  → Assimp (model.cpp)
  → Mesh[] (CPU 버텍스/인덱스 사본 보유)
  → BuildSceneBuffers (context.cpp)
    → 통합 StructuredBuffer (t0:vertex / t1:index / t2:meshInfo / t3:material)
  → CS Dispatch → PathTracer.hlsl → Scene.hlsli SceneIntersect
```

**폴백 동작**: `Model::Load` 실패 시 `meshInfos`가 비어 더미 1개 삽입 → `Scene.hlsli`의 하드코딩 씬(금속구 + 체커 바닥) 렌더링.

---

## 🎯 목표

1. **glTF 도입**: Assimp OBJ → tinygltf 기반 `.gltf`/`.glb` 로더로 교체
2. **리팩토링**: 현재 코드의 중복/구조 문제 해소
3. **렌더링 개선**: 텍스처 지원, BVH 가속 구조 도입 준비

---

## 🧠 기본 원칙

- **안정성**: 기존 패스 트레이싱 렌더링 결과 보존. 소규모 점진적 수정.
- **명확성**: 과도한 추상화 금지. 현재 파일 구조 유지 우선.
- **엄격성**: 기능 변경/추측 코드 금지. 컨텍스트 부족 시 반드시 선 질문.

---

## 🧩 C++ / DX11 코딩 스타일

- SRP 준수, **함수 최대 50줄**.
- Magic number 상수화 (예: `MAX_BOUNCES = 3`, `WINDOW_WIDTH = 960`).
- 스마트 포인터: `std::unique_ptr` (CLASS_PTR 매크로), `ComPtr` 필수.
- HRESULT 검사: `FAILED(hr)` 후 `SPDLOG_ERROR` + `return false` 패턴 유지.
- CPU 데이터 사본(`m_cpuVertices`, `m_cpuIndices`) 패턴은 현재 `BuildSceneBuffers`와 결합되어 있으므로 glTF 전환 전까지 유지.

---

## ⚡ 현재 코드의 알려진 문제점 (작업 전 확인)

| 위치 | 문제 | 우선순위 |
|---|---|---|
| `Scene.hlsli` | `Path_tracing` 버전과 `PT_Object_Loading` 버전이 별개로 존재 (메시 버퍼 바인딩 구조 다름) | 높음 |
| `Scene.hlsli` | `SceneIntersect`에서 삼각형 전체 순회 O(n) — BVH 없음 | 중간 |
| `model.cpp` | Assimp Shininess→Roughness 변환이 근사치 (`1 - shininess/1000`) | 낮음 |
| `BuildSceneBuffers` | 메시마다 `push_back` 반복 → `reserve` 없음 | 낮음 |
| `Mesh` | CPU 사본(`m_cpuVertices`)이 GPU 업로드 후에도 메모리 상주 | 낮음 |
| `context.cpp` | 하드코딩된 모델 경로 `"model/backpack.obj"` | 즉시 수정 |

---

## 🔄 glTF 도입 계획

### Phase 1 — tinygltf 의존성 추가 (`Dependency.cmake`)
```cmake
ExternalProject_Add(
    dep_tinygltf
    GIT_REPOSITORY "https://github.com/syoyo/tinygltf.git"
    GIT_TAG        "v2.8.21"
    GIT_SHALLOW    1
    UPDATE_DISCONNECTED 1
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    TEST_COMMAND      ""
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E copy
        ${PROJECT_BINARY_DIR}/dep_tinygltf-prefix/src/dep_tinygltf/tiny_gltf.h
        ${DEP_INSTALL_DIR}/include/tiny_gltf.h
)
```
`CMakeLists.txt`에 `src/gltf_loader.h/cpp` 추가.

### Phase 2 — `GltfLoader` 클래스 (model.h/cpp 대체)

**목표 인터페이스** (기존 `Model`과 호환 유지):
```cpp
// src/gltf_loader.h
CLASS_PTR(GltfLoader)
class GltfLoader {
public:
    // 기존 Model::Load와 동일한 시그니처
    static GltfLoaderUPtr Load(ID3D11Device* device, const std::string& filepath);
    const std::vector<MeshUPtr>& GetMeshes() const { return m_meshes; }
private:
    bool LoadGltf(ID3D11Device* device, const std::string& filepath);
    MeshUPtr ProcessPrimitive(ID3D11Device* device,
                               const tinygltf::Model& model,
                               const tinygltf::Primitive& prim);
    MaterialData ExtractMaterial(const tinygltf::Model& model, int matIndex);
    std::vector<MeshUPtr> m_meshes;
};
```

**tinygltf 파싱 규칙**:
- `accessor` → `bufferView` → `buffer` 직접 포인터 접근으로 데이터 복사 최소화.
- `POSITION`, `NORMAL`, `TEXCOORD_0`, `TANGENT` 어트리뷰트 순서대로 `Vertex` 구조체로 합성.
- 인덱스 타입 처리: `TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT` / `_UNSIGNED_INT` 모두 `uint32_t`로 변환.
- `baseColorFactor` → `albedo`, `metallicFactor` → `metallic`, `roughnessFactor` → `roughness`, `emissiveFactor` → `emissive` 직접 매핑.

### Phase 3 — `context.cpp` 교체
```cpp
// 기존
m_model = Model::Load(device, "model/night_street.obj");

// 변경
m_model = GltfLoader::Load(device, "model/scene.glb");
```
`BuildSceneBuffers`는 `GetMeshes()` 인터페이스가 동일하므로 **수정 없음**.

---

## 🧱 셰이더 구조 가이드

현재 셰이더 분리 구조는 유지. 수정 시 파일 역할 준수:

| 파일 | 역할 | 수정 시 주의 |
|---|---|---|
| `PathTracer.hlsl` | CS 진입점, 카메라 레이, TracePath 루프 | `MAX_BOUNCES` 상수만 조정 |
| `Scene.hlsli` | `SceneIntersect` — 교차 판정 | t0~t3 레지스터 바인딩 순서 고정 |
| `BRDF.hlsli` | GGX PBR 함수 | `PI` define 중복 주의 |
| `Intersection.hlsli` | 삼각형/구 교차 수식 | 순수 수학, 부수효과 없음 |
| `Utility.hlsli` | RNG, 하늘 색상 | `GetRandomSamples` 시드 변경 금지 |

**GPU 버퍼 레지스터 (변경 금지)**:
```hlsl
t0: StructuredBuffer<ShaderVertex>    g_vertices
t1: StructuredBuffer<uint>            g_indices
t2: StructuredBuffer<ShaderMeshInfo>  g_meshInfos
t3: StructuredBuffer<ShaderMaterial>  g_materials
u0: RWTexture2D<float4>               g_accum   (HDR 누적)
u1: RWTexture2D<unorm float4>         g_output  (LDR 출력)
b0: cbuffer GlobalUB                  (카메라 파라미터)
```

---

## 📝 AI 작업 방식

- 응답 순서: **1. 문제점 → 2. 개선 방향 → 3. 코드 → 4. 요약**
- 불필요한 설명 생략, 핵심과 코드 위주.
- 파일 하나 수정 시 해당 파일 전체를 제시 (부분 diff 금지 — 컨텍스트 유실 방지).
- 셰이더(`*.hlsl`, `*.hlsli`) 수정 시 반드시 레지스터 바인딩 테이블 재확인.
- `BuildSceneBuffers` 수정 시 CPU 사본(`GetVertices()`/`GetIndices()`) 의존성 확인.
