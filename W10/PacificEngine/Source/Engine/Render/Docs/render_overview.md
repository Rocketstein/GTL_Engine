# Render Overview

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.0 |

> Scope: current `PacificEngine` renderer centered on `Source/Engine/Render`.
> 목적: 다음 작업자가 문서만 보고도 현재 렌더 경로와 파이프라인 선택 규칙을 빠르게 파악할 수 있게 하는 개요 문서.

## 1. 현재 구조

렌더러는 아래 5개 축으로 나뉜다.

| Layer | Main files | Responsibility |
|---|---|---|
| Scene | `Scene/`, `Scene/Proxies/` | 프록시와 장면 상태를 소유한다. |
| Submission | `Submission/Collect/`, `Submission/Command/` | 가시성 판정 후 draw command를 만든다. |
| Execute | `Execute/Context/`, `Execute/Registry/`, `Execute/Passes/`, `Execute/Runner/` | 현재 view mode와 render path에 맞는 pass tree를 실행한다. |
| Resources | `Resources/`, `RHI/D3D11/` | 공용 constant buffer, sampler, shader, state object를 관리한다. |
| Visibility | `Visibility/` | frustum, occlusion, LOD, light culling을 담당한다. |

핵심 흐름은 `Scene`가 무엇이 존재하는지 말하고, `Submission`이 이번 프레임에 무엇을 그릴지 정리하고, `Execute`가 언제 어떤 pass를 돌릴지 결정하고, `Resources/RHI`가 그 작업을 D3D11로 보낸다는 점이다.

## 2. 한 프레임 흐름

```text
FRenderer::BeginCollect
  reset collected data, draw commands, overlay batches, frame batches

FRenderer::CollectWorld
  FDrawCollector::CollectWorld
    CollectScenePrimitives
    CollectSceneLights
    CollectShadowCasters
    CollectDebugRender / CollectSkeletalDebug

FRenderer::CreatePipelineContext
  bind SceneView, viewport targets, scene, renderer, device, resources,
  view-mode registry, collected scene/overlay data, occlusion, LOD context

FRenderer::BuildDrawCommands
  build proxy draw commands and fullscreen/overlay commands
  update local light buffer before commands capture LocalLightSRV
  sort draw commands by sort key and pass range

FRenderer::RenderFrame
  BeginFrame
  PreparePipelineExecution
    UpdateFrameBuffer
    BindSystemSamplers
    DrawCommandList.Sort
    reset submit-state cache
  RunRootPipeline
    FRenderPipelineRunner::ExecutePipeline
      recursively visit registered pipelines and passes
      pass.Execute(context)
        PrepareInputs
        PrepareTargets
        SubmitDrawCommands
  ExecutePresentPass
  EndFrame / Present
```

`BuildDrawCommands`와 `Execute`는 분리되어 있다. 명령은 미리 한 번 만들고, 파이프라인 실행은 정렬된 범위만 소비한다.

## 3. 현재 파이프라인 트리

### Root

```text
DefaultRootPipeline
  ScenePipeline
```

```text
EditorRootPipeline
  ScenePipeline
  OverlayPipeline
```

`PresentPass`는 tree 안에서 돌지 않고 `FRenderer::ExecutePresentPass`가 root pipeline 이후 별도로 호출한다.

### Scene

```text
ScenePipeline
  ForwardPipeline
  PostProcessPipeline
  UIPass
```

### Forward

```text
ForwardPipeline
  ForwardLitPipeline
    DepthPrePass
    ShadowMapPass
    ForwardOpaquePass
    AlphaBlendPass
    SubUVPass

  ForwardUnlitPipeline
    DepthPrePass
    ForwardOpaquePass
    AlphaBlendPass
    SubUVPass

  ForwardWorldNormalPipeline
    DepthPrePass
    ForwardOpaquePass

  ForwardSceneDepthPipeline
    DepthPrePass
    NonLitViewModePass
```

### Post / Overlay

```text
PostProcessPipeline
  HeightFogPass
  FXAAPass
  FinalPostProcessCompositePass
```

```text
OverlayPipeline
  LightHitMapPass
  DebugLinePass
  OutlinePipeline
    SelectionMaskPass
    OutlinePass
  SkeletalDebugPass
  OverlayBillboardPass
  GizmoPass
  OverlayTextPass
```

## 4. View mode routing

`FViewModePassRegistry` drives both pipeline gating and shader variant selection.

| ViewMode | Pipeline |
|---|---|
| `Lit_Lambert` | `ForwardLitPipeline` |
| `Lit_Phong` | `ForwardLitPipeline` |
| `Unlit` | `ForwardUnlitPipeline` |
| `Wireframe` | `ForwardUnlitPipeline` |
| `WorldNormal` | `ForwardWorldNormalPipeline` |
| `SceneDepth` | `ForwardSceneDepthPipeline` |

Current pass enable rules:

- `Lit_Lambert` and `Lit_Phong` enable depth pre-pass, lighting, alpha blend, height fog, and FXAA.
- `Unlit` enables depth pre-pass, opaque, alpha blend, height fog, and FXAA, but not lighting.
- `Wireframe` uses `ForwardUnlitPipeline` but disables depth pre-pass, alpha blend, fog, and FXAA.
- `WorldNormal` uses depth pre-pass and opaque only.
- `SceneDepth` uses depth pre-pass and `NonLitViewModePass` only.

`AdditiveDecalPass` exists in the registry and has draw-command support, but all current view modes disable additive decals and no active pipeline tree references the pass today.

## 5. Execution contexts

`FRenderPipelineContext` is the shared packet passed through every pass.

| Field | Meaning |
|---|---|
| `SceneView` | current camera and display settings |
| `Targets` | viewport RTV/DSV and copy targets |
| `Scene` | active scene |
| `Renderer` | owning `FRenderer` |
| `Device` / `Context` | D3D11 device wrapper and immediate context |
| `Resources` | `FFrameResources` |
| `StateCache` | shared draw-submit state cache |
| `DrawCommandList` | sorted draw commands |
| `RenderPassPresets` | default per-pass state presets |
| `ViewMode.Registry` | active `FViewModePassRegistry` |
| `Submission.SceneData` | collected primitive/light data |
| `Submission.OverlayData` | collected overlay/editor data |
| `Occlusion` | GPU occlusion helper |
| `LightCulling` | optional tile-based light culling helper; currently null in the default renderer path |
| `LODContext` | LOD update context |

## 6. Resource buckets

### Frame-scoped

- `FFrameResources::FrameBuffer`
- `GlobalLightBuffer`
- `ShadowPassBuffer`
- `PerObjectCBPool`
- `LocalLightBuffer` / `LocalLightSRV`
- `TextBatch`
- `UIBatch`
- system samplers (`LinearClamp`, `LinearWrap`, `PointClamp`, `ShadowSampler`)

### Viewport-scoped

- `ViewportRTV` / `ViewportDSV`
- `ViewportRenderTexture`
- `SceneColorCopyTexture` / `SceneColorCopySRV`
- `DepthTexture`
- `DepthCopyTexture` / `DepthCopySRV`
- `StencilCopySRV`

## 7. Registered vs wired

A few names in the codebase are easy to confuse, so this is the current status.

- `GridPass` exists as a node type and a pass class, but it is not registered in `FRenderPassRegistry` and is not part of any active tree.
- `AdditiveDecalPass` is registered, but no active tree currently includes it.
- `PresentPass` is registered and executed separately after the root tree.
- `LightHitMapPass` is editor-only in practice because it only appears under `EditorRootPipeline`.

## 8. Reading order

If you want the shortest route to understanding the renderer, read these in order:

1. `render_overview.md`
2. `render_execution.md`
3. `render_pipeline_reference.md`
4. `render_pass_reference.md`
5. `render_resource.md`
6. `Source/Engine/Render/Renderer.cpp`
7. `Source/Engine/Render/Execute/Registry/RenderPipelineRegistry.cpp`
8. `Source/Engine/Render/Execute/Registry/ViewModePassRegistry.cpp`




