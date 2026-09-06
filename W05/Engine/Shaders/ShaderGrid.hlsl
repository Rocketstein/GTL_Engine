cbuffer FGridLineConstants : register(b0)
{
    row_major float4x4 LineViewProjection;
};

cbuffer FGridQuadConstants : register(b0)
{
    row_major float4x4 QuadViewProjection;
    row_major float4x4 QuadInverseViewProjection;
    float4 GridParams;
    float4 EyePositionFade;
    float4 RenderParams;
};

struct FLineVSInput
{
    float3 Position : POSITION;
    float4 Color    : COLOR0;
};

struct FLineVSOutput
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

FLineVSOutput GridLineVS(FLineVSInput In)
{
    FLineVSOutput Out;
    Out.Position = mul(float4(In.Position, 1.0f), LineViewProjection);
    Out.Color = In.Color;
    return Out;
}

float4 GridLinePS(FLineVSOutput In) : SV_TARGET
{
    return In.Color;
}

struct FQuadVSInput
{
    float3 Position : POSITION;
};

struct FQuadVSOutput
{
    float4 Position      : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNear     : TEXCOORD1;
    float3 WorldFar      : TEXCOORD2;
};

FQuadVSOutput GridQuadVS(FQuadVSInput In)
{
    FQuadVSOutput Out;

    const bool bScreenSpaceInfinite = (RenderParams.x > 0.5f);
    if (bScreenSpaceInfinite)
    {
        float4 ClipPosition = float4(In.Position.xy, 0.0f, 1.0f);
        float4 NearWorld = mul(float4(In.Position.xy, 0.0f, 1.0f), QuadInverseViewProjection);
        float4 FarWorld = mul(float4(In.Position.xy, 1.0f, 1.0f), QuadInverseViewProjection);

        NearWorld.xyz /= max(NearWorld.w, 1e-4f);
        FarWorld.xyz /= max(FarWorld.w, 1e-4f);

        Out.Position = ClipPosition;
        Out.WorldPosition = float3(0.0f, 0.0f, 0.0f);
        Out.WorldNear = NearWorld.xyz;
        Out.WorldFar = FarWorld.xyz;
        return Out;
    }

    Out.WorldPosition = In.Position;
    Out.WorldNear = float3(0.0f, 0.0f, 0.0f);
    Out.WorldFar = float3(0.0f, 0.0f, 0.0f);
    Out.Position = mul(float4(In.Position, 1.0f), QuadViewProjection);
    return Out;
}

float GridLineFactor(float2 Coord, float Thickness)
{
    float2 Cell = abs(frac(Coord - 0.5f) - 0.5f) / max(fwidth(Coord), 1e-4f);
    float DistanceToLine = min(Cell.x, Cell.y);
    return saturate(1.0f - DistanceToLine / max(Thickness, 1e-4f));
}

float4 EvaluateGridAtWorldPosition(float3 WorldPosition) : SV_TARGET
{
    const float Spacing = max(GridParams.x, 1.0f);
    const float MajorEvery = max(GridParams.y, 1.0f);
    const float MinorThickness = max(GridParams.z, 0.25f);
    const float MajorThickness = max(GridParams.w, MinorThickness);

    const float2 GridUV = WorldPosition.xy / Spacing;
    const float2 MajorUV = GridUV / MajorEvery;

    float Minor = GridLineFactor(GridUV, MinorThickness);
    float Major = GridLineFactor(MajorUV, MajorThickness);

    float3 MinorColor = float3(0.24f, 0.24f, 0.24f);
    float3 MajorColor = float3(0.46f, 0.46f, 0.46f);

    float AxisX = saturate(1.0f - abs(WorldPosition.y) / max(fwidth(WorldPosition.y) * 2.0f, 1e-4f));
    float AxisY = saturate(1.0f - abs(WorldPosition.x) / max(fwidth(WorldPosition.x) * 2.0f, 1e-4f));

    float3 Color = MinorColor * Minor;
    Color = lerp(Color, MajorColor, Major);
    Color = lerp(Color, float3(0.85f, 0.20f, 0.20f), AxisX);
    Color = lerp(Color, float3(0.20f, 0.85f, 0.20f), AxisY);

    const float FadeDistance = max(EyePositionFade.w, Spacing);
    const float DistanceXY = distance(WorldPosition.xy, EyePositionFade.xy);
    const float Fade = saturate(1.0f - DistanceXY / FadeDistance);
    const float Alpha = saturate(max(Minor, Major) * Fade + max(AxisX, AxisY) * 0.65f);

    clip(Alpha - 0.01f);
    return float4(Color, 1.0f);
}

float4 GridQuadPS(FQuadVSOutput In) : SV_TARGET
{
    const bool bScreenSpaceInfinite = (RenderParams.x > 0.5f);
    if (bScreenSpaceInfinite)
    {
        const float3 RayOrigin = In.WorldNear;
        const float3 RayDirection = normalize(In.WorldFar - In.WorldNear);
        const float Denominator = RayDirection.z;
        clip(abs(Denominator) - 1e-5f);

        const float T = -RayOrigin.z / Denominator;
        clip(T);

        const float3 WorldPosition = RayOrigin + (RayDirection * T);
        return EvaluateGridAtWorldPosition(WorldPosition);
    }

    return EvaluateGridAtWorldPosition(In.WorldPosition);
}
