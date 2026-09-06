# HDR Bloom / Gamma Correction 문제 분석 및 해결 기록

이 문서는 HDR Bloom 구현 중 발생했던 `Gamma Correction을 켜면 Bloom이 사라지는 문제`, D3D11 MRT 경고, 그리고 셰이더 템플릿 생성 크래시를 정리한 기록이다.

핵심 결론은 다음과 같다.

1. Bloom 알고리즘 자체보다 렌더 패스 정렬과 후처리 입력 복사 타이밍이 문제였다.
2. `GammaCorrection` 패스의 enum 값이 16인데, draw command sort key는 render pass를 4비트만 저장하고 있었다.
3. 그 결과 `GammaCorrection` command가 `PreDepth` bucket처럼 정렬될 수 있었고, `DrawCommandList::GetPassRange()`가 기대하는 "패스별 contiguous range" 가 깨졌다.
4. Gamma pass는 최종 tone mapping 패스여야 하는데, 잘못된 command range 때문에 Bloom이 합성된 SceneColor가 아니라 Bloom 이전 상태를 다시 화면에 쓰는 것처럼 보였다.
5. 별도로 UberLit MRT pixel shader가 single-RT 패스에서 사용되면서 D3D11 `RENDERTARGETVIEW_NOT_SET` 경고가 발생했다.
6. 일부 셰이더 생성 실패가 `FMaterialTemplate::Create()`까지 흘러가면 null 또는 invalid shader dereference로 크래시가 날 수 있었다.

## 구현 목표

Bloom 파이프라인의 의도된 순서는 다음과 같다.

```text
Opaque / Decal / AlphaBlend
        |
        v
HDR SceneColor
        |
        v
Bloom Prefilter -> Downsample -> Blur -> Composite
        |
        v
Bloom이 합성된 HDR SceneColor
        |
        v
FXAA, UI 등 후처리
        |
        v
GammaCorrection: Tone Mapping + Linear to sRGB
        |
        v
Backbuffer 표시
```

중요한 점은 Bloom composite가 tone mapping 이전에 수행되어야 한다는 것이다. Bloom은 HDR 값, 즉 `1.0`을 초과하는 밝기 정보를 이용해 threshold/prefilter를 수행한다. GammaCorrection은 마지막에 HDR SceneColor를 LDR display color로 변환해야 한다.

## 증상

### 1. Gamma Correction을 끄면 Bloom이 보임

Gamma Correction을 끄면 Bloom pass가 합성한 결과가 그대로 viewport에 남아서 빛번짐이 보였다.

반대로 Gamma Correction을 켜면 Bloom을 켜지 않은 것과 거의 동일하게 보였다. 즉, Bloom prefilter/blur/composite가 전혀 안 된 것처럼 최종 화면이 덮였다.

이 증상 때문에 처음에는 다음 가능성을 의심했다.

- Bloom threshold가 너무 높다.
- Particle emissive가 HDR로 올라가지 않는다.
- Bloom composite가 tone mapping 후에 수행된다.
- Gamma shader의 tone mapping 곡선이 Bloom을 너무 강하게 압축한다.
- RenderTarget/SRV binding이 꼬여 Bloom texture가 비어 있다.

하지만 실제 핵심 원인은 draw command 정렬 버그였다.

### 2. D3D11 경고

다음 경고가 반복되었다.

```text
ID3D11DeviceContext::DrawIndexed:
The Pixel Shader expects a Render Target View bound to slot 1, but none is bound.
The Pixel Shader expects a Render Target View bound to slot 2, but none is bound.
```

이 경고는 pixel shader가 `SV_TARGET1`, `SV_TARGET2`를 출력하는데 현재 pass에는 RTV slot 1, 2가 바인딩되어 있지 않을 때 발생한다.

### 3. MaterialTemplate 크래시

다음 코드에서 크래시가 발생했다.

```cpp
void FMaterialTemplate::Create(FShader* InShader)
{
    ParameterLayout = InShader->GetParameterLayout();
    Shader = InShader;
}
```

`InShader`가 null이거나 invalid shader일 수 있는데, 방어 없이 바로 dereference하고 있었다.

## 원인 1: RenderPass SortKey 비트 폭 부족

문제가 된 구조는 `FDrawCommand::ComputeSortKey()`였다.

기존 코드는 render pass를 4비트만 저장했다.

```cpp
Key |= (static_cast<uint64>(InPass) & 0xF) << 60;
```

그런데 `ERenderPass` enum에는 `GammaCorrection`까지 포함되어 있고, `GammaCorrection`의 값은 16이다.

```cpp
enum class ERenderPass : uint32
{
    PreDepth,        // 0
    LightCulling,    // 1
    ShadowMap,       // 2
    Opaque,          // 3
    Decal,           // 4
    SelectionMask,   // 5
    EditorLines,     // 6
    PostProcess,     // 7
    AdditiveDecal,   // 8
    AlphaBlend,      // 9
    Bloom,           // 10
    FXAA,            // 11
    GizmoOuter,      // 12
    GizmoInner,      // 13
    OverlayFont,     // 14
    UI,              // 15
    GammaCorrection, // 16
    MAX
};
```

`16 & 0xF`는 `0`이다. 즉 `GammaCorrection` command가 sort key 상으로는 `PreDepth`와 같은 pass bucket에 들어갈 수 있었다.

이 엔진의 command 실행 방식은 다음 전제를 가진다.

1. `DrawCommandList::Sort()`가 command를 pass 순서대로 정렬한다.
2. `DrawCommandList::Sort()`가 정렬된 command 배열을 순회하며 pass별 offset range를 만든다.
3. 각 render pass는 `GetPassRange(Pass)`로 자기 command 범위만 실행한다.

그런데 Gamma command가 sort key에서 pass 0처럼 섞이면, command 배열이 실제 `ERenderPass` 순서대로 그룹화되지 않는다. 그러면 `PassOffsets` 계산이 깨진다.

결과적으로 어떤 pass의 command range가 비거나, Gamma command가 기대한 마지막 위치가 아닌 곳에 놓이거나, Bloom 이후에 읽어야 할 SceneColor가 아닌 이전 상태를 읽는 문제가 발생할 수 있었다.

사용자 관점에서는 이것이 "Gamma Correction을 켜면 Bloom 합성 결과가 사라지고 Bloom Off와 같은 화면이 된다" 로 보였다.

## 해결 1: SortKey와 Sort 비교 수정

수정 파일:

- `KraftonEngine/Source/Engine/Render/Command/DrawCommand.h`
- `KraftonEngine/Source/Engine/Render/Command/DrawCommandList.cpp`

### RenderPass 비트 폭 확장

Pass 저장 비트를 4비트에서 5비트로 확장했다.

```cpp
Key |= (static_cast<uint64>(InPass) & 0x1F) << 59; // [63:59] Pass
```

`0x1F`는 5비트이므로 0~31까지 표현 가능하다. 현재 `ERenderPass::MAX`는 17이므로 충분하다.

나머지 bit layout도 한 칸씩 조정했다.

```cpp
Key |= (static_cast<uint64>(PtrHash16(InShader))) << 43;
Key |= (static_cast<uint64>(PtrHash16(InMeshId))) << 27;
Key |= (static_cast<uint64>(PtrHash16(InSRV))) << 11;
Key |= (static_cast<uint64>(UserBits) & 0x7FF);
```

### Sort에서 Pass를 명시적으로 우선 비교

SortKey만 믿지 않고, `stable_sort` 비교 함수에서 `A.Pass != B.Pass`를 먼저 비교하도록 했다.

```cpp
if (A.Pass != B.Pass)
{
    return static_cast<uint32>(A.Pass) < static_cast<uint32>(B.Pass);
}
```

이렇게 하면 앞으로 SortKey layout이 다시 바뀌더라도 pass grouping 전제는 쉽게 깨지지 않는다.

## 원인 2: 꺼진 후처리 패스도 BeginPass에서 복사 리소스를 만짐

`GammaCorrectionPass::BeginPass()`와 `FXAAPass::BeginPass()`는 command가 없더라도 render pass pipeline에서 호출된다.

기존 구조에서는 GammaCorrection show flag가 꺼져 있어도 `BeginPass()`가 SceneColor copy를 수행할 수 있었다.

```cpp
DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
```

후처리 패스의 BeginPass가 꺼진 상태에서도 `SceneColorCopyTexture`를 갱신하면, 다른 후처리 패스가 기대하는 입력 상태를 불필요하게 건드릴 수 있다.

## 해결 2: Gamma/FXAA BeginPass에서 show flag 확인

수정 파일:

- `KraftonEngine/Source/Engine/Render/RenderPass/GammaCorrectionPass.cpp`
- `KraftonEngine/Source/Engine/Render/RenderPass/FXAAPass.cpp`

GammaCorrection:

```cpp
if (!Frame.RenderOptions.ShowFlags.bGammaCorrection)
{
    return false;
}
```

FXAA:

```cpp
if (!Frame.RenderOptions.ShowFlags.bFXAA)
    return false;
```

이제 꺼진 후처리 패스는 BeginPass에서 리소스 복사나 바인딩을 하지 않는다.

## 원인 3: CopyResource 전에 RTV가 아직 바인딩되어 있음

SceneColor를 postprocess input으로 쓰려면 다음 흐름이 필요하다.

```text
ViewportRenderTexture(RTV로 사용 중)
        |
        | CopyResource
        v
SceneColorCopyTexture(SRV로 샘플링)
```

D3D11에서는 같은 resource를 render target으로 바인딩한 상태에서 복사/샘플링을 섞으면 상태 충돌이 발생할 수 있다. 그래서 `CopyResource()` 전에 render target binding을 해제해야 한다.

## 해결 3: CopyResource 전에 OM render target 해제

GammaCorrection과 FXAA에서 `CopyResource()` 전에 다음 호출을 추가했다.

```cpp
DC->OMSetRenderTargets(0, nullptr, nullptr);
DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);
```

Bloom pass 쪽도 동일하게 복사 전에 render target을 해제한다.

## 원인 4: UberLit MRT shader가 single-RT pass에서 사용됨

`UberLit.hlsl`은 Opaque pass에서 GBuffer용 MRT를 출력한다.

```hlsl
struct UberPS_Output
{
    float4 Color : SV_TARGET0;
#if !defined(UBER_COLOR_ONLY) || !UBER_COLOR_ONLY
    float4 Normal : SV_TARGET1;
    float4 Culling : SV_TARGET2;
#endif
};
```

Opaque pass에서는 slot 0, 1, 2 RTV가 바인딩된다.

```cpp
ID3D11RenderTargetView* RTVs[3] = {
    Cache.RTV,
    Frame.NormalRTV,
    Frame.CullingHeatmapRTV
};
DC->OMSetRenderTargets(NumRTs, RTVs, Cache.DSV);
```

하지만 AlphaBlend, AdditiveDecal, SelectionMask, PostProcess 같은 pass는 일반적으로 slot 0만 쓴다. 그런데 이 pass에서 MRT 출력을 가진 UberLit pixel shader가 사용되면 D3D11은 다음 경고를 띄운다.

```text
Pixel Shader expects a Render Target View bound to slot 1, but none is bound.
Pixel Shader expects a Render Target View bound to slot 2, but none is bound.
```

## 해결 4: UberLit color-only permutation 사용

수정 파일:

- `KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.cpp`
- `KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.h`
- `KraftonEngine/Source/Engine/Render/Shader/ShaderManager.cpp`
- `KraftonEngine/Source/Engine/Render/Shader/ShaderManager.h`

### path 기반 UberLit 판별

기존 코드는 proxy shader pointer가 기본 UberLit pointer와 같은지만 확인했다.

이 방식은 UberLit permutation이 늘어나면 놓칠 수 있다.

그래서 `FShaderManager::IsShaderFromPath()`를 추가해, 특정 shader pointer가 어떤 shader path에서 생성되었는지 캐시를 통해 확인하게 했다.

```cpp
bool FShaderManager::IsShaderFromPath(const FShader* Shader, const FString& Path) const;
```

### MRT가 없는 pass에서는 color-only UberLit 선택

`DrawCommandBuilder`에서 현재 frame이 MRT를 가지고 있는지 저장했다.

```cpp
bCollectHasMRT = Frame.NormalRTV != nullptr && Frame.CullingHeatmapRTV != nullptr;
```

그리고 다음 조건이면 color-only permutation을 사용한다.

```cpp
const bool bColorOnly = Pass != ERenderPass::Opaque || !bCollectHasMRT;
```

즉 Opaque가 아니거나, Opaque라도 MRT가 준비되지 않은 경우에는 `SV_TARGET1/2`를 출력하지 않는 UberLit permutation을 사용한다.

## 원인 5: Gamma Correction이 단순 sRGB 변환만 수행

HDR Bloom을 넣기 전에는 SceneColor가 대체로 0~1 범위였기 때문에 gamma 변환만 해도 큰 문제가 없었다.

하지만 HDR Bloom 이후에는 다음과 같은 값이 생긴다.

```text
SceneColor.rgb > 1.0
```

이 값을 단순히 sRGB 변환하면 displayable range로 잘 매핑되지 않는다. Bloom 합성은 tone mapping 전에 수행하고, 최종 GammaCorrection pass에서 tone mapping 후 sRGB 변환을 해야 한다.

## 해결 5: GammaCorrection에 ACES tone mapping 추가

수정 파일:

- `KraftonEngine/Shaders/PostProcess/GammaCorrection.hlsl`

기존 exponential mapping 대신 ACES filmic curve를 적용했다.

```hlsl
float3 ACESFilm(float3 color)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;
    return saturate((color * (A * color + B)) / (color * (C * color + D) + E));
}

float3 ApplyToneMapping(float3 hdrColor)
{
    hdrColor = max(hdrColor, 0.0f) * max(Exposure, 0.0f);
    return ACESFilm(hdrColor);
}
```

최종 pixel shader는 다음 순서로 처리한다.

```hlsl
float4 sceneColor = SceneColorTexture.SampleLevel(LinearClampSampler, input.uv, 0);
float3 toneMapped = ApplyToneMapping(sceneColor.rgb);
return float4(LinearToSRGB(toneMapped), sceneColor.a);
```

## 원인 6: 셰이더 생성 실패가 MaterialTemplate 크래시로 이어짐

Bloom/Gamma 작업 중 shader permutation이 늘어나면서, 셰이더 생성 실패가 더 잘 드러났다.

기존 `FMaterialTemplate::Create()`는 `InShader`가 유효하다고 가정했다.

```cpp
ParameterLayout = InShader->GetParameterLayout();
Shader = InShader;
```

하지만 `FShaderManager::GetOrCreate()` 또는 `PreCompile()`이 실패한 shader object를 반환하거나, material manager가 null shader를 제대로 걸러내지 못하면 여기서 바로 크래시가 난다.

## 해결 6: invalid shader 방어

수정 파일:

- `KraftonEngine/Source/Engine/Materials/Material.cpp`
- `KraftonEngine/Source/Engine/Materials/MaterialManager.cpp`
- `KraftonEngine/Source/Engine/Render/Shader/ShaderManager.cpp`
- `KraftonEngine/Source/Engine/Render/Command/DrawCommandList.cpp`

`FMaterialTemplate::Create()`:

```cpp
ParameterLayout.clear();
Shader = nullptr;

if (!InShader || !InShader->IsValid())
{
    UE_LOG("[MaterialTemplate] Invalid shader passed to template creation.");
    return;
}
```

`FMaterialManager::GetOrCreateTemplate()`:

```cpp
if (!Shader || !Shader->IsValid())
{
    UE_LOG("[MaterialManager] Failed to create material template. ShaderPath=%s", ShaderPath.c_str());
    return nullptr;
}
```

`FShaderManager::GetOrCreate()` / `PreCompile()`:

```cpp
if (!CacheEntry.Shader->IsValid())
{
    UE_LOG("[ShaderManager] Failed to create shader ...");
    return nullptr;
}
```

`FDrawCommandList::SubmitCommand()`:

```cpp
if (!Cmd.Shader || !Cmd.Shader->IsValid())
{
    return;
}
```

이 방어들은 근본 원인을 숨기기 위한 것이 아니라, 셰이더 컴파일 실패를 로그로 드러내고 렌더 스레드 크래시를 막기 위한 안전장치다.

## 최종 패스 흐름

최종적으로 기대하는 흐름은 다음과 같다.

```text
1. Viewport BeginRender
   - HDR SceneColor RT clear
   - Depth clear

2. Scene geometry passes
   - PreDepth
   - Opaque with MRT
   - Decal / PostProcess / AlphaBlend

3. Bloom
   - Copy HDR SceneColor to SceneColorCopyTexture
   - Prefilter bright HDR pixels
   - Downsample mip chain
   - Blur ping-pong
   - Composite bloom back into HDR SceneColor

4. FXAA, if enabled
   - Copy current SceneColor
   - Read SceneColorCopyTexture
   - Write anti-aliased result to SceneColor

5. UI / Overlay
   - Render editor/game UI into SceneColor target

6. GammaCorrection, if enabled
   - Copy final SceneColor
   - Tone map HDR to LDR
   - Convert linear to sRGB
   - Write display color to SceneColor target
```

## 재발 방지 체크리스트

새 render pass를 추가할 때는 다음을 확인해야 한다.

1. `ERenderPass` enum 값이 `FDrawCommand::ComputeSortKey()`의 pass bit width 안에 들어가는지 확인한다.
2. `DrawCommandList::Sort()`가 실제 `A.Pass`를 우선 비교하는지 유지한다.
3. `GetPassRange()`는 command가 pass별로 contiguous하게 정렬되어 있다는 전제를 가진다.
4. 후처리 pass의 `BeginPass()`는 해당 show flag가 꺼져 있으면 `false`를 반환해야 한다.
5. `CopyResource()` 전에 source/destination resource가 RTV/SRV/UAV로 충돌 바인딩되어 있지 않은지 확인한다.
6. single-RT pass에서 MRT 출력 pixel shader를 쓰지 않는다.
7. HDR pipeline에서는 Bloom composite 후 tone mapping을 수행한다.
8. GammaCorrection은 "gamma만" 하는 pass가 아니라 최종 display transform pass로 취급한다.
9. shader permutation 추가 시 `FShaderManager`가 invalid shader를 성공처럼 캐시하지 않게 한다.
10. 런타임 셰이더 컴파일 실패는 C++ 빌드 성공과 별개이므로 `fxc` 또는 실제 실행 로그로 확인한다.

## 검증한 항목

빌드:

```powershell
cmd /c "set Path=& call ""C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"" -no_logo && msbuild ""KraftonEngine.sln"" /p:Configuration=Debug /p:Platform=x64 /m /v:minimal"
```

확인 결과:

- `Debug x64` 빌드 성공
- `UberLit` color-only permutation 컴파일 확인
- `Billboard.hlsl` 컴파일 확인
- `GammaCorrection.hlsl` 컴파일 확인
- `BloomPrefilter.hlsl` / `BloomComposite.hlsl` 컴파일 확인
- Runtime에서 `Bloom On + Gamma Correction On` 조합 정상 표시 확인

## 요약

이번 문제는 Bloom shader나 particle emissive만 보면 잘 안 보이는 종류의 버그였다. 실제로는 렌더 패스 enum이 16개를 넘으면서 기존 sort key의 4비트 pass 저장 방식이 overflow했고, 그 결과 command range 계산이 깨져 최종 GammaCorrection이 Bloom이 합성된 올바른 SceneColor를 처리하지 못했다.

해결은 세 축으로 이루어졌다.

1. Render pass 정렬 안정화
2. 후처리 pass의 입력 복사/토글 조건 정리
3. MRT shader와 shader invalid 상태 방어

이후 Bloom pipeline은 다음 원칙을 따른다.

```text
HDR SceneColor 유지
-> Bloom은 tone mapping 전에 합성
-> GammaCorrection에서 tone mapping + sRGB 변환
-> render pass command는 enum 순서대로 안정적으로 group/execute
```
