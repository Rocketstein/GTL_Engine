// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "SubUVComponent.h"
#include "Object/ObjectFactory.h"

#include <cstring>
#include "Render/Resources/Buffers/MeshBufferManager.h"
#include "Resource/SpriteVfxManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Scene/Proxies/Primitive/SubUVSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USubUVComponent, UBillboardComponent)

FPrimitiveProxy* USubUVComponent::CreateSceneProxy()
{
    return new FSubUVSceneProxy(this);
}

void USubUVComponent::Serialize(FArchive& Ar)
{
    /*
        공통 width/height 직렬화를 위해 부모 빌보드 직렬화를 먼저 호출합니다.
        SubUV는 별도 SpriteVfx 리소스를 사용하므로 SpriteVfx 전용 값은 아래에서 추가로 저장합니다.
    */
    UBillboardComponent::Serialize(Ar);

    Ar << SpriteVfxName;
    Ar << FrameIndex;
    Ar << PlayRate;
    Ar << bLoop;
}

void USubUVComponent::PostDuplicate()
{
    UBillboardComponent::PostDuplicate();
    // SpriteVfx 리소스를 다시 바인딩합니다.
    SetSpriteVfx(SpriteVfxName);
}

USubUVComponent::USubUVComponent()
{
    SetVisibility(true);
}

void USubUVComponent::SetSpriteVfx(const FName& InSpriteVfxName)
{
    SpriteVfxName = InSpriteVfxName;
    CachedSpriteVfx = FSpriteVfxManager::Get().FindSpriteVfx(InSpriteVfxName);
    MarkWorldBoundsDirty();
}

void USubUVComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    /*
        SubUV는 빌보드의 texture 속성을 노출하지 않습니다.
        primitive 공통 속성과 SubUV 전용 속만 에디터에 등록합니다.
    */
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "SpriteVfx", EPropertyType::Name, &SpriteVfxName });
    OutProps.push_back({ "Width", EPropertyType::Float, &Width, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
    OutProps.push_back({ "Play Rate", EPropertyType::Float, &PlayRate, 1.0f, 120.0f, 1.0f });
    OutProps.push_back({ "bLoop", EPropertyType::Bool, &bLoop });
}

void USubUVComponent::PostEditProperty(const char* PropertyName)
{
    /*
        빌보드 texture 처리 경로를 건너뛰기 위해 primitive 편집 처리를 직접 호출합니다.
    */
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "SpriteVfx") == 0)
    {
        SetSpriteVfx(SpriteVfxName);
        // SpriteVfx 교체는 UV 그리드와 texture를 바꾸므로 메시 단계까지 dirty 처리합니다.
        MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
    }
    else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Transform);
        MarkWorldBoundsDirty();
    }
}

void USubUVComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    // 부모(BillboardComponent)의 Tick 호출을 제거합니다. 
    // 부모는 매 프레임 행렬을 직접 업데이트하려 시도하는데, SubUV는 프록시에서 이를 처리하므로 충돌 및 스케일 중복 적용을 유발합니다.
    UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSpriteVfx)
        return;
    if (!bLoop && bIsExecute)
        return;

    const uint32 TotalFrames = CachedSpriteVfx->Columns * CachedSpriteVfx->Rows;
    if (TotalFrames == 0)
        return;

    TimeAccumulator += DeltaTime;
    const float FrameDuration = 1.0f / PlayRate;
    const uint32 OldFrameIndex = FrameIndex;

    while (TimeAccumulator >= FrameDuration)
    {
        TimeAccumulator -= FrameDuration;

        if (bLoop)
        {
            bIsExecute = false;
            FrameIndex = (FrameIndex + 1) % TotalFrames; // 臾댄븳 諛섎났
        }
        else
        {
            if (FrameIndex < TotalFrames - 1)
            {
                FrameIndex++;
            }
            else
            {
                bIsExecute = true; // 마지막 프레임 도달 후 완료
                TimeAccumulator = 0.0f;
                break;
            }
        }
    }

    if (OldFrameIndex != FrameIndex)
    {
        MarkProxyDirty(ESceneProxyDirtyFlag::Mesh);
    }
}

