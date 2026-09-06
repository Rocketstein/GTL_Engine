#pragma once

#include "Object/Object.h"

#include "Source/Engine/UI/RmlWidgetAsset.generated.h"

UCLASS()
class URmlWidgetAsset : public UObject
{
public:
	GENERATED_BODY()

	void SetDocumentPath(const FString& InPath) { DocumentPath = InPath; }
	const FString& GetDocumentPath() const { return DocumentPath; }

	void SetStyleSheetPath(const FString& InPath) { StyleSheetPath = InPath; }
	const FString& GetStyleSheetPath() const { return StyleSheetPath; }

	void SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const { return SourcePath; }

private:
	FString SourcePath;
	FString DocumentPath;
	FString StyleSheetPath;
};
