# Week 11 — Krafton Engine: Property Reflection

> C++ 멤버의 메타데이터를 빌드 타임에 생성하고, 에디터와 직렬화가 같은 스키마를 사용하도록 구축한 Unreal 스타일 프로퍼티 리플렉션 프로젝트입니다.  
> 이 저장소는 [원본 협업 프로젝트](https://github.com/keonwookang0914/Jungle_Week11_Team4)의 결과물 중 **Rocketstein (Hyungjun Kim)**의 작업을 중심으로 정리한 포트폴리오 스냅샷입니다.

## 프로젝트 개요

이전 주차의 DirectX 11 기반 엔진·에디터를 확장해, 클래스마다 수작업으로 작성하던 속성 UI와 저장 코드를 공통 메타데이터 계층으로 통합했습니다. 헤더에 `UCLASS`, `USTRUCT`, `UENUM`, `UPROPERTY`를 선언하면 Python 코드 생성기가 등록 코드를 만들고, 런타임의 `UClass`·`UScriptStruct`·`UEnum`과 다형적 `FProperty`가 이를 소비합니다.

Week 11의 핵심은 단순한 매크로 추가가 아니라 **C++ 선언 → 코드 생성 → 런타임 등록 → 에디터 편집 → JSON 저장·복원**으로 이어지는 하나의 데이터 경로를 만든 것입니다.

- **개발 기간:** 2026.05.14 ~ 2026.05.20
- **개발 환경:** Windows, Visual Studio 2022, C++20
- **주요 기술:** DirectX 11, HLSL, Dear ImGui, Python Code Generation, JSON
- **프로젝트 형태:** 팀 프로젝트 / 개인 기여 중심 포트폴리오

## 주요 기능

- `UCLASS` / `USTRUCT` / `UENUM` / `UPROPERTY` 기반 헤더 코드 생성
- 클래스·구조체·열거형 메타데이터의 정적 등록과 이름 기반 조회
- 값과 메타데이터를 분리한 다형적 `FProperty` 계층
- Bool, 수치, 문자열, 벡터, 열거형, 구조체, 배열, 에셋 참조 프로퍼티
- 상속 관계를 포함한 편집 가능·직렬화 가능 프로퍼티 열거
- `DisplayName`, `Category`, 범위, 편집·Transient 계열 Flag 메타데이터
- ImGui Property Panel의 타입별 Widget 자동 구성
- 동일한 프로퍼티 스키마를 사용하는 JSON Scene 저장·복원
- `UObject → UField → UStruct → UClass` 메타 객체 계층과 `UEnum`
- Skeletal Animation, Anim Notify, IK 등 팀 단위 Week 11 기능과의 통합

## 담당 작업

### 1. 프로퍼티 리플렉션 기반과 코드 생성 파이프라인

- 기존 프로젝트 구조를 자동으로 수집하는 `GenerateProjectFiles.py`를 정비하고 생성 소스를 빌드 대상에 포함
- 헤더의 리플렉션 마커를 파싱해 `.generated.h`와 `.gen.cpp`를 생성하는 흐름 구축
- 클래스·구조체·열거형 Registry와 프로퍼티 타입 분류, 정적 Registrar 생성 구현
- `GENERATED_BODY`가 `StaticClass`, `StaticStruct`, 상위 타입, Registrar 접근 코드를 제공하도록 구성
- `DisplayName`, `Category`, Min / Max / Speed, Property Flag를 생성 코드에 반영

대표 커밋: [`b306f657`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/b306f657d7999f8ec58d87ed6a21be8693040d2d), [`20e1b79c`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/20e1b79ce696ce70dfcf272fbe6e4b8727ccdc11), [`ad1a23b9`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/ad1a23b9d047bfa2d8287517c332b39e1932b494)

### 2. 다형적 `FProperty`와 Schema / Instance 분리

- 값 포인터를 매 접근마다 복사하던 단일 `FProperty`를 타입별 파생 클래스 구조로 전환
- `FProperty`가 값 자체를 소유하지 않고 Name, Flag, Offset과 타입별 메타데이터만 보관하도록 재설계
- `ContainerPtrToValuePtr`로 객체·구조체·배열 원소의 실제 주소를 계산해 스키마 재사용 가능
- 복사로 인한 Object Slicing을 제거하고 `Serialize` / `Deserialize`를 가상 함수 기반 타입별 동작으로 분리
- `FArrayProperty`의 Inner Property 소유권과 재귀적 접근 구조 정리

대표 커밋: [`3375dd14`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/3375dd14), [`6ee18304`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/6ee18304), [`60d02c8d`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/60d02c8d69a42be1612169ffce19353e574b9246), [`167e2b52`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/167e2b52506db743dfca98dd3d1d9b05926ebcc0)

### 3. 에디터·직렬화 연동과 기존 컴포넌트 마이그레이션

- `EditorPropertyWidget`이 객체의 편집 가능 프로퍼티를 순회하고 타입별 ImGui Widget을 그리도록 연결
- 배열과 구조체가 중첩되어도 Container를 재귀적으로 전달해 동일한 Property UI 경로 사용
- `FArrayAccessor`와 배열 직렬화·역직렬화, `FixedSize` 처리 및 배열 원소 변경 전파 구현
- 상위 클래스부터 파생 클래스까지 프로퍼티를 수집하고 `UPROPERTY_HIDE`로 상속 프로퍼티를 숨기는 기능 추가
- Actor, Light, Billboard, Scene·Mesh·Movement 계열 Component의 수작업 프로퍼티 코드를 매크로 기반 등록으로 이전
- Scene 저장기가 Non-Transient 프로퍼티만 공통 `Serialize` / `Deserialize` 경로로 처리하도록 통합

대표 커밋: [`60d02c8d`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/60d02c8d69a42be1612169ffce19353e574b9246), [`167e2b52`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/167e2b52506db743dfca98dd3d1d9b05926ebcc0)

### 4. `UField`·`UStruct`·`UEnum` 메타 객체 계층

- 리플렉션 타입을 `UObject → UField → UStruct → UClass` 상속 구조로 정리
- `UStruct`에 SuperStruct, 크기, Child Property와 상속 순회·이름 조회 기능 구성
- `UEnum`에 이름–정수 값 테이블, Underlying Size, C++ 표현 형식과 양방향 조회 기능 구현
- 코드 생성기가 일반 enum과 `enum class`, `USTRUCT`의 Child Property를 등록하도록 확장
- 정적 메타 객체를 Object Array 초기화 이후 안전하게 등록하는 Deferred Registration 흐름 연결
- 핵심 타입의 Cast Flag를 상속해 자주 쓰는 `IsA` 검사를 빠르게 처리할 기반 마련

대표 커밋: [`d43e6524`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/d43e652438ac7f6f7deffc65139c23b140f9473b), [`167e2b52`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/167e2b52506db743dfca98dd3d1d9b05926ebcc0)

### 5. 프로퍼티 수명 정책과 후속 Reflection / GC 설계

- 복제 시 초기화해야 하는 값을 구분하도록 `DuplicateTransient`, `NonPIEDuplicateTransient` Flag 추가
- `FField`, `FFieldClass`, `FFieldVariant`의 기초 타입과 문서를 추가해 경량 메타 필드 계층 실험
- Object / Class Property, `FField` Iterator, CDO, Mark-and-Sweep GC, `UFunction`으로 이어지는 구현 순서와 의존성 정리
- 최종 코드 기준의 Property Reflection 문서와 특수 프로퍼티 타입 동작·제약 갱신

대표 커밋: [`f6784016`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/f67840161ebb5e9c0fc1795f001e17e1ab916c1c), [`bb179ed5`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/bb179ed5059e930a5d5bc824a9488f42c5ca01d8), [`aaaeeb43`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/aaaeeb43518fe7561804f0d0faf49cc407f8b749), [`8c1d5e1b`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/8c1d5e1b9496c1293e5aad41855ca12ef0626678), [`a9702b62`](https://github.com/keonwookang0914/Jungle_Week11_Team4/commit/a9702b6251019f9f4e80df1424758eff2e768608)

## 리플렉션 데이터 흐름

```text
C++ Header
UCLASS / USTRUCT / UENUM / UPROPERTY
                    │
                    ▼
          Scripts/GenerateCode.py
             │                 │
             ▼                 ▼
    *.generated.h          *.gen.cpp
  Generated Body       Metadata / Registrar
             └───────────────┬─┘
                             ▼
              UClass / UScriptStruct / UEnum
                             │
                        FProperty Schema
                 Name · Flag · Offset · Type
                       ┌─────┴─────┐
                       ▼           ▼
            EditorPropertyWidget  SceneSaveManager
              ImGui 속성 편집      JSON 저장·복원
```

`FProperty`는 인스턴스 주소를 보관하지 않습니다. 에디터와 직렬화 시스템은 동일한 스키마를 받아 `Container + Offset`으로 실제 값에 접근하므로, 타입별 동작을 중복 구현하지 않고 재사용할 수 있습니다.

## 프로젝트 구조

```text
.
├─ Docs/
│  ├─ PropertyReflectionSystem.md    # 현행 리플렉션 구조
│  ├─ SpecialPropertyTypes.md        # Enum / Struct / Array / 참조 타입
│  └─ GC Plan.md                     # FField·CDO·GC·UFunction 후속 계획
├─ KraftonEngine/
│  ├─ Source/
│  │  ├─ Engine/Core/Property/       # FProperty 계층과 FField 실험
│  │  ├─ Engine/Core/UObject/        # Weak / Soft Object Pointer
│  │  ├─ Engine/Object/              # UObject, UField, UStruct, UClass, UEnum
│  │  ├─ Engine/Serialization/       # Property 기반 Scene 저장·복원
│  │  └─ Editor/UI/                  # Property Panel
│  ├─ Intermediate/Generated/        # 생성된 Header / Source
│  ├─ Shaders/
│  └─ ThirdParty/
├─ Scripts/
│  ├─ GenerateCode.py                # 리플렉션 코드 생성
│  └─ GenerateProjectFiles.py        # Visual Studio 프로젝트 생성
├─ KraftonEngine.sln
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
- Python은 저장소의 `Scripts/python` 런타임 사용

### 개발 빌드

1. 파일 추가·이동 후 프로젝트를 다시 구성하려면 `GenerateProjectFiles.bat`을 실행합니다.
2. `KraftonEngine.sln`을 Visual Studio에서 엽니다.
3. `Debug | x64` 또는 `Release | x64`로 빌드합니다.
4. 빌드 전 단계에서 `GenerateCode.py`가 리플렉션 생성 파일을 갱신합니다.

### 실행 빌드

- `GameBuild.bat`: `Game | x64` 구성 빌드
- `ReleaseBuild.bat`: `Release | x64` 구성 빌드

## 현재 상태와 한계

- Windows + DirectX 11 환경을 대상으로 합니다.
- `UClass`, `UScriptStruct`, `UEnum`과 코드 생성 기반 프로퍼티 등록은 동작합니다.
- 에디터 UI와 Scene JSON 저장·복원은 동일한 `FProperty` 스키마를 사용합니다.
- 일반적인 Hard `UObject*` / `UClass*` Property는 아직 완성 단계가 아니며 일부 참조 타입은 전용 Property로 처리합니다.
- `FField`, `FFieldClass`, `FFieldVariant`는 기초 구현·검토 단계로, 현재 `FProperty`와 `UStruct::ChildProperties`에 완전히 통합되지 않았습니다.
- CDO, Mark-and-Sweep GC, `TFieldIterator`, C++ Call-by-Name 방식의 `UFunction`은 구현 계획에 포함된 후속 범위입니다.
- `UFUNCTION` 파싱과 Lua Binding 생성 경로는 있으나 범용 함수 메타데이터 시스템은 아직 제공하지 않습니다.

## 참고

- 전체 협업 이력과 팀 단위 변경사항은 [keonwookang0914/Jungle_Week11_Team4](https://github.com/keonwookang0914/Jungle_Week11_Team4)에서 확인할 수 있습니다.
- 원본 저장소는 Week 10 프로젝트에서 Fork되었으므로, 2026.05.14 이전 이력은 Week 11 개인 기여 집계에서 제외했습니다.
- 핵심 개발 기간에 Rocketstein 작성자 정보로 확인되는 29개 커밋에는 Merge·Revert가 포함되어 있어, 위 목록은 커밋 메시지와 실제 변경 파일 및 최종 코드를 함께 확인한 대표 커밋만 제시합니다.
- 최종 코드와 설계 문서를 대조해 구현 완료 범위와 후속 계획을 구분했습니다.
