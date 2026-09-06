#pragma once
#include "Render/Scene/Proxies/Primitive/PrimitiveProxy.h"
#include "Materials/MaterialCore.h"
#include "Materials/Material.h"
#include <memory>

class UMeshComponent;

class FMeshSceneProxy : public FPrimitiveProxy {
public:
    static constexpr uint32 MAX_LOD = 4;

    FMeshSceneProxy(UMeshComponent* InComponent);

    virtual void UpdateMaterial()			override;
    virtual void UpdateMesh()				override;
    virtual void UpdateShadow()				override = 0;
    virtual void UpdateLOD(uint32 LODLevel) override = 0;


protected:
	virtual ~FMeshSceneProxy() = default;
    virtual UMeshComponent* GetMeshComponent() const = 0;

    // 모든 LOD의 SectionRenderData 재구축
    virtual void RebuildSectionRenderData() = 0;

    // FLODDrawData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FLODDrawData
    {
        FMeshBuffer*                                     MeshBuffer = nullptr;
        TArray<FMeshSectionRenderData>                   SectionRenderData;
        TArray<std::unique_ptr<FMaterialConstantBuffer>> OwnedMaterialCBs;
    };

    FLODDrawData                                     LODData[MAX_LOD];
    TArray<std::unique_ptr<FMaterialConstantBuffer>> ActiveOwnedMaterialCBs;
    uint32                                           LODCount = 1;


	static bool SectionMaterialLess(const FMeshSectionRenderData& A, const FMeshSectionRenderData& B);
    static bool TryGetTextureSRV(UMaterial* Material, std::initializer_list<const char*> SlotNames, ID3D11ShaderResourceView*& OutSRV);
    static TArray<FShaderResourceBinding> BuildReflectedTextureBindings(const UMaterial* Material, const FGraphicsProgram* GraphicsProgram);
	static float GetScalarOrDefault(const UMaterial* Material, const char* ParamName, float DefaultValue);
    static FVector4 GetVector4OrDefault(const UMaterial* Material, const char* ParamName, const FVector4& DefaultValue);
    static std::unique_ptr<FMaterialConstantBuffer> BuildMeshMaterialCB(const UMaterial* Material, ID3D11Device* Device, ID3D11DeviceContext* Context,
                                                                       ID3D11ShaderResourceView* DiffuseSRV, ID3D11ShaderResourceView* NormalSRV,
                                                                       ID3D11ShaderResourceView* SpecularSRV);
    void SortSectionRenderDataByMaterial(TArray<FMeshSectionRenderData>& Draws);
};
