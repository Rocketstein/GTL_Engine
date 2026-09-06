# Render Pipeline Construction

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.0 |

> Companion to `render_overview.md` and `render_execution.md`.
> 이 문서는 현재 renderer가 어떤 registry와 resource owner를 초기화해서 tree를 구성하는지에 초점을 둔다.

## 1. Build systems

렌더러는 세 가지 build system으로 나눠 보면 가장 이해하기 쉽다.

| System | Source | Built result |
|---|---|---|
| Shader reflection | HLSL files | `FGraphicsProgram` / `FShaderVariantCache` 결과 |
| Pipeline tree | C++ registry tables | `FRenderPipelineDesc` / `FRenderPass` tree |
| Runtime resources | D3D11 resource owners | constant buffer, sampler, SRV, RTV/DSV, batch, cache |

이 셋은 서로 독립적으로 준비되지만, 실행 시점에는 `FRenderPipelineContext`와 `FDrawCommand`에서 만난다.

## 2. Initialization order

`FRenderer::Create` 기준 초기화 순서는 다음과 같다.

1. `FD3DDevice` 생성
2. `FShaderManager::Initialize`
3. `FConstantBufferCache::Initialize`
4. `FFrameResources::Create`
5. `FRenderPassRegistry::Initialize`
6. `FRenderPipelineRegistry::Initialize`
7. `FViewModePassRegistry::Initialize`
8. overlay batch resource 생성
9. `FGPUProfiler::Initialize`

이 순서가 중요한 이유는 registry가 resource owner를 참조하고, pass 구현이 frame resource와 shader cache를 가정하기 때문이다.

## 3. Registry wiring

### `FRenderPassRegistry`

- concrete pass instance를 보관한다.
- `InitializeDefaultRenderPassPresets`로 logical pass state를 채운다.
- `FindPass`는 enum key를 concrete pass 객체로 바꿔 준다.

### `FRenderPipelineRegistry`

- root, scene, forward, post-process, overlay, outline tree를 보관한다.
- child는 `PipelineNode` 또는 `PassNode`로 등록된다.
- runner는 이 tree를 재귀적으로 순회한다.

### `FViewModePassRegistry`

- 현재 view mode에 어떤 pass family가 살아 있는지 정의한다.
- opaque pass shader variant와 일부 fullscreen variant를 관리한다.
- hot reload가 켜져 있으면 variant cache를 갱신한다.

## 4. Runtime resource owners

| Owner | Responsibility |
|---|---|
| `FFrameResources` | frame-scoped CB, SRV, samplers, text/UI batch, per-object CB pool |
| `FViewportRenderTargets` | viewport-local RTV/DSV, depth/color copies, stencil copy |
| `FShaderManager` | built-in and custom shader cache |
| `FConstantBufferCache` | small per-pass constant buffer reuse |
| `FViewModePassRegistry` | view-mode shader variant cache |
| `FRenderPassRegistry` | pass instances and pass presets |
| `FRenderPipelineRegistry` | pipeline tree |

## 5. Important construction facts

- There is no deferred intermediate surface owner in the current mainline renderer.
- `PresentPass` is not wired into the tree and is executed separately after the root pipeline.
- `GridPass` is still a declared node type, but it is not part of the active registry wiring.
- `AdditiveDecalPass` is registered, but the current tree does not include it.

## 6. What to read next

If you need the actual runtime behavior after the tree is built, continue with:

1. `render_execution.md`
2. `render_pipeline_reference.md`
3. `render_pass_reference.md`
4. `render_resource.md`
