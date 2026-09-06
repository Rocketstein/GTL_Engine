#pragma once
#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Reflection/UStruct.h"

#include "Source/Engine/Component/Camera/FPostProcessSettings.generated.h"

USTRUCT()
struct FPostProcessSettings
{
	GENERATED_BODY()

	// DOF 전용 설정
	UPROPERTY(Edit, Save, Category="Depth of Field", DisplayName="Aperture", Min=0.0f, Speed=0.01f)
	float Aperture = 0.5f;			// 0 = pass disabled
	UPROPERTY(Edit, Save, Category="Depth of Field", DisplayName="Focus Distance", Min=0.0f, Speed=0.1f)
	float FocusDistance = 20.0f;	// focus plane distance
	UPROPERTY(Edit, Save, Category="Depth of Field", DisplayName="Focal Length", Min=0.0f, Speed=0.1f)
	float FocalLength = 15.0f;		// <= FocusDistance
	UPROPERTY(Edit, Save, Category="Depth of Field", DisplayName="DOF Samples", Min=1, Speed=1.0f)
	int	  DOFSample = 32;			// Number of blur taps
};
