<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 14 — Krafton Engine Final Game: Game Flow, Lock-On & Encounter Direction

> A Sekiro-inspired action-combat game built with a custom DirectX 11 engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the Week 14 results of the [original collaborative project](https://github.com/Chanil-Chong/Jungle_Week14_Team6).

## Project Overview

This project integrates the rendering, animation, physics, particle, audio, and Lua scripting systems developed in Krafton Engine through Week 13 into a standalone game. The player fights ordinary enemies in a Japanese-inspired battlefield before a Blood Moon transition and boss introduction lead into the final encounter.

The in-game credits list my role as `Game Flow & Transition`. A review of Rocketstein-authored commits on the final `main` branch shows that the implementation also covers the lock-on system, title and controls screens, Blood Moon and boss encounters, enemy-spawn effects, and several engine-integration fixes.

- **Development period:** June 2026 (Week 14)
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, HLSL, C++, Lua/sol2, RmlUi, FMOD, PhysX, NvCloth
- **Areas of responsibility:** Game flow, lock-on, UI and scene transitions, encounter direction, and gameplay integration
- **Project type:** Four-person team project / portfolio focused on individual contributions

## Key Features

- Action combat built around movement, attacks, guard/deflect, posture, and executions
- Lock-on with target acquisition and switching, camera tracking, and an on-screen marker
- Pause, death, revive, victory, and a file-backed leaderboard
- Connected title, options, controls, credits, and gameplay scenes
- Opening enemy spawns, a Blood Moon phase, and a boss introduction
- Combat feedback using particles, BGM, camera shake, and hit reactions
- Keyboard/mouse and XInput gamepad controls
- Standalone game builds and release packaging

## My Contributions

### 1. Game Phases and Pause, Death, Revive, and Victory Flow

- Added `AFinaleGameMode` and `AFinaleGameState` with Playing, Paused, CutScene, Dead, Defeated, Victory, GameOver, and Leaderboard phases
- Implemented pause transitions and Lua bindings, then integrated RmlUi overlays so runtime UI follows the active phase
- Separated ordinary pause from a soft pause in which cinematic and UI Actors continue ticking, preserving input and transitions on the Defeated screen
- Synchronised death-camera fades, Death Icon alpha, `GIVE IN`, and the transition to true death
- Restored player vitals, fade-in and icon effects, and player control during revive
- Entered the Victory phase after the boss was defeated and recorded active time spent in Playing plus revive count as the score
- Implemented a top-six leaderboard prioritising faster clear times and fewer revives, together with a three-letter initials-entry UI

Representative implementations: [`AFinaleGameMode`](./KraftonEngine/Source/Game/GameMode/AFinaleGameMode.cpp), [`AFinaleGameState`](./KraftonEngine/Source/Game/GameMode/GameState.cpp), [`GameSoftPauseState`](./KraftonEngine/Source/Game/GameMode/GameSoftPauseState.h), [`GameFlowController.lua`](./KraftonEngine/Content/Script/Game/GameFlowController.lua), [`LeaderboardStore`](./KraftonEngine/Source/Game/Leaderboard/LeaderboardStore.h)

### 2. Lock-On Targeting System

- Implemented new `ULockOnComponent` and `ULockOnMarkerComponent` classes and connected lock-on input to the character's Lua command pipeline
- Searched for candidates by distance and screen direction, acquiring and releasing only valid targets
- Interpolated camera rotation, spring-arm length, and target offset during lock-on to maintain a useful combat view
- Switched to adjacent targets using mouse or right-stick directional input and automatically released targets whose HP reached zero
- Added a world-space marker material and dedicated `GameOverlayPass` to render the lock-on marker over the combat scene
- Fixed marker alpha and transparent render-order issues and installed the Component on the player in the final `GamePlay.Scene`

Representative implementations: [`LockOnComponent`](./KraftonEngine/Source/Game/Components/LockOnComponent.cpp), [`LockOnMarkerComponent`](./KraftonEngine/Source/Game/Components/LockOnMarkerComponent.cpp), [`CharacterLockOn.lua`](./KraftonEngine/Content/Script/FinalGameJamScript/Character/CharacterLockOn.lua), [`GameOverlayPass`](./KraftonEngine/Source/Engine/Render/RenderPass/GameOverlayPass.cpp)

### 3. Title, Controls, Scene Fades, and BGM State

- Built `GameTitle.Scene` and an RmlUi title menu with Start, Options, Controls, Credits, and Exit actions
- Applied title-logo and button assets and refined hover states, spacing, layout, and the Credits scene
- Created a Controls help page covering keyboard/mouse and gamepad input
- Added missing gamepad input for pause and execution and standardised the revive key as `Q`
- Implemented an RmlUi black-sheet scene-fade module for transitions before and after Title → Gameplay
- Designed `BGMState` to own one named looping channel and prevent duplicate playback of title, battle, and boss tracks
- Synchronised scene fades with BGM volume and prevented music from restarting when returning from Credits to the title
- Fixed nested scene-path handling in Project Settings and set the starting scene to `Game/GameTitle`

Representative implementations: [`TitleMenu.lua`](./KraftonEngine/Content/Script/Game/TitleMenu.lua), [`SceneTransition.lua`](./KraftonEngine/Content/Script/Game/SceneTransition.lua), [`BGMState.lua`](./KraftonEngine/Content/Script/Game/BGMState.lua), `Controls.rml`

### 4. Blood Moon, Boss Introduction, and Enemy-Spawn Direction

- Monitored the opening enemies and started the Blood Moon phase exactly once after they had all been removed
- Interpolated spotlight and height-fog colours and activated a Blood Moon billboard and particles to transform the battlefield atmosphere
- Faded out the battle BGM, transitioned to the boss track, and invoked the Boss Intro Director
- Kept the boss hidden at a preparation stage outside the arena until the introduction, then restored its location and visibility
- Synchronised Blood Moon visuals, player-input freeze, boss spawn effects, weapon visibility, and walk animation during the cinematic
- Activated the boss encounter and HUD when the introduction ended, then handed boss death to the Victory flow
- Added `UEnemySpawnEffectComponent`, a particle asset, and an Opening Director that applies per-enemy spawn delays
- Placed the existing engine cinematic camera in `GamePlay.Scene` and connected it to the enemy-spawn sequence and playback completion

Representative implementations: [`BloodMoonPhase.lua`](./KraftonEngine/Content/Script/Game/BloodMoonPhase.lua), [`BossIntroDirector.lua`](./KraftonEngine/Content/Script/Game/BossIntroDirector.lua), [`IntroSpawnDirector.lua`](./KraftonEngine/Content/Script/Game/IntroSpawnDirector.lua), [`EnemySpawnEffect`](./KraftonEngine/Source/Game/Components/EnemySpawnEffect.cpp)

### 5. Gameplay, Rendering, and Physics Integration Fixes

- Implemented a Kill Volume that triggers immediate true death for the player and normal kill handling for enemies
- Connected camera shake to damage reactions and added `AddImpulseToBoneAtLocation` for position-based ragdoll impulses
- Changed static-mesh physics queries to use actual vertex geometry rather than bounding boxes
- Prevented physics ticks from running before mesh cooking completed
- Moved the transparent pass later in the rendering pipeline and prevented particles from creating unnecessary shadow passes
- Extended FBX importer and material integration to handle masked quads and specular reflections correctly
- Corrected final-scene alpha, ordering, and visibility issues for the lock-on icon, Blood Moon particles, and enemy-spawn particles

## Game-Flow Architecture

```text
GameTitle.Scene
 Title / Options / Controls / Credits
        │ Start
        ▼
Scene Fade-Out + BGM Ducking
        │
        ▼
GamePlay.Scene
 Intro Cinematic + Staggered Enemy Spawn
        │
        ▼
Playing ── ESC / Start ── Paused
   │
   ├─ Player Death ──▶ Dead ── Q Revive ──▶ Playing
   │                         └─ Give In / Full Fade ──▶ Defeated
   │
   └─ Minor Enemies Cleared
          │
          ▼
      Blood Moon Phase
      Lighting / Fog / Particles / BGM
          │
          ▼
      Boss Intro → Boss Encounter
          │ Boss Slain
          ▼
        Victory → Score Submit / Leaderboard / Title
```

`AFinaleGameMode` owns phases and runtime authority, while `GameFlowController.lua` reads the current phase and drives UI and fade effects. `SceneTransition` and `BGMState` coordinate the visual and audio transition between Title and Gameplay.

## Lock-On Architecture

```text
Middle Mouse / R3
        │
        ▼
ULockOnComponent
  ├─ Distance/Angle-Based Target Search
  ├─ Mouse / Right-Stick Target Switching
  ├─ Camera Rotation / Spring-Arm Interpolation
  └─ Release on Target Death or Excess Distance
        │
        ▼
ULockOnMarkerComponent
        │
        ▼
GameOverlayPass → Lock-On Marker
```

## Controls

| Action | Keyboard / Mouse | Gamepad |
| --- | --- | --- |
| Move | `W` / `A` / `S` / `D` | Left Stick |
| Camera | Mouse | Right Stick |
| Attack | Left Mouse | `RB` |
| Guard / Deflect | Hold / tap Right Mouse | Hold / tap `LB` |
| Execution | `E` | `RT` |
| Lock-on | Middle Mouse | `R3` |
| Switch target | Move mouse left/right | Move Right Stick left/right |
| Pause | `Esc` | Start |
| Revive | `Q` | — |

## Project Structure

```text
.
├─ KraftonEngine.sln
├─ KraftonEngine/
│  ├─ Content/
│  │  ├─ Game/UI/                         # Title, controls, pause, death, victory
│  │  ├─ Scene/Game/                      # GameTitle, GamePlay, GameCredits
│  │  └─ Script/Game/                     # Flow, transitions, encounter directors
│  ├─ Source/
│  │  ├─ Engine/Render/RenderPass/         # GameOverlay and transparent pipeline
│  │  └─ Game/
│  │     ├─ Components/                   # LockOn, marker, EnemySpawnEffect
│  │     ├─ GameMode/                     # Finale GameMode and GameState
│  │     ├─ Leaderboard/
│  │     └─ Lua/GameLuaBindings.*
│  ├─ Shaders/
│  └─ ThirdParty/                         # Lua, RmlUi, FMOD, FBX, PhysX, NvCloth
├─ GenerateProjectFiles.bat
├─ GameBuild.bat
├─ PackageRelease.bat
└─ ReleaseBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore

The project uses the NuGet package `directxtk_desktop_win10`, together with repository-provided Lua/sol2, RmlUi, FMOD, FBX SDK, PhysX, and NvCloth libraries.

### Build Instructions

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `KraftonEngine.sln` in Visual Studio.
3. Restore NuGet packages.
4. Build `Debug | x64` or `Release | x64` to inspect the editor, or `Game | x64` for the standalone game.

The standalone game starts from the `Game/GameTitle` scene configured in `ProjectSettings.ini`. Use `GameBuild.bat` for a game build and `PackageRelease.bat` or `ReleaseBuild.bat` for release packaging.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 14 — Krafton Engine Final Game: Game Flow · Lock-on · Encounter Direction

> DirectX 11 기반 커스텀 엔진으로 제작한 Sekiro 스타일 액션 전투 게임입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/Chanil-Chong/Jungle_Week14_Team6)의 Week 14 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 13까지 개발한 Krafton Engine의 Rendering, Animation, Physics, Particle, Audio와 Lua Scripting 기능을 하나의 Standalone Game으로 통합했습니다. 플레이어는 일본풍 전장에서 일반 적과 전투하고, Blood Moon 연출과 Boss Intro를 거쳐 최종 전투에 진입합니다.

게임 내 Credits에는 담당 역할이 `Game Flow & Transition`으로 표기되어 있습니다. 실제 `main` 브랜치의 Rocketstein 작성 커밋을 다시 확인한 결과, Game Phase와 UI Flow뿐 아니라 Lock-on System, Title·Controls, Blood Moon·Boss Encounter, Enemy Spawn Effect와 여러 Engine Integration Fix를 구현했습니다.

- **개발 기간:** 2026.06 (Week 14)
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, C++, Lua·sol2, RmlUi, FMOD, PhysX, NvCloth
- **담당 영역:** Game Flow, Lock-on, UI·Scene Transition, Encounter Direction, Gameplay Integration
- **프로젝트 형태:** 4인 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- 이동, 공격, Guard·Deflect, Posture와 Execution 기반 Action Combat
- Target 탐색·전환, Camera 추적과 Marker 표시를 포함한 Lock-on
- Pause, Death, Revive, Victory와 File-backed Leaderboard
- Title, Options, Controls, Credits와 Gameplay Scene 연결
- Opening Enemy Spawn, Blood Moon Phase와 Boss Intro
- Particle, BGM, Camera Shake와 Hit Reaction을 활용한 Combat Feedback
- Keyboard·Mouse 및 XInput Gamepad 조작
- Standalone Game Build와 Release Packaging

## 담당 작업

### 1. Game Phase와 Pause·Death·Revive·Victory Flow

- `AFinaleGameMode`와 `AFinaleGameState`를 추가하고 Playing, Paused, CutScene, Dead, Defeated, Victory, GameOver, Leaderboard Phase 구성
- Pause 전환과 Lua Binding을 구현하고 Phase를 기준으로 RmlUi Overlay가 Runtime 상태를 따라가도록 통합
- 일반 Pause와 연출·UI Actor는 계속 Tick하는 Soft Pause를 분리해 Defeated 화면에서도 UI 입력과 전환 유지
- 사망 Camera Fade, Death Icon Alpha, `GIVE IN`과 True Death 전환 시점을 동기화
- Revive 시 Player Vital을 초기화하고 Fade-in·Icon 연출과 Player Control 복구 연결
- Boss 처치 후 Victory Phase로 전환하고 Playing 상태의 Active Time과 Revive Count를 점수로 기록
- 빠른 Clear Time과 적은 Revive 횟수를 우선하는 Top-6 Leaderboard 및 3-letter Initial 입력 UI 구현

대표 구현: [`AFinaleGameMode`](./KraftonEngine/Source/Game/GameMode/AFinaleGameMode.cpp), [`AFinaleGameState`](./KraftonEngine/Source/Game/GameMode/GameState.cpp), [`GameSoftPauseState`](./KraftonEngine/Source/Game/GameMode/GameSoftPauseState.h), [`GameFlowController.lua`](./KraftonEngine/Content/Script/Game/GameFlowController.lua), [`LeaderboardStore`](./KraftonEngine/Source/Game/Leaderboard/LeaderboardStore.h)

### 2. Lock-on Targeting System

- `ULockOnComponent`와 `ULockOnMarkerComponent`를 신규 구현하고 Character Lua Command Pipeline에 Lock-on 입력 연결
- 거리와 화면 방향을 기준으로 후보를 탐색하고 유효 Target을 획득·해제하도록 구성
- Lock-on 중 Camera Rotation, Spring Arm 길이와 Target Offset을 보간해 전투 시야 유지
- Mouse·Right Stick 방향 입력으로 인접 Target을 전환하고 Target HP가 0이 되면 Lock-on 자동 해제
- World-space Marker Material과 전용 `GameOverlayPass`를 추가해 전투 Scene 위에 Lock-on 표식 출력
- Marker Alpha와 Transparent Render Order 문제를 수정하고 최종 `GamePlay.Scene`의 Player에 Component 배치

대표 구현: [`LockOnComponent`](./KraftonEngine/Source/Game/Components/LockOnComponent.cpp), [`LockOnMarkerComponent`](./KraftonEngine/Source/Game/Components/LockOnMarkerComponent.cpp), [`CharacterLockOn.lua`](./KraftonEngine/Content/Script/FinalGameJamScript/Character/CharacterLockOn.lua), [`GameOverlayPass`](./KraftonEngine/Source/Engine/Render/RenderPass/GameOverlayPass.cpp)

### 3. Title·Controls·Scene Fade와 BGM State

- `GameTitle.Scene`과 RmlUi 기반 Title Menu를 구성하고 Start, Options, Controls, Credits, Exit 동작 연결
- Title Logo와 Button Asset을 적용하고 Hover 상태, 간격, 배치와 Credits Scene을 정리
- Keyboard·Mouse와 Gamepad 조작법을 안내하는 Controls Help Page 제작
- Pause와 Execution의 누락된 Gamepad Input을 추가하고 Revive Key를 `Q`로 통일
- RmlUi Black Sheet 기반 Scene Fade Module을 작성해 Title → Gameplay 전환 전후 Fade 처리
- `BGMState`가 Named Loop Channel 하나를 관리하도록 구성해 Title·Battle·Boss Track의 중복 재생 방지
- Scene Fade와 BGM Volume을 동기화하고 Credits에서 Title로 복귀할 때 음악이 재시작되지 않도록 처리
- Project Settings가 중첩된 Scene Path를 인식하도록 수정하고 시작 Scene을 `Game/GameTitle`로 설정

대표 구현: [`TitleMenu.lua`](./KraftonEngine/Content/Script/Game/TitleMenu.lua), [`SceneTransition.lua`](./KraftonEngine/Content/Script/Game/SceneTransition.lua), [`BGMState.lua`](./KraftonEngine/Content/Script/Game/BGMState.lua), `Controls.rml`

### 4. Blood Moon·Boss Intro와 Enemy Spawn 연출

- 일반 Enemy가 등장한 뒤 모두 제거됐는지 감시해 Blood Moon Phase가 한 번만 시작되도록 구성
- Spotlight·Height Fog 색상을 보간하고 Blood Moon Billboard·Particle을 활성화해 전장 분위기 전환
- Battle BGM을 Fade-out한 뒤 Boss BGM으로 넘기고 Boss Intro Director 호출
- Boss를 Intro 전까지 Arena 밖의 Prep Stage에 숨겨 두고 Phase 시작 시 위치·표시 상태 복구
- Cinematic 동안 Blood Moon 표시, Player Input Freeze, Boss Spawn Effect·Weapon·Walk Animation을 동기화
- Intro 종료 시 Boss Encounter와 HUD를 활성화하고 Boss 사망 시 Victory Flow로 인계
- `UEnemySpawnEffectComponent`와 Particle Asset을 추가하고 Enemy별 Spawn Delay를 사용하는 Opening Director 구현
- 기존 Engine의 Cinematic Camera를 `GamePlay.Scene`에 배치하고 Enemy Spawn Sequence와 재생 종료 시점을 연결

대표 구현: [`BloodMoonPhase.lua`](./KraftonEngine/Content/Script/Game/BloodMoonPhase.lua), [`BossIntroDirector.lua`](./KraftonEngine/Content/Script/Game/BossIntroDirector.lua), [`IntroSpawnDirector.lua`](./KraftonEngine/Content/Script/Game/IntroSpawnDirector.lua), [`EnemySpawnEffect`](./KraftonEngine/Source/Game/Components/EnemySpawnEffect.cpp)

### 5. Gameplay·Rendering·Physics 통합 수정

- Kill Volume을 구현하고 Player는 즉시 True Death, Enemy는 Kill 처리하도록 Actor Type별 분기
- 피격 시 Camera Shake를 연결하고 `AddImpulseToBoneAtLocation`을 추가해 위치 기반 Ragdoll 충격 지원
- Static Mesh Physics Query가 Bounding Box가 아닌 실제 Vertex Geometry를 사용하도록 수정
- Mesh Cooking이 끝나기 전에 Physics Tick이 실행되는 문제를 방지
- Transparent Pass를 Rendering Pipeline 후반으로 이동하고 Particle이 불필요한 Shadow Pass를 생성하는 문제 수정
- Masked Quad와 Specular Reflection을 올바르게 처리하도록 FBX Importer 및 Material 통합 보완
- Lock-on Icon, Blood Moon Particle, Enemy Spawn Particle의 Alpha·순서·표시 문제를 최종 Scene에서 수정

## Game Flow 구조

```text
GameTitle.Scene
 Title / Options / Controls / Credits
        │ Start
        ▼
Scene Fade-out + BGM Ducking
        │
        ▼
GamePlay.Scene
 Intro Cinematic + Staggered Enemy Spawn
        │
        ▼
Playing ── ESC / Start ── Paused
   │
   ├─ Player Death ──▶ Dead ── Q Revive ──▶ Playing
   │                         └─ Give In / Full Fade ──▶ Defeated
   │
   └─ Minor Enemies Cleared
          │
          ▼
      Blood Moon Phase
      Lighting / Fog / Particle / BGM
          │
          ▼
      Boss Intro → Boss Encounter
          │ Boss Slain
          ▼
        Victory → Score Submit / Leaderboard / Title
```

`AFinaleGameMode`가 Phase와 Runtime 권한을 관리하고, `GameFlowController.lua`는 현재 Phase를 읽어 UI와 Fade 연출을 구동합니다. Title과 Gameplay 사이의 시각·청각 전환은 `SceneTransition`과 `BGMState`가 함께 처리합니다.

## Lock-on 구조

```text
Mouse Middle / R3
        │
        ▼
ULockOnComponent
  ├─ Distance·Angle 기반 Target 탐색
  ├─ Mouse / Right Stick Target 전환
  ├─ Camera Rotation·Spring Arm 보간
  └─ Target 사망·거리 이탈 시 해제
        │
        ▼
ULockOnMarkerComponent
        │
        ▼
GameOverlayPass → Lock-on Marker
```

## 조작 방법

| 구분 | Keyboard / Mouse | Gamepad |
| --- | --- | --- |
| 이동 | `W` / `A` / `S` / `D` | Left Stick |
| Camera | Mouse | Right Stick |
| 공격 | Left Mouse | `RB` |
| Guard / Deflect | Right Mouse Hold / Tap | `LB` Hold / Tap |
| Execution | `E` | `RT` |
| Lock-on | Middle Mouse | `R3` |
| Target 전환 | Mouse 좌우 이동 | Right Stick 좌우 |
| Pause | `Esc` | Start |
| Revive | `Q` | — |

## 프로젝트 구조

```text
.
├─ KraftonEngine.sln
├─ KraftonEngine/
│  ├─ Content/
│  │  ├─ Game/UI/                         # Title, Controls, Pause, Death, Victory
│  │  ├─ Scene/Game/                      # GameTitle, GamePlay, GameCredits
│  │  └─ Script/Game/                     # Flow·Transition·Encounter Director
│  ├─ Source/
│  │  ├─ Engine/Render/RenderPass/         # GameOverlay·Transparent Pipeline
│  │  └─ Game/
│  │     ├─ Components/                   # LockOn·Marker·EnemySpawnEffect
│  │     ├─ GameMode/                     # Finale GameMode·GameState
│  │     ├─ Leaderboard/
│  │     └─ Lua/GameLuaBindings.*
│  ├─ Shaders/
│  └─ ThirdParty/                         # Lua, RmlUi, FMOD, FBX, PhysX, NvCloth
├─ GenerateProjectFiles.bat
├─ GameBuild.bat
├─ PackageRelease.bat
└─ ReleaseBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore

프로젝트는 NuGet의 `directxtk_desktop_win10`과 저장소 내 Lua·sol2, RmlUi, FMOD, FBX SDK, PhysX, NvCloth Library를 사용합니다.

### 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio Project File을 생성합니다.
2. `KraftonEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet Package를 복원합니다.
4. Editor 확인은 `Debug | x64` 또는 `Release | x64`, Standalone 실행은 `Game | x64`로 빌드합니다.

Standalone Game은 `ProjectSettings.ini`의 `Game/GameTitle` Scene에서 시작합니다. 게임 빌드는 `GameBuild.bat`, 배포 Package는 `PackageRelease.bat` 또는 `ReleaseBuild.bat`으로 구성할 수 있습니다.
