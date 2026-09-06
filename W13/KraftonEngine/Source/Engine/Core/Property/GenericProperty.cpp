#include "GenericProperty.h"

#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Math/Transform.h"
#include "Serialization/Archive.h"

void FGenericProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::ByteBool:
	{
		bool bValue = Ar.IsSaving() ? (*static_cast<uint8*>(ValuePtr) != 0) : false;
		Ar << bValue;
		if (Ar.IsLoading())
		{
			*static_cast<uint8*>(ValuePtr) = bValue ? 1 : 0;
		}
		break;
	}
	case EPropertyType::Vec3:
		Ar << *static_cast<FVector*>(ValuePtr);
		break;
	case EPropertyType::Rotator:
		Ar << *static_cast<FRotator*>(ValuePtr);
		break;
	case EPropertyType::Transform:
		// FTransform 은 trivially-copyable — FArchive 템플릿 operator<< 로 직렬화.
		Ar << *static_cast<FTransform*>(ValuePtr);
		break;
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
		Ar << *static_cast<FVector4*>(ValuePtr);
		break;
	default:
		break;
	}
}
