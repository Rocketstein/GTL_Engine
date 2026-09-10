> **Languages:** English · [한국어](#한국어)

# Week 10 — Pacific Engine: Skeletal Mesh & FBX

> A project that adds an FBX asset pipeline, skeletal-mesh skinning, and dedicated editing tools to a custom DirectX 11 engine.  
> This repository is a portfolio snapshot highlighting the work of **Rocketstein (Hyungjun Kim)** within the [original collaborative project](https://github.com/MozziDog/Jungle_Week10_Team6).

## Project Overview

Building on the Week 9 engine, we implemented a pipeline for importing static and skeletal FBX assets and editing and rendering character poses using skeleton hierarchies and skin weights. Mesh, section, material, bone, and bind-pose data extracted through the FBX SDK is converted into engine assets, while CPU-skinned results are connected to DirectX 11 vertex buffers and scene proxies.

The final team project supports a Skeletal Mesh Viewer, bone-hierarchy and gizmo editing, skeleton debug overlays, scene spawning, and pose persistence. The **My Contributions** section below distinguishes work authored by Rocketstein using commits and the files actually changed.

- **Core development period:** 8–14 May 2026
- **Development environment:** Windows, Visual Studio 2022, C++20
- **Key technologies:** DirectX 11, Autodesk FBX SDK, CPU skinning, HLSL, Dear ImGui
- **Areas of responsibility:** Skeletal debug pass, skeletal rendering resources, FBX static import, mesh picking, scene spawning, and pose persistence
- **Project type:** Team project / portfolio focused on individual contributions

## Key Features

- Autodesk FBX SDK-based static and skeletal mesh import
- Coordinate-axis and unit conversion for FBX scenes, with mesh-node traversal
- Skeleton hierarchy, reference/display poses, and bind-matrix management
- CPU skinning using per-vertex bone indices and weights
- Per-SubMesh/Section material slots and shader-specific dynamic vertex layouts
- Bone hierarchy, picking, transform gizmos, and pose reset in the Skeletal Mesh Viewer
- Skeleton debug views for skin bind pose and FBX local pose
- Skeletal debug render pass showing bone nodes and parent–child links
- Scene spawning of an edited skeletal mesh and pose from the viewer
- Scene serialisation of skeletal-mesh assets, material overrides, and bone poses
- Registry-composed render pipeline with a forward-rendering path

## My Contributions

### 1. Skeletal Debug Render Pass and Bone Visualisation

- Added `FSkeletalDebugPass` as a separate editor render pass and registered it with the pass, pipeline, and shader registries
- Collected bone world transforms and parent indices from scene proxies and submitted them through the overlay path
- Built cone meshes to represent bone nodes and line batches to show parent–child relationships
- Allocated an independent constant buffer for each bone and connected draw-command shaders, meshes, render states, and sort keys
- Added a `SkeletalDebug` show flag and editor UI so mesh and skeleton visibility could be controlled independently
- Adjusted cone scale and debug-line depth testing for correct display across model sizes and occlusion relationships

Representative commits: [`60142eb6`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/60142eb6bedae7477692d1207a6f176b94e2dd07), [`51eee6dc`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/51eee6dc74ad5b1632e415764a6bcbfd1f49b5c8), [`fbb11a5f`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/fbb11a5fd8ee63775f89eeace4915583d487e49a)

### 2. Skeletal-Mesh Render Data and GPU-Resource Stabilisation

- Clarified buffer ownership and reference relationships among `FSkeletalSubMesh`, `FSkeletalMeshBuffer`, and `FMeshSectionRenderData`
- Connected each CPU-updated skinned SubMesh vertex buffer to section draw data in the scene proxy
- Correctly mapped material section indices to the complete override-material slot list and fixed draw-command sort keys
- Fixed material-base-index corruption following an invalid SubMesh buffer
- Prevented crashes when loading a new skeletal mesh into a Component that already owned one
- Fixed a render-buffer leak during skeletal SubMesh asset shutdown
- Integrated shader-contract-driven runtime vertex buffers with Component-level ownership and cleanup

Representative commits: [`05ef2395`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/05ef2395fa13dcc0c374831d3c1f8c4c3ae35012), [`e31657a6`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/e31657a6fc8cd4af6e32d12303200579405501ec), [`e66f09d1`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/e66f09d1ed12132e421a1a29da11ece958a93e08), [`83179c79`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/83179c79d98af773dedf35b63c0afc11e2586e21)

### 3. FBX Static-Mesh Import and Coordinate-System Correction

- Implemented `ImportStaticAndCacheAll` to traverse every mesh node in the FBX scene tree and convert it into static geometry
- Triangulated polygons and extracted positions, normals, UVs, tangents, and material sections
- Normalised FBX scenes to the engine's Z-up, X-forward, left-handed coordinate system and metre units
- Baked node global and geometric transforms into vertices and reversed triangle winding for negative determinants
- Merged vertices, indices, and sections from multiple mesh nodes into one static-mesh asset
- Consolidated material slots by slot name and connected the binary-cache output path
- Corrected axis-orientation mismatches between skeletal meshes and bones so poses and geometry aligned

Representative commits: [`4de83bd0`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/4de83bd05508376ea070a7a471f867b500459c88), [`0839c4a1`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/0839c4a16db4636e6dd1f678a79733dd70c462b5), [`a06149d3`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/a06149d3a069e7e17fd37e129efde6a770aa0452)

### 4. Precise Picking of Deformed Skeletal Meshes

- Added a broad phase that first tests the ray against the world AABB
- Transformed the ray into Component-local space and tested it against the currently CPU-skinned vertex and index arrays
- Selected the nearest result returned by a Möller–Trumbore-based `RaycastTriangles` implementation
- Recorded the actual `USkinnedMeshComponent` in the hit result and forwarded it to editor selection and the Details panel
- Based picking on the currently deformed surface rather than the reference mesh

Representative commit: [`ca537f5a`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/ca537f5a571804f8a9571a504fc5533b177ed417)

### 5. Viewer-to-Scene Spawning and Pose Persistence

- Added **Spawn in Scene** to the Skeletal Mesh Viewer and implemented the `ASkeletalMeshActor` creation path
- Copied the viewer's active mesh and bone-local matrices into the new scene Component so the edited pose was preserved
- Registered the Actor with the Editor World and octree for immediate rendering and selection
- Serialised the skeletal-mesh path, material slots, current bone-local matrices, and debug-pose mode
- Reconnected assets and restored saved material overrides and poses during load and duplication
- Added the missing Matrix property type to scene JSON and aligned `PostEditProperty` responsibility with the shared `USkinnedMeshComponent` flow
- Prepared `w10.Scene` and `w10_demo.Scene` to demonstrate viewer editing and scene placement

Representative commits: [`7f8f7869`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/7f8f786951e984bb15b18df501ff90286a7a6385), [`ff0fa51b`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/ff0fa51b1d6221946479692800313428f69635c0), [`59b5027b`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/59b5027bf12df0207dd8f04b9398606aec975ca9), [`cc83df4a`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/cc83df4a12701805825dbced5d8fca01bca2461a), [`f1cdc2de`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/f1cdc2de0668ab2f4be7bd5509b96342169203af)

## Skeletal Asset and Rendering Architecture

```text
FBX File
   │
   ▼
FFBXImporter
   ├─ Static Geometry ── Node Transform Bake ── UStaticMesh
   │
   └─ Skeletal Geometry
        ├─ USkeleton ── Bone Hierarchy / Bind Pose
        ├─ USkeletalMesh ── SubMesh / Section / Material
        └─ Bone Index / Weight
                    │
                    ▼
          USkinnedMeshComponent
      Current Bone Local / Global Matrix
                    │
              CPU Skinning
                    │
          Skinned Vertex Buffer
                    │
                    ▼
       FSkeletalMeshSceneProxy
       Per-Section Material / Shader Selection
                    │
                    ▼
      Forward Render Draw Commands

Skeletal Mesh Viewer
   ├─ Bone Hierarchy / Picking / Gizmo
   ├─ Reference / FBX Local Pose Debug
   ├─ SkeletalDebugPass ── Cone + Parent Line
   └─ Spawn in Scene ── Copy Pose ── Scene Save
```

## Editor Workflow

1. Import an FBX asset to generate static-mesh or skeletal-mesh and skeleton caches.
2. Open the skeletal-mesh asset in the dedicated viewer.
3. Select a bone in the hierarchy or pick it in the viewport.
4. Edit the selected bone's local transform with the gizmo and inspect the skinned result.
5. Compare geometry and debug poses using **Mesh / Skeleton** and **FBX Local Bones**.
6. Restore the reference pose with **Reset Pose**.
7. Use **Spawn in Scene** to place the current mesh and edited pose in the Editor World.
8. Save and reload the scene to verify persistence of the skeletal mesh, materials, and pose.

## Project Structure

```text
.
├─ PacificEngine.sln
├─ PacificEngine/
│  ├─ Asset/Content/
│  │  ├─ Models/                       # FBX and conversion-source assets
│  │  ├─ Materials/                    # Imported and test materials
│  │  └─ Scene/                        # FBX tests and Week 10 demos
│  ├─ Settings/
│  ├─ Shaders/Render/Editor/
│  │  └─ SkeletalDebug.hlsl
│  └─ Source/
│     ├─ Editor/
│     │  ├─ UI/EditorSkeletalMeshViewerPanel.*
│     │  └─ Viewport/                  # Skeletal-mesh preview viewer
│     └─ Engine/
│        ├─ Animation/                  # Early AnimationSequence work
│        ├─ Component/                  # Skinned/skeletal mesh Components
│        ├─ Mesh/                       # FBX importer, skeleton, skeletal mesh
│        └─ Render/
│           ├─ Execute/Passes/Editor/   # SkeletalDebugPass
│           ├─ Scene/Proxies/Primitive/
│           └─ RHI/D3D11/Buffers/
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ EditorBuild.bat
├─ GameBuild.bat
└─ DemoBuild.bat
```

## Building and Running

### Requirements

- Windows 10/11
- Visual Studio 2022
- MSVC v143 and Windows 10 SDK
- DirectX 11-capable GPU
- NuGet package restore
- Autodesk FBX SDK runtime and libraries included in the repository

### Build Instructions

1. Run `GenerateProjectFiles.bat` if Visual Studio project files need to be generated.
2. Open `PacificEngine.sln` in Visual Studio.
3. Restore the NuGet package `directxtk_desktop_win10`.
4. Build and run `Debug | x64` or `Release | x64`.

Depending on the build configuration, the project links `libfbxsdk.lib` from `ThirdParty/FBX_SDK/lib/debug` or `release` and copies the FBX SDK DLL into the runtime directory.

## Current Status and Limitations

- Targets Windows and DirectX 11.
- Skinning currently recalculates vertices on the CPU and updates GPU buffers, so its cost increases with complex meshes or many instances.
- `UAnimationSequence` and FBX animation extraction are explicitly marked as untested drafts in the code and should not be treated as complete animation-playback functionality.
- The Skeletal Mesh Viewer's **Save** button remains an unimplemented placeholder. Pose persistence uses **Spawn in Scene** followed by the normal scene-save path.
- Shader-specific runtime vertex-buffer caches are released when their owning objects are destroyed, but cache cleanup during repeated shader/material replacement could be improved further.
- Existing deferred/forward decal paths remain documented separately in `KNOWN_ISSUES.md` and are outside the Week 10 skeletal-work scope.

## Notes

- The complete collaboration history and team-wide changes are available in [MozziDog/Jungle_Week10_Team6](https://github.com/MozziDog/Jungle_Week10_Team6).
- The original repository was forked from [chodott/Jungle_Week9_Team2](https://github.com/chodott/Jungle_Week9_Team2), so history before 8 May 2026 was excluded from the Week 10 individual-contribution accounting.
- The prior private snapshot and the original `main` contained identical sets of 1,504 file blobs; neither had a root README.
- The 27 top-level commits attributed to Rocketstein during the core development period include merges and squashes. Contributions were therefore identified by reviewing commit messages, changed files, and the final code together.
- Bone-hierarchy and gizmo work in the Skeletal Mesh Viewer, skeletal FBX import, CPU skinning, and final render-pipeline integration also include contributions from other team members.

---

## 한국어

# Week 10 — Pacific Engine: Skeletal Mesh & FBX

> DirectX 11 기반 커스텀 엔진에 FBX Asset Pipeline, Skeletal Mesh Skinning과 전용 편집 도구를 구축한 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/MozziDog/Jungle_Week10_Team6)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

Week 9 엔진을 기반으로 정적·스켈레탈 FBX Asset을 불러오고, Skeleton Hierarchy와 Skin Weight를 이용해 캐릭터의 Pose를 편집·렌더링하는 파이프라인을 구현했습니다. FBX SDK에서 추출한 Mesh, Section, Material, Bone과 Bind Pose 정보를 엔진 Asset으로 변환하고, CPU Skinning 결과를 DirectX 11 Vertex Buffer와 Scene Proxy에 연결했습니다.

최종 팀 결과물은 Skeletal Mesh Viewer, Bone Hierarchy·Gizmo 편집, Skeleton Debug Overlay, Scene Spawn과 Pose 저장을 지원합니다. 아래 **담당 작업**은 이 가운데 Rocketstein이 직접 작성한 커밋과 실제 변경 파일을 기준으로 구분했습니다.

- **핵심 개발 기간:** 2026.05.08 ~ 2026.05.14
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, Autodesk FBX SDK, CPU Skinning, HLSL, Dear ImGui
- **담당 영역:** Skeletal Debug Pass, Skeletal Render Resource, FBX Static Import, Mesh Picking, Scene Spawn·Pose Persistence
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- Autodesk FBX SDK 기반 Static / Skeletal Mesh Import
- FBX Scene의 좌표축·단위계 변환과 Mesh Node 순회
- Skeleton Hierarchy, Reference / Display Pose와 Bind Matrix 관리
- Vertex별 Bone Index·Weight를 이용한 CPU Skinning
- SubMesh / Section별 Material Slot과 Shader별 동적 Vertex Layout
- Skeletal Mesh Viewer의 Bone Hierarchy, Picking, Transform Gizmo와 Pose Reset
- Skin Bind Pose / FBX Local Pose Skeleton Debug View
- Bone Node와 부모–자식 연결을 표시하는 Skeletal Debug Render Pass
- Viewer에서 편집한 Skeletal Mesh와 Pose의 Scene Spawn
- Skeletal Mesh Asset, Material Override와 Bone Pose의 Scene 직렬화
- Registry 기반 조립형 Render Pipeline과 Forward Rendering 경로

## 담당 작업

### 1. Skeletal Debug Render Pass와 Bone 시각화

- `FSkeletalDebugPass`를 별도 Editor Render Pass로 추가하고 Pass / Pipeline / Shader Registry에 등록
- Scene Proxy에서 Bone의 World Transform과 Parent Index를 수집해 Overlay Submission 경로로 전달
- Bone Node를 표시하는 Cone Mesh와 부모–자식 관계를 나타내는 Line Batch 구성
- Bone마다 독립된 Constant Buffer를 할당하고 Draw Command의 Shader, Mesh, Render State와 Sort Key 연결
- `SkeletalDebug` Show Flag와 에디터 UI를 추가해 Mesh와 Skeleton 표시를 독립적으로 전환
- Cone 크기와 Debug Line의 Depth Test 상태를 보정해 모델 크기·가림 관계에 맞게 표시

대표 커밋: [`60142eb6`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/60142eb6bedae7477692d1207a6f176b94e2dd07), [`51eee6dc`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/51eee6dc74ad5b1632e415764a6bcbfd1f49b5c8), [`fbb11a5f`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/fbb11a5fd8ee63775f89eeace4915583d487e49a)

### 2. Skeletal Mesh 렌더 데이터와 GPU Resource 안정화

- `FSkeletalSubMesh`, `FSkeletalMeshBuffer`, `FMeshSectionRenderData` 사이의 Buffer 소유·참조 관계 정리
- CPU Skinning으로 갱신된 SubMesh별 Vertex Buffer를 Scene Proxy의 Section Draw Data에 연결
- Material Section Index를 전체 Override Material Slot에 올바르게 대응시키고 Draw Command Sort Key 보정
- 유효하지 않은 SubMesh Buffer가 있을 때 이후 Material Base Index가 어긋나는 문제 수정
- 이미 Mesh를 보유한 Component에 새 Skeletal Mesh를 로드할 때 발생하던 Crash 방지
- Skeletal SubMesh Asset 종료 시 Render Buffer가 해제되지 않던 Resource Leak 수정
- Shader 입력 계약에 따라 생성한 Runtime Vertex Buffer를 Component 단위로 관리·정리하도록 후속 구조에 연결

대표 커밋: [`05ef2395`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/05ef2395fa13dcc0c374831d3c1f8c4c3ae35012), [`e31657a6`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/e31657a6fc8cd4af6e32d12303200579405501ec), [`e66f09d1`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/e66f09d1ed12132e421a1a29da11ece958a93e08), [`83179c79`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/83179c79d98af773dedf35b63c0afc11e2586e21)

### 3. FBX Static Mesh Import와 좌표계 보정

- FBX Scene Tree의 모든 Mesh Node를 순회해 정적 Geometry로 변환하는 `ImportStaticAndCacheAll` 구현
- Polygon을 Triangle로 변환하고 Position, Normal, UV, Tangent와 Material Section 추출
- FBX Scene을 엔진의 Z-up, X-forward, Left-handed 좌표계와 Meter 단위로 정규화
- Node Global Transform과 Geometric Transform을 Vertex에 Bake하고 음수 Determinant일 때 Triangle Winding 반전
- 여러 Mesh Node의 Vertex / Index / Section을 하나의 Static Mesh Asset으로 병합
- Material Slot Name을 기준으로 Slot을 통합하고 Binary Cache 저장 경로 연결
- Skeletal Mesh와 Bone 사이의 축 방향·Orientation 불일치를 수정해 Pose와 Geometry 정렬

대표 커밋: [`4de83bd0`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/4de83bd05508376ea070a7a471f867b500459c88), [`0839c4a1`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/0839c4a16db4636e6dd1f678a79733dd70c462b5), [`a06149d3`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/a06149d3a069e7e17fd37e129efde6a770aa0452)

### 4. 변형된 Skeletal Mesh의 정밀 Picking

- World AABB와 Ray의 교차 여부를 먼저 검사하는 Broad Phase 구성
- Ray를 Component Local Space로 변환하고 현재 CPU Skinning이 적용된 Vertex / Index 배열을 대상으로 Triangle 검사
- Möller–Trumbore 알고리즘 기반 `RaycastTriangles` 결과 중 가장 가까운 Hit 선택
- 선택 결과에 실제 `USkinnedMeshComponent`를 기록해 Editor Selection과 Details Panel로 전달
- Reference Mesh가 아닌 현재 Pose의 변형된 표면을 기준으로 Picking하도록 구성

대표 커밋: [`ca537f5a`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/ca537f5a571804f8a9571a504fc5533b177ed417)

### 5. Viewer→Scene Spawn과 Pose 영속화

- Skeletal Mesh Viewer에 **Spawn in Scene** 동작을 추가하고 `ASkeletalMeshActor` 생성 경로 구현
- Viewer의 Active Mesh와 Bone Local Matrix를 새 Scene Component에 복사해 편집한 Pose 그대로 배치
- Actor를 Editor World와 Octree에 등록해 Spawn 직후 렌더링·선택되도록 연결
- Skeletal Mesh Path, Material Slot, 현재 Bone Local Matrix와 Debug Pose Mode 직렬화
- Load / Duplicate 시 Asset을 다시 연결한 뒤 저장된 Material Override와 Pose를 복원
- Scene JSON의 Matrix Property Type 누락을 수정하고 `PostEditProperty` 책임을 공통 `USkinnedMeshComponent` 흐름에 맞게 정리
- 발표용 `w10.Scene`, `w10_demo.Scene`을 통해 Viewer 편집과 Scene 배치 결과 구성

대표 커밋: [`7f8f7869`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/7f8f786951e984bb15b18df501ff90286a7a6385), [`ff0fa51b`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/ff0fa51b1d6221946479692800313428f69635c0), [`59b5027b`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/59b5027bf12df0207dd8f04b9398606aec975ca9), [`cc83df4a`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/cc83df4a12701805825dbced5d8fca01bca2461a), [`f1cdc2de`](https://github.com/MozziDog/Jungle_Week10_Team6/commit/f1cdc2de0668ab2f4be7bd5509b96342169203af)

## Skeletal Asset·렌더링 구조

```text
FBX File
   │
   ▼
FFBXImporter
   ├─ Static Geometry ── Node Transform Bake ── UStaticMesh
   │
   └─ Skeletal Geometry
        ├─ USkeleton ── Bone Hierarchy / Bind Pose
        ├─ USkeletalMesh ── SubMesh / Section / Material
        └─ Bone Index / Weight
                    │
                    ▼
          USkinnedMeshComponent
      Current Bone Local / Global Matrix
                    │
              CPU Skinning
                    │
          Skinned Vertex Buffer
                    │
                    ▼
       FSkeletalMeshSceneProxy
       Section별 Material·Shader 선택
                    │
                    ▼
      Forward Render Draw Commands

Skeletal Mesh Viewer
   ├─ Bone Hierarchy / Picking / Gizmo
   ├─ Reference·FBX Local Pose Debug
   ├─ SkeletalDebugPass ── Cone + Parent Line
   └─ Spawn in Scene ── Pose 복사 ── Scene Save
```

## 에디터 사용 흐름

1. FBX Asset을 Import해 Static Mesh 또는 Skeletal Mesh와 Skeleton Cache를 생성합니다.
2. Skeletal Mesh Asset을 전용 Viewer에서 엽니다.
3. Bone Hierarchy에서 Bone을 선택하거나 Viewport에서 Picking합니다.
4. Gizmo로 선택 Bone의 Local Transform을 수정하고 Skinning 결과를 확인합니다.
5. **Mesh / Skeleton**, **FBX Local Bones** 옵션으로 Geometry와 Debug Pose를 비교합니다.
6. **Reset Pose**로 Reference Pose를 복원합니다.
7. **Spawn in Scene**으로 현재 Mesh와 편집 Pose를 Editor World에 배치합니다.
8. Scene을 저장·다시 불러와 Skeletal Mesh, Material과 Pose가 유지되는지 확인합니다.

## 프로젝트 구조

```text
.
├─ PacificEngine.sln
├─ PacificEngine/
│  ├─ Asset/Content/
│  │  ├─ Models/                       # FBX 및 변환 대상 Asset
│  │  ├─ Materials/                    # Import·테스트 Material
│  │  └─ Scene/                        # FBX Test, Week 10 Demo
│  ├─ Settings/
│  ├─ Shaders/Render/Editor/
│  │  └─ SkeletalDebug.hlsl
│  └─ Source/
│     ├─ Editor/
│     │  ├─ UI/EditorSkeletalMeshViewerPanel.*
│     │  └─ Viewport/                  # Skeletal Mesh Preview Viewer
│     └─ Engine/
│        ├─ Animation/                  # AnimationSequence 초안
│        ├─ Component/                  # Skinned / Skeletal Mesh Component
│        ├─ Mesh/                       # FBX Importer, Skeleton, Skeletal Mesh
│        └─ Render/
│           ├─ Execute/Passes/Editor/   # SkeletalDebugPass
│           ├─ Scene/Proxies/Primitive/
│           └─ RHI/D3D11/Buffers/
├─ Scripts/
├─ GenerateProjectFiles.bat
├─ EditorBuild.bat
├─ GameBuild.bat
└─ DemoBuild.bat
```

## 빌드 및 실행

### 요구 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143, Windows 10 SDK
- DirectX 11 지원 GPU
- NuGet Package Restore
- 저장소에 포함된 Autodesk FBX SDK Runtime / Library

### 빌드

1. 필요하면 `GenerateProjectFiles.bat`을 실행해 Visual Studio 프로젝트 파일을 생성합니다.
2. `PacificEngine.sln`을 Visual Studio에서 엽니다.
3. NuGet의 `directxtk_desktop_win10` Package를 복원합니다.
4. `Debug | x64` 또는 `Release | x64`로 빌드하고 실행합니다.

빌드 구성에 따라 `ThirdParty/FBX_SDK/lib/debug` 또는 `release`의 `libfbxsdk.lib`를 링크하며, 실행 디렉터리에 FBX SDK DLL을 복사합니다.

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- Skinning은 현재 CPU에서 Vertex를 다시 계산하고 GPU Buffer를 갱신하는 방식이므로 복잡한 Mesh나 Instance가 많을 때 비용이 증가합니다.
- `UAnimationSequence`와 FBX Animation 추출부는 코드에서도 테스트되지 않은 초안으로 명시되어 있어, 완성된 Animation Playback 기능으로 보지 않습니다.
- Skeletal Mesh Viewer의 **Save** 버튼은 최종 코드에서 동작이 구현되지 않은 Placeholder이며, Scene의 Pose 저장은 **Spawn in Scene** 후 Scene Save 경로를 사용합니다.
- Shader별 Runtime Vertex Buffer Cache는 소유 객체가 파괴될 때 해제되지만, 반복적인 Shader / Material 교체 중 Cache 정리 정책에는 추가 개선 여지가 있습니다.
- 기존 Deferred / Forward Decal 경로는 `KNOWN_ISSUES.md`에 별도 Known Issue로 남아 있으며 Week 10 Skeletal 작업 범위에 포함되지 않습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [MozziDog/Jungle_Week10_Team6](https://github.com/MozziDog/Jungle_Week10_Team6)에서 확인할 수 있습니다.
- 원본은 [chodott/Jungle_Week9_Team2](https://github.com/chodott/Jungle_Week9_Team2)에서 Fork되었으므로 2026.05.08 이전 이력은 Week 10 개인 기여 집계에서 제외했습니다.
- 비공개 저장소의 기존 스냅샷은 원본 `main`과 1,504개 File Blob이 모두 동일했으며, 양쪽 모두 루트 README는 없었습니다.
- 핵심 개발 기간에 Rocketstein 작성자 정보로 기록된 27개 Top-level Commit에는 Merge·Squash Commit이 포함되어 있어, 담당 작업은 커밋 메시지뿐 아니라 실제 변경 파일과 최종 코드를 함께 확인해 정리했습니다.
- Skeletal Mesh Viewer의 Hierarchy·Gizmo, FBX Skeletal Import, CPU Skinning과 Render Pipeline 최종 통합에는 팀원의 작업도 포함되어 있습니다.