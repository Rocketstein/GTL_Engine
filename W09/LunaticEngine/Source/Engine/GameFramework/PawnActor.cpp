#include "PawnActor.h"

#include "Component/BillboardComponent.h"
#include "Component/SceneComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(APawnActor, AActor)

APawnActor::APawnActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void APawnActor::BeginPlay()
{
	Super::BeginPlay();
	// GameMode가 런타임에 스폰한 Pawn은 InitDefaultComponents가 안 돈 상태로 들어옴 (에디터 전용 호출).
	// 최소 루트 SceneComponent라도 만들어두지 않으면 후속 트랜스폼 호출이 nullptr을 deref.
	// 빌보드는 에디터 전용이라 게임 빌드에선 만들지 않는다.
	if (!GetRootComponent() && GetComponents().empty())
	{
		USceneComponent* Root = AddComponent<USceneComponent>();
		Root->SetCanDeleteFromDetails(false);
		SetRootComponent(Root);
	}
}

void APawnActor::InitDefaultComponents()
{
	RootSceneComponent = AddComponent<USceneComponent>();
	RootSceneComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(RootSceneComponent);

	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetCanDeleteFromDetails(false);
	BillboardComponent->AttachToComponent(RootSceneComponent);
	BillboardComponent->SetAbsoluteScale(true);
	BillboardComponent->SetEditorOnlyComponent(true);
	BillboardComponent->SetHiddenInComponentTree(true);
	BillboardComponent->SetSpriteSize(1.0f, 1.0f);

	const FString IconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Pawn"));
	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (UTexture2D* Texture = UTexture2D::LoadFromFile(IconPath, Device))
	{
		BillboardComponent->SetTexture(Texture);
	}
}
