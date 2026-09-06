# Renderer Simplification Phases

| 구분 | 내용 |
|---|---|
| 최초 작성자 | OpenAI Codex |
| 최초 작성일 | 2026-05-13 |
| 최근 수정자 | OpenAI Codex |
| 최근 수정일 | 2026-05-13 |
| 버전 | 1.0 |

## 1. 개요

이 문서는 현재 렌더러를 신규 작업자가 읽기 쉬운 구조로 단순화하기 위해 필요한 큰 작업 단위를 `Phase` 기준으로 정리한다.

이번 정리의 목표는 다음과 같다.

- 에디터와 런타임에서 `Deferred / Forward` 토글을 제거한다.
- 기본 렌더 경로를 `Forward` 하나로 고정한다.
- pass 조립형 파이프라인 구조 자체는 유지한다.
- 추후 deferred 혹은 hybrid pipeline을 재구성할 가능성은 남기되, 기본 코드 진입점에서는 혼동 요소를 제거한다.
- 과제성 기능으로 추가된 저우선순위 shading path / permutation을 정리한다.
- `Gouraud` shader 및 관련 view mode / permutation을 제거한다.

이 문서는 "어떤 파일을 수정할지"보다 "어떤 덩어리의 작업을 어떤 순서로 끝낼지"를 정의하는 계획 문서다.

## 2. 범위와 전제

### 2.1 포함 범위

- `RenderShadingPath` 제거
- `DeferredPipeline` 계열 비활성화 혹은 분리
- `Forward` 단일 기본 경로화
- `Gouraud` shader 제거
- view mode / pass registry / shader warm-up 정리
- deferred light culling 자산의 forward 이식 준비

### 2.2 제외 범위

- 새로운 PBR 파이프라인 도입
- cluster 기반의 완전한 신규 Forward+ 설계
- material system 전면 개편
- decal / fog / post process의 기능 확장

### 2.3 핵심 전제

- 기본 lit shading은 `Lambert`, `BlinnPhong` 중심으로 유지한다.
- `Gouraud`는 유지 가치보다 유지 비용이 더 크다고 판단한다.
- deferred 관련 코드는 "기본 실행 경로에서 제거"와 "완전 삭제"를 구분해서 다룬다.

## 3. Phase 요약

| Phase | 이름 | 목적 | 예상 규모 |
|---|---|---|---|
| Phase 1 | Baseline Prune | 과제성 기능과 `Gouraud` 제거, 유지 대상 축소 | 중 |
| Phase 2 | Single Path Lock | `RenderPath` 토글 제거, forward 단일 경로 고정 | 중 |
| Phase 3 | Pipeline Simplification | pipeline / pass / registry에서 deferred 기본 분기 제거 | 중~대 |
| Phase 4 | Deferred Isolation | deferred 자산 격리 혹은 문서화 후 제거 | 중 |
| Phase 5 | Forward Light Culling Migration | deferred light culling 자산을 forward lit 경로로 이식 | 중~대 |
| Phase 6 | Documentation and Hardening | 문서, 검증, 신규 작업자 진입점 정리 | 소~중 |

## 4. 상세 Phase

### Phase 1. Baseline Prune

#### 목표

본격적인 경로 단순화 전에 유지 대상이 아닌 shading / permutation / view mode를 먼저 제거한다.

#### 주요 작업

- `Gouraud` 관련 shader entry 제거
- `Lit_Gouraud` view mode 제거
- `Gouraud` 전용 pass desc 제거
- `Gouraud` warm-up 경로 제거
- `Gouraud`를 가정한 문서, enum, UI 표시 정리
- 과제성 기능 중 유지 우선순위가 낮은 shading permutation 정리

#### 포함 대상 예시

- `EViewMode::Lit_Gouraud`
- `BuildViewModeDeferredOpaquePassDesc` / `BuildViewModeForwardOpaquePassDesc` 내 Gouraud 분기
- `OpaquePass.hlsl` / deferred shader의 Gouraud entry
- shader warm-up 목록

#### 기대 효과

- 이후 phase에서 고려해야 할 permutation 수 감소
- forward light culling 이식 시 VS Gouraud 특수 케이스 제거
- 신규 작업자가 읽어야 할 lit 경로 단순화

#### 선행 조건

- 유지할 view mode / shading model 목록 확정

#### 종료 조건

- 빌드 상에서 `Gouraud` 관련 enum, shader entry, pass desc, warm-up 참조가 제거된다.
- 에디터에서 `Gouraud`를 선택할 수 없다.

### Phase 2. Single Path Lock

#### 목표

사용자 노출 설정과 runtime 분기에서 `Deferred / Forward` 토글을 제거하고, 기본 렌더 경로를 `Forward`로 고정한다.

#### 주요 작업

- `EditorSettings`의 `RenderShadingPath` 제거
- 에디터 메뉴의 `Deferred Shading` / `Forward Shading` 선택 UI 제거
- `SceneView.RenderPath` 기본값 및 전달 경로 단순화
- render path warm-up API 단순화
- `RenderPath` 기반 실행 분기 제거 혹은 상수화

#### 기대 효과

- 렌더 경로 선택의 진입점이 하나로 줄어듦
- 신규 작업자가 "현재 어떤 path가 활성화되어 있는지" 추적할 필요가 줄어듦
- phase 3 이후 registry 정리를 안전하게 진행할 수 있음

#### 선행 조건

- Phase 1 완료

#### 종료 조건

- 에디터 설정과 메뉴에서 shading path 토글이 사라진다.
- 일반 scene rendering은 항상 forward 기준으로 실행된다.

### Phase 3. Pipeline Simplification

#### 목표

기본 실행 구조에서 deferred/forward 이중 트리를 제거하고, pipeline / runner / pass registry를 forward 중심 구조로 재편한다.

#### 주요 작업

- `ScenePipeline -> DeferredPipeline / ForwardPipeline` 이중 구조 해체
- `ShouldExecutePipeline` 의 deferred/forward 분기 축소
- `MapPassToNodeType` 의 path 기반 분기 축소
- `ViewModePassRegistry` 에서 path 축을 줄이거나 forward 중심으로 재정렬
- `DeferredLighting` fullscreen draw command 빌드 경로를 기본 흐름에서 제거

#### 기대 효과

- pipeline graph 단순화
- draw command build와 pass lookup의 조건문 감소
- 렌더 경로를 읽는 사람이 "기본 경로"를 한 번에 파악 가능

#### 선행 조건

- Phase 2 완료

#### 종료 조건

- 기본 렌더 graph가 forward 단일 경로 기준으로 표현된다.
- deferred pipeline은 기본 실행 트리에서 제거되거나 별도 실험 경로로 분리된다.

### Phase 4. Deferred Isolation

#### 목표

deferred rendering 관련 자산을 기본 코드 경로에서 격리한다. 유지할지 삭제할지의 판단도 이 phase에서 마무리한다.

#### 주요 작업

- `DeferredOpaquePass`, `DeferredDecalPass`, `DeferredLightingPass`의 처리 방침 확정
- `FViewModeSurfaces`의 필요 범위 재검토
- deferred shader / docs / pass registration 분리
- 실험용 혹은 보관용 디렉터리/registry로 격리하거나 완전 제거
- 완전 제거 시 재도입 가이드를 문서로 남김

#### 선택지

- 격리안
  - 빌드 가능 상태로 남기되 기본 registry와 기본 문서 흐름에서 제외
- 제거안
  - 코드에서는 제거하고, 설계/복구 가이드를 문서로 남김

#### 기대 효과

- 신규 작업자가 기본 렌더러를 읽을 때 deferred intermediate surface 계약까지 동시에 이해할 필요가 없어짐
- 유지 비용과 혼동 비용을 분리해서 관리 가능

#### 선행 조건

- Phase 3 완료

#### 종료 조건

- deferred 관련 코드가 기본 진입점에서 보이지 않거나, 완전히 제거되어 문서로만 남는다.

### Phase 5. Forward Light Culling Migration

#### 목표

기존 deferred 쪽에서 사용하던 tile-based light culling 자산을 forward lit 경로에서 재사용 가능하도록 이식한다.

#### 주요 작업

- `LightCullingPass`를 forward lit 경로에 배치
- forward lit path에서 depth copy, tile mask, debug hit map, local light SRV 바인딩
- forward lighting 함수가 전수 순회 대신 tile mask를 사용하도록 수정
- constant buffer 슬롯 계약 재정리
- `Lambert`, `BlinnPhong` 기준으로 우선 적용

#### 유의사항

- 현재 light culling params와 static mesh material이 모두 `b2`를 사용하므로 슬롯 재설계가 필요할 수 있다.
- Gouraud 제거 이후에 진행하는 것이 합리적이다.
- 이 phase는 "forward 고정"과 분리해서 수행해야 리스크가 관리된다.

#### 기대 효과

- local light 수 증가 시 forward lit 비용 완화
- deferred 전용으로 남아 있던 compute 자산의 재활용

#### 선행 조건

- Phase 1 완료
- Phase 3 완료
- deferred 완전 삭제보다 먼저 끝낼 수도 있으나, 기본 경로는 이미 forward로 고정되어 있어야 한다.

#### 종료 조건

- forward lit path가 tile-based light culling 결과를 소비한다.
- `Lambert`, `BlinnPhong` view mode에서 기존 대비 과도한 기능 회귀가 없다.

### Phase 6. Documentation and Hardening

#### 목표

정리된 구조를 기준으로 문서와 검증 체계를 맞추고, 신규 작업자가 진입할 때 필요한 안내를 갱신한다.

#### 주요 작업

- pipeline / pass / execution 문서 갱신
- 제거된 기능 목록과 의사결정 근거 기록
- deferred 복구 가이드 혹은 archived note 작성
- 신규 작업자용 진입 문서 정리
- smoke test / visual regression 체크리스트 정리

#### 기대 효과

- 구조 개선의 의도가 문서에 남음
- 추후 다시 deferred 또는 hybrid를 시도할 때 기준점 확보

#### 선행 조건

- 앞선 phase들의 구현 완료

#### 종료 조건

- 문서와 실제 코드 구조가 일치한다.
- 신규 작업자가 기본 렌더 경로를 빠르게 이해할 수 있는 상태가 된다.

## 5. 권장 수행 순서

권장 순서는 아래와 같다.

1. Phase 1 `Baseline Prune`
2. Phase 2 `Single Path Lock`
3. Phase 3 `Pipeline Simplification`
4. Phase 4 `Deferred Isolation`
5. Phase 5 `Forward Light Culling Migration`
6. Phase 6 `Documentation and Hardening`

이 순서를 권장하는 이유는 다음과 같다.

- 먼저 permutation 수를 줄여야 이후 구조 단순화의 난이도가 내려간다.
- 사용자 토글을 먼저 제거해야 "기본 경로"를 정의할 수 있다.
- 기본 경로가 정리된 뒤에 deferred를 격리해야 삭제/보존 판단이 쉬워진다.
- light culling 이식은 구조 안정화 이후에 해야 리스크가 작다.

## 6. 리스크 정리

| 항목 | 설명 |
|---|---|
| view mode 회귀 | `WorldNormal`, `SceneDepth`, `Unlit`가 deferred intermediate surface에 간접 의존할 수 있다 |
| shader binding 충돌 | forward light culling 이식 시 `b2` 슬롯 충돌 가능성이 있다 |
| 문서-코드 불일치 | 구조를 바꾼 뒤 기존 deferred/forward 문서가 빠르게 낡을 수 있다 |
| 실험 자산 잔존 | deferred 코드를 "남겨두기만" 하면 오히려 신규 작업자 혼동이 커질 수 있다 |
| decal 처리 차이 | deferred decal pass 제거 후 forward decal 처리와 기능 차이를 재검증해야 한다 |

## 7. 의사결정 포인트

구현 전 아래 결정은 확정하는 편이 좋다.

1. `Gouraud` 제거를 최종 결정할 것인지
2. deferred 코드를 격리할지, 완전 삭제할지
3. `WorldNormal`, `SceneDepth`를 반드시 현행 품질로 유지할지
4. forward light culling 이식을 이번 정리의 범위에 넣을지, 후속 phase로 뺄지

## 8. 최종 목표 상태

모든 phase가 끝난 후 기대하는 최종 상태는 아래와 같다.

- 신규 작업자는 기본적으로 forward 단일 경로만 읽으면 된다.
- pipeline tree는 기본 실행 관점에서 단순하다.
- `Gouraud` 및 과제성 저우선순위 permutation은 제거된다.
- deferred 관련 자산은 실험/보관 영역으로 분리되거나 문서만 남는다.
- local light culling은 필요 시 forward lit 경로에서 사용된다.
