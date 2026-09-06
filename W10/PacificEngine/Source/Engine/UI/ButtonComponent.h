#pragma once

#include "UI/TextureUIComponent.h"

#include <functional>
#include <utility>

class UUIButtonComponent : public UTextureUIComponent
{
public:
    DECLARE_CLASS(UUIButtonComponent, UTextureUIComponent)

    using FButtonCallback = std::function<void(UUIButtonComponent*)>;

    UUIButtonComponent();

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    bool HandleUIPointerEvent(const FViewportPointerEvent& Event) override;
    void OnUIPointerEnter(const FViewportPointerEvent& Event) override;
    void OnUIPointerLeave(const FViewportPointerEvent& Event) override;

    void SetInteractable(bool bInInteractable);
    bool IsInteractable() const { return bInteractable; }
    bool IsHovered() const { return bHovered; }
    bool IsPressed() const { return bPressed; }
    int32 GetClickCount() const { return ClickCount; }

    void SetNormalTint(const FVector4& InTint);
    void SetHoveredTint(const FVector4& InTint);
    void SetPressedTint(const FVector4& InTint);
    void SetDisabledTint(const FVector4& InTint);

    const FVector4& GetNormalTint() const { return NormalTint; }
    const FVector4& GetHoveredTint() const { return HoveredTint; }
    const FVector4& GetPressedTint() const { return PressedTint; }
    const FVector4& GetDisabledTint() const { return DisabledTint; }

    void SetOnClicked(FButtonCallback Callback) { OnClicked = std::move(Callback); }
    void SetOnPressed(FButtonCallback Callback) { OnPressed = std::move(Callback); }
    void SetOnReleased(FButtonCallback Callback) { OnReleased = std::move(Callback); }
    void SetOnHovered(FButtonCallback Callback) { OnHovered = std::move(Callback); }
    void SetOnUnhovered(FButtonCallback Callback) { OnUnhovered = std::move(Callback); }

private:
    void UpdateVisualState();
    void SetHoveredInternal(bool bInHovered);
    void SetPressedInternal(bool bInPressed);

private:
    bool bInteractable = true;
    bool bHovered = false;
    bool bPressed = false;
    int32 ClickCount = 0;

    FVector4 NormalTint = { 1.0f, 1.0f, 1.0f, 1.0f };
    FVector4 HoveredTint = { 0.9f, 0.95f, 1.0f, 1.0f };
    FVector4 PressedTint = { 0.72f, 0.82f, 1.0f, 1.0f };
    FVector4 DisabledTint = { 0.35f, 0.35f, 0.35f, 0.65f };

    FButtonCallback OnClicked;
    FButtonCallback OnPressed;
    FButtonCallback OnReleased;
    FButtonCallback OnHovered;
    FButtonCallback OnUnhovered;
};
