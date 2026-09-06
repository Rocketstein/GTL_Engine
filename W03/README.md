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

대표 커밋: [`41835850`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/418358502d14077e60dd087ac49b41200fc6c123), [`e1f1079f`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/e1f1079f4bbfb67cc1863cc3e60abd2d4489b94b), [`de191bd1`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/de191bd12a93c217926db2660473bbec570f2031), [`c0310ef7`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/c0310ef730b90ffecf5f826c9851559ff7482323)

### 2. World·Scene 구조 재설계

- `UWorld`가 `FSceneManager`를 통해 여러 `UScene`과 Active Scene을 관리하도록 구조 확장
- Actor의 생성·Tick·종료 책임을 Scene 단위로 분리
- Camera와 Gizmo의 관리 책임을 `FEditorViewportClient`로 이동해 에디터 엔진 의존성 축소
- Scene 생성, 전환, 삭제 시 카메라·기즈모·선택 상태가 함께 갱신되도록 연결
- Scene Manager 창과 Loaded Scene 선택 UI 구현

대표 커밋: [`331848b3`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/331848b39cfaebeb8bc052eb07477abfe3e269f3), [`9c09ad1e`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/9c09ad1e882d572ccdbd7ed8b1187d8a1d4c2eaf), [`f9f74aed`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/f9f74aed0bee6d993514bb302739dd9936807e31), [`ac7ca66a`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/ac7ca66ab748005fc378b0ef359247ee4f81dafd)

### 3. 씬 저장·불러오기 복구 및 확장

- 리팩터링된 World·Scene 구조에 맞춰 기존 JSON 저장·불러오기 기능 복구
- Scene, Actor, Component를 UUID로 직렬화하고 로드 후 부모·Root Component·소유 Scene 관계를 재연결
- 파일 대화상자를 통한 `.Scene` 저장·불러오기와 중복 UUID 방지 처리
- 씬 전환 후 뷰포트와 기즈모가 이전 씬 객체를 계속 참조하는 문제 수정

대표 커밋: [`331848b3`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/331848b39cfaebeb8bc052eb07477abfe3e269f3), [`f9f74aed`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/f9f74aed0bee6d993514bb302739dd9936807e31), [`badc62ca`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/badc62ca5e7f5292c5847fc075b67d54d93636c3)

### 4. Billboard·Spotlight 기반 구현

- 항상 카메라를 향하는 텍스처 Quad인 `UBillBoardComponent` 구현
- Billboard 텍스처 로딩, Ray Casting 피킹, AABB 갱신, 직렬화 지원
- `ULightComponent`, `USpotlightComponent`, `ASpotlight` 구조 설계 및 에디터 Spawn 연동
- Spotlight의 높이·반지름·Yaw·Pitch로 Cone 정점을 계산하고 Line Batch로 시각화
- 선택한 Spotlight의 방향, 크기, 정점 수, 색상을 ImGui에서 실시간 편집하도록 구현
- Spotlight 아이콘과 렌더 수집·메시 버퍼·저장 시스템 연동

대표 커밋: [`eb35897c`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/eb35897c08b5f2ebf35ee2dde47b16928de54b91), [`bff8cb6f`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/bff8cb6f0142d25ff34c5e7196781250d15d9042), [`f9f74aed`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/f9f74aed0bee6d993514bb302739dd9936807e31), [`badc62ca`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/badc62ca5e7f5292c5847fc075b67d54d93636c3)

### 5. 통합 안정화와 후속 수정

- SubUV가 다른 객체 뒤에서 잘못 가려지는 Shadowing 문제와 씬 로드 후 텍스처 복원 문제 수정
- Scene·Actor·Object Factory 전반의 명명과 참조 관계 정리

대표 커밋: [`28e5c81c`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/28e5c81c5132f9b7290fd195e0179f2c33fe110c), [`badc62ca`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/badc62ca5e7f5292c5847fc075b67d54d93636c3), [`d51f10e7`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/d51f10e76d0fdfbec9008bab15671fc3aa7f3d6a), [`1b3ef804`](https://github.com/Rocketstein/Game-Tech-Lab-W3/commit/1b3ef80432532ffa839872a0931868e242a555a1)

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

