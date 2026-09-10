<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 3 — ZZUP Engine

> An extension of the Week 2 engine featuring multi-scene editing, billboards, spotlights, SubUV animation, text rendering, and debug visualisation.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original team project](https://github.com/Rocketstein/Game-Tech-Lab-W3).

## Project Overview

Building on the existing DirectX 11 engine architecture, we created an editing environment capable of switching between and managing multiple scenes. We also added visualisation features required by an editor, including 2D textures, text, and lighting debug geometry.

- **Development period:** 19–26 March 2026
- **Development environment:** Windows, Visual Studio 2022, C++17
- **Key technologies:** DirectX 11, HLSL, Dear ImGui, JSON, DirectXTK
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- `FSceneManager` for creating, switching, and deleting multiple `UScene` instances
- UUID-based object serialisation and scene save/load support
- Lit, Unlit, and Wireframe view modes with render show flags
- Dynamic line batching for grids, AABBs, and spotlight debug rendering
- SubUV animation with sprite-sheet playback and an editing panel
- Camera-facing billboards and texture-resource loading
- Korean font atlases, GPU-instanced text rendering, and `UTextComponent`
- `FName`-based object naming with UUID display and editing
- Ray-casting picking, Ctrl multi-selection, and translation/rotation/scale gizmos
- ImGui-based Scene Manager, Inspector, camera, and view settings

## My Contributions

### 1. Week 3 Project Foundation and Build Stabilisation

- Prepared the Week 2 engine as the starting point for the Week 3 project and reorganised the solution and project structure
- Updated include paths and fixed build errors after moving engine files
- Resolved UTF-8 encoding issues affecting Korean comments and UI strings

### 2. Redesign of the World and Scene Architecture

- Extended `UWorld` to manage multiple `UScene` instances and the active scene through `FSceneManager`
- Separated Actor creation, ticking, and shutdown responsibilities at scene level
- Moved camera and gizmo ownership into `FEditorViewportClient`, reducing dependencies on the editor engine
- Connected camera, gizmo, and selection-state updates to scene creation, switching, and deletion
- Implemented a Scene Manager window and loaded-scene selection UI

### 3. Restoring and Extending Scene Save/Load

- Restored the existing JSON save/load functionality for the refactored world and scene architecture
- Serialised scenes, actors, and components by UUID, then reconnected parent, root-component, and owning-scene relationships after loading
- Added `.Scene` save/load support through file dialogs and prevented duplicate UUIDs
- Fixed stale viewport and gizmo references to objects from the previous scene after switching scenes

### 4. Billboard and Spotlight Foundations

- Implemented `UBillBoardComponent`, a textured quad that always faces the camera
- Added billboard texture loading, ray-casting picking, AABB updates, and serialisation
- Designed and implemented `ULightComponent`, `USpotlightComponent`, and `ASpotlight`, and connected them to editor spawning
- Calculated spotlight cone vertices from height, radius, yaw, and pitch, then visualised them through line batching
- Added real-time ImGui editing for the selected spotlight's direction, size, vertex count, and colour
- Integrated spotlight icons with render collection, mesh buffers, and scene saving

### 5. Integration Stabilisation and Follow-up Fixes

- Fixed SubUV shadowing that incorrectly hid effects behind unrelated objects, as well as texture restoration after scene loading
- Cleaned up naming and reference relationships across scenes, actors, and the object factory

## Controls

| Input | Action |
| --- | --- |
| `W` / `A` / `S` / `D` | Move the camera forwards/backwards and left/right |
| `Q` / `E` | Move the camera down/up |
| Arrow keys or right-mouse drag | Rotate the camera |
| Mouse wheel | Adjust FOV or orthographic width |
| `O` | Switch between perspective and orthographic projection |
| Left click | Select an object or gizmo axis |
| `Ctrl` + left click | Select multiple objects |
| Left-mouse drag | Edit the transform using the selected gizmo |
| `Space` | Cycle Translate → Rotate → Scale modes |

The ImGui **Jungle Control Panel** provides scene, camera, gizmo, and view-mode settings. Scene, object, and component properties can be edited through the **Scene Manager** and **Picked Object** windows.

## Project Structure

```text
.
├─ Assets/                 # Shaders, font atlases, icons, effect textures
├─ Editor/                 # Editor engine, viewport, ImGui panels
├─ Engine/
│  ├─ Classes/            # Actors and spotlights
│  ├─ Core/               # Input, FName, shared types, console
│  ├─ FileManager/        # Scene serialisation and editor settings
│  ├─ Fonts/              # Font cache and DDS loading
│  ├─ Render/             # D3D11 device, resources, render commands, pipeline
│  └─ Scene/              # Camera, UScene, FSceneManager
├─ Object/                # UObject, object manager, object factory
├─ World/
│  ├─ Gizmo/              # Transform-gizmo management
│  ├─ Light/              # Light and spotlight components
│  └─ Primitives/         # Basic shapes, billboards, text, SubUV
└─ Saves/                 # Saved scene data
```

## Building and Running

1. Open `Week3.sln` in Visual Studio 2022 on Windows.
2. Select a `Debug` or `Release` configuration for `x64`.
3. Build and run the solution.

The project targets MSVC `v143`, the Windows 10 SDK, and C++17.

## Notes

- The complete collaboration history and team-wide changes are available in [Rocketstein/Game-Tech-Lab-W3](https://github.com/Rocketstein/Game-Tech-Lab-W3).
- The individual contributions documented here were identified by reviewing author metadata, commit messages, and the files changed in the original repository.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 3 — ZZUP Engine

> Week 2 엔진을 확장해 멀티 씬 편집, Billboard, Spotlight, SubUV, 텍스트 렌더링과 디버그 뷰를 구현한 프로젝트입니다.  
> 이 저장소는 [원본 팀 프로젝트](https://github.com/Rocketstein/Game-Tech-Lab-W3)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

기존 DirectX 11 기반 엔진 구조를 확장해 여러 씬을 전환·관리하는 편집 환경을 만들고, 2D 텍스처와 텍스트, 조명 디버그 표현 등 에디터에서 필요한 시각화 기능을 추가했습니다.

- **개발 기간:** 2026.03.19 ~ 2026.03.26
- **개발 환경:** Windows, Visual Studio 2022, C++17
- **주요 기술:** DirectX 11, HLSL, Dear ImGui, JSON, DirectXTK
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- 여러 `UScene`을 생성·전환·삭제하는 `FSceneManager`
- UUID 기반 오브젝트 직렬화와 씬 저장·불러오기
- Lit / Unlit / Wireframe View Mode와 렌더 Show Flag
- 동적 Line Batch 기반 Grid, AABB, Spotlight 디버그 렌더링
- Sprite Sheet를 재생하는 SubUV 애니메이션과 편집 패널
- 카메라를 향하는 Billboard 및 텍스처 리소스 로딩
- 한글 Font Atlas, GPU Instancing 기반 텍스트 렌더링과 `UTextComponent`
- `FName` 기반 오브젝트 이름 관리와 UUID 표시·편집
- Ray Casting 피킹, Ctrl 다중 선택, 이동·회전·크기 조절 기즈모
- ImGui 기반 Scene Manager, Inspector, 카메라·뷰 설정

## 담당 작업

### 1. Week 3 프로젝트 기반 구성과 빌드 안정화

- Week 2 엔진을 Week 3 프로젝트의 출발점으로 구성하고 솔루션·프로젝트 구조 정리
- 이동된 엔진 파일에 맞춰 Include 경로와 빌드 오류 수정
- 한글 주석과 UI 문자열을 위한 UTF-8 인코딩 문제 정리

### 2. World·Scene 구조 재설계

- `UWorld`가 `FSceneManager`를 통해 여러 `UScene`과 Active Scene을 관리하도록 구조 확장
- Actor의 생성·Tick·종료 책임을 Scene 단위로 분리
- Camera와 Gizmo의 관리 책임을 `FEditorViewportClient`로 이동해 에디터 엔진 의존성 축소
- Scene 생성, 전환, 삭제 시 카메라·기즈모·선택 상태가 함께 갱신되도록 연결
- Scene Manager 창과 Loaded Scene 선택 UI 구현

### 3. 씬 저장·불러오기 복구 및 확장

- 리팩터링된 World·Scene 구조에 맞춰 기존 JSON 저장·불러오기 기능 복구
- Scene, Actor, Component를 UUID로 직렬화하고 로드 후 부모·Root Component·소유 Scene 관계를 재연결
- 파일 대화상자를 통한 `.Scene` 저장·불러오기와 중복 UUID 방지 처리
- 씬 전환 후 뷰포트와 기즈모가 이전 씬 객체를 계속 참조하는 문제 수정

### 4. Billboard·Spotlight 기반 구현

- 항상 카메라를 향하는 텍스처 Quad인 `UBillBoardComponent` 구현
- Billboard 텍스처 로딩, Ray Casting 피킹, AABB 갱신, 직렬화 지원
- `ULightComponent`, `USpotlightComponent`, `ASpotlight` 구조 설계 및 에디터 Spawn 연동
- Spotlight의 높이·반지름·Yaw·Pitch로 Cone 정점을 계산하고 Line Batch로 시각화
- 선택한 Spotlight의 방향, 크기, 정점 수, 색상을 ImGui에서 실시간 편집하도록 구현
- Spotlight 아이콘과 렌더 수집·메시 버퍼·저장 시스템 연동

### 5. 통합 안정화와 후속 수정

- SubUV가 다른 객체 뒤에서 잘못 가려지는 Shadowing 문제와 씬 로드 후 텍스처 복원 문제 수정
- Scene·Actor·Object Factory 전반의 명명과 참조 관계 정리

## 조작 방법

| 입력 | 동작 |
| --- | --- |
| `W` / `A` / `S` / `D` | 카메라 전후·좌우 이동 |
| `Q` / `E` | 카메라 하강·상승 |
| 방향키 또는 마우스 우클릭 드래그 | 카메라 회전 |
| 마우스 휠 | FOV 또는 Orthographic Width 조절 |
| `O` | Perspective / Orthographic 전환 |
| 마우스 좌클릭 | 오브젝트 또는 기즈모 축 선택 |
| `Ctrl` + 좌클릭 | 오브젝트 다중 선택 |
| 좌클릭 드래그 | 선택한 기즈모로 Transform 편집 |
| `Space` | Translate → Rotate → Scale 모드 순환 |

ImGui의 **Jungle Control Panel**에서는 Scene, Camera, Gizmo, View Mode를 설정할 수 있으며, **Scene Manager**와 **Picked Object** 창에서 씬·오브젝트·컴포넌트 속성을 편집할 수 있습니다.

## 프로젝트 구조

```text
.
├─ Assets/                 # Shader, Font Atlas, Icon, Effect Texture
├─ Editor/                 # 에디터 엔진, 뷰포트, ImGui 패널
├─ Engine/
│  ├─ Classes/            # Actor와 Spotlight
│  ├─ Core/               # 입력, FName, 공용 타입, 콘솔
│  ├─ FileManager/        # 씬 직렬화와 에디터 설정
│  ├─ Fonts/              # Font Cache와 DDS 로딩
│  ├─ Render/             # D3D11 장치, 리소스, 렌더 명령·파이프라인
│  └─ Scene/              # Camera, UScene, FSceneManager
├─ Object/                # UObject, Object Manager, Object Factory
├─ World/
│  ├─ Gizmo/              # Transform Gizmo 관리
│  ├─ Light/              # Light와 Spotlight Component
│  └─ Primitives/         # 기본 도형, Billboard, Text, SubUV
└─ Saves/                 # 저장된 Scene 데이터
```

## 빌드 및 실행

1. Windows에서 Visual Studio 2022로 `Week3.sln`을 엽니다.
2. `Debug` 또는 `Release`, `x64` 구성을 선택합니다.
3. 솔루션을 빌드한 뒤 실행합니다.

프로젝트는 MSVC `v143`, Windows 10 SDK, C++17을 기준으로 구성되어 있습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [Rocketstein/Game-Tech-Lab-W3](https://github.com/Rocketstein/Game-Tech-Lab-W3)에서 확인할 수 있습니다.
- 이 README의 개인 기여 내역은 원본 저장소의 작성자 정보, 커밋 메시지 및 실제 변경 파일을 함께 확인해 정리했습니다.
