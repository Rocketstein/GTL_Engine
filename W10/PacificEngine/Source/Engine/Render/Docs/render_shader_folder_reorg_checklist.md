# Shader Folder Reorganization Checklist

## Goal
- Remove `Forward` naming from the opaque shader entrypoint path.
- Collapse include-only shader headers into fewer, clearer meaning units.
- Keep include paths, `.vcxproj`, and `.filters` in sync with the physical file layout.

## Decisions
- Rename `Shaders/Passes/Scene/Forward/ForwardOpaquePass.hlsl` to `Shaders/Passes/Scene/Opaque/OpaquePass.hlsl`.
- Move `Shaders/Render/Scene/Shared/OpaquePassTypes.hlsli` to `Shaders/Render/Scene/Opaque/OpaquePassTypes.hlsli`.
- Move `Shaders/Common/VertexLayouts.hlsl` to `Shaders/Resources/VertexLayouts.hlsl` so `Common` can disappear.
- Keep runtime C++ pass type names unchanged for now unless the build work shows a strong need to rename them too.

## Checklist
- [ ] Move the shader files to the new folders.
- [ ] Update every `#include` that references the moved files.
- [ ] Update shader registration paths in C++.
- [ ] Update `.vcxproj` and `.vcxproj.filters` entries.
- [ ] Update active render docs that mention the moved shader paths.
- [ ] Build and fix any broken include paths or stale references.

## Validation
- The solution should build without shader include errors.
- Forward opaque rendering should still compile and render the same images.
- The new folder layout should keep include-only files grouped by purpose instead of by legacy implementation history.
