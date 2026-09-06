#pragma once

#include "Core/CoreTypes.h"
#include "sol.hpp"

class AActor;
class UActorComponent;
class UCollider2DComponent;

struct FLuaComponentHandle;
struct FLuaCollider2DHandle;

class FActorPoolManager;
class FPostProcessController;
class UWorld;

struct FLuaPostProcessHandle
{
	FPostProcessController* Controller = nullptr;

	FLuaPostProcessHandle() = default;
	explicit FLuaPostProcessHandle(FPostProcessController* InController);

	bool IsValid() const;

	bool FadeIn(float Duration, const sol::variadic_args& Args) const;
	bool FadeOut(float Duration, const sol::variadic_args& Args) const;
	bool FadeTo(float TargetAlpha, float Duration, const sol::variadic_args& Args) const;
	bool SetFadeAlpha(float Alpha, const sol::variadic_args& Args) const;
	void StopFade() const;
	void ClearFade() const;
	bool IsFadePlaying() const;
	float GetFadeAlpha() const;
	sol::table GetFadeColor(sol::this_state State) const;

	bool SetVignettingEnabled(bool bEnabled) const;
	bool SetVignetting(float Intensity, float Radius, float Softness, const sol::variadic_args& Args) const;
	bool IsVignettingEnabled() const;
	float GetVignettingIntensity() const;
	float GetVignettingRadius() const;
	float GetVignettingSoftness() const;
	sol::table GetVignettingColor(sol::this_state State) const;

	bool SetGammaCorrectionEnabled(bool bEnabled) const;
	bool SetDisplayGamma(float DisplayGamma) const;
	bool IsGammaCorrectionEnabled() const;
	float GetDisplayGamma() const;

	bool SetLetterboxEnabled(bool bEnabled) const;
	bool SetLetterbox(float TargetAspectRatio, float Opacity, const sol::variadic_args& Args) const;
	bool IsLetterboxEnabled() const;
	float GetLetterboxTargetAspectRatio() const;
	float GetLetterboxOpacity() const;
	sol::table GetLetterboxColor(sol::this_state State) const;
};

struct FLuaWorldHandle
{
	UWorld* World = nullptr;

	FLuaWorldHandle() = default;
	explicit FLuaWorldHandle(UWorld* InWorld);

	bool IsValid() const;
	bool IsGameplayPaused() const;
	void SetGameplayPaused(bool bPaused) const;

	float GetTimeDilation() const;
	void SetTimeDilation(float InTimeDilation) const;

	sol::table GetActorsByTag(sol::this_state State, const FString& Tag) const;
};

struct FLuaActorHandle
{
	uint32 UUID = 0;

	FLuaActorHandle() = default;
	explicit FLuaActorHandle(const AActor* InActor);

	AActor* Resolve() const;
	bool IsValid() const;

	uint32 GetUUID() const { return UUID; }
	FLuaWorldHandle GetWorld() const;

	FString GetName() const;
	FString GetActorClassName() const;

	FString GetTag() const;
	void SetTag(const FString& InTag) const;

	sol::table GetLocation(sol::this_state State) const;
	bool SetLocation(const sol::object& Value) const;
	bool AddWorldOffset(const sol::object& Value) const;

	sol::table GetRotation(sol::this_state State) const;
	bool SetRotation(const sol::object& Value) const;

	sol::table GetScale(sol::this_state State) const;
	bool SetScale(const sol::object& Value) const;

	sol::table GetForward(sol::this_state State) const;

	bool IsVisible() const;
	bool SetVisible(bool bVisible) const;

	float GetCustomTimeDilation() const;
	void SetCustomTimeDilation(float InTimeDilation) const;

	bool InitFlyingWave(const sol::object& Dirobj, float Speed, float LifeTime) const;

	bool SetVfxPreset(const FString& PresetName) const;

	FLuaComponentHandle GetComponent(const sol::variadic_args& Args) const;
	FLuaComponentHandle GetComponentByName(const sol::variadic_args& Args) const;
	sol::table GetComponents(sol::this_state State, const sol::variadic_args& Args) const;
};

struct FLuaComponentHandle
{
	uint32 UUID = 0;

	FLuaComponentHandle() = default;
	explicit FLuaComponentHandle(const UActorComponent* InComponent);

	UActorComponent* Resolve() const;
	bool IsValid() const;

	FString GetName() const;
	FString GetComponentClassName() const;
	bool IsA(const FString& ClassName) const;

	FLuaActorHandle GetOwner() const;
	FLuaWorldHandle GetWorld() const;

	bool IsActive() const;
	bool SetActive(bool bActive) const;
	bool IsScriptComponent() const;
	bool CallScript(const FString& FunctionName, const sol::variadic_args& Args) const;

	// SubUV API
	bool SetSpriteVfx(const FString& SpriteVfxName) const;
	bool Play() const;
	bool SetLoop(bool bLoop) const;
	bool SetFrameRate(float FPS) const;

	// SceneComponent API
	sol::table GetWorldLocation(sol::this_state State) const;
	bool SetWorldLocation(const sol::object& Value) const;

	sol::table GetRelativeLocation(sol::this_state State) const;
	bool SetRelativeLocation(const sol::object& Value) const;
	sol::table GetRelativeRotation(sol::this_state State) const;
	bool SetRelativeRotation(const sol::object& Value) const;

	sol::table GetForwardVector(sol::this_state State) const;

	bool LookAt(const sol::object& State) const;

	sol::object LuaCast(sol::this_state State, const FString& ClassName) const;

	// PrimitiveComponent API
	bool IsVisible() const;
	bool SetVisibility(bool bVisible) const;

	bool ShouldGenerateOverlapEvents() const;
	bool SetGenerateOverlapEvents(bool bGenerate) const;

	bool IsOverlappingActor(const FLuaActorHandle& OtherActor) const;
	bool IsOverlappingComponent(const FLuaComponentHandle& OtherComponent) const;

	bool IsUIComponent() const;
	bool SetUIRenderSpace(const FString& RenderSpace) const;
	bool SetUITexturePath(const FString& TexturePath) const;
	bool SetUIAnchor(const sol::object& Value) const;
	bool SetUIAnchorMin(const sol::object& Value) const;
	bool SetUIAnchorMax(const sol::object& Value) const;
	sol::table GetUIAnchorMin(sol::this_state State) const;
	sol::table GetUIAnchorMax(sol::this_state State) const;
	bool SetUIAnchoredPosition(const sol::object& Value) const;
	sol::table GetUIAnchoredPosition(sol::this_state State) const;
	bool SetUISizeDelta(const sol::object& Value) const;
	sol::table GetUISizeDelta(sol::this_state State) const;
	bool SetUIWorldSize(const sol::object& Value) const;
	bool SetUIBillboard(bool bBillboard) const;
	bool SetUIPivot(const sol::object& Value) const;
	sol::table GetUIPivot(sol::this_state State) const;
	bool SetUIRotationDegrees(float Degrees) const;
	float GetUIRotationDegrees() const;
	bool SetUITint(float R, float G, float B, float A) const;
	sol::table GetUITint(sol::this_state State) const;
	bool SetUIVisibility(bool bVisible) const;
	bool SetUIHitTestVisible(bool bHitTestVisible) const;
	bool SetUILayer(int32 Layer) const;
	bool SetUIZOrder(int32 ZOrder) const;

	bool SetStaticMesh(const FString& MeshPath) const;
	bool SetMaterial(int32 Index, const FString& MaterialPath) const;
	bool SetMaterialVector4Parameter(int32 Index, const FString& ParamName, const sol::table& Value) const;

	bool SetIntensity(float Intensity) const;

	bool IsUIText() const;
	FString GetUIText() const;
	bool SetUIText(const FString& Text) const;
	bool SetUIFont(const FString& FontName) const;
	bool SetUIFontSize(float FontSize) const;
	bool SetUITextHorizontalAlignment(const FString& Alignment) const;
	bool SetUITextVerticalAlignment(const FString& Alignment) const;

	bool IsUIButton() const;
	bool IsButtonInteractable() const;
	bool SetButtonInteractable(bool bInteractable) const;
	bool IsButtonHovered() const;
	bool IsButtonPressed() const;
	int32 GetButtonClickCount() const;
};

struct FLuaCollider2DHandle : public FLuaComponentHandle
{
	using FLuaComponentHandle::FLuaComponentHandle;

	UCollider2DComponent* ResolveCollider() const;

	// Common Collider2D methods
	sol::table GetShapeWorldLocation2D(sol::this_state State) const;
	float GetCollisionPlaneZ() const;
};

struct FLuaBoxCollider2DHandle : public FLuaCollider2DHandle
{
	using FLuaCollider2DHandle::FLuaCollider2DHandle;

	sol::table GetBoxExtent(sol::this_state State) const;
	bool SetBoxExtent(const sol::object& Value) const;
};

struct FLuaCircleCollider2DHandle : public FLuaCollider2DHandle
{
	using FLuaCollider2DHandle::FLuaCollider2DHandle;

	float GetRadius() const;
	bool SetRadius(float Radius) const;
};

class FActorPoolManager;

struct FLuaActorPoolManagerHandle
{
	FActorPoolManager* Manager = nullptr;

	FLuaActorPoolManagerHandle() = default;
	explicit FLuaActorPoolManagerHandle(FActorPoolManager* InManager);

	bool IsValid() const;

	void Warmup(const FString& ClassName, uint32 Count) const;
	FLuaActorHandle Acquire(const FString& ClassName) const;
	void Release(const FLuaActorHandle& Actor) const;
	void ReleaseActiveByClass(const FString& ClassName) const;
	uint32 GetActiveCount(const FString& ClassName) const;
};

void RegisterLuaEngineBindings(sol::state& Lua);
