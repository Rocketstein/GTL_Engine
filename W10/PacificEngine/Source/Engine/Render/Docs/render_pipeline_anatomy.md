# Rendering Pipeline Anatomy

> Scope: current PacificEngine renderer, centered on `Source/Engine/Render`.  `render_overview.md` is the canonical quick-start reference for the current tree.
> This document explains how a frame moves from world/component data to GPU work.

## 1. Mental Model

PacificEngine's renderer is split into four layers:

| Layer | Main files | Responsibility |
|---|---|---|
| Scene | `Scene/`, `Scene/Proxies/` | Own render-thread-facing proxies for primitives, lights, effects, UI, and editor helpers. |
| Submission | `Submission/Collect/`, `Submission/Command/` | Cull and classify proxies, then convert them into `FDrawCommand`s. |
| Execute | `Execute/Context/`, `Execute/Registry/`, `Execute/Passes/`, `Execute/Runner/` | Choose and run the pass tree for the current render path and view mode. |
| Resources/RHI | `Resources/`, `RHI/D3D11/` | Manage D3D11 buffers, shaders, render states, render targets, samplers, and shared per-frame data. |

The key distinction is:

- `Scene` says what exists.
- `Submission` says what should be drawn this frame.
- `Execute` says when each pass runs.
- `Resources/RHI` says how that work reaches D3D11.

The renderer avoids hard-coding one monolithic render function. Instead, it builds a command list up front, sorts it by pass/state/material, and then each render pass submits only the command range that belongs to it.

## 2. One Frame At A Glance

The normal flow is:

```text
FRenderer::BeginCollect
  reset collected data, draw commands, overlay batches, frame batches

FRenderer::CollectWorld
  FDrawCollector::CollectWorld
    CollectScenePrimitives
    CollectSceneLights
    CollectShadowCasters
    CollectEditorHelpers
    UpdateShadowDataInCBs

FRenderer::CreatePipelineContext
  bind SceneView, viewport targets, scene, renderer, device, resources,
  view-mode registry/surfaces, collected scene data, light culling, LOD context

FRenderer::BuildDrawCommands
  convert visible proxies and pass-level fullscreen/overlay work into FDrawCommand entries

FRenderer::RenderFrame
  BeginFrame
  RunRootPipeline
    PreparePipelineExecution
      UpdateFrameBuffer
      BindSystemSamplers
      DrawCommandList.Sort
      reset submit state cache
    FRenderPipelineRunner::ExecutePipeline
      recursively visit registered pipelines and passes
      pass.Execute(context)
        PrepareInputs
        PrepareTargets
        SubmitDrawCommands
    FinalizePipelineExecution
  ExecutePresentPass
  EndFrame / Present
```

Most frame-specific decisions are represented in `FRenderPipelineContext`, not stored globally in each pass.

## 3. SceneView: The Frame's Camera Contract

`FSceneView` is the renderer's per-view input. It carries:

- camera matrices and derived values: `View`, `Proj`, `CameraPosition`, frustum volume, camera basis vectors;
- viewport size: `ViewportWidth`, `ViewportHeight`;
- display settings: `ViewMode`, `RenderPath`, `ShowFlags`, post-process settings;
- visibility helpers: `OcclusionCulling`, `LODContext`.

`FRenderer::UpdateFrameBuffer` copies the important view data into `FFrameCBData` and binds it at `ECBSlot::Frame` for both VS and PS:

- `View`
- `Projection`
- `InvViewProj`
- `CameraWorldPos`
- wireframe flags/color
- total engine time

That means most shaders should treat the frame constant buffer as the source of truth for view transforms.

## 4. Scene Proxies

The renderer does not draw components directly. Components create/update scene proxies, and the scene owns those proxies.

Important proxy families:

| Proxy type | Purpose |
|---|---|
| `FPrimitiveProxy` | Base renderable primitive state: bounds, owner, pass, blend/depth/rasterizer overrides, per-object constants, material resources. |
| `FMeshSceneProxy` / `FStaticMeshSceneProxy` | Mesh rendering data and section/material state for static meshes. |
| `FSkeletalMeshSceneProxy` | Skeletal mesh render/debug proxy. Mesh rendering is currently stubbed, but skeleton debug extraction exists. |
| `FDecalSceneProxy` | Decal volume and decal material constants. |
| `FLightProxy` and derived light proxies | Ambient, directional, point, and spot light render data and shadow data. |
| `FUIProxy` | World/screen UI draw data. |
| editor helper proxies | Billboards, text, gizmo/debug primitives, selection visuals. |

`FScene::UpdateDirtyProxies`, `UpdateDirtyUIProxies`, and `UpdateDirtyLightProxies` are called during collection, so dirty component state is pushed into the proxy registry before visibility and command building.

## 5. Collection

`FDrawCollector` is the collector. It owns:

- `FCollectedSceneData`
- `FCollectedOverlayData`

### 5.1 Primitive Collection

`CollectScenePrimitives` performs:

1. Reset previous primitive buckets.
2. Update dirty scene/UI/light proxies.
3. Seed candidates with `bNeverCull` proxies.
4. Query the world's spatial partition using the view frustum.
5. Append frustum-visible proxies from the scene registry as a fallback.
6. Refresh LOD on a staggered schedule through `FLODUpdateContext`.
7. Run per-viewport updates for proxies that need view-dependent state.
8. Gather AABBs for GPU occlusion.
9. Skip occluded non-opaque proxies. Opaque proxies are still collected so depth/opaque commands can recover from stale occlusion readback.
10. Fill:
    - `VisibleProxies`
    - `OpaqueProxies`
    - `TransparentProxies`

The split is simple: proxies using `EBlendState::AlphaBlend` go to `TransparentProxies`; everything else goes to `OpaqueProxies`.

### 5.2 Light Collection

`CollectSceneLights` walks scene light proxies and produces:

- `VisibleLightProxies`
- `GlobalLights`, including ambient and directional lights
- `LocalLights`, including point and spot lights

Directional lights are stored in fixed global-light slots up to `MAX_DIRECTIONAL_LIGHTS`. Point and spot lights become entries in the local light structured buffer.

Shadow information is copied into the same collected light data:

- directional cascade matrices, splits, and sampling rects;
- spot shadow view-projection and sampling rect;
- point light cubemap face matrices and sampling rects;
- bias, slope bias, normal bias, sharpening, and ESM exponent values.

After shadow atlas allocation changes, `UpdateShadowDataInCBs` refreshes the collected light constant-buffer data so lighting shaders sample the right atlas regions.

### 5.3 Overlay Collection

Overlay data is separate from scene geometry. It feeds editor-only or debug passes:

- grid lines;
- debug lines and AABBs;
- editor helper billboards;
- editor helper text;
- selection visuals;
- skeletal debug instances.

`FRenderer::CollectDebugRender` also lets selected proxies contribute selected visuals via `CollectSelectedVisuals`.

`FRenderer::CollectSkeletalDebug` calls `DrawCollector.CollectSkeletalDebug(Scene.GetPrimitiveProxies())`. For skeletal meshes, `FSkeletalMeshSceneProxy::BuildSkeletalDebugInstance` pulls bone world matrices from the owning `USkinnedMeshComponent`.

## 6. Pipeline Context

`FRenderPipelineContext` is the shared execution packet passed to every pass.

Important fields:

| Field | Meaning |
|---|---|
| `SceneView` | Current view/camera/render-path configuration. |
| `Targets` | Viewport color/depth/copy targets. |
| `Scene` | Active scene being rendered. |
| `Renderer` | Owning `FRenderer`; used for registries, per-object CBs, batches, and pass lookup. |
| `Device` / `Context` | D3D11 device wrapper and immediate context. |
| `Resources` | `FFrameResources`: frame CBs, local light buffers, batches, UI buffers, etc. |
| `StateCache` | Shared draw-submit cache used to avoid redundant D3D11 state binds across pass ranges. |
| `DrawCommandList` | Sorted list of draw commands. |
| `RenderPassPresets` | Default state/topology presets per logical `ERenderPass`. |
| `ViewMode.Registry` | View-mode to shader/pass config mapping. |
| `Submission.SceneData` | Collected primitives/lights. |
| `Submission.OverlayData` | Collected debug/editor overlay data. |
| `LightCulling` | Tile-based light culling resources and outputs used by lit forward view modes. |
| `LODContext` | Current LOD update context. |

`CreatePipelineContext` acquires light-culling resources and view-mode config data per viewport. The mainline renderer no longer allocates deferred intermediate surfaces.

## 7. Draw Command Construction

`FRenderer::BuildDrawCommands` is the bridge from collected proxies to executable GPU work.

It performs these steps:

1. Ensure `Submission.SceneData` and `Submission.OverlayData` are populated.
2. Update the local light structured buffer before commands capture `LocalLightSRV`.
3. Build batched world text commands.
4. Query the `FViewModePassRegistry` to know whether the active view mode uses depth pre-pass, opaque, additive decals, alpha blend, lighting, non-lit visualization, height fog, and FXAA.
5. Iterate visible primitive proxies.
6. For opaque proxies, emit a depth pre-pass command when the view mode requires it.
7. Skip proxy passes disabled by the current view mode.
8. Ask the concrete render pass to build commands for that proxy.
9. Add selection-mask commands for selected outline-capable proxies.
10. Add pass-level commands for shadow maps, forward lit shading, non-lit view modes, fog, FXAA, final post-process, UI, outline, skeletal debug, editor lines, billboards, text, and light-hit-map overlay.

The proxy's `ERenderPass` maps to an execute pass by `MapPassToNodeType`:

| Proxy pass | Execute pass node |
|---|---|
| `DepthPre` | `DepthPrePass` |
| `ShadowMap` | `ShadowMapPass` |
| `Opaque` | `ForwardOpaquePass` |
| `AlphaBlend` | `AlphaBlendPass` |
| `SubUV` | `SubUVPass` |
| `SelectionMask` | `SelectionMaskPass` |
| `EditorLines` | `DebugLinePass` |
| `OverlayBillboard` | `OverlayBillboardPass` |
| `OverlayTextWorld` / `OverlayFont` | `OverlayTextPass` |
| `UI` | `UIPass` |
| `PostProcess` | `HeightFogPass` |

### 7.1 Mesh Commands

`DrawCommandBuild::BuildMeshDrawCommand` handles mesh commands.

For each mesh section, it creates an `FDrawCommand` containing:

- shader program;
- mesh buffer or raw vertex/index buffers;
- first index and index count;
- depth/stencil, blend, rasterizer, and topology;
- per-object constant buffer;
- material/per-shader constant buffers;
- global light constant buffer;
- local light SRV;
- diffuse/normal/specular SRVs;
- logical pass;
- sort key.

Shader selection depends on the pass:

- `ShadowMap` uses shadow encode shaders or depth-only depending on shadow filter mode.
- depth pre-pass and selection mask use `DepthOnly`.
- opaque commands may use a view-mode shader override from `FViewModePassRegistry`.
- otherwise the proxy's own shader is used.

Mask-like passes intentionally avoid material textures and material constant buffers.

### 7.2 Command Sorting And Ranges

`FDrawCommandList::Sort` sorts by `SortKey`, then computes `PassOffsets`.

Each pass later calls `GetPassRange(ERenderPass, start, end)` and submits only its own command range. This is why command construction can happen once, before the pipeline tree runs.

`SubmitCommand` caches bound state across commands:

- depth/stencil state;
- blend state;
- rasterizer state;
- primitive topology;
- shader program;
- mesh/raw buffers;
- per-object and per-shader constant buffers;
- light constant buffer;
- local light SRV;
- material SRVs.

Only changed state is rebound unless `bForceAll` is set.

## 8. Pass And Pipeline Registries

There are two different registries:

- `FRenderPassRegistry`: concrete pass object lookup by `ERenderPassNodeType`.
- `FRenderPipelineRegistry`: tree of pipelines and pass nodes by `ERenderPipelineType`.

The runner walks the pipeline tree recursively. A child can be another pipeline or a pass.

```text
Pipeline node
  child pipeline
    pass
    pass
  pass
```

`FRenderPipelineRunner` gates child execution through `ShouldExecutePipeline` and `ShouldExecutePass`.

Those functions inspect `ViewMode.ActiveViewMode` and `FViewModePassRegistry`.

## 9. Registered Pipeline Tree

Current root choices:

```text
DefaultRootPipeline
  ScenePipeline

EditorRootPipeline
  ScenePipeline
  OverlayPipeline
```

`ScenePipeline`:

```text
ScenePipeline
  ForwardPipeline
  PostProcessPipeline
  UIPass
```

Forward:

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

Post and editor overlay:

```text
PostProcessPipeline
  HeightFogPass
  FXAAPass
  FinalPostProcessCompositePass

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

PresentPipeline
  PresentPass
```

## 10. Render Pass Lifecycle

Every `FRenderPass` exposes:

```cpp
PrepareInputs(Context);
PrepareTargets(Context);
SubmitDrawCommands(Context);
```

`Execute` calls those three methods in that order.

The fourth method, `BuildDrawCommands`, is deliberately separate. It runs earlier during command-list construction.

This means passes have two responsibilities at different times:

- Build time: append commands for proxies or fullscreen work.
- Execute time: bind inputs/targets and submit the prebuilt command range.

## 11. Forward Path

### 11.1 Depth Pre-pass

`DepthPrePass` writes scene depth first.

Opaque mesh commands are emitted for `ERenderPass::DepthPre` when the view mode requires depth pre-pass. They use `DepthOnly` shader and no material SRVs.

The project uses reversed-Z behavior in places. After depth pre-pass, opaque drawing can use depth-read-only style state so equal-depth pixels from the same geometry survive the comparison.

### 11.2 Shadow Map

`ShadowMapPass` runs for lit forward paths.

It renders visible shadow casters from the light's point of view into shadow atlas resources:

- directional cascades;
- spot light shadow maps;
- point light cubemap faces.

The pass exposes shadow atlas SRVs and moment atlas SRVs. Opaque and lighting passes bind these into `ESystemTexSlot::ShadowAtlasBase` and `ESystemTexSlot::ShadowMomentAtlasBase`.

### 11.3 Light Culling

`TileBasedLightCulling` runs in lit forward modes.

It consumes:

- readable depth;
- local light structured buffer;
- frame/light-culling params.

It produces:

- per-tile light mask SRV;
- debug hit map SRV.

Forward opaque can bind the tile mask. The light-hit-map overlay can visualize the debug output.
### 11.4 Forward Opaque

`ForwardOpaquePass` writes directly to the viewport color target.

It binds:

- global light CB;
- local light SRV;
- shadow atlas SRVs;
- shadow moment atlas SRVs;
- tile mask/debug hit map from light culling when enabled.

## 12. View Modes

`FViewModePassRegistry` owns shader variants and pass usage rules for each view mode.

Current high-level routing:

| View mode | Pipeline |
|---|---|
| `Lit_Lambert` | `ForwardLitPipeline` |
| `Lit_Phong` | `ForwardLitPipeline` |
| `Unlit` | `ForwardUnlitPipeline` |
| `Wireframe` | `ForwardUnlitPipeline` |
| `WorldNormal` | `ForwardWorldNormalPipeline` |
| `SceneDepth` | `ForwardSceneDepthPipeline` |

Non-lit visualization modes use `NonLitViewModePass` when a fullscreen visualization is required. Forward world-normal is special: `ForwardOpaquePass` can output the world-normal view directly, avoiding an intermediate normal texture and fullscreen readback.

## 14. Post Process

`PostProcessPipeline` currently runs:

1. `HeightFogPass`
2. `FXAAPass`
3. `FinalPostProcessCompositePass`

`HeightFogPass` reads scene color/depth and applies fog. `FXAAPass` reads the scene color copy. `FinalPostProcessCompositePass` is gated by post-process settings:

- vignetting;
- gamma correction;
- fade;
- letterbox.

If none of those settings are enabled, the runner skips final composite.

## 15. UI And Overlay

`UIPass` is part of `ScenePipeline`, after post-process in the registered tree. It draws `FUIProxy` objects when `ShowFlags.bUI` is enabled.

UI command building:

1. Collect visible image/text UI proxies.
2. Sort by render space, layer, z-order, and proxy id.
3. Batch quads/text through `FUIBatch`.
4. Emit ordered commands with custom UI sort keys.

Editor overlay is only included under `EditorRootPipeline`.

Overlay pass order:

1. `LightHitMapPass`
2. `DebugLinePass`
3. `SelectionMaskPass`
4. `OutlinePass`
5. `SkeletalDebugPass`
6. `OverlayBillboardPass`
7. `GizmoPass`
8. `OverlayTextPass`

Selection outlining is a two-stage sub-pipeline:

- `SelectionMaskPass` writes selected object mask/stencil/depth data.
- `OutlinePass` runs the outline post-process using that mask.

## 16. Skeletal Mesh Notes

Skeletal mesh rendering is not fully wired yet, but skeletal debug already has a path through the renderer.

Current flow:

```text
USkinnedMeshComponent
  FSkeletalMeshSceneProxy
    BuildSkeletalDebugInstance
      bone world matrices

FRenderer::CollectSkeletalDebug
  FDrawCollector::CollectSkeletalDebug
    FCollectedOverlayData skeletal instances

FRenderer::BuildDrawCommands
  FSkeletalDebugPass::BuildDrawCommands
    DrawCommandBuild::BuildSkeletalDebugDrawCommand

EditorRootPipeline
  OverlayPipeline
    SkeletalDebugPass
      Submit ERenderPass::SkeletalDebug range
```

`FSkeletalMeshSceneProxy::RebuildSectionRenderData` is currently a stub with commented intended behavior. Once skeletal mesh asset parsing/formatting lands, that function is the likely bridge that should:

- get the active skeletal mesh asset and LOD;
- pick mesh buffers per LOD;
- build section render data;
- map material slots and overrides;
- resolve diffuse/normal/specular SRVs;
- build material constant buffers;
- sort sections by material/state;
- expose the resulting `MeshBuffer` and `SectionRenderData` to normal mesh command building.

The important integration point is that skeletal mesh sections should ideally look like other `FMeshSceneProxy` sections by the time `BuildMeshDrawCommand` sees them. That keeps skeletal mesh rendering inside the existing command/pass pipeline instead of creating a parallel renderer.

For GPU skinning, expect extra requirements beyond the current static mesh command shape:

- a skeletal vertex buffer layout with blend indices/weights;
- bone matrix palette upload, probably a per-shader CB or structured buffer;
- skeletal shader variants for depth, shadow, opaque, and debug modes;
- per-object dirty tracking for animation-updated bone data;
- compatible shadow/depth behavior so skeletal meshes cast shadows and participate in depth pre-pass.

## 17. Resource And Binding Conventions

Common constant buffer slots:

| Slot | Typical use |
|---|---|
| `ECBSlot::Frame` | Per-frame view/camera/time data. |
| `ECBSlot::PerObject` | World transform and per-object flags/indices. |
| `ECBSlot::PerShader0` | Material/pass-specific params, light-culling params, UI params, decal params. |
| `ECBSlot::PerShader1` | Additional material/pass-specific params. |
| `ECBSlot::Light` | Global light data. |

Common SRV slots:

| Slot family | Typical use |
|---|---|
| `t0`-style material slots | Diffuse/base, normal, specular, font, or pass-local primary texture. |
| `ESystemTexSlot::SceneDepth` | Depth copy SRV. |
| `ESystemTexSlot::SceneColor` | Scene color copy SRV. |
| `ESystemTexSlot::LocalLights` | Local light structured buffer. |
| `ESystemTexSlot::LightTileMask` | Tile light mask for lit forward view modes. |
| `ESystemTexSlot::DebugHitMap` | Light culling debug hit map. |
| `ESystemTexSlot::ShadowAtlasBase` | Shadow atlas pages. |
| `ESystemTexSlot::ShadowMomentAtlasBase` | Moment shadow atlas pages. |

Passes deliberately null SRVs around render-target writes to avoid D3D11 read/write hazards.

## 18. State Presets

`InitializeDefaultRenderPassPresets` defines the default draw state for each logical `ERenderPass`.

Examples:

| Pass | Depth | Blend | Rasterizer | Topology |
|---|---|---|---|---|
| `DepthPre` | `Default` | `NoColor` | `SolidBackCull` | triangle list |
| `ShadowMap` | `Default` | `Opaque` | `SolidFrontCull` | triangle list |
| `Opaque` | `Default` | `Opaque` | `SolidBackCull` | triangle list |
| `AlphaBlend` | `Default` | `AlphaBlend` | `SolidBackCull` | triangle list |
| `SubUV` | `DepthReadOnly` | `AlphaBlend` | `SolidNoCull` | triangle list |
| `SelectionMask` | `StencilWrite` | `NoColor` | `SolidNoCull` | triangle list |
| `EditorLines` | `DepthReadOnly` | `AlphaBlend` | `SolidBackCull` | line list |
| `UI` | `NoDepth` | `AlphaBlend` | `SolidNoCull` | triangle list |
| `SkeletalDebug` | `NoDepth` | `AlphaBlend` | `SolidBackCull` | triangle list |

Individual mesh sections can override depth, blend, and rasterizer state. `BuildMeshDrawCommand` applies section overrides unless the pass is mask-like.

## 19. Target Handoffs

The main target objects come from `FViewportRenderTargets`:

- viewport RTV/DSV;
- viewport render texture;
- depth texture and depth copy texture/SRV;
- scene color copy texture/SRV;
- source viewport pointer.

Typical target transitions:

- `BeginFrame` clears viewport color/depth and binds viewport RTV/DSV.
- `DepthPrePass` writes depth.
- forward opaque writes viewport color directly.
- post-process passes copy current viewport color into scene color copy when they need readable input.
- `PresentPass` copies viewport output to the final present target when rendering to a viewport render target.

D3D11 does not allow the same texture to be bound for reading and writing at the same time. This is why passes often:

- unbind render targets before copying;
- bind null SRVs before writing to a texture;
- copy depth/color into separate readable textures before fullscreen passes.

## 20. Where To Add New Rendering Work

Use these entry points:

| Goal | Likely place |
|---|---|
| Add a new mesh-like renderable | Create/update a scene proxy, collect it as a primitive, emit commands through a pass. |
| Add a new per-object visual mode | Extend `FViewModePassRegistry`, shader variants, and possibly `NonLitViewModePass`. |
| Add a new fullscreen post-process | Add a pass class, register it in `FRenderPassRegistry`, place it in `FRenderPipelineRegistry`, and build a fullscreen command. |
| Add a new editor overlay | Add overlay collection data, a pass or command builder, and register it under `OverlayPipeline`. |
| Add a new material texture/parameter | Extend section render data/material CB building and HLSL binding conventions. |
| Add skeletal mesh rendering | Fill skeletal proxy section data, add skeletal mesh buffers/layouts, add shader variants, and keep command construction compatible with `BuildMeshDrawCommand`. |

The safest pattern is to make new renderables conform to the existing proxy -> collected data -> draw command -> pass range flow.

## 21. Debugging Checklist

When something does not render:

1. Is the component creating a scene proxy?
2. Does `Scene.UpdateDirtyProxies` know about the dirty proxy?
3. Is the proxy inside the frustum or marked `bNeverCull`?
4. Is it being filtered by editor-world helper rules?
5. Is it visible after occlusion/LOD/per-viewport update?
6. Is its pass enabled by the current view mode?
7. Does `BuildDrawCommands` emit an `FDrawCommand` for the expected `ERenderPass`?
8. After `DrawCommandList.Sort`, does the pass range contain that command?
9. Is the expected pipeline active for the current `RenderPath` and `ViewMode`?
10. Does the pass bind a valid target before submitting?
11. Are SRVs unbound before a texture is used as a render target?
12. Does the command have a valid shader, mesh buffer/raw buffers, index count, and texture/CB inputs?
13. Is the shader expecting the same vertex layout and binding slots the command supplies?


