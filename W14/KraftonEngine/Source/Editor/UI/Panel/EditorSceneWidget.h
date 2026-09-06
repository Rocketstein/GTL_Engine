#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Types/CoreTypes.h"
#include "Object/Ptr/WeakObjectPtr.h"

class AActor;

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	void RenderActorOutliner();
	void HandleSceneManagerShortcuts();
	void BeginRenameActor(AActor* TargetActor);
	void RenderRenamePopup();
	bool TryRenameActor(AActor* TargetActor, const FString& NewName);

	TArray<int32> ValidActorIndices;
	TWeakObjectPtr<AActor> RenameTargetActor = nullptr;
	bool bRenamePopupRequested = false;
	bool bFocusRenameInputNextFrame = false;
	char RenameBuffer[256] = {};
	FString RenameErrorText;
};
