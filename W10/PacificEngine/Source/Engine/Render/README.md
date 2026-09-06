# Render

> 문서 버전: 1.1  
> 최종 수정일: 2026-05-14

## 세부 문서 바로가기

- [렌더러 개요: Overview](./Docs/render_overview.md)
- [실행 구조: Execution](./Docs/render_execution.md)
- [리소스: Resources](./Docs/render_resource.md)
- [파이프라인 참조: Pipeline Reference](./Docs/render_pipeline_reference.md)
- [패스 참조: Pass Reference](./Docs/render_pass_reference.md)
- [이름 규약: Naming Convention](./Docs/render_conventions_naming.md)
- [바인딩 규약: Binding Convention](./Docs/render_conventions_binding.md)

## 설계 요약

> 조립형 렌더 파이프라인: 원하는 렌더 실행단위를 자유롭게 조합해 파이프라인을 만들자.

## 모듈 구성

렌더 모듈은 다음 계층으로 구성된다.

- `Scene`
  - SceneProxy와 장면 렌더 데이터 관리
- `Submission`
  - Scene / Overlay 데이터를 수집하고 draw command로 변환
- `Execute`
  - pass 실행, pipeline tree 순회, registry 기반 실행 선택, 실행 context 관리
- `Resources`
  - 프레임 공용 버퍼, 렌더 상태, 셰이더, 바인딩 슬롯 등 렌더 공용 자원 관리
- `Visibility`
  - frustum, occlusion, LOD, light culling 등 가시성 판단과 화면 제출 최적화 관리

렌더 실행은 단일 함수에 하드코딩하지 않고,  
**Registry에 등록된 최상위(Root) 실행 단위**를 `FRenderPipelineRunner`가 순회하는 방식으로 구성한다.

가장 빠른 진입점은 [렌더러 개요](./Docs/render_overview.md)이며, 그 다음으로 [실행 구조](./Docs/render_execution.md)와 [파이프라인 참조](./Docs/render_pipeline_reference.md)를 읽으면 현재 트리와 선택 규칙을 바로 파악할 수 있다.

## 핵심 용어

| 용어 | 의미 |
|---|---|
| Pass | 실제 GPU 작업을 수행하는 최소 실행 단위 |
| Pipeline | Pass 또는 다른 Pipeline을 묶는 실행 조합 단위 |
| Registry | Pass, Pipeline, ViewMode별 실행 규칙을 등록 · 조회하는 계층 |
| Runner | 등록된 실행 트리를 실제로 순회하며 실행하는 계층 |
| Context | 실행 중 공유되는 상태와 참조를 담는 구조 |
| Draw Command | 실제 draw 제출 단위 |
