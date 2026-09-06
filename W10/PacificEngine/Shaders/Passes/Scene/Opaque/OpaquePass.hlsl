/*
    OpaquePass.hlsl는 opaque geometry pass의 셰이더입니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - t20~24: ShadowMap
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp, s3: Shadow
    - u#: Compute/후처리용 UAV
*/

/*
    OpaquePass.hlsl
    - Role: opaque geometry shading
    - Main bindings: b0 Frame, b1 PerObject, b2 StaticMeshMaterial, b4 GlobalLight,
      t0 BaseColor, t1 Normal, t2 Specular, t6 LocalLights
    - Available preprocessor defines:
      - LIGHTING_MODEL_LAMBERT
      - LIGHTING_MODEL_BLINNPHONG
      - LIGHTING_MODEL_WORLDNORMAL
      - LIGHTING_MODEL_UNLIT
      - USE_NORMAL_MAP
      - FORWARD_ENABLE_LIGHTING
*/

#include "../../../Utils/Functions.hlsl"
#include "../../../Resources/BindingSlots.hlsli"
#include "../../../Render/Scene/Opaque/OpaquePassTypes.hlsli"
#include "../../../Render/Scene/Material/SurfaceEvaluation.hlsli"
#include "../../../Render/Scene/Lighting/LightingEvaluation.hlsli"

#ifndef FORWARD_ENABLE_LIGHTING
#define FORWARD_ENABLE_LIGHTING 1
#endif

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#define FORWARD_NORMAL_TEXTURE g_NormalMap
#else
#define FORWARD_NORMAL_TEXTURE g_txColor
#endif

Texture2D g_SpecularMap : register(t2);

FVSOutput VSmain(VS_Input_PNCT_T Input)
{
    FVSOutput Output;
    Output.position         = ApplyMVP(Input.position);
    Output.worldPos         = mul(float4(Input.position, 1.0f), Model).xyz;
    Output.worldNormal      = normalize(mul(Input.normal, (float3x3)NormalMatrix));
    Output.worldTangent.xyz = normalize(mul(Input.tangent.xyz, (float3x3)NormalMatrix));
    Output.worldTangent.w   = Input.tangent.w;
    Output.color            = Input.color;
    Output.texcoord         = Input.texcoord;
    return Output;
}

float4 PSmain(FVSOutput Input) : SV_TARGET0
{
    float4 BaseColor = SampleStaticMeshBaseColor(g_txColor, Input.texcoord) * GetStaticMeshSectionColorOrWhite();

#if defined(LIGHTING_MODEL_UNLIT)
    return BaseColor;
#endif

    float3 WorldNormal = ResolveStaticMeshSurfaceNormal(Input.worldNormal, Input.worldTangent, Input.texcoord, FORWARD_NORMAL_TEXTURE);

#if defined(LIGHTING_MODEL_WORLDNORMAL)
    return float4(WorldNormal * 0.5f + 0.5f, BaseColor.a);
#endif

#if FORWARD_ENABLE_LIGHTING
    #if defined(LIGHTING_MODEL_BLINNPHONG)
        float2 MaterialParam = ResolveStaticMeshMaterialParam(Input.texcoord, g_SpecularMap);
        float3 ViewDir = normalize(CameraWorldPos - Input.worldPos);
        return ComputeForwardBlinnPhongLighting(
            BaseColor,
            WorldNormal,
            float4(MaterialParam.x, MaterialParam.y, 0.0f, 1.0f),
            Input.worldPos,
            ViewDir,
            Input.position);
    #elif defined(LIGHTING_MODEL_LAMBERT)
        return ComputeForwardLambertLighting(BaseColor, WorldNormal, Input.worldPos, Input.position);
    #else
        return BaseColor;
    #endif
#else
    return BaseColor;
#endif
}

#undef FORWARD_NORMAL_TEXTURE
