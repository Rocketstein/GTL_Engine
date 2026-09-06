#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Mesh/SkeletalMesh.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    FPrimitiveProxy* CreateSceneProxy() override;
    void SetSkeletalMesh(USkeletalMesh* InMesh);

    // Property Editor 지원
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

};
