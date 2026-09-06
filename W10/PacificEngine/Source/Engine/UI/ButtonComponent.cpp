#include "UI/ButtonComponent.h"

#include "Input/InputTypes.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Sound/SoundManager.h"

#include <cstring>
#include <utility>

namespace
{
    const FName UIClickSoundKey("ui_click");
}

IMPLEMENT_CLASS(UUIButtonComponent, UTextureUIComponent)

UUIButtonComponent::UUIButtonComponent()
{
    SetTintColor(NormalTint);
}

void UUIButtonComponent::Serialize(FArchive& Ar)
{
    UTextureUIComponent::Serialize(Ar);
    Ar << bInteractable;
    Ar << ClickCount;
    Ar << NormalTint;
    Ar << HoveredTint;
    Ar << PressedTint;
    Ar << DisabledTint;
}

void UUIButtonComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UTextureUIComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Button Interactable", EPropertyType::Bool, &bInteractable });
    OutProps.push_back({ "Button Normal Tint", EPropertyType::Color4, &NormalTint });
    OutProps.push_back({ "Button Hovered Tint", EPropertyType::Color4, &HoveredTint });
    OutProps.push_back({ "Button Pressed Tint", EPropertyType::Color4, &PressedTint });
    OutProps.push_back({ "Button Disabled Tint", EPropertyType::Color4, &DisabledTint });
    OutProps.push_back({ "Button Click Count", EPropertyType::Int, &ClickCount });
}

void UUIButtonComponent::PostEditProperty(const char* PropertyName)
{
    UTextureUIComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "Button Interactable") == 0)
    {
        if (!bInteractable)
        {
            bHovered = false;
            bPressed = false;
        }
        UpdateVisualState();
        return;
    }

    if (std::strcmp(PropertyName, "Button Normal Tint") == 0 ||
        std::strcmp(PropertyName, "Button Hovered Tint") == 0 ||
        std::strcmp(PropertyName, "Button Pressed Tint") == 0 ||
        std::strcmp(PropertyName, "Button Disabled Tint") == 0)
    {
        UpdateVisualState();
    }
}

bool UUIButtonComponent::HandleUIPointerEvent(const FViewportPointerEvent& Event)
{
    if (!bInteractable)
    {
        return true;
    }

    if (Event.Button == EPointerButton::Left && Event.Type == EPointerEventType::Pressed)
    {
        SetPressedInternal(true);
        if (OnPressed)
        {
            OnPressed(this);
        }
        return true;
    }

    if (Event.Button == EPointerButton::Left && Event.Type == EPointerEventType::Released)
    {
        const bool bWasPressed = bPressed;
        SetPressedInternal(false);

        if (OnReleased)
        {
            OnReleased(this);
        }

        if (bWasPressed && bHovered)
        {
            ++ClickCount;
            FSoundManager::Get().Play(UIClickSoundKey, ESoundBus::UI, 1.0f);
            if (OnClicked)
            {
                OnClicked(this);
            }
        }

        return true;
    }

    return true;
}

void UUIButtonComponent::OnUIPointerEnter(const FViewportPointerEvent& Event)
{
    (void)Event;
    SetHoveredInternal(true);
}

void UUIButtonComponent::OnUIPointerLeave(const FViewportPointerEvent& Event)
{
    (void)Event;
    SetHoveredInternal(false);
    SetPressedInternal(false);
}

void UUIButtonComponent::SetInteractable(bool bInInteractable)
{
    if (bInteractable == bInInteractable)
    {
        return;
    }

    bInteractable = bInInteractable;
    if (!bInteractable)
    {
        bHovered = false;
        bPressed = false;
    }
    UpdateVisualState();
}

void UUIButtonComponent::SetNormalTint(const FVector4& InTint)
{
    NormalTint = InTint;
    UpdateVisualState();
}

void UUIButtonComponent::SetHoveredTint(const FVector4& InTint)
{
    HoveredTint = InTint;
    UpdateVisualState();
}

void UUIButtonComponent::SetPressedTint(const FVector4& InTint)
{
    PressedTint = InTint;
    UpdateVisualState();
}

void UUIButtonComponent::SetDisabledTint(const FVector4& InTint)
{
    DisabledTint = InTint;
    UpdateVisualState();
}

void UUIButtonComponent::UpdateVisualState()
{
    if (!bInteractable)
    {
        SetTintColor(DisabledTint);
        return;
    }

    if (bPressed)
    {
        SetTintColor(PressedTint);
        return;
    }

    if (bHovered)
    {
        SetTintColor(HoveredTint);
        return;
    }

    SetTintColor(NormalTint);
}

void UUIButtonComponent::SetHoveredInternal(bool bInHovered)
{
    if (!bInteractable || bHovered == bInHovered)
    {
        return;
    }

    bHovered = bInHovered;
    UpdateVisualState();

    if (bHovered)
    {
        if (OnHovered)
        {
            OnHovered(this);
        }
    }
    else
    {
        if (OnUnhovered)
        {
            OnUnhovered(this);
        }
    }
}

void UUIButtonComponent::SetPressedInternal(bool bInPressed)
{
    if (!bInteractable || bPressed == bInPressed)
    {
        return;
    }

    bPressed = bInPressed;
    UpdateVisualState();
}
