> **Languages:** English · [한국어](#한국어)

# Week 9 — Lunatic Engine

> A three-lane runner built by combining Lua gameplay logic with a custom C++ DirectX 11 engine.

- **Core development period:** 30 April–7 May 2026
- **Areas of responsibility:** Collision shapes and overlaps, procedural maps and obstacles, the Imposter Gizmo mechanic, Player Camera Manager, and Camera Modifiers
- **Engine:** C++20, DirectX 11, HLSL, ImGui
- **Gameplay:** LuaJIT, sol2, coroutines, hot reload
- **Environment:** Windows, Visual Studio 2022, MSVC v143

## Project Overview

`Lunatic Engine` is a team project that built a playable game runtime on top of the rendering and editor engine developed in previous weeks. The result is a runner in which the player moves between three lanes while navigating procedurally chained chunks, obstacles, items, and a fake editor-gizmo mechanic.

Rather than merely adding isolated engine features, Week 9 connected C++ collision, camera, and map systems to Lua-based player control, game state, and UI feedback, producing a complete Title → Story → Play → Result flow.

## Key Features

- Template-based runtime generation and recycling of map chunks, with three-lane obstacle placement
- Box, sphere, and capsule shape components with collision tests for every supported pairing
- C++ and Lua delivery of begin/end overlap and hit events
- Obstacles such as barriers, pendulums, and wireballs, plus item and Imposter Gizmo mechanics
- Player Camera Manager, view-target changes, camera shake, fade, and letterboxing
- LuaJIT script components, coroutines, hot reload, and engine API bindings
- Title, Story, Playground, and Result scenes with HUD, dialogue, and score systems
- Editor, Demo, and Shipping configurations with asset cooking

## My Contributions

### 1. Collision Shapes and the Overlap Foundation

- Implemented `UBoxComponent`, `USphereComponent`, and `UCapsuleComponent` on the shared `UShapeComponent` base
- Added per-shape world AABB calculation and selection-state debug drawing for visual inspection of collision volumes in the editor
- Split the existing ray-picking `FHitResult` into `FRayHitResult`, then defined collision-oriented `FHitResult` and `FOverlapInfo` structures
- Registered Box–Box, Box–Sphere, Box–Capsule, Sphere–Sphere, Sphere–Capsule, and Capsule–Capsule tests with `FCollisionDispatcher`
- Connected world-level overlap updates to Component begin/end overlap events and removed redundant pair checks

Representative commits: [`2842d1ed`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2842d1edb221c0baaf94ebab0ccf2ed580a9c50c), [`2ec0064e`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2ec0064e0184b82d72014660c1fc7fc4ecaa6eb2), [`5578f8c6`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/5578f8c6f0e9474479dc13d281ba00f21f1e0352)

### 2. Procedural Map Chunks and Obstacles

- Implemented a runtime recycling system in which `AMapManager` maintains the active chunk count ahead of the player and removes chunks that have been passed
- Connected each new chunk to the previous template's exit location and rotation, then placed obstacles and items probabilistically across three lanes
- Selected placement candidates so all lanes could not become blocked simultaneously, and cleaned up owned obstacles and items with each removed chunk
- Added wireball and pendulum obstacles, and corrected barrier variants, collision-box sizes, root components, and texture assignment
- Randomly selected normal or bugged floor materials to add visual variation during a run
- Fixed chunks being removed too early after the player crossed an exit and adjusted the length of the starting section

Representative commits: [`e86152b4`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/e86152b481f1f573f731787f34ee67bd13b58807), [`c58db0a2`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c58db0a2f0ea6f1a6d99e62a219005e1babebf73), [`07ddd1c6`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/07ddd1c6926774b47eacfde11083f2f431858aaa), [`cefd40ef`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/cefd40eff248df1f629f7902f9b681dfccd14d83), [`e2ebd9a9`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/e2ebd9a9849d809e349fa7cb43211833dfb5e457), [`69aafb54`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/69aafb54eadacc858bbc5d1fff207413db5556fe)

### 3. Imposter Gizmo Mechanic

- Built `FGimmickManager`, which selects a random obstacle target and chooses a translation, rotation, or scale effect
- Implemented a fake editor-style gizmo that captures an obstacle at runtime and transforms it after a delay
- Preserved and restored the target's selection-outline state and safely released it if the target was destroyed first
- Tuned activation delay, spawn probability, and target-selection range to create sudden obstacle transformations during play

Representative commits: [`b7f9ab0b`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/b7f9ab0b2e7ccf04d7ad42cbf802dd4cbc31b682), [`4f47b5c7`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/4f47b5c73f46c3c7d29b3fea2b78def2ed5e73eb), [`d33fb2b8`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/d33fb2b8f3cba83908194739d88d93c0c2b47bf5)

### 4. Player Camera Manager and Final POV Path

- Consolidated position, rotation, FOV, and post-processing into `FMinimalViewInfo`
- Implemented `APlayerCameraManager` to calculate the current POV from the view target each frame and retain previous/current camera-cache snapshots
- Applied a priority-sorted list of Camera Modifiers to the final POV and removed modifiers after completion
- Connected final-POV delivery across Game Mode, Camera Components, Frame Context, and editor/default render pipelines
- Fixed missing ticks, shadowed disabled defaults, null access, and double destruction of UObjects

Representative commits: [`fac04d55`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/fac04d55fb39298c5ac5742edb784b3a19646a3d), [`4db5ea78`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/4db5ea78cb5b481856e952a80cbf1fc7eb1bc426), [`7776b012`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/7776b012b1dac32a85d76d841f0b7e36344956fd), [`548c3ed5`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/548c3ed522ae8ae4a9f707a502f4c4cdda12f5c7), [`2881f358`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2881f35864d2558df16ff4a2e2aa2255824aed1c), [`43507b08`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/43507b08be71a58b28e69ff4fa7f4ec07948e01f)

### 5. Camera Modifiers, Shake, and Fade

- Implemented modifier fade-in/fade-out alpha, pending-disable state, and priority handling across the effect lifetime
- Added `UCurveFloat` interpolation and split camera-shake translation XYZ and rotation pitch/yaw/roll into six independent curves
- Accumulated location and rotation offsets from sine-wave and curve-based shake patterns into the final POV
- Clarified ownership and destruction between camera-shake patterns and their internal curve UObjects, preventing leaks and double frees
- Passed fade colour, start/end alpha, and duration into camera post-process values

Representative commits: [`8f2f1d5c`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/8f2f1d5c1e67b774e4bf06261ed99d2f5da8ecac), [`df004263`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/df004263adacd8ee41775fd397d63c5fe8c6d635), [`6020184e`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/6020184e4dd6a761349df1140b056ebcca52b4f5), [`56974f6f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/56974f6f95f3d684bc6d97d64c5970f27e35e822), [`c010b37f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c010b37f68e3b5020c120513c0d12399d844bb11)

## Game-Runtime Architecture

```text
Title / Story / Playground / Result Scene
                    │
                    ▼
             Lua GameManager
       State · Score · Items · UI · Dialogue
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
      ARunner             AMapManager
 Input · Spring Arm     Active-Chunk Management
          │                   │
          │           AMapChunk Template
          │             │            │
          │         Obstacle       Item / Gizmo
          │             │
          └──── Collision Dispatcher ────┐
                                        ▼
                              Lua Hit / Overlap Feedback

AActor::CalcCamera
        │
        ▼
APlayerCameraManager
View Target → Modifier / Shake → Fade / Letterbox
        │
        ▼
FMinimalViewInfo Camera Cache → Frame Context → Render Pipeline
```

## Project Structure

```text
.
├─ LunaticEngine.sln
├─ LunaticEngine/
│  ├─ Asset/Content/Scene/             # Title, Story, Playground, Result
│  ├─ Scripts/
│  │  ├─ Common/                       # Shared Lua modules
│  │  ├─ Game/                         # Game state, player, items, camera
│  │  └─ UI/                           # HUD, dialogue, scene UI
│  ├─ Shaders/                         # Geometry, lighting, post-process, UI
│  ├─ Source/
│  │  ├─ Editor/
│  │  ├─ Engine/
│  │  │  ├─ Camera/                    # Manager, modifiers, shake, POV
│  │  │  ├─ Collision/                 # Dispatcher, BVH, overlap
│  │  │  ├─ Component/Shape/           # Box, sphere, capsule
│  │  │  └─ Scripting/                 # Lua runtime and bindings
│  │  └─ Game/
│  │     ├─ GameActors/                # Obstacles, items, gimmicks
│  │     ├─ Map/                       # Chunk templates and manager
│  │     └─ Player/                    # Runner
│  └─ ThirdParty/                      # ImGui, sol2, etc.
├─ GenerateProjectFiles.bat
├─ DemoBuild.bat
├─ ReleaseBuild.bat
└─ ShippingBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore

The project uses `directxtk_desktop_win10` and `luajit.native` through NuGet.

### Development Build

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `LunaticEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build and run `Debug | x64` or `Release | x64`.

### Deployment Builds

- `DemoBuild.bat`: Builds the Demo configuration and copies runtime resources into `DemoBuild/`
- `ReleaseBuild.bat`: Builds the Release configuration and copies runtime resources into `ReleaseBuild/`
- `ShippingBuild.bat`: Builds Shipping, cooks scenes as `.umap`, and stages only the required assets, shaders, and scripts

## Current Status and Limitations

- Targets Windows and DirectX 11.
- The collision dispatcher currently supports all six pairings among box, sphere, and capsule shapes.
- Some rotated/branching map-template types and the `MustJump` decision are disabled in the current code, so the procedural layout mainly varies along a straight path.
- Although additional obstacle Actor classes remain in the codebase, not every type is enabled in the current chunk spawn table.
- `ProjectSettings.ini` starts from the `Title` scene with `AGameModeBase`; the actual game flow is connected through scenes and Lua scripts.
- Missing Basic Shape, Spaceship, and SlideOrJump OBJ assets were restored on 19 August 2026 to preserve the repository.

## Notes

- The complete collaboration history and team-wide changes are available in [Rocketstein/Jungle_Week9_Team6](https://github.com/Rocketstein/Jungle_Week9_Team6).
- The original repository was forked from Week 8, so history before 30 April 2026 was excluded from the Week 9 individual-contribution accounting.
- The 84 commits attributed to Rocketstein during the core development period include merges and checkpoints; the list above therefore presents representative commits identified from messages, changed files, and the final code.
- Subsequent asset-restoration commits: [`c751f11f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c751f11f828df289ec9e9bc37d08a0ec26dfbc33), [`76f5df27`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/76f5df27688ac872105712e776fcf95c1ed58802)

---

## 한국어

# Week 9 — Lunatic Engine

> C++로 제작한 DirectX 11 기반 커스텀 엔진 위에 Lua 게임 로직을 결합해 완성한 3레인 러너 프로젝트입니다.

- **핵심 개발 기간:** 2026.04.30 ~ 2026.05.07
- **담당 영역:** Collision Shape·Overlap, 절차적 맵과 장애물, Imposter Gizmo 기믹, Player Camera Manager·Camera Modifier
- **Engine:** C++20, DirectX 11, HLSL, ImGui
- **Gameplay:** LuaJIT, sol2, Coroutine, Hot Reload
- **Environment:** Windows, Visual Studio 2022, MSVC v143

## 프로젝트 개요

`Lunatic Engine`은 이전 주차의 렌더링·에디터 엔진을 기반으로 실제 플레이 가능한 게임 런타임을 구축한 팀 프로젝트입니다. 플레이어가 3개 레인을 달리며 절차적으로 이어지는 청크, 장애물, 아이템과 가짜 편집 기즈모 기믹에 대응하는 러너 게임을 구현했습니다.

Week 9에서는 엔진 기능을 단순히 추가하는 데 그치지 않고, C++의 충돌·카메라·맵 시스템과 Lua의 플레이어 제어·게임 상태·UI 피드백을 연결해 Title → Story → Play → Result로 이어지는 실행 흐름을 구성했습니다.

## 주요 기능

- Template 기반 런타임 Map Chunk 생성·회수와 3레인 장애물 배치
- Box / Sphere / Capsule Shape Component와 조합별 충돌 판정
- Begin / End Overlap 및 Hit Event를 C++과 Lua 게임 로직으로 전달
- Barrier, Pendulum, Wireball 등 장애물과 Item·Imposter Gizmo 기믹
- Player Camera Manager, View Target 전환, Camera Shake·Fade·Letterbox
- LuaJIT 기반 Script Component, Coroutine, Hot Reload와 Engine API Binding
- Title, Story, Playground, Result Scene과 HUD·Dialogue·Score 시스템
- Editor / Demo / Shipping 구성을 포함한 빌드 및 Asset Cooking 흐름

## 담당 작업

### 1. Collision Shape와 Overlap 기반 구축

- `UShapeComponent`를 공통 기반으로 `UBoxComponent`, `USphereComponent`, `UCapsuleComponent` 구현
- Shape별 World AABB 계산과 선택 상태 Debug Draw를 추가해 에디터에서 충돌 영역을 시각적으로 확인
- 기존 Ray Picking용 `FHitResult`를 `FRayHitResult`로 분리하고 실제 충돌 결과용 `FHitResult`, `FOverlapInfo` 정의
- `FCollisionDispatcher`에 Box–Box, Box–Sphere, Box–Capsule, Sphere–Sphere, Sphere–Capsule, Capsule–Capsule 판정 등록
- World 단위 Overlap 갱신과 Component Begin / End Overlap 흐름을 연결하고 불필요한 중복 비교 제거

대표 커밋: [`2842d1ed`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2842d1edb221c0baaf94ebab0ccf2ed580a9c50c), [`2ec0064e`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2ec0064e0184b82d72014660c1fc7fc4ecaa6eb2), [`5578f8c6`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/5578f8c6f0e9474479dc13d281ba00f21f1e0352)

### 2. 절차적 Map Chunk와 장애물 시스템

- `AMapManager`가 플레이어 전방의 Active Chunk 수를 유지하고 통과한 선두 Chunk를 회수하도록 런타임 순환 구조 구현
- Chunk Template의 Exit Location / Rotation을 기준으로 다음 Chunk를 연결하고 3개 Lane에 확률적으로 장애물·아이템 배치
- 특정 Lane이 모두 막히지 않도록 배치 후보를 선택하고 Chunk가 제거될 때 소유한 장애물·아이템도 함께 정리
- Wireball과 Pendulum 장애물을 추가하고 Barrier 변형, 충돌 Box 크기·Root Component·Texture 적용을 보정
- 일반 / Bugged 바닥 Material을 확률적으로 선택해 주행 중 시각적 변주 제공
- 플레이어가 Chunk 출구를 지난 뒤 너무 일찍 제거되던 문제와 시작 구간 길이 조정

대표 커밋: [`e86152b4`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/e86152b481f1f573f731787f34ee67bd13b58807), [`c58db0a2`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c58db0a2f0ea6f1a6d99e62a219005e1babebf73), [`07ddd1c6`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/07ddd1c6926774b47eacfde11083f2f431858aaa), [`cefd40ef`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/cefd40eff248df1f629f7902f9b681dfccd14d83), [`e2ebd9a9`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/e2ebd9a9849d809e349fa7cb43211833dfb5e457), [`69aafb54`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/69aafb54eadacc858bbc5d1fff207413db5556fe)

### 3. Imposter Gizmo 기믹

- 장애물을 무작위 Target으로 선택하는 `FGimmickManager`와 Translate / Rotation / Scale 기믹 선택 흐름 구성
- 런타임에서 Editor Gizmo처럼 보이는 가짜 기즈모가 장애물을 Capture하고 일정 지연 후 Transform을 수행하도록 구현
- Capture 대상의 선택 Outline 상태를 저장·복구하고 Target이 먼저 파괴된 경우 안전하게 Release
- 활성화 지연, 기믹 생성 확률과 대상 선택 범위를 조정해 플레이 중 갑작스러운 장애물 변형을 연출

대표 커밋: [`b7f9ab0b`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/b7f9ab0b2e7ccf04d7ad42cbf802dd4cbc31b682), [`4f47b5c7`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/4f47b5c73f46c3c7d29b3fea2b78def2ed5e73eb), [`d33fb2b8`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/d33fb2b8f3cba83908194739d88d93c0c2b47bf5)

### 4. Player Camera Manager와 최종 POV 경로

- Camera 상태를 위치·회전·FOV·Post Process를 함께 담는 `FMinimalViewInfo`로 정리
- `APlayerCameraManager`가 View Target에서 매 Frame POV를 계산하고 이전/현재 Camera Cache Snapshot을 유지하도록 구현
- 우선순위로 정렬된 Camera Modifier 목록을 최종 POV에 적용하고 종료된 Modifier를 회수
- Game Mode와 Camera Component, Frame Context, Editor / Default Render Pipeline 사이에 최종 POV 전달 경로 연결
- Tick 누락, 비활성화 기본값 Shadowing, Null 접근과 UObject 이중 소멸 문제 수정

대표 커밋: [`fac04d55`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/fac04d55fb39298c5ac5742edb784b3a19646a3d), [`4db5ea78`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/4db5ea78cb5b481856e952a80cbf1fc7eb1bc426), [`7776b012`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/7776b012b1dac32a85d76d841f0b7e36344956fd), [`548c3ed5`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/548c3ed522ae8ae4a9f707a502f4c4cdda12f5c7), [`2881f358`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/2881f35864d2558df16ff4a2e2aa2255824aed1c), [`43507b08`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/43507b08be71a58b28e69ff4fa7f4ec07948e01f)

### 5. Camera Modifier·Shake·Fade

- Modifier의 Alpha In / Out, Pending Disable과 Priority 처리로 효과의 진입·종료 수명주기 구현
- `UCurveFloat` 보간 기능을 추가하고 Camera Shake의 이동 XYZ·회전 Pitch/Yaw/Roll을 6개 독립 Curve로 분리
- Sin Wave와 Curve 기반 Shake Pattern이 Location / Rotation Offset을 최종 POV에 누적하도록 구성
- Camera Shake Pattern과 내부 Curve UObject의 소유·소멸 관계를 정리해 누수와 이중 해제를 방지
- Fade 색상·시작/종료 Alpha·Duration을 Camera Post Process 값으로 전달

대표 커밋: [`8f2f1d5c`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/8f2f1d5c1e67b774e4bf06261ed99d2f5da8ecac), [`df004263`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/df004263adacd8ee41775fd397d63c5fe8c6d635), [`6020184e`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/6020184e4dd6a761349df1140b056ebcca52b4f5), [`56974f6f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/56974f6f95f3d684bc6d97d64c5970f27e35e822), [`c010b37f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c010b37f68e3b5020c120513c0d12399d844bb11)

## 게임 런타임 구조

```text
Title / Story / Playground / Result Scene
                    │
                    ▼
             Lua GameManager
       상태·점수·아이템·UI·Dialogue
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
      ARunner             AMapManager
  입력·Spring Arm       Active Chunk 유지
          │                   │
          │           AMapChunk Template
          │             │            │
          │         Obstacle       Item / Gizmo
          │             │
          └──── Collision Dispatcher ────┐
                                        ▼
                              Lua Hit / Overlap Feedback

AActor::CalcCamera
        │
        ▼
APlayerCameraManager
View Target → Modifier / Shake → Fade / Letterbox
        │
        ▼
FMinimalViewInfo Camera Cache → Frame Context → Render Pipeline
```

## 프로젝트 구조

```text
.
├─ LunaticEngine.sln
├─ LunaticEngine/
│  ├─ Asset/Content/Scene/             # Title, Story, Playground, Result
│  ├─ Scripts/
│  │  ├─ Common/                       # Lua 공용 모듈
│  │  ├─ Game/                         # Game State, Player, Item, Camera
│  │  └─ UI/                           # HUD, Dialogue, Scene UI
│  ├─ Shaders/                         # Geometry, Lighting, Post Process, UI
│  ├─ Source/
│  │  ├─ Editor/
│  │  ├─ Engine/
│  │  │  ├─ Camera/                    # Manager, Modifier, Shake, POV
│  │  │  ├─ Collision/                 # Dispatcher, BVH, Overlap
│  │  │  ├─ Component/Shape/           # Box, Sphere, Capsule
│  │  │  └─ Scripting/                 # Lua Runtime 및 Binding
│  │  └─ Game/
│  │     ├─ GameActors/                # Obstacle, Item, Gimmick
│  │     ├─ Map/                       # Chunk Template / Manager
│  │     └─ Player/                    # Runner
│  └─ ThirdParty/                      # ImGui, sol2 등
├─ GenerateProjectFiles.bat
├─ DemoBuild.bat
├─ ReleaseBuild.bat
└─ ShippingBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore

NuGet으로 `directxtk_desktop_win10`과 `luajit.native`를 사용합니다.

### 개발 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio 프로젝트 파일을 생성합니다.
2. `LunaticEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

### 배포 빌드

- `DemoBuild.bat`: Demo 구성과 실행 리소스를 `DemoBuild/`에 복사
- `ReleaseBuild.bat`: Release 구성과 실행 리소스를 `ReleaseBuild/`에 복사
- `ShippingBuild.bat`: Shipping 빌드 후 Scene을 `.umap`으로 Cook하고 필요한 Asset·Shader·Script만 Stage

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- Collision Dispatcher는 현재 Box / Sphere / Capsule 사이의 6개 조합을 지원합니다.
- Map Template의 회전·분기 타입 일부와 `MustJump` 결정은 현재 코드에서 비활성화되어 있어 직선형 변주가 중심입니다.
- 장애물 Actor 클래스가 모두 남아 있더라도 모든 타입이 현재 Chunk Spawn Table에서 활성화된 것은 아닙니다.
- `ProjectSettings.ini`의 기본 시작 Scene은 `Title`, 기본 Game Mode는 `AGameModeBase`이며 실제 게임 흐름은 Scene과 Lua Script에서 연결됩니다.
- 2026.08.19에 저장소 보존을 위해 누락된 Basic Shape·Spaceship·SlideOrJump OBJ Asset을 후속 복구했습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [Rocketstein/Jungle_Week9_Team6](https://github.com/Rocketstein/Jungle_Week9_Team6)에서 확인할 수 있습니다.
- 원본은 Week 8 저장소에서 Fork되었으므로 2026.04.30 이전 이력은 Week 9 개인 기여 집계에서 제외했습니다.
- 핵심 개발 기간에 Rocketstein 작성자 정보로 기록된 84개 커밋에는 Merge·Checkpoint가 포함되어 있어, 위 목록은 메시지와 실제 변경 파일 및 최종 코드를 함께 확인한 대표 커밋만 제시합니다.
- 후속 자산 복구 커밋: [`c751f11f`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/c751f11f828df289ec9e9bc37d08a0ec26dfbc33), [`76f5df27`](https://github.com/Rocketstein/Jungle_Week9_Team6/commit/76f5df27688ac872105712e776fcf95c1ed58802)