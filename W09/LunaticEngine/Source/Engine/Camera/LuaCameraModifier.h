#pragma once
#include "CameraModifier.h"

#include <memory>

class ULuaCameraModifier : public UCameraModifier
{
public:
	DECLARE_CLASS(ULuaCameraModifier, UCameraModifier)

	ULuaCameraModifier();

	// Function : Load Lua camera modifier script and call Begin(params)
	// input : InScriptPath, Params
	// InScriptPath : Lua modifier script path under Scripts
	// Params : numeric parameters passed to Lua Begin(params)
	bool Initialize(const FString& InScriptPath, const TMap<FString, float>& Params = {});

	// Function : Execute Lua Update(deltaTime, view) against final POV
	// input : DeltaTime, InOutPOV
	// DeltaTime : frame delta time passed to Lua
	// InOutPOV : final POV exposed to Lua as mutable camera view
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

	const FString& GetScriptPath() const { return ScriptPath; }
	const FString& GetLastError() const;

protected:
	~ULuaCameraModifier() override;

public:
	FString ScriptPath;

private:
	struct FLuaModifierImpl;
	std::unique_ptr<FLuaModifierImpl> Impl;
};
