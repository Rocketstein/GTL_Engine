#pragma once

#include "Object/Object.h"
#include "Core/Logging/Log.h"
#include "Object/GarbageCollection.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "Source/Engine/UI/UserWidget.generated.h"
#include <sol/sol.hpp>
#include <utility>

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

class APlayerController;
class FWidgetClickEventListener;
namespace Rml { class ElementDocument; }

class FWidgetClickEventListener final : public Rml::EventListener
{
public:
	FWidgetClickEventListener(FString InElementId, sol::protected_function InCallback)
		: ElementId(std::move(InElementId))
		, Callback(std::move(InCallback))
	{
	}

	void ProcessEvent(Rml::Event& /*Event*/) override
	{
		if (!Callback.valid())
		{
			return;
		}

		FScopedGarbageCollectionBlocker GCBlocker;
		sol::protected_function_result Result = Callback();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua] UI click callback error: %s", Err.what());
		}
	}

	const FString& GetElementId() const { return ElementId; }

private:
	FString ElementId;
	sol::protected_function Callback;
};

UCLASS()
class UUserWidget : public UObject
{
public:
	GENERATED_BODY()
	UUserWidget() = default;
	~UUserWidget() override = default;
	void BeginDestroy() override;

	void Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath);
	UFUNCTION(Callable, Category="UI")
	void AddToViewport(int32 InZOrder = 0);
	UFUNCTION(Callable, Category="UI")
	void RemoveFromParent();
	UFUNCTION(Callable, Category="UI")
	bool ReloadDocument();
	UFUNCTION(Callable, Category="UI")
	bool ReloadStyleSheet();
	void BindClick(const FString& ElementId, sol::protected_function Callback);
	void RegisterEventListeners();
	void ClearEventListeners();
	UFUNCTION(Callable, Category="UI")
	void SetText(const FString& ElementId, const FString& Text);
	UFUNCTION(Callable, Category="UI")
	bool SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value);

	UFUNCTION(Pure, Category="UI")
	APlayerController* GetOwningPlayer() const { return OwningPlayer; }
	UFUNCTION(Pure, Category="UI")
	const FString& GetDocumentPath() const { return DocumentPath; }
	UFUNCTION(Pure, Category="UI")
	int32 GetZOrder() const { return ZOrder; }
	UFUNCTION(Pure, Category="UI")
	bool IsInViewport() const { return bInViewport; }
	UFUNCTION(Pure, Category="UI")
	bool IsDocumentLoaded() const { return bDocumentLoaded; }
	Rml::ElementDocument* GetDocument() const { return Document; }

	// 메뉴/대화창처럼 사용자가 클릭/포인팅을 해야 하는 widget 은 true 로 설정.
	// UUIManager 가 viewport 에 올라온 widget 중 하나라도 이 값이 true 면 GameViewportClient
	// 에 알려 시스템 커서를 보이고 raw mouse / clip 을 해제하도록 한다. HUD 처럼 비대화형
	// 오버레이는 false 유지.
	UFUNCTION(Callable, Category="UI")
	void SetWantsMouse(bool bInWantsMouse) { bWantsMouse = bInWantsMouse; }
	UFUNCTION(Pure, Category="UI")
	bool WantsMouse() const { return bWantsMouse; }

	UFUNCTION(Callable, Category="UI")
	void SetWantsKeyboard(bool bInWantsKeyboard) { bWantsKeyboard = bInWantsKeyboard; }
	UFUNCTION(Pure, Category="UI")
	bool WantsKeyboard() const { return bWantsKeyboard; }

	UFUNCTION(Callable, Category="UI")
	void SetWantsTextInput(bool bInWantsTextInput) { bWantsTextInput = bInWantsTextInput; }
	UFUNCTION(Pure, Category="UI")
	bool WantsTextInput() const { return bWantsTextInput; }

	UFUNCTION(Callable, Category="UI")
	void SetBlocksGameInput(bool bInBlocksGameInput) { bBlocksGameInput = bInBlocksGameInput; }
	UFUNCTION(Pure, Category="UI")
	bool BlocksGameInput() const { return bBlocksGameInput; }

	UFUNCTION(Callable, Category="UI")
	void SetBlocksGameKeyboard(bool bInBlocksGameKeyboard) { bBlocksGameKeyboard = bInBlocksGameKeyboard; }
	UFUNCTION(Pure, Category="UI")
	bool BlocksGameKeyboard() const { return bBlocksGameKeyboard; }

	UFUNCTION(Callable, Category="UI")
	void SetBlocksGameMouseLook(bool bInBlocksGameMouseLook) { bBlocksGameMouseLook = bInBlocksGameMouseLook; }
	UFUNCTION(Pure, Category="UI")
	bool BlocksGameMouseLook() const { return bBlocksGameMouseLook; }

	void MarkDocumentLoaded(Rml::ElementDocument* InDocument) { Document = InDocument; bDocumentLoaded = Document != nullptr; }
	void MarkRemovedFromViewport() { bInViewport = false; }
	void ClearDocument() { Document = nullptr; bDocumentLoaded = false; }

private:
	TWeakObjectPtr<APlayerController> OwningPlayer;
	Rml::ElementDocument* Document = nullptr;
	FString DocumentPath;
	TArray<std::pair<FString, sol::protected_function>> PendingClickBindings;
	TArray<FWidgetClickEventListener*> ClickListeners;
	int32 ZOrder = 0;
	bool bInViewport = false;
	bool bDocumentLoaded = false;
	bool bWantsMouse = false;
	bool bWantsKeyboard = false;
	bool bWantsTextInput = false;
	bool bBlocksGameInput = false;
	bool bBlocksGameKeyboard = false;
	bool bBlocksGameMouseLook = false;
};
