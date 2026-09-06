# Render Resources

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.2 |
| 기준 코드 | `Source/Engine/Render/Renderer.cpp`, `Resources/FrameResources.h`, `Execute/Context/RenderPipelineContext.h`, `Execute/Context/Viewport/ViewportRenderTargets.h` |

## 1. 개요

렌더 리소스는 크게 3개 축으로 본다.

1. `Context`
   - pass와 pipeline 실행 중 필요한 공용 상태와 참조를 전달한다.
2. `Runtime Resources`
   - 실제 D3D11 GPU resource, state object, sampler, batch, cache를 보관한다.
3. `Constant Buffer Payload`
   - CPU에서 구성해 GPU constant buffer로 업로드하는 데이터 레이아웃이다.

`FRenderPipelineContext`는 resource owner가 아니라 실행 중 참조를 모아 넘기는 패킷이다. 실제 소유권은 `FFrameResources`, `FViewportRenderTargets`, `FShaderManager`, `FViewModePassRegistry`, `FConstantBufferCache` 같은 관리 객체에 있다.

## 2. Execution context

### 2.1 `FViewModeExecutionContext`

- `Registry`
- `ActiveViewMode`

현재 view mode와 registry를 pass runner와 pass 구현에 전달한다.

### 2.2 `FRenderSubmissionContext`

- `SceneData`
- `OverlayData`

submission 단계에서 만든 scene/overlay 결과를 execution 단계로 전달한다.

### 2.3 `FRenderPipelineContext`

| Field | 역할 |
|---|---|
| `SceneView` | camera, render option, show flag, viewport 정보 |
| `Targets` | viewport RTV/DSV 및 copy target |
| `Scene` | 현재 scene |
| `Renderer` | owning `FRenderer` |
| `Device` / `Context` | D3D11 device wrapper와 immediate context |
| `Resources` | `FFrameResources` |
| `StateCache` | draw submit state cache |
| `DrawCommandList` | sorted draw commands |
| `RenderPassPresets` | logical pass별 기본 state preset |
| `Occlusion` | occlusion helper |
| `LightCulling` | optional tile-based light culling helper; currently null in the default renderer path |
| `LODContext` | LOD update context |
| `ViewMode` | active view mode execution context |
| `Submission` | collected scene/overlay data |

## 3. Frame-scoped resources

`FFrameResources`가 프레임 공용 리소스를 소유한다.

| Type | 역할 |
|---|---|
| `FrameBuffer` | `b0` frame/view constant buffer |
| `PerObjectConstantBuffer` | fallback per-object CB |
| `GlobalLightBuffer` | `b4` global light CB |
| `ShadowPassBuffer` | `b5` shadow pass CB |
| `LinearClampSampler` | `s0` |
| `LinearWrapSampler` | `s1` |
| `PointClampSampler` | `s2` |
| `ShadowSampler` | `s3`, comparison sampler |
| `LocalLightBuffer` | local light structured buffer |
| `LocalLightSRV` | local light SRV |
| `PerObjectCBPool` | proxy id 기반 per-object CB pool |
| `PerBoneDebugCBPool` | skeletal debug CB pool |
| `TextBatch` | world text/font batching |
| `UIBatch` | UI batching |

`FrameResources.BeginFrame()`는 매 프레임 배치와 일부 카운터를 정리하고, `UpdateLocalLights()`는 local light 개수에 맞춰 structured buffer와 SRV를 재사용 또는 확장한다.

## 4. Viewport-scoped resources

`FViewportRenderTargets`는 viewport 수명에 종속된 타깃을 묶는다.

| Type | 역할 |
|---|---|
| `ViewportRTV` | 현재 viewport color target |
| `ViewportDSV` | 현재 viewport depth target |
| `ViewportRenderTexture` | viewport color texture |
| `SceneColorCopyTexture` / `SceneColorCopySRV` | fog / FXAA / outline 입력용 color copy |
| `DepthTexture` | viewport depth texture |
| `DepthCopyTexture` / `DepthCopySRV` | scene depth / fog / outline 입력용 depth copy |
| `StencilCopySRV` | outline / selection 계열에서 읽는 stencil copy |

`D3D11`은 같은 texture를 읽기와 쓰기에 동시에 둘 수 없으므로, 후처리 패스는 copy SRV를 읽고 viewport RTV/DSV를 쓴다.

## 5. Persistent managers

| Manager | 역할 |
|---|---|
| `FShaderManager` | built-in shader와 custom shader cache를 관리한다. |
| `FConstantBufferCache` | pass-local constant buffer 재사용을 관리한다. |
| `FViewModePassRegistry` | view-mode별 pass 규칙과 shader variant를 관리한다. |
| `FRenderPassRegistry` | concrete pass instance와 pass preset을 관리한다. |
| `FRenderPipelineRegistry` | pipeline tree를 관리한다. |

## 6. Binding slots

현재 코드 기준 대표 슬롯은 아래와 같다.

### Constant buffers

| Slot | 이름 | 용도 |
|---|---|---|
| `b0` | `ECBSlot::Frame` | frame/view data |
| `b1` | `ECBSlot::PerObject` | per-object data |
| `b2` | `ECBSlot::PerShader0` | pass/material params |
| `b3` | `ECBSlot::PerShader1` | pass/material params |
| `b4` | `ECBSlot::Light` | global light data |
| `b5` | `ECBSlot::ShadowPass` / `PerShader2` | shadow pass / extra pass params |

### System SRVs

| Slot | 이름 | 용도 |
|---|---|---|
| `t6` | `ESystemTexSlot::LocalLights` | local light buffer |
| `t7` | `ESystemTexSlot::LightTileMask` | tile light mask |
| `t8` | `ESystemTexSlot::DebugHitMap` | light culling debug hit map |
| `t10` | `ESystemTexSlot::SceneDepth` | depth copy |
| `t11` | `ESystemTexSlot::SceneColor` | color copy |
| `t13` | `ESystemTexSlot::Stencil` | stencil copy |
| `t20`-`t23` | `ESystemTexSlot::ShadowAtlasBase` | shadow atlas pages |
| `t48`-`t51` | `ESystemTexSlot::ShadowMomentAtlasBase` | moment atlas pages |

### Samplers

| Slot | 이름 |
|---|---|
| `s0` | `ESamplerSlot::LinearClamp` |
| `s1` | `ESamplerSlot::LinearWrap` |
| `s2` | `ESamplerSlot::PointClamp` |
| `s3` | `ShadowSampler` |

## 7. Important note

`FViewModeSurfaces`는 현재 mainline renderer에 없다. 현재 non-lit visualization은 `NonLitViewModePass`와 viewport copy targets로 처리하며, deferred intermediate surface를 소유하지 않는다.
