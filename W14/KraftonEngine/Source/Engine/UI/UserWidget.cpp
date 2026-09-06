#include "UI/UserWidget.h"

#include "Object/Reflection/ObjectFactory.h"
#include "UI/UIManager.h"


void UUserWidget::BeginDestroy()
{
	// Rml::ElementDocument and Rml event listeners are external runtime resources,
	// not UObject references. GC can destroy a widget without going through the
	// regular UIManager shutdown path, so detach listeners and release the document
	// handle before the UObject enters PendingKill/Garbage state.
	ClearEventListeners();

	// Clear saved Lua callbacks before the Lua VM can shut down; otherwise final
	// widget GC may crash in sol::reference::deref while destroying these refs.
	PendingClickBindings.clear();
	if (Document)
	{
		Document->Close();
		ClearDocument();
	}
	bInViewport = false;
	UObject::BeginDestroy();
}


void UUserWidget::Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath)
{
	OwningPlayer = InOwningPlayer;
	DocumentPath = InDocumentPath;
}

void UUserWidget::AddToViewport(int32 InZOrder)
{
	ZOrder = InZOrder;
	bInViewport = true;
	UUIManager::Get().AddToViewport(this, InZOrder);
}

void UUserWidget::RemoveFromParent()
{
	UUIManager::Get().RemoveFromViewport(this);
	bInViewport = false;
}

bool UUserWidget::ReloadDocument()
{
	return UUIManager::Get().ReloadDocument(this);
}

bool UUserWidget::ReloadStyleSheet()
{
	return UUIManager::Get().ReloadStyleSheet(this);
}

void UUserWidget::BindClick(const FString& ElementId, sol::protected_function Callback)
{
	PendingClickBindings.push_back({ ElementId, Callback });
	if (IsDocumentLoaded())
	{
		RegisterEventListeners();
	}
}

void UUserWidget::RegisterEventListeners()
{
	if (!Document)
	{
		return;
	}

	ClearEventListeners();

	for (const auto& Binding : PendingClickBindings)
	{
		Rml::Element* Element = Document->GetElementById(Binding.first);
		if (!Element)
		{
			UE_LOG("[RmlUi] Click target not found: %s", Binding.first.c_str());
			continue;
		}

		auto* Listener = new FWidgetClickEventListener(Binding.first, Binding.second);
		Element->AddEventListener("click", Listener);
		ClickListeners.push_back(Listener);
	}
}

void UUserWidget::ClearEventListeners()
{
	if (Document)
	{
		for (FWidgetClickEventListener* Listener : ClickListeners)
		{
			if (!Listener)
			{
				continue;
			}

			Rml::Element* Element = Document->GetElementById(Listener->GetElementId());
			if (Element)
			{
				Element->RemoveEventListener("click", Listener);
			}
		}
	}

	for (FWidgetClickEventListener* Listener : ClickListeners)
	{
		delete Listener;
	}
	ClickListeners.clear();
}

void UUserWidget::SetText(const FString& ElementId, const FString& Text)
{
	if (!Document)
	{
		return;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Text target not found: %s", ElementId.c_str());
		return;
	}

	Element->SetInnerRML(Text.c_str());
}

bool UUserWidget::SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Property target not found: %s", ElementId.c_str());
		return false;
	}

	return Element->SetProperty(PropertyName.c_str(), Value.c_str());
}
