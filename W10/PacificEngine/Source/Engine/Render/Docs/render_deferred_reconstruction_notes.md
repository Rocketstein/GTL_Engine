# Deferred Reconstruction Notes

## Purpose

This document preserves the minimum design intent of the removed deferred subtree so the renderer can be reintroduced later without keeping dormant runtime code in the mainline.

## Why It Was Removed

- The active renderer path is now forward-only.
- Deferred and forward code paths had diverged enough to raise onboarding and maintenance cost.
- The deferred subtree depended on intermediate surface contracts that no longer match the simplified renderer flow.

## Previously Removed Runtime Pieces

- `ViewModeSurfaces`
- `DeferredOpaquePass`
- `DeferredDecalPass`
- `DeferredLightingPass`
- `LightCullingPass`
- Deferred pipeline enums and pipeline registry entries
- Deferred pass-node enums and pass registry entries
- Deferred fullscreen draw-command path

## Previous Pass Order

The old deferred lit path was composed in this order:

1. `DepthPrePass`
2. `ShadowMapPass`
3. `LightCullingPass`
4. `DeferredOpaquePass`
5. `DeferredDecalPass`
6. `DeferredLightingPass`
7. optional transparent passes

The unlit and debug variants removed some of the later stages but still relied on the same intermediate surface contract.

## Previous Surface Contract

`ViewModeSurfaces` owned the deferred intermediate render targets that were consumed across passes.

- `BaseColor`
- `Surface1`
- `Surface2`
- `ModifiedBaseColor`
- `ModifiedSurface1`
- `ModifiedSurface2`

In practice:

- Lambert used base color plus normal-oriented surface data.
- Blinn-Phong additionally used material parameter data.
- Decal wrote into the modified surfaces.
- Deferred lighting sampled both original and modified surfaces when present.

## Shader Entry Points That Backed The Removed Path

- `Shaders/Passes/Scene/Deferred/DeferredOpaquePass.hlsl`
- `Shaders/Passes/Scene/Deferred/DeferredDecalPS.hlsl`
- `Shaders/Passes/Scene/Deferred/DeferredLightingPS.hlsl`
- `Shaders/Passes/Visibility/LightCullingPassCS.hlsl`

These shader files are intentionally left in the repository for reference and for debug assets that still include parts of the old deferred wrappers.

## Reintroduction Guidance

If deferred or hybrid rendering is needed again, do not restore the deleted C++ subtree verbatim. Rebuild it against the current forward-only pipeline contracts.

Recommended order:

1. Define a fresh intermediate surface owner that is explicit about lifetime and viewport ownership.
2. Reintroduce deferred pass-node enums only after the pass classes exist again.
3. Add a separate pipeline branch under the current pass-pipeline registry rather than reviving the previous dual-path toggle model.
4. Rebind light culling as an explicit dependency of the passes that consume it.
5. Reevaluate decal support independently instead of assuming the old deferred decal contract.

## Known Reference Points

- Forward-only view-mode config now lives in `ViewModePassRegistry.cpp`.
- The renderer no longer allocates deferred intermediate surfaces during pipeline-context creation.
- The project file no longer builds the removed deferred pass sources.

## Recommendation

If hybrid rendering is revisited later, treat this document and git history as reference material, not as an instruction to restore the old architecture wholesale.
