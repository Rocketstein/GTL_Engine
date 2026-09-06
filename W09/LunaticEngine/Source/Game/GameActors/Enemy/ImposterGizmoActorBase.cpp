#include "ImposterGizmoActorBase.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Render/Scene/FScene.h"

#include <algorithm>
#include <random>

DEFINE_CLASS(AImposterGizmoActorBase, AGimmickActorBase)

namespace {
	std::mt19937& RandomEngine()
	{
		static std::mt19937 Engine(std::random_device{}());
		return Engine;
	}
}

void AImposterGizmoActorBase::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	if (!Target) return;
	if (!HasAliveTarget())
	{
		Release();
		return;
	}

	if (ElapsedDelay < ActivationDelay) ElapsedDelay += DeltaTime;
	if (ElapsedDelay >= ActivationDelay) Transform(DeltaTime);
	if (PreviewGizmo && HasAliveTarget())
	{
		PreviewGizmo->UpdateGizmoTransform();
	}
}

void AImposterGizmoActorBase::Capture(AActor* InActor) {
	if (!InActor) return;
	if (!IsAliveObject(InActor)) return;
	if (Target)
	{
		ReleaseCapturedActorTint();
	}
	Target = InActor;
	ElapsedDelay = 0.0f;
	Elapsed = 0.0f;
	bTransforming = false;
	ApplyCapturedActorTint();

	if (!PreviewGizmo)
	{
		PreviewGizmo = AddComponent<UGizmoComponent>();
	}
}

bool AImposterGizmoActorBase::HasAliveTarget() const
{
	return Target && IsAliveObject(Target);
}

uint8 AImposterGizmoActorBase::SetOffsetAxis() {
	std::uniform_int_distribution<int> Distribution(0, 2);
	OffsetAxis = Distribution(RandomEngine());
	return OffsetAxis;
}

FLuaActorProxy AImposterGizmoActorBase::GetCapturedActorProxy() const {
	FLuaActorProxy Proxy;
	Proxy.Actor = HasAliveTarget() ? Target : nullptr;
	return Proxy;
}

AActor* AImposterGizmoActorBase::GetCapturedActor() const
{
	return HasAliveTarget() ? Target : nullptr;
}

void AImposterGizmoActorBase::Release() {
	if (PreviewGizmo && IsAliveObject(PreviewGizmo))
	{
		PreviewGizmo->SetSelectedAxis(-1);
	}
	ReleaseCapturedActorTint();
	Target = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->DestroyActor(this);
	}
}

void AImposterGizmoActorBase::ApplyCapturedActorTint()
{
	if (!HasAliveTarget())
	{
		return;
	}

	UWorld* World = Target->GetWorld();
	if (!World)
	{
		return;
	}

	FScene& Scene = World->GetScene();
	bCapturedActorWasSelected = Target->IsActorSelected();
	Target->SetActorSelected(true);

	for (UPrimitiveComponent* Primitive : Target->GetPrimitiveComponents())
	{
		if (!Primitive)
		{
			continue;
		}

		FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy();
		const bool bWasSelected = Scene.IsProxySelected(Proxy);
		CapturedTintedComponents.push_back(Primitive);
		CapturedTintedComponentWasSelected.push_back(bWasSelected);
		if (Proxy)
		{
			Scene.SetProxySelected(Proxy, true);
		}
	}
}

void AImposterGizmoActorBase::ReleaseCapturedActorTint()
{
	if (Target && IsAliveObject(Target))
	{
		if (UWorld* World = Target->GetWorld())
		{
			FScene& Scene = World->GetScene();
			for (int32 Index = 0; Index < static_cast<int32>(CapturedTintedComponents.size()); ++Index)
			{
				UPrimitiveComponent* Primitive = CapturedTintedComponents[Index];
				if (!Primitive || !IsAliveObject(Primitive))
				{
					continue;
				}

				if (FPrimitiveSceneProxy* Proxy = Primitive->GetSceneProxy())
				{
					const bool bWasSelected = Index < static_cast<int32>(CapturedTintedComponentWasSelected.size())
						? CapturedTintedComponentWasSelected[Index]
						: false;
					Scene.SetProxySelected(Proxy, bWasSelected);
				}
			}
		}

		Target->SetActorSelected(bCapturedActorWasSelected);
	}

	CapturedTintedComponents.clear();
	CapturedTintedComponentWasSelected.clear();
	bCapturedActorWasSelected = false;
}
