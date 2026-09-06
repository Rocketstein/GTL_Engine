#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"

namespace ParticleSerialization
{
	template<typename ObjectT>
	void DestroyObjectTree(ObjectT*& Object)
	{
		if (Object)
		{
			UObjectManager::Get().DestroyObject(Object);
			Object = nullptr;
		}
	}

	template<typename ObjectT>
	void DestroyObjectArray(TArray<ObjectT*>& Objects)
	{
		for (ObjectT*& Object : Objects)
		{
			DestroyObjectTree(Object);
		}
		Objects.clear();
	}

	// 저장 가능한 파티클 에셋 객체 트리를 어떻게 파일에서 복원할 것인지
	template<typename ObjectT>
	void SerializeInstancedObject(FArchive& Ar, ObjectT*& Object, UObject* Outer)
	{
		FString ClassName = "None";
		if (Ar.IsSaving())
		{
			ClassName = Object ? FString(Object->GetClass()->GetName()) : FString("None");
		}

		Ar << ClassName;

		if (Ar.IsLoading())
		{
			DestroyObjectTree(Object);

			if (ClassName.empty() || ClassName == "None")
			{
				return;
			}

			UObject* NewObject = FObjectFactory::Get().Create(ClassName, Outer);
			if (!NewObject || !NewObject->IsA<ObjectT>())
			{
				UObjectManager::Get().DestroyObject(NewObject);
				return;
			}

			Object = static_cast<ObjectT*>(NewObject);
		}

		if (Object)
		{
			Object->Serialize(Ar);
		}
	}

	template<typename ObjectT>
	void SerializeInstancedObjectArray(FArchive& Ar, TArray<ObjectT*>& Objects, UObject* Outer)
	{
		uint32 Num = static_cast<uint32>(Objects.size());
		Ar << Num;

		if (Ar.IsLoading())
		{
			DestroyObjectArray(Objects);
			Objects.reserve(Num);
			for (uint32 Index = 0; Index < Num; ++Index)
			{
				ObjectT* Object = nullptr;
				SerializeInstancedObject(Ar, Object, Outer);
				Objects.push_back(Object);
			}
			return;
		}

		for (ObjectT*& Object : Objects)
		{
			SerializeInstancedObject(Ar, Object, Outer);
		}
	}
}
