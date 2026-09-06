#ifndef VERTEX_LAYOUTS_HLSL
#define VERTEX_LAYOUTS_HLSL

// ============================================================
// VS Input Layouts — C++ VertexTypes.h 와 1:1 대응
// ============================================================

// FVertex (Position + Color)
// 사용: Primitive, Editor, Gizmo, Outline, Line
struct VS_Input_PC
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

// FVertexPNCT (Position + Normal + Color + TexCoord)
// 사용: StaticMesh, OutlinePNCT
struct VS_Input_PNCT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXTCOORD;
};

struct VS_Input_PNCTT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 texcoord : TEXTCOORD;
    float4 tangent : TANGENT;
};

struct VS_Input_PNCTT_InstancedParticle
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 texcoord : TEXTCOORD;
    float4 tangent : TANGENT;

    float3 instanceLocation : INSTANCE_LOCATION;
    float3 instanceSize : INSTANCE_SIZE;
    float4 instanceColor : INSTANCE_COLOR;
    float  instanceRotation : INSTANCE_ROTATION;
};

// 나도 이러고 싶지 않았다.
struct VS_Input_PNCTTBB
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 texcoord : TEXTCOORD;
    float4 tangent : TANGENT;
    int4   boneIndices : BONEINDEX;
    float4 boneWeights : BONEWEIGHT;
};

// FTriangleVertex (Position + Normal + Color)
// 사용: PhysicsBody (솔리드 디버그 바디)
struct VS_Input_PNC
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

// FTextureVertex (Position + TexCoord)
// 사용: Font, SubUV, OverlayFont
struct VS_Input_PT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

// Position only (Outline primitive expansion)
struct VS_Input_P
{
    float3 position : POSITION;
};

// ============================================================
// PS Input (VS -> PS 전달 구조체)
// ============================================================

// SV_POSITION + Color
struct PS_Input_Color
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

// SV_POSITION + TexCoord
struct PS_Input_Tex
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// SV_POSITION + Normal + Color + TexCoord (StaticMesh)
struct PS_Input_Full
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXTCOORD;
};

// SV_POSITION + UV (PostProcess: HeightFog, Outline, SceneDepth)
struct PS_Input_UV
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// SV_POSITION only (Outline)
struct PS_Input_PosOnly
{
    float4 position : SV_POSITION;
};

// SV_POSITION + Color + WorldPos (Editor)
struct PS_Input_ColorWorld
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 worldPos : TEXCOORD0;
};

// SV_POSITION + 월드 법선 + Color (PhysicsBody 솔리드 라이팅)
struct PS_Input_LitColor
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color : COLOR;
};

// SV_POSITION + Depth (ShadowDepth)
struct PS_Input_Shadow
{
    float4 position : SV_POSITION;
    float  depth    : TEXCOORD0;    // VSM용 normalized depth
};

#endif // VERTEX_LAYOUTS_HLSL
