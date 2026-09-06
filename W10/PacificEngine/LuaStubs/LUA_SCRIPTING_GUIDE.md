# CrashEngine Lua Scripting Guide

이 가이드는 CrashEngine의 Lua 스크립팅 시스템에 대한 기술 명세 및 활용 방법을 담고 있습니다.

## 1. 스크립트 기본 구조

모든 Lua 스크립트는 `Script` 테이블을 정의하고 반환해야 합니다.

```lua
local Vec = require("Core.Vector") -- 벡터 연산 모듈

local Script = {
    properties = {
        MyFloat = { type = "float", default = 10.0, min = 0.0, max = 100.0, speed = 0.1 },
    }
}

function Script:BeginPlay()
    local actor = self:GetActor()
end

function Script:Tick(deltaTime)
    local actor = self:GetActor()
    local loc = actor:GetLocation()
end

return Script
```

## 2. 엔진 API (주요 클래스)

### Actor (객체)
- `GetLocation()`, `SetLocation(vec3)`: 월드 좌표 기준
- `GetRotation()`, `SetRotation(vec3)`: x=Roll, y=Pitch, z=Yaw (도 단위)
- `GetScale()`, `SetScale(vec3)`: 스케일 조절
- `GetForward()`: 전방 벡터 반환
- `GetTag()`, `SetTag(string)`: 태그 시스템
- `GetComponent(className, index)`: 특정 타입의 컴포넌트 찾기 (index는 1부터 시작)
    - 예: `targetActor:GetComponent("UScriptComponent")` -- 다른 액터의 스크립트 핸들 가져오기
- `GetComponents(className)`: 특정 타입의 모든 컴포넌트를 테이블로 반환
- `GetName()`, `GetClassName()`: 이름 및 클래스 이름 조회
- `IsVisible()`, `SetVisible(bool)`: 렌더링 활성화 여부
- `IsValid()`: 유효성 검사

### ScriptComponent (현재 스크립트)
- `GetActor()`: 소유 액터(`Actor` 핸들) 반환. **가장 먼저 호출하여 사용하세요.**
- `QueryActorByTagClosest(tag, pos, radius)`: 반경 내 특정 태그 액터 검색
- `StartCoroutine(func)`: 코루틴 시작
- `StopCoroutine(id)`: 코루틴 중지

### Component (컴포넌트)
- `GetWorldLocation()`: 컴포넌트의 월드 좌표
- `SetWorldLocation(vec3)`
- `GetRelativeLocation()`
- `GetForwardVector()`

### Input (입력)
- `Input.GetAxis("Horizontal" | "Vertical")`
- `Input.GetKeyDown(keyCode)`

## 3. 수학 및 벡터 연산

프로젝트의 표준 방식인 `Core.Vector` 모듈 사용을 강력히 권장합니다.
```lua
local Vec = require("Core.Vector")
local nextPos = Vec.Add(currentPos, Vec.Mul(velocity, deltaTime))
```

## 4. C++ API 추가 방법 (Binding)

1.  **Actor 관련**: `Source/Engine/Scripting/Binding/LuaActorBinding.cpp`
2.  **Component 관련**: `Source/Engine/Scripting/Binding/LuaComponentBinding.cpp`
3.  **Math/Common**: `Source/Engine/Scripting/Binding/LuaCommonBinding.cpp`
## 5. GameManager (Global Singleton)

`GameManager`는 게임의 전역 상태(시간, 킬 카운트, 플레이어 스탯)를 관리하는 싱글톤 모듈입니다.

### 주요 기능
- **상태 조회**: `KillCount`, `TimeRemaining`, `CurrentPlayerHP`, `IsGameOver` 등
- **이벤트 전송**:
  - `GameManager.AddKill()`: 적 처치 시 호출
  - `GameManager.PlayerGetDamage(damage)`: 플레이어에게 데미지를 줄 때 호출 (비효율적인 쿼리 방지)
  - `GameManager.AddExp(amount)`: 경험치 획득 시 호출
- **게임 제어**: `SetPaused(bool)`, `RestartGame()`

### 사용 예시 (Enemy 스크립트에서 데미지 주기)
```lua
local GameManager = require("GameManager")

function Script:OnOverlap(other)
    if other:GetTag() == "Player" then
        GameManager.PlayerGetDamage(10.0) -- 플레이어에게 10 데미지
    end
end
```

