#include "SpotLightComponent.h"
#include "SimpleJSON/json.hpp"

DEFINE_CLASS(USpotlightComponent, ULightComponent)
REGISTER_FACTORY(USpotlightComponent)

USpotlightComponent::USpotlightComponent() {
	UpdateLight();
}

void USpotlightComponent::UpdateWorldMatrix() {
	if (bUpdateFlag == false)
	{
		return;
	}

	FMatrix relativeMatrix = GetRelativeMatrixTemp();

	if (ParentComponent != nullptr)
	{
		CachedWorldMatrix = relativeMatrix * ParentComponent->GetWorldMatrix();
	}
	else
	{
		CachedWorldMatrix = relativeMatrix;
	}

	bUpdateFlag = false;
	bIsLightDirty = true;
}

void USpotlightComponent::UpdateLight() {
	CircleVertices.clear();
	CircleVertices.reserve(NumCircleVertex);

	// Base vectors
	FVector BaseNormal = GetUpVector().Normalized();
	FVector BaseRight = GetRightVector().Normalized();
	FVector BaseForward = GetForwardVector().Normalized();

	// Apply pitch around the local right axis, then yaw around the local up axis
	FMatrix PitchMat = FMatrix::MakeRotationAxis(BaseRight, Pitch);
	FMatrix YawMat = FMatrix::MakeRotationAxis(BaseForward, Yaw);
	FMatrix RotMat = PitchMat * YawMat;

	UnitNormal = (RotMat.TransformVector(BaseNormal)).Normalized();
	CircleCenter = GetWorldLocation() + UnitNormal * ConeHeight;

	// Recompute U and V perpendicular to the rotated axis
	FVector U = (RotMat.TransformVector(BaseForward)).Normalized();
	FVector V = UnitNormal.Cross(U).Normalized();

	UnitOrthogonal = U;

	float Angle = 2.0f * M_PI / NumCircleVertex;
	for (size_t i = 0; i < NumCircleVertex; i++) {
		FVector Point = CircleCenter + (U * cosf(Angle * i) + V * sinf(Angle * i)) * Radius;
		CircleVertices.push_back(Point);
	}
}

void USpotlightComponent::RenderLines(FBatchedLine* BatchLine) {
	if (!BatchLine || CircleVertices.empty()) {
		return;
	}

	FVector ConeApex = GetWorldLocation();
	for (size_t i = 0; i < NumCircleVertex; i++) {
		// Point to Apex 
		BatchLine->AddLineRaw(CircleVertices[i], ConeApex, Color);

		// Point to next Point
		BatchLine->AddLineRaw(CircleVertices[i], CircleVertices[(i + 1) % NumCircleVertex], Color);
	}
}

void USpotlightComponent::TickComponent(float DeltaTIme) {
	if (bIsLightDirty) {
		UpdateLight();
		bIsLightDirty = false;
	}
}

void USpotlightComponent::Serialize(json::JSON& j) const {
	ULightComponent::Serialize(j);
	j["ConeHeight"]      = ConeHeight;
	j["Radius"]          = Radius;
	j["Yaw"]             = Yaw;
	j["Pitch"]           = Pitch;
	j["NumCircleVertex"] = NumCircleVertex;
}

void USpotlightComponent::Deserialize(const json::JSON& j) {
	ULightComponent::Deserialize(j);
	auto& jj = const_cast<json::JSON&>(j);
	SetConeHeight(jj["ConeHeight"].ToFloat());
	SetConeRadius(jj["Radius"].ToFloat());
	SetYaw(jj["Yaw"].ToFloat());
	SetPitch(jj["Pitch"].ToFloat());
	SetNumVertex(jj["NumCircleVertex"].ToInt());
}
