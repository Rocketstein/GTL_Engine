#include "Engine/Runtime/Engine.h"

#include "Audio/AudioManager.h"
#include "Platform/Paths.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Profiling/Stats.h"
#include "Profiling/StartupProfiler.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/ResourceManager.h"
#include "Render/Pipeline/DefaultRenderPipeline.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Mesh/ObjManager.h"
#include "Texture/Texture2D.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "GameFramework/GameInstance.h"
#include "Object/ObjectFactory.h"
#include "Core/ProjectSettings.h"
#include "Core/Notification.h"

#include "Core/TickFunction.h"
#include "Collision/CollisionDispatcher.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"

DEFINE_CLASS(UEngine, UObject)

UEngine* GEngine = nullptr;


bool GIsEditor = false;

namespace
{
	ELevelTick ToLevelTickType(EWorldType WorldType)
	{
		switch (WorldType)
		{
		case EWorldType::Editor:
			return ELevelTick::LEVELTICK_ViewportsOnly;
		case EWorldType::PIE:
		case EWorldType::Game:
			return ELevelTick::LEVELTICK_All;
		default:
			return ELevelTick::LEVELTICK_TimeOnly;
		}
	}
}

void UEngine::Init(FWindowsWindow* InWindow)
{
	Window = InWindow;

	UE_LOG_CATEGORY(Engine, Info, "[INIT] UEngine::Init begin");

	// 싱글턴 초기화 순서 보장
	FNamePool::Get();
	FObjectFactory::Get();
	UE_LOG_CATEGORY(Engine, Info, "[INIT] Core singletons ready");

	// GameInstance를 만들기 전에 ProjectSettings 로드 — 에디터가 다시 로드해도 안전
	FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());
	UE_LOG_CATEGORY(Engine, Info, "[INIT] Reloaded project settings from %s", FProjectSettings::GetDefaultPath().c_str());

	const FString& GameInstanceClassName = FProjectSettings::Get().Game.GameInstanceClass;
	UE_LOG_CATEGORY(Engine, Info, "[INIT] Creating GameInstance class=%s", GameInstanceClassName.c_str());
	UObject* GameInstanceObj = FObjectFactory::Get().Create(GameInstanceClassName);
	GameInstance = Cast<UGameInstance>(GameInstanceObj);
	if (!GameInstance)
	{
		// 잘못된 클래스 이름이거나 등록 안된 경우 베이스로 폴백
		if (GameInstanceObj) UObjectManager::Get().DestroyObject(GameInstanceObj);
		GameInstance = UObjectManager::Get().CreateObject<UGameInstance>();
		UE_LOG_CATEGORY(Engine, Warning, "[INIT] GameInstance fallback to UGameInstance");
	}
	else
	{
		UE_LOG_CATEGORY(Engine, Info, "[INIT] GameInstance created successfully");
	}

	{
		SCOPE_STARTUP_STAT("Renderer::Create");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Renderer::Create begin");
		Renderer.Create(Window->GetHWND());
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Renderer::Create complete");
	}

	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();

	{
		SCOPE_STARTUP_STAT("MeshBufferManager::Init");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] MeshBufferManager::Init begin");
		FMeshBufferManager::Get().Initialize(Device);
		UE_LOG_CATEGORY(Engine, Info, "[INIT] MeshBufferManager::Init complete");
	}

	{
		SCOPE_STARTUP_STAT("ResourceManager::LoadDefaultResources");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Loading default resources");
		FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::DefaultContentResourceFilePath()), Device);
	}

	{
		SCOPE_STARTUP_STAT("ResourceManager::LoadEditorResources");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Loading editor resources");
		FResourceManager::Get().LoadFromFile(FPaths::ToUtf8(FPaths::EditorResourceFilePath()), Device);
	}

	{
		SCOPE_STARTUP_STAT("ResourceManager::LoadProjectResources");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Loading project resources");
		FResourceManager::Get().LoadFromScanFile(FPaths::ToUtf8(FPaths::ProjectResourcePathsFilePath()), Device);
	}

	{
		SCOPE_STARTUP_STAT("ResourceManager::LoadFromDir");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] Scanning resource directory: %s", FPaths::ToUtf8(FPaths::RootDir()).c_str());
		FResourceManager::Get().LoadFromDirectory(FPaths::ToUtf8(FPaths::RootDir()), Device);
	}

	{
		SCOPE_STARTUP_STAT("RenderPipeline::Create");
		UE_LOG_CATEGORY(Engine, Info, "[INIT] RenderPipeline::Create begin");
		SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
		UE_LOG_CATEGORY(Engine, Info, "[INIT] RenderPipeline::Create complete");
	}
	UE_LOG_CATEGORY(Engine, Info, "[INIT] GameInstance::Init begin");
	GameInstance->Init();
	UE_LOG_CATEGORY(Engine, Info, "[INIT] GameInstance::Init complete");

	UE_LOG_CATEGORY(Engine, Info, "[INIT] FDirectoryWatcher::Initialize begin");
	FDirectoryWatcher::Get().Initialize();
	UE_LOG_CATEGORY(Engine, Info, "[INIT] FDirectoryWatcher::Initialize complete");
	UE_LOG_CATEGORY(Engine, Info, "[INIT] FCollisionDispatcher::Init begin");
	FCollisionDispatcher::Get().Init();
	UE_LOG_CATEGORY(Engine, Info, "[INIT] FCollisionDispatcher::Init complete");
	UE_LOG_CATEGORY(Engine, Info, "[INIT] UEngine::Init complete");
}

void UEngine::Shutdown()
{
	FAudioManager::Get().Shutdown();

	if (GameInstance)
	{
		GameInstance->Shutdown();
		UObjectManager::Get().DestroyObject(GameInstance);
		GameInstance = nullptr;
	}

	FDirectoryWatcher::Get().Shutdown();
	FLogManager::Get().Shutdown();
	RenderPipeline.reset();
	FResourceManager::Get().ReleaseGPUResources();
	UTexture2D::ReleaseAllGPU();
	FObjManager::ReleaseAllGPU();
	FMeshBufferManager::Get().Release();
	Renderer.Release();
}

void UEngine::BeginPlay()
{
	FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	if (Context && Context->World)
	{
		if (Context->WorldType == EWorldType::Game || Context->WorldType == EWorldType::PIE)
		{
			Context->World->BeginPlay();
		}
	}
}

void UEngine::Tick(float DeltaTime)
{
	if (!PendingSceneLoadReference.empty())
	{
		const FString SceneToLoad = PendingSceneLoadReference;
		PendingSceneLoadReference.clear();
		LoadScene(SceneToLoad);
	}

	FDirectoryWatcher::Get().ProcessChanges();
	FNotificationManager::Get().Tick(DeltaTime);
	FAudioManager::Get().Update();
	WorldTick(DeltaTime);
	Render(DeltaTime);
}

bool UEngine::RequestSceneLoad(const FString& InSceneReference)
{
	if (InSceneReference.empty())
	{
		return false;
	}

	PendingSceneLoadReference = InSceneReference;
	UE_LOG_CATEGORY(Engine, Info, "[SceneLoad] Queued scene load: %s", InSceneReference.c_str());
	FNotificationManager::Get().AddNotification("Queued scene load: " + InSceneReference, ENotificationType::Info, 1.5f);
	return true;
}

void UEngine::Render(float DeltaTime)
{
	if (RenderPipeline)
	{
		SCOPE_STAT_CAT("UEngine::Render", "2_Render");
		RenderPipeline->Execute(DeltaTime, Renderer);
	}
}

void UEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> InPipeline)
{
	RenderPipeline = std::move(InPipeline);
}

void UEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	Renderer.GetFD3DDevice().OnResizeViewport(Width, Height);
	Renderer.ResetRenderStateCache();

	// Standalone Game/Shipping 모드에서는 윈도우 크기에 맞춰 오프스크린 뷰포트도 리사이즈해야 함.
	// 그렇지 않으면 CopyResource(백버퍼, 뷰포트RT) 단계에서 크기 불일치로 렌더링이 중단됨.
	if (!GIsEditor)
	{
		if (GameViewportClient && GameViewportClient->GetViewport())
		{
			GameViewportClient->GetViewport()->Resize(Width, Height);
		}

		if (UWorld* World = GetWorld())
		{
			if (UCameraComponent* Camera = World->GetActiveCamera())
			{
				Camera->OnResize(Width, Height);
			}
		}
	}
}

void UEngine::WorldTick(float DeltaTime)
{
	SCOPE_STAT_CAT("UEngine::WorldTick", "1_WorldTick");

	// PIE 활성 시 Editor 월드는 sleep (UE 동작과 동일).
	// culling/octree/visibility 갱신을 건너뛰어 50k+ 환경에서 비용 2배를 방지.
	bool bHasPIEWorld = false;
	for (const FWorldContext& Ctx : WorldList)
	{
		if (Ctx.WorldType == EWorldType::PIE && Ctx.World)
		{
			bHasPIEWorld = true;
			break;
		}
	}

	// 월드 타입별 Tick 라우팅:
	// - Editor: bTickInEditor 액터만 TickManager 대상
	// - PIE/Game: BeginPlay 이후 bNeedsTick 액터만 TickManager 대상
	// - 기타:   시간 갱신만 유지
	for (FWorldContext& Ctx : WorldList)
	{
		UWorld* World = Ctx.World;
		if (!World) continue;

		// PIE 활성 시 Editor 월드는 완전히 skip
		if (bHasPIEWorld && Ctx.WorldType == EWorldType::Editor)
		{
			continue;
		}

		ELevelTick TickType = ToLevelTickType(Ctx.WorldType);
		if (bGamePaused && (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game))
		{
			TickType = LEVELTICK_PauseTick;
		}

		// 월드 단위 업데이트 (FlushPrimitive / VisibleProxies / DebugDraw /s TickManager)
		World->Tick(DeltaTime, TickType);
	}
}

UWorld* UEngine::GetWorld() const
{
	const FWorldContext* Context = GetWorldContextFromHandle(ActiveWorldHandle);
	return Context ? Context->World : nullptr;
}

FWorldContext& UEngine::CreateWorldContext(EWorldType Type, const FName& Handle, const FString& Name)
{
	FWorldContext Context;
	Context.WorldType = Type;
	Context.ContextHandle = Handle;
	Context.ContextName = Name.empty() ? Handle.ToString() : Name;
	Context.World = UObjectManager::Get().CreateObject<UWorld>();
	WorldList.push_back(Context);
	return WorldList.back();
}

void UEngine::DestroyWorldContext(const FName& Handle)
{
	for (auto it = WorldList.begin(); it != WorldList.end(); ++it)
	{
		if (it->ContextHandle == Handle)
		{
			it->World->EndPlay();
			UObjectManager::Get().DestroyObject(it->World);
			WorldList.erase(it);
			return;
		}
	}
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName& Handle) const
{
	for (const FWorldContext& Ctx : WorldList)
	{
		if (Ctx.ContextHandle == Handle)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* World)
{
	for (FWorldContext& Ctx : WorldList)
	{
		if (Ctx.World == World)
		{
			return &Ctx;
		}
	}
	return nullptr;
}

void UEngine::SetActiveWorld(const FName& Handle)
{
	ActiveWorldHandle = Handle;
}
