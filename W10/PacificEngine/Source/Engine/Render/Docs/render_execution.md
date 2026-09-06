# Render Execution Structure

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.2 |
| 기준 코드 | `Source/Engine/Render/Renderer.cpp`, `Execute/Registry/RenderPipelineRegistry.cpp`, `Execute/Runner/RenderPipelineRunner.cpp` |

## 1. 개요

렌더 실행은 `Pass`와 `Pipeline`을 조합한 트리 구조로 구성한다.

- `Pass`는 실제 GPU 작업을 수행하는 leaf node다.
- `Pipeline`은 pass 또는 하위 pipeline을 순서대로 묶는 내부 node다.
- `FRenderPipelineRunner`는 registry에 등록된 트리를 순회하며, 현재 `SceneView.ViewMode`와 pass registry 규칙에 따라 필요한 노드만 실행한다.

현재 mainline은 forward 기반이며, deferred subtree는 기본 실행 경로에 없다.

## 2. 실행 흐름

```text
FRenderer::BeginCollect
  reset collected data, draw commands, overlay batches, frame batches

FRenderer::CollectWorld / CollectDebugRender / CollectSkeletalDebug
  gather scene, light, overlay, debug data

FRenderer::CreatePipelineContext
  bind view, scene, target, resource, registry, and submission references

FRenderer::BuildDrawCommands
  build mesh and fullscreen commands
  update local-light SRV before command creation
  sort commands and compute pass ranges

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
  ExecutePresentPass
  EndFrame / Present
```

`BuildDrawCommands`는 submission 단계, `RunRootPipeline`은 execution 단계다. 둘은 분리되어 있으며, pass는 이미 만들어진 draw command range만 소비한다.

## 3. 현재 트리

### Root pipeline

```text
DefaultRootPipeline
  ScenePipeline
```

```text
EditorRootPipeline
  ScenePipeline
  OverlayPipeline
```

### Scene pipeline

```text
ScenePipeline
  ForwardPipeline
  PostProcessPipeline
  UIPass
```

### Forward pipeline

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

### Post / overlay

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

`PresentPass`는 tree 밖에서 `FRenderer::ExecutePresentPass`로 실행된다.

## 4. View mode routing

`FViewModePassRegistry` controls which subtrees are enabled for the active view mode.

| ViewMode | Active pipeline |
|---|---|
| `Lit_Lambert` | `ForwardLitPipeline` |
| `Lit_Phong` | `ForwardLitPipeline` |
| `Unlit` | `ForwardUnlitPipeline` |
| `Wireframe` | `ForwardUnlitPipeline` |
| `WorldNormal` | `ForwardWorldNormalPipeline` |
| `SceneDepth` | `ForwardSceneDepthPipeline` |

Current pass gating:

- `Lit_Lambert` and `Lit_Phong` enable depth pre-pass, lighting, alpha blend, height fog, and FXAA.
- `Unlit` enables depth pre-pass, opaque, alpha blend, height fog, and FXAA, but not lighting.
- `Wireframe` uses the unlit pipeline but disables depth pre-pass, alpha blend, height fog, and FXAA.
- `WorldNormal` uses depth pre-pass and opaque only.
- `SceneDepth` uses depth pre-pass and `NonLitViewModePass` only.

`AdditiveDecalPass` is registered, but every current view mode disables additive decals and no active pipeline references it.

## 5. Execution rules

`RenderPipelineRunner::ShouldExecutePipeline` and `ShouldExecutePass` are the two runtime gates.

- Pipeline gates are mostly view-mode based.
- Pass gates cover depth pre-pass, opaque, additive decal, alpha blend, non-lit view mode, fog, FXAA, and final post-process composite.
- `FinalPostProcessCompositePass` is only run when vignetting, gamma correction, fade, or letterbox is enabled.
- `PresentPass` is executed separately after the root pipeline, only when viewport targets exist.

## 6. Quick reference

- `DefaultRootPipeline` is for normal scene rendering.
- `EditorRootPipeline` adds overlay and editor-only passes.
- `ScenePipeline` is the main scene branch.
- `ForwardPipeline` selects the active view-mode subtree.
- `PostProcessPipeline` applies fog, FXAA, and final composite.
- `OverlayPipeline` contains editor/debug rendering.
- `OutlinePipeline` is a nested overlay pipeline used by editor selection outlines.
