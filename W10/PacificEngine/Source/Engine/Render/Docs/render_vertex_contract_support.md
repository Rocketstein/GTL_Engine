# Render Vertex Contract Support

| 구분 | 내용 |
|---|---|
| 목적 | 현재 엔진이 지원하는 vertex semantic / compatibility / reflected binding 경계를 기록 |
| 상태 | Runtime vertex contract 일반화 반영 기준 |
| 범위 | static mesh, skeletal mesh, custom shader 일반 경로 |

## 1. 목표 보장

현재 일반 경로의 목표는 아래와 같다.

- 지원 semantic / format 규칙 안에 들어오는 메시-셰이더 조합은 runtime packer가 shader input contract 기준으로 vertex blob을 다시 조립한다.
- 이 범위 안에서는 값이 기대와 다를 수 있어도 vertex stride / element offset mismatch 때문에 다음 semantic이 밀리는 상태는 만들지 않는다.
- 지원 경계 밖 semantic 또는 format/component 조합은 warning log 대상이며 동작을 보장하지 않는다.

## 2. 지원 Semantic

일반 경로가 현재 canonical semantic으로 지원하는 항목은 아래다.

- `POSITION0`
- `NORMAL0`
- `COLOR0`
- `TANGENT0`
- `TEXCOORD0`
- `TEXCOORD1`

skeletal asset source model 전용 추가 항목:

- `BLENDINDICES0`
- `BLENDWEIGHT0`

메모:

- `BLENDINDICES0`, `BLENDWEIGHT0`는 skeletal asset source에서만 의미가 있다.
- 현재 일반 static/custom render path의 canonical compatibility contract에는 직접 노출하지 않는다.

## 3. 지원 Scalar / Component 규칙

지원 scalar type:

- 일반 semantic source: `float32`
- `BLENDINDICES0`: `uint16`
- reflected shader input 해석:
  - `float32`
  - `uint32`
  - `sint32`

지원 component 정책:

- exact match 허용
- widening 허용:
  - `float2 -> float3`
  - `float2 -> float4`
  - `float3 -> float4`
- widening 시 앞 component를 복사하고 남는 component는 `0`으로 채운다.

비지원:

- 위 규칙 밖의 scalar type / component 조합
- arbitrary custom semantic

## 4. Default-Fill / Strict Requirement

default-fill 허용:

- `COLOR0`
- `TEXCOORD1`

default-fill 값:

- `COLOR0`: `1,1,1,1`
- `TEXCOORD1`: `0,0` 기반, 요청 component 수 범위 안에서 zero-fill

strict-required:

- `NORMAL0`
- `TANGENT0`

즉 shader가 `NORMAL0`, `TANGENT0`를 선언했는데 source가 없으면 일반 경로는 incompatibility로 판정한다.

## 5. Runtime Upload 상태

static mesh:

- 초기 GPU upload는 runtime packer 기반이다.
- shader reflected input contract와 base upload contract가 다르면 shader-specific static buffer를 별도로 만들 수 있다.

skeletal mesh:

- runtime skinning 결과 업로드도 runtime packer 기반이다.
- material / shader 변경 시 필요한 reflected layout 집합을 다시 계산한다.
- component-local shader-specific runtime buffer cache는 live set 기준 prune 대상이다.

## 6. Reflected Texture Binding 상태

현재 custom shader의 texture binding은 일반 reflected 경로를 사용한다.

- shader reflection에서 texture resource name / slot index를 수집한다.
- material texture parameter는 canonical slot alias 규칙으로 정규화된다.
- reflected resource name이 canonical alias와 매칭되면 파일명 특례 없이 자동 연결된다.

대표 canonical alias:

- diffuse/base color:
  - `DiffuseTexture`
  - `BaseColorTexture`
  - `AlbedoTexture`
  - `BaseTexture`
  - `DiffuseMap`
  - `AlbedoMap`
  - `g_txColor`
- normal:
  - `NormalTexture`
  - `NormalMap`
  - `NormalMapTexture`
  - `BumpTexture`
  - `BumpMap`
  - `g_NormalMap`
- specular:
  - `SpecularTexture`
  - `SpecularMap`
  - `SpecularMapTexture`
  - `SpecularMask`
  - `SpecularMaskTexture`
  - `GlossMap`
  - `g_SpecularMap`

## 7. Warning Policy

validator warning이 나는 경우:

- reflected shader input에 미지원 semantic이 포함됨
- reflected shader input format이 현재 request list builder가 해석하지 못함
- mesh가 지원 semantic을 갖고 있어도 requested element와 exact match / widening compatibility를 만족하지 못함
- strict-required semantic이 source에 없음

로그 위치:

- `BuildDrawCommand.cpp`

핵심 로그 형태:

- `Mesh/Shader input contract has no compatibility request list`
- `Mesh/Shader input mismatch detected`

## 8. 남은 Known Limits

- static shader-specific runtime buffer cache는 shared asset 소유라 exact-live-set prune를 아직 적용하지 않았다.
- special pass(`DepthPre`, `SelectionMask`, `ShadowMap`)는 reflected shader input 전체가 아니라 고정 최소 contract 정책을 사용한다.
- vertex struct 이름과 일부 field 배치는 legacy naming을 아직 유지한다.
