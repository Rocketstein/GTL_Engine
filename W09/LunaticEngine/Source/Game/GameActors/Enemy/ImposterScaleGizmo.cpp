#include "ImposterScaleGizmo.h"
#include "Game/GameActors/Obstacle/MustSlideObstacleActor.h"

#include <algorithm>
#include <random>

IMPLEMENT_CLASS(AImposterScaleGizmo, AImposterGizmoActorBase)

namespace {
	constexpr float LerpDuration = 0.5f;

	std::mt19937& RandomEngine()
	{
		static std::mt19937 Engine(std::random_device{}());
		return Engine;
	}

	float RandomScaleFactor()
	{
		std::uniform_real_distribution<float> Distribution(2.f, 4.0f);
		return Distribution(RandomEngine());
	}
}

void AImposterScaleGizmo::Capture(AActor* InTarget) {
	if (!InTarget || !InTarget->IsA<AObstacleActorBase>()) return;
	AImposterGizmoActorBase::Capture(InTarget);
	if (!HasAliveTarget() || !PreviewGizmo) return;
	PreviewGizmo->SetScaleMode();
	PreviewGizmo->SetTarget(Target);
	PreviewGizmo->SetSelectedAxis(SetOffsetAxis());
}

void AImposterScaleGizmo::Transform(float DeltaTime) {
	if (!HasAliveTarget())
	{
		Release();
		return;
	}

	if (!bTransforming)
	{
		StartScale = Target->GetActorScale();
		const FVector ScaleOffset = GetScaleOffset();
		TargetScale = FVector(
			StartScale.X * ScaleOffset.X,
			StartScale.Y * ScaleOffset.Y,
			StartScale.Z * ScaleOffset.Z);
		Elapsed = 0.0f;
		bTransforming = true;

		if (PreviewGizmo)
		{
			PreviewGizmo->SetHolding(true);
		}
	}

	Elapsed += (std::max)(0.0f, DeltaTime);
	const float Alpha = (std::min)(Elapsed / LerpDuration, 1.0f);
	Target->SetActorScale(FVector::Lerp(StartScale, TargetScale, Alpha));

	if (Alpha >= 1.0f)
	{
		bTransforming = false;
		Release();
	}
}

// Return a scale within the range [1.2, 2]
FVector AImposterScaleGizmo::GetScaleOffset() {

	switch(OffsetAxis) {
	case (0) :
	{
		return FVector(RandomScaleFactor(), 1, 1);
		break;
	}
	case (1):
		return FVector(1, RandomScaleFactor(), 1);
		break;
	default:
		float ScaleFactor = RandomScaleFactor();
		if (Target && Target->IsA<AMustSlideObstacleActor>())
				ScaleFactor = ScaleFactor > 1.2f ? 1.2f : ScaleFactor;
		return FVector(1, 1, ScaleFactor);
		break;
	}

	//const float ScaleFactor = RandomScaleFactor();
	//return FVector(ScaleFactor, ScaleFactor, ScaleFactor);
}
