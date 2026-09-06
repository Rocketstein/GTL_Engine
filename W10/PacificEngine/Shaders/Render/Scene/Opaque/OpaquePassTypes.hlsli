/*
    OpaquePassTypes.hlsli는 scene opaque geometry 경로에서 공유하는 입출력 타입을 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
*/

#ifndef OPAQUE_PASS_TYPES_HLSLI
#define OPAQUE_PASS_TYPES_HLSLI

struct FVSOutput
{
    float4 position     : SV_POSITION;
    float3 worldNormal  : TEXCOORD0;
    float4 worldTangent : TEXCOORD1;
    float3 worldPos     : TEXCOORD2;
    float4 color        : COLOR0;
    float2 texcoord     : TEXCOORD3;
};

#endif
