# Render Phase 1 Baseline Prune Plan

| 구분 | 내용 |
|---|---|
| 최초 작성자 | OpenAI Codex |
| 최초 작성일 | 2026-05-13 |
| 최근 수정자 | OpenAI Codex |
| 최근 수정일 | 2026-05-13 |
| 버전 | 1.0 |

## 1. 개요

이 문서는 [render_renderer_simplification_phases.md](./render_renderer_simplification_phases.md)의 `Phase 1. Baseline Prune`를 실제 작업 가능한 수준으로 풀어쓴 상세 계획서다.

`Phase 1`의 목적은 이후의 renderer simplification을 시작하기 전에 유지 가치가 낮은 shading permutation과 과제성 기능을 먼저 정리해, 이후 phase들이 다뤄야 할 경우의 수를 줄이는 것이다.

이번 phase의 중심 작업은 아래 두 가지다.

- `Gouraud` shader / view mode / permutation 제거
- 향후 유지하지 않을 저우선순위 shading 경로 정리 기준 확정

본 phase는 아직 `Deferred / Forward` 토글을 제거하지 않는다. 그 작업은 `Phase 2` 이후의 범위다. 다만 `Phase 1`은 그 이후 작업을 쉽게 만들기 위한 준비 phase다.

## 2. 목표

### 2.1 1차 목표

- `EViewMode::Lit_Gouraud` 제거
- `Gouraud` 전용 shader entry 제거
- `Gouraud` 기반 pass desc / warm-up / UI 표시 제거
- `Gouraud`를 특별 취급하는 렌더 코드 제거

### 2.2 2차 목표

- `Lambert`, `BlinnPhong`, `Unlit`, `WorldNormal`, `SceneDepth`, `Wireframe` 중심으로 view mode 체계를 재정렬
- 이후 phase에서 다뤄야 하는 shading permutation 수를 줄임

### 2.3 비목표

- `Deferred / Forward` path 구조 변경
- deferred pass 삭제
- forward light culling 이식
- material system 재설계

## 3. 왜 먼저 해야 하는가

`Gouraud`는 현재 코드에서 생각보다 넓게 퍼져 있다.

- view mode enum에 존재한다.
- 에디터 UI에서 직접 선택 가능하다.
- deferred / forward pass desc 생성 분기에 모두 들어간다.
- deferred opaque / deferred lighting / deferred decal / forward opaque shader permutation에 모두 들어간다.
  - surface packing / decode 경로를 forward surface contract 기준으로 단순화했다.
- warm-up 목록과 overlay stat 표시에도 남아 있다.

특히 forward 경로에서 Gouraud는 VS 단계에서 조명을 계산한다. 이는 이후 `Phase 5`의 forward light culling 이식과 구조적으로 잘 맞지 않는다. 따라서 `Gouraud` 제거는 단순한 기능 축소가 아니라 후속 phase 리스크를 줄이는 선행 정리다.

## 4. 현재 영향 범위

### 4.1 코드 영향 범주

| 범주 | 설명 |
|---|---|
| View mode enum | `Lit_Gouraud` 제거와 enum 재정렬 |
| Editor UI | 툴바 / 스켈레탈 뷰어의 radio button 제거 |
| View mode registry | shading model 매핑, pass desc 생성, warm-up 대상 정리 |
| Shader permutation | deferred / forward / decal / uber lit의 Gouraud 분기 제거 |
| Surface contract | Gouraud용 `Surface1` / packed output 경로 검토 |
| Runtime comments / stats | Gouraud 기준으로 남은 설명과 통계 라벨 정리 |
| Docs | pipeline/execution/reference 문서에서 `Lit_Gouraud` 제거 |

### 4.2 주요 참조 위치

- view mode enum
  - [ViewTypes.h](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Context/Scene/ViewTypes.h:13)
- shading model enum / helper
  - [ShadingModel.h](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Context/ViewMode/ShadingModel.h:11)
- editor UI
  - [EditorToolbarPanel.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Editor/UI/EditorToolbarPanel.cpp:1022)
  - [EditorSkeletalMeshViewerPanel.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Editor/UI/EditorSkeletalMeshViewerPanel.cpp:166)
- warm-up / editor boot path
  - [EditorEngine.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Editor/EditorEngine.cpp:55)
- view mode registry
  - [ViewModePassRegistry.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp:138)
  - [ViewModePassRegistry.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp:185)
  - [ViewModePassRegistry.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp:230)
  - [ViewModePassRegistry.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp:272)
  - [ViewModePassRegistry.cpp](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp:379)
- forward shader
  - [OpaquePass.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Opaque/OpaquePass.hlsl:82)
  - [OpaquePass.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Opaque/OpaquePass.hlsl:154)
- deferred shader
  - [DeferredOpaquePass.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Deferred/DeferredOpaquePass.hlsl:63)
  - [DeferredOpaquePass.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Deferred/DeferredOpaquePass.hlsl:74)
  - [DeferredLightingPS.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Deferred/DeferredLightingPS.hlsl:78)
  - [DeferredDecalPS.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Passes/Scene/Deferred/DeferredDecalPS.hlsl:95)
- lighting helpers
  - [DirectLighting.hlsli](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Render/Scene/Lighting/DirectLighting.hlsli:278)
  - [BRDF.hlsli](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Render/Scene/Lighting/BRDF.hlsli:26)
- surface packing / types
  - `FSurfaceData` / surface helper inline refactor
  - [SurfacePacking.hlsli](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Surface/SurfacePacking.hlsli:8)
  - [VertexLayouts.hlsl](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Shaders/Resources/VertexLayouts.hlsl:147)

## 5. 작업 묶음

`Phase 1`은 아래 다섯 개 작업 묶음으로 나눈다.

### Workstream A. View Mode and Shading Model Prune

#### 목적

엔진이 `Gouraud`를 정식 view mode / shading model로 인식하지 않게 만든다.

#### 작업 항목

- `EViewMode::Lit_Gouraud` 제거
- `EShadingModel::Gouraud` 제거
- `GetShadingModelFromViewMode` 정리
- lit 여부 판정 helper 정리
- shading model string helper 정리
- overlay stat / 디버그 텍스트에서 Gouraud 항목 제거

#### 예상 영향

- enum 값 변경에 따른 직렬화 / ini / ui 인덱스 변화 가능
- 이전 설정 파일이 `Lit_Gouraud` 값을 갖고 있을 경우 fallback 필요

#### 산출물

- Gouraud 없는 view mode / shading model 체계
- 레거시 설정값 fallback 정책

### Workstream B. Editor and UX Cleanup

#### 목적

에디터에서 사용자가 `Gouraud`를 선택하거나 전제할 수 없게 만든다.

#### 작업 항목

- viewport toolbar radio button 제거
- skeletal mesh viewer panel radio button 제거
- warm-up 목록에서 Gouraud 제거
- 필요 시 기존 Gouraud 선택 상태를 `Lit_Phong` 또는 `Lit_Lambert`로 자동 치환

#### 예상 영향

- 에디터 재시작 시 예전 설정을 읽는 경우 표시 mode 치환 필요

#### 산출물

- UI 상에서 Gouraud가 보이지 않는 상태
- 레거시 설정을 읽어도 안전하게 다른 lit mode로 치환되는 상태

### Workstream C. View Mode Pass Registry Cleanup

#### 목적

pass desc 생성과 warm-up에서 Gouraud permutation을 제거한다.

#### 작업 항목

- deferred opaque pass desc의 Gouraud 분기 제거
- forward opaque pass desc의 Gouraud 분기 제거
- deferred decal pass desc의 Gouraud 분기 제거
- deferred lighting pass desc의 Gouraud define 제거
- `InitializeViewModePassConfig`의 Gouraud path 제거
- pass registry / warm-up 대상 재검토

#### 예상 영향

- shader variant cache key 수 감소
- `Lit_Gouraud`를 전제로 하던 코드가 있으면 build break 가능

#### 산출물

- registry 기준으로 Gouraud permutation이 생성되지 않는 상태

### Workstream D. Shader and Surface Contract Cleanup

#### 목적

셰이더 레벨에서 Gouraud entry와 packed surface 규약을 제거한다.

#### 작업 항목

- `OpaquePass.hlsl`의 `PS_Forward_Gouraud` 제거
- `DeferredOpaquePass.hlsl`의 `PS_Opaque_Gouraud` 제거
- `DeferredLightingPS.hlsl`의 Gouraud branch 제거
- `DeferredDecalPS.hlsl`의 `PS_Decal_Gouraud` 제거
- `UberLit.hlsl`의 Gouraud branch 제거
- `DirectLighting.hlsli` / `BRDF.hlsli`의 Gouraud helper 제거
- `SurfacePacking.hlsli`의 forward-only helper만 유지
- `FSurfaceData`를 삭제하고 필요한 surface 값을 지역 변수로 직접 계산
- `VertexLayouts.hlsl`의 Gouraud target/comment 정리

#### 유의사항

- `Surface.Gouraud` 필드를 즉시 제거할지, 후속 phase까지 남길지 결정이 필요하다.
- immediate cleanup 관점에서는 제거가 맞지만, 너무 많은 shader 파일을 동시에 건드리면 리스크가 커질 수 있다.

#### 권장 방침

- `Phase 1`에서는 사용 경로를 먼저 제거한다.
- `Surface.Gouraud` 같은 잔여 필드는 실제 미사용이 확인되면 같은 phase 안에서 정리하되, 빌드 리스크가 크면 `Phase 1.5` 수준의 follow-up cleanup으로 분리 가능하다.

#### 산출물

- Gouraud shader entry와 define이 제거된 shader set

### Workstream E. Documentation and Reference Cleanup

#### 목적

문서와 코드가 동일한 renderer mental model을 갖도록 맞춘다.

#### 작업 항목

- pipeline reference에서 `Lit_Gouraud` 제거
- execution/anatomy 문서의 lit pipeline 설명 수정
- 신규 phase 문서와 실제 제거 범위 일치 확인
- "현재 유지하는 shading model" 목록 명시

#### 우선 갱신 문서

- [render_pipeline_reference.md](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Docs/render_pipeline_reference.md:164)
- [render_pipeline_anatomy.md](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Docs/render_pipeline_anatomy.md:530)
- [render_execution.md](/abs/path/C:/Projects/Jungle_Week10_Team6/CrashEngine/Source/Engine/Render/Docs/render_execution.md:113)

#### 산출물

- Gouraud가 더 이상 현재 renderer 기능 목록에 없는 문서 상태

## 6. 추천 구현 순서

권장 구현 순서는 아래와 같다.

1. Workstream A
2. Workstream B
3. Workstream C
4. Workstream D
5. Workstream E

이 순서를 권장하는 이유는 다음과 같다.

- enum / model 체계를 먼저 줄여야 이후 코드가 "무엇을 없애야 하는지"가 분명해진다.
- UI를 빨리 제거해야 레거시 상태를 재현하는 경로가 줄어든다.
- registry를 정리해야 shader compile 경로가 안정된다.
- shader cleanup은 마지막에 몰아 처리하는 편이 빌드 에러 원인 추적이 쉽다.

## 7. 결정해야 할 세부 정책

구현 전 아래 항목을 확정하는 것이 좋다.

1. `Lit_Gouraud` 레거시 설정값을 무엇으로 치환할지
2. 기본 lit fallback을 `Lit_Phong`으로 할지 `Lit_Lambert`로 할지
3. `Surface.Gouraud` 필드를 이번 phase 안에서 완전히 제거할지
4. `DeferredDecal`의 Gouraud branch를 바로 제거할지, deferred isolation 시점까지 유예할지

### 권장 정책

- 레거시 `Lit_Gouraud` fallback: `Lit_Phong`
- 이유
  - 현재 기본 lit 명칭과 사용자 기대치에 가장 가깝다.
  - 후속 phase에서 forward 단일 경로로 고정해도 자연스럽다.

## 8. 리스크와 대응

| 리스크 | 설명 | 대응 |
|---|---|---|
| enum 재배치 회귀 | 저장된 값이 다른 mode로 해석될 수 있다 | load 시 legacy 값 치환 추가 |
| shader compile break | registry에서 제거한 entry를 다른 셰이더가 여전히 참조할 수 있다 | registry cleanup 후 전수 빌드 |
| deferred surface contract 잔재 | Gouraud용 packed surface 규약이 남아 혼동을 줄 수 있다 | 미사용 확인 후 정리 또는 TODO 명시 |
| 문서 불일치 | 코드 제거 후 문서가 남아 혼란 유발 | 같은 phase에서 문서 갱신 포함 |
| hidden dependency | overlay stat, debug ui, custom shader가 Gouraud를 가정할 수 있다 | 참조 검색 기반 검토와 smoke test 수행 |

## 9. 검증 계획

### 9.1 정적 검증

- `rg` 기준 `Lit_Gouraud`, `LIGHTING_MODEL_GOURAUD`, `PS_Forward_Gouraud`, `PS_Opaque_Gouraud`, `PS_Decal_Gouraud`, `ComputeGouraudLighting` 참조가 제거되었는지 확인
- build compile error 없이 shader variant 생성이 되는지 확인

### 9.2 기능 검증

- editor viewport가 `Lit_Phong`, `Lit_Lambert`, `Unlit`, `WorldNormal`, `SceneDepth`, `Wireframe`에서 정상 동작하는지 확인
- skeletal mesh viewer가 동일하게 동작하는지 확인
- lit mode에서 local light / directional light / shadow가 기존과 동등하게 보이는지 확인

### 9.3 회귀 검증

- 에디터 기존 설정 파일을 읽을 때 크래시 없이 fallback 되는지 확인
- shader warm-up 시 Gouraud compile 시도가 남아 있지 않은지 확인
- overlay stat / debug text가 잘못된 shading model 이름을 출력하지 않는지 확인

## 10. 완료 정의

`Phase 1`은 아래 조건이 모두 만족되면 완료로 본다.

- `Gouraud`가 editor UI에서 선택 불가하다.
- `Gouraud` 관련 view mode / shading model enum 경로가 제거되었다.
- pass registry와 shader permutation에서 Gouraud variant가 생성되지 않는다.
- Gouraud 전용 shader entry와 lighting helper가 제거되었다.
- 레거시 설정값이 안전한 lit mode로 fallback 된다.
- 관련 문서에서 Gouraud가 현재 지원 기능처럼 보이지 않는다.

## 11. 후속 Phase에 주는 이점

`Phase 1` 완료 후에는 다음 이점이 생긴다.

- `Phase 2`에서 forward 단일 경로 고정 시 고려할 lit mode 수가 감소한다.
- `Phase 3`에서 pipeline tree 정리 시 registry 분기가 줄어든다.
- `Phase 5`에서 forward light culling 이식 시 VS Gouraud 특수 케이스를 더 이상 다루지 않아도 된다.

즉, `Phase 1`은 기능 삭제 자체보다도 후속 renderer simplification의 난이도를 낮추는 준비 단계로 보는 것이 맞다.
