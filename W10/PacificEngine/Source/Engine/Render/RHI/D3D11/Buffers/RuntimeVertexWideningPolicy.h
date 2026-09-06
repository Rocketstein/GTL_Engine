#pragma once

#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

#include <cstring>

inline uint32 GetRuntimeVertexScalarTypeByteWidth(ERuntimeVertexScalarType ScalarType)
{
    switch (ScalarType)
    {
    case ERuntimeVertexScalarType::Float32:
    case ERuntimeVertexScalarType::UInt32:
    case ERuntimeVertexScalarType::SInt32:
        return 4;
    case ERuntimeVertexScalarType::UInt16:
        return 2;
    default:
        return 0;
    }
}

inline uint32 GetRuntimeVertexElementByteWidth(const FRuntimeVertexElementRequest& Request)
{
    return Request.ComponentCount * GetRuntimeVertexScalarTypeByteWidth(Request.ScalarType);
}

inline uint32 GetRuntimeVertexElementByteWidth(const FRuntimeVertexSemanticSourceDesc& SourceDesc)
{
    return SourceDesc.ComponentCount * GetRuntimeVertexScalarTypeByteWidth(SourceDesc.ScalarType);
}

inline ERuntimeVertexScalarType GetDXGIFormatScalarType(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return ERuntimeVertexScalarType::Float32;
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
        return ERuntimeVertexScalarType::UInt32;
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return ERuntimeVertexScalarType::SInt32;
    default:
        return ERuntimeVertexScalarType::Unknown;
    }
}

inline uint32 GetDXGIFormatComponentCount(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
        return 1;
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
        return 2;
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 3;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 4;
    default:
        return 0;
    }
}

inline bool BuildRuntimeVertexElementRequest(
    const FString& SemanticName,
    uint32 SemanticIndex,
    DXGI_FORMAT Format,
    FRuntimeVertexElementRequest& OutRequest)
{
    OutRequest.SemanticName = SemanticName;
    OutRequest.SemanticIndex = SemanticIndex;
    OutRequest.ComponentCount = GetDXGIFormatComponentCount(Format);
    OutRequest.ScalarType = GetDXGIFormatScalarType(Format);
    return OutRequest.ComponentCount > 0 && OutRequest.ScalarType != ERuntimeVertexScalarType::Unknown;
}

inline bool BuildCanonicalRuntimeVertexSourceDesc(
    const FString& SemanticName,
    uint32 SemanticIndex,
    FRuntimeVertexSemanticSourceDesc& OutSourceDesc)
{
    OutSourceDesc.SemanticName = SemanticName;
    OutSourceDesc.SemanticIndex = SemanticIndex;
    OutSourceDesc.ScalarType = ERuntimeVertexScalarType::Float32;

    if (SemanticName == "POSITION" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::Position;
        OutSourceDesc.ComponentCount = 3;
        return true;
    }

    if (SemanticName == "NORMAL" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::Normal;
        OutSourceDesc.ComponentCount = 3;
        return true;
    }

    if (SemanticName == "COLOR" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::Color;
        OutSourceDesc.ComponentCount = 4;
        return true;
    }

    if (SemanticName == "TANGENT" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::Tangent;
        OutSourceDesc.ComponentCount = 4;
        return true;
    }

    if (SemanticName == "TEXCOORD" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::UV0;
        OutSourceDesc.ComponentCount = 2;
        return true;
    }

    if (SemanticName == "TEXCOORD" && SemanticIndex == 1)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::UV1;
        OutSourceDesc.ComponentCount = 2;
        return true;
    }

    if (SemanticName == "BLENDINDICES" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::BoneIndices;
        OutSourceDesc.ComponentCount = SkeletalMeshLimits::MaxBone;
        OutSourceDesc.ScalarType = ERuntimeVertexScalarType::UInt16;
        return true;
    }

    if (SemanticName == "BLENDWEIGHT" && SemanticIndex == 0)
    {
        OutSourceDesc.Source = ERuntimeVertexSemanticSource::BoneWeights;
        OutSourceDesc.ComponentCount = SkeletalMeshLimits::MaxBone;
        return true;
    }

    OutSourceDesc = {};
    return false;
}

inline bool BuildCanonicalRuntimeVertexSourceDesc(
    const FVertexSemantic& Semantic,
    FRuntimeVertexSemanticSourceDesc& OutSourceDesc)
{
    return BuildCanonicalRuntimeVertexSourceDesc(Semantic.SemanticName, Semantic.SemanticIndex, OutSourceDesc);
}

inline bool IsRuntimeVertexElementExactMatch(
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    const FRuntimeVertexElementRequest& Request)
{
    return SourceDesc.ComponentCount == Request.ComponentCount &&
           SourceDesc.ScalarType == Request.ScalarType;
}

inline bool IsRuntimeVertexWideningCompatible(
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    const FRuntimeVertexElementRequest& Request)
{
    if (SourceDesc.ScalarType != ERuntimeVertexScalarType::Float32 ||
        Request.ScalarType != ERuntimeVertexScalarType::Float32)
    {
        return false;
    }

    const bool bIsSupportedPattern =
        (SourceDesc.ComponentCount == 2 && (Request.ComponentCount == 3 || Request.ComponentCount == 4)) ||
        (SourceDesc.ComponentCount == 3 && Request.ComponentCount == 4);

    return bIsSupportedPattern;
}

inline bool IsRuntimeVertexStrictRequiredSemantic(
    const FString& SemanticName,
    uint32 SemanticIndex)
{
    return (SemanticName == "NORMAL" && SemanticIndex == 0) ||
           (SemanticName == "TANGENT" && SemanticIndex == 0);
}

inline bool IsRuntimeVertexStrictRequiredSemantic(const FVertexSemantic& Semantic)
{
    return IsRuntimeVertexStrictRequiredSemantic(Semantic.SemanticName, Semantic.SemanticIndex);
}

inline bool IsRuntimeVertexStrictRequiredElement(const FRuntimeVertexElementRequest& Request)
{
    return IsRuntimeVertexStrictRequiredSemantic(Request.SemanticName, Request.SemanticIndex);
}

inline bool BuildRuntimeVertexDefaultFillRequest(
    const FString& SemanticName,
    uint32 SemanticIndex,
    FRuntimeVertexElementRequest& OutRequest)
{
    if (IsRuntimeVertexStrictRequiredSemantic(SemanticName, SemanticIndex))
    {
        return false;
    }

    if (SemanticName == "COLOR" && SemanticIndex == 0)
    {
        OutRequest.SemanticName = SemanticName;
        OutRequest.SemanticIndex = SemanticIndex;
        OutRequest.ComponentCount = 4;
        OutRequest.ScalarType = ERuntimeVertexScalarType::Float32;
        return true;
    }

    if (SemanticName == "TEXCOORD" && SemanticIndex == 1)
    {
        OutRequest.SemanticName = SemanticName;
        OutRequest.SemanticIndex = SemanticIndex;
        OutRequest.ComponentCount = 2;
        OutRequest.ScalarType = ERuntimeVertexScalarType::Float32;
        return true;
    }

    return false;
}

inline bool CanDefaultFillRuntimeVertexElement(const FRuntimeVertexElementRequest& Request)
{
    if (Request.ScalarType != ERuntimeVertexScalarType::Float32)
    {
        return false;
    }

    if (Request.SemanticName == "COLOR" && Request.SemanticIndex == 0)
    {
        return Request.ComponentCount >= 1 && Request.ComponentCount <= 4;
    }

    if (Request.SemanticName == "TEXCOORD" && Request.SemanticIndex == 1)
    {
        return Request.ComponentCount >= 1 && Request.ComponentCount <= 4;
    }

    return false;
}

inline bool WriteDefaultFilledRuntimeVertexElement(
    const FRuntimeVertexElementRequest& Request,
    void* OutTargetBytes)
{
    if (!CanDefaultFillRuntimeVertexElement(Request))
    {
        return false;
    }

    float* TargetFloats = static_cast<float*>(OutTargetBytes);
    const float DefaultValue =
        (Request.SemanticName == "COLOR" && Request.SemanticIndex == 0) ? 1.0f : 0.0f;

    for (uint32 ComponentIndex = 0; ComponentIndex < Request.ComponentCount; ++ComponentIndex)
    {
        TargetFloats[ComponentIndex] = DefaultValue;
    }

    return true;
}

inline bool CopyRuntimeVertexElementWithWidening(
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    const FRuntimeVertexElementRequest& Request,
    const void* InSourceBytes,
    void* OutTargetBytes)
{
    if (IsRuntimeVertexElementExactMatch(SourceDesc, Request))
    {
        std::memcpy(OutTargetBytes, InSourceBytes, GetRuntimeVertexElementByteWidth(Request));
        return true;
    }

    if (!IsRuntimeVertexWideningCompatible(SourceDesc, Request))
    {
        return false;
    }

    const float* SourceFloats = static_cast<const float*>(InSourceBytes);
    float* TargetFloats = static_cast<float*>(OutTargetBytes);
    for (uint32 ComponentIndex = 0; ComponentIndex < Request.ComponentCount; ++ComponentIndex)
    {
        TargetFloats[ComponentIndex] =
            ComponentIndex < SourceDesc.ComponentCount ? SourceFloats[ComponentIndex] : 0.0f;
    }
    return true;
}
