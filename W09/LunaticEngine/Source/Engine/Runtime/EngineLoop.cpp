#include "Engine/Runtime/EngineLoop.h"
#include "Core/Log.h"
#include "Core/ProjectSettings.h"
#include "Profiling/StartupProfiler.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Input/InputManager.h"
#include <iostream>
#include <objbase.h>
#include <wincodec.h>

#if IS_OBJ_VIEWER
#include "ObjViewer/ObjViewerEngine.h"
#elif WITH_EDITOR
#include "Editor/EditorEngine.h"
#else
#include "Engine/Runtime/GameEngine.h"
#endif

void FEngineLoop::CreateEngine()
{
#if IS_OBJ_VIEWER
	GEngine = UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
	GEngine = UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	GEngine = UObjectManager::Get().CreateObject<UGameEngine>();
#endif
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	FLogManager::Get().Initialize();

	// WIC inproc factory는 MTA에서 가장 안정적으로 동작한다.
	HRESULT ComInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(ComInitHr) && ComInitHr != RPC_E_CHANGED_MODE)
	{
		UE_LOG_CATEGORY(EngineInit, Warning, "[INIT] CoInitializeEx failed: 0x%08X", static_cast<unsigned int>(ComInitHr));
	}

	// WindowsCodecs.dll을 미리 로드해 두어 lazy-load 실패로 인한 WIC 팩토리 vtable 손상을 차단.
	if (HMODULE WicModule = LoadLibraryW(L"WindowsCodecs.dll"))
	{
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] WindowsCodecs.dll loaded at 0x%p", WicModule);
	}
	else
	{
		UE_LOG_CATEGORY(EngineInit, Error, "[INIT] WindowsCodecs.dll load failed: GLE=%lu", GetLastError());
	}

	// WIC 팩토리 사전 생성: 초기 실패는 명시적 로그로 노출.
	{
		IWICImagingFactory* WicFactory = nullptr;
		HRESULT WicHr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&WicFactory));
		if (SUCCEEDED(WicHr) && WicFactory)
		{
			UE_LOG_CATEGORY(EngineInit, Info, "[INIT] WIC ImagingFactory pre-warmed");
			WicFactory->Release();
		}
		else
		{
			UE_LOG_CATEGORY(EngineInit, Error, "[INIT] WIC ImagingFactory creation failed: 0x%08X", static_cast<unsigned int>(WicHr));
		}
	}

	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine loop init begin");
	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Loading project settings: %s", FProjectSettings::GetDefaultPath().c_str());
	FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());
	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Project settings loaded");

	{
		SCOPE_STARTUP_STAT("WindowsApplication::Init");
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] WindowsApplication::Init begin");
		if (!Application.Init(hInstance))
		{
			UE_LOG_CATEGORY(EngineInit, Error, "[INIT] WindowsApplication::Init failed");
			return false;
		}
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] WindowsApplication::Init complete");
	}

	Application.SetOnSizingCallback([this]()
		{
			Timer.Tick();
			GEngine->Tick(Timer.GetDeltaTime());
		});

	Application.SetOnResizedCallback([](unsigned int Width, unsigned int Height)
		{
			if (GEngine)
			{
				GEngine->OnWindowResized(Width, Height);
			}
		});

	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Creating engine instance");
	CreateEngine();
	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine instance created");

	{
		SCOPE_STARTUP_STAT("Engine::Init");
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine::Init begin");
		GEngine->Init(&Application.GetWindow());
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine::Init complete");
	}

	GEngine->SetTimer(&Timer);
	Timer.SetMaxFPS(
		FProjectSettings::Get().Performance.bLimitFPS
			? static_cast<float>(FProjectSettings::Get().Performance.MaxFPS)
			: 0.0f);

	{
		SCOPE_STARTUP_STAT("Engine::BeginPlay");
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine::BeginPlay begin");
		GEngine->BeginPlay();
		UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine::BeginPlay complete");
	}

	Timer.Initialize();
	FStartupProfiler::Get().Finish();
	UE_LOG_CATEGORY(EngineInit, Info, "[INIT] Engine loop init complete");

	return true;
}

int FEngineLoop::RunCookOnly(const FString& OutputSceneRoot)
{
	const int32 Cooked = FSceneSaveManager::CookAllScenes(OutputSceneRoot);
	std::cerr << "[Cook] Total cooked: " << Cooked << " scenes" << std::endl;
	return Cooked > 0 ? 0 : 1;
}

int FEngineLoop::Run()
{
	timeBeginPeriod(1);
	while (!Application.IsExitRequested())
	{
		Application.PumpMessages();

		if (Application.IsExitRequested())
		{
			break;
		}

		FInputManager::Get().Tick();

//		if (FInputManager::Get().IsKeyPressed(VK_ESCAPE))
//		{
//#if WITH_EDITOR
//			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
//			{
//				if (EditorEngine->IsPlayingInEditor())
//				{
//					EditorEngine->RequestEndPlayMap();
//				}
//			}
//#else
//			Application.RequestExit();
//#endif
//		}

		Timer.Tick();
		float DeltaTime = Timer.GetDeltaTime();
		GEngine->Tick(DeltaTime);

		const float MaxFPS = Timer.GetMaxFPS();
		const float TargetDeltaTime = MaxFPS > 0.0f ? (1.0f / MaxFPS) : 0.0f;
		float FrameProcessTime = Timer.GetTimeSinceLastTick();
		if (TargetDeltaTime > 0.0f && FrameProcessTime < TargetDeltaTime)
		{
			float RemainingTimeMS = (TargetDeltaTime - FrameProcessTime) * 1000.f;
			if (RemainingTimeMS > 2.0f)
			{
				Sleep(static_cast<DWORD>(RemainingTimeMS -1.f));
			}
			while (Timer.GetTimeSinceLastTick() < TargetDeltaTime)
			{
				_mm_pause();
			}
		}

	}
	timeEndPeriod(1);
	return 0;
}

void FEngineLoop::Shutdown()
{
	if (GEngine)
	{
		GEngine->Shutdown();
		UObjectManager::Get().DestroyObject(GEngine);
		GEngine = nullptr;
	}

	CoUninitialize();
}
