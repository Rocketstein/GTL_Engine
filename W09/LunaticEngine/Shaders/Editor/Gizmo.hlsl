#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"

cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
    uint AxisMask;
    uint3 _gizmoPad;
};

PS_Input_Gizmo VS(VS_Input_Gizmo input)
{
    PS_Input_Gizmo output;
    output.position = ApplyMVP(input.position);
    output.color = input.color * GizmoColorTint;
    output.subID = input.subID;
    return output;
}

float4 PS(PS_Input_Gizmo input) : SV_TARGET
{
    uint axis = input.subID;

    if (axis < 3u && !(AxisMask & (1u << axis)))
    {
        discard;
    }

    float4 outColor = input.color;

    if (axis == SelectedAxis)
    {
        outColor.rgb = float3(1.0f, 1.0f, 0.0f);
        outColor.a = 1.0f;
    }

    if ((bool)bIsInnerGizmo)
    {
        outColor.a *= HoveredAxisOpacity;
    }

    if (axis != SelectedAxis && bClicking)
    {
        discard;
    }

    return saturate(outColor);
}
