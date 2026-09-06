# Known Issues

## Deferred / Forward Decal Path

Status: Known broken

Current decal behavior is not stable enough to keep iterating inside the shader refactor phase.

Observed issues:
- Deferred rendering path can produce incorrect decal lighting or invalid normal/surface results.
- Forward rendering path may fail to render decals at all.
- Recent attempts to patch the deferred decal path caused broader regressions such as full-screen gray coverage while decals were visible.

Reason for deferral:
- The current decal implementation mixes fullscreen projection, reconstructed world position, legacy surface mutation, and hybrid forward/deferred binding rules.
- This makes the decal path expensive to stabilize during the current 1.5 shader-structure refactor.
- The project may replace the current decal path with a screen-space decal implementation later, which would make further investment in the current path lower value.

Decision:
- Keep decal issues out of scope for the current shader refactor phase.
- Preserve the shader readability / shared-helper refactor work.
- Treat decal support as a separate follow-up task or redesign task.

Recommended next step:
- Revisit decals in a dedicated task.
- Prefer evaluating a screen-space decal implementation over continuing to patch the current volume/fullscreen hybrid path.

## Runtime Vertex Buffer Cache Growth

Status: Partially addressed design, static follow-up required

Static and skeletal runtime vertex contract generalization now creates shader-contract-specific vertex buffers on demand.

Observed issue:
- `UStaticMesh` caches per-shader packed vertex buffers in `RuntimeRenderBufferCache`.
- `USkinnedMeshComponent` caches per-submesh packed vertex buffers in `SkinnedRuntimeRenderBufferCaches`.
- These caches currently grow during a session when multiple custom/debug shaders with different vertex input contracts are applied.
- Old cache entries are released when the owning mesh/component is destroyed or GPU resources are fully released, but there is no pruning during normal editor/runtime use yet.

Impact:
- Repeated material or shader swapping can increase GPU memory usage over time.
- The issue is more noticeable in editor workflows that validate multiple reflected vertex input layouts on the same mesh.

Decision:
- `USkinnedMeshComponent` is a good candidate for exact-live-set pruning after `Resolve Dirty` / material rebuild because its shader-specific runtime buffer cache is component-local.
- `UStaticMesh` still needs separate follow-up work because `RuntimeRenderBufferCache` lives on the shared mesh asset, not on a single component/proxy. A naive exact-live-set prune at one component's dirty-resolution point could delete a layout that another component using the same static mesh asset still needs.

Recommended next step:
- Implement exact-live-set pruning for skeletal runtime buffer caches at the component dirty-resolution point.
- Keep static shader-specific buffer cache growth tracked as a known issue until shared-asset usage ownership or a safer prune policy is introduced.
