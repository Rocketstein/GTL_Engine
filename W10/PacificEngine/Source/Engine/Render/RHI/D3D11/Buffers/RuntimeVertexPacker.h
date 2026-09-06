#pragma once

#include "Render/RHI/D3D11/Buffers/VertexTypes.h"

struct FStaticMesh;
struct FSkeletalSubMesh;

bool IsRuntimeVertexPackerExperimentEnabled();

FRuntimePackedMeshData PackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics);
FRuntimePackedMeshData PackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements);

FRuntimePackedMeshData PackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics);
FRuntimePackedMeshData PackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements);

FRuntimePackedVertexData PackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FVertexSemanticDescriptor& InRequestedSemantics);
FRuntimePackedVertexData PackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FRuntimeVertexElementRequestList& InRequestedElements);

bool TryPackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedMeshData& OutPackedMeshData);
bool TryPackStaticMeshVertices(
    const FStaticMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedMeshData& OutPackedMeshData);

bool TryPackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedMeshData& OutPackedMeshData);
bool TryPackSkeletalMeshVertices(
    const FSkeletalSubMesh& InMesh,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedMeshData& OutPackedMeshData);

bool TryPackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FVertexSemanticDescriptor& InRequestedSemantics,
    FRuntimePackedVertexData& OutPackedVertexData);
bool TryPackSkinnedRuntimeVertices(
    const TArray<FVertexPNCT_T>& InVertices,
    const TArray<FVector2>* InUV1s,
    const FRuntimeVertexElementRequestList& InRequestedElements,
    FRuntimePackedVertexData& OutPackedVertexData);
