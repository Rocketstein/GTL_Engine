> **Languages:** English · [한국어](#한국어)

# Week 13 — Krafton Engine: PhysX Simulation & Advanced Rendering

> A project integrating PhysX vehicles, ragdolls, cloth, physics-asset editing, and DOF/HDR/Bloom post-processing into a custom DirectX 11 engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/CaptainTangerine/Jungle_Week13_Team4).

## Project Overview

Building on the Week 12 engine, we extended the connections among a PhysX 4.1 physics scene, animation, and rendering. Physics Assets define skeletal bodies and constraints for ragdolls, while four-wheeled vehicles and NvCloth simulation run within the same engine runtime. On the rendering side, the project adds per-camera post-process settings together with depth of field, HDR, and Bloom pipelines.

The final team project supports collision filtering, a Physics Asset Editor, ragdolls, vehicles, NvCloth, DOF, HDR, and Bloom.

- **Core development period:** 29 May–3 June 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, PhysX 4.1, NvCloth, Dear ImGui, FMOD
- **Areas of responsibility:** PhysX vehicles, camera post-processing and DOF integration, Physics Asset debug rendering, and physics profiling
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- PhysX-scene creation of Actors, shapes, and constraints, with simulation/fetch/transform synchronisation
- Collision channels and masks, simulation/query filters, and contact/trigger callbacks
- Physics Asset body and constraint editing with bidirectional transitions between skeletal animation and ragdolls
- Four-wheeled vehicles with suspension raycasts, tyre friction, steering, braking, and automatic gear simulation
- NvCloth assets, simulation Components, and a dedicated editor
- Per-camera aperture, focus distance, focal length, and sample settings
- DOF built from half-resolution CoC and blur passes with full-resolution compositing
- HDR render targets, Bloom downsampling/upsampling, and gamma correction
- Wireframe and solid debug rendering for physics bodies and constraints
- Runtime profiling overlays and console commands for physics, collision, skinning, and other systems

## My Contributions

### 1. Parametric Four-Wheeled PhysX Vehicle System

- Implemented `UWheeledVehicleMovementComponent` and `AWheeledVehicle`, integrating `PxVehicleDrive4W` with the engine's Pawn/Component architecture
- Derived chassis dimensions, wheel radius/width, and wheel centres from chassis and wheel mesh bounds, then created convex geometry at runtime
- Configured chassis mass, inertia, centre of mass, suspension, tyres, differential, engine, gears, clutch, and Ackermann geometry
- Added vehicle registration/removal, surface–tyre friction pairs, and shared batched suspension-raycast buffers to `FPhysXVehicleManager`
- Applied analogue-input smoothing and speed-dependent steering scales, and implemented auto-reverse by braking before changing gear when switching between forward and reverse input
- Read chassis world pose and wheel-local poses after physics updates and synchronised them with the Actor and four wheel-mesh Components
- Removed vehicle Actors from ordinary body synchronisation so the manager became the single writer of simulation results

Representative commits: [`bdfb326a`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/bdfb326a306e7c9499eb8eefa5fc02708850a11f), [`d0be345e`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/d0be345ef345ac6b3883b25a500399ac49067c46), [`c1e30e11`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/c1e30e11f6f8f1db37855d8ab26717cb89c7b43f74e0df49)

### 2. Vehicle Demo Scene and Engine-Audio Feedback

- Added car-body and wheel assets and materials to the engine content pipeline and built `DriveDemo.Scene`
- Made vehicle sizing reusable across meshes by automatically deriving physics parameters from static-mesh extents
- Exposed engine angular velocity and gear ratio through the Vehicle Movement Component
- Connected an engine loop to the FMOD Audio Manager and varied sound pitch with engine speed
- Completed a single-scene demonstration covering vehicle creation, simulation, wheel poses, and audio

Representative commits: [`18cb16cd`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/18cb16cd9508ae65f1d93c81820547d7c0101afa), [`e1f1be34`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/e1f1be34558c55000d4278b4227fd2cf8c722fad), [`47b6cca7`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/47b6cca7ab236f1db37855d8ab26717cb89c7b43), [`464c1a13`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/464c1a1302203f05719a8cd693d937dd163df364)

### 3. Camera Post-Process Settings and the DOF Render Path

- Added the reflectable and serialisable `FPostProcessSettings` structure containing aperture, focus distance, focal length, and sample count
- Connected post-process settings to `UCameraComponent` and exposed them in the editor Inspector
- Implemented the camera-override path that transfers active-camera settings into `FViewportRenderOptions` for the game render pipeline during PIE
- Built DOF CoC and blur resources at half resolution and combined them with full-resolution scene colour in the final composite pass
- Explicitly unbound textures, switched render targets and viewports, and restored state after passes to prevent Direct3D 11 resource hazards
- Corrected downsample-resource sizing so odd viewport dimensions still receive sufficient coverage

Representative commits: [`2122aeed`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/2122aeedb36a74e4153e1a8fb9752f18ed05a05c), [`0dc47ef5`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/0dc47ef5d330b47f142b246128a2c8748e2e043a), [`5fcf898a`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/5fcf898acbcd1edaf27e6a8ed0b68e2127ea8d44), [`8a8a8e1b`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/8a8a8e1bb5bae79df9cde0af291efc2ac0b7b7ce)

### 4. Solid Physics-Asset Debug Rendering and Overlay Pass

- Implemented a solid-geometry builder that produces position/normal triangle soups for Physics Asset spheres, boxes, and capsules in addition to wire lines
- Generated smooth sphere normals, per-face box normals, and cylinder/hemisphere capsule geometry according to primitive conventions
- Added a minimal lighting shader and vertex format for physics bodies and connected them to debug draw commands
- Consolidated multiple triangle geometries in `BuildPhysicsBodyCommands` to simplify Physics Asset debug submission
- Propagated selected-body highlighting, body/constraint visibility, and solid/wireframe toggles through the scene-proxy cache
- Added `OverlayAlphaPass` to composite translucent physics bodies while preserving depth, and routed the Physics Asset Debug Component through it

Representative commits: [`4049ca07`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/4049ca0798d8b4d10a09c64ac2b95987accd56e0), [`92d5d854`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/92d5d854021fe7d5d4c8d8659ce0a9a8c600fa66), [`b1d16ac0`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/b1d16ac0f8d7a71b84f0f3fbf60a828ab3c50ba4), [`7067b877`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/7067b8773c644160540724272c9cc3c8c7fb46ab), [`f9e6c9ad`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/f9e6c9adcf6bf54d2632377fcc1c3dcc96e607c8)

### 5. Physics Profiling and Console Integration

- Added `FPhysicsStats` to collect the number of active constraints and simulating dynamic bodies in each frame
- Extracted valid physics counters from `PxSimulationStatistics` after `fetchResults`
- Combined existing `FStatManager` timing snapshots with physics counters in a single overlay
- Registered the `stat physics` command with the editor Console and managed the overlay's visibility state
- Reset statistics when restarting a scene or switching worlds so counters from the previous frame did not remain visible

Representative commits: [`8b6060ae`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/8b6060ae7f446f70ae3c879b2859748451a10db1), [`20aed4fa`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/20aed4fade5a9ef200681d035d8a81eacdfb2007)

## Vehicle-Simulation Architecture

```text
Player Input
 Throttle / Brake / Steering / Handbrake
        │
        ▼
UWheeledVehicleMovementComponent
 Parametric Chassis / Wheel Setup
        │ Register
        ▼
FPhysXVehicleManager::PreTick
 Gear Selection → Input Smoothing → Speed-Based Steering
        │
        ▼
Batch Suspension Raycast
        │
        ▼
PxVehicleUpdates
 Tyre Friction / Suspension / Engine / Gear
        │
        ├─ Chassis World Pose ──▶ AWheeledVehicle Transform
        ├─ Wheel Local Poses ───▶ Four Wheel-Mesh Components
        └─ Engine Omega ────────▶ FMOD Engine-Sound Pitch
```

Because physics geometry is derived from mesh dimensions, chassis and wheel parameters can adapt automatically when content changes. The manager batches suspension queries across vehicles and sends the resulting poses back to the transforms used by rendering Components.

## Camera DOF and Debug-Rendering Flow

```text
Active UCameraComponent
 FPostProcessSettings
        │
        ▼
FViewportRenderOptions
        │
        ├─ CoC Pass ─────── Half Resolution
        ├─ Blur Pass ────── Half Resolution
        └─ Composite Pass ─ Full-Resolution Scene Colour

UPhysicsAssetDebugComponent
 Body / Constraint / Selection State
        │
        ▼
FPhysicsAssetDebugSceneProxy
 Wire Lines + Solid Triangle Soup
        │
        ├─ Wireframe Debug Draw
        └─ Physics Body Shader → OverlayAlphaPass
```

Editor and PIE pipelines share the same camera post-process structure. The Physics Asset debug Component stores display state as a snapshot and submits its geometry separately to wireframe and translucent-solid paths.

## Validation Workflow

1. Open `DriveDemo.Scene`, start PIE, and verify chassis/wheel poses and engine-audio integration.
2. Adjust aperture, focus distance, focal length, and DOF samples on a Camera Component to compare post-process results.
3. Toggle body/constraint and solid/wireframe visibility in the Physics Asset Editor and inspect selected-body highlighting.
4. Run `stat physics` in the editor Console to inspect physics timing, active constraints, and simulating-body counters.
5. Resize the viewport and verify that half-resolution DOF resources and the full-resolution composite update together.

## Project Structure

```text
.
├─ KraftonEngine.sln
├─ Docs/                                  # HDR/Bloom, reflection, and Physics API docs
├─ KraftonEngine/
│  ├─ Content/
│  │  ├─ Audio/                          # Vehicle engine sound
│  │  ├─ Material/                       # Car and physics-debug materials
│  │  └─ Scene/DriveDemo.Scene
│  ├─ Shaders/
│  │  ├─ Editor/PhysicsBody.hlsl
│  │  └─ PostProcess/                    # DOF, HDR, Bloom, Gamma
│  └─ Source/
│     ├─ Editor/UI/Asset/                 # Physics Asset and cloth editors
│     └─ Engine/
│        ├─ Component/
│        │  ├─ Movement/WheeledVehicleMovementComponent.*
│        │  └─ Debug/PhysicsAssetDebugComponent.*
│        ├─ GameFramework/Pawn/WheeledVehicle.*
│        ├─ Physics/
│        │  ├─ PhysXPhysicsScene.*
│        │  ├─ PhysXVehicleManager.*
│        │  └─ Asset/                     # Bodies, constraints, Physics Assets
│        ├─ Profiling/Stats/PhysicsStats.*
│        └─ Render/
│           ├─ Proxy/PhysicsAssetDebugSceneProxy.*
│           └─ RenderPass/                # DOF, OverlayAlpha, HDR, Bloom
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ GameBuild.bat
└─ ReleaseBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore

The project uses the NuGet package `directxtk_desktop_win10`, together with repository-provided Lua, RmlUi, FMOD, FBX SDK, PhysX, and NvCloth libraries.

### Build Instructions

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `KraftonEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build and run `Debug | x64` or `Release | x64`.

Use `GameBuild.bat` for a game-runtime build and `ReleaseBuild.bat` for a distributable build.

## Notes

- The complete collaboration history and team-wide changes are available in [CaptainTangerine/Jungle_Week13_Team4](https://github.com/CaptainTangerine/Jungle_Week13_Team4).
- Week 13 contributions were counted from the repository's creation on 29 May 2026. The 33 top-level commits attributed to Rocketstein include merges and squashes.
- Contributions were identified by comparing not only commit messages but also changed files and the final implementation.
- Physics-scene core work, collision, ragdolls, the Physics Asset Editor, NvCloth, and final HDR/Bloom integration also include work by other team members.

---

## 한국어

# Week 13 — Krafton Engine: PhysX Simulation & Advanced Rendering

> DirectX 11 기반 커스텀 엔진에 PhysX 차량·Ragdoll·Cloth와 Physics Asset 편집 흐름, DOF·HDR·Bloom 후처리를 통합한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/CaptainTangerine/Jungle_Week13_Team4)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 12 엔진을 기반으로 PhysX 4.1 물리 Scene과 Animation·Rendering 사이의 연결을 확장했습니다. Physics Asset으로 Skeletal Body와 Constraint를 구성하고 Ragdoll을 구동하며, 4륜 차량과 NvCloth Simulation을 동일한 Engine Runtime에 통합했습니다. Rendering 쪽에서는 Camera별 Post Process 설정과 DOF, HDR·Bloom Pipeline을 추가했습니다.

최종 팀 결과물은 Collision Filtering, Physics Asset Editor, Ragdoll, Vehicle, NvCloth와 DOF·HDR·Bloom을 지원합니다.

- **핵심 개발 기간:** 2026.05.29 ~ 2026.06.03
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, PhysX 4.1, NvCloth, Dear ImGui, FMOD
- **담당 영역:** PhysX Vehicle, Camera Post Process·DOF Integration, Physics Asset Debug Rendering, Physics Profiling
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- PhysX Scene 기반 Actor·Shape·Constraint 생성과 Simulation / Fetch / Transform Sync
- Collision Channel·Mask와 Simulation / Query Filter, Contact·Trigger Callback
- Physics Asset의 Body·Constraint 편집, Skeletal Animation과 Ragdoll 양방향 전환
- 4륜 Vehicle의 Suspension Raycast, Tire Friction, Steering·Brake·Auto Gear Simulation
- NvCloth 기반 Cloth Asset, Simulation Component와 전용 Editor
- Camera별 Aperture·Focus Distance·Focal Length·Sample 설정
- Half-resolution CoC·Blur와 Full-resolution Composite로 구성한 DOF
- HDR Render Target, Bloom Downsample·Upsample와 Gamma Correction
- Physics Body·Constraint의 Wireframe / Solid Debug Rendering
- Physics·Collision·Skinning 등 Runtime Profiling Overlay와 Console Command

## 담당 작업

### 1. Parametric PhysX 4륜 Vehicle System

- `UWheeledVehicleMovementComponent`와 `AWheeledVehicle`을 구현해 Engine의 Pawn·Component 구조에 `PxVehicleDrive4W` 통합
- Chassis·Wheel Mesh Bounds에서 차체 크기, Wheel Radius·Width와 Wheel Center를 산출하고 Convex Geometry를 Runtime에 생성
- Chassis Mass·Inertia·Center of Mass, Suspension, Tire, Differential, Engine, Gear, Clutch와 Ackermann Geometry 구성
- `FPhysXVehicleManager`에 차량 등록·해제, Surface–Tire Friction Pair와 공유 Batch Suspension Raycast Buffer 관리
- Analog Input Smoothing과 속도별 Steering Scale을 적용하고 전진·후진 입력 전환 시 제동 후 Gear를 바꾸는 Auto-reverse 처리
- Physics Update 후 Chassis World Pose와 Wheel Local Pose를 읽어 Actor와 네 Wheel Mesh Component에 동기화
- Vehicle Actor를 일반 Body Sync 대상과 분리해 Manager가 Simulation의 단일 Writer가 되도록 책임 정리

대표 커밋: [`bdfb326a`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/bdfb326a306e7c9499eb8eefa5fc02708850a11f), [`d0be345e`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/d0be345ef345ac6b3883b25a500399ac49067c46), [`c1e30e11`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/c1e30e11f6f8f1db37855d8ab26717cb89c7b43f74e0df49)

### 2. Vehicle Demo Scene과 Engine Audio Feedback

- Car Body·Wheel Asset과 Material을 Engine Content Pipeline에 추가하고 `DriveDemo.Scene` 구성
- Static Mesh Extent를 사용하는 차량 크기 자동 보정으로 다른 Mesh에서도 Physics Parameter를 재사용할 수 있도록 개선
- Vehicle Movement Component가 Engine Angular Velocity와 Gear Ratio를 노출하도록 구성
- FMOD Audio Manager에 Engine Loop를 연결하고 회전수에 따라 Engine Sound Pitch가 변하도록 Audio Hook 구현
- 차량 생성·Simulation·Wheel Pose·Audio까지 한 Scene에서 검증할 수 있는 Demo 흐름 완성

대표 커밋: [`18cb16cd`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/18cb16cd9508ae65f1d93c81820547d7c0101afa), [`e1f1be34`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/e1f1be34558c55000d4278b4227fd2cf8c722fad), [`47b6cca7`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/47b6cca7ab236f1db37855d8ab26717cb89c7b43), [`464c1a13`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/464c1a1302203f05719a8cd693d937dd163df364)

### 3. Camera Post Process Settings와 DOF Render Path

- Aperture, Focus Distance, Focal Length와 Sample Count를 담는 `FPostProcessSettings` 구조를 Reflection·Serialization 체계에 추가
- `UCameraComponent`가 Post Process 설정을 보유하도록 연결하고 Editor Inspector에서 값을 조정할 수 있도록 구성
- PIE에서 Active Camera의 설정이 Game Render Pipeline의 `FViewportRenderOptions`로 전달되도록 Camera Override 경로 구현
- DOF의 CoC·Blur Resource를 Half Resolution으로 구성하고 마지막 Composite 단계에서 Full Resolution Scene Color와 결합
- Texture Binding 해제, Render Target·Viewport 전환과 Pass 종료 후 상태 복원을 명시해 D3D11 Resource Hazard 방지
- 홀수 Viewport 크기에서도 Downsample Resource가 충분한 영역을 갖도록 크기 계산 보정

대표 커밋: [`2122aeed`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/2122aeedb36a74e4153e1a8fb9752f18ed05a05c), [`0dc47ef5`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/0dc47ef5d330b47f142b246128a2c8748e2e043a), [`5fcf898a`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/5fcf898acbcd1edaf27e6a8ed0b68e2127ea8d44), [`8a8a8e1b`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/8a8a8e1bb5bae79df9cde0af291efc2ac0b7b7ce)

### 4. Physics Asset Solid Debug Rendering과 Overlay Pass

- Physics Asset의 Sphere·Box·Capsule을 Wire Line뿐 아니라 Position·Normal을 가진 Triangle Soup으로 생성하는 Solid Builder 구현
- Sphere의 Smooth Normal, Box의 Face Normal, Capsule의 Cylinder·Hemisphere Geometry를 Primitive 규약에 맞춰 구성
- Physics Body 전용 Minimal Lighting Shader와 Vertex Format을 추가하고 Debug Draw Command에 연결
- 여러 Triangle Geometry를 `BuildPhysicsBodyCommands`에서 통합해 Physics Asset Debug Draw Submission 정리
- 선택 Body Highlight, Body / Constraint 표시와 Solid / Wireframe Toggle을 Scene Proxy Cache에 반영
- Depth를 유지하면서 반투명 Physics Body를 합성하는 `OverlayAlphaPass`를 추가하고 Physics Asset Debug Component를 해당 경로로 Routing

대표 커밋: [`4049ca07`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/4049ca0798d8b4d10a09c64ac2b95987accd56e0), [`92d5d854`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/92d5d854021fe7d5d4c8d8659ce0a9a8c600fa66), [`b1d16ac0`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/b1d16ac0f8d7a71b84f0f3fbf60a828ab3c50ba4), [`7067b877`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/7067b8773c644160540724272c9cc3c8c7fb46ab), [`f9e6c9ad`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/f9e6c9adcf6bf54d2632377fcc1c3dcc96e607c8)

### 5. Physics Profiling과 Console Integration

- 한 Frame의 Active Constraint 수와 Simulating Dynamic Body 수를 수집하는 `FPhysicsStats` 추가
- `fetchResults` 이후 `PxSimulationStatistics`에서 유효한 Physics Counter를 추출
- 기존 `FStatManager` Timing Snapshot과 Physics Counter를 결합한 Overlay 구성
- Editor Console에 `stat physics` Command를 등록하고 Physics Overlay 표시 상태를 관리
- Scene을 다시 시작하거나 World를 전환할 때 이전 Frame Counter가 남지 않도록 Stat Reset 경로 연결

대표 커밋: [`8b6060ae`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/8b6060ae7f446f70ae3c879b2859748451a10db1), [`20aed4fa`](https://github.com/CaptainTangerine/Jungle_Week13_Team4/commit/20aed4fade5a9ef200681d035d8a81eacdfb2007)

## Vehicle Simulation 구조

```text
Player Input
 Throttle / Brake / Steering / Handbrake
        │
        ▼
UWheeledVehicleMovementComponent
 Parametric Chassis·Wheel Setup
        │ Register
        ▼
FPhysXVehicleManager::PreTick
 Gear Selection → Input Smoothing → Speed-based Steering
        │
        ▼
Batch Suspension Raycast
        │
        ▼
PxVehicleUpdates
 Tire Friction / Suspension / Engine / Gear
        │
        ├─ Chassis World Pose ──▶ AWheeledVehicle Transform
        ├─ Wheel Local Poses ───▶ Four Wheel Mesh Components
        └─ Engine Omega ────────▶ FMOD Engine Sound Pitch
```

차량은 Mesh 크기로부터 Physics Geometry를 구성하므로 Content가 바뀌어도 Chassis와 Wheel Parameter를 자동으로 맞출 수 있습니다. Suspension Query는 Manager가 차량별 요청을 모아 Batch로 실행하며, 결과 Pose는 Render Component가 사용하는 Transform으로 다시 전달됩니다.

## Camera DOF와 Debug Render 흐름

```text
Active UCameraComponent
 FPostProcessSettings
        │
        ▼
FViewportRenderOptions
        │
        ├─ CoC Pass ─────── Half Resolution
        ├─ Blur Pass ────── Half Resolution
        └─ Composite Pass ─ Full Resolution Scene Color

UPhysicsAssetDebugComponent
 Body / Constraint / Selection State
        │
        ▼
FPhysicsAssetDebugSceneProxy
 Wire Lines + Solid Triangle Soup
        │
        ├─ Wireframe Debug Draw
        └─ Physics Body Shader → OverlayAlphaPass
```

Camera의 Post Process 값은 Editor와 PIE Pipeline이 같은 구조를 공유합니다. Physics Asset Debug Geometry는 Component의 표시 상태를 Snapshot으로 보관한 뒤 Wireframe과 반투명 Solid 경로에 각각 제출합니다.

## 확인 흐름

1. Content의 `DriveDemo.Scene`을 열고 PIE를 시작해 Chassis·Wheel Pose와 Engine Audio 연동을 확인합니다.
2. Camera Component의 Aperture, Focus Distance, Focal Length와 DOF Sample 값을 조정해 Post Process 결과를 비교합니다.
3. Physics Asset Editor에서 Body / Constraint, Solid / Wireframe 표시를 전환하고 선택 Body Highlight를 확인합니다.
4. Editor Console에서 `stat physics`를 실행해 Physics Timing, Active Constraint와 Simulating Body Counter를 확인합니다.
5. Viewport 크기를 변경하며 Half-resolution DOF Resource와 Full-resolution Composite가 함께 갱신되는지 확인합니다.

## 프로젝트 구조

```text
.
├─ KraftonEngine.sln
├─ Docs/                                  # HDR·Bloom, Reflection, Physics API 문서
├─ KraftonEngine/
│  ├─ Content/
│  │  ├─ Audio/                          # Vehicle Engine Sound
│  │  ├─ Material/                       # Car·Physics Debug Material
│  │  └─ Scene/DriveDemo.Scene
│  ├─ Shaders/
│  │  ├─ Editor/PhysicsBody.hlsl
│  │  └─ PostProcess/                    # DOF·HDR·Bloom·Gamma
│  └─ Source/
│     ├─ Editor/UI/Asset/                 # Physics Asset·Cloth Editor
│     └─ Engine/
│        ├─ Component/
│        │  ├─ Movement/WheeledVehicleMovementComponent.*
│        │  └─ Debug/PhysicsAssetDebugComponent.*
│        ├─ GameFramework/Pawn/WheeledVehicle.*
│        ├─ Physics/
│        │  ├─ PhysXPhysicsScene.*
│        │  ├─ PhysXVehicleManager.*
│        │  └─ Asset/                     # Body·Constraint·Physics Asset
│        ├─ Profiling/Stats/PhysicsStats.*
│        └─ Render/
│           ├─ Proxy/PhysicsAssetDebugSceneProxy.*
│           └─ RenderPass/                # DOF·OverlayAlpha·HDR·Bloom
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ GameBuild.bat
└─ ReleaseBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore

프로젝트는 NuGet의 `directxtk_desktop_win10`과 저장소 내 Lua, RmlUi, FMOD, FBX SDK, PhysX, NvCloth Library를 사용합니다.

### 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio Project File을 생성합니다.
2. `KraftonEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

게임 실행용 결과물은 `GameBuild.bat`, 배포용 결과물은 `ReleaseBuild.bat`으로 구성할 수 있습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [CaptainTangerine/Jungle_Week13_Team4](https://github.com/CaptainTangerine/Jungle_Week13_Team4)에서 확인할 수 있습니다.
- 원본 저장소 생성일인 2026.05.29부터 Week 13 기여를 집계했으며, Rocketstein 작성자 정보로 기록된 33개 Top-level Commit에는 Merge·Squash Commit이 포함되어 있습니다.
- 담당 작업은 Commit Message만이 아니라 변경 File과 최종 구현을 함께 대조해 정리했습니다.
- Physics Scene Core, Collision, Ragdoll, Physics Asset Editor, NvCloth와 HDR·Bloom의 최종 통합에는 팀원의 작업도 포함되어 있습니다.
