# Week 2 — ZZUP Engine

> C++와 DirectX 11로 구현한 게임 엔진 및 에디터 프로젝트입니다.  
> 이 저장소는 [원본 팀 프로젝트](https://github.com/JHCard7872/Week2)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

렌더링 루프부터 오브젝트·컴포넌트 구조, 카메라, 피킹, 기즈모, 씬 직렬화, ImGui 기반 편집 도구까지 게임 엔진의 핵심 흐름을 직접 구현했습니다.

- **개발 기간:** 2026.03.13 ~ 2026.03.18
- **개발 환경:** Windows, Visual Studio 2022, C++17
- **주요 기술:** DirectX 11, HLSL, Dear ImGui, JSON
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- DirectX 11 기반 프리미티브 렌더링과 렌더 패스 관리
- `UObject` 기반 `AActor` / `UActorComponent` 구조 및 런타임 타입 정보
- Perspective / Orthographic 카메라와 키보드·마우스 뷰포트 조작
- Ray Casting 기반 오브젝트 피킹
- 이동·회전·크기 조절 기즈모 및 World / Local 좌표계 전환
- ImGui 기반 오브젝트 생성, 속성 편집, 삭제, 콘솔 및 상태 모니터링
- JSON 기반 씬 저장·불러오기와 New Scene 워크플로
- UObject - 오브젝트 최상위 클래스 및 팩토리 패턴 정의 

## 담당 작업

### 1. 엔진 수학 및 카메라·입력 기반 구현

- `FVector`, `FVector4`, `FMatrix` 연산과 변환 행렬 기반 마련
- View / Projection 행렬을 사용하는 카메라 구현
- 카메라를 `USceneComponent`로 리팩터링해 Transform 계층과 연동
- 키보드 이동, 마우스 드래그 회전, 휠 줌, Perspective / Orthographic 전환 구현 및 조작 오류 보정

대표 커밋: [`0236441`](https://github.com/JHCard7872/Week2/commit/0236441), [`314fc53`](https://github.com/JHCard7872/Week2/commit/314fc53), [`9d51e26`](https://github.com/JHCard7872/Week2/commit/9d51e26), [`08bd010`](https://github.com/JHCard7872/Week2/commit/08bd010), [`7de8997`](https://github.com/JHCard7872/Week2/commit/7de8997)

### 2. 오브젝트·월드 구조와 수명주기 관리

- `UWorld`, `AActor`, `UObjectManager` 구조 설계 및 구현
- 매크로 기반 런타임 타입 정보(RTTI), `IsA` / `Cast` 지원
- Actor–Component 소유 관계와 Root Component 연결
- Pending Kill 객체 정리 시스템 및 메모리 관련 오류 수정
- 타입 이름으로 객체를 생성할 수 있는 Object Factory 기반 마련

대표 커밋: [`442fbe4`](https://github.com/JHCard7872/Week2/commit/442fbe4), [`2447db4`](https://github.com/JHCard7872/Week2/commit/2447db4), [`14e5f9b`](https://github.com/JHCard7872/Week2/commit/14e5f9b), [`9376142`](https://github.com/JHCard7872/Week2/commit/9376142), [`e80a445`](https://github.com/JHCard7872/Week2/commit/e80a445)

### 3. 렌더 수집 구조와 에디터 상호작용

- World / Actor / Component의 렌더 정보를 모으는 `FRenderCollector` 도입
- 화면 좌표를 월드 Ray로 변환해 가장 가까운 프리미티브를 선택하는 피킹 구현
- 선택 객체 정보를 ImGui 패널에 연결하고 Transform을 실시간 편집하도록 구성
- 이동·회전·크기 조절 기즈모의 드래그, 축 선택, 화면 크기 보정 및 콘솔 피드백 개선

대표 커밋: [`dfdc2ad`](https://github.com/JHCard7872/Week2/commit/dfdc2ad), [`2501ef2`](https://github.com/JHCard7872/Week2/commit/2501ef2), [`ac91906`](https://github.com/JHCard7872/Week2/commit/ac91906), [`dcb967e`](https://github.com/JHCard7872/Week2/commit/dcb967e)

### 4. 씬 직렬화와 에디터 워크플로

- 오브젝트 타입과 Transform을 JSON으로 저장하고 복원하는 `FSceneSaveManager` 구현
- New / Save / Load Scene 기능을 ImGui 컨트롤 패널에 통합
- 로드 후 UI 선택 상태와 뷰포트를 안전하게 초기화하도록 개선
- 잘못된 경로, 반복 저장·불러오기, 파일 입출력 충돌로 인한 크래시 수정
- 씬 작업 완료 알림과 FOV·카메라·다중 Spawn 조작 개선

대표 커밋: [`3ffbf67`](https://github.com/JHCard7872/Week2/commit/3ffbf67), [`bf68f99`](https://github.com/JHCard7872/Week2/commit/bf68f99), [`8109fef`](https://github.com/JHCard7872/Week2/commit/8109fef), [`43056a8`](https://github.com/JHCard7872/Week2/commit/43056a8), [`7577e9f`](https://github.com/JHCard7872/Week2/commit/7577e9f)

## 조작 방법

| 입력 | 동작 |
| --- | --- |
| `W` / `A` / `S` / `D` | 카메라 전후·좌우 이동 |
| `Q` / `E` | 카메라 하강·상승 |
| 방향키 또는 마우스 우클릭 드래그 | 카메라 회전 |
| 마우스 휠 | FOV 또는 Orthographic Width 조절 |
| `O` | Perspective / Orthographic 전환 |
| 마우스 좌클릭 | 오브젝트 또는 기즈모 축 선택 |
| 좌클릭 드래그 | 선택한 기즈모로 Transform 편집 |
| `Space` | Translate → Rotate → Scale 모드 순환 |

ImGui의 **Jungle Control Panel**에서는 프리미티브 생성, 씬 저장·불러오기, 카메라 설정, 기즈모 모드와 좌표계를 조작할 수 있습니다.

## 프로젝트 구조

```text
Week2/
├─ Editor/                 # 에디터 엔진, 뷰포트 입력, ImGui UI
├─ Engine/
│  ├─ Classes/            # Actor 계층
│  ├─ Core/               # 입력, 공용 타입, 콘솔
│  ├─ Math/               # Vector, Matrix, 수학 유틸리티
│  ├─ Physics/            # 충돌 및 Ray 타입
│  ├─ Render/             # D3D11 장치, 리소스, 렌더 명령·수집·파이프라인
│  └─ Scene/              # Camera 및 씬 저장 관리자
├─ Object/                # UObject, Object Manager, Object Factory
├─ World/                 # Component, Primitive, Gizmo, Mesh
├─ Saves/                 # 저장된 씬 데이터
└─ ShaderW0.hlsl          # 프리미티브·그리드·아웃라인·기즈모 셰이더
```

## 빌드 및 실행

1. Windows에서 Visual Studio 2022로 `Week2.sln`을 엽니다.
2. `Debug` 또는 `Release`, `x64` 구성을 선택합니다.
3. 솔루션을 빌드한 뒤 실행합니다.

프로젝트는 MSVC `v143`, Windows 10 SDK, C++17을 기준으로 구성되어 있습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [JHCard7872/Week2](https://github.com/JHCard7872/Week2)에서 확인할 수 있습니다.
- 이 README의 개인 기여 내역은 원본 저장소의 작성자 정보, 커밋 메시지 및 실제 변경 파일을 함께 확인해 정리했습니다.
