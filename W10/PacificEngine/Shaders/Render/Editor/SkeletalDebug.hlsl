/*
    SkeletalDebug.hlsl draws bone visualization meshes (e.g. joint cones)
    on top of the editor scene with a flat per-object color.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject 상수 버퍼 (Model + PrimitiveColor)
*/

#include "../../Utils/Functions.hlsl"
#include "../../Resources/VertexLayouts.hlsl"

// Cone meshes loaded through FObjManager use the FVertexPNCT_T layout, so the
// VS input must declare the full PNCT_T signature for the reflected input
// layout to match the vertex buffer stride. Only position is consumed here.
struct VS_Output
{
    float4 position : SV_POSITION;
};

VS_Output VS(VS_Input_PNCT_T input)
{
    VS_Output output;
    output.position = ApplyMVP(input.position);
    return output;
}

float4 PS(VS_Output input) : SV_TARGET
{
    return PrimitiveColor;
}
