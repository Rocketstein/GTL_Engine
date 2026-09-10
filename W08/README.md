<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 8 — Krafton Engine: Shadow Mapping

> A project that adds directional cascaded shadow maps and spot/point-light shadow atlases to a custom DirectX 11 engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/shimwoojin/Jungle_Week8_Team2).

## Project Overview

Building on the Week 7 lighting and light-culling renderer, we extended the shadow-rendering pipeline so directional, spot, and point lights could cast dynamic shadows from scene geometry. Directional lights use a four-cascade Cascaded Shadow Map, while multiple spot and point lights share shadow atlases whose resolution is allocated according to estimated screen contribution.

The final team project supports hard shadows, PCF and VSM filtering, multiple atlas pages, shadow-caster frustum culling, and shadow-map debug views.

- **Development period:** 23–29 April 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, shadow mapping, CSM, PCF, VSM, Dear ImGui
- **Areas of responsibility:** Spot/point shadow atlases, adaptive resolution and batch allocation, shadow-quality and resource stabilisation, InterpToMovement integration
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- Four-cascade shadow maps with cascade blending for directional lights
- A 2D shadow atlas for spot lights and cube-face atlas regions for point lights
- Screen-contribution-based shadow-resolution estimation and power-of-two tile allocation
- Multiple atlas pages according to capacity, with resolution reduction when the page limit is exceeded
- Hard, PCF, and VSM shadow filters with separable Gaussian blur
- Per-light bias, slope bias, normal bias, sharpening, and resolution-scale controls
- Shadow-caster frustum culling and rendering statistics
- Debug widgets for inspecting CSM, spot and point shadow maps, atlas pages, and allocated regions
- `UInterpToMovementComponent` for movement along control points
- Scene editing and serialisation, picking, gizmos, and multipass rendering

## My Contributions

### 1. Buddy-Allocation Shadow-Atlas Quadtree

- Implemented `FShadowAtlasQuadTree`, using a buddy-style scheme that treats the full atlas as the root node and recursively subdivides it into four until the requested size is reached
- Stored each node's position, resolution, occupied/split state, and child indices, then recursively searched for the best-fitting tile
- Returned allocations as pixel coordinates and dimensions in `FAtlasRegion` for use by viewports and shader UV transforms
- Corrected failed allocations for non-power-of-two requests and added a minimum-resolution bound
- Changed `RemainingSpace` tracking from linear extent to actual area

### 2. Screen-Contribution-Based Adaptive Resolution and Batch Allocation

- Estimated each light's projected screen area from camera position, direction, FOV, and light radius
- Calculated requested resolution from projected area, colour luminance, intensity, and `ShadowResolutionScale`
- Normalised requests to the nearest power of two and clamped them to the atlas's minimum and maximum sizes
- Batched light requests and allocated larger tiles first, reducing fragmentation caused by servicing small requests before large ones
- Replaced immediate per-light region creation with an `AddToBatch → CommitBatch` flow

### 3. Spot-Light Shadow-Atlas Rendering Path

- Connected spot-light view/projection matrices, atlas viewports, and depth rendering to `ShadowMapPass`
- Used `FSpotLightParams` as the atlas-evaluation input and applied per-light resolution scales
- Fixed batch-to-region mapping errors that prevented several spot lights from casting shadows simultaneously
- Bound the spot-atlas texture and shadow data to shader-resource slots and added atlas-region debug overlays
- Switched the shadow-depth pass to front-face culling and resolved resource hazards in the VSM path

### 4. Point-Light Cube-Face Atlas

- Extracted the shared node-allocation logic into `FAtlasQuadTreeBase` and extended it with dedicated spot- and point-light evaluation classes
- Allocated the six +X, -X, +Y, -Y, +Z, and -Z views of each point light as independent atlas regions
- Recorded the light index and cube-face orientation in `FAtlasRegion` and connected them to GPU shadow data
- Replaced the existing `Texture2DArray` point-shadow path with atlas allocation and screen-contribution-based resolution evaluation
- Corrected point-atlas region-frame display, the resolution-evaluation formula, and `ShadowResolutionScale` behaviour

### 5. Shadow-Quality Corrections and Engine Integration

- Sent separate normal-bias values for directional, spot, and point lights through the constant-buffer path
- Fixed cases where normal bias was not updated before GPU upload or referenced another light's constants
- Expanded editor ranges so negative bias values could be configured in light properties
- Resolved a Direct3D 11 read/write hazard caused by binding the previous frame's shadow SRV while using the same resource as a depth target
- Ported `UInterpToMovementComponent` from the previous week into the evolved engine architecture and reconnected its property UI, `Vec3Array` serialisation, and demo-scene usage
- Prepared `w8demo.Scene` to demonstrate and test spot/point atlases and InterpToMovement

## Shadow-Rendering Architecture

```text
SceneEnvironment
 Directional / Spot / Point Lights
              │
              ▼
   Shadow Light Frustum Culling
              │
      ┌───────┴────────┐
      ▼                ▼
Directional CSM   Spot / Point Lights
 4 Cascades       Screen-Contribution Evaluation
      │                │
      │         Page Grouping
      │                │
      │         Quadtree Batch Allocation
      │          Spot: 1 face / Point: 6 faces
      └───────┬────────┘
              ▼
      Shadow Depth Pass
     Front Cull + Caster Cull
              │
       ┌──────┴──────┐
       ▼             ▼
 Hard / PCF       VSM Moments
 Depth SRV       Gaussian Blur
       └──────┬──────┘
              ▼
   Forward Lighting Shader
```

For each visible shadow-casting light, the spot/point atlas estimates the required resolution every frame. The final architecture groups lights into pages according to an atlas-area budget. If the page limit would be exceeded, it proportionally reduces requested resolutions, then places the largest tiles first in each page's quadtree.

## Shadow Settings and Debugging

The default shadow settings in `KraftonEngine/Settings/ProjectSettings.ini` are:

| Setting | Default | Description |
| --- | ---: | --- |
| `CSMResolution` | 2048 | Resolution of each directional-light CSM cascade |
| `SpotAtlasResolution` | 4096 | Size of one spot-atlas page |
| `PointAtlasResolution` | 4096 | Size of one point-atlas page |
| `MaxSpotAtlasPages` | 4 | Maximum number of spot-atlas pages |
| `MaxPointAtlasPages` | 4 | Maximum number of point-atlas pages |
| `bShadows` | `true` | Enables or disables shadows globally |

The editor's **Shadow Map Debug** window can display:

- CSM cascades C0–C3 and their near/far ranges
- Per-page depth for spot and point atlases
- Allocated light indices, region boundaries, and tile resolutions
- The selected spot light's region or all six cube faces of a point light
- Linear and power visualisations with adjustable brightness

## Project Structure

```text
.
├─ KraftonEngine.sln
├─ KraftonEngine/
│  ├─ Asset/Scene/                    # Test and demo scenes
│  ├─ Settings/                       # Editor and project settings
│  ├─ Shaders/
│  │  ├─ Common/ShadowSampling.hlsli
│  │  ├─ Lighting/ShadowDepth.hlsl
│  │  └─ PostProcess/ShadowMapVis.hlsl
│  └─ Source/
│     ├─ Editor/UI/                   # Shadow-map debug and project settings
│     └─ Engine/
│        ├─ Component/Light/          # Directional, spot, and point settings
│        ├─ Component/Movement/       # InterpToMovement
│        ├─ Profiling/                # Shadow statistics
│        └─ Render/
│           ├─ RenderPass/ShadowMapPass.*
│           ├─ Resource/              # Shadow GPU resources
│           └─ Shadow/                # Atlas-quadtree implementation
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ DemoBuild.bat
└─ ReleaseBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore

### Build Instructions

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `KraftonEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build and run `Debug | x64` or `Release | x64`.

Use `DemoBuild.bat` or `ReleaseBuild.bat` to prepare deployable executables and resources.

## Current Status and Limitations

- Targets Windows and DirectX 11.
- The default atlas size is 4096×4096 per page, with up to four pages each for spot and point lights.
- A point light consumes six cube-face regions, so it uses more atlas area than a spot light at the same resolution.
- The atlas is reset and greedily repacked from largest request to smallest every frame; it does not use persistent allocation.
- When the page limit is exceeded, all requested resolutions are reduced, so individual shadow quality can fall in scenes with many lights.
- The final integration of directional CSM/VSM resources, selected-light debugging, and multiple atlas pages also includes work by other team members.

## Notes

- The complete collaboration history and team-wide changes are available in [shimwoojin/Jungle_Week8_Team2](https://github.com/shimwoojin/Jungle_Week8_Team2).
- The previous private-repository snapshot differed from the original `main` in only `KraftonEngine/Settings/Editor.ini` among 648 source blobs; neither repository had a root README.
- The 57 commits attributed to Rocketstein during Week 8 include merges, checkpoints, and squashed team changes. Contributions were therefore identified by reviewing commit messages, changed files, and the final code together.
- Directional CSM and the initial point-shadow implementation were written by other team members; they are listed under project features but not claimed as Rocketstein's individual contributions.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 8 — Krafton Engine: Shadow Mapping

> DirectX 11 기반 커스텀 엔진에 Directional CSM과 Spot / Point Light Shadow Atlas를 구축한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/shimwoojin/Jungle_Week8_Team2)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 7의 라이팅·Light Culling 렌더러를 기반으로, Directional / Spot / Point Light가 실제 Scene Geometry에 동적 그림자를 투영하도록 Shadow Rendering Pipeline을 확장했습니다. Directional Light에는 4단계 Cascaded Shadow Map을, 다수의 Spot / Point Light에는 화면 기여도에 따라 해상도를 배분하는 Shadow Atlas를 적용했습니다.

최종 팀 결과물은 Hard Shadow, PCF, VSM 필터와 다중 Atlas Page, Shadow Caster Frustum Culling, Shadow Map Debug View를 지원합니다. 

- **개발 기간:** 2026.04.23 ~ 2026.04.29
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, Shadow Mapping, CSM, PCF, VSM, Dear ImGui
- **담당 영역:** Spot / Point Shadow Atlas, 적응형 해상도·Batch Allocation, Shadow 품질·리소스 안정화, InterpToMovement 통합
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- Directional Light용 4-Cascade Shadow Map과 Cascade Blend
- Spot Light용 2D Shadow Atlas와 Point Light의 Cube Face Atlas
- 화면 기여도 기반 Shadow Resolution 평가와 Power-of-Two Tile 할당
- Atlas 용량에 따른 다중 Page 구성과 최대 Page 초과 시 해상도 축소
- Hard / PCF / VSM Shadow Filter와 Separable Gaussian Blur
- Light별 Bias, Slope Bias, Normal Bias, Sharpen, Resolution Scale 설정
- Shadow Caster Frustum Culling과 렌더링 통계
- CSM / Spot / Point Shadow Map, Atlas Page와 할당 Region을 확인하는 Debug Widget
- Control Point를 따라 이동하는 `UInterpToMovementComponent`
- Scene 편집·직렬화, Picking, Gizmo와 멀티패스 렌더링

## 담당 작업

### 1. Buddy Allocation 기반 Shadow Atlas QuadTree

- Atlas 전체를 Root Node로 두고 요청 크기까지 4분할하는 Buddy 방식의 `FShadowAtlasQuadTree` 구현
- 각 Node에 위치, 해상도, 점유·분할 상태와 자식 인덱스를 저장하고 가장 적합한 Tile을 재귀적으로 탐색
- 할당 결과를 `FAtlasRegion`의 Pixel 좌표와 크기로 반환해 Viewport 및 Shader UV 변환에 사용할 수 있도록 구성
- Power-of-Two가 아닌 요청으로 유효한 영역을 찾지 못하던 문제를 보정하고 최소 해상도 Bound 추가
- 선형 길이가 아닌 면적을 기준으로 `RemainingSpace`를 추적하도록 수정

### 2. 화면 기여도 기반 적응형 해상도와 Batch Allocation

- 카메라 위치·방향·FOV와 Light의 Radius를 이용해 화면에 투영되는 면적을 추정
- 투영 면적에 Color Luminance, Intensity, `ShadowResolutionScale`을 반영해 Light별 요청 해상도 산출
- 요청 해상도를 가장 가까운 Power-of-Two로 정규화하고 Atlas 최소·최대 범위로 제한
- Light 요청을 Batch에 모아 큰 Tile부터 할당해 작은 요청이 공간을 먼저 파편화하는 현상 완화
- 개별 `Add`마다 Region을 즉시 생성하던 흐름을 `AddToBatch → CommitBatch`로 변경

### 3. Spot Light Shadow Atlas 렌더 경로

- `ShadowMapPass`에 Spot Light의 View / Projection, Atlas Viewport와 Depth Rendering 경로 연결
- `FSpotLightParams`를 Atlas 평가 입력으로 사용하고 Light별 Resolution Scale 적용
- 여러 Spot Light가 동시에 Shadow를 투영하지 못하던 Batch·Region 대응 오류 수정
- Spot Atlas Texture와 Shadow Data를 Shader Resource Slot에 연결하고 Atlas Region Debug Overlay 추가
- Shadow Depth Pass에서 Front-face Culling을 사용하도록 변경하고 VSM 경로의 리소스 Hazard 수정

### 4. Point Light Cube Face Atlas

- 공통 Node 할당 로직을 `FAtlasQuadTreeBase`로 분리하고 Spot / Point 전용 평가 클래스로 확장
- Point Light 한 개의 +X, -X, +Y, -Y, +Z, -Z 여섯 방향을 독립 Atlas Region으로 할당
- `FAtlasRegion`에 Light Index와 Cube Face Orientation을 기록해 GPU Shadow Data와 연결
- 기존 `Texture2DArray` 기반 Point Shadow를 Atlas 방식으로 전환하고 화면 기여도 기반 해상도 평가 적용
- Point Atlas의 Region Frame 표시, 해상도 평가식과 `ShadowResolutionScale` 동작 보정

### 5. Shadow 품질 보정과 엔진 통합

- Directional / Spot / Point Light의 Normal Bias 값을 별도 Constant Buffer 경로로 전달
- Normal Bias가 GPU Upload 전에 갱신되지 않거나 다른 Light의 상수를 참조하던 문제 수정
- Light Property에서 음수 Bias를 설정할 수 있도록 편집 범위를 보정
- 이전 Frame의 Shadow SRV가 Depth Target과 동시에 바인딩되어 발생하는 Direct3D 11 Read / Write Hazard 해결
- 이전 주차의 `UInterpToMovementComponent`를 발전된 엔진 구조로 이식하고 Property UI, `Vec3Array` 직렬화와 Demo Scene에 재연결
- Spotlight / Point Atlas와 InterpToMovement를 점검할 수 있는 테스트·발표용 `w8demo.Scene` 구성

## Shadow 렌더링 구조

```text
SceneEnvironment
 Directional / Spot / Point Lights
              │
              ▼
   Shadow Light Frustum Culling
              │
      ┌───────┴────────┐
      ▼                ▼
Directional CSM   Spot / Point Lights
 4 Cascades       화면 기여도 평가
      │                │
      │         Page Grouping
      │                │
      │         QuadTree Batch Allocation
      │          Spot 1면 / Point 6면
      └───────┬────────┘
              ▼
      Shadow Depth Pass
     Front Cull + Caster Cull
              │
       ┌──────┴──────┐
       ▼             ▼
 Hard / PCF       VSM Moments
 Depth SRV       Gaussian Blur
       └──────┬──────┘
              ▼
   Forward Lighting Shader
```

Spot / Point Atlas는 각 Frame에 보이는 Shadow Light의 예상 해상도를 계산합니다. 최종 구조에서는 Atlas 면적 예산으로 Light를 Page에 나누고, Page 수 제한을 넘으면 해상도를 비례 축소한 뒤 각 Page에서 큰 Tile부터 QuadTree에 배치합니다.

## Shadow 설정과 디버깅

`KraftonEngine/Settings/ProjectSettings.ini`의 기본 Shadow 설정은 다음과 같습니다.

| 설정 | 기본값 | 설명 |
| --- | ---: | --- |
| `CSMResolution` | 2048 | Directional CSM의 Cascade 해상도 |
| `SpotAtlasResolution` | 4096 | Spot Atlas 한 Page의 크기 |
| `PointAtlasResolution` | 4096 | Point Atlas 한 Page의 크기 |
| `MaxSpotAtlasPages` | 4 | Spot Atlas 최대 Page 수 |
| `MaxPointAtlasPages` | 4 | Point Atlas 최대 Page 수 |
| `bShadows` | `true` | 전체 Shadow 활성화 여부 |

에디터의 **Shadow Map Debug** 창에서는 다음 항목을 확인할 수 있습니다.

- CSM의 C0 ~ C3 Cascade와 Near / Far 범위
- Spot / Point Atlas의 Page별 Depth
- Atlas에 할당된 Light Index, Region 경계와 Tile 해상도
- 선택한 Spot Light의 개별 Region과 Point Light의 6개 Cube Face
- Linear / Power 시각화와 Brightness 조정

## 프로젝트 구조

```text
.
├─ KraftonEngine.sln
├─ KraftonEngine/
│  ├─ Asset/Scene/                    # 테스트·Demo Scene
│  ├─ Settings/                       # Editor / Project 설정
│  ├─ Shaders/
│  │  ├─ Common/ShadowSampling.hlsli
│  │  ├─ Lighting/ShadowDepth.hlsl
│  │  └─ PostProcess/ShadowMapVis.hlsl
│  └─ Source/
│     ├─ Editor/UI/                   # Shadow Map Debug, Project Settings
│     └─ Engine/
│        ├─ Component/Light/          # Directional / Spot / Point 설정
│        ├─ Component/Movement/       # InterpToMovement
│        ├─ Profiling/                # Shadow Stats
│        └─ Render/
│           ├─ RenderPass/ShadowMapPass.*
│           ├─ Resource/              # Shadow GPU Resource
│           └─ Shadow/                # Atlas QuadTree 구현
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ DemoBuild.bat
└─ ReleaseBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore

### 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio 프로젝트 파일을 생성합니다.
2. `KraftonEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

배포용 실행 파일과 리소스는 `DemoBuild.bat` 또는 `ReleaseBuild.bat`으로 구성할 수 있습니다.

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- 기본 Atlas는 Page당 4096×4096, Spot / Point 각각 최대 4 Page로 설정되어 있습니다.
- Point Light 한 개는 여섯 Cube Face Region을 사용하므로 동일 해상도의 Spot Light보다 Atlas 면적을 더 많이 소비합니다.
- Atlas는 매 Frame Reset 후 큰 요청부터 Greedy하게 다시 할당하므로 Persistent Allocation 방식은 아닙니다.
- Page 제한을 초과하면 전체 요청 해상도를 축소하므로 Light가 매우 많을 때 개별 Shadow 품질이 낮아질 수 있습니다.
- Directional CSM과 VSM Resource 구조, 선택 Light 단위 Debug 확장과 다중 Page 최종 통합에는 팀원의 작업도 포함되어 있습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [shimwoojin/Jungle_Week8_Team2](https://github.com/shimwoojin/Jungle_Week8_Team2)에서 확인할 수 있습니다.
- 비공개 저장소의 기존 스냅샷은 원본 `main`과 648개 Source Blob 중 `KraftonEngine/Settings/Editor.ini` 한 파일만 달랐으며, 루트 README는 없었습니다.
- Week 8 기간 중 Rocketstein 작성자 정보로 기록된 57개 커밋에는 Merge·Checkpoint와 팀 변경을 합친 Squash Commit이 포함되어 있어, 담당 작업은 커밋 메시지뿐 아니라 실제 변경 파일과 최종 코드를 함께 확인해 정리했습니다.
- Directional CSM과 초기 Point Shadow 등 팀원이 작성한 기능은 프로젝트의 **주요 기능**에는 포함하되 Rocketstein의 개인 담당으로 기재하지 않았습니다.
