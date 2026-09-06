#include "Render/RHI/D3D11/Buffers/RuntimeVertexPacker.h"

#include "Core/Logging/LogMacros.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Render/RHI/D3D11/Buffers/RuntimeVertexWideningPolicy.h"

#include <cstring>
#include <string>
#include <unordered_set>

namespace
{
constexpr bool GRuntimeVertexPackerExperimentEnabled = true;

const FRuntimeVertexSemanticSourceDesc* FindSourceDesc(
    const FRuntimeVertexSemanticSourceModel& SourceModel,
    const FVertexSemantic& RequestedSemantic)
{
    for (const FRuntimeVertexSemanticSourceDesc& SourceDesc : SourceModel.Sources)
    {
        if (_stricmp(SourceDesc.SemanticName.c_str(), RequestedSemantic.SemanticName.c_str()) == 0 &&
            SourceDesc.SemanticIndex == RequestedSemantic.SemanticIndex)
        {
            return &SourceDesc;
        }
    }

    return nullptr;
}

bool BuildCompatibleRequestList(
    const FRuntimeVertexSemanticSourceModel& SourceModel,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimeVertexElementRequestList& OutRequestList)
{
    OutRequestList.Elements.clear();
    OutRequestList.Elements.reserve(InRequestedSemantics.Elements.size());

    for (const FVertexSemantic& RequestedSemantic : InRequestedSemantics.Elements)
    {
        const FRuntimeVertexSemanticSourceDesc* SourceDesc = FindSourceDesc(SourceModel, RequestedSemantic);
        if (!SourceDesc)
        {
            if (IsRuntimeVertexStrictRequiredSemantic(RequestedSemantic))
            {
                return false;
            }

            FRuntimeVertexElementRequest DefaultFillRequest;
            if (!BuildRuntimeVertexDefaultFillRequest(
                    RequestedSemantic.SemanticName,
                    RequestedSemantic.SemanticIndex,
                    DefaultFillRequest))
            {
                return false;
            }

            OutRequestList.Elements.push_back(DefaultFillRequest);
            continue;
        }

        FRuntimeVertexElementRequest Request;
        Request.SemanticName = RequestedSemantic.SemanticName;
        Request.SemanticIndex = RequestedSemantic.SemanticIndex;
        Request.ComponentCount = SourceDesc->ComponentCount;
        Request.ScalarType = SourceDesc->ScalarType;
        OutRequestList.Elements.push_back(Request);
    }

    return true;
}

bool BuildCompatibleRequestList(
    const FRuntimeVertexSemanticSourceModel& SourceModel,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimeVertexElementRequestList& OutRequestList)
{
    OutRequestList.Elements.clear();
    OutRequestList.Elements.reserve(InRequestedElements.Elements.size());

    for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
    {
        const FVertexSemantic RequestedSemantic = { RequestedElement.SemanticName, RequestedElement.SemanticIndex };
        const FRuntimeVertexSemanticSourceDesc* SourceDesc = FindSourceDesc(SourceModel, RequestedSemantic);
        if (!SourceDesc)
        {
            if (IsRuntimeVertexStrictRequiredElement(RequestedElement) ||
                !CanDefaultFillRuntimeVertexElement(RequestedElement))
            {
                return false;
            }

            OutRequestList.Elements.push_back(RequestedElement);
            continue;
        }

        if (!IsRuntimeVertexElementExactMatch(*SourceDesc, RequestedElement) &&
            !IsRuntimeVertexWideningCompatible(*SourceDesc, RequestedElement))
        {
            return false;
        }

        OutRequestList.Elements.push_back(RequestedElement);
    }

    return true;
}

bool ReadStaticMeshSourceBytes(
    const FStaticMesh& InMesh,
    uint32 VertexIndex,
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    uint8* OutDest)
{
    const FVertexPNCT_T& Vertex = InMesh.Vertices[VertexIndex];

    switch (SourceDesc.Source)
    {
    case ERuntimeVertexSemanticSource::Position:
        std::memcpy(OutDest, &Vertex.Position, sizeof(Vertex.Position));
        return true;
    case ERuntimeVertexSemanticSource::Normal:
        std::memcpy(OutDest, &Vertex.Normal, sizeof(Vertex.Normal));
        return true;
    case ERuntimeVertexSemanticSource::Color:
        std::memcpy(OutDest, &Vertex.Color, sizeof(Vertex.Color));
        return true;
    case ERuntimeVertexSemanticSource::UV0:
        std::memcpy(OutDest, &Vertex.UV, sizeof(Vertex.UV));
        return true;
    case ERuntimeVertexSemanticSource::Tangent:
        std::memcpy(OutDest, &Vertex.Tangent, sizeof(Vertex.Tangent));
        return true;
    case ERuntimeVertexSemanticSource::UV1:
        if (!InMesh.HasValidUV1Data())
        {
            return false;
        }
        std::memcpy(OutDest, &InMesh.UV1s[VertexIndex], GetRuntimeVertexElementByteWidth(SourceDesc));
        return true;
    default:
        return false;
    }
}

bool ReadSkeletalMeshSourceBytes(
    const FSkeletalSubMesh& InMesh,
    uint32 VertexIndex,
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    uint8* OutDest)
{
    const FVertexSkinned& Vertex = InMesh.Vertices[VertexIndex];

    switch (SourceDesc.Source)
    {
    case ERuntimeVertexSemanticSource::Position:
        std::memcpy(OutDest, &Vertex.Position, sizeof(Vertex.Position));
        return true;
    case ERuntimeVertexSemanticSource::Normal:
        std::memcpy(OutDest, &Vertex.Normal, sizeof(Vertex.Normal));
        return true;
    case ERuntimeVertexSemanticSource::Color:
        std::memcpy(OutDest, &Vertex.Color, sizeof(Vertex.Color));
        return true;
    case ERuntimeVertexSemanticSource::UV0:
        std::memcpy(OutDest, &Vertex.UV, sizeof(Vertex.UV));
        return true;
    case ERuntimeVertexSemanticSource::Tangent:
        std::memcpy(OutDest, &Vertex.Tangent, sizeof(Vertex.Tangent));
        return true;
    case ERuntimeVertexSemanticSource::UV1:
        if (!InMesh.HasValidUV1Data())
        {
            return false;
        }
        std::memcpy(OutDest, &InMesh.UV1s[VertexIndex], GetRuntimeVertexElementByteWidth(SourceDesc));
        return true;
    case ERuntimeVertexSemanticSource::BoneIndices:
        std::memcpy(OutDest, Vertex.BoneIndices, GetRuntimeVertexElementByteWidth(SourceDesc));
        return true;
    case ERuntimeVertexSemanticSource::BoneWeights:
        std::memcpy(OutDest, Vertex.BoneWeights, GetRuntimeVertexElementByteWidth(SourceDesc));
        return true;
    default:
        return false;
    }
}

bool ReadSkinnedRuntimeSourceBytes(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    uint32 VertexIndex,
    const FRuntimeVertexSemanticSourceDesc& SourceDesc,
    uint8* OutDest)
{
    const FVertexPNCT_T& Vertex = InVertices[VertexIndex];

    switch (SourceDesc.Source)
    {
    case ERuntimeVertexSemanticSource::Position:
        std::memcpy(OutDest, &Vertex.Position, sizeof(Vertex.Position));
        return true;
    case ERuntimeVertexSemanticSource::Normal:
        std::memcpy(OutDest, &Vertex.Normal, sizeof(Vertex.Normal));
        return true;
    case ERuntimeVertexSemanticSource::Color:
        std::memcpy(OutDest, &Vertex.Color, sizeof(Vertex.Color));
        return true;
    case ERuntimeVertexSemanticSource::UV0:
        std::memcpy(OutDest, &Vertex.UV, sizeof(Vertex.UV));
        return true;
    case ERuntimeVertexSemanticSource::Tangent:
        std::memcpy(OutDest, &Vertex.Tangent, sizeof(Vertex.Tangent));
        return true;
    case ERuntimeVertexSemanticSource::UV1:
        if (!InUV1s || InUV1s->size() != InVertices.size())
        {
            return false;
        }
        std::memcpy(OutDest, &(*InUV1s)[VertexIndex], GetRuntimeVertexElementByteWidth(SourceDesc));
        return true;
    default:
        return false;
    }
}

template <typename WriteFn>
FRuntimePackedVertexData PackVertices(
    uint32 VertexCount,
    const FRuntimeVertexSemanticSourceModel& SourceModel,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    WriteFn&& ReadSourceBytes)
{
    FRuntimePackedVertexData PackedData;
    PackedData.VertexCount = VertexCount;
    PackedData.VertexSemanticDescriptor.Elements.reserve(InRequestedElements.Elements.size());
    for (const FRuntimeVertexElementRequest& Request : InRequestedElements.Elements)
    {
        PackedData.VertexSemanticDescriptor.Elements.push_back({ Request.SemanticName, Request.SemanticIndex });
    }
    if (VertexCount == 0 || InRequestedElements.IsEmpty())
    {
        return PackedData;
    }

    TArray<const FRuntimeVertexSemanticSourceDesc*> ResolvedSources;
    ResolvedSources.reserve(InRequestedElements.Elements.size());

    uint32 Stride = 0;
    for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
    {
        const FVertexSemantic RequestedSemantic = { RequestedElement.SemanticName, RequestedElement.SemanticIndex };
        const FRuntimeVertexSemanticSourceDesc* SourceDesc = FindSourceDesc(SourceModel, RequestedSemantic);

        const uint32 TargetByteWidth = GetRuntimeVertexElementByteWidth(RequestedElement);
        if (TargetByteWidth == 0)
        {
            return {};
        }

        if (!SourceDesc && !CanDefaultFillRuntimeVertexElement(RequestedElement))
        {
            return {};
        }

        ResolvedSources.push_back(SourceDesc);
        Stride += TargetByteWidth;
    }

    PackedData.VertexStride = Stride;
    PackedData.VertexBlob.resize(static_cast<size_t>(VertexCount) * Stride);
    for (uint32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        uint8* Dest = PackedData.VertexBlob.data() + static_cast<size_t>(VertexIndex) * Stride;
        for (size_t ElementIndex = 0; ElementIndex < ResolvedSources.size(); ++ElementIndex)
        {
            const FRuntimeVertexSemanticSourceDesc* SourceDesc = ResolvedSources[ElementIndex];
            const FRuntimeVertexElementRequest& RequestedElement = InRequestedElements.Elements[ElementIndex];
            const uint32 TargetByteWidth = GetRuntimeVertexElementByteWidth(RequestedElement);
            if (TargetByteWidth == 0)
            {
                return {};
            }

            if (!SourceDesc)
            {
                if (!WriteDefaultFilledRuntimeVertexElement(RequestedElement, Dest))
                {
                    return {};
                }
                Dest += TargetByteWidth;
                continue;
            }

            const uint32 SourceByteWidth = GetRuntimeVertexElementByteWidth(*SourceDesc);
            if (SourceByteWidth == 0)
            {
                return {};
            }

            uint8 SourceBytes[32] = {};
            if (!ReadSourceBytes(VertexIndex, *SourceDesc, SourceBytes))
            {
                return {};
            }

            if (!CopyRuntimeVertexElementWithWidening(*SourceDesc, RequestedElement, SourceBytes, Dest))
            {
                return {};
            }

            Dest += TargetByteWidth;
        }
    }

    return PackedData;
}

FString JoinVertexSemanticDescriptorForLog(const FVertexSemanticDescriptor& Descriptor)
{
    FString Result;
    for (const FVertexSemantic& Semantic : Descriptor.Elements)
    {
        if (!Result.empty())
        {
            Result += ",";
        }

        Result += Semantic.SemanticName;
        if (Semantic.SemanticIndex > 0)
        {
            Result += std::to_string(Semantic.SemanticIndex);
        }
    }

    return Result.empty() ? "<none>" : Result;
}

void LogRuntimeVertexPackerFallbackOnce(const char* Context, const char* Reason, const FVertexSemanticDescriptor& Descriptor)
{
    static std::unordered_set<std::string> LoggedKeys;

    const FString DescriptorLabel = JoinVertexSemanticDescriptorForLog(Descriptor);
    const char* ContextLabel = Context ? Context : "UnknownContext";
    const char* ReasonLabel = Reason ? Reason : "UnknownReason";

    const std::string Key =
        std::string(ContextLabel) + "|" + ReasonLabel + "|" + DescriptorLabel;

    if (!LoggedKeys.insert(Key).second)
    {
        return;
    }

    UE_LOG(Render,
           Info,
           "Runtime vertex packer fallback activated. Context=%s Reason=%s RequestedSemantics=%s",
           ContextLabel,
           ReasonLabel,
           DescriptorLabel.c_str());
}
} // namespace

bool IsRuntimeVertexPackerExperimentEnabled()
{
    return GRuntimeVertexPackerExperimentEnabled;
}

FRuntimePackedMeshData PackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics)
{
    FRuntimePackedMeshData PackedMeshData;
    PackedMeshData.Indices = InMesh.Indices;
    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(InMesh.BuildRuntimeSemanticSourceModel(), InRequestedSemantics, RequestList))
    {
        return PackedMeshData;
    }
    PackedMeshData.Vertices = PackVertices(
        static_cast<uint32>(InMesh.Vertices.size()),
        InMesh.BuildRuntimeSemanticSourceModel(),
        RequestList,
        [&InMesh](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadStaticMeshSourceBytes(InMesh, VertexIndex, SourceDesc, OutDest);
        });
    return PackedMeshData;
}

FRuntimePackedMeshData PackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements)
{
    FRuntimePackedMeshData PackedMeshData;
    PackedMeshData.Indices = InMesh.Indices;
    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(InMesh.BuildRuntimeSemanticSourceModel(), InRequestedElements, RequestList))
    {
        return PackedMeshData;
    }
    PackedMeshData.Vertices = PackVertices(
        static_cast<uint32>(InMesh.Vertices.size()),
        InMesh.BuildRuntimeSemanticSourceModel(),
        RequestList,
        [&InMesh](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadStaticMeshSourceBytes(InMesh, VertexIndex, SourceDesc, OutDest);
        });
    return PackedMeshData;
}

FRuntimePackedMeshData PackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics)
{
    FRuntimePackedMeshData PackedMeshData;
    PackedMeshData.Indices = InMesh.Indices;
    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(InMesh.BuildRuntimeSemanticSourceModel(), InRequestedSemantics, RequestList))
    {
        return PackedMeshData;
    }
    PackedMeshData.Vertices = PackVertices(
        static_cast<uint32>(InMesh.Vertices.size()),
        InMesh.BuildRuntimeSemanticSourceModel(),
        RequestList,
        [&InMesh](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadSkeletalMeshSourceBytes(InMesh, VertexIndex, SourceDesc, OutDest);
        });
    return PackedMeshData;
}

FRuntimePackedMeshData PackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements)
{
    FRuntimePackedMeshData PackedMeshData;
    PackedMeshData.Indices = InMesh.Indices;
    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(InMesh.BuildRuntimeSemanticSourceModel(), InRequestedElements, RequestList))
    {
        return PackedMeshData;
    }
    PackedMeshData.Vertices = PackVertices(
        static_cast<uint32>(InMesh.Vertices.size()),
        InMesh.BuildRuntimeSemanticSourceModel(),
        RequestList,
        [&InMesh](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadSkeletalMeshSourceBytes(InMesh, VertexIndex, SourceDesc, OutDest);
        });
    return PackedMeshData;
}

FRuntimePackedVertexData PackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FVertexSemanticDescriptor& InRequestedSemantics)
{
    FRuntimeVertexSemanticSourceModel SourceModel;
    SourceModel.Sources.reserve(InUV1s && InUV1s->size() == InVertices.size() ? 6 : 5);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Position, "POSITION", 0, 3);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Normal, "NORMAL", 0, 3);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Color, "COLOR", 0, 4);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::UV0, "TEXCOORD", 0, 2);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Tangent, "TANGENT", 0, 4);
    if (InUV1s && InUV1s->size() == InVertices.size())
    {
        SourceModel.AddSource(ERuntimeVertexSemanticSource::UV1, "TEXCOORD", 1, 2);
    }

    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(SourceModel, InRequestedSemantics, RequestList))
    {
        return {};
    }

    return PackVertices(
        static_cast<uint32>(InVertices.size()),
        SourceModel,
        RequestList,
        [&InVertices, InUV1s](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadSkinnedRuntimeSourceBytes(InVertices, InUV1s, VertexIndex, SourceDesc, OutDest);
        });
}

FRuntimePackedVertexData PackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FRuntimeVertexElementRequestList& InRequestedElements)
{
    FRuntimeVertexSemanticSourceModel SourceModel;
    SourceModel.Sources.reserve(InUV1s && InUV1s->size() == InVertices.size() ? 6 : 5);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Position, "POSITION", 0, 3);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Normal, "NORMAL", 0, 3);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Color, "COLOR", 0, 4);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::UV0, "TEXCOORD", 0, 2);
    SourceModel.AddSource(ERuntimeVertexSemanticSource::Tangent, "TANGENT", 0, 4);
    if (InUV1s && InUV1s->size() == InVertices.size())
    {
        SourceModel.AddSource(ERuntimeVertexSemanticSource::UV1, "TEXCOORD", 1, 2);
    }

    FRuntimeVertexElementRequestList RequestList;
    if (!BuildCompatibleRequestList(SourceModel, InRequestedElements, RequestList))
    {
        return {};
    }

    return PackVertices(
        static_cast<uint32>(InVertices.size()),
        SourceModel,
        RequestList,
        [&InVertices, InUV1s](uint32 VertexIndex, const FRuntimeVertexSemanticSourceDesc& SourceDesc, uint8* OutDest)
        {
            return ReadSkinnedRuntimeSourceBytes(InVertices, InUV1s, VertexIndex, SourceDesc, OutDest);
        });
}

bool TryPackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedMeshData& OutPackedMeshData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        LogRuntimeVertexPackerFallbackOnce("StaticMeshCreate", "ExperimentDisabled", InRequestedSemantics);
        OutPackedMeshData = {};
        return false;
    }

    OutPackedMeshData = PackStaticMeshVertices(InMesh, InRequestedSemantics);
    if (!OutPackedMeshData.Vertices.IsValid())
    {
        LogRuntimeVertexPackerFallbackOnce("StaticMeshCreate", "PackFailed", InRequestedSemantics);
        return false;
    }

    return true;
}

bool TryPackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedMeshData& OutPackedMeshData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("StaticMeshCreate", "ExperimentDisabled", RequestedSemantics);
        OutPackedMeshData = {};
        return false;
    }

    OutPackedMeshData = PackStaticMeshVertices(InMesh, InRequestedElements);
    if (!OutPackedMeshData.Vertices.IsValid())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("StaticMeshCreate", "PackFailed", RequestedSemantics);
        return false;
    }

    return true;
}

bool TryPackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedMeshData& OutPackedMeshData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        LogRuntimeVertexPackerFallbackOnce("SkeletalMeshCreate", "ExperimentDisabled", InRequestedSemantics);
        OutPackedMeshData = {};
        return false;
    }

    OutPackedMeshData = PackSkeletalMeshVertices(InMesh, InRequestedSemantics);
    if (!OutPackedMeshData.Vertices.IsValid())
    {
        LogRuntimeVertexPackerFallbackOnce("SkeletalMeshCreate", "PackFailed", InRequestedSemantics);
        return false;
    }

    return true;
}

bool TryPackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedMeshData& OutPackedMeshData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("SkeletalMeshCreate", "ExperimentDisabled", RequestedSemantics);
        OutPackedMeshData = {};
        return false;
    }

    OutPackedMeshData = PackSkeletalMeshVertices(InMesh, InRequestedElements);
    if (!OutPackedMeshData.Vertices.IsValid())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("SkeletalMeshCreate", "PackFailed", RequestedSemantics);
        return false;
    }

    return true;
}

bool TryPackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedVertexData& OutPackedVertexData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        LogRuntimeVertexPackerFallbackOnce("SkinnedRuntimeUpdate", "ExperimentDisabled", InRequestedSemantics);
        OutPackedVertexData = {};
        return false;
    }

    OutPackedVertexData = PackSkinnedRuntimeVertices(InVertices, InUV1s, InRequestedSemantics);
    if (!OutPackedVertexData.IsValid())
    {
        LogRuntimeVertexPackerFallbackOnce("SkinnedRuntimeUpdate", "PackFailed", InRequestedSemantics);
        return false;
    }

    return true;
}

bool TryPackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedVertexData& OutPackedVertexData)
{
    if (!IsRuntimeVertexPackerExperimentEnabled())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("SkinnedRuntimeUpdate", "ExperimentDisabled", RequestedSemantics);
        OutPackedVertexData = {};
        return false;
    }

    OutPackedVertexData = PackSkinnedRuntimeVertices(InVertices, InUV1s, InRequestedElements);
    if (!OutPackedVertexData.IsValid())
    {
        FVertexSemanticDescriptor RequestedSemantics;
        RequestedSemantics.Elements.reserve(InRequestedElements.Elements.size());
        for (const FRuntimeVertexElementRequest& RequestedElement : InRequestedElements.Elements)
        {
            RequestedSemantics.Elements.push_back({ RequestedElement.SemanticName, RequestedElement.SemanticIndex });
        }
        LogRuntimeVertexPackerFallbackOnce("SkinnedRuntimeUpdate", "PackFailed", RequestedSemantics);
        return false;
    }

    return true;
}
