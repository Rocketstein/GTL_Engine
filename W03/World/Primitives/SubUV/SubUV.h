#pragma once
#include "Engine/Core/CoreTypes.h"
#include "World/PrimitiveComponent.h"
#include "Engine/Render/Resource/RenderResourceManager.h"


class USubUVComponent :
    public UPrimitiveComponent
{
public:
    DECLARE_CLASS(USubUVComponent, UPrimitiveComponent)
    USubUVComponent(std::wstring filename = L"Assets/Effects/Explosion.PNG");

    bool GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand) override;

    void Update(float deltatime) override;
    void UpdateFrame(float deltaTime);

    EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }

    std::wstring GetFileName() { return filename; }
    void ReloadTextureResource(std::wstring filename = L"Assets/Effects/Explosion.PNG");

    void UpdateWorldAABB() override;

    void Serialize(json::JSON& j) const override;
    void Deserialize(const json::JSON& j) override;

public:
    static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_SubUV;

    bool bIsBillBoard = true;
    bool bLoop = true;

    int CellRows, CellColumns;

    float OffsetLeftX, OffsetUpY, OffsetRightX, OffsetBottomY;

    int PlayRate = 24;

private:
    bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

private:
    const FUVMeshData* UVMeshData = nullptr;
    int Texture2DId = -1;

    float TextureWidth, TextureHeight;

    int CurrentFrameIndex;
    int FrameCount;

    float currentU, currentV;
    float CellSizeX, CellSizeY;

    std::wstring filename;

    float Timer;
    float ElaspedTime;

    TextureInfo Texture2Dinfo;

};