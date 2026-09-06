# Render Pipeline Reference

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.2 |
| 기준 코드 | `Source/Engine/Render/Execute/Registry/RenderPipelineRegistry.cpp`, `Source/Engine/Render/Execute/Runner/RenderPipelineRunner.cpp` |

## 1. 개요

이 문서는 현재 엔진에 등록된 pipeline tree와 view-mode 선택 규칙을 정리한다.

- 실제 tree 정의: `RenderPipelineRegistry.cpp`
- 실행 조건: `RenderPipelineRunner.cpp`
- pass 세부 설명: `render_pass_reference.md`

## 2. Root pipelines

### `DefaultRootPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | 일반 scene 렌더링 진입점 |
| 자식 | `ScenePipeline` |

### `EditorRootPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene 렌더링 뒤 editor overlay까지 포함하는 진입점 |
| 자식 | `ScenePipeline`, `OverlayPipeline` |

## 3. Scene branch

### `ScenePipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene rendering 상위 분기 |
| 자식 | `ForwardPipeline`, `PostProcessPipeline`, `UIPass` |

### `ForwardPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | forward shading 하위 분기 |
| 실행 조건 | `SceneView.ViewMode`에 따라 `ForwardLitPipeline` / `ForwardUnlitPipeline` / `ForwardWorldNormalPipeline` / `ForwardSceneDepthPipeline` 중 하나만 활성화 |
| 자식 | `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline`, `ForwardSceneDepthPipeline` |

## 4. Forward subtrees

### `ForwardLitPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | lit view mode용 forward opaque 경로 |
| 실행 조건 | `Lit_Lambert`, `Lit_Phong` |
| 자식 | `DepthPrePass`, `ShadowMapPass`, `ForwardOpaquePass`, `AlphaBlendPass`, `SubUVPass` |

### `ForwardUnlitPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | unlit / wireframe 경로 |
| 실행 조건 | `Unlit`, `Wireframe` |
| 자식 | `DepthPrePass`, `ForwardOpaquePass`, `AlphaBlendPass`, `SubUVPass` |

### `ForwardWorldNormalPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | world normal visualization |
| 실행 조건 | `WorldNormal` |
| 자식 | `DepthPrePass`, `ForwardOpaquePass` |

### `ForwardSceneDepthPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene depth visualization |
| 실행 조건 | `SceneDepth` |
| 자식 | `DepthPrePass`, `NonLitViewModePass` |

## 5. Post / overlay / present

### `PostProcessPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | scene 결과에 공통 후처리를 적용 |
| 자식 | `HeightFogPass`, `FXAAPass`, `FinalPostProcessCompositePass` |

### `OverlayPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | editor/debug overlay 렌더링 |
| 자식 | `LightHitMapPass`, `DebugLinePass`, `OutlinePipeline`, `SkeletalDebugPass`, `OverlayBillboardPass`, `GizmoPass`, `OverlayTextPass` |

### `OutlinePipeline`

| 항목 | 내용 |
|---|---|
| 역할 | selection outline 전용 하위 분기 |
| 자식 | `SelectionMaskPass`, `OutlinePass` |

### `PresentPipeline`

| 항목 | 내용 |
|---|---|
| 역할 | viewport output을 present target으로 복사/출력 |
| 자식 | `PresentPass` |

## 6. View mode summary

| ViewMode | Pipeline |
|---|---|
| `Lit_Lambert` | `ForwardLitPipeline` |
| `Lit_Phong` | `ForwardLitPipeline` |
| `Unlit` | `ForwardUnlitPipeline` |
| `Wireframe` | `ForwardUnlitPipeline` |
| `WorldNormal` | `ForwardWorldNormalPipeline` |
| `SceneDepth` | `ForwardSceneDepthPipeline` |

## 7. Notes

- `ScenePipeline` is the main scene branch in both root trees.
- `UIPass` lives directly under `ScenePipeline`, not under `OverlayPipeline`.
- `PresentPass` is executed outside the registry tree by `FRenderer::ExecutePresentPass`.
- `AdditiveDecalPass` is registered in the pass registry, but the current pipeline tree does not reference it.
- `GridPass` is not part of the active tree.
