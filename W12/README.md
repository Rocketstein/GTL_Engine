<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 12 — Krafton Engine: Cascade Particle System

> A project that adds Cascade-style particle simulation, rendering, and a dedicated editor to a custom DirectX 11 engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/Rocketstein/Jungle_Week12_Team2).

## Project Overview

Building on the Week 11 engine, we implemented a Cascade-style particle system organised as `ParticleSystem → Emitter → LODLevel → Module`. Particles are simulated on the CPU and converted into render snapshots. Scene proxies then pack those snapshots into vertex, index, and instance data for sprite, mesh, beam, or ribbon rendering before submitting them to DirectX 11 render passes.

The final team project supports modular emitters, LOD, a particle editor, collision and events, sprite/mesh/beam/ribbon rendering, and profiling.

- **Core development period:** 22–28 May 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, CPU particle simulation, GPU instancing, Dear ImGui
- **Areas of responsibility:** Particle render bridge, sprite and mesh packing, sorting, screen alignment, blend routing, and beam particles
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- Cascade-style asset hierarchy built from `UParticleSystem`, emitters, LOD levels, and modules
- Spawn-rate and burst control, lifetime, location, velocity, acceleration, rotation, size, and colour modules
- Constant, uniform, and curve-based float/vector distributions
- Sprite, instanced-mesh, beam, and ribbon particle rendering
- SubUV flipbooks, six screen-alignment modes, and per-particle rotation
- Particle- and emitter-level sorting with opaque, alpha-blended, and additive paths
- Beam source, target, noise, tangent curves, tapering, texture tiling, and multiple sheets
- Mesh and ribbon TypeData, orbit, and collision modules
- Spawn, death, collision, and burst events with event receivers
- Distance-based LOD, emitter/module insertion, deletion and reordering, and a curve editor
- Particle profiling across CPU simulation, packing, and upload stages
- Particle-asset serialisation, a preview viewport, and scene placement

## My Contributions

### 1. Snapshot Bridge Between CPU Simulation and the Renderer

- Implemented `FParticleDataContainer` to copy raw particle data and active indices from CPU emitters into a one-frame snapshot
- Allocated particle data as a single 16-byte-aligned memory block, placing the index region after the same block
- Disabled copying and provided move construction, move assignment, and `Free` on destruction to make snapshot ownership explicit
- Defined per-type replay data and render-side dynamic-data contracts for sprites, meshes, beams, and ribbons
- Connected `UParticleSystemComponent` simulation output to `FParticleSystemSceneProxy::UpdateDynamicData`
- Configured the proxy to release the previous frame's snapshot, take ownership of the new one, and update mesh, material, and per-viewport state

Representative commits: [`65f128d0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/65f128d08682ed130540ae15ddf9b926ba33384c), [`1b2999ff`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/1b2999ff7cf06f9544a9d460a057b2094e71786b), [`c0095688`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/c0095688ce691c013707cc101661e4c038f51c89)

### 2. Sprite Packing and GPU Instancing for Mesh Particles

- Expanded each sprite into four camera-facing quad vertices and six indices, packing them into shared dynamic buffers
- Allowed multiple sprite emitters to share one buffer while retaining per-emitter `FirstIndex / IndexCount` sections
- Regenerated the shared index pattern only when the maximum particle count increased and otherwise updated primarily vertex data each frame
- Reduced repeated allocation and uploads through pre-`reserve`, empty-emitter early-outs, and conditional dirty flags
- Packed per-mesh-particle transforms, colours, and dynamic parameters into an instance buffer
- Added a second vertex buffer, instance stride/count/start, and a `DrawIndexedInstanced` path to `FDrawCommand`
- Propagated LOD levels in mesh-emitter snapshots and bound static-mesh geometry together with instance data

Representative commits: [`65f128d0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/65f128d08682ed130540ae15ddf9b926ba33384c), [`b483c6ac`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/b483c6aca622d144b5b46045b32b7e00f96d96f8), [`559ab7b2`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/559ab7b2eb33c95ad9a623f1333e54bdc1757ffd), [`2c3dbc2b`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/2c3dbc2b130ad4d24c1132da560a11a624550a2b)

### 3. Particle/Emitter Sorting, Screen Alignment, and Blend Routing

- Defined sorting interfaces based on view-projection depth, camera distance, and particle age
- Sorted active sprite and mesh particle indices per view to produce back-to-front rendering for transparent particles
- Implemented cross-emitter priority by stable-sorting a separate section-index map rather than mutating the physical emitter array
- Rebuilt emitter order and section draws only when `SortPriority` changed through a dirty flag
- Implemented Square, Rectangle, Velocity, Away From Center, Type Specific, and Facing Camera Position alignment modes in HLSL
- Added particle rotation, SubUV frame calculation, and texture-alpha/luminance-based alpha threshold and power controls
- Routed each emitter to the opaque pass or the alpha/additive states of the alpha-blend pass according to material blend mode
- Fixed billboard winding, horizontal sprite inversion, and transparency-clipping errors

Representative commits: [`94fa4bb7`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/94fa4bb7e80071b689870d8c753560a3b6f175cd), [`202826c0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/202826c09973b50ac6cd818653b94a59c7acc27f), [`94f7bc81`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/94f7bc81370c79fa4dc696146983347ddb52f3e5), [`affef204`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/affef20482c6b382dfedaec3e8e1c1ef99b8b1e4)

### 4. Beam TypeData, Runtime Instances, and Dynamic Geometry

- Adapted Cascade's `ParticleModuleTypeDataBeam2` structure to the engine's reflection, LOD, and serialisation systems
- Added distance/target modes, interpolation points, maximum beam count, travel speed, width, texture tiling, and multiple-sheet controls
- Generated source/target positions, tangent and strength, colour and alpha, width and taper, and progress as replay data for each beam particle
- Built camera-facing quad strips from source/target tangent curves and intermediate noise points
- Crossed multiple sheets and packed per-segment vertices and indices into a shared dynamic beam buffer
- Applied full/partial tapering and distance-based texture tiling
- Maintained `bAlwaysOn` beam counts and updated sibling-emitter endpoints and beam progress
- Connected beam shaders, a default material, smoke tests, and particle-asset save/load

Representative commits: [`4e3bac5c`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/4e3bac5c9ad86298b49e5b6a96469674ef17e81d), [`a938b950`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/a938b950fc23d15c48ef6f71beb87b063e24ee9e)

### 5. Beam Source, Target, and Noise Modules with Rendering Integration

- Implemented a reflectable beam-module base and source, target, and noise modules as UObjects
- Resolved source and target positions in Default, User Set, and Sibling Emitter modes, including absolute/local-space conversion
- Stored endpoint/tangent locks and user-defined tangents and strengths in particle payloads, updating them during spawn and tick
- Generated intermediate beam noise and target offsets from frequency, range, speed, and lock time
- Interpolated noise offsets over time and combined them with Hermite-form curves to create an animated beam centreline
- Stored noise data in per-emitter arenas and reconnected per-particle-slot slices to preserve pointer stability
- Exposed beam modules and TypeData choices in the Particle Editor
- Added a viewport show flag and render-collector gate for global particle visibility

Representative commits: [`a938b950`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/a938b950fc23d15c48ef6f71beb87b063e24ee9e), [`d100167e`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/d100167e70dbed9bd25bde9b52101daf52dbc7b0)

## Particle Simulation and Rendering Architecture

```text
UParticleSystem Asset
        │
        ├─ UParticleEmitter
        │      └─ UParticleLODLevel
        │             ├─ Required / Spawn
        │             ├─ Lifetime / Location / Velocity
        │             ├─ Size / Colour / Orbit / Collision
        │             └─ TypeData: Sprite / Mesh / Beam / Ribbon
        │
        ▼
UParticleSystemComponent
 FParticleEmitterInstance CPU Simulation
        │
        ▼
FDynamicEmitterReplayData
 Particle Data + Active Indices + Render Settings
        │
        ▼
FParticleSystemSceneProxy
        │
        ├─ SpritePacker ── Quad Vertices / Shared Index Pattern
        ├─ MeshPacker ──── Instance Buffer
        ├─ BeamPacker ──── Camera-Facing Multi-Sheet Strip
        └─ RibbonPacker ── Trail Strip
        │
        ▼
Emitter Priority → Particle Sort → Blend Route
        │
        ▼
DrawCommand
 DrawIndexed / DrawIndexedInstanced
        │
        ▼
Opaque or AlphaBlend Render Pass
```

A frame snapshot separates CPU simulation from the render proxy, so the renderer does not directly traverse UObject or module state. Sprites, beams, and ribbons pack view-dependent geometry on the CPU; mesh particles reuse static geometry and update only instance data.

## Particle Editor Workflow

1. Create or open a Particle System asset in the Content Drawer.
2. Add an emitter and select Sprite, Mesh, Beam, or Ribbon TypeData.
3. Combine Required, Spawn, Lifetime, Location, Velocity, Size, Colour, and other modules.
4. Add LOD levels when needed and edit their distance-specific module settings.
5. Play or restart the simulation in the preview viewport and inspect the result.
6. Edit distribution curves in the Curve panel.
7. Reorder, duplicate, or delete emitters and modules, then save the asset.
8. Assign the asset as the template of a `UParticleSystemComponent` to use it in a scene.
9. Toggle all particle rendering through the viewport Particle show flag.

## Project Structure

```text
.
├─ KraftonEngine.sln
├─ Docs/
│  ├─ Particle_render_plan.md
│  └─ W12_particle/                    # Cascade architecture and rendering research
├─ KraftonEngine/
│  ├─ Asset/
│  │  ├─ Materials/Editor/             # Sprite, mesh, beam, and ribbon materials
│  │  └─ Particle/                     # Particle assets and textures
│  ├─ Shaders/Particle/                # Per-type HLSL
│  └─ Source/
│     ├─ Editor/UI/Asset/
│     │  └─ ParticleEditorWidget.*
│     └─ Engine/
│        ├─ Component/ParticleSystemComponent.*
│        ├─ Particle/
│        │  ├─ BeamModule/              # Source, target, noise
│        │  ├─ TypeData/                # Beam and ribbon
│        │  └─ Particle*                # Asset, LOD, module, instance
│        ├─ Profiling/ParticleStats.*
│        └─ Render/
│           ├─ Particle/ParticleDynamicData.*
│           └─ Proxy/ParticleSystemSceneProxy.*
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

The project uses the NuGet packages `directxtk_desktop_win10` and `NVIDIA.PhysX`, together with repository-provided Lua, RmlUi, FMOD, and FBX SDK libraries.

### Build Instructions

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `KraftonEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build and run `Debug | x64` or `Release | x64`.

Use `GameBuild.bat` for a game-runtime build and `ReleaseBuild.bat` for a distributable build.

## Notes

- The complete collaboration history and team-wide changes are available in [Rocketstein/Jungle_Week12_Team2](https://github.com/Rocketstein/Jungle_Week12_Team2).
- The original repository was forked from [keonwookang0914/Jungle_Week11_Team4](https://github.com/keonwookang0914/Jungle_Week11_Team4), so history before 22 May 2026 was excluded from Week 12 individual-contribution accounting.
- Of 1,924 file blobs, the previous private snapshot differed from the original `main` only in `KraftonEngine/Settings/Editor.ini` and `KraftonEngine/Settings/imgui.ini`; neither repository had a root README.
- The 26 top-level commits attributed to Rocketstein during the core development period include merges and squashes. Contributions were therefore identified by reviewing commit messages, changed files, and the final code together.
- Particle CPU simulation, the editor, LOD, ribbon rendering, collision/events, and final integration also include work by other team members.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 12 — Krafton Engine: Cascade Particle System

> DirectX 11 기반 커스텀 엔진에 Cascade 스타일 Particle Simulation, Rendering과 전용 Editor를 구축한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/Rocketstein/Jungle_Week12_Team2)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 11 엔진을 기반으로 `ParticleSystem → Emitter → LODLevel → Module` 계층을 갖는 Cascade 스타일 Particle System을 구현했습니다. CPU에서 Particle을 Simulation하고 Render Snapshot을 생성한 뒤, Scene Proxy가 Sprite / Mesh / Beam / Ribbon 유형에 맞는 Vertex·Index·Instance Data로 변환해 DirectX 11 Render Pass에 제출합니다.

최종 팀 결과물은 Module 조합, LOD, Particle Editor, Collision·Event, Sprite·Mesh·Beam·Ribbon Rendering과 Profiling을 지원합니다. 

- **핵심 개발 기간:** 2026.05.22 ~ 2026.05.28
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, CPU Particle Simulation, GPU Instancing, Dear ImGui
- **담당 영역:** Particle Render Bridge, Sprite / Mesh Packing, Sorting·Screen Alignment·Blend Routing, Beam Particle
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- `UParticleSystem`, Emitter, LOD Level과 Module 기반 Cascade형 Asset 구조
- Spawn Rate / Burst, Lifetime, Location, Velocity, Acceleration, Rotation, Size와 Color Module
- Constant / Uniform / Curve 기반 Float·Vector Distribution
- Sprite / Instanced Mesh / Beam / Ribbon Particle Rendering
- SubUV Flipbook, 6종 Screen Alignment와 Particle별 회전
- Particle 및 Emitter 단위 정렬과 Opaque / Alpha Blend / Additive 경로
- Beam Source / Target / Noise, Tangent Curve, Taper, Texture Tiling과 Multi-sheet
- Mesh·Ribbon TypeData, Orbit와 Collision Module
- Spawn / Death / Collision / Burst Event와 Event Receiver
- 거리 기반 LOD, Emitter·Module 추가·삭제·이동과 Curve Editor
- CPU Simulation·Packing·Upload 단계별 Particle Profiling
- Particle Asset 직렬화, Preview Viewport와 Scene 배치

## 담당 작업

### 1. CPU Simulation과 Renderer 사이의 Snapshot Bridge

- CPU Emitter의 Raw Particle Data와 Active Index를 한 Frame용 Snapshot으로 복사하는 `FParticleDataContainer` 구현
- Particle Data를 16-byte 정렬된 단일 Memory Block으로 할당하고 Index 영역을 같은 Block 뒤에 배치
- Copy를 금지하고 Move Constructor / Move Assignment, 소멸 시 `Free`를 제공해 Snapshot 소유권 명확화
- Sprite / Mesh / Beam / Ribbon별 Replay Data와 Render-side Dynamic Data Contract 구성
- `UParticleSystemComponent`가 Simulation 결과를 `FParticleSystemSceneProxy::UpdateDynamicData`로 넘기도록 연결
- Proxy가 이전 Frame Snapshot을 해제하고 새 데이터를 인수한 뒤 Mesh·Material·Per-viewport 상태를 갱신하도록 구성

대표 커밋: [`65f128d0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/65f128d08682ed130540ae15ddf9b926ba33384c), [`1b2999ff`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/1b2999ff7cf06f9544a9d460a057b2094e71786b), [`c0095688`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/c0095688ce691c013707cc101661e4c038f51c89)

### 2. Sprite Packing과 Mesh Particle GPU Instancing

- Sprite 한 개를 카메라 방향 Quad의 4개 Vertex와 6개 Index로 확장해 공용 Dynamic Buffer에 Packing
- 여러 Sprite Emitter가 하나의 Buffer를 공유하되 Emitter별 `FirstIndex / IndexCount` Section 유지
- 최대 Particle 수가 증가할 때만 공통 Index Pattern을 재생성하고 매 Frame에는 Vertex 중심으로 갱신
- 사전 `reserve`, 빈 Emitter Early-out과 Dirty Flag 조건화를 적용해 반복 할당·Upload 감소
- Mesh Particle별 Transform·Color·Dynamic Parameter를 Instance Buffer로 Packing
- `FDrawCommand`에 두 번째 Vertex Buffer, Instance Stride / Count / Start를 추가하고 `DrawIndexedInstanced` 경로 연결
- Mesh Emitter Snapshot에 LOD Level을 전달하고 Static Mesh Geometry와 Instance Data를 함께 Binding

대표 커밋: [`65f128d0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/65f128d08682ed130540ae15ddf9b926ba33384c), [`b483c6ac`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/b483c6aca622d144b5b46045b32b7e00f96d96f8), [`559ab7b2`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/559ab7b2eb33c95ad9a623f1333e54bdc1757ffd), [`2c3dbc2b`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/2c3dbc2b130ad4d24c1132da560a11a624550a2b)

### 3. Particle·Emitter 정렬, Screen Alignment와 Blend Routing

- View Projection Depth, Camera Distance와 Particle Age 기준 정렬 인터페이스 구성
- Sprite / Mesh의 Active Particle Index를 View별로 정렬해 투명 Particle의 Back-to-front Draw 순서 반영
- 물리적 Emitter 배열을 변경하지 않고 별도 Section Index Map을 Stable Sort하는 Cross-emitter Priority 구현
- `SortPriority` 변경 시에만 Emitter 순서와 Section Draw를 다시 구성하도록 Dirty Flag 연결
- Square / Rectangle / Velocity / Away From Center / Type Specific / Facing Camera Position 정렬을 HLSL에 구현
- Particle 회전과 SubUV Frame 계산, Texture Alpha / Luminance 기반 Alpha Threshold·Power 처리
- Emitter Material의 Blend Mode에 따라 Opaque Pass 또는 Alpha Blend Pass의 Alpha / Additive State로 Routing
- Billboard Winding, Sprite 좌우 반전과 Transparency Clipping 오류 수정

대표 커밋: [`94fa4bb7`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/94fa4bb7e80071b689870d8c753560a3b6f175cd), [`202826c0`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/202826c09973b50ac6cd818653b94a59c7acc27f), [`94f7bc81`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/94f7bc81370c79fa4dc696146983347ddb52f3e5), [`affef204`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/affef20482c6b382dfedaec3e8e1c1ef99b8b1e4)

### 4. Beam TypeData, Runtime Instance와 Dynamic Geometry

- Cascade의 `ParticleModuleTypeDataBeam2` 구조를 엔진 Reflection·LOD·Serialization 체계에 맞게 구성
- Distance / Target 방식, Interpolation Point, 최대 Beam 수, 진행 Speed, Width, Texture Tile과 Multi-sheet 설정
- Beam Particle마다 Source / Target, Tangent·Strength, Color·Alpha, Width·Taper와 진행률을 Replay Data로 생성
- Source / Target Tangent를 이용한 Curve와 중간 Noise Point를 기반으로 Camera-facing Quad Strip 생성
- 여러 Sheet를 교차 배치하고 Segment별 Vertex / Index를 공용 Dynamic Beam Buffer에 Packing
- Full / Partial Taper와 거리 기반 Texture Tiling 적용
- `bAlwaysOn` Beam 수 유지, Sibling Emitter Endpoint와 Beam 진행률 갱신
- Beam Shader, 기본 Material, Smoke Test와 Particle Asset 저장·불러오기 연결

대표 커밋: [`4e3bac5c`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/4e3bac5c9ad86298b49e5b6a96469674ef17e81d), [`a938b950`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/a938b950fc23d15c48ef6f71beb87b063e24ee9e)

### 5. Beam Source / Target / Noise Module과 렌더 통합

- Beam 전용 Module Base와 Source / Target / Noise Module을 Reflection 가능한 UObject로 구현
- Default, User Set, Sibling Emitter 방식의 Source / Target 좌표 해석과 Absolute / Local Space 변환
- Endpoint와 Tangent Lock, 사용자 Tangent·Strength 설정을 Particle Payload에 저장하고 Spawn / Update에서 갱신
- Frequency, Range, Speed와 Lock Time에 따라 Beam 중간점의 Noise Offset·Target Offset 생성
- Noise Offset을 시간에 따라 보간하고 Hermite-form Curve에 합성해 흔들리는 Beam Centerline 구성
- Noise Data를 Emitter별 Arena에 보관하고 Particle Slot별 Slice를 재연결해 Pointer 안정성 확보
- Beam Module을 Particle Editor의 추가 가능 Module과 TypeData 선택에 연결
- 전체 Particle 표시를 제어하는 Viewport Show Flag와 Render Collector Gate 추가

대표 커밋: [`a938b950`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/a938b950fc23d15c48ef6f71beb87b063e24ee9e), [`d100167e`](https://github.com/Rocketstein/Jungle_Week12_Team2/commit/d100167e70dbed9bd25bde9b52101daf52dbc7b0)

## Particle 실행·렌더링 구조

```text
UParticleSystem Asset
        │
        ├─ UParticleEmitter
        │      └─ UParticleLODLevel
        │             ├─ Required / Spawn
        │             ├─ Lifetime / Location / Velocity
        │             ├─ Size / Color / Orbit / Collision
        │             └─ TypeData: Sprite / Mesh / Beam / Ribbon
        │
        ▼
UParticleSystemComponent
 FParticleEmitterInstance CPU Simulation
        │
        ▼
FDynamicEmitterReplayData
 Particle Data + Active Indices + Render Settings
        │
        ▼
FParticleSystemSceneProxy
        │
        ├─ SpritePacker ── Quad Vertex / Shared Index Pattern
        ├─ MeshPacker ──── Instance Buffer
        ├─ BeamPacker ──── Camera-facing Multi-sheet Strip
        └─ RibbonPacker ── Trail Strip
        │
        ▼
Emitter Priority → Particle Sort → Blend Route
        │
        ▼
DrawCommand
 DrawIndexed / DrawIndexedInstanced
        │
        ▼
Opaque or AlphaBlend Render Pass
```

CPU Simulation과 Render Proxy 사이에는 Frame Snapshot을 두어 UObject·Module 상태를 Renderer가 직접 순회하지 않도록 분리했습니다. Sprite / Beam / Ribbon은 View에 따라 달라지는 Geometry를 CPU에서 Packing하고, Mesh Particle은 Static Geometry를 재사용하면서 Instance Data만 갱신합니다.

## Particle Editor 사용 흐름

1. Content Drawer에서 Particle System Asset을 생성하거나 엽니다.
2. Emitter를 추가하고 Sprite / Mesh / Beam / Ribbon TypeData를 선택합니다.
3. Required, Spawn, Lifetime, Location, Velocity, Size, Color 등의 Module을 조합합니다.
4. 필요하면 LOD를 추가하고 거리별 Module 설정을 편집합니다.
5. Preview Viewport에서 Simulation을 재생·재시작하며 결과를 확인합니다.
6. Curve Panel에서 Distribution Curve를 조정합니다.
7. Emitter와 Module을 이동·복제·삭제하고 Asset을 저장합니다.
8. `UParticleSystemComponent`의 Template으로 Asset을 지정해 Scene에서 사용합니다.
9. Viewport의 Particle Show Flag로 전체 Particle 표시를 전환합니다.

## 프로젝트 구조

```text
.
├─ KraftonEngine.sln
├─ Docs/
│  ├─ Particle_render_plan.md
│  └─ W12_particle/                    # Cascade 구조·렌더링 조사 문서
├─ KraftonEngine/
│  ├─ Asset/
│  │  ├─ Materials/Editor/             # Sprite / Mesh / Beam / Ribbon Material
│  │  └─ Particle/                     # Particle Asset·Texture
│  ├─ Shaders/Particle/                # 유형별 HLSL
│  └─ Source/
│     ├─ Editor/UI/Asset/
│     │  └─ ParticleEditorWidget.*
│     └─ Engine/
│        ├─ Component/ParticleSystemComponent.*
│        ├─ Particle/
│        │  ├─ BeamModule/              # Source / Target / Noise
│        │  ├─ TypeData/                # Beam / Ribbon
│        │  └─ Particle*                # Asset, LOD, Module, Instance
│        ├─ Profiling/ParticleStats.*
│        └─ Render/
│           ├─ Particle/ParticleDynamicData.*
│           └─ Proxy/ParticleSystemSceneProxy.*
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

프로젝트는 NuGet의 `directxtk_desktop_win10`, `NVIDIA.PhysX`와 저장소 내 Lua, RmlUi, FMOD, FBX SDK Library를 사용합니다.

### 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio 프로젝트 파일을 생성합니다.
2. `KraftonEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

게임 실행용 결과물은 `GameBuild.bat`, 배포용 결과물은 `ReleaseBuild.bat`으로 구성할 수 있습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [Rocketstein/Jungle_Week12_Team2](https://github.com/Rocketstein/Jungle_Week12_Team2)에서 확인할 수 있습니다.
- 원본은 [keonwookang0914/Jungle_Week11_Team4](https://github.com/keonwookang0914/Jungle_Week11_Team4)에서 Fork되었으므로 2026.05.22 이전 이력은 Week 12 개인 기여 집계에서 제외했습니다.
- 비공개 저장소의 기존 스냅샷은 원본 `main`과 1,924개 File Blob 중 `KraftonEngine/Settings/Editor.ini`, `KraftonEngine/Settings/imgui.ini` 두 파일만 달랐으며, 양쪽 모두 루트 README는 없었습니다.
- 핵심 개발 기간에 Rocketstein 작성자 정보로 기록된 26개 Top-level Commit에는 Merge·Squash Commit이 포함되어 있어, 담당 작업은 커밋 메시지뿐 아니라 실제 변경 파일과 최종 코드를 함께 확인해 정리했습니다.
- Particle CPU Simulation, Editor, LOD, Ribbon, Collision·Event와 최종 통합에는 팀원의 작업도 포함되어 있습니다.