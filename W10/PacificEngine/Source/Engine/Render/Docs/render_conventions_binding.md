# Render Conventions: Binding

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-04-24 |
| 최근 수정자 | 김태현 |
| 최근 수정일 | 2026-05-14 |
| 버전 | 1.3 |

## 1. Slot 분류

| 구분 | 표기 | 의미 |
|---|---|---|
| Constant Buffer | `b#` | 상수 버퍼 슬롯 |
| Shader Resource View | `t#` | 읽기 전용 리소스 슬롯 |
| Sampler | `s#` | sampler 슬롯 |
| Unordered Access View | `u#` | 읽기/쓰기 가능한 리소스 슬롯 |

## 2. Constant Buffer Slot 규약

C++ 기준 슬롯 정의는 `Render/Resources/Bindings/RenderBindingSlots.h`에 둔다.

| Slot | 이름 | 의미 |
|---|---|---|
| `b0` | `ECBSlot::Frame` | frame/view 공용 상수 |
| `b1` | `ECBSlot::PerObject` | object 단위 상수 |
| `b2` | `ECBSlot::PerShader0` | pass/material/shader 추가 상수 |
| `b3` | `ECBSlot::PerShader1` | pass/material/shader 추가 상수 |
| `b4` | `ECBSlot::Light` | 글로벌 조명 상수 |
| `b5` | `ECBSlot::ShadowPass` / `PerShader2` | shadow pass 또는 추가 pass 입력 상수 |

- `b0`: view, projection, camera, time 등
- `b1`: transform, normal matrix, color 등
- `b2`, `b3`: pass-local 또는 material-local 추가 파라미터
- `b4`: 글로벌 조명 데이터
- `b5`: shadow map 생성 전용 또는 pass-local 추가 상수

## 3. System SRV Slot 규약

| Slot | 이름 | 의미 |
|---|---|---|
| `t6` | `ESystemTexSlot::LocalLights` | local light buffer SRV |
| `t7` | `ESystemTexSlot::LightTileMask` | per-tile light culling mask |
| `t8` | `ESystemTexSlot::DebugHitMap` | light culling debug hit map |
| `t10` | `ESystemTexSlot::SceneDepth` | depth copy SRV |
| `t11` | `ESystemTexSlot::SceneColor` | scene color copy SRV |
| `t13` | `ESystemTexSlot::Stencil` | stencil copy SRV |
| `t20~t23` | `ESystemTexSlot::ShadowAtlasBase` | shadow atlas pages |
| `t48~t51` | `ESystemTexSlot::ShadowMomentAtlasBase` | shadow moment atlas pages |

## 4. 일반 SRV 사용 규약

| Slot | 의미 |
|---|---|
| `t0` | primary texture 또는 fullscreen input |
| `t1` | normal texture 또는 auxiliary input |
| `t2` | specular / material texture |
| `t3` | modified base color 등 추가 input |
| `t4` | modified surface1 |
| `t5` | modified surface2 |

추가 규칙:

- material/custom shader의 texture SRV는 reflection으로 추출한 shader resource slot을 그대로 따른다.
- `t0~t5`는 pass-fixed 의미가 아니라 material/custom shader가 reflected binding으로 사용할 수 있는 일반 영역으로 본다.
- shadow atlas는 현재 `t20~t23`과 `t48~t51`의 4-page array로 관리한다.
- alias 밖의 resource name은 동일한 material texture parameter 이름이 필요하다.

## 5. Sampler Slot 규약

| Slot | 이름 |
|---|---|
| `s0` | `ESamplerSlot::LinearClamp` |
| `s1` | `ESamplerSlot::LinearWrap` |
| `s2` | `ESamplerSlot::PointClamp` |
| `s3` | `ShadowSampler` |

## 6. Binding Scope

### 6.1 Frame Scope
- frame CB
- global light CB
- 공용 sampler

### 6.2 Pass Scope
- fog pass 전용 CB
- outline pass 전용 SRV
- fullscreen pass 입력

### 6.3 Draw Scope
- per-object CB
- material texture
- material parameter CB

## 7. Binding 분리 기준

| 구분 | 설명 |
|---|---|
| Global binding | 여러 pass가 공통으로 참조 |
| Pass-local binding | 특정 pass 안에서만 유효 |
| Material binding | 머티리얼 단위로 변경 |
| Per-object binding | 오브젝트마다 변경 |

## 8. Source of Truth

| 파일 | 역할 |
|---|---|
| `Render/Resources/Bindings/RenderBindingSlots.h` | C++ 측 슬롯 상수 정의 |
| `Render/Resources/FrameResources.h` | frame 공용 CB, sampler, light buffer 정의 |
| `Render/Resources/Buffers/ConstantBufferData.h` | CB payload 구조 정의 |
| `Submission/Command/BuildDrawCommand.cpp` | draw command 수준 실제 바인딩 구성 |
| `Scene/Proxies/Primitive/MeshSceneProxy.cpp` | reflected texture binding과 material texture 매칭 구성 |
| 각 pass 구현 파일 | pass-local SRV/CB 바인딩 및 unbind 처리 |

## 9. Pass 추가 시 체크 항목

1. 어떤 CB를 `b0~b5` 중 어디에 둘지 결정한다.
2. 어떤 SRV를 system slot에 둘지, local slot에 둘지 결정한다.
3. sampler를 새로 둘 필요가 있는지 확인한다.
4. pass 종료 후 unbind가 필요한지 확인한다.
5. 후속 pass 입력과 충돌하지 않는지 확인한다.
6. `RenderPassPreset`과 실제 셰이더 바인딩이 일치하는지 확인한다.
