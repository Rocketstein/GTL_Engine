#include "Object.h"
#include "EngineStatics.h"
#include "SimpleJSON/json.hpp"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = EngineStatics::GenUUID();
	bPendingKill = false;
	InternalIndex = 0;   // Set by UObjectManager::CreateObject
	AllocationSize = 0;  // Set by UObjectManager::CreateObject
}

UObject::~UObject()
{
	EngineStatics::OnDeallocated(AllocationSize);
	//uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	//if (InternalIndex != LastIndex)
	//{
	//	UObject* LastObject = GUObjectArray[LastIndex];

	//	GUObjectArray[InternalIndex] = LastObject;

	//	LastObject->InternalIndex = this->InternalIndex;
	//}

	//GUObjectArray.pop_back();

	//EngineStatics::OnDeallocated(sizeof(UObject));
}

void UObject::SerializeHeader(json::JSON& j) const {
	j["ClassName"] = GetTypeInfo()->name;
	j["FName"] = Name.GetFString();
	j["UUID"] = UUID;
	j["InternalIndex"] = InternalIndex;
}

void UObject::DeserializeHeader(const json::JSON& j) {

}

const FTypeInfo UObject::s_TypeInfo = { "UObject", nullptr, sizeof(UObject) };

//#include "Engine/World.h"
//void UObjectManager::PurgeScene() {
//	for (UObject* Obj : GUObjectArray) {
//		if (Obj->IsA<UWorld>()) {
//			UWorld* World = Obj->Cast<UWorld>();
//				World->EndPlay();
//		}
//	}
//
//	CollectGarbage();
//	GUObjectArray.clear();
//}
