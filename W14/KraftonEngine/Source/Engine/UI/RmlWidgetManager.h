#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

class URmlWidgetAsset;

class FRmlWidgetManager : public TSingleton<FRmlWidgetManager>, public FGCObject
{
	friend class TSingleton<FRmlWidgetManager>;

public:
	URmlWidgetAsset* Load(const FString& Path);
	URmlWidgetAsset* Find(const FString& Path) const;
	void ClearCache();

	const char* GetReferencerName() const override { return "FRmlWidgetManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	TMap<FString, URmlWidgetAsset*> LoadedWidgets;
};
