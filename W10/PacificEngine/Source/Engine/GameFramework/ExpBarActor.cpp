#include "GameFramework/ExpBarActor.h"

#include "Component/ScriptComponent.h"
#include "Object/ObjectFactory.h"
#include "UI/TextureUIComponent.h"

IMPLEMENT_CLASS(AExpBarActor, AActor)

namespace
{
constexpr const char* ExpBarBackName = "PlayerExpBar_Back";
constexpr const char* ExpBarFillName = "PlayerExpBar_Fill";
constexpr const char* ExpBarScriptName = "ExpBarScript";
constexpr const char* ExpBarScriptPath = "UI/ExpBar.lua";

const FVector2 ExpBarAnchorMin(0.08f, 0.035f);
const FVector2 ExpBarAnchorMax(0.92f, 0.035f);
const FVector2 ExpBarBackSizeDelta(0.0f, 18.0f);
const FVector2 ExpBarFillSizeDelta(0.0f, 12.0f);
}

void AExpBarActor::BeginPlay()
{
    EnsureExpBarComponents();
    AActor::BeginPlay();
}

void AExpBarActor::InitDefaultComponents()
{
    EnsureExpBarComponents();
}

void AExpBarActor::EnsureExpBarComponents()
{
    BackComponent = GetOrCreateTextureUIComponent(ExpBarBackName);
    FillComponent = GetOrCreateTextureUIComponent(ExpBarFillName);
    ScriptComponent = GetOrCreateScriptComponent();

    ConfigureBackComponent(BackComponent);
    ConfigureFillComponent(FillComponent);

    if (BackComponent)
    {
        SetRootComponent(BackComponent);
    }
    if (BackComponent && FillComponent && FillComponent->GetParent() != BackComponent)
    {
        FillComponent->AttachToComponent(BackComponent);
    }
}

UTextureUIComponent* AExpBarActor::GetOrCreateTextureUIComponent(const FString& Name)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UTextureUIComponent* TextureUI = Cast<UTextureUIComponent>(Component);
        if (TextureUI && TextureUI->GetFName().ToString() == Name)
        {
            TextureUI->SetActive(true);
            return TextureUI;
        }
    }

    UTextureUIComponent* Component = AddComponent<UTextureUIComponent>();
    if (!Component)
    {
        return nullptr;
    }

    Component->SetFName(Name);
    Component->SetActive(true);
    return Component;
}

UScriptComponent* AExpBarActor::GetOrCreateScriptComponent()
{
    for (UActorComponent* Component : OwnedComponents)
    {
        UScriptComponent* Script = Cast<UScriptComponent>(Component);
        if (Script && Script->GetFName().ToString() == ExpBarScriptName)
        {
            Script->SetScriptPath(ExpBarScriptPath);
            return Script;
        }
    }

    UScriptComponent* Script = AddComponent<UScriptComponent>();
    if (!Script)
    {
        return nullptr;
    }

    Script->SetFName(ExpBarScriptName);
    Script->SetScriptPath(ExpBarScriptPath);
    return Script;
}

void AExpBarActor::ConfigureBackComponent(UTextureUIComponent* Component)
{
    if (!Component)
    {
        return;
    }

    Component->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    Component->SetGeometryType(EUIGeometryType::Quad);
    Component->SetTexturePath("");
    Component->ResetSubUVRect();
    Component->SetAnchorMin(ExpBarAnchorMin);
    Component->SetAnchorMax(ExpBarAnchorMax);
    Component->SetAnchoredPosition(FVector2(0.0f, 0.0f));
    Component->SetSizeDelta(ExpBarBackSizeDelta);
    Component->SetPivot(FVector2(0.5f, 0.5f));
    Component->SetRotationDegrees(0.0f);
    Component->SetLayer(200);
    Component->SetZOrder(0);
    Component->SetHitTestVisible(false);
    Component->SetTintColor(FVector4(0.035f, 0.04f, 0.065f, 0.84f));
    Component->SetVisibility(true);
}

void AExpBarActor::ConfigureFillComponent(UTextureUIComponent* Component)
{
    if (!Component)
    {
        return;
    }

    Component->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    Component->SetGeometryType(EUIGeometryType::Quad);
    Component->SetTexturePath("");
    Component->ResetSubUVRect();
    Component->SetAnchorMin(ExpBarAnchorMin);
    Component->SetAnchorMax(ExpBarAnchorMin);
    Component->SetAnchoredPosition(FVector2(0.0f, 0.0f));
    Component->SetSizeDelta(ExpBarFillSizeDelta);
    Component->SetPivot(FVector2(0.0f, 0.5f));
    Component->SetRotationDegrees(0.0f);
    Component->SetLayer(200);
    Component->SetZOrder(1);
    Component->SetHitTestVisible(false);
    Component->SetTintColor(FVector4(0.36f, 0.72f, 1.0f, 0.95f));
    Component->SetVisibility(false);
}
