#ifndef DOF_COMMON_HLSL
#define DOF_COMMON_HLSL

#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

// Shared across CoC / Blur / Composite (matches C++ FDOFCB, PerShader0).
cbuffer DOFCB : register(b2)
{
    float Aperture;
    float FocusDist;
    float FocalLength;
    int   DOFSample;
    float CameraNear;
    float CameraFar;
    float2 SourceTexelSize;
};

// Reversed-Z: d=1 at near, d=0 at far -> view-space distance.
float LinearizeDepth(float d)
{
    return CameraNear * CameraFar / (CameraNear - d * (CameraNear - CameraFar));
}

// Thin-lens CoC, signed: <0 foreground, 0 in focus, >0 background. Clamped to [-1,1].
float ComputeCoC(float viewDist)
{
    float coc = Aperture * FocalLength * (viewDist - FocusDist) /
                (viewDist * max(FocusDist - FocalLength, 1e-4f));
    return clamp(coc, -1.0f, 1.0f);
}

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

#endif // DOF_COMMON_HLSL
