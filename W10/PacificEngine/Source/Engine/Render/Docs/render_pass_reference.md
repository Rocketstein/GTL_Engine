# Render Pass Reference

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.2 |
| 기준 코드 | `Source/Engine/Render/Execute/Registry/RenderPassRegistry.cpp`, `Source/Engine/Render/Execute/Passes/*` |

> 이 문서는 현재 등록된 concrete pass와 실제 실행 위치를 정리한다.
> 일부 pass는 registry에는 있지만 pipeline tree에는 없거나, tree 밖에서 별도로 실행된다.

## 1. Scene passes

### `DepthPrePass`

| 항목 | 내용 |
|---|---|
| 역할 | scene depth를 먼저 기록하는 depth pre-pass |
| 소속 파이프라인 | `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline`, `ForwardSceneDepthPipeline` |
| 주요 입력 | visible primitive, per-object constant buffer |
| 주요 출력 | viewport depth/stencil |

### `ShadowMapPass`

| 항목 | 내용 |
|---|---|
| 역할 | shadow casting light 기준으로 shadow atlas를 만든다 |
| 소속 파이프라인 | `ForwardLitPipeline` |
| 주요 입력 | `VisibleLightProxies`, light별 shadow caster, per-object CB |
| 주요 출력 | shadow atlas DSV / SRV, moment atlas SRV |

### `ForwardOpaquePass`

| 항목 | 내용 |
|---|---|
| 역할 | forward path의 opaque geometry를 처리한다 |
| 소속 파이프라인 | `ForwardLitPipeline`, `ForwardUnlitPipeline`, `ForwardWorldNormalPipeline` |
| 주요 입력 | visible primitive, material texture, per-object CB, global light CB, local light SRV, shadow atlas SRV |
| 주요 출력 | viewport color |
| 비고 | lit / unlit / world-normal shader variant를 view mode registry가 선택한다. world normal은 viewport color에 직접 쓴다. |

### `AdditiveDecalPass`

| 항목 | 내용 |
|---|---|
| 역할 | additive decal geometry를 렌더한다 |
| 소속 | registry에는 등록되어 있으나 현재 tree에는 포함되지 않는다 |
| 주요 입력 | decal primitive, material texture, per-object CB |
| 주요 출력 | viewport color |

### `AlphaBlendPass`

| 항목 | 내용 |
|---|---|
| 역할 | alpha blended geometry를 렌더한다 |
| 소속 파이프라인 | `ForwardLitPipeline`, `ForwardUnlitPipeline` |

### `SubUVPass`

| 항목 | 내용 |
|---|---|
| 역할 | SubUV sprite-sheet geometry를 렌더한다 |
| 소속 파이프라인 | `ForwardLitPipeline`, `ForwardUnlitPipeline` |

### `NonLitViewModePass`

| 항목 | 내용 |
|---|---|
| 역할 | scene depth 같은 비조명 view mode를 fullscreen으로 출력한다 |
| 소속 파이프라인 | `ForwardSceneDepthPipeline` |
| 주요 입력 | scene depth SRV, variant-specific CB |
| 비고 | 현재 tree에서 실사용되는 variant는 `SceneDepth`다. |

## 2. Post-process passes

### `HeightFogPass`

| 항목 | 내용 |
|---|---|
| 역할 | height fog를 fullscreen으로 적용한다 |
| 소속 파이프라인 | `PostProcessPipeline` |
| 주요 입력 | scene color copy SRV, depth copy SRV, fog CB |

### `FXAAPass`

| 항목 | 내용 |
|---|---|
| 역할 | FXAA를 fullscreen으로 적용한다 |
| 소속 파이프라인 | `PostProcessPipeline` |
| 주요 입력 | scene color copy SRV |

### `FinalPostProcessCompositePass`

| 항목 | 내용 |
|---|---|
| 역할 | vignette, gamma correction, fade, letterbox를 합성한다 |
| 소속 파이프라인 | `PostProcessPipeline` |
| 실행 조건 | 해당 post-process 옵션 중 하나 이상이 켜져 있을 때만 실행 |

### `PresentPass`

| 항목 | 내용 |
|---|---|
| 역할 | viewport output을 final present target으로 복사한다 |
| 소속 | registry tree 밖에서 `FRenderer::ExecutePresentPass`가 별도로 호출한다 |

## 3. Overlay / editor passes

### `LightHitMapPass`

| 항목 | 내용 |
|---|---|
| 역할 | light culling debug hit map을 화면에 보여준다 |
| 소속 파이프라인 | `OverlayPipeline` |

### `DebugLinePass`

| 항목 | 내용 |
|---|---|
| 역할 | debug line batch를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### `SelectionMaskPass`

| 항목 | 내용 |
|---|---|
| 역할 | 선택된 오브젝트의 mask/stencil/depth를 기록한다 |
| 소속 파이프라인 | `OutlinePipeline` |

### `OutlinePass`

| 항목 | 내용 |
|---|---|
| 역할 | selection mask를 사용해 outline 후처리를 적용한다 |
| 소속 파이프라인 | `OutlinePipeline` |

### `SkeletalDebugPass`

| 항목 | 내용 |
|---|---|
| 역할 | skeletal debug line/node overlay를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### `OverlayBillboardPass`

| 항목 | 내용 |
|---|---|
| 역할 | editor helper billboard를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### `GizmoPass`

| 항목 | 내용 |
|---|---|
| 역할 | transform gizmo를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

### `OverlayTextPass`

| 항목 | 내용 |
|---|---|
| 역할 | world/screen overlay text를 렌더한다 |
| 소속 파이프라인 | `OverlayPipeline` |

## 4. Current wiring notes

- `GridPass` node type is declared, but it is not registered in the active pass registry.
- `AdditiveDecalPass` is registered, but no active pipeline tree references it today.
- `PresentPass` is not part of the tree and is executed separately after the root pipeline.
- `ForwardWorldNormalPipeline` uses the `ForwardOpaquePass` world-normal shader variant directly instead of an intermediate fullscreen normal pass.
