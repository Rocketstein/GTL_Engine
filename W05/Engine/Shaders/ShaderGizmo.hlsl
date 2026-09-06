// Slot b0: per-object world and WVP matrices (row-major, matches FD3D11PerObjectConstants)
cbuffer FPerObjectConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 WorldViewProjection;
};

// Slot b1: gizmo-specific parameters
cbuffer FGizmoParams : register(b1)
{
    float  SelectedAxis; // -1 = none, 0 = X, 1 = Y, 2 = Z
    float3 Pad;
};

struct VSInput
{
    float3 Position  : POSITION;
    float4 Color     : COLOR;
    float  AxisIndex : TEXCOORD1; // which axis this vertex belongs to
};

struct VSOutput
{
    float4 PositionCS : SV_POSITION;
    float4 Color      : COLOR;
    float  AxisIndex  : TEXCOORD1;
};

VSOutput mainVS(VSInput Input)
{
    VSOutput Output;
    Output.PositionCS = mul(float4(Input.Position, 1.0f), WorldViewProjection);
    Output.Color      = Input.Color;
    Output.AxisIndex  = Input.AxisIndex;
    return Output;
}

float4 mainPS(VSOutput Input) : SV_TARGET
{
    float4 Color = Input.Color;

    // Brighten the axis currently under the cursor
    if (SelectedAxis >= 0.0f && abs(Input.AxisIndex - SelectedAxis) < 0.5f)
        Color.rgb = saturate(Color.rgb * 1.8f + 0.2f);

    return Color;
}
