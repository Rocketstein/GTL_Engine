#include "LuaBindingInternal.h"
#include "Scripting/LuaEngineBinding.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/Collision/Collider2DComponent.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"
#include "Scripting/LuaScriptTypes.h"
#include "UI/ButtonComponent.h"
#include "UI/TextUIComponent.h"
#include "UI/TextureUIComponent.h"
#include "UI/UIComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Mesh/ObjManager.h"
#include "Materials/MaterialManager.h"
#include "Engine/Runtime/Engine.h"
#include "Component/PointLightComponent.h"
#include "Component/SubUVComponent.h"

namespace
{
    UClass* FindClassByName(const FString& ClassName)
    {
        if (ClassName.empty()) return nullptr;
        for (UClass* Class : UClass::GetAllClasses())
        {
            if (Class && ClassName == Class->GetName()) return Class;
        }
        return nullptr;
    }

    bool DoesComponentMatchClass(UActorComponent* Component, const FString& ClassName)
    {
        if (!Component) return false;
        if (ClassName.empty()) return true;
        UClass* TargetClass = FindClassByName(ClassName);
        if (!TargetClass || !Component->GetClass()) return false;
        return Component->GetClass()->IsA(TargetClass);
    }

    bool ParseTextHorizontalAlignment(const FString& Alignment, EUITextHAlign& OutAlign)
    {
        if (Alignment == "Left")
        {
            OutAlign = EUITextHAlign::Left;
            return true;
        }
        if (Alignment == "Center" || Alignment == "Centre")
        {
            OutAlign = EUITextHAlign::Center;
            return true;
        }
        if (Alignment == "Right")
        {
            OutAlign = EUITextHAlign::Right;
            return true;
        }
        return false;
    }

    bool ParseTextVerticalAlignment(const FString& Alignment, EUITextVAlign& OutAlign)
    {
        if (Alignment == "Top")
        {
            OutAlign = EUITextVAlign::Top;
            return true;
        }
        if (Alignment == "Center" || Alignment == "Centre" || Alignment == "Middle")
        {
            OutAlign = EUITextVAlign::Center;
            return true;
        }
        if (Alignment == "Bottom")
        {
            OutAlign = EUITextVAlign::Bottom;
            return true;
        }
        return false;
    }
}

FLuaComponentHandle::FLuaComponentHandle(const UActorComponent* InComponent)
    : UUID(InComponent ? InComponent->GetUUID() : 0)
{
}

UActorComponent* FLuaComponentHandle::Resolve() const
{
    if (UUID == 0) return nullptr;
    UObject* Object = UObjectManager::Get().FindByUUID(UUID);
    return Cast<UActorComponent>(Object);
}

bool FLuaComponentHandle::IsValid() const
{
    return Resolve() != nullptr;
}

FString FLuaComponentHandle::GetName() const
{
    UActorComponent* Component = Resolve();
    return Component ? Component->GetFName().ToString() : "";
}

FString FLuaComponentHandle::GetComponentClassName() const
{
    UActorComponent* Component = Resolve();
    return Component && Component->GetClass() ? Component->GetClass()->GetName() : "";
}

bool FLuaComponentHandle::IsA(const FString& ClassName) const
{
    UActorComponent* Component = Resolve();
    return DoesComponentMatchClass(Component, ClassName);
}

FLuaActorHandle FLuaComponentHandle::GetOwner() const
{
    UActorComponent* Component = Resolve();
    return FLuaActorHandle(Component ? Component->GetOwner() : nullptr);
}

FLuaWorldHandle FLuaComponentHandle::GetWorld() const
{
    UActorComponent* Component = Resolve();
    return FLuaWorldHandle(Component ? Component->GetWorld() : nullptr);
}

bool FLuaComponentHandle::IsActive() const
{
    UActorComponent* Component = Resolve();
    return Component ? Component->IsActive() : false;
}

bool FLuaComponentHandle::SetActive(bool bActive) const
{
    UActorComponent* Component = Resolve();
    if (!Component) return false;
    Component->SetActive(bActive);
    return true;
}

bool FLuaComponentHandle::SetSpriteVfx(const FString& SpriteVfxName) const
{
    USubUVComponent* SubUV = Cast<USubUVComponent>(Resolve());
    if (!SubUV) return false;
    SubUV->SetSpriteVfx(FName(SpriteVfxName));
    return true;
}

bool FLuaComponentHandle::Play() const
{
    USubUVComponent* SubUV = Cast<USubUVComponent>(Resolve());
    if (!SubUV) return false;
    SubUV->Play();
    return true;
}

bool FLuaComponentHandle::SetLoop(bool bLoop) const
{
    USubUVComponent* SubUV = Cast<USubUVComponent>(Resolve());
    if (!SubUV) return false;
    SubUV->SetLoop(bLoop);
    return true;
}

bool FLuaComponentHandle::SetFrameRate(float FPS) const
{
    USubUVComponent* SubUV = Cast<USubUVComponent>(Resolve());
    if (!SubUV) return false;
    SubUV->SetFrameRate(FPS);
    return true;
}

bool FLuaComponentHandle::IsScriptComponent() const
{
    return Cast<UScriptComponent>(Resolve()) != nullptr;
}

bool FLuaComponentHandle::CallScript(const FString& FunctionName, const sol::variadic_args& Args) const
{
    UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Resolve());
    if (!ScriptComponent) return false;
    return ScriptComponent->CallScriptFunction(FunctionName, Args);
}

sol::table FLuaComponentHandle::GetWorldLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetWorldLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetWorldLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->SetWorldLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetRelativeLocation(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetRelativeLocation() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetRelativeLocation(const sol::object& Value) const
{
    FVector Location;
    if (!ReadLuaVec3(Value, Location)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->SetRelativeLocation(Location);
    return true;
}

sol::table FLuaComponentHandle::GetRelativeRotation(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetRelativeRotation().ToVector() : FVector(0.0f, 0.0f, 0.0f));
}

bool FLuaComponentHandle::SetRelativeRotation(const sol::object& Value) const
{
    FVector Rotation;
    if (!ReadLuaVec3(Value, Rotation)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->SetRelativeRotation(Rotation);
    return true;
}

sol::table FLuaComponentHandle::GetForwardVector(sol::this_state State) const
{
    sol::state_view Lua(State);
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    return MakeLuaVec3(Lua, SceneComponent ? SceneComponent->GetForwardVector() : FVector(0.0f, 0.0f, 1.0f));
}

bool FLuaComponentHandle::LookAt(const sol::object& Value) const
{
    FVector Target;
    if (!ReadLuaVec3(Value, Target)) return false;
    USceneComponent* SceneComponent = Cast<USceneComponent>(Resolve());
    if (!SceneComponent) return false;
    SceneComponent->LookAt(Target);
    return true;
}

sol::object FLuaComponentHandle::LuaCast(sol::this_state State, const FString& ClassName) const
{
    sol::state_view Lua(State);
    UActorComponent* Component = Resolve();
    if (!Component) return sol::nil;

    if (ClassName == "Collider2D" || ClassName == "UCollider2DComponent")
    {
        if (Cast<UCollider2DComponent>(Component)) return sol::make_object(Lua, FLuaCollider2DHandle(Component));
        return sol::nil;
    }
    if (ClassName == "BoxCollider2D" || ClassName == "UBoxCollider2DComponent")
    {
        if (IsA("UBoxCollider2DComponent")) return sol::make_object(Lua, FLuaBoxCollider2DHandle(Component));
        return sol::nil;
    }
    if (ClassName == "CircleCollider2D" || ClassName == "UCircleCollider2DComponent")
    {
        if (IsA("UCircleCollider2DComponent")) return sol::make_object(Lua, FLuaCircleCollider2DHandle(Component));
        return sol::nil;
    }
    UE_LOG("[Lua]", Warning, "LuaCast to %s not supported.", ClassName.c_str());
    return sol::nil;
}

bool FLuaComponentHandle::IsVisible() const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    return PrimitiveComponent ? PrimitiveComponent->IsVisible() : false;
}

bool FLuaComponentHandle::SetVisibility(bool bVisible) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    if (!PrimitiveComponent) return false;
    PrimitiveComponent->SetVisibility(bVisible);
    return true;
}

bool FLuaComponentHandle::ShouldGenerateOverlapEvents() const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    return PrimitiveComponent ? PrimitiveComponent->ShouldGenerateOverlapEvents() : false;
}

bool FLuaComponentHandle::SetGenerateOverlapEvents(bool bGenerate) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    if (!PrimitiveComponent) return false;
    PrimitiveComponent->SetGenerateOverlapEvents(bGenerate);
    return true;
}

bool FLuaComponentHandle::IsOverlappingActor(const FLuaActorHandle& OtherActor) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    AActor* Other = OtherActor.Resolve();
    if (!PrimitiveComponent || !Other) return false;
    return PrimitiveComponent->IsOverlappingActor(Other);
}

bool FLuaComponentHandle::IsOverlappingComponent(const FLuaComponentHandle& OtherComponent) const
{
    UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Resolve());
    UPrimitiveComponent* OtherPrimitive = Cast<UPrimitiveComponent>(OtherComponent.Resolve());
    if (!PrimitiveComponent || !OtherPrimitive) return false;
    return PrimitiveComponent->IsOverlappingComponent(OtherPrimitive);
}

bool FLuaComponentHandle::IsUIComponent() const
{
    return Cast<UUIComponent>(Resolve()) != nullptr;
}

bool FLuaComponentHandle::SetUIRenderSpace(const FString& RenderSpace) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;

    if (RenderSpace == "WorldSpace" || RenderSpace == "World")
    {
        UI->SetRenderSpace(EUIRenderSpace::WorldSpace);
        return true;
    }
    if (RenderSpace == "ScreenSpace" || RenderSpace == "Screen")
    {
        UI->SetRenderSpace(EUIRenderSpace::ScreenSpace);
        return true;
    }

    return false;
}

bool FLuaComponentHandle::SetUITexturePath(const FString& TexturePath) const
{
    UTextureUIComponent* UI = Cast<UTextureUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetTexturePath(TexturePath);
    return true;
}

bool FLuaComponentHandle::SetUIAnchor(const sol::object& Value) const
{
    FVector2 Anchor;
    if (!ReadLuaVec2(Value, Anchor)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetAnchorMin(Anchor);
    UI->SetAnchorMax(Anchor);
    return true;
}

bool FLuaComponentHandle::SetUIAnchorMin(const sol::object& Value) const
{
    FVector2 Anchor;
    if (!ReadLuaVec2(Value, Anchor)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetAnchorMin(Anchor);
    return true;
}

bool FLuaComponentHandle::SetUIAnchorMax(const sol::object& Value) const
{
    FVector2 Anchor;
    if (!ReadLuaVec2(Value, Anchor)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetAnchorMax(Anchor);
    return true;
}

sol::table FLuaComponentHandle::GetUIAnchorMin(sol::this_state State) const
{
    sol::state_view Lua(State);
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return MakeLuaVec2(Lua, UI ? UI->GetAnchorMin() : FVector2(0.0f, 0.0f));
}

sol::table FLuaComponentHandle::GetUIAnchorMax(sol::this_state State) const
{
    sol::state_view Lua(State);
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return MakeLuaVec2(Lua, UI ? UI->GetAnchorMax() : FVector2(0.0f, 0.0f));
}

bool FLuaComponentHandle::SetUIAnchoredPosition(const sol::object& Value) const
{
    FVector2 Position;
    if (!ReadLuaVec2(Value, Position)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetAnchoredPosition(Position);
    return true;
}

sol::table FLuaComponentHandle::GetUIAnchoredPosition(sol::this_state State) const
{
    sol::state_view Lua(State);
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return MakeLuaVec2(Lua, UI ? UI->GetAnchoredPosition() : FVector2(0.0f, 0.0f));
}

bool FLuaComponentHandle::SetUISizeDelta(const sol::object& Value) const
{
    FVector2 Size;
    if (!ReadLuaVec2(Value, Size)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetSizeDelta(Size);
    return true;
}

sol::table FLuaComponentHandle::GetUISizeDelta(sol::this_state State) const
{
    sol::state_view Lua(State);
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return MakeLuaVec2(Lua, UI ? UI->GetSizeDelta() : FVector2(0.0f, 0.0f));
}

bool FLuaComponentHandle::SetUIWorldSize(const sol::object& Value) const
{
    FVector2 Size;
    if (!ReadLuaVec2(Value, Size)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetWorldSize(Size);
    return true;
}

bool FLuaComponentHandle::SetUIBillboard(bool bBillboard) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetBillboard(bBillboard);
    return true;
}

bool FLuaComponentHandle::SetUIPivot(const sol::object& Value) const
{
    FVector2 Pivot;
    if (!ReadLuaVec2(Value, Pivot)) return false;
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetPivot(Pivot);
    return true;
}

sol::table FLuaComponentHandle::GetUIPivot(sol::this_state State) const
{
    sol::state_view Lua(State);
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return MakeLuaVec2(Lua, UI ? UI->GetPivot() : FVector2(0.5f, 0.5f));
}

bool FLuaComponentHandle::SetUIRotationDegrees(float Degrees) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetRotationDegrees(Degrees);
    return true;
}

float FLuaComponentHandle::GetUIRotationDegrees() const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    return UI ? UI->GetRotationDegrees() : 0.0f;
}

bool FLuaComponentHandle::SetUITint(float R, float G, float B, float A) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetTintColor(FVector4(R, G, B, A));
    return true;
}

sol::table FLuaComponentHandle::GetUITint(sol::this_state State) const
{
    sol::state_view Lua(State);
    sol::table Color = Lua.create_table();
    const UUIComponent* UI = Cast<UUIComponent>(Resolve());
    const FVector4 Tint = UI ? UI->GetTintColor() : FVector4(1.0f, 1.0f, 1.0f, 1.0f);

    Color[1] = Tint.R;
    Color[2] = Tint.G;
    Color[3] = Tint.B;
    Color[4] = Tint.A;
    Color["r"] = Tint.R;
    Color["g"] = Tint.G;
    Color["b"] = Tint.B;
    Color["a"] = Tint.A;
    Color["x"] = Tint.X;
    Color["y"] = Tint.Y;
    Color["z"] = Tint.Z;
    Color["w"] = Tint.W;
    return Color;
}

bool FLuaComponentHandle::SetUIVisibility(bool bVisible) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetVisibility(bVisible);
    return true;
}

bool FLuaComponentHandle::SetUIHitTestVisible(bool bHitTestVisible) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetHitTestVisible(bHitTestVisible);
    return true;
}

bool FLuaComponentHandle::SetUILayer(int32 Layer) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetLayer(Layer);
    return true;
}

bool FLuaComponentHandle::SetUIZOrder(int32 ZOrder) const
{
    UUIComponent* UI = Cast<UUIComponent>(Resolve());
    if (!UI) return false;
    UI->SetZOrder(ZOrder);
    return true;
}

bool FLuaComponentHandle::SetStaticMesh(const FString& MeshPath) const
{
    UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Resolve());
    if (!MeshComp) return false;

    ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Mesh = FObjManager::Get().Load(MeshPath);
    if (!Mesh) return false;

    MeshComp->SetStaticMesh(Mesh);
    return true;
}

bool FLuaComponentHandle::SetMaterial(int32 Index, const FString& MaterialPath) const
{
    UMeshComponent* MeshComp = Cast<UMeshComponent>(Resolve());
    if (!MeshComp) return false;

    UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
    if (!Material) return false;

    MeshComp->SetMaterial(Index, Material);
    return true;
}

bool FLuaComponentHandle::SetMaterialVector4Parameter(int32 Index, const FString& ParamName, const sol::table& Value) const
{
    UMeshComponent* MeshComp = Cast<UMeshComponent>(Resolve());
    if (!MeshComp) return false;

    UMaterial* Material = MeshComp->GetMaterial(Index);
    if (!Material) return false;

    FVector4 Vec(0, 0, 0, 1);
    sol::object XObj = Value["x"];
    if (!XObj.valid() || XObj == sol::nil)
    {
        XObj = Value["X"];
    }

    if (XObj.valid() && XObj.get_type() == sol::type::number)
    {
        sol::optional<float> x = Value["x"];
        sol::optional<float> X = Value["X"];
        sol::optional<float> y = Value["y"];
        sol::optional<float> Y = Value["Y"];
        sol::optional<float> z = Value["z"];
        sol::optional<float> Z = Value["Z"];
        sol::optional<float> w = Value["w"];
        sol::optional<float> W = Value["W"];

        Vec.X = x.value_or(X.value_or(0.0f));
        Vec.Y = y.value_or(Y.value_or(0.0f));
        Vec.Z = z.value_or(Z.value_or(0.0f));
        Vec.W = w.value_or(W.value_or(1.0f));
    }
    else
    {
        sol::optional<float> v1 = Value[1];
        sol::optional<float> v2 = Value[2];
        sol::optional<float> v3 = Value[3];
        sol::optional<float> v4 = Value[4];
        Vec.X = v1.value_or(0.0f);
        Vec.Y = v2.value_or(0.0f);
        Vec.Z = v3.value_or(0.0f);
        Vec.W = v4.value_or(1.0f);
    }

    Material->SetVector4Parameter(ParamName, Vec);
    MeshComp->MarkRenderStateDirty();
    return true;
}

bool FLuaComponentHandle::SetIntensity(float Intensity) const
{
    ULightComponentBase* LightBase = Cast<ULightComponentBase>(Resolve());
    if (!LightBase) return false;
    LightBase->SetIntensity(Intensity);

    if (ULightComponent* Light = Cast<ULightComponent>(LightBase))
    {
        Light->MarkRenderStateDirty();
    }
    return true;
}

bool FLuaComponentHandle::IsUIText() const
{
    return Cast<UTextUIComponent>(Resolve()) != nullptr;
}

FString FLuaComponentHandle::GetUIText() const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    return TextUI ? TextUI->GetText() : "";
}

bool FLuaComponentHandle::SetUIText(const FString& Text) const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    if (!TextUI) return false;
    TextUI->SetText(Text);
    return true;
}

bool FLuaComponentHandle::SetUIFont(const FString& FontName) const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    if (!TextUI) return false;
    TextUI->SetFont(FName(FontName));
    return true;
}

bool FLuaComponentHandle::SetUIFontSize(float FontSize) const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    if (!TextUI) return false;
    TextUI->SetFontSize(FontSize);
    return true;
}

bool FLuaComponentHandle::SetUITextHorizontalAlignment(const FString& Alignment) const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    if (!TextUI) return false;

    EUITextHAlign Align = EUITextHAlign::Left;
    if (!ParseTextHorizontalAlignment(Alignment, Align)) return false;

    TextUI->SetHorizontalAlignment(Align);
    return true;
}

bool FLuaComponentHandle::SetUITextVerticalAlignment(const FString& Alignment) const
{
    UTextUIComponent* TextUI = Cast<UTextUIComponent>(Resolve());
    if (!TextUI) return false;

    EUITextVAlign Align = EUITextVAlign::Top;
    if (!ParseTextVerticalAlignment(Alignment, Align)) return false;

    TextUI->SetVerticalAlignment(Align);
    return true;
}

bool FLuaComponentHandle::IsUIButton() const
{
    return Cast<UUIButtonComponent>(Resolve()) != nullptr;
}

bool FLuaComponentHandle::IsButtonInteractable() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsInteractable() : false;
}

bool FLuaComponentHandle::SetButtonInteractable(bool bInteractable) const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    if (!Button) return false;
    Button->SetInteractable(bInteractable);
    return true;
}

bool FLuaComponentHandle::IsButtonHovered() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsHovered() : false;
}

bool FLuaComponentHandle::IsButtonPressed() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->IsPressed() : false;
}

int32 FLuaComponentHandle::GetButtonClickCount() const
{
    UUIButtonComponent* Button = Cast<UUIButtonComponent>(Resolve());
    return Button ? Button->GetClickCount() : 0;
}

namespace LuaBinding
{
    void RegisterComponent(sol::state& Lua)
    {
        Lua.new_usertype<FLuaComponentHandle>(
            "Component",
            sol::no_constructor,

            "IsValid", &FLuaComponentHandle::IsValid,
            "GetName", &FLuaComponentHandle::GetName,
            "GetClassName", &FLuaComponentHandle::GetComponentClassName,
            "IsA", &FLuaComponentHandle::IsA,
            "GetOwner", &FLuaComponentHandle::GetOwner,
            "GetWorld", &FLuaComponentHandle::GetWorld,
            "IsActive", &FLuaComponentHandle::IsActive,
            "SetActive", &FLuaComponentHandle::SetActive,
            "SetSpriteVfx", &FLuaComponentHandle::SetSpriteVfx,
            "Play", &FLuaComponentHandle::Play,
            "SetLoop", &FLuaComponentHandle::SetLoop,
            "SetFrameRate", &FLuaComponentHandle::SetFrameRate,
            "IsScriptComponent", &FLuaComponentHandle::IsScriptComponent,
            "CallScript", &FLuaComponentHandle::CallScript,

            "GetWorldLocation", &FLuaComponentHandle::GetWorldLocation,
            "SetWorldLocation", &FLuaComponentHandle::SetWorldLocation,
            "GetRelativeLocation", &FLuaComponentHandle::GetRelativeLocation,
            "SetRelativeLocation", &FLuaComponentHandle::SetRelativeLocation,
            "GetRelativeRotation", &FLuaComponentHandle::GetRelativeRotation,
            "SetRelativeRotation", &FLuaComponentHandle::SetRelativeRotation,
            "GetForwardVector", &FLuaComponentHandle::GetForwardVector,
            "LookAt", &FLuaComponentHandle::LookAt,
            "Cast", &FLuaComponentHandle::LuaCast,

            "IsVisible", &FLuaComponentHandle::IsVisible,
            "SetVisibility", &FLuaComponentHandle::SetVisibility,
            "ShouldGenerateOverlapEvents", &FLuaComponentHandle::ShouldGenerateOverlapEvents,
            "SetGenerateOverlapEvents", &FLuaComponentHandle::SetGenerateOverlapEvents,
            "IsOverlappingActor", &FLuaComponentHandle::IsOverlappingActor,
            "IsOverlappingComponent", &FLuaComponentHandle::IsOverlappingComponent,

            "IsUIComponent", &FLuaComponentHandle::IsUIComponent,
            "SetUIRenderSpace", &FLuaComponentHandle::SetUIRenderSpace,
            "SetUITexturePath", &FLuaComponentHandle::SetUITexturePath,
            "SetUIAnchor", &FLuaComponentHandle::SetUIAnchor,
            "SetUIAnchorMin", &FLuaComponentHandle::SetUIAnchorMin,
            "SetUIAnchorMax", &FLuaComponentHandle::SetUIAnchorMax,
            "GetUIAnchorMin", &FLuaComponentHandle::GetUIAnchorMin,
            "GetUIAnchorMax", &FLuaComponentHandle::GetUIAnchorMax,
            "SetUIAnchoredPosition", &FLuaComponentHandle::SetUIAnchoredPosition,
            "GetUIAnchoredPosition", &FLuaComponentHandle::GetUIAnchoredPosition,
            "SetUISizeDelta", &FLuaComponentHandle::SetUISizeDelta,
            "GetUISizeDelta", &FLuaComponentHandle::GetUISizeDelta,
            "SetUIWorldSize", &FLuaComponentHandle::SetUIWorldSize,
            "SetUIBillboard", &FLuaComponentHandle::SetUIBillboard,
            "SetUIPivot", &FLuaComponentHandle::SetUIPivot,
            "GetUIPivot", &FLuaComponentHandle::GetUIPivot,
            "SetUIRotationDegrees", &FLuaComponentHandle::SetUIRotationDegrees,
            "GetUIRotationDegrees", &FLuaComponentHandle::GetUIRotationDegrees,
            "SetUITint", &FLuaComponentHandle::SetUITint,
            "GetUITint", &FLuaComponentHandle::GetUITint,
            "SetUIVisibility", &FLuaComponentHandle::SetUIVisibility,
            "SetUIHitTestVisible", &FLuaComponentHandle::SetUIHitTestVisible,
            "SetUILayer", &FLuaComponentHandle::SetUILayer,
            "SetUIZOrder", &FLuaComponentHandle::SetUIZOrder,
            "SetStaticMesh", &FLuaComponentHandle::SetStaticMesh,
            "SetMaterial", &FLuaComponentHandle::SetMaterial,
            "SetMaterialVector4Parameter", &FLuaComponentHandle::SetMaterialVector4Parameter,
            "SetIntensity", &FLuaComponentHandle::SetIntensity,
            "IsUIText", &FLuaComponentHandle::IsUIText,
            "GetUIText", &FLuaComponentHandle::GetUIText,
            "SetUIText", &FLuaComponentHandle::SetUIText,
            "SetUIFont", &FLuaComponentHandle::SetUIFont,
            "SetUIFontSize", &FLuaComponentHandle::SetUIFontSize,
            "SetUITextHorizontalAlignment", &FLuaComponentHandle::SetUITextHorizontalAlignment,
            "SetUITextVerticalAlignment", &FLuaComponentHandle::SetUITextVerticalAlignment,

            "IsUIButton", &FLuaComponentHandle::IsUIButton,
            "IsButtonInteractable", &FLuaComponentHandle::IsButtonInteractable,
            "SetButtonInteractable", &FLuaComponentHandle::SetButtonInteractable,
            "IsButtonHovered", &FLuaComponentHandle::IsButtonHovered,
            "IsButtonPressed", &FLuaComponentHandle::IsButtonPressed,
            "GetButtonClickCount", &FLuaComponentHandle::GetButtonClickCount);
    }
}
