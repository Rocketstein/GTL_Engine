# Enemy Health Bar World-Space HUD Plan

## Context

잡몹 체력바는 보스/플레이어 HUD처럼 화면 고정 UI가 아니라, 적의 월드 위치를 매 프레임 화면 좌표로 변환해서 따라다녀야 한다.

현재 RmlUi HUD는 `HUD_Manager.lua`에서 `UUserWidget:SetProperty()`로 `display`, `width` 같은 속성을 갱신하는 구조다. 따라서 enemy health bar도 RML 문서를 여러 개 생성하지 않고, `HUD.rml` 안에 고정 개수의 슬롯을 만들어 재사용하는 방식이 가장 안정적이다.

## Goals

- 잡몹 머리 위에 작은 체력바를 표시한다.
- 적 위치가 움직이면 체력바도 화면 좌표를 따라 움직인다.
- 체력 감소는 mask width 비율로 표현한다.
- 체력바 바로 아래에 PostureBar를 함께 표시한다.
- PostureBar는 체력바와 같은 width를 쓰고, height는 체력바의 약 0.7배로 둔다.
- 화면 밖, 카메라 뒤, 사망, 너무 먼 적은 숨긴다.
- RML widget 생성/삭제를 매 프레임 하지 않는다.
- 추후 lock-on, 최근 피격 적, 가까운 적 우선 표시 같은 정책을 붙일 수 있게 한다.

## No Goals

- 이번 계획에서 실제 구현을 완료하지 않는다.
- 잡몹 체력바용 신규 에디터 기능은 만들지 않는다.
- 체력바 occlusion을 완벽한 depth test 기반으로 처리하지 않는다. 필요하면 후속 phase에서 ray/depth 기반으로 추가한다.
- 모든 적을 무제한 표시하지 않는다. 슬롯 풀 방식으로 제한한다.

## Recommended Architecture

```text
Enemy Actor / HealthComponent
    -> choose visible enemies
    -> head world position
    -> C++ ProjectWorldToScreen
    -> Lua HUD.SetEnemyHealthBar(index, x, y, hpRatio, postureRatio, visible)
    -> RML enemy-bar-N left/top/display + enemy-hp-mask-N/enemy-posture-mask-N width
```

핵심 책임 분리는 다음과 같다.

- C++: 적 목록 수집, 카메라 투영, 화면 표시 여부 판단
- Lua: HUD API 노출, RML element property 갱신
- RML/RCSS: enemy health/posture bar 슬롯과 시각 스타일 정의

## Phase 1: Static Enemy Bar Slot Prototype

### Goal

월드 위치 연동 전에 RML/RCSS만으로 enemy health bar가 정상 렌더되는지 확인한다.

### Tasks

1. `KraftonEngine/Content/Game/UI/HUD.rml`에 `enemy-bars` 컨테이너를 추가한다.
2. `enemy-bar-0`부터 최소 8개, 권장 16개 슬롯을 미리 만든다.
3. 각 슬롯 내부에 health background/hp mask/fill과 posture background/posture mask/fill을 둔다.
4. PostureBar는 health bar 바로 아래에 배치하고, width는 동일하게, height는 health bar의 약 0.7배로 잡는다.
5. `KraftonEngine/Content/Game/UI/HUD.rcss`에 작은 enemy health/posture bar 스타일을 추가한다.
6. 기본값은 `display: none`으로 둔다.
7. 임시로 `enemy-bar-0` 하나만 보이게 해서 위치와 크기를 확인한다.
   - Phase 2에서 Lua API 제어로 넘어가면 이 임시 표시 규칙은 제거한다.

### Suggested RML Shape

```html
<div id="enemy-bars">
    <div id="enemy-bar-0" class="enemy-healthbar">
        <div class="enemy-health-bg">
            <div id="enemy-hp-mask-0" class="enemy-hp-mask">
                <div class="enemy-hp-fill"></div>
            </div>
        </div>
        <div class="enemy-posture-bg">
            <div id="enemy-posture-mask-0" class="enemy-posture-mask">
                <div class="enemy-posture-fill"></div>
            </div>
        </div>
    </div>
</div>
```

### Validation

- HUD 로드 시 RmlUi warning이 없어야 한다.
- `enemy-bar-0`이 지정한 위치에 보인다.
- `enemy-hp-mask-0` width를 줄이면 hp fill이 줄어든다.
- `enemy-posture-mask-0` width를 줄이면 posture fill이 줄어든다.
- PostureBar는 체력바 아래에 같은 width로 붙고, height는 체력바보다 낮게 보인다.

## Phase 2: HUD Manager Enemy Bar API

### Goal

Lua에서 enemy health bar 슬롯의 위치, 체력 비율, posture 비율, 표시 여부를 제어할 수 있게 한다.

### Tasks

1. `HUD_Manager.lua`에 enemy bar 최대 개수 상수를 추가한다.
2. `SetEnemyHealthBar(index, x, y, hpRatio, postureRatio, visible)` 함수를 추가한다.
3. `SetEnemyHealthBarVisible(index, visible)` 보조 함수가 필요하면 추가한다.
4. `_G.HUD.SetEnemyHealthBar`로 외부 노출한다.
5. `EndPlay()`에서 enemy bar 캐시 상태를 초기화한다.
6. `x`, `y`는 화면 픽셀 좌표 기준으로 처리한다.
7. `hpRatio`, `postureRatio`는 각각 0.0~1.0으로 clamp해서 mask width에 반영한다.
8. 최종 `left/top`은 health/posture bar 묶음의 중심 정렬을 위해 half width/height를 보정한다.

### Suggested Lua API

```lua
HUD.SetEnemyHealthBar(0, 640.0, 320.0, 0.75, 0.40, true)
HUD.SetEnemyHealthBar(1, 0.0, 0.0, 0.0, 0.0, false)
```

### Validation

- Lua에서 index 0 체력바를 임의 위치에 표시할 수 있다.
- hpRatio `1.0`, `0.5`, `0.0`이 정상적으로 hp mask width에 반영된다.
- postureRatio `1.0`, `0.5`, `0.0`이 정상적으로 posture mask width에 반영된다.
- `visible=false` 호출 시 해당 슬롯이 숨겨진다.

## Phase 3: World To Screen Projection API

### Goal

게임 코드나 Lua에서 적의 월드 위치를 화면 좌표로 변환할 수 있는 공용 경로를 만든다.

### Tasks

1. 에디터 쪽 `ProjectWorldToViewport()` 구현을 참고한다.
   - `KraftonEngine/Source/Editor/UI/Asset/Mesh/MeshEditorWidget.cpp`
2. 게임 런타임에서 사용할 수 있는 `ProjectWorldToScreen` 함수를 C++에 추가한다.
3. 현재 게임 카메라의 `FMinimalViewInfo`와 viewport size를 사용한다.
4. clip W가 0에 가깝거나 카메라 뒤면 false를 반환한다.
5. NDC가 화면 범위를 벗어나면 false를 반환한다.
6. Lua에서 필요하면 `Engine.ProjectWorldToScreen(Vector)` 형태로 binding한다.

### Projection Rule

```text
Clip = WorldPosition * ViewProjection
NDC = Clip.xy / Clip.w
ScreenX = (NDC.x * 0.5 + 0.5) * ViewportWidth
ScreenY = (0.5 - NDC.y * 0.5) * ViewportHeight
```

### Validation

- 카메라 정면의 actor 위치가 화면 중앙 근처로 투영된다.
- 카메라 뒤의 위치는 false가 된다.
- 화면 밖 위치는 false가 된다.

## Phase 4: Enemy Source And Update Policy

### Goal

어떤 적에게 체력바를 표시할지 결정하고, 표시할 적만 HUD slot에 매핑한다.

### Tasks

1. 적 후보는 `Enemy` tag가 붙은 actor 기준으로 수집한다.
   - `HealthComponent`가 없거나 dead 상태인 actor는 제외한다.
   - `EncounterComponent`가 있는 boss actor는 boss HUD와 중복되지 않도록 제외한다.
2. 사망한 적은 제외한다.
3. 보스는 기존 boss HUD가 있으므로 기본적으로 제외한다.
4. 최대 표시 개수는 enemy bar slot 개수로 제한한다.
5. actor 고유 id를 기준으로 `enemyId -> slotIndex` 매핑 테이블을 유지한다.
   - 이미 슬롯을 받은 적은 표시 대상인 동안 같은 슬롯을 계속 쓴다.
   - 사망, 거리 초과, 화면 밖 등으로 숨길 때 슬롯을 반환한다.
   - 빈 슬롯은 새로 표시 대상이 된 적에게 배정한다.
6. 우선순위는 다음 중 하나로 시작한다.
   - 최근 피격된 적 우선
   - 플레이어와 가까운 적 우선
   - 화면 중앙에 가까운 적 우선
   - lock-on 대상 우선
7. actor별 health ratio와 posture ratio를 가져와 HUD API에 전달한다.
8. 남는 슬롯은 모두 숨긴다.

### Validation

- 적이 여러 명 있어도 최대 슬롯 수만큼만 표시된다.
- 적이 죽으면 해당 health bar가 숨겨진다.
- 보스는 잡몹 체력바로 중복 표시되지 않는다.

## Phase 5: Head Anchor And Screen Placement

### Goal

체력바가 적 몸통이 아니라 머리 위에 자연스럽게 붙도록 anchor 위치를 정한다.

### Tasks

1. 기본 anchor는 actor location + fixed vertical offset으로 시작한다.
2. 적마다 높이가 다르면 capsule/mesh bounds 기반 offset으로 바꾼다.
3. 화면 좌표로 변환한 뒤 bar half width만큼 `left`를 보정한다.
4. bar가 화면 가장자리에서 잘리는 문제는 clamp 여부를 결정한다.
5. 카메라 거리에 따른 scale은 후속 옵션으로 둔다.

### Suggested Start Rule

```cpp
FVector Anchor = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, EnemyHeight + 20.0f);
```

### Validation

- 이동 중인 적의 머리 위를 자연스럽게 따라간다.
- 가까운 적과 먼 적 모두 체력바가 너무 크게 흔들리지 않는다.

## Phase 6: Visibility And Polish

### Goal

보기에 거슬리지 않도록 표시 조건과 애니메이션을 다듬는다.

### Tasks

1. 너무 먼 적은 숨긴다.
2. 화면 가장자리 밖으로 나가면 숨긴다.
3. 최근 피격 후 N초 동안만 표시하는 옵션을 검토한다.
4. hpRatio/postureRatio가 변경될 때 즉시 반영할지, 보간할지 결정한다.
5. 시야가 막힌 적도 표시 조건을 만족하면 보여준다.
   - 벽 뒤 occlusion은 1차 구현에서 제외한다.
   - 표시 여부는 lock-on, 전투 상태, 피격/자세 변화 같은 gameplay signal로 결정한다.
6. z-order는 player/boss HUD보다 낮게 둘지 정책을 정한다.

### Validation

- 화면에 너무 많은 체력바가 떠서 지저분하지 않아야 한다.
- 적이 화면 밖으로 나갈 때 잔상처럼 남지 않아야 한다.
- hp 변화가 RML mask에 안정적으로 반영되어야 한다.

## Phase 7: Integration And Regression Check

### Goal

기존 HUD와 충돌 없이 enemy health bar를 통합한다.

### Tasks

1. player HUD, boss HUD, item HUD 위치와 겹침을 확인한다.
2. RmlUi scissor/mask 동작이 player/boss HP와 같이 정상인지 확인한다.
3. gamma correction 이후 UI 렌더 순서가 유지되는지 확인한다.
4. 많은 적이 있을 때 property update 비용을 확인한다.
5. 필요하면 update frequency를 낮춘다.
   - 예: 매 프레임 위치, hp는 변경 시만

### Validation

- Debug x64 빌드 성공
- HUD 로드 warning 없음
- player/boss HUD 기존 기능 유지
- 적 10명 이상에서 프레임 드랍이나 UI 깜빡임 없음

## Implementation Notes

- RML document는 하나만 유지한다.
- enemy bar는 slot pool로 재사용한다.
- 생성/삭제보다 `display` 토글이 낫다.
- 투영 계산은 Lua보다 C++에서 처리하는 편이 안전하다.
- Lua는 최종 위치와 비율을 RML property로 전달하는 얇은 계층으로 유지한다.
- `left/top`은 문자열 `"123.0px"` 형식으로 설정한다.
- `width`는 현재 HUD 방식과 맞춰 `"75.0%"` 형식으로 설정한다.
- PostureBar width는 hp bar와 같은 기준을 쓰고, height만 hp bar의 약 0.7배로 둔다.

## Open Decisions

- 최대 enemy health bar 슬롯 개수: 8, 12, 16 중 결정
- 보스도 enemy bar 대상에 포함할지 여부
- lock-on 대상만 표시할지, 피격된 적도 표시할지 여부
- 거리별 scale 적용 여부
- occlusion 처리를 1차 구현에 넣을지 여부
- PostureBar 색상과 감소 애니메이션을 hp bar와 다르게 둘지 여부

## Implementation Status

- Phase 1~7의 1차 구현은 완료한다.
- enemy health/posture bar 슬롯 개수는 16개로 고정한다.
- 표시 후보는 `Enemy` tag가 붙은 actor이며, `HealthComponent`가 없거나 dead 상태면 제외한다.
- `EncounterComponent`가 있는 boss actor는 기존 boss HUD와 중복되지 않도록 제외한다.
- 기본 상태에서는 enemy bar를 표시하지 않는다.
- actor 단위 visibility API를 통해 락온/전투/피격 처리 코드가 표시 여부를 명시적으로 열 수 있다.
  - `HUD.SetEnemyHealthBarActorVisible(enemy, true)`는 해당 enemy를 계속 표시한다.
  - `HUD.SetEnemyHealthBarActorVisible(enemy, true, seconds)`는 해당 enemy를 일정 시간만 표시한다.
  - `HUD.SetEnemyHealthBarActorVisible(enemy, false)`는 해당 enemy의 수동 표시를 해제하고 현재 슬롯을 숨긴다.
  - `HUD.SetEnemyHealthBarLockOnTarget(enemy)` / `HUD.ClearEnemyHealthBarLockOnTarget()`은 lock-on 대상 교체용 helper다.
- 적의 `EnemyAIBrainComponent:HasValidTarget()`이 true이면 전투 상태로 보고 표시한다.
- hp ratio 또는 posture ratio가 변하면 `3.0s` 동안 일시 표시한다.
- actor 고유 id 기준으로 `enemyId -> slotIndex` 매핑을 유지해서 같은 적은 가능한 한 같은 슬롯을 계속 사용한다.
- 우선순위는 기존 슬롯 보유 적을 먼저 유지하고, 신규 대상은 화면 중앙에 가까운 순으로 배정한다.
- head anchor는 capsule half height + padding을 우선 사용하고, capsule을 얻지 못하면 fixed vertical offset을 사용한다.
- 화면 밖이거나 bar 전체가 안전 영역 밖으로 걸치는 대상은 숨긴다.
- 카메라와의 거리가 `40.0`을 넘는 대상은 숨긴다.
- 벽 뒤 occlusion은 적용하지 않는다. 조건을 만족한 적은 벽 뒤에 있어도 bar를 표시한다.
- 후보 수집과 투영 갱신은 `1/60s` 간격으로 수행한다.
- 거리별 scale, 전용 depth 기반 occlusion, 감소 애니메이션은 후속 polish 항목으로 남긴다.

