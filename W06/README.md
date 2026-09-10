<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 6 — Nips Engine

> A project extending a DirectX 11 multipass renderer and editor/PIE input architecture with dynamic lighting and path-following movement components.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/jskim-research/Jungle_Week6_Team4).

## Project Overview

Building on the editor and rendering engine developed in previous weeks, we extended the architecture to support playable sessions and more complex rendering effects. Week 6 separated Editor World and PIE (Play In Editor) input, introduced a G-buffer-based light pass and fireball lights, and integrated control-point movement and camera-pursuit components with the editor and serialisation workflow.

At team level, the project also integrated decals, height fog, FXAA, dynamic BVH and frustum culling, LOD, material instances, and a PIE world.

- **Development period:** 9–15 April 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, Dear ImGui, JSON
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- Separate input and camera control for Editor World and PIE states
- Multipass rendering using colour, normal, depth, and world-position G-buffers
- Multiple point lights evaluated through a Structured Buffer
- Fireball Actor combining an emissive mesh with a dynamic light
- Decal volume projection with fade-in and fade-out
- Height-fog post-processing with up to eight layers
- FXAA, selection outlines, and Lit/Unlit/Wireframe view modes
- Control-point-based `InterpToMovementComponent`
- Camera-following `PursuitMovementComponent`
- Dynamic-BVH-based picking and frustum culling, with static-mesh LOD
- ImGui property panels and JSON scene save/restore
- Up to four perspective or orthographic editor viewports

## My Contributions

### 1. Editor / PIE Input-Controller Architecture

- Split input handling into the `IBaseEditorController` interface and `FEditorInputRouter`
- Assigned edit-mode selection, gizmo, and camera input to `FEditorWorldController`, and play-mode input to `FPIEController`
- Connected active controller, world, and camera-target switching to PIE start and stop events
- Routed keyboard press/hold/release and mouse movement, click, drag, and wheel events through a shared input router
- Fixed incorrect yaw/pitch jumps when entering PIE and residual camera impulses after controller switches
- Left the cursor unconfined in the editor except during right-mouse dragging, while allowing PIE sessions to exit with `Esc`
- Extracted ray–AABB, ray–triangle, and component raycasts into the reusable `FRayCollision` utility

### 2. Fireball and Multipass Dynamic Lighting

- Added `AFireballActor` and `UFireballComponent`, exposing editable radius, falloff, intensity, and colour properties
- Collected fireball data through `RenderCollector`, converted it into an `FLightData` array, and uploaded it through a GPU Structured Buffer
- Implemented a fullscreen light pass that calculates light direction, distance attenuation, and the Lambert term from G-buffer world position and normals
- Accumulated contributions from multiple lights and added global ambient lighting to the final colour path
- Added emissive colour to static-mesh constant buffers and allowed emissive surfaces to bypass the light pass
- Connected Lit, Unlit, and Wireframe modes to global-lighting behaviour in the light pass
- Fixed HLSL/C++ constant-buffer alignment, GPU padding, SRV unbinding, and default-culling issues

### 3. Control-Point-Based InterpTo Movement

- Implemented `UInterpToMovementComponent` to move along multiple `FVector` control points
- Calculated each segment's interpolation duration from its proportion of total path length, reducing speed variation
- Added four playback modes: `OneShot`, `OneShotReverse`, `Loop`, and `PingPong`
- Calculated movement-facing rotation with quaternions and applied `Slerp` for smooth orientation changes along the path
- Added editor UI for adding, deleting, and editing control points, together with Initiate, Stop, and Reset operations
- Added `Vec3Array` and `Enum` property types and connected them to duplication and JSON save/restore
- Allowed control points to be edited in Actor-local space before converting them to world space when PIE begins

### 4. Pursuit Movement and Camera Tracking

- Implemented `UPursuitMovementComponent`, which periodically updates a target position and interpolates towards it
- Exposed detection radius, pursuit speed, update interval, and target-facing rotation through the property panel
- Calculated target yaw and pitch and smoothly interpolated orientation during movement
- Configured the component to track the primary perspective `FViewportCamera` automatically when no explicit target exists at PIE start
- Registered the component with the component factory and copied configuration while resetting runtime state during duplication

### 5. Property, Serialisation, and Integration Stabilisation

- Added `EPropertyType::Color` and connected ImGui `ColorEdit4` to fireball-property editing
- Serialised and restored colour values in JSON and preserved colour when duplicating fireballs
- Corrected C++/HLSL data layouts in the light pass and fixed static-mesh emissive-texture propagation
- Prepared fireball meshes/materials and a basic spotlight placement demo, and connected them to the editor spawn list
- Fixed integration issues discovered during demonstrations, including PIE camera flight, InterpTo local points, and fireball culling

## Engine Execution Flow

```text
Windows Input
      │
      ▼
 InputSystem
      │
      ▼
FEditorInputRouter
      ├──────── Editor World ────────┐
      │    Camera / Picking / Gizmo  │
      └──────── PIE Controller ──────┤
             Play Camera / Exit      │
                                     ▼
                         World / Actor / Component
                              │             │
                    Movement Components    │
                 InterpTo / Pursuit        │
                              │             ▼
                              └──── RenderCollector
                                            │
                                            ▼
                                         RenderBus
                                            │
        G-Buffer Opaque ──► Light Pass ──► Fog / FXAA
             Colour · Normal · Depth · World Position
```

### Fireball-Light Path

```text
UFireballComponent
Radius · Falloff · Intensity · Colour
                 │
                 ▼
          FLightData Array
                 │
                 ▼
     GPU Structured Buffer
                 │
                 ▼
Fullscreen LightPass.hlsl
G-buffer reconstruction → distance attenuation → N·L → light accumulation
```

## Project Structure

```text
.
├─ FEATURE_GUIDE.md
├─ NipsEngine.sln
├─ NipsEngine/
│  ├─ Asset/                         # Meshes, materials, textures, scenes
│  ├─ Settings/                      # Editor settings
│  ├─ Shaders/
│  │  ├─ Multipass/                  # Lighting, fog, FXAA
│  │  ├─ ShaderDecal.hlsl
│  │  └─ Selection / Outline Shader
│  └─ Source/
│     ├─ Editor/                     # Editor engine, viewport, property UI
│     └─ Engine/
│        ├─ Component/Movement/      # InterpTo, Pursuit, Projectile, Rotating
│        ├─ Input/Controller/        # Editor / PIE input controllers
│        ├─ Render/                  # Collector, bus, render passes, resources
│        ├─ Spatial/                 # Dynamic BVH and world spatial index
│        └─ Serialization/           # JSON scene save/restore
├─ Scripts/GenerateProjectFiles.py
├─ GenerateProjectFiles.bat
└─ ReleaseBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore

The project uses `directxtk_desktop_win10` through NuGet and targets C++20.

### Build Instructions

1. Run `GenerateProjectFiles.bat` after adding or moving files if the project must be regenerated.
2. Open `NipsEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build and run `Debug | x64` or `Release | x64`.

`ReleaseBuild.bat` builds the `Release | x64` configuration through MSBuild. The earlier `ObjViewer | x64` configuration also remains in the project.

## Current Status and Limitations

- Targets Windows and DirectX 11.
- Fireball lights are point lights centred on Lambert diffuse lighting and distance attenuation.
- The spotlight Actor is a basic placement demonstration combining an icon and decal, rather than a complete spotlight-lighting implementation.
- `PursuitMovementComponent` currently targets the editor's `FViewportCamera`, not a general-purpose scene component.
- Core PIE camera movement, rotation, and exit flow work, but some mouse click/drag handlers remain unimplemented.
- InterpTo control-point editing supports only `FVector` arrays and not curve-based interpolation.
- `FEATURE_GUIDE.md` describes team-level decal, height-fog, and fireball features; individual contribution boundaries should be verified against this README and the commit history.

## Notes

- Original collaborative repository: [jskim-research/Jungle_Week6_Team4](https://github.com/jskim-research/Jungle_Week6_Team4)
- The original repository is no longer available through the GitHub API, so I cross-checked commits from 9–15 April 2026 in [Rocketstein/Nips_W7](https://github.com/Rocketstein/Nips_W7), which continues the same history, against this repository's final code.
- Week 5 history before 9 April 2026 and Week 7 work after 15 April 2026 were excluded from the individual-contribution accounting.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 6 — Nips Engine

> DirectX 11 기반 멀티패스 렌더러와 에디터/PIE 입력 구조를 확장하고, 동적 광원 및 경로 이동 컴포넌트를 구현한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/jskim-research/Jungle_Week6_Team4)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

이전 주차의 에디터·렌더링 엔진을 기반으로 실제 플레이 세션과 복합 렌더 효과를 다룰 수 있도록 구조를 확장했습니다. Week 6에서는 Editor World와 PIE(Play In Editor)의 입력을 분리하고, G-Buffer 기반 Light Pass와 Fireball 광원, 제어점 기반 이동 및 카메라 추적 컴포넌트를 에디터·직렬화 흐름에 연결했습니다.

팀 단위로는 Decal, Height Fog, FXAA, 동적 BVH·Frustum Culling, LOD, Material Instance, PIE World 등도 함께 통합했습니다.

- **개발 기간:** 2026.04.09 ~ 2026.04.15
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, Dear ImGui, JSON
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- Editor World와 PIE 상태를 분리한 입력·카메라 제어
- G-Buffer의 Color / Normal / Depth / World Position을 사용하는 멀티패스 렌더링
- Structured Buffer 기반 다중 Point Light 계산
- 발광 메시와 동적 광원을 결합한 Fireball Actor
- Decal Volume Projection과 Fade In / Out
- 최대 8개 레이어의 Height Fog Post Process
- FXAA, Selection Outline, Lit / Unlit / Wireframe View Mode
- Control Point 기반 `InterpToMovementComponent`
- 카메라를 추적하는 `PursuitMovementComponent`
- Dynamic BVH 기반 Picking·Frustum Culling과 Static Mesh LOD
- ImGui Property Panel과 JSON Scene 저장·복원
- 최대 4개 Perspective / Orthographic 편집 뷰포트

## 담당 작업

### 1. Editor / PIE 입력 컨트롤러 구조

- 입력 처리를 `IBaseEditorController` 인터페이스와 `FEditorInputRouter`로 분리
- 편집 모드의 선택·기즈모·카메라 조작은 `FEditorWorldController`, 플레이 모드는 `FPIEController`가 담당하도록 구성
- PIE 시작·종료 시 활성 Controller와 World, Camera Target을 전환하도록 연결
- 키보드 Press / Hold / Release와 마우스 이동·클릭·드래그·휠 이벤트를 공통 Router에서 전달
- PIE 진입 시 Yaw / Pitch가 잘못된 방향으로 튀는 문제와 Controller 전환 후 Camera Impulse가 남는 문제 수정
- Editor에서는 커서를 가두지 않고 우클릭 드래그 중에만 Capture하며, PIE에서는 `Esc`로 세션 종료
- Ray–AABB, Ray–Triangle, Component Raycast를 `FRayCollision` 유틸리티로 분리해 선택·기즈모 경로에서 재사용

### 2. Fireball과 멀티패스 동적 광원

- `AFireballActor`와 `UFireballComponent`를 추가하고 Radius, Falloff, Intensity, Color를 편집 가능한 속성으로 제공
- `RenderCollector`가 Fireball 정보를 수집해 `FLightData` 배열로 만들고 GPU Structured Buffer에 전달
- Fullscreen Light Pass가 G-Buffer의 World Position과 Normal로 광원 방향·거리 감쇠·Lambert 항을 계산하도록 구현
- 여러 광원의 기여도를 누적하고 전역 Ambient를 더하는 최종 색상 경로 구성
- Static Mesh 상수 버퍼에 Emissive Color를 추가하고 발광 Surface는 Light Pass를 우회하도록 처리
- Lit / Unlit / Wireframe 모드가 Light Pass의 전역 조명 적용 여부에 반영되도록 연결
- HLSL/C++ Constant Buffer 정렬, GPU Padding, SRV 해제와 기본 Culling 문제 수정

### 3. Control Point 기반 InterpTo Movement

- 여러 `FVector` Control Point를 따라 이동하는 `UInterpToMovementComponent` 구현
- 전체 경로 대비 구간 거리 비율로 각 구간의 보간 시간을 계산해 속도 편차 완화
- `OneShot`, `OneShotReverse`, `Loop`, `PingPong` 네 가지 재생 모드 지원
- 이동 방향을 Quaternion으로 계산하고 `Slerp`해 회전이 자연스럽게 경로를 향하도록 구성
- 에디터에서 Control Point 배열을 추가·삭제·편집하고 Initiate / Stop / Reset을 실행하는 UI 추가
- `Vec3Array`와 `Enum` Property Type을 추가해 복제와 JSON 저장·복원까지 연결
- Control Point를 Actor 기준 Local 좌표로 편집한 뒤 PIE 시작 시 World 좌표로 변환하도록 수정

### 4. Pursuit Movement와 카메라 추적

- 일정 간격으로 목표 위치를 갱신하고 보간 이동하는 `UPursuitMovementComponent` 구현
- Detection Radius, Pursuit Speed, Update Interval과 목표 방향 회전 옵션을 Property Panel에 노출
- 목표 방향의 Yaw / Pitch를 계산하고 이동 중 방향을 부드럽게 보간
- 별도 Target이 없으면 PIE 시작 시 주 Perspective `FViewportCamera`를 자동 추적하도록 연결
- Component Factory 등록과 Duplicate 시 설정 복사·런타임 상태 초기화 처리

### 5. 프로퍼티·직렬화 및 통합 안정화

- `EPropertyType::Color`를 추가하고 ImGui `ColorEdit4`와 Fireball 속성 편집 연결
- Color 값을 JSON으로 저장·복원하고 Duplicate된 Fireball에도 색상이 유지되도록 수정
- Light Pass의 C++ / HLSL 데이터 배치와 Static Mesh Emissive Texture 전달 오류 보정
- Fireball Mesh·Material과 기본 Spotlight 배치 데모를 정리하고 Editor Spawn 목록에 연결
- PIE Camera 비행 이동, InterpTo Local Point, Fireball Culling 등 시연 중 발견된 통합 문제 수정

## 엔진 실행 흐름

```text
Windows Input
      │
      ▼
 InputSystem
      │
      ▼
FEditorInputRouter
      ├──────── Editor World ────────┐
      │    Camera / Picking / Gizmo  │
      └──────── PIE Controller ──────┤
             Play Camera / Exit      │
                                     ▼
                         World / Actor / Component
                              │             │
                    Movement Components    │
                 InterpTo / Pursuit        │
                              │             ▼
                              └──── RenderCollector
                                            │
                                            ▼
                                         RenderBus
                                            │
        G-Buffer Opaque ──► Light Pass ──► Fog / FXAA
             Color · Normal · Depth · World Position
```

### Fireball Light 경로

```text
UFireballComponent
Radius · Falloff · Intensity · Color
                 │
                 ▼
          FLightData Array
                 │
                 ▼
     GPU Structured Buffer
                 │
                 ▼
Fullscreen LightPass.hlsl
G-Buffer 복원 → 거리 감쇠 → N·L → 광원 누적
```

## 프로젝트 구조

```text
.
├─ FEATURE_GUIDE.md
├─ NipsEngine.sln
├─ NipsEngine/
│  ├─ Asset/                         # Mesh, Material, Texture, Scene
│  ├─ Settings/                      # Editor 설정
│  ├─ Shaders/
│  │  ├─ Multipass/                  # Light, Fog, FXAA
│  │  ├─ ShaderDecal.hlsl
│  │  └─ Selection / Outline Shader
│  └─ Source/
│     ├─ Editor/                     # Editor Engine, Viewport, Property UI
│     └─ Engine/
│        ├─ Component/Movement/      # InterpTo, Pursuit, Projectile, Rotating
│        ├─ Input/Controller/        # Editor / PIE Input Controller
│        ├─ Render/                  # Collector, Bus, Render Pass, Resource
│        ├─ Spatial/                 # Dynamic BVH와 World Spatial Index
│        └─ Serialization/           # JSON Scene 저장·복원
├─ Scripts/GenerateProjectFiles.py
├─ GenerateProjectFiles.bat
└─ ReleaseBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore

NuGet으로 `directxtk_desktop_win10`을 사용하며, 프로젝트는 C++20으로 구성되어 있습니다.

### 빌드

1. 파일 추가·이동 후 프로젝트를 재생성하려면 `GenerateProjectFiles.bat`을 실행합니다.
2. `NipsEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

`ReleaseBuild.bat`은 `Release | x64` 구성을 MSBuild로 빌드합니다. 이전 주차의 `ObjViewer | x64` 구성도 프로젝트에 남아 있습니다.

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- Fireball Light는 Point Light 형태이며 Lambert Diffuse와 거리 감쇠가 중심입니다.
- Spotlight Actor는 아이콘과 Decal을 조합한 기본 배치 데모로, 별도의 완성된 Spotlight 조명 계산은 아닙니다.
- `PursuitMovementComponent`의 Target은 현재 범용 Scene Component가 아니라 Editor의 `FViewportCamera`입니다.
- PIE Controller의 핵심 Camera 이동·회전과 종료 흐름은 동작하지만 일부 마우스 클릭·드래그 Handler는 비어 있습니다.
- InterpTo의 Control Point 편집은 `FVector` 배열 전용이며 Curve 기반 보간은 지원하지 않습니다.
- `FEATURE_GUIDE.md`는 팀 단위 Decal·Height Fog·Fireball을 설명하며 개인 기여 구분은 이 README와 커밋 기록을 기준으로 확인해야 합니다.

## 참고

- 원본 협업 저장소: [jskim-research/Jungle_Week6_Team4](https://github.com/jskim-research/Jungle_Week6_Team4)
- 현재 원본 저장소는 GitHub API에서 조회되지 않아, 동일 이력을 이어받은 [Rocketstein/Nips_W7](https://github.com/Rocketstein/Nips_W7)의 2026.04.09 ~ 2026.04.15 커밋과 이 저장소의 최종 코드를 교차 확인했습니다.
- 2026.04.09 이전의 Week 5 이력과 2026.04.16 이후 Week 7 작업은 개인 기여 집계에서 제외했습니다.
