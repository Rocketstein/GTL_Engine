# Render Conventions: Naming

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.1 |

## 1. 기본 원칙

- 이름은 역할을 바로 드러내게 짓는다.
- 문서와 코드 모두 현재 코드 기준 타입명을 사용한다.
- `Pass`, `Pipeline`, `Context`, `Resources`를 접미사로 써서 역할을 구분한다.
- 현재 mainline은 `Forward` 접두사를 사용한다.
- `Deferred` 접두사는 복원 노트나 레거시 참조에만 남긴다.
- 여러 렌더 경로에서 공유하는 요소는 기존 이름을 유지한다.

## 2. Pass Naming

- 실제 GPU 작업 단위는 `Pass` 접미사를 사용한다.
- forward 전용 pass는 `Forward` 접두사를 붙인다.
- 여러 렌더 경로에서 공유하는 pass는 기존 이름을 유지한다.
- 이름은 수행 작업을 바로 드러내게 짓는다.

예
- `DepthPrePass`
- `ForwardOpaquePass`
- `FXAAPass`
- `PresentPass`
- `OverlayTextPass`
- `NonLitViewModePass`

## 3. Pipeline Naming

- 실행 경로를 묶는 단위는 `Pipeline` 접미사를 사용한다.
- 렌더 경로 분기가 생기면 현재 mainline 기준으로 `ForwardPipeline`을 둔다.
- 경로 전용 하위 pipeline은 `ForwardLitPipeline`, `ForwardSceneDepthPipeline`처럼 접두사를 붙인다.
- 여러 렌더 경로에서 공유하는 pipeline은 기존 이름을 유지한다.
- 최상위 진입점일 때만 `RootPipeline`을 사용한다.

예
- `ScenePipeline`
- `ForwardPipeline`
- `ForwardLitPipeline`
- `ForwardSceneDepthPipeline`
- `PostProcessPipeline`
- `OverlayPipeline`
- `OutlinePipeline`
- `DefaultRootPipeline`
- `EditorRootPipeline`

## 4. Context Naming

- 실행 중 공유되는 문맥 타입은 `Context` 접미사를 사용한다.
- 상위 공통 문맥과 하위 역할 문맥을 이름으로 구분한다.

예
- `FRenderPipelineContext`
- `FViewModeExecutionContext`
- `FRenderSubmissionContext`
- `FRenderCollectContext`
- `FSceneView`

## 5. Resource Naming

- 프레임 공용 리소스 묶음은 `Resources`를 사용한다.
- viewport 출력 묶음은 `RenderTargets`를 사용한다.
- tile light culling 묶음은 `LightCulling`을 사용한다.
- C++ constant buffer payload 타입은 `CBData`를 사용한다.

예
- `FFrameResources`
- `FViewportRenderTargets`
- `FTileBasedLightCulling`
- `FFrameCBData`
- `FPerObjectCBData`
- `FFogCBData`

## 6. Shader Buffer Naming

- C++ constant buffer payload 타입은 `*CBData`를 사용한다.
- HLSL struct 타입은 `CBData`를 붙이지 않는다.
- HLSL `cbuffer` 이름은 `*Params`를 사용한다.

예
- C++: `FAmbientLightCBData`, `FDirectionalLightCBData`, `FLocalLightCBData`
- HLSL struct: `FAmbientLight`, `FDirectionalLight`, `FLocalLight`
- HLSL cbuffer: `FrameParams`, `PerObjectParams`, `GlobalLightParams`

## 7. 실행 정의 데이터 Naming

| 용어 | 의미 |
|---|---|
| `Desc` | 구조나 객체의 정체성을 설명하는 정의 데이터 |
| `Config` | 조건에 따라 무엇을 생성할지 결정하는 설정 데이터 |
| `Preset` | 실행 시 바로 적용할 수 있는 고정 사용값 묶음 |

예
- `FRenderPipelineDesc`
- `FViewModePassConfig`
- `FViewModePassDesc`
- `FRenderPassPreset`
- `FRenderPassDrawPreset`

## 8. Vertex Contract Naming

- vertex semantic 집합의 canonical preset은 `*Preset` 정적 집합으로 둔다.
- 현재 vertex semantic descriptor preset은 `FVertexSemanticDescriptorPreset`에 모은다.
- preset 이름은 shader contract 읽기에 필요한 semantic 묶음을 짧게 드러내되, 과도한 helper 함수명은 피한다.

예
- `FVertexSemanticDescriptorPreset::PC`
- `FVertexSemanticDescriptorPreset::PT`
- `FVertexSemanticDescriptorPreset::PUVC`
- `FVertexSemanticDescriptorPreset::PNCT`
- `FVertexSemanticDescriptorPreset::PNCTT`
- `FVertexSemanticDescriptorPreset::PNCTTUV1`

규칙:

- runtime upload 계약은 vertex struct type 추론보다 descriptor preset 또는 reflected request list를 우선 source of truth로 둔다.
- 새 preset이 필요하면 helper 함수 추가보다 preset 집합에 정적 항목을 추가한다.

