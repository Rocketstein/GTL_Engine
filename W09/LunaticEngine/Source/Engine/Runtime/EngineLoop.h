#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Profiling/Timer.h"
#include "Core/CoreTypes.h"
#include <mmsystem.h>

#pragma comment(lib, "Winmm.lib")
class FEngineLoop
{
public:
	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	int RunCookOnly(const FString& OutputSceneRoot = "");
	void Shutdown();

private:
	void CreateEngine();

private:
	FWindowsApplication Application;
	FTimer Timer;
};
