> **Languages:** English · [한국어](#한국어)

# Week 7 — Nips Engine

> A project implementing a unified lighting shader, normal mapping, and GPU light culling in a DirectX 11 editor engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/Rocketstein/Nips_W7).

## Project Overview

Building on the Week 6 editor and multipass renderer, we extended the forward-rendering architecture to process many lights efficiently. The project introduced shader permutations for material features and lighting models, then integrated tiled and clustered light culling using a depth prepass and compute shaders.

- **Development period:** 16–22 April 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, compute shaders, Dear ImGui, JSON
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- Up to four perspective or orthographic editor viewports
- Ambient, directional, point, and spot lights
- Gouraud, Lambert, Blinn–Phong, and unlit lighting models
- Shader permutations combining diffuse, normal, and specular maps with cull modes
- OBJ tangent generation and TBN-based normal mapping
- 16×16 tiled light culling using a depth prepass
- Clustered light culling with 24 logarithmic depth slices
- Per-viewport `None`, `Tiled`, and `Clustered` culling-mode selection
- Heatmap view for inspecting light density by tile or cluster
- Decals, height fog, FXAA, selection outlines, and editor overlays
- Scene editing and serialisation, picking, gizmos, and static-mesh/material workflows

## My Contributions

### 1. Unified UberLit Shader and Runtime Lighting-Model Switching

- Extended `ResourceManager` to compile and cache shaders by preprocessor macros and permutation keys
- Consolidated ambient, directional, point, and spot-light calculations in `Lighting.hlsl` and integrated them into `UberLit.hlsl`
- Separated vertex- and pixel-lighting paths for Gouraud, Lambert, and Blinn–Phong models, including per-light attenuation and spotlight-cone calculations
- Connected material binding to selection of the active lighting-model permutation through the Renderer → Render Pass → Material path
- Added runtime lighting-model selection to each viewport's ImGui menu

Representative commits: [`e3138041`](https://github.com/Rocketstein/Nips_W7/commit/e31380418af9e61a277f55a0ca89229e109df477), [`61db3da1`](https://github.com/Rocketstein/Nips_W7/commit/61db3da1b39efd9eb5cde7f2e832b390de50dd40), [`9568e430`](https://github.com/Rocketstein/Nips_W7/commit/9568e4305c25d953addc548e3825975aed74137f), [`5b3628be`](https://github.com/Rocketstein/Nips_W7/commit/5b3628bee712b7965c83d8665fbff7d3d32361c8)

### 2. Tangent Space and the Normal-Mapping Pipeline

- Calculated tangents and bitangents from OBJ triangle position and UV deltas, accumulating them per vertex
- Generated `FNormalVertex` tangents using Gram–Schmidt orthogonalisation and a handedness sign
- Propagated tangent data through vertex input, GPU buffers, and binary serialisation so normal mapping remained available for cached meshes
- Fixed case-sensitivity and option parsing for MTL `map_Bump`, then connected normal textures to material parameters
- Implemented a pixel-shader path that transforms tangent-space normals into world space with a TBN matrix
- Fixed a shader-compilation failure in the Gouraud + Normal Map combination caused by an unused pixel tangent

Representative commits: [`92dc9076`](https://github.com/Rocketstein/Nips_W7/commit/92dc90760b08cfc8ba4439f6fe8fe5b752c2867e), [`fc946f42`](https://github.com/Rocketstein/Nips_W7/commit/fc946f4297efdd746887e390833d277ad3006317), [`44eb564e`](https://github.com/Rocketstein/Nips_W7/commit/44eb564ed49d435c68ca419d6344e3483f55eea6), [`62e62da8`](https://github.com/Rocketstein/Nips_W7/commit/62e62da87395284c86407e6463f7502998d2cca6)

### 3. Depth Prepass and Tiled Light Culling

- Added a `DepthPrePass` before the opaque pass and configured the depth texture for shader-resource access
- Implemented a Direct3D 11 compute-shader wrapper with loading, binding, and unbinding paths
- Divided the screen into 16×16-pixel tiles and used each tile's minimum and maximum depth to reject non-intersecting light spheres
- Wrote selected light indices and per-tile `offset / count` data into UAV Structured Buffers
- Inserted `LightCullingPass` into the rendering pipeline and consumed its output SRVs in the `UberLit` pixel shader
- Fixed SRV/UAV read-write conflicts and buffer-sizing issues between compute and pixel shaders

Representative commits: [`ce555b49`](https://github.com/Rocketstein/Nips_W7/commit/ce555b4962d64f5911f065add31bcdc5bead68e3), [`a6d717ab`](https://github.com/Rocketstein/Nips_W7/commit/a6d717abff3d6d27fdb0d0caac4c2a88a49a6f39), [`c397e7ff`](https://github.com/Rocketstein/Nips_W7/commit/c397e7ffd127f4c88beee904db86f6f5b68467cc), [`18ebfd93`](https://github.com/Rocketstein/Nips_W7/commit/18ebfd937cdb2a9fadae1967a8458aad39c4a6d2), [`7f7e263c`](https://github.com/Rocketstein/Nips_W7/commit/7f7e263cd76bb0f2fce31ac07a356fd0145b0d72), [`9d44f10c`](https://github.com/Rocketstein/Nips_W7/commit/9d44f10c1ab3a7d89ed7ee0b0c2235cc684c7638)

### 4. Clustered Light Culling and Culling-Quality Improvements

- Extended 2D tiles into 3D clusters using 24 logarithmic depth slices
- Added per-viewport `None`, `Tiled`, and `Clustered` modes and selected the matching compute and UberLit permutations
- Added a heatmap view that colours clusters by light count for real-time inspection
- Corrected orthographic-view errors by using different depth-reconstruction paths for perspective and orthographic projections
- Ranked candidate lights beyond the 128-light cluster limit by colour magnitude, intensity, radius, and camera distance
- Reduced light flickering caused by parallel collection order by using deterministic priority selection

Representative commits: [`40d799ed`](https://github.com/Rocketstein/Nips_W7/commit/40d799ed7b6fd05e7679f5de519f0a7ebeb5f03f), [`edfbaf27`](https://github.com/Rocketstein/Nips_W7/commit/edfbaf27a82bd1c5a026bf9857e8c2b7a78fdccf), [`37d13afe`](https://github.com/Rocketstein/Nips_W7/commit/37d13afe27bae8c832ecaad11da91ef72cfb4155)

### 5. Decal Integration and Rendering Stabilisation

- Analysed and fixed incorrect background overwrites caused by simple draw order and alpha handling when multiple decals overlapped
- Uploaded per-decal inverse world matrices, tints, and texture indices through a Structured Buffer
- Combined decal textures in a `Texture2DArray` and performed volume tests and projection in one shader path
- Removed duplicate decal commands and consolidated shared light/decal constants into `UberConstants`
- Fixed constant-buffer packing, SRV register collisions, and wireframe rasteriser-state leakage
- Corrected shader and GPU-resource lifetime issues in `ResourceManager` and fixed wireframe output problems

Representative commits: [`2ddd02a1`](https://github.com/Rocketstein/Nips_W7/commit/2ddd02a110b59a111c4b65e467860e4b79bb9eac), [`d7cfb31b`](https://github.com/Rocketstein/Nips_W7/commit/d7cfb31b2502ae26984219cc613893cb7b75c9cd), [`2b67b4cb`](https://github.com/Rocketstein/Nips_W7/commit/2b67b4cb6ec4040fbb5e888aaed4bfdab49ca5db), [`f434c33a`](https://github.com/Rocketstein/Nips_W7/commit/f434c33a71319e8c06c137fe3918d78181caffa9), [`53252e98`](https://github.com/Rocketstein/Nips_W7/commit/53252e98332c83855e37aaf6872d7758c0924389), [`690240ff`](https://github.com/Rocketstein/Nips_W7/commit/690240ff93deae116c63c2e27c68e312a6b884b7)

## Rendering Architecture

Each editor viewport uses its own `FSceneView` and render-target set. `FEditorRenderPipeline` collects render commands from the world before passing them to `FRenderPipeline`.

```text
World / Components
        │
        ▼
FRenderCollector ── Frustum Culling
        │
        ▼
FRenderBus ── View / Projection / Light / Decal / View Mode
        │
        ▼
Depth Prepass
        │
        ▼
Light Culling Compute Pass ── None / Tiled / Clustered
        │
        ▼
Opaque (UberLit) → Light → Fog → FXAA
        │
        ▼
Font / SubUV / Translucent / Selection / Grid / Gizmo / Outline
```

### Shader Permutations

`FShaderHelper` combines the following features as bit flags and generates the required HLSL preprocessor macros:

- **Lighting model:** Unlit, Gouraud, Lambert, Blinn–Phong, Heatmap
- **Material features:** Diffuse Map, Normal Map, Specular Map, Emissive Map, Alpha Mask
- **Light culling:** None, Tiled, Clustered

Rather than compiling every theoretical combination, the engine creates and caches only valid combinations registered by the renderer at startup.

### Light Culling

- **Tile size:** 16×16 pixels
- **Depth slices:** 24
- **Maximum lights per tile/cluster:** 128
- **Scratch candidates:** 512
- **Default mode:** Clustered

Tiled mode uses each tile's measured minimum and maximum depth. Clustered mode divides the near-to-far range into logarithmic slices. With culling disabled, the shader iterates over all point and spot lights.

## Editor Workflow

1. Place ambient, directional, point, or spot lights in the editor.
2. Adjust colour, intensity, radius, falloff, and cone angle in the Property panel.
3. Compare Gouraud, Lambert, Blinn–Phong, and Unlit through the viewport **View Mode** menu.
4. Switch between Clustered, Tiled, and None under **Light Culling**.
5. Inspect per-tile or per-cluster light density using the **Heatmap** view.
6. Apply a material with a normal map to validate tangent-space lighting.

## Project Structure

```text
.
├─ NipsEngine.sln
├─ NipsEngine/
│  ├─ Asset/                         # Meshes, materials, textures, scenes
│  ├─ Settings/                      # Editor settings
│  ├─ Shaders/
│  │  ├─ Common.hlsl
│  │  ├─ Lighting.hlsl
│  │  ├─ UberLit.hlsl
│  │  ├─ DepthPrepass.hlsl
│  │  ├─ LightCullingCS.hlsl
│  │  └─ Multipass/                  # Lighting, fog, FXAA
│  └─ Source/
│     ├─ Editor/                     # Editor, viewport, UI, render pipeline
│     ├─ Engine/
│     │  ├─ Component/PostProcess/Light/
│     │  ├─ Render/Renderer/RenderFlow/
│     │  ├─ Render/Resource/
│     │  └─ Runtime/
│     └─ Misc/ObjViewer/             # OBJ viewer from an earlier week
├─ Scripts/
└─ GenerateProjectFiles.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143
- Windows 10 SDK
- DirectX 11-capable GPU

### Build Instructions

1. Open `NipsEngine.sln` in Visual Studio.
2. Select `Debug | x64` or `Release | x64`.
3. Restore the NuGet package `directxtk_desktop_win10`.
4. Build and run the solution.

Run the following script if the project files need to be regenerated:

```powershell
.\GenerateProjectFiles.bat
```

The `ObjViewer | x64` configuration remains available, but Week 7's main work is the editor lighting and light-culling path.

## Current Status and Limitations

- Targets Windows and DirectX 11.
- Light-culling tile size, slice count, and maximum light count are currently fixed constants.
- When a cluster has more than 128 candidates, only the highest-priority lights are used.
- The standalone `DecalRenderPass` is disabled in the current `main` pipeline; decal data is supplied through the unified shader path.
- `FEATURE_GUIDE.md` includes Week 6 feature documentation, so Week 7 changes should be verified against this README and the commit history.

## Notes

- The complete collaboration history and team-wide changes are available in [Rocketstein/Nips_W7](https://github.com/Rocketstein/Nips_W7).
- Individual contributions were identified from author metadata, commit messages, and changed files following the Week 7 starting commit on 16 April 2026.
- This document distinguishes inherited editor, decal, height-fog, and FXAA functionality from the new Week 7 lighting and culling work.

---

## 한국어

# Week 7 — Nips Engine

> DirectX 11 기반 편집기 엔진에 통합 라이팅 셰이더, 노멀 매핑, GPU Light Culling을 구현한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/Rocketstein/Nips_W7)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 6에서 이어진 에디터·멀티패스 렌더러를 기반으로, 여러 광원을 효율적으로 처리하는 Forward Rendering 구조를 확장했습니다. 재질 특성과 라이팅 모델에 따른 Shader Permutation을 구성하고, Depth Prepass와 Compute Shader를 이용한 Tiled / Clustered Light Culling을 렌더 파이프라인에 통합했습니다.

- **개발 기간:** 2026.04.16 ~ 2026.04.22
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, Compute Shader, Dear ImGui, JSON
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- 최대 4개의 Perspective / Orthographic 편집 뷰포트
- Ambient / Directional / Point / Spot Light
- Gouraud / Lambert / Blinn-Phong / Unlit 라이팅 모델
- Diffuse / Normal / Specular Map과 Cull Mode를 조합하는 Shader Permutation
- OBJ Tangent 생성과 TBN 기반 Normal Mapping
- Depth Prepass 기반 16×16 Tiled Light Culling
- 24개 로그 깊이 Slice를 사용하는 Clustered Light Culling
- 뷰포트별 `None / Tiled / Clustered` 컬링 모드 전환
- 타일·클러스터별 광원 밀도를 확인하는 Heatmap View
- Decal, Height Fog, FXAA, Selection Outline과 Editor Overlay
- Scene 편집·직렬화, Picking, Gizmo, Static Mesh / Material 워크플로

## 담당 작업

### 1. UberLit 통합 셰이더와 런타임 라이팅 모델 전환

- `ResourceManager`가 전처리 매크로와 Permutation Key별로 셰이더를 컴파일·캐시하도록 로딩 인터페이스 확장
- Ambient, Directional, Point, Spot Light 계산을 공통 `Lighting.hlsl`로 정리하고 `UberLit.hlsl`에 통합
- Gouraud, Lambert, Blinn-Phong의 Vertex / Pixel Lighting 경로를 분리하고 광원 타입별 감쇠와 Spotlight Cone 계산 연결
- Material Bind 시 현재 라이팅 모델의 Permutation을 선택하도록 Renderer → Render Pass → Material 흐름 구성
- 각 뷰포트의 메뉴에서 라이팅 모델을 런타임에 전환하도록 ImGui UI 연동

대표 커밋: [`e3138041`](https://github.com/Rocketstein/Nips_W7/commit/e31380418af9e61a277f55a0ca89229e109df477), [`61db3da1`](https://github.com/Rocketstein/Nips_W7/commit/61db3da1b39efd9eb5cde7f2e832b390de50dd40), [`9568e430`](https://github.com/Rocketstein/Nips_W7/commit/9568e4305c25d953addc548e3825975aed74137f), [`5b3628be`](https://github.com/Rocketstein/Nips_W7/commit/5b3628bee712b7965c83d8665fbff7d3d32361c8)

### 2. Tangent Space와 Normal Mapping 파이프라인

- OBJ Triangle의 위치·UV 변화량으로 Tangent / Bitangent를 계산하고 Vertex별로 누적
- Gram-Schmidt 직교화와 Handedness 부호를 적용해 `FNormalVertex`의 Tangent 생성
- Tangent를 Vertex Input, GPU Buffer, Binary Serializer까지 연결해 캐시된 Mesh에서도 Normal Mapping 유지
- MTL의 `map_Bump` 대소문자와 옵션 처리 문제를 수정하고 Normal Texture를 Material Parameter로 연동
- TBN Matrix로 Tangent-space Normal을 World-space로 변환하는 Pixel Shader 경로 구현
- Gouraud + Normal Map 조합에서 사용되지 않는 Pixel Tangent 때문에 셰이더가 컴파일되지 않던 문제 수정

대표 커밋: [`92dc9076`](https://github.com/Rocketstein/Nips_W7/commit/92dc90760b08cfc8ba4439f6fe8fe5b752c2867e), [`fc946f42`](https://github.com/Rocketstein/Nips_W7/commit/fc946f4297efdd746887e390833d277ad3006317), [`44eb564e`](https://github.com/Rocketstein/Nips_W7/commit/44eb564ed49d435c68ca419d6344e3483f55eea6), [`62e62da8`](https://github.com/Rocketstein/Nips_W7/commit/62e62da87395284c86407e6463f7502998d2cca6)

### 3. Depth Prepass와 Tiled Light Culling

- Opaque Pass 앞에 `DepthPrePass`를 추가하고 Depth Texture를 Shader Resource로 사용할 수 있도록 렌더 타깃 구성
- Direct3D 11 Compute Shader 래퍼와 로딩·바인딩·해제 경로 구현
- 화면을 16×16 Pixel Tile로 나누고 Tile 내부 Depth의 Min / Max를 이용해 광원 Sphere를 선별
- 선택된 광원 인덱스와 Tile별 `offset / count`를 UAV Structured Buffer에 기록
- `LightCullingPass`를 렌더 파이프라인에 배치하고 결과 SRV를 `UberLit` Pixel Shader에서 소비하도록 연결
- Compute / Pixel Shader 사이의 SRV·UAV Read/Write 충돌과 Buffer 크기 문제 수정

대표 커밋: [`ce555b49`](https://github.com/Rocketstein/Nips_W7/commit/ce555b4962d64f5911f065add31bcdc5bead68e3), [`a6d717ab`](https://github.com/Rocketstein/Nips_W7/commit/a6d717abff3d6d27fdb0d0caac4c2a88a49a6f39), [`c397e7ff`](https://github.com/Rocketstein/Nips_W7/commit/c397e7ffd127f4c88beee904db86f6f5b68467cc), [`18ebfd93`](https://github.com/Rocketstein/Nips_W7/commit/18ebfd937cdb2a9fadae1967a8458aad39c4a6d2), [`7f7e263c`](https://github.com/Rocketstein/Nips_W7/commit/7f7e263cd76bb0f2fce31ac07a356fd0145b0d72), [`9d44f10c`](https://github.com/Rocketstein/Nips_W7/commit/9d44f10c1ab3a7d89ed7ee0b0c2235cc684c7638)

### 4. Clustered Light Culling과 컬링 품질 개선

- 2D Tile을 24개의 로그 깊이 Slice로 확장해 3D Cluster 단위로 광원을 분류
- `None / Tiled / Clustered` 모드를 뷰포트별로 선택하고 각 모드에 맞는 Compute / UberLit Permutation 사용
- Cluster의 광원 수를 색으로 표현하는 Heatmap View를 추가해 컬링 결과를 실시간 점검
- Perspective와 Orthographic View가 서로 다른 Depth 복원 방식을 사용하도록 분기해 직교 뷰 오류 수정
- Cluster당 최대 128개를 초과한 후보를 Color 크기·Intensity·Radius·카메라 거리 기반 중요도로 정렬
- 병렬 수집 순서에 따라 선택 광원이 바뀌며 발생하던 Light Flicker를 결정적인 우선순위 선택으로 완화

대표 커밋: [`40d799ed`](https://github.com/Rocketstein/Nips_W7/commit/40d799ed7b6fd05e7679f5de519f0a7ebeb5f03f), [`edfbaf27`](https://github.com/Rocketstein/Nips_W7/commit/edfbaf27a82bd1c5a026bf9857e8c2b7a78fdccf), [`37d13afe`](https://github.com/Rocketstein/Nips_W7/commit/37d13afe27bae8c832ecaad11da91ef72cfb4155)

### 5. Decal 통합과 렌더링 안정화

- 여러 Decal이 겹칠 때 단순 Draw 순서와 Alpha 때문에 배경이 잘못 덮이는 문제 분석·수정
- Decal별 Inverse World Matrix, Tint, Texture Index를 Structured Buffer로 전달
- 서로 다른 Decal Texture를 Texture2DArray로 묶어 한 Shader 경로에서 Volume Test와 투영 수행
- 중복 Decal Command를 걸러내고 Light / Decal 공용 상수 데이터를 `UberConstants`로 정리
- Constant Buffer Packing, SRV Register 충돌, Wireframe Rasterizer 상태 오염 문제 수정
- `ResourceManager`의 Shader·GPU Resource 수명주기와 Wireframe 출력 오류 보정

대표 커밋: [`2ddd02a1`](https://github.com/Rocketstein/Nips_W7/commit/2ddd02a110b59a111c4b65e467860e4b79bb9eac), [`d7cfb31b`](https://github.com/Rocketstein/Nips_W7/commit/d7cfb31b2502ae26984219cc613893cb7b75c9cd), [`2b67b4cb`](https://github.com/Rocketstein/Nips_W7/commit/2b67b4cb6ec4040fbb5e888aaed4bfdab49ca5db), [`f434c33a`](https://github.com/Rocketstein/Nips_W7/commit/f434c33a71319e8c06c137fe3918d78181caffa9), [`53252e98`](https://github.com/Rocketstein/Nips_W7/commit/53252e98332c83855e37aaf6872d7758c0924389), [`690240ff`](https://github.com/Rocketstein/Nips_W7/commit/690240ff93deae116c63c2e27c68e312a6b884b7)

## 렌더링 구조

현재 Editor의 각 뷰포트는 독립적인 `FSceneView`와 Render Target Set을 사용합니다. `FEditorRenderPipeline`이 World의 Render Command를 수집한 뒤 `FRenderPipeline`에 전달합니다.

```text
World / Components
        │
        ▼
FRenderCollector ── Frustum Culling
        │
        ▼
FRenderBus ── View / Projection / Light / Decal / View Mode
        │
        ▼
Depth Prepass
        │
        ▼
Light Culling Compute Pass ── None / Tiled / Clustered
        │
        ▼
Opaque (UberLit) → Light → Fog → FXAA
        │
        ▼
Font / SubUV / Translucent / Selection / Grid / Gizmo / Outline
```

### Shader Permutation

`FShaderHelper`가 다음 요소를 Bit Flag로 조합해 필요한 HLSL 전처리 매크로를 생성합니다.

- **Lighting Model:** Unlit, Gouraud, Lambert, Blinn-Phong, Heatmap
- **Material Feature:** Diffuse Map, Normal Map, Specular Map, Emissive Map, Alpha Mask
- **Light Culling:** None, Tiled, Clustered

모든 조합을 무조건 컴파일하지 않고 Renderer가 등록한 유효 조합만 시작 시 생성·캐시합니다.

### Light Culling

- **Tile Size:** 16×16 pixels
- **Depth Slices:** 24
- **Maximum Lights per Tile / Cluster:** 128
- **Scratch Candidates:** 512
- **기본 모드:** Clustered

Tiled 모드는 Tile의 실제 Min / Max Depth를 사용하고, Clustered 모드는 Near / Far 구간을 로그 스케일 Slice로 분할합니다. 컬링을 끄면 전체 Point / Spot Light를 순회합니다.

## 에디터 사용 흐름

1. Editor에서 Ambient, Directional, Point 또는 Spot Light를 배치합니다.
2. Property 패널에서 Color, Intensity, Radius, Falloff, Cone Angle을 조정합니다.
3. 뷰포트 메뉴의 **View Mode**에서 Gouraud / Lambert / Blinn-Phong / Unlit을 비교합니다.
4. **Light Culling**에서 Clustered / Tiled / None을 전환합니다.
5. **Heatmap** View로 타일·클러스터별 광원 밀도를 확인합니다.
6. Normal Map이 연결된 Material을 적용해 Tangent-space 조명 결과를 검증합니다.

## 프로젝트 구조

```text
.
├─ NipsEngine.sln
├─ NipsEngine/
│  ├─ Asset/                         # Mesh, Material, Texture, Scene
│  ├─ Settings/                      # Editor 설정
│  ├─ Shaders/
│  │  ├─ Common.hlsl
│  │  ├─ Lighting.hlsl
│  │  ├─ UberLit.hlsl
│  │  ├─ DepthPrepass.hlsl
│  │  ├─ LightCullingCS.hlsl
│  │  └─ Multipass/                  # Light, Fog, FXAA
│  └─ Source/
│     ├─ Editor/                     # Editor, Viewport, UI, Render Pipeline
│     ├─ Engine/
│     │  ├─ Component/PostProcess/Light/
│     │  ├─ Render/Renderer/RenderFlow/
│     │  ├─ Render/Resource/
│     │  └─ Runtime/
│     └─ Misc/ObjViewer/             # 이전 주차 OBJ Viewer
├─ Scripts/
└─ GenerateProjectFiles.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143
- Windows 10 SDK
- DirectX 11 지원 GPU

### 빌드

1. `NipsEngine.sln`을 Visual Studio에서 엽니다.
2. `Debug | x64` 또는 `Release | x64`를 선택합니다.
3. NuGet의 `directxtk_desktop_win10` 패키지를 복원합니다.
4. 솔루션을 빌드하고 실행합니다.

프로젝트 파일을 다시 생성해야 하면 다음 스크립트를 실행합니다.

```powershell
.\GenerateProjectFiles.bat
```

`ObjViewer | x64` 구성도 남아 있지만, Week 7의 핵심 작업은 Editor의 라이팅·Light Culling 렌더 경로입니다.

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- Light Culling의 Tile Size, Slice 수, 최대 광원 수는 현재 상수로 고정되어 있습니다.
- Cluster당 후보가 128개를 넘으면 중요도 기준 상위 광원만 사용합니다.
- 현재 `main`의 독립 `DecalRenderPass`는 Pipeline 목록에서 비활성화되어 있으며, Decal 데이터는 통합 Shader 경로로 전달됩니다.
- 프로젝트의 `FEATURE_GUIDE.md`는 Week 6 기능 설명을 포함하므로 Week 7 변경사항은 이 README와 커밋 기록을 기준으로 확인해야 합니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [Rocketstein/Nips_W7](https://github.com/Rocketstein/Nips_W7)에서 확인할 수 있습니다.
- 개인 기여 내역은 `2026-04-16`의 Week 7 시작 커밋 이후 작성자 정보, 커밋 메시지와 실제 변경 파일을 함께 확인해 정리했습니다.
- 이전 주차에서 이어진 Editor, Decal, Height Fog, FXAA 등의 기반 기능과 Week 7의 신규 라이팅·컬링 작업을 구분해 기술했습니다.