#include "LightComponentBase.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Component/BillboardComponent.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(ULightComponentBase, USceneComponent)
HIDE_FROM_COMPONENT_LIST(ULightComponentBase)

void ULightComponentBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Intensity",EPropertyType::Float,&Intensity,0.0f,50.f,0.05f });
	OutProps.push_back({ "Color",EPropertyType::Color4,&LightColor });
	OutProps.push_back({ "Visible",EPropertyType::Bool,&bVisible });
	OutProps.push_back({ "Cast Shadows",EPropertyType::Bool,&bCastShadows });
}

void ULightComponentBase::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << Intensity;
	Ar << LightColor;
	Ar << bVisible;
	Ar << bCastShadows;
}

UBillboardComponent* ULightComponentBase::EnsureEditorBillboard()
{
	if (!Owner)
	{
		return nullptr;
	}

	const char* IconTextureKey = nullptr;
	switch (GetLightType())
	{
	case ELightComponentType::Ambient:
		IconTextureKey = "Editor.Billboard.AmbientLight";
		break;
	case ELightComponentType::Directional:
		IconTextureKey = "Editor.Billboard.DirectionalLight";
		break;
	case ELightComponentType::Point:
		IconTextureKey = "Editor.Billboard.PointLight";
		break;
	case ELightComponentType::Spot:
		IconTextureKey = "Editor.Billboard.SpotLight";
		break;
	}

	if (!IconTextureKey)
	{
		return nullptr;
	}

	for (USceneComponent* Child : GetChildren())
	{
		UBillboardComponent* Billboard = Cast<UBillboardComponent>(Child);
		if (Billboard && Billboard->IsEditorOnlyComponent())
		{
			// ?먮뵒???꾩씠肄?鍮뚮낫?쒕뒗 遺紐??ㅼ??쇨낵 而댄룷?뚰듃 ?몃━ 湲곕낯 ?쒖떆?먯꽌 遺꾨━?쒕떎.
			Billboard->SetAbsoluteScale(true);
			Billboard->SetHiddenInComponentTree(true);
			return Billboard;
		}
	}

	UBillboardComponent* Billboard = Owner->AddComponent<UBillboardComponent>();
	if (Billboard)
	{
		Billboard->AttachToComponent(this);
		// ?먮뵒???꾩씠肄?鍮뚮낫?쒕뒗 遺紐??ㅼ??쇨낵 而댄룷?뚰듃 ?몃━ 湲곕낯 ?쒖떆?먯꽌 遺꾨━?쒕떎.
		Billboard->SetAbsoluteScale(true);
		Billboard->SetEditorOnlyComponent(true);
		Billboard->SetHiddenInComponentTree(true);
		ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
		const FString ResolvedIconTexturePath = FResourceManager::Get().ResolvePath(FName(IconTextureKey));
		if (UTexture2D* Texture = UTexture2D::LoadFromFile(ResolvedIconTexturePath, Device))
		{
			Billboard->SetTexture(Texture);
		}
	}

	return Billboard;
}
