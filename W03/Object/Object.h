#pragma once

#include "EngineStatics.h"
#include "Core/Name.h"

#define DECLARE_CLASS(ClassName, ParentClass)                          \
    static const FTypeInfo s_TypeInfo;                                 \
    const FTypeInfo* GetTypeInfo() const override {                    \
        return &s_TypeInfo;                                            \
    }                                                                  \
    static ClassName* Cast(UObject* Obj) {                             \
        return Obj ? Obj->Cast<ClassName>() : nullptr;                 \
    }

#define DEFINE_CLASS(ClassName, ParentClass)                           \
    const FTypeInfo ClassName::s_TypeInfo = {                          \
        #ClassName,                                                    \
        &ParentClass::s_TypeInfo,                                      \
        sizeof(ClassName)                                              \
    };

enum EClassFlags : uint32_t {
	CF_None		 = 0,
	CF_Actor	 = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera	 = 1 << 2,
};

struct FTypeInfo {
	const char* name;
	const FTypeInfo* Parent;
	size_t size;
	uint32_t ClassFlags = CF_None;

	bool IsA(const FTypeInfo* Other) const {
		for (const FTypeInfo* T = this; T; T = T->Parent) {
			if (T == Other) {
				return true;
			}
		}
		return false;
	}
};

namespace json { class JSON; }

class UObject {
public:
	uint32 UUID;
	uint32 InternalIndex;
	uint32 AllocationSize;
	FName Name;
	bool bPendingKill;

	UObject();
	virtual ~UObject();

	static void* operator new(size_t Size)
	{
		void* Ptr = std::malloc(Size);
		if (Ptr)
		{
			EngineStatics::OnAllocated(static_cast<uint32>(Size));
		}
		return Ptr;
	}

	static void operator delete(void* Ptr, size_t Size)
	{
		if (Ptr)
		{
			EngineStatics::OnDeallocated(static_cast<uint32>(Size));
			std::free(Ptr);
;		}
	}

	void SetNewName(FString NewName) { Name = FName(NewName); };
	void SetNewName(const char* NewName) { Name = FName(NewName); };
	void SerializeHeader(json::JSON& j) const;
	void DeserializeHeader(const json::JSON& j);
	virtual void Serialize(json::JSON& j) const {}
	virtual void Deserialize(const json::JSON& j) {}

	// RTTI stuffs
	virtual const FTypeInfo* GetTypeInfo() const { return &s_TypeInfo; }

	template<typename T>
	bool IsA() const { return GetTypeInfo()->IsA(&T::s_TypeInfo); }

	template<typename T>
	T* Cast() { return IsA<T>() ? static_cast<T*>(this) : nullptr; }

	template<typename T>
	const T* Cast() const { return IsA<T>() ? static_cast<const T*>(this) : nullptr; }

	static const FTypeInfo s_TypeInfo;
};

extern TArray<UObject*> GUObjectArray;


class FObjectManager {
public:
	// Singleton
	static FObjectManager& Get()
	{
		static FObjectManager instance;
		return instance;
	}

	template<typename T>
	T* CreateObject() {
		T* Obj = new T();
		Obj->Name = FName(FString(Obj->GetTypeInfo()->name) + "_" + std::to_string(Obj->UUID));
		Obj->InternalIndex = static_cast<uint32>(GUObjectArray.size());
		Obj->AllocationSize = static_cast<uint32>(sizeof(T));
		GUObjectArray.push_back(Obj);
		return Obj;
	}

	// Does not destroy the target object right away. Only marks for death
	void DestroyObject(UObject* Obj) {
		if (Obj) {
			Obj->bPendingKill = true;
			if (NumPendingToKill++ > GC_Threshold) {
				//CollectGarbage();
			}
		}
	}

	//void CollectGarbage() {
	//	if (bIsCollectingGarbage) return;
	//	bIsCollectingGarbage = true;
	//	for (int32 i = (int32)GUObjectArray.size() - 1; i >= 0; i--) {
	//		if (!GUObjectArray[i] || GUObjectArray[i]->bPendingKill) {
	//			delete GUObjectArray[i];
	//			GUObjectArray.erase(GUObjectArray.begin() + i);
	//		}
	//	}
	//	NumPendingToKill = 0;
	//	bIsCollectingGarbage = false;
	//}

	void ForceFlush() {
		GUObjectArray.clear();  // shutdown only
		NumPendingToKill = 0;
	}

	//UObject* FindByUUID(uint32 UUID)
	//{
	//	for (auto* Obj : GUObjectArray)
	//		if (Obj && Obj->UUID == UUID)
	//			return Obj;
	//	return nullptr;
	//}

	//UObject* FindByIndex(uint32 Index)
	//{
	//	if (Index >= GUObjectArray.size()) return nullptr;
	//	return GUObjectArray[Index];   // may be null if destroyed
	//}


private:
	FObjectManager() = default;
	~FObjectManager() { /*CollectGarbage(); */}

	uint32 NumPendingToKill = 0;
	const uint32 GC_Threshold = 512;
	bool bIsCollectingGarbage = false;
};
