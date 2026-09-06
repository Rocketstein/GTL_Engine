#include "Editor/EditorEngine.h"

#include "Audio/AudioManager.h"
#include "Profiling/StartupProfiler.h"
#include "Core/Notification.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Editor/History/SceneHistoryBuilder.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/World.h"
#include "Viewport/GameViewportClient.h"
#include "Editor/EditorRenderPipeline.h"
#include "Editor/UI/EditorFileUtils.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Core/ProjectSettings.h"
#include "Engine/Input/InputManager.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Engine/Platform/Paths.h"
#include "Core/AsciiUtils.h"
#include "Texture/Texture2D.h"
#include "Object/Object.h"
#include <filesystem>
#include <fstream>
#include <set>

IMPLEMENT_CLASS(UEditorEngine, UEngine)

namespace
{
bool EndsWithIgnoreCase(const FString& Value, const char* Suffix)
{
	if (!Suffix)
	{
		return false;
	}

	const FString SuffixString = Suffix;
	if (Value.size() < SuffixString.size())
	{
		return false;
	}

	for (size_t Index = 0; Index < SuffixString.size(); ++Index)
	{
		const char Left = AsciiUtils::ToLower(Value[Value.size() - SuffixString.size() + Index]);
		const char Right = AsciiUtils::ToLower(SuffixString[Index]);
		if (Left != Right)
		{
			return false;
		}
	}

	return true;
}

FString BuildScenePathFromStem(const FString& InStem)
{
	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(InStem) + FSceneSaveManager::SceneExtension);
	return FPaths::ToUtf8(ScenePath.wstring());
}

FString GetFileStem(const FString& InPath)
{
	const std::filesystem::path Path(FPaths::ToWide(InPath));
	return FPaths::ToUtf8(Path.stem().wstring());
}

UCameraComponent* FindFirstCameraComponent(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (UCameraComponent* Camera = Cast<UCameraComponent>(Comp))
			{
				return Camera;
			}
		}
	}

	return nullptr;
}

UCameraComponent* EnsurePIEActiveCamera(UWorld* World, const FPerspectiveCameraData& CameraData)
{
	if (!World)
	{
		return nullptr;
	}

	if (UCameraComponent* ActiveCamera = World->GetActiveCamera())
	{
		if (IsAliveObject(ActiveCamera) && ActiveCamera->GetWorld() == World)
		{
			return ActiveCamera;
		}
		World->SetActiveCamera(nullptr);
	}

	if (UCameraComponent* SceneCamera = FindFirstCameraComponent(World))
	{
		World->SetActiveCamera(SceneCamera);
		return SceneCamera;
	}

	AActor* CamActor = World->SpawnActor<AActor>();
	if (!CamActor)
	{
		return nullptr;
	}

	CamActor->SetFName(FName("DefaultPIECamera"));
	UCameraComponent* Cam = CamActor->AddComponent<UCameraComponent>();
	CamActor->SetRootComponent(Cam);
	if (CameraData.bValid)
	{
		Cam->SetRelativeLocation(CameraData.Location);
		Cam->SetRelativeRotation(CameraData.Rotation);
	}
	else
	{
		Cam->SetRelativeLocation(FVector(0.0f, -10.0f, 5.0f));
		Cam->SetRelativeRotation(FVector(0.0f, -25.0f, 90.0f));
	}

	World->SetActiveCamera(Cam);
	return Cam;
}

bool ImportAssetFile(
	const FString& SourcePath,
	const std::wstring& RelativeDestinationDir,
	FString& OutImportedRelativePath)
{
	if (SourcePath.empty())
	{
		return false;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path SourceAbsolute = std::filesystem::path(FPaths::ToWide(SourcePath)).lexically_normal();
	if (!std::filesystem::exists(SourceAbsolute))
	{
		return false;
	}

	const std::filesystem::path RelativeToRoot = SourceAbsolute.lexically_relative(ProjectRoot);
	const bool bAlreadyInProject = !RelativeToRoot.empty() && RelativeToRoot.native().find(L"..") != 0;
	if (bAlreadyInProject)
	{
		OutImportedRelativePath = FPaths::ToUtf8(RelativeToRoot.generic_wstring());
		return true;
	}

	const std::filesystem::path DestinationDir = ProjectRoot / RelativeDestinationDir;
	FPaths::CreateDir(DestinationDir.wstring());

	std::filesystem::path DestinationPath = DestinationDir / SourceAbsolute.filename();
	int32 Suffix = 1;
	while (std::filesystem::exists(DestinationPath))
	{
		DestinationPath = DestinationDir
			/ (SourceAbsolute.stem().wstring() + L"_" + std::to_wstring(Suffix++) + SourceAbsolute.extension().wstring());
	}

	std::filesystem::copy_file(SourceAbsolute, DestinationPath, std::filesystem::copy_options::overwrite_existing);
	OutImportedRelativePath = FPaths::ToUtf8(DestinationPath.lexically_relative(ProjectRoot).generic_wstring());
	return true;
}

}

void UEditorEngine::Init(FWindowsWindow* InWindow)
{
	
	GIsEditor = true;

	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);

	if (InWindow)
	{
		FInputManager::Get().SetOwnerWindow(InWindow->GetHWND());
	}

	{
		SCOPE_STARTUP_STAT("ObjManager::ScanMeshAssets");
		FObjManager::ScanMeshAssets();
	}

	{
		SCOPE_STARTUP_STAT("MaterialManager::ScanAssets");
		FMaterialManager::Get().ScanMaterialAssets();
	}
	UTexture2D::ScanTextureAssets();

	// 에디터 전용 초기화
	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());
	FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

	{
		SCOPE_STARTUP_STAT("EditorMainPanel::Create");
		MainPanel.Create(Window, Renderer, this);
	}

	// 기본 월드 생성 — 모든 서브시스템 초기화의 기반
	CreateWorldContext(EWorldType::Editor, FName("Default"));
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// Selection & Gizmo
	SelectionManager.Init();
	SelectionManager.SetWorld(GetWorld());

	// 뷰포트 레이아웃 초기화 + 저장된 설정 복원
	ViewportLayout.Initialize(this, Window, Renderer, &SelectionManager);
	ViewportLayout.LoadFromSettings();

	{
		SCOPE_STARTUP_STAT("Editor::LoadStartLevel");
		LoadStartLevel();
	}
	ApplyTransformSettingsToGizmo();

	// Editor render pipeline
	{
		SCOPE_STARTUP_STAT("EditorRenderPipeline::Create");
		SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
	}
}

void UEditorEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	ViewportLayout.SaveToSettings();
	MainPanel.SaveToSettings();
	FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	CloseScene();
	SelectionManager.Shutdown();
	MainPanel.Release();

	// 뷰포트 레이아웃 해제
	ViewportLayout.Release();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	// 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
	// FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
	if (!PendingSceneLoadReference.empty())
	{
		const FString SceneToLoad = PendingSceneLoadReference;
		PendingSceneLoadReference.clear();
		LoadScene(SceneToLoad);
	}
	// --- PIE 요청 처리 (프레임 경계에서 처리되도록 Tick 선두에서 소비) ---
	if (bRequestEndPlayMapQueued)
	{
		bRequestEndPlayMapQueued = false;
		EndPlayMap();
	}
	if (PlaySessionRequest.has_value())
	{
		StartQueuedPlaySessionRequest();
	}
	ProcessDeferredEditorActions();

	ApplyTransformSettingsToGizmo();
	FDirectoryWatcher::Get().ProcessChanges();
	if (UTexture2D::HasPendingTextureRefresh())
	{
		UTexture2D::RefreshChangedTextures(Renderer.GetFD3DDevice().GetDevice());
	}
	FNotificationManager::Get().Tick(DeltaTime);
	FAudioManager::Get().Update();
	MainPanel.Update();

	for (FEditorViewportClient* VC : ViewportLayout.GetAllViewportClients())
	{
		VC->Tick(DeltaTime);
	}

	WorldTick(DeltaTime);
	Render(DeltaTime);

	if (!IsPIEPossessedMode())
	{
		SelectionManager.Tick();
	}
}

bool UEditorEngine::LoadScene(const FString& InSceneReference)
{
	if (!IsPlayingInEditor() || InSceneReference.empty())
	{
		UE_LOG_CATEGORY(EditorEngine, Warning, "[SceneLoad] Ignored PIE load request. IsPlayingInEditor=%d Scene=%s", IsPlayingInEditor() ? 1 : 0, InSceneReference.c_str());
		return false;
	}

	std::filesystem::path ChosenPath;
	const std::filesystem::path RawPath = FPaths::ToWide(InSceneReference);
	const std::filesystem::path SceneDir = FSceneSaveManager::GetSceneDirectory();

	auto TrySetChosenPath = [&ChosenPath](const std::filesystem::path& Candidate)
	{
		if (!Candidate.empty() && std::filesystem::exists(Candidate))
		{
			ChosenPath = Candidate;
			return true;
		}
		return false;
	};

	if (RawPath.is_absolute())
	{
		TrySetChosenPath(RawPath);
	}
	else
	{
		TrySetChosenPath(RawPath);
		if (ChosenPath.empty())
		{
			TrySetChosenPath(SceneDir / RawPath);
		}
	}

	if (ChosenPath.empty())
	{
		const bool bHasSceneExtension = EndsWithIgnoreCase(InSceneReference, ".scene");
		const bool bHasUmapExtension = EndsWithIgnoreCase(InSceneReference, ".umap");
		if (bHasSceneExtension || bHasUmapExtension)
		{
			const std::filesystem::path FileName = RawPath.filename();
			if (!TrySetChosenPath(SceneDir / FileName))
			{
				UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Failed to resolve scene path from reference: %s", InSceneReference.c_str());
				FNotificationManager::Get().AddNotification("Scene load failed: " + InSceneReference, ENotificationType::Error, 3.0f);
				return false;
			}
		}
		else
		{
			const std::wstring StemW = FPaths::ToWide(InSceneReference);
			if (!TrySetChosenPath(SceneDir / (StemW + L".umap")))
			{
				if (!TrySetChosenPath(SceneDir / (StemW + FSceneSaveManager::SceneExtension)))
				{
					UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Failed to find scene file for reference: %s", InSceneReference.c_str());
					FNotificationManager::Get().AddNotification("Scene not found: " + InSceneReference, ENotificationType::Error, 3.0f);
					return false;
				}
			}
		}
	}

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context)
	{
		UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] No active world context for handle: %s", GetActiveWorldHandle().ToString().c_str());
		return false;
	}

	UE_LOG_CATEGORY(EditorEngine, Info, "[SceneLoad] Loading PIE scene '%s' from '%s'", InSceneReference.c_str(), FPaths::ToUtf8(ChosenPath.wstring()).c_str());

	if (IRenderPipeline* Pipeline = GetRenderPipeline())
	{
		Pipeline->OnSceneCleared();
	}

	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(nullptr);

	if (UGameViewportClient* PIEViewportClient = GetGameViewportClient())
	{
		PIEViewportClient->UnPossess();
	}

	if (Context->World)
	{
		Context->World->EndPlay();
		UObjectManager::Get().DestroyObject(Context->World);
		Context->World = nullptr;
	}

	FPerspectiveCameraData DummyCamera;
	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());
	if (EndsWithIgnoreCase(FilePath, ".umap"))
	{
		Context->World = UObjectManager::Get().CreateObject<UWorld>();
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
		Context->WorldType = EWorldType::PIE;
		Context->ContextName = RawPath.stem().empty() ? "PIE" : FPaths::ToUtf8(RawPath.stem().wstring());
		Context->ContextHandle = GetActiveWorldHandle();
	}
	else
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
		Context->WorldType = EWorldType::PIE;
		Context->ContextHandle = GetActiveWorldHandle();
	}

	SetActiveWorld(Context->ContextHandle);

	if (!Context->World)
	{
		UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Context world is null after loading '%s'", InSceneReference.c_str());
		FNotificationManager::Get().AddNotification("Scene load failed: " + InSceneReference, ENotificationType::Error, 3.0f);
		return false;
	}

	Context->World->SetWorldType(EWorldType::PIE);
	SelectionManager.SetWorld(Context->World);
	Context->World->WarmupPickingData();
	EnsurePIEActiveCamera(Context->World, DummyCamera);
	if (!Context->World->HasBegunPlay())
	{
		Context->World->BeginPlay();
	}
	EnsurePIEActiveCamera(Context->World, DummyCamera);

	if (UGameViewportClient* PIEViewportClient = GetGameViewportClient())
	{
		if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
		{
			PIEViewportClient->SetViewport(ActiveVC->GetViewport());
			PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
		}

		if (UCameraComponent* GameCamera = EnsurePIEActiveCamera(Context->World, DummyCamera))
		{
			PIEViewportClient->Possess(GameCamera);
		}
	}

	FNotificationManager::Get().AddNotification("Loaded scene: " + InSceneReference, ENotificationType::Success, 2.0f);
	UE_LOG_CATEGORY(EditorEngine, Info, "[SceneLoad] Loaded PIE scene successfully: %s", InSceneReference.c_str());
	return true;
}

UCameraComponent* UEditorEngine::GetCamera() const
{
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		return ActiveVC->GetCamera();
	}
	return nullptr;
}

bool UEditorEngine::FocusActorInViewport(AActor* Actor)
{
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		return ActiveVC->FocusActor(Actor);
	}
	return false;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

void UEditorEngine::RenderPIEOverlayPopups()
{
	if (!IsPlayingInEditor())
	{
		return;
	}

	const FRect* AnchorRect = nullptr;
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		AnchorRect = &ActiveVC->GetViewportScreenRect();
	}

	PIEOverlay.RenderWithinCurrentFrame(AnchorRect);
}

void UEditorEngine::OpenScoreSavePopup(int32 InScore)
{
	PIEOverlay.OpenScoreSavePopup(InScore);
}

bool UEditorEngine::ConsumeScoreSavePopupResult(FString& OutNickname)
{
	return PIEOverlay.ConsumeScoreSavePopupResult(OutNickname);
}

void UEditorEngine::OpenMessagePopup(const FString& InMessage)
{
	PIEOverlay.OpenMessagePopup(InMessage);
}

bool UEditorEngine::ConsumeMessagePopupConfirmed()
{
	return PIEOverlay.ConsumeMessagePopupConfirmed();
}

void UEditorEngine::OpenScoreboardPopup(const FString& InFilePath)
{
	PIEOverlay.OpenScoreboardPopup(InFilePath);
}

void UEditorEngine::OpenTitleOptionsPopup()
{
	PIEOverlay.OpenTitleOptionsPopup();
}

void UEditorEngine::OpenTitleCreditsPopup()
{
	PIEOverlay.OpenTitleCreditsPopup();
}

bool UEditorEngine::IsScoreSavePopupOpen() const
{
	return PIEOverlay.IsScoreSavePopupOpen();
}

void UEditorEngine::ToggleCoordSystem()
{
	FEditorSettings& Settings = FEditorSettings::Get();
	Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World)
		? EEditorCoordSystem::Local
		: EEditorCoordSystem::World;
	ApplyTransformSettingsToGizmo();
}

void UEditorEngine::ApplyTransformSettingsToGizmo()
{
	UGizmoComponent* Gizmo = GetGizmo();
	if (!Gizmo)
	{
		return;
	}

	const FEditorSettings& Settings = FEditorSettings::Get();
	const bool bForceLocalForScale = Gizmo->GetMode() == EGizmoMode::Scale;
	Gizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
	// 에디터 설정의 좌표계/스냅 값을 매 프레임 Gizmo 상태와 동기화한다.
	Gizmo->SetSnapSettings(
		Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
		Settings.bEnableRotationSnap, Settings.RotationSnapSize,
		Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
}

// ─── PIE (Play In Editor) ────────────────────────────────
// UE 패턴 요약: Request는 단일 슬롯(std::optional)에 저장만 하고 즉시 실행하지 않는다.
// 실제 StartPIE는 다음 Tick 선두의 StartQueuedPlaySessionRequest에서 일어난다.
// 이유는 UI 콜백/트랜잭션 도중 같은 불안정한 타이밍을 피하기 위함.

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
	// 동시 요청은 UE와 동일하게 덮어쓴다 (진짜 큐 아님 — 단일 슬롯).
	PlaySessionRequest = InParams;
}

void UEditorEngine::CancelRequestPlaySession()
{
	PlaySessionRequest.reset();
}

void UEditorEngine::RequestEndPlayMap()
{
	if (!PlayInEditorSessionInfo.has_value())
	{
		return;
	}
	bRequestEndPlayMapQueued = true;
}

void UEditorEngine::StartQueuedPlaySessionRequest()
{
	if (!PlaySessionRequest.has_value())
	{
		return;
	}

	const FRequestPlaySessionParams Params = *PlaySessionRequest;
	PlaySessionRequest.reset();

	// 이미 PIE 중이면 기존 세션을 정리 후 새로 시작 (단순화).
	if (PlayInEditorSessionInfo.has_value())
	{
		EndPlayMap();
	}

	switch (Params.SessionDestination)
	{
	case EPIESessionDestination::InProcess:
		StartPlayInEditorSession(Params);
		break;
	}
}

void UEditorEngine::StartPlayInEditorSession(const FRequestPlaySessionParams& Params)
{
	SetGamePaused(false);
	FInputManager::Get().ResetAllKeyStates();
	FAudioManager::Get().StopAll();

	// 1) 현재 에디터 월드를 복제해 PIE 월드 생성 (UE의 CreatePIEWorldByDuplication 대응).
	UWorld* EditorWorld = GetWorld();
	if (!EditorWorld)
	{
		return;
	}
	// DuplicateAs(PIE)로 복제하면 Actor 복제 전에 WorldType이 설정되어
	// EditorOnly 컴포넌트의 프록시가 아예 생성되지 않음.
	UWorld* PIEWorld = EditorWorld->DuplicateAs(EWorldType::PIE);
	if (!PIEWorld)
	{
		return;
	}

	// 2) PIE WorldContext를 WorldList에 등록.
	FWorldContext Ctx;
	Ctx.WorldType = EWorldType::PIE;
	Ctx.ContextHandle = FName("PIE");
	Ctx.ContextName = "PIE";
	Ctx.World = PIEWorld;
	WorldList.push_back(Ctx);

	// 3) 세션 정보 기록 (이전 활성 핸들 포함 — EndPlayMap에서 복원).
	FPlayInEditorSessionInfo Info;
	Info.OriginalRequestParams = Params;
	Info.PIEStartTime = 0.0;
	Info.PreviousActiveWorldHandle = GetActiveWorldHandle();
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
		{
			Info.SavedViewportCamera.Location = VCCamera->GetWorldLocation();
			Info.SavedViewportCamera.Rotation = VCCamera->GetRelativeRotation();
			Info.SavedViewportCamera.CameraState = VCCamera->GetCameraState();
			Info.SavedViewportCamera.bValid = true;
		}
	}
	PlayInEditorSessionInfo = Info;

	// 4) ActiveWorldHandle을 PIE로 전환 — 이후 GetWorld()는 PIE 월드를 반환.
	SetActiveWorld(FName("PIE"));

	// GPU Occlusion readback은 ProxyId 기반이라 월드가 갈리면 stale.
	// 이전 프레임 결과를 무효화해야 wrong-proxy hit 방지.
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
	{
		Pipeline->OnSceneCleared();
	}

	// 활성 뷰포트 카메라를 PIE 월드의 ActiveCamera로  설정 —
	//    LOD 갱신 등에서 ActiveCamera를 참조하므로 BeginPlay 전 placeholder가 필요.
	//    BeginPlay 이후에는 GameMode/PlayerController가 possess한 카메라가 있으면
	//    그 쪽으로 교체되고, 없으면 씬 안의 첫 CameraComponent로 교체된다 
	UCameraComponent* PlaceholderCamera = nullptr;
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
		{
			PlaceholderCamera = VCCamera;
			PIEWorld->SetActiveCamera(VCCamera);
		}
	}

	// 6) Selection을 PIE 월드 기준으로 재바인딩 — 에디터 액터를 가리킨 채로 두면
	//    픽킹(=PIE 월드) / outliner / outline 렌더가 모두 어긋난다.
	SelectionManager.ClearSelection();
	SelectionManager.SetGizmoEnabled(false); // PIE 중에는 에디터 gizmo를 숨긴다.
	SelectionManager.SetWorld(PIEWorld);

	if (!GetGameViewportClient())
	{
		UGameViewportClient* PIEViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
		SetGameViewportClient(PIEViewportClient);
	}
	if (UGameViewportClient* PIEViewportClient = GetGameViewportClient())
	{
		if (Window)
		{
			PIEViewportClient->SetOwnerWindow(Window->GetHWND());
		}
		UCameraComponent* InitialTargetCamera = PIEWorld->GetActiveCamera();
		FViewport* InitialViewport = nullptr;
		if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
		{
			InitialTargetCamera = ActiveVC->GetCamera() ? ActiveVC->GetCamera() : InitialTargetCamera;
			InitialViewport = ActiveVC->GetViewport();
			PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
		}
		PIEViewportClient->OnBeginPIE(InitialTargetCamera, InitialViewport);
	}
	EnterPIEPossessedMode();
	
	//이 코드와 대응되는 게 아래 EndPlayMap()에 있음.
	//MainPanel.HideEditorWindowsForPIE(); //PIE 중에는 에디터 패널을 숨김.
	//ViewportLayout.DisableWorldAxisForPIE(); //PIE 중에는 월드 축 렌더링을 비활성화.

	// 7) BeginPlay 트리거 — 모든 등록/바인딩이 끝난 다음 첫 Tick 이전에 호출.
	//    UWorld::BeginPlay가 bHasBegunPlay를 먼저 세팅하므로 BeginPlay 도중
	//    SpawnActor로 만든 신규 액터도 자동으로 BeginPlay된다.
	PIEWorld->BeginPlay();

	// GameMode/PlayerController가 ActiveCamera를 갱신하지 않은 경우
	//    (레벨에 GameMode가 지정되지 않은 Playground) 씬 안에 미리 배치된
	//    첫 CameraComponent를 찾아 ActiveCamera로 사용한다. GameEngine::LoadLevel과
	//    동일한 폴백 — Editor VC 카메라가 PIE에서 그대로 보이는 버그를 막기 위한 것
	if (PIEWorld->GetActiveCamera() == PlaceholderCamera)
	{
		UCameraComponent* SceneCamera = nullptr;
		for (AActor* Actor : PIEWorld->GetActors())
		{
			if (!Actor) continue;
			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
				{
					SceneCamera = Cam;
					break;
				}
			}
			if (SceneCamera) break;
		}
		if (SceneCamera)
		{
			PIEWorld->SetActiveCamera(SceneCamera);
		}
	}

	if (UGameViewportClient* PIEViewportClient = GetGameViewportClient())
	{
		if (UCameraComponent* GameCamera = PIEWorld->GetActiveCamera())
		{
			PIEViewportClient->Possess(GameCamera);
		}
	}
}

void UEditorEngine::EndPlayMap()
{
	SetGamePaused(false);
	FAudioManager::Get().StopAll();
	if (!PlayInEditorSessionInfo.has_value())
	{
		return;
	}

	// 활성 월드를 PIE 시작 전 핸들로 복원.
	const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
	SetActiveWorld(PrevHandle);

	// 복귀한 Editor 월드의 VisibleProxies/캐시된 카메라 상태를 강제 무효화.
	// PIE 중 Editor WorldTick이 skip되어 캐시가 PIE 시작 전 시점 그대로 남아 있고,
	// NeedsVisibleProxyRebuild()가 카메라 변화 기반이라 false를 반환하면 stale
	// VisibleProxies가 그대로 재사용되어 dangling proxy 참조로 크래시가 날 수 있다.
	//
	// 또한 Renderer::PerObjectCBPool은 ProxyId로 인덱싱되는 월드 간 공유 풀이라,
	// PIE 중 PIE 프록시가 덮어쓴 슬롯이 그대로 남아 있으면 Editor 프록시의
	// bPerObjectCBDirty=false 상태로 인해 업로드가 skip되어 PIE 마지막 transform으로
	// 렌더된다. 모든 Editor 프록시를 PerObjectCB dirty로 마킹해 재업로드 강제.
	if (UWorld* EditorWorld = GetWorld())
	{
		EditorWorld->GetScene().MarkAllPerObjectCBDirty();

		// ActiveCamera는 PIE 시작 시 PIE 월드로 옮겨졌고 PIE 월드와 함께 파괴됐다.
		// Editor 월드의 ActiveCamera는 여전히 그 dangling 포인터를 가리킬 수 있으므로
		// 활성 뷰포트의 카메라로 다시 바인딩해 줘야 frustum culling이 정상 동작한다.
		if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
		{
			if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
			{
				if (PlayInEditorSessionInfo->SavedViewportCamera.bValid)
				{
					const FPIEViewportCameraSnapshot& SavedCamera = PlayInEditorSessionInfo->SavedViewportCamera;
					VCCamera->SetWorldLocation(SavedCamera.Location);
					VCCamera->SetRelativeRotation(SavedCamera.Rotation);
					VCCamera->SetCameraState(SavedCamera.CameraState);
				}

				EditorWorld->SetActiveCamera(VCCamera);
			}
		}
	}

	// Selection을 에디터 월드로 복원 — PIE 액터는 곧 파괴되므로 먼저 비운다.
	SelectionManager.ClearSelection();
	SelectionManager.SetGizmoEnabled(true); // PIE 종료 후 에디터 gizmo 복원
	SelectionManager.SetWorld(GetWorld());
	
	//이 코드와 대응되는 게 위의 StartPlayInEditorSession()에 있음.
	//MainPanel.RestoreEditorWindowsAfterPIE();
	//ViewportLayout.RestoreWorldAxisAfterPIE();

	if (UGameViewportClient* PIEViewportClient = GetGameViewportClient())
	{
		PIEViewportClient->OnEndPIE();
		UObjectManager::Get().DestroyObject(PIEViewportClient);
		SetGameViewportClient(nullptr);
	}

	// PIE WorldContext 제거 (DestroyWorldContext가 EndPlay + DestroyObject 수행).
	DestroyWorldContext(FName("PIE"));

	// PIE 월드의 프록시가 모두 파괴됐으므로 GPU Occlusion readback 무효화.
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
	{
		Pipeline->OnSceneCleared();
	}

	PlayInEditorSessionInfo.reset();
	PIEControlMode = EPIEControlMode::Possessed;
	// InputSystem::Get().ResetCaptureStateForPIEEnd();
}

bool UEditorEngine::TogglePIEControlMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	if (PIEControlMode == EPIEControlMode::Possessed)
	{
		return EnterPIEEjectedMode();
	}
	return EnterPIEPossessedMode();
}

bool UEditorEngine::EnterPIEPossessedMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	PIEControlMode = EPIEControlMode::Possessed;
	SyncGameViewportPIEControlState(true);
	FInputManager::Get().ResetAllKeyStates();
	return true;
}

bool UEditorEngine::EnterPIEEjectedMode()
{
	if (!IsPlayingInEditor())
	{
		return false;
	}

	PIEControlMode = EPIEControlMode::Ejected;
	SyncGameViewportPIEControlState(false);
	FInputManager::Get().ResetAllKeyStates();
	return true;
}

void UEditorEngine::SyncGameViewportPIEControlState(bool bPossessedMode)
{
	UGameViewportClient* PIEViewportClient = GetGameViewportClient();
	if (!PIEViewportClient)
	{
		return;
	}

	PIEViewportClient->SetPIEPossessedInputEnabled(bPossessedMode);
	if (!bPossessedMode)
	{
		return;
	}

	if (Window)
	{
		PIEViewportClient->SetOwnerWindow(Window->GetHWND());
	}

	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		// PIEViewportClient->Possess(ActiveVC->GetCamera());
		PIEViewportClient->SetViewport(ActiveVC->GetViewport());
		PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
		// return;
	}
	// CameraComponent 우선 Possess 시도
	if (UWorld* World = GetWorld())
	{
		if (UCameraComponent* GameCamera = World->GetActiveCamera())
		{
			PIEViewportClient->Possess(GameCamera);
			return;
		}
	}
	// 이후 ViewportClient Possess 시도
	if (FLevelEditorViewportClient* ActiveVC = ViewportLayout.GetActiveViewport())
	{
		PIEViewportClient->Possess(ActiveVC->GetCamera());
	}
}

// ─── 기존 메서드 ──────────────────────────────────────────

void UEditorEngine::ResetViewport()
{
	ViewportLayout.ResetViewport(GetWorld());
}

void UEditorEngine::CloseScene()
{
	ClearScene();
}

void UEditorEngine::NewScene()
{
	StopPlayInEditorImmediate();
	ClearScene();
	FWorldContext& Ctx = CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
	Ctx.World->InitWorld();
	SetActiveWorld(Ctx.ContextHandle);
	SelectionManager.SetWorld(GetWorld());

	ResetViewport();
	CurrentLevelFilePath.clear();
}

void UEditorEngine::LoadStartLevel()
{
	const FString& StartLevel = FEditorSettings::Get().EditorStartLevel;
	if (StartLevel.empty())
	{
		return;
	}

	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(StartLevel) + FSceneSaveManager::SceneExtension);
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

	if (!LoadSceneFromPath(FilePath))
	{
		// 로드 실패 시 빈 씬으로 복구
		NewScene();
	}
}

void UEditorEngine::ClearScene()
{
	StopPlayInEditorImmediate();
	DestroyCurrentSceneWorlds(true, true);
}

void UEditorEngine::DestroyCurrentSceneWorlds(bool bClearHistory, bool bResetLevelPath)
{
	if (bClearHistory)
	{
		ClearTrackedTransformHistory();
	}

	SelectionManager.ClearSelection();
	SelectionManager.SetWorld(nullptr);

	// 씬 프록시 파괴 전 GPU Occlusion 스테이징 데이터 무효화
	if (IRenderPipeline* Pipeline = GetRenderPipeline())
		Pipeline->OnSceneCleared();

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}

	WorldList.clear();
	ActiveWorldHandle = FName::None;
	InvalidateTrackedSceneSnapshotCache();
	if (bResetLevelPath)
	{
		CurrentLevelFilePath.clear();
	}

	ViewportLayout.DestroyAllCameras();
}

void UEditorEngine::BeginTrackedSceneChange()
{
	if (bTrackingSceneChange || IsPlayingInEditor())
	{
		return;
	}

	FTrackedSceneSnapshot Snapshot;
	if (CachedTrackedSceneSnapshot.has_value())
	{
		Snapshot = *CachedTrackedSceneSnapshot;
	}
	else
	{
		Snapshot = FSceneHistoryBuilder::CaptureSnapshot(*this);
	}

	PendingTrackedSceneBefore = Snapshot;
	bTrackingSceneChange = true;
}

void UEditorEngine::CommitTrackedSceneChange()
{
	if (!bTrackingSceneChange || !PendingTrackedSceneBefore.has_value())
	{
		return;
	}

	const FTrackedSceneSnapshot Before = *PendingTrackedSceneBefore;
	const FTrackedSceneSnapshot After = FSceneHistoryBuilder::CaptureSnapshot(*this);

	PendingTrackedSceneBefore.reset();
	bTrackingSceneChange = false;
	CachedTrackedSceneSnapshot = After;

	if (!FSceneHistoryBuilder::HasMeaningfulDelta(Before, After))
	{
		return;
	}

	const FTrackedSceneChange Change = FSceneHistoryBuilder::BuildChange(Before, After);

	if (SceneHistoryCursor + 1 < static_cast<int32>(SceneHistory.size()))
	{
		SceneHistory.erase(SceneHistory.begin() + (SceneHistoryCursor + 1), SceneHistory.end());
	}

	SceneHistory.push_back(Change);
	if (SceneHistory.size() > 10)
	{
		SceneHistory.erase(SceneHistory.begin());
	}
	SceneHistoryCursor = static_cast<int32>(SceneHistory.size()) - 1;
}

void UEditorEngine::CancelTrackedSceneChange()
{
	PendingTrackedSceneBefore.reset();
	bTrackingSceneChange = false;
}

bool UEditorEngine::CanUndoSceneChange() const
{
	return SceneHistoryCursor >= 0 && SceneHistoryCursor < static_cast<int32>(SceneHistory.size());
}

bool UEditorEngine::CanRedoSceneChange() const
{
	return SceneHistoryCursor + 1 < static_cast<int32>(SceneHistory.size());
}

void UEditorEngine::UndoTrackedSceneChange()
{
	if (!CanUndoSceneChange())
	{
		return;
	}

	const FTrackedSceneChange& Change = SceneHistory[SceneHistoryCursor];
	ApplyTrackedSceneChange(Change, false);
	--SceneHistoryCursor;
}

void UEditorEngine::RedoTrackedSceneChange()
{
	if (!CanRedoSceneChange())
	{
		return;
	}

	const int32 RedoIndex = SceneHistoryCursor + 1;
	const FTrackedSceneChange& Change = SceneHistory[RedoIndex];
	ApplyTrackedSceneChange(Change, true);
	SceneHistoryCursor = RedoIndex;
}

void UEditorEngine::ClearTrackedTransformHistory()
{
	SceneHistory.clear();
	SceneHistoryCursor = -1;
	PendingTrackedSceneBefore.reset();
	CachedTrackedSceneSnapshot.reset();
	bTrackingSceneChange = false;
}

void UEditorEngine::BeginTrackedTransformChange()
{
	BeginTrackedSceneChange();
}

void UEditorEngine::CommitTrackedTransformChange()
{
	CommitTrackedSceneChange();
}

bool UEditorEngine::CanUndoTransformChange() const
{
	return CanUndoSceneChange();
}

bool UEditorEngine::CanRedoTransformChange() const
{
	return CanRedoSceneChange();
}

void UEditorEngine::UndoTrackedTransformChange()
{
	UndoTrackedSceneChange();
}

void UEditorEngine::RedoTrackedTransformChange()
{
	RedoTrackedSceneChange();
}

void UEditorEngine::ApplyTrackedSceneChange(const FTrackedSceneChange& Change, bool bRedo)
{
	SelectionManager.ClearSelection();
	ApplyTrackedActorDeltas(Change, bRedo);
	RestoreTrackedActorOrder(bRedo ? Change.AfterActorOrderUUIDs : Change.BeforeActorOrderUUIDs);
	RestoreTrackedFolderOrder(bRedo ? Change.AfterOutlinerFolders : Change.BeforeOutlinerFolders);
	if (UWorld* World = GetWorld())
	{
		World->WarmupPickingData();
	}
	RestoreViewportCamera(bRedo ? Change.AfterCameraData : Change.BeforeCameraData);

	TArray<uint32> PreferredSelection = FSceneHistoryBuilder::GetChangedActorUUIDs(Change, bRedo);
	if (PreferredSelection.empty())
	{
		PreferredSelection = bRedo ? Change.AfterSelectedActorUUIDs : Change.BeforeSelectedActorUUIDs;
	}
	RestoreTrackedSelection(PreferredSelection);
	CachedTrackedSceneSnapshot = FSceneHistoryBuilder::CaptureSnapshot(*this);
}

void UEditorEngine::ApplyTrackedActorDeltas(const FTrackedSceneChange& Change, bool bRedo)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bRedo)
	{
		for (const FDeletedActorDelta& Delta : Change.DeletedActors)
		{
			AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
			if (ExistingActor)
			{
				World->DestroyActor(ExistingActor);
			}
		}

		for (const FCreatedActorDelta& Delta : Change.CreatedActors)
		{
			AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
			if (!ExistingActor)
			{
				FSceneSaveManager::LoadActorFromJSONString(Delta.SerializedActor, World);
			}
		}

		for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
		{
			AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
			if (ExistingActor)
			{
				if (!FSceneSaveManager::ApplyActorFromJSONString(ExistingActor, Delta.AfterSerializedActor))
				{
					World->DestroyActor(ExistingActor);
					FSceneSaveManager::LoadActorFromJSONString(Delta.AfterSerializedActor, World);
				}
			}
			else
			{
				FSceneSaveManager::LoadActorFromJSONString(Delta.AfterSerializedActor, World);
			}
		}
		return;
	}

	for (const FCreatedActorDelta& Delta : Change.CreatedActors)
	{
		AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
		if (ExistingActor)
		{
			World->DestroyActor(ExistingActor);
		}
	}

	for (const FDeletedActorDelta& Delta : Change.DeletedActors)
	{
		AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
		if (!ExistingActor)
		{
			FSceneSaveManager::LoadActorFromJSONString(Delta.SerializedActor, World);
		}
	}

	for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
	{
		AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
		if (ExistingActor)
		{
			if (!FSceneSaveManager::ApplyActorFromJSONString(ExistingActor, Delta.BeforeSerializedActor))
			{
				World->DestroyActor(ExistingActor);
				FSceneSaveManager::LoadActorFromJSONString(Delta.BeforeSerializedActor, World);
			}
		}
		else
		{
			FSceneSaveManager::LoadActorFromJSONString(Delta.BeforeSerializedActor, World);
		}
	}
}

void UEditorEngine::RestoreTrackedActorOrder(const TArray<uint32>& OrderedUUIDs)
{
	UWorld* World = GetWorld();
	ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
	if (!World || !PersistentLevel || OrderedUUIDs.empty())
	{
		return;
	}

	std::set<uint32> OrderedUUIDSet(OrderedUUIDs.begin(), OrderedUUIDs.end());
	size_t PrefixCount = 0;
	for (AActor* Actor : PersistentLevel->GetActors())
	{
		if (!Actor || OrderedUUIDSet.find(Actor->GetUUID()) != OrderedUUIDSet.end())
		{
			break;
		}
		++PrefixCount;
	}

	for (size_t Index = 0; Index < OrderedUUIDs.size(); ++Index)
	{
		AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(OrderedUUIDs[Index]));
		if (!Actor)
		{
			continue;
		}

		World->MoveActorToIndex(Actor, PrefixCount + Index);
	}
}

void UEditorEngine::RestoreTrackedFolderOrder(const TArray<FString>& OrderedFolders)
{
	UWorld* World = GetWorld();
	ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
	if (!PersistentLevel)
	{
		return;
	}

	PersistentLevel->SetOutlinerFolders(OrderedFolders);
}

void UEditorEngine::RestoreTrackedSelection(const TArray<uint32>& SelectedUUIDs)
{
	TArray<AActor*> RestoredSelection;
	for (uint32 SelectedUUID : SelectedUUIDs)
	{
		if (AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(SelectedUUID)))
		{
			RestoredSelection.push_back(Actor);
		}
	}

	if (!RestoredSelection.empty())
	{
		SelectionManager.SelectActors(RestoredSelection);
	}
	else
	{
		SelectionManager.ClearSelection();
	}

	if (UGizmoComponent* Gizmo = GetGizmo())
	{
		Gizmo->UpdateGizmoTransform();
	}
}

void UEditorEngine::InvalidateTrackedSceneSnapshotCache()
{
	CachedTrackedSceneSnapshot.reset();
}

UCameraComponent* UEditorEngine::FindSceneViewportCamera() const
{
	for (FLevelEditorViewportClient* VC : ViewportLayout.GetLevelViewportClients())
	{
		if (!VC)
		{
			continue;
		}

		if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective
			|| VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
		{
			return VC->GetCamera();
		}
	}

	return nullptr;
}

void UEditorEngine::RestoreViewportCamera(const FPerspectiveCameraData& CamData)
{
	if (!CamData.bValid)
	{
		return;
	}

	if (UCameraComponent* Camera = FindSceneViewportCamera())
	{
		Camera->SetWorldLocation(CamData.Location);
		Camera->SetRelativeRotation(CamData.Rotation);
		FMinimalViewInfo CameraState = Camera->GetCameraState();
		CameraState.FOV = CamData.FOV;
		CameraState.NearZ = CamData.NearClip;
		CameraState.FarZ = CamData.FarClip;
		Camera->SetCameraState(CameraState);
	}
}

bool UEditorEngine::SaveSceneAs(const FString& InScenePath)
{
	if (InScenePath.empty())
	{
		return false;
	}

	StopPlayInEditorImmediate();
	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context || !Context->World)
	{
		return false;
	}

	if (InScenePath.ends_with(".umap") || InScenePath.ends_with(".UMAP"))
	{
		FSceneSaveManager::SaveWorldToBinary(InScenePath, Context->World);
	}
	else
	{
		FSceneSaveManager::SaveSceneAsJSON(InScenePath, *Context, FindSceneViewportCamera());
	}
	
	CurrentLevelFilePath = InScenePath;
	return true;
}

bool UEditorEngine::SaveScene()
{
	if (HasCurrentLevelFilePath())
	{
		return SaveSceneAs(CurrentLevelFilePath);
	}

	return SaveSceneAsWithDialog();
}

void UEditorEngine::RequestSaveSceneAsDialog()
{
	// Native file dialogs are deferred to the next tick so they do not open
	// while the ImGui menu/popup stack is still being processed.
	bRequestSaveSceneAsDialogQueued = true;
}

bool UEditorEngine::SaveSceneAsWithDialog()
{
	const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
	const std::wstring DefaultFile = HasCurrentLevelFilePath()
		? std::filesystem::path(FPaths::ToWide(CurrentLevelFilePath)).filename().wstring()
		: std::wstring(L"Untitled"); // Removed the forced extension so the dialog uses the selected filter's default
	const FString SelectedPath = FEditorFileUtils::SaveFileDialog({
		.Filter = L"Binary Scene (*.umap)\0*.umap\0JSON Scene (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0",
		.Title = L"Save Scene As",
		.InitialDirectory = InitialDir.c_str(),
		.DefaultFileName = DefaultFile.c_str(),
		.OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
		.bFileMustExist = false,
		.bPathMustExist = true,
		.bPromptOverwrite = true,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return false;
	}

	return SaveSceneAs(SelectedPath);
}

void UEditorEngine::ProcessDeferredEditorActions()
{
	if (!bRequestSaveSceneAsDialogQueued)
	{
		return;
	}

	bRequestSaveSceneAsDialogQueued = false;
	SaveSceneAsWithDialog();
}

bool UEditorEngine::LoadSceneFromPath(const FString& InScenePath)
{
	if (InScenePath.empty())
	{
		return false;
	}

	StopPlayInEditorImmediate();
	ClearScene();

	FWorldContext LoadContext;
	FPerspectiveCameraData CameraData;

	if (FSceneSaveManager::IsJsonFile(InScenePath))
	{
		FSceneSaveManager::LoadSceneFromJSON(InScenePath, LoadContext, CameraData);
	}
	else if (InScenePath.ends_with(".umap") || InScenePath.ends_with(".UMAP"))
	{
		LoadContext.World = UObjectManager::Get().CreateObject<UWorld>();
		FSceneSaveManager::LoadWorldFromBinary(InScenePath, LoadContext.World);
		LoadContext.WorldType = EWorldType::Editor;
		LoadContext.ContextName = "Loaded Binary Scene";
		LoadContext.ContextHandle = FName("Loaded Binary Scene");
	}
	if (!LoadContext.World)
	{
		return false;
	}

	WorldList.push_back(LoadContext);
	SetActiveWorld(LoadContext.ContextHandle);
	SelectionManager.SetWorld(LoadContext.World);
	LoadContext.World->WarmupPickingData();
	ResetViewport();
	RestoreViewportCamera(CameraData);

	CurrentLevelFilePath = InScenePath;
	return true;
}

bool UEditorEngine::LoadSceneWithDialog()
{
	const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
	const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
		.Filter = L"Scene Files (*.Scene;*.umap)\0*.Scene;*.umap\0All Files (*.*)\0*.*\0",
		.Title = L"Load Scene",
		.InitialDirectory = InitialDir.c_str(),
		.OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
		.bFileMustExist = true,
		.bPathMustExist = true,
		.bPromptOverwrite = false,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return false;
	}

	return LoadSceneFromPath(SelectedPath);
}

bool UEditorEngine::ImportMaterialWithDialog()
{
	const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
		.Filter = L"Material Files (*.mat)\0*.mat\0All Files (*.*)\0*.*\0",
		.Title = L"Import Material",
		.InitialDirectory = FPaths::AssetDir().c_str(),
		.OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
		.bFileMustExist = true,
		.bPathMustExist = true,
		.bPromptOverwrite = false,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return false;
	}

	FString ImportedPath;
	if (!ImportAssetFile(SelectedPath, L"Asset\\Materials\\Imported", ImportedPath))
	{
		return false;
	}

	FMaterialManager::Get().ScanMaterialAssets();
	MainPanel.RefreshContentBrowser();
	return true;
}

bool UEditorEngine::ImportTextureWithDialog()
{
	const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
		.Filter = L"Texture Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds\0All Files (*.*)\0*.*\0",
		.Title = L"Import Texture",
		.InitialDirectory = FPaths::AssetDir().c_str(),
		.OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
		.bFileMustExist = true,
		.bPathMustExist = true,
		.bPromptOverwrite = false,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return false;
	}

	FString ImportedPath;
	if (!ImportAssetFile(SelectedPath, L"Asset\\Textures\\Imported", ImportedPath))
	{
		return false;
	}

	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UTexture2D::LoadFromFile(ImportedPath, Device);
	UTexture2D::ScanTextureAssets();
	MainPanel.RefreshContentBrowser();
	return true;
}
