#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"
#include "Asset/AssetRegistry.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"

class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

private:
	void RenameActor(AActor* PrimaryActor);
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	// 위젯 렌더링은 공용 FPropertyTable 로 위임(forwarder). 액터 패널 고유의 회전 캐시 적용만
	// 컨텍스트 콜백으로 주입한다(SoftObject 에셋 피커 포함 나머지는 FPropertyTable 내장).
	bool RenderPropertyWidget(TArray<struct FPropertyValue>& Props, int32& Index, bool bDispatchChange = true, const FString& PropertyPath = {});

	void PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors);

	void AddComponentToActor(AActor* Actor, UClass* ComponentClass);

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true; // true: Actor details, false: Component details
	bool bShowEditorOnlyComponents = false;

	char RenameBuffer[256] = {};
	bool bShowDuplicateWarning = false;
};
