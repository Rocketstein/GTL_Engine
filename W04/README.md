<a id="english"></a>

> **Languages:** English · [한국어](#한국어)

# Week 4 — CO-PASS Engine

![CO-PASS Logo](Editor/Resources/Tool/copass.png)

> A custom-engine project featuring a multi-viewport editor and OBJ viewer built with Win32, Direct3D 11, and ImGui.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original team project](https://github.com/DKael/KraftonJungle_Week4_Team8).

CO-PASS Engine is a custom editor and rendering-engine project built with **Win32, Direct3D 11, and ImGui**. It centres on two areas:

- **Editor-oriented workflows**
  - Editing through an Outliner, Details panel, Content Browser, Console, and Control Panel
- **Real-time rendering and asset-pipeline experiments**
  - Meshes, sprites, billboard text, outlines, picking, asset loading, and scene serialisation

Rather than implementing a full gameplay framework, this repository focuses on **editor development**, **renderer architecture**, and **scene/asset workflows**.

## 1. Project Overview

CO-PASS Engine is an educational engine project in which the following systems were implemented directly.

- **Development period:** 27 March–2 April 2026
- **Development environment:** Windows, Visual Studio 2022, C++17
- **Key technologies:** Direct3D 11, HLSL, Dear ImGui, JSON, Win32 API
- **Project type:** Team project / portfolio focused on individual contributions

- Win32-based editor application
- Direct3D 11 rendering pipeline
- ImGui docking-based editor UI
- Panel system and menu registry
- Scene saving and loading using JSON-based `.Scene` files
- Resource loading through an AssetManager and specialised loaders
- Content Browser, drag-and-drop, and Details-panel integration
- Six multi-viewport layouts, from Single to Four Way, with draggable splitters
- Per-viewport camera, input, and rendering contexts with shared selection state
- Standalone OBJ viewer with an orbit camera, coordinate-system conversion, scaling, and cull-mode controls
- Viewport cameras, selection, gizmos, outlines, and AABB visualisation
- Sprite, SubUV, and atlas-text rendering

## 2. Key Features

### Editor Features

- **Multi Viewport**
  - `Single`, `TwoColumn`, `TwoRow`, `ColumnTwoRow`, `TwoRowColumn`, and `FourWay` layouts
  - Draggable splitters that preserve their proportions when the window is resized
  - Independent camera and input contexts per viewport, with a shared selection controller
- **Outliner**
  - Displays Actors in the current scene
  - Supports Actor creation and selection
- **Details**
  - Edits properties of the selected Actor or Component
  - Supports transforms and a manual per-component property system
- **Content Browser**
  - Indexes the `Editor/Content` directory
  - Provides a folder tree, file list, search, filtering, and drag-and-drop
- **Console**
  - Controls scenes, creates and deletes Actors, and changes camera, grid, and view-mode settings
- **Control Panel**
  - Camera transform, projection, and FOV
  - Viewport layout and orientation
  - View mode, show flags, grid spacing, and navigation speed
- **Shortcuts / About**
  - Shortcut reference and project-information pop-ups
- **OBJ Viewer**
  - Loads and renders OBJ static meshes through a separate engine loop
  - Orbit camera, six-direction camera alignment, and Slerp transitions
  - Y-up to Z-up coordinate-system conversion, view-mode controls, and cull-mode controls
  - Per-axis and absolute scaling, with automatic correction for extremely large or small models

### Rendering Features

- Mesh rendering
- Sprite rendering
- Billboard-text rendering
- Selection outlines
- Object-ID-based picking
- Grid, world-axis, gizmo, and AABB overlays
- `Lit`, `Unlit`, and `Wireframe` view modes
- Editor and scene show-flag toggles

### Data and Workflow Features

- `.Scene` save/load support with perspective-camera state restoration
- Scene asset paths stored using `/Game/...` virtual paths
- Texture, font, and sprite-atlas loading through the AssetManager
- Direct entry of asset paths in the Details panel
- Drag-and-drop of textures, fonts, and atlases from the Content Browser into the Details panel

## 3. My Contributions

### 1. Multi-Viewport Layouts and a Slate-Style Window System

- Implemented six viewport layouts, covering one-, two-, three-, and four-way splits, around `FWindowOverlayManager`
- Moved the reusable `SWidget` → `SWindow` → `SSplitter` hierarchy into the engine's Core Runtime
- Implemented splitter hit testing and drag input while preserving horizontal and vertical split ratios across window resizes
- Added `D3D11WidgetRenderer` and widget render data to draw splitter boundaries and visualise layout interaction state
- Connected the layouts to the ImGui Control Panel, dynamically creating and destroying the viewport panels required by each configuration

Representative commits: [`538cb969`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/538cb969787e166598077851a9f6d299b2750ec8), [`75b1d8c7`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/75b1d8c7d836c6f77a360d1860e3f5282ae15d00), [`92be2f17`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/92be2f1742d791e39b4693b186ecc6e921db0fd5), [`19d89d60`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/19d89d6022d3350e2283c9bf5648b20997b6184b), [`73b6df79`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/73b6df797002cb6ab6cf0f6cbd322cc21af11071)

### 2. Multi-Viewport Input, Selection, and Rendering Stabilisation

- Reworked the editor loop so every active viewport ticks and renders with its own camera and render context
- Used each secondary viewport's camera origin for picking and gizmo movement, fixing screen/world coordinate mismatches
- Shared one `FViewportSelectionController` between viewports to prevent duplicate selections and deletion conflicts
- Improved the rendering path by collecting view-independent scene data once per frame and reusing it across panels
- Propagated newly loaded scenes and the most recently focused panel to the Overlay Manager, Console, and Global Editor Context

Representative commits: [`74504602`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/745046028fcd003fbfe2b6837b6a64683b4de2ea), [`931280a8`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/931280a8a5fed07746d4a198a719200ca76f5071), [`1b7ae7df`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/1b7ae7df64b1da4c0d2bef21fdfbcefc9de6ca08), [`0c0b3150`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/0c0b31508be6e1b2be751f8a1a44f3e5299edc36), [`3dfcdf38`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/3dfcdf38d829ecb13e270327f71c655c2eade31a), [`e3c910fb`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/e3c910fbe926f9ac68aff7da4bb12e36742670a8)

### 3. Orthographic Views and Camera-State Serialisation

- Added front, back, left, right, top, and bottom orthographic orientations with movement constraints for secondary viewports
- Defined default orientations and ortho heights for each layout, allowing the focused view's orientation to be changed in the Control Panel
- Serialised and restored perspective-camera location, rotation, FOV, and near/far planes in scene JSON
- Reset the navigation controller's target location after scene loading, preventing interpolation back to the previous camera position
- Fixed UUID, primitive, and sprite visibility settings that were not being applied to the actual scene show flags

Representative commits: [`9083e89c`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/9083e89c4aa8bf140128b72064346c2345378782), [`30d23293`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/30d2329305bb5445903fdad99383d2d4246acac4), [`5d9bb3c2`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/5d9bb3c2e1ac776af3cd0e37508e16878641ea99), [`a72b3962`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/a72b396262d9711b1afeee93abc0bcd404cc1be2)

### 4. Standalone OBJ Viewer and Renderer Integration

- Added `FObjViewerEngineLoop`, enabled by the `IS_OBJ_VIEWER` configuration, to provide a model-inspection path independent of the editor
- Implemented OBJ static-mesh loading, automatic camera fitting, mouse-based orbit/pan/zoom controls, and an FPS display
- Separated input, camera, and UI responsibilities into `FOrbitalCameraController` and `FViewerImGui`
- Stabilised model and material-texture loading by fixing file-dialog and Texture Loader initialisation errors
- Propagated Lit/Unlit/Wireframe view modes and Back/Front/None cull modes from the viewer to the renderer

Representative commits: [`595e8f24`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/595e8f24defff8a59bd432124be087b81c15858f), [`148079b7`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/148079b78075c525d4be429708abfa1ab20255f1), [`f97c85b8`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/f97c85b8a5ffb39b052c063a72225d6926c9c4ce), [`f27d441e`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/f27d441e2ceabbc9bddd15c7a7488a059849f720), [`ef6b4027`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/ef6b40279afd5825c6896e130a7ef735e033480d)

### 5. OBJ Viewer Model-Inspection UX

- Connected six-direction camera-alignment controls to quaternion Slerp for smooth viewpoint transitions
- Added a change-of-basis option to convert Y-up models into the engine's Z-up coordinate system
- Added per-axis relative scaling and absolute scaling, with automatic normalisation for models of extreme size
- Fixed an interaction bug that reset absolute scale after axis alignment or scale dragging
- Added OBJ sample assets for validating complex models with multiple materials

Representative commits: [`c75c2084`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/c75c208432a14d6ecb2f76cb47fd85cd9413044c), [`0b0f22f3`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/0b0f22f35db4f057d3232ea4abaed8f41d1bddb1), [`db179cfd`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/db179cfd17433edbf2cc47fd5a527769a713e4c8), [`19bb401b`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/19bb401b62fefb59d0a39eaeb44ef56a4562a622), [`279ff109`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/279ff109208458a72b60dd4afe1830d2fb71b067), [`83ad228b`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/83ad228b9f6462ab75a2b2b34df612ba9820344c), [`c61466a1`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/c61466a193f61cc2fed659ae9013b0c0869e4a05)

## 4. Technology Stack

- **Language:** C++
- **Platform:** Windows
- **Graphics API:** Direct3D 11
- **Windowing/Input:** Win32 API
- **Editor UI:** Dear ImGui
- **Data:** JSON
- **Project system:** Visual Studio Solution

Third-party components:

- Dear ImGui
- nlohmann/json
- DirectXTK Desktop Win10 NuGet package

## 5. Project Structure

```text
.
├─ Editor/
│  ├─ Content/                 # Content root used by the editor at runtime
│  ├─ Resources/               # Icons, logo, and .rc resources
│  ├─ Saved/Config/            # editor.ini
│  └─ Source/
│     ├─ Camera/
│     ├─ Chrome/
│     ├─ Content/
│     ├─ Editor/
│     ├─ Input/
│     ├─ Launch/
│     ├─ Menu/
│     ├─ Panel/
│     ├─ Viewport/
│     ├─ Viewer/               # OBJ viewer, orbit camera, viewer ImGui
│     └─ ThirdParty/imgui/
├─ Engine/
│  ├─ Resources/Mesh/          # Built-in primitive-mesh data
│  └─ Source/
│     ├─ ApplicationCore/
│     ├─ Asset/
│     ├─ Core/
│     ├─ CoreUObject/
│     ├─ Engine/
│     ├─ Renderer/
│     └─ SceneIO/
├─ Docs/
├─ Scripts/
└─ Kraftonjungle_Team2.sln
```

## 6. Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- Desktop development with C++ workload
- Direct3D 11 runtime support

### Build Instructions

1. Open the repository.
2. Open `Kraftonjungle_Team2.sln` in Visual Studio.
3. Select a configuration, typically `Debug | x64`.
4. Build the solution.

Command-line build:

```powershell
msbuild .\Kraftonjungle_Team2.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m:1 /nologo /v:minimal
```

To regenerate project files:

```powershell
.\GenerateProjectFiles.bat
```

### Runtime Root and Content Paths

At launch, the application root is initialised relative to the `Editor` directory. The default content root is therefore:

- `Editor/Content`

For example:

- Scenes: `Editor/Content/Scenes`
- Fonts: `Editor/Content/Font`
- Textures: `Editor/Content/Texture`

Editor settings are stored in:

- `Editor/Saved/Config/editor.ini`

## 7. Editor Workflow

### Main Panels

- **Outliner:** Scene Actor list and creation
- **Details:** Property editing for the selected object
- **Control Panel:** Camera and view settings
- **Console:** Command-driven controls
- **State:** FPS and other runtime information
- **Content Browser:** Asset browsing

### Typical Workflow

1. Create an Actor in the Outliner.
2. Select it in the viewport or Outliner.
3. Edit its transform or Component properties in Details.
4. Drag a texture, font, or atlas from the Content Browser into Details.
5. Save the work as a `.Scene` file and reopen it later.

## 8. Shortcuts

The complete shortcut list is available in the editor under **Help > Shortcuts**. Representative controls in the current codebase include:

- `Right Mouse Drag`: Rotate the camera
- `Middle Mouse Drag`: Pan the camera
- `Alt + Left Mouse Drag`: Orbit around the selected object
- `Mouse Wheel`: Zoom the camera or adjust FOV/orthographic settings
- `W / A / S / D / Q / E`: Move the camera
- `F`: Focus the camera on the selected object
- `Left Mouse Click`: Single selection
- `Ctrl + Click`: Toggle selection / multi-select
- `Delete`: Delete selected Actors
- `Space`: Change gizmo type

## 9. Asset System

The CO-PASS Engine asset system separates **reading source files** from **creating engine resources**.

Key files:

- `Engine/Source/Asset/AssetManager.h`
- `Engine/Source/Asset/AssetLoader.h`
- `Engine/Source/Asset/TextureLoader.cpp`
- `Engine/Source/Asset/FontAtlasLoader.cpp`
- `Engine/Source/Asset/SubUVAtlasLoader.cpp`

### Concepts

- `UAssetManager`
  - Loader registration
  - Source-cache management
  - Loaded-asset cache management
- `IAssetLoader`
  - Responsible for loading a particular extension or asset type
- `UAsset`
  - Engine asset object that stores a loading result
- `Resource`
  - The actual GPU/rendering resource used directly by the renderer

### Supported Asset Types

- `Texture`
- `Font`
- `SpriteAtlas`

The code's enum also anticipates meshes, shaders, and materials, but the three types above form the current core implementation.

### Loading Flow

1. A Component stores an `AssetPath`.
2. `ResolveAssetReferences()` is called.
3. `UAssetManager::Load()` runs.
4. A loader reads the file and creates a `UAsset`.
5. The Component obtains its final `Resource*` from the `UAsset`.

### Source Cache and Asset Cache

The AssetManager separates two caches:

- **Source Cache**
  - File bytes, file size, modification time, and hash
- **Loaded Asset Cache**
  - Reuses final assets using type, path, and build signature as the cache key

This design reduces redundant decoding and reconstruction when the same file is loaded repeatedly.

## 10. Connecting Components to Assets

Components in the current project generally follow this pattern:

- Persistent value: `AssetPath`
- Runtime value: `Resource*`

Examples:

- `USpriteComponent`
  - `TexturePath`
  - `FTextureResource*`
- `UAtlasTextComponent`
  - `FontPath`
  - `FFontResource*`
- `USubUVComponent`
  - `SubUVAtlasPath`
  - `FSubUVAtlasResource*`

Rather than owning the `UAsset` itself, a Component stores a path and resolves it when it needs to acquire the corresponding resource.

## 11. Content Browser

The Content Browser is an **editor-specific index and browser**, not simply an AssetManager UI.

Key files:

- `Editor/Source/Content/EditorContentIndex.h`
- `Editor/Source/Panel/ContentBrowserPanel.cpp`

Features:

- Recursive scanning of `Editor/Content`
- `/Game/...` virtual-path generation
- Folder-tree and file-list views
- Search within the current folder
- Search including subdirectories
- Type filters
- Drag-and-drop

Recognised item types:

- Scene
- Texture
- Font
- Sprite Atlas
- Unknown File

## 12. Scene System

Scenes are JSON documents using the `.Scene` extension.

Key files:

- `Engine/Source/SceneIO/SceneSerializer.h`
- `Engine/Source/SceneIO/SceneSerializer.cpp`
- `Engine/Source/SceneIO/SceneTypeRegistry.cpp`
- `Engine/Source/SceneIO/SceneAssetPath.h`

### Features

- Serialises Actor and Component structures
- Serialises Component hierarchies
- Serialises each Component using the manual property system
- Stores `/Game/...` asset paths
- Restores concrete Actor and Component types through a registry when loading
- Falls back to `UnknownActor` and `UnknownComponent` for unrecognised types

### Role of Scene Files

- Store editor scene documents
- Remain separate from ordinary items in the AssetManager's asset cache

## 13. Manual Property System

Instead of a complete reflection system such as Unreal's `UProperty`, each Component explicitly declares which properties should be exposed and persisted.

Key file:

- `Engine/Source/Engine/Component/Core/ComponentProperty.h`

Advantages:

- Shares one definition between the Details UI and scene serialisation
- Remains extensible without automatic reflection
- Lets each Component author control its exposure policy directly

Values such as text, texture paths, colours, and animation speeds are therefore reflected in both the Details panel and `.Scene` files through the same system.

## 14. Rendering Pipeline

Key files:

- `Engine/Source/Renderer/RendererModule.cpp`
- `Engine/Source/Renderer/D3D11/D3D11RHI.cpp`
- `Engine/Source/Renderer/D3D11/D3D11MeshBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11SpriteBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11TextBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11OutlineRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11ObjectIdRenderer.cpp`

The current rendering order is approximately:

1. Scene primitive meshes
2. Selection outlines
3. Sprites
4. Billboard text
5. Gizmos
6. Grid, axes, AABBs, and other line overlays

Additional features:

- Picking through off-screen Object ID rendering
- Dedicated selection-outline renderer
- Selected objects use a distinct colour in wireframe mode
- Korean glyphs can be rendered through UTF-8 text decoding

## 15. Included Actor and Component Examples

### Actors

- `ACubeActor`
- `ASphereActor`
- `AConeActor`
- `ACylinderActor`
- `ARingActor`
- `ATriangleActor`
- `ASpriteActor`
- `AAtlasSpriteActor`
- `AEffectActor`
- `AFlipbookActor`
- `ATextActor`
- `AUnknownActor`

### Components

- Mesh
  - `UCubeComponent`
  - `USphereComponent`
  - `UConeComponent`
  - `UCylinderComponent`
  - `URingComponent`
  - `UTriangleComponent`
  - `UQuadComponent`
- Sprite
  - `USpriteComponent`
  - `UAtlasComponent`
  - `USubUVComponent`
  - `USubUVAnimatedComponent`
- Text
  - `UAtlasTextComponent`
  - `UUUIDComponent`
- Core
  - `USceneComponent`
  - `UPrimitiveComponent`
  - `UUnknownComponent`

## 16. Console Commands

The Console panel exposes commands for changing editor state.

Examples:

```text
scene.new
scene.save
scene.open "Editor/Content/Scenes/Sample.Scene"
actor.spawn cube 3
actor.spawn sphere 2
actor.delete_selected
select.clear
select.focus
camera.reset
camera.speed 300
camera.rot_speed 0.2
grid.spacing 50
viewmode wireframe
show.grid on
show.outline off
content.refresh
content.find font
```

## 17. Known Limitations and Current Status

As an educational engine/editor project, the current implementation has the following characteristics:

- Targets Windows and D3D11
- Does not provide a complete reflection system
- Does not include Undo/Redo
- Does not include a multithreaded asset pipeline
- Focuses more on editor and rendering architecture experiments than on a game runtime

It is therefore intended for learning through direct design and implementation rather than as a general-purpose commercial engine.

## 18. Documentation

Additional documentation is stored in `Docs/`.

## 19. Licence

The project code is provided under the **MIT Licence**.

See [LICENSE](LICENSE) for details.

Third-party libraries remain subject to their respective licence terms.

## Notes

- The complete collaboration history and team-wide changes are available in [DKael/KraftonJungle_Week4_Team8](https://github.com/DKael/KraftonJungle_Week4_Team8).
- The individual contributions documented here were identified by reviewing author metadata, commit messages, and the files changed in the original repository.

---

**CO-PASS Engine**  
A level editor and rendering sandbox for the CO-PASS project.

---

## 한국어

> **Languages:** [English](#english) · 한국어

# Week 4 — CO-PASS Engine

![CO-PASS Logo](Editor/Resources/Tool/copass.png)

> Win32 + Direct3D 11 + ImGui 기반의 멀티 뷰포트 에디터와 OBJ Viewer를 구현한 자체 엔진 프로젝트입니다.  
> 이 저장소는 [원본 팀 프로젝트](https://github.com/DKael/KraftonJungle_Week4_Team8)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

CO-PASS Engine은 **Win32 + Direct3D 11 + ImGui** 기반으로 만든 자체 에디터/렌더링 엔진 프로젝트입니다.  
현재 프로젝트는 다음 두 가지를 중심으로 구성되어 있습니다.

- **에디터 중심 워크플로우**
  - Outliner, Details, Content Browser, Console, Control Panel 기반 편집
- **실시간 렌더링/자산 파이프라인 실험**
  - Mesh / Sprite / Billboard Text / Outline / Picking / Asset Loading / Scene Serialization

이 저장소는 게임 플레이 프레임워크보다 **에디터 제작**, **렌더러 구조화**, **씬/에셋 워크플로우 구현**에 더 초점을 둡니다.

## 1. 프로젝트 개요

CO-PASS Engine은 다음 기능을 직접 구현한 학습형 엔진 프로젝트입니다.

- **개발 기간:** 2026.03.27 ~ 2026.04.02
- **개발 환경:** Windows, Visual Studio 2022, C++17
- **주요 기술:** Direct3D 11, HLSL, Dear ImGui, JSON, Win32 API
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

- Win32 기반 에디터 애플리케이션
- Direct3D 11 렌더링 파이프라인
- ImGui 도킹 기반 에디터 UI
- 패널 시스템과 메뉴 레지스트리
- Scene 저장/불러오기 (`.Scene`, JSON 기반)
- AssetManager + Loader 기반 리소스 로딩
- Content Browser / Drag & Drop / Details 연동
- Single부터 Four Way까지 6종 멀티 뷰포트 레이아웃과 드래그 가능한 Splitter
- Viewport별 카메라·입력·렌더 컨텍스트와 공유 선택 상태
- 독립 OBJ Viewer와 Orbit Camera, 좌표계·스케일·Cull Mode 제어
- Viewport 카메라, 선택, Gizmo, Outline, AABB 표시
- Sprite / SubUV / Atlas Text 렌더링

## 2. 주요 특징

### 에디터 기능

- **Multi Viewport**
  - `Single`, `TwoColumn`, `TwoRow`, `ColumnTwoRow`, `TwoRowColumn`, `FourWay` 레이아웃
  - 드래그 가능한 Splitter와 창 크기 변경 시 분할 비율 유지
  - 뷰포트별 카메라·입력 컨텍스트와 공용 선택 컨트롤러
- **Outliner**
  - 현재 씬의 Actor 목록 표시
  - Actor 추가/선택
- **Details**
  - 선택된 Actor/Component의 속성 편집
  - Transform 및 컴포넌트별 수동 property 시스템 지원
- **Content Browser**
  - `Editor/Content` 폴더 인덱싱
  - 폴더 트리, 파일 목록, 검색, 필터, 드래그 앤 드롭
- **Console**
  - 씬 제어, Actor 생성/삭제, 카메라/그리드/뷰모드 변경
- **Control Panel**
  - 카메라 Transform, Projection, FOV
  - Viewport Layout / Orientation
  - View Mode, Show Flags, Grid Spacing, Navigation 속도
- **Shortcuts / About**
  - 단축키 안내와 프로젝트 정보 팝업
- **OBJ Viewer**
  - 별도 엔진 루프에서 OBJ Static Mesh 로딩·렌더링
  - Orbit Camera, 6방향 카메라 정렬, Slerp 전환
  - Y-up → Z-up 좌표계 변환과 View Mode / Cull Mode 제어
  - 축별·절대 스케일 조정 및 극단적인 모델 크기 자동 보정

### 렌더링 기능

- Mesh 렌더링
- Sprite 렌더링
- Billboard Text 렌더링
- 선택된 오브젝트 Outline 표시
- Object ID 기반 Picking
- Grid / World Axes / Gizmo / AABB 오버레이
- `Lit / Unlit / Wireframe` View Mode
- Editor Show Flags / Scene Show Flags 토글

### 데이터/워크플로우 기능

- `.Scene` 저장/불러오기와 Perspective Camera 상태 복원
- `/Game/...` 가상 경로 기반 scene asset path 저장
- AssetManager를 통한 Texture / Font / SpriteAtlas 로딩
- Details에서 AssetPath 직접 입력
- Content Browser에서 Details로 텍스처/폰트/아틀라스 드래그 앤 드롭

## 3. 담당 작업

### 1. 멀티 뷰포트 레이아웃과 Slate형 윈도 시스템

- `FWindowOverlayManager`를 중심으로 1·2·3·4분할을 아우르는 6종 뷰포트 레이아웃 구현
- `SWidget` → `SWindow` → `SSplitter` 계층을 엔진 Core Runtime으로 분리해 재사용 가능한 윈도 구조 마련
- Splitter Hit Test와 드래그 입력을 구현하고, 창 크기가 바뀌어도 가로·세로 분할 비율이 유지되도록 처리
- `D3D11WidgetRenderer`와 Widget Render Data를 추가해 Splitter 경계를 렌더링하고 레이아웃 조작 상태를 시각화
- ImGui Control Panel에서 레이아웃을 즉시 전환하고 각 구성에 필요한 뷰포트 패널을 동적으로 생성·정리하도록 연결

대표 커밋: [`538cb969`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/538cb969787e166598077851a9f6d299b2750ec8), [`75b1d8c7`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/75b1d8c7d836c6f77a360d1860e3f5282ae15d00), [`92be2f17`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/92be2f1742d791e39b4693b186ecc6e921db0fd5), [`19d89d60`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/19d89d6022d3350e2283c9bf5648b20997b6184b), [`73b6df79`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/73b6df797002cb6ab6cf0f6cbd322cc21af11071)

### 2. 멀티 뷰포트 입력·선택·렌더링 안정화

- 모든 활성 뷰포트가 각자의 카메라와 렌더 컨텍스트로 Tick·Render되도록 에디터 루프 재구성
- 서브 뷰포트의 Camera Origin을 Picking과 Gizmo 이동 계산에 반영해 화면·월드 좌표 불일치 수정
- 여러 뷰포트가 하나의 `FViewportSelectionController`를 공유하도록 해 중복 선택과 삭제 충돌 방지
- 씬의 View-independent Render Data를 프레임당 한 번만 수집하고 각 패널에서 재사용하도록 렌더 경로 개선
- 새로 로드한 Scene과 마지막으로 포커스된 패널을 Overlay Manager, Console, Global Editor Context에 전파

대표 커밋: [`74504602`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/745046028fcd003fbfe2b6837b6a64683b4de2ea), [`931280a8`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/931280a8a5fed07746d4a198a719200ca76f5071), [`1b7ae7df`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/1b7ae7df64b1da4c0d2bef21fdfbcefc9de6ca08), [`0c0b3150`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/0c0b31508be6e1b2be751f8a1a44f3e5299edc36), [`3dfcdf38`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/3dfcdf38d829ecb13e270327f71c655c2eade31a), [`e3c910fb`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/e3c910fbe926f9ac68aff7da4bb12e36742670a8)

### 3. 정사영 뷰와 카메라 상태 직렬화

- 서브 뷰포트에 Front / Back / Left / Right / Top / Bottom 기준의 정사영 방향과 이동 제한 적용
- 레이아웃별 기본 Orientation과 Ortho Height를 설정하고 Control Panel에서 포커스된 뷰의 방향을 변경하도록 구현
- Perspective Camera의 Location, Rotation, FOV, Near/Far Plane을 Scene JSON에 저장·복원
- 씬 로드 후 Navigation Controller의 목표 위치를 함께 초기화해 이전 카메라로 보간되는 문제 수정
- UUID, Primitive, Sprite 표시 여부가 실제 Scene Show Flag에 반영되지 않던 문제 수정

대표 커밋: [`9083e89c`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/9083e89c4aa8bf140128b72064346c2345378782), [`30d23293`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/30d2329305bb5445903fdad99383d2d4246acac4), [`5d9bb3c2`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/5d9bb3c2e1ac776af3cd0e37508e16878641ea99), [`a72b3962`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/a72b396262d9711b1afeee93abc0bcd404cc1be2)

### 4. 독립 OBJ Viewer와 렌더러 연동

- `IS_OBJ_VIEWER` 구성에서 동작하는 `FObjViewerEngineLoop`를 추가해 에디터와 분리된 모델 검사 실행 경로 구현
- OBJ Static Mesh 로딩, 자동 Camera Fit, 마우스 기반 Orbit / Pan / Zoom과 FPS 표시 지원
- `FOrbitalCameraController`와 `FViewerImGui`를 별도 모듈로 분리해 입력·카메라·UI 책임 정리
- 파일 대화상자와 Texture Loader 초기화 오류를 수정해 모델과 재질 텍스처 로딩 안정화
- Lit / Unlit / Wireframe View Mode와 Back / Front / None Cull Mode를 Viewer에서 렌더러까지 전달

대표 커밋: [`595e8f24`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/595e8f24defff8a59bd432124be087b81c15858f), [`148079b7`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/148079b78075c525d4be429708abfa1ab20255f1), [`f97c85b8`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/f97c85b8a5ffb39b052c063a72225d6926c9c4ce), [`f27d441e`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/f27d441e2ceabbc9bddd15c7a7488a059849f720), [`ef6b4027`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/ef6b40279afd5825c6896e130a7ef735e033480d)

### 5. OBJ Viewer 모델 검사 UX 개선

- 6방향 카메라 정렬 입력과 Quaternion Slerp를 연결해 시점 전환을 부드럽게 구현
- Y-up 모델을 Z-up 엔진 좌표계로 바꾸는 Change of Basis 옵션 추가
- XYZ 축별 상대 스케일과 절대 스케일을 함께 제공하고, 극단적인 크기의 모델을 자동 정규화
- 축 정렬 또는 Scale Drag 이후 Absolute Scale이 초기화되던 상호작용 오류 수정
- 복잡한 다중 Material 모델을 검증할 수 있도록 OBJ 샘플 자산 보강

대표 커밋: [`c75c2084`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/c75c208432a14d6ecb2f76cb47fd85cd9413044c), [`0b0f22f3`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/0b0f22f35db4f057d3232ea4abaed8f41d1bddb1), [`db179cfd`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/db179cfd17433edbf2cc47fd5a527769a713e4c8), [`19bb401b`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/19bb401b62fefb59d0a39eaeb44ef56a4562a622), [`279ff109`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/279ff109208458a72b60dd4afe1830d2fb71b067), [`83ad228b`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/83ad228b9f6462ab75a2b2b34df612ba9820344c), [`c61466a1`](https://github.com/DKael/KraftonJungle_Week4_Team8/commit/c61466a193f61cc2fed659ae9013b0c0869e4a05)

## 4. 기술 스택

- **Language**: C++
- **Platform**: Windows
- **Graphics API**: Direct3D 11
- **Windowing/Input**: Win32 API
- **Editor UI**: Dear ImGui
- **Data**: JSON
- **Project System**: Visual Studio Solution

서드파티 사용 요소:

- Dear ImGui
- nlohmann/json
- DirectXTK Desktop Win10 NuGet package

## 5. 프로젝트 구조

```text
.
├─ Editor/
│  ├─ Content/                 # 에디터 실행 시 사용하는 콘텐츠 루트
│  ├─ Resources/               # 아이콘, 로고, rc 리소스
│  ├─ Saved/Config/            # editor.ini
│  └─ Source/
│     ├─ Camera/
│     ├─ Chrome/
│     ├─ Content/
│     ├─ Editor/
│     ├─ Input/
│     ├─ Launch/
│     ├─ Menu/
│     ├─ Panel/
│     ├─ Viewport/
│     ├─ Viewer/                  # OBJ Viewer, Orbit Camera, Viewer ImGui
│     └─ ThirdParty/imgui/
├─ Engine/
│  ├─ Resources/Mesh/          # 기본 primitive mesh 데이터
│  └─ Source/
│     ├─ ApplicationCore/
│     ├─ Asset/
│     ├─ Core/
│     ├─ CoreUObject/
│     ├─ Engine/
│     ├─ Renderer/
│     └─ SceneIO/
├─ Docs/
├─ Scripts/
└─ Kraftonjungle_Team2.sln
```

## 6. 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- Desktop development with C++
- Direct3D 11 실행 환경

### 빌드 방법

1. 저장소를 엽니다.
2. `Kraftonjungle_Team2.sln`을 Visual Studio에서 엽니다.
3. 구성은 보통 `Debug | x64`를 사용합니다.
4. 솔루션을 빌드합니다.

명령행 빌드:

```powershell
msbuild .\Kraftonjungle_Team2.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m:1 /nologo /v:minimal
```

프로젝트 파일을 다시 생성해야 하면:

```powershell
.\GenerateProjectFiles.bat
```

### 실행 루트와 콘텐츠 경로

실행 시 앱 루트는 `Editor` 폴더 기준으로 초기화됩니다.  
즉, 기본 콘텐츠 루트는 다음 경로를 사용합니다.

- `Editor/Content`

예를 들어:

- Scene: `Editor/Content/Scenes`
- Font: `Editor/Content/Font`
- Texture: `Editor/Content/Texture`

에디터 설정은 다음 파일에 저장됩니다.

- `Editor/Saved/Config/editor.ini`

## 7. 에디터 사용 흐름

### 기본 패널

- **Outliner**: 씬 Actor 목록과 생성
- **Details**: 선택 대상 속성 편집
- **Control Panel**: 카메라/뷰 설정
- **Console**: 명령 기반 조작
- **State**: FPS 등 상태 표시
- **Content Browser**: 에셋 탐색

### 대표 작업 흐름

1. Outliner에서 Actor를 생성합니다.
2. Viewport 또는 Outliner에서 Actor를 선택합니다.
3. Details에서 Transform / Component 속성을 수정합니다.
4. Content Browser에서 텍스처/폰트/아틀라스를 드래그해 Details에 적용합니다.
5. `.Scene`으로 저장하고 다시 열어 작업을 이어갑니다.

## 8. 단축키

실제 단축키 목록은 에디터에서 **Help > Shortcuts** 패널로 확인할 수 있습니다.  
현재 코드 기준으로 대표적인 조작은 다음과 같습니다.

- `Mouse Right Drag`: 카메라 회전
- `Mouse Middle Drag`: 카메라 이동
- `Alt + Mouse Left Drag`: 선택 대상을 기준으로 orbit
- `Mouse Wheel`: 카메라 줌 또는 FOV/Ortho 변경
- `W / A / S / D / Q / E`: 카메라 이동
- `F`: 선택 대상에 카메라 포커스
- `Mouse Left Click`: 단일 선택
- `Ctrl + Click`: 선택 토글/다중 선택
- `Delete`: 선택 Actor 삭제
- `Space`: Gizmo 타입 전환

## 9. Asset System

CO-PASS Engine의 Asset System은 **파일을 직접 읽는 단계**와 **실제 엔진 리소스를 만드는 단계**를 분리합니다.

핵심 파일:

- `Engine/Source/Asset/AssetManager.h`
- `Engine/Source/Asset/AssetLoader.h`
- `Engine/Source/Asset/TextureLoader.cpp`
- `Engine/Source/Asset/FontAtlasLoader.cpp`
- `Engine/Source/Asset/SubUVAtlasLoader.cpp`

### 개념

- `UAssetManager`
  - Loader 등록
  - Source cache 관리
  - 이미 로드된 Asset cache 관리
- `IAssetLoader`
  - 확장자/타입별 로딩 책임
- `UAsset`
  - 로딩 결과를 담는 엔진 asset 객체
- `Resource`
  - 렌더러가 직접 사용하는 실제 GPU/렌더 자원

### 지원 Asset 타입

- `Texture`
- `Font`
- `SpriteAtlas`

코드상 enum은 Mesh, Shader, Material도 고려하고 있지만 현재 핵심 구현은 위 세 가지입니다.

### 로딩 흐름

1. 컴포넌트가 `AssetPath`를 가짐
2. `ResolveAssetReferences()` 호출
3. `UAssetManager::Load()` 실행
4. Loader가 파일을 읽어 `UAsset` 생성
5. 컴포넌트가 `UAsset`에서 최종 `Resource*`를 연결

### Source Cache와 Asset Cache

AssetManager는 두 가지 캐시를 분리합니다.

- **Source Cache**
  - 파일 바이트, 파일 크기, 수정 시간, 해시
- **Loaded Asset Cache**
  - 타입 + 경로 + 빌드 시그니처 기반으로 최종 asset 재사용

이 구조 덕분에 같은 파일을 반복 로드할 때 불필요한 디코딩과 재생성을 줄일 수 있습니다.

## 10. Component와 Asset 연결 방식

현재 프로젝트의 컴포넌트는 보통 다음 구조를 따릅니다.

- 저장용 값: `AssetPath`
- 런타임 값: `Resource*`

예시:

- `USpriteComponent`
  - `TexturePath`
  - `FTextureResource*`
- `UAtlasTextComponent`
  - `FontPath`
  - `FFontResource*`
- `USubUVComponent`
  - `SubUVAtlasPath`
  - `FSubUVAtlasResource*`

즉, 컴포넌트가 `UAsset` 자체를 소유하기보다 **경로를 저장하고 필요 시 resolve해서 Resource를 잡는 방식**입니다.

## 11. Content Browser

Content Browser는 `AssetManager 화면`이 아니라 **에디터 전용 인덱스/브라우저**입니다.

핵심 파일:

- `Editor/Source/Content/EditorContentIndex.h`
- `Editor/Source/Panel/ContentBrowserPanel.cpp`

기능:

- `Editor/Content` 재귀 스캔
- `/Game/...` 가상 경로 생성
- 폴더 트리 / 파일 목록 표시
- 현재 폴더 기준 검색
- 하위 디렉터리 포함 검색
- 타입 필터
- 드래그 앤 드롭

현재 인식하는 항목:

- Scene
- Texture
- Font
- Sprite Atlas
- Unknown File

## 12. Scene System

씬은 `.Scene` 확장자를 사용하는 **JSON 문서형 파일**입니다.

핵심 파일:

- `Engine/Source/SceneIO/SceneSerializer.h`
- `Engine/Source/SceneIO/SceneSerializer.cpp`
- `Engine/Source/SceneIO/SceneTypeRegistry.cpp`
- `Engine/Source/SceneIO/SceneAssetPath.h`

### 특징

- Actor / Component 구조 저장
- Component 계층 구조 저장
- 각 Component의 수동 property 시스템 기반 직렬화
- `/Game/...` 기반 asset path 저장
- 로드 시 registry를 통해 실제 Actor/Component 타입 복원
- 알 수 없는 타입은 `UnknownActor`, `UnknownComponent`로 fallback

### Scene 파일의 역할

- 에디터 씬 문서 저장
- AssetManager의 일반 asset cache 항목과는 분리된 문서형 데이터

## 13. 수동 Property 시스템

이 프로젝트는 Unreal의 `UProperty` 같은 완전한 reflection 시스템 대신, **각 컴포넌트가 직접 어떤 속성을 노출하고 저장할지 선언하는 구조**를 사용합니다.

핵심 파일:

- `Engine/Source/Engine/Component/Core/ComponentProperty.h`

장점:

- Details UI와 Scene 직렬화를 같은 정의로 공유
- 자동 reflection 없이도 확장 가능
- 컴포넌트 작성자가 직접 노출 정책을 제어 가능

예를 들어 텍스트, 텍스처 경로, 색상, 애니메이션 속도 같은 값이 이 시스템을 통해 Details와 `.Scene` 양쪽에 반영됩니다.

## 14. 렌더링 파이프라인

핵심 파일:

- `Engine/Source/Renderer/RendererModule.cpp`
- `Engine/Source/Renderer/D3D11/D3D11RHI.cpp`
- `Engine/Source/Renderer/D3D11/D3D11MeshBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11SpriteBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11TextBatchRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11OutlineRenderer.cpp`
- `Engine/Source/Renderer/D3D11/D3D11ObjectIdRenderer.cpp`

현재 렌더링은 대략 다음 순서로 진행됩니다.

1. Scene primitive mesh
2. Selection outline
3. Sprite
4. Billboard text
5. Gizmo
6. Grid / Axes / AABB / 기타 line overlay

추가 특징:

- Object ID 오프스크린 렌더링으로 picking 수행
- Selection outline 전용 renderer 존재
- Wireframe 모드에서 선택 오브젝트는 별도 색으로 표시
- UTF-8 텍스트 디코딩을 통해 한글 glyph 렌더링 가능

## 15. 현재 포함된 Actor / Component 예시

### Actor

- `ACubeActor`
- `ASphereActor`
- `AConeActor`
- `ACylinderActor`
- `ARingActor`
- `ATriangleActor`
- `ASpriteActor`
- `AAtlasSpriteActor`
- `AEffectActor`
- `AFlipbookActor`
- `ATextActor`
- `AUnknownActor`

### Component

- Mesh
  - `UCubeComponent`
  - `USphereComponent`
  - `UConeComponent`
  - `UCylinderComponent`
  - `URingComponent`
  - `UTriangleComponent`
  - `UQuadComponent`
- Sprite
  - `USpriteComponent`
  - `UAtlasComponent`
  - `USubUVComponent`
  - `USubUVAnimatedComponent`
- Text
  - `UAtlasTextComponent`
  - `UUUIDComponent`
- Core
  - `USceneComponent`
  - `UPrimitiveComponent`
  - `UUnknownComponent`

## 16. Console 명령

Console 패널에는 에디터 상태를 바꾸는 명령들이 연결되어 있습니다.

예시:

```text
scene.new
scene.save
scene.open "Editor/Content/Scenes/Sample.Scene"
actor.spawn cube 3
actor.spawn sphere 2
actor.delete_selected
select.clear
select.focus
camera.reset
camera.speed 300
camera.rot_speed 0.2
grid.spacing 50
viewmode wireframe
show.grid on
show.outline off
content.refresh
content.find font
```

## 17. 알려진 한계 / 현재 상태

현재 프로젝트는 학습형 엔진/에디터이기 때문에 다음 성격을 가집니다.

- Windows + D3D11 환경에 맞춰져 있음
- 완전한 reflection 시스템은 없음
- Undo/Redo 시스템 없음
- 멀티스레드 asset pipeline 없음
- 게임 런타임보다 에디터/렌더링 구조 실험에 더 집중

즉, 범용 상용 엔진보다 **직접 설계하고 구조를 이해하는 목적**에 적합합니다.

## 18. 문서

추가 문서는 `Docs/` 폴더에 정리합니다.

## 19. 라이선스

이 프로젝트의 코드는 **MIT License**를 따릅니다.

자세한 내용은 [LICENSE](LICENSE)를 참고하세요.

서드파티 라이브러리는 각 라이브러리의 라이선스 조건을 따릅니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [DKael/KraftonJungle_Week4_Team8](https://github.com/DKael/KraftonJungle_Week4_Team8)에서 확인할 수 있습니다.
- 이 README의 개인 기여 내역은 원본 저장소의 작성자 정보, 커밋 메시지 및 실제 변경 파일을 함께 확인해 정리했습니다.

---

**CO-PASS Engine**  
Level editor and rendering sandbox for the CO-PASS project.