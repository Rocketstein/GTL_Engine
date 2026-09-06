// =============================================================================
// ShadowDepth.hlsl — Shadow Depth Pass VS / PS
// =============================================================================
// DrawShadowCasters 전용 셰이더.
// b1(PerObjectBuffer)에 Model, b2(PerShader0)에 LightViewProj가 바인딩된 상태.
//
// Hard/PCF: PS=null (depth-only) — C++ 측에서 PSSetShader(nullptr)
// VSM:      PS가 depth moment(z, z^2)를 RTV에 기록
// =============================================================================

#include "Common/ConstantBuffers.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/Skinning.hlsli"
#include "Common/SystemSamplers.hlsli"

// b2: Light ViewProj — Shadow depth pass 전용
cbuffer ShadowLightBuffer : register(b2)
{
    float4x4 LightViewProj;
};

#if defined(SHADOW_MASKED)
// 알파 마스크 그림자 — Masked 머티리얼(예: 풀 quad)의 투명 영역을 그림자 맵에서 제외.
// 메인 패스(UberLit)의 OpacityMaskTexture(t2) / OpacityMaskClipValue 와 동일한 의미.
Texture2D OpacityMaskTexture : register(t2);
cbuffer ShadowMaskBuffer : register(b3)
{
    float OpacityMaskClipValue;
    float3 _maskPad;
};
#endif

// =============================================================================
// Vertex Shader — position-only transform (Model * LightViewProj)
// =============================================================================
// InputLayout은 VS_Input_PNCTT(StaticMesh)와 호환.
// Normal/Color/TexCoord/Tangent는 무시하고 Position만 사용.
PS_Input_Shadow VS_StaticMesh(VS_Input_PNCTT input)
{
    PS_Input_Shadow output;
    float4 worldPos = mul(float4(input.position, 1.0f), Model);
    float4 clipPos = mul(worldPos, LightViewProj);
    output.position = clipPos;
    output.depth = clipPos.z / clipPos.w;
    output.texcoord = input.texcoord;
    return output;
}

PS_Input_Shadow VS_SkeletalMesh(VS_Input_PNCTTBB input)
{
    PS_Input_Shadow output;

    FSkinningResult skinned = ApplyLinearBlendSkinning(
        input.position,
        input.normal,
        input.tangent.xyz,
        input.boneIndices,
        input.boneWeights);
    float4 worldPos = mul(skinned.position, Model);

    float4 clipPos = mul(worldPos, LightViewProj);
    output.position = clipPos;
    output.depth = clipPos.z / clipPos.w;
    output.texcoord = input.texcoord;
    return output;
}


// =============================================================================
// Pixel Shader — EVSM moment 출력 (Hard/PCF 모드에서는 바인딩하지 않음)
// =============================================================================
// RTV format: R32G32_FLOAT — (moment1, moment2) = (exp(c*d), exp(2c*d))
// EVSM: 지수 워프로 깊이 분포를 분리하여 light bleeding 대폭 감소
float2 PS(PS_Input_Shadow input) : SV_TARGET
{
#if defined(SHADOW_MASKED)
    // 투명 픽셀은 그림자 깊이/모멘트 기록에서 제외 → quad 가 아닌 실제 풀 모양만 그림자에 기여.
    float mask = OpacityMaskTexture.Sample(LinearWrapSampler, input.texcoord).r;
    clip(mask - OpacityMaskClipValue);
#endif

    float d = input.depth;
    float e = exp(EVSM_EXPONENT * d);

    float dx = ddx(e);
    float dy = ddy(e);

    // moment2에 partial derivative bias 추가 (shadow acne 완화)
    float moment2 = e * e + 0.25f * (dx * dx + dy * dy);

    return float2(e, moment2);
}
