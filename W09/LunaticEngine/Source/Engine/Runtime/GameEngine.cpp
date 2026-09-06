#include "Engine/Runtime/GameEngine.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/Paths.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"
#include "Component/ActorComponent.h"
#include "Core/AsciiUtils.h"
#include "Input/InputManager.h"
#include "Object/Object.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Pipeline/Renderer.h"
#include <filesystem>

IMPLEMENT_CLASS(UGameEngine, UEngine)

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
}

void UGameEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	ImGuiOverlay.Initialize(InWindow, GetRenderer());
	FInputManager::Get().ClearGuiCaptureOverride();

	CreateWorldContext(EWorldType::Game, FName("GameWorld"));
	SetActiveWorld(FName("GameWorld"));
	GetWorld()->InitWorld();

	UGameViewportClient* ViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
	SetGameViewportClient(ViewportClient);
	if (InWindow)
	{
		ViewportClient->SetOwnerWindow(InWindow->GetHWND());
	}

	// Game/Shipping은 ImGui가 없으므로 백버퍼에 합성해줄 손이 없다.
	// 윈도우 크기로 FViewport를 만들고 DefaultRenderPipeline이 여기에 렌더 → 백버퍼 복사하도록.
	if (InWindow)
	{
		const uint32 Width  = static_cast<uint32>((std::max)(1.0f, InWindow->GetWidth()));
		const uint32 Height = static_cast<uint32>((std::max)(1.0f, InWindow->GetHeight()));
		FViewport* GameViewport = new FViewport();
		GameViewport->Initialize(GetRenderer().GetFD3DDevice().GetDevice(), Width, Height);
		ViewportClient->SetViewport(GameViewport);
	}

	LoadStartLevel();

	if (FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle()))
	{
		ViewportClient->OnBeginPIE(Context->World ? Context->World->GetActiveCamera() : nullptr, ViewportClient->GetViewport());
		// Game/Shipping build에서는 기본 스펙테이터 이동 로직을 비활성화 (플레이어 스크립트와 충돌 방지)
		ViewportClient->SetPIEPossessedInputEnabled(false);
		// 스펙테이터 이동은 끄되, 윈도우 마우스 메시지가 InputManager로 흐르도록 트래킹은 켠다.
		FInputManager::Get().SetTrackingMouse(true);
	}
}

void UGameEngine::Shutdown()
{
	ImGuiOverlay.Shutdown();
	FInputManager::Get().SetGuiCaptureOverride(false, false, false);
	UEngine::Shutdown();
}

void UGameEngine::BeginPlay()
{
	UEngine::BeginPlay();
}

void UGameEngine::Tick(float DeltaTime)
{
	const bool bPopupOpen = IsScoreSavePopupOpen();
	FInputManager::Get().SetGuiCaptureOverride(bPopupOpen, bPopupOpen, bPopupOpen);
	if (UGameViewportClient* GameVC = GetGameViewportClient())
	{
		if (bPopupOpen)
		{
			GameVC->SetPIEPossessedInputEnabled(false);
		}

		if (!bPopupOpen)
		{
			GameVC->ProcessPIEInput(DeltaTime);
			GameVC->Tick(DeltaTime);
		}
	}
	UEngine::Tick(DeltaTime);
}

void UGameEngine::RenderImGuiOverlay(FRenderer& InRenderer)
{
	ImGuiOverlay.Render(InRenderer);
}

void UGameEngine::OpenScoreSavePopup(int32 InScore)
{
	ImGuiOverlay.OpenScoreSavePopup(InScore);
}

bool UGameEngine::ConsumeScoreSavePopupResult(FString& OutNickname)
{
	return ImGuiOverlay.ConsumeScoreSavePopupResult(OutNickname);
}

void UGameEngine::OpenMessagePopup(const FString& InMessage)
{
	ImGuiOverlay.OpenMessagePopup(InMessage);
}

bool UGameEngine::ConsumeMessagePopupConfirmed()
{
	return ImGuiOverlay.ConsumeMessagePopupConfirmed();
}

void UGameEngine::OpenScoreboardPopup(const FString& InFilePath)
{
	ImGuiOverlay.OpenScoreboardPopup(InFilePath);
}

void UGameEngine::OpenTitleOptionsPopup()
{
	ImGuiOverlay.OpenTitleOptionsPopup();
}

void UGameEngine::OpenTitleCreditsPopup()
{
	ImGuiOverlay.OpenTitleCreditsPopup();
}

bool UGameEngine::IsScoreSavePopupOpen() const
{
	return ImGuiOverlay.IsScoreSavePopupOpen();
}

void UGameEngine::LoadStartLevel()
{
	const FString& StartLevel = FProjectSettings::Get().Game.DefaultScene;
	if (StartLevel.empty())
	{
		return;
	}

	if (LoadScene(StartLevel))
	{
		return;
	}

	const std::filesystem::path SceneDir = FSceneSaveManager::GetSceneDirectory();
	const std::wstring StemW = FPaths::ToWide(StartLevel);

	// 우선순위: 쿠킹된 .umap → .Scene(JSON, dev/fallback). Shipping에서는 .umap 전용.
	const std::filesystem::path UmapPath = SceneDir / (StemW + L".umap");
	const std::filesystem::path ScenePath = SceneDir / (StemW + FSceneSaveManager::SceneExtension);

	std::filesystem::path ChosenPath;
	if (std::filesystem::exists(UmapPath))
	{
		ChosenPath = UmapPath;
	}
#if !defined(SHIPPING) || SHIPPING == 0
	else if (std::filesystem::exists(ScenePath))
	{
		ChosenPath = ScenePath;
	}
#endif

	if (ChosenPath.empty())
	{
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context || !Context->World)
	{
		return;
	}

	const FName OriginalHandle = Context->ContextHandle;
	FPerspectiveCameraData CamData;

	if (FSceneSaveManager::IsJsonFile(FilePath))
	{
		// 쿠킹된 .umap도 현재는 JSON 텍스트로 저장되어 있어 binary parser로는 못 읽는다.
		// JSON인 경우 LoadSceneFromJSON이 새 World를 생성하므로 기존 World 정리 필요
		UWorld* OldWorld = Context->World;
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, CamData);
		if (OldWorld && OldWorld != Context->World)
		{
			OldWorld->EndPlay();
			UObjectManager::Get().DestroyObject(OldWorld);
		}
	}
	else if (ChosenPath.extension() == ".umap")
	{
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
	}
	else
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, CamData);
	}

	// LoadSceneFromJSON/LoadWorldFromBinary 이후 WorldType/Handle 강제 복구
	Context->WorldType = EWorldType::Game;
	Context->ContextHandle = OriginalHandle;
	SetActiveWorld(OriginalHandle);

	if (Context->World)
	{
		Context->World->SetWorldType(EWorldType::Game);
		Context->World->WarmupPickingData();

		// Game/Shipping에서는 에디터 뷰포트가 없으므로 씬 안의 첫 카메라 컴포넌트를 ActiveCamera로 잡음.
		if (!Context->World->GetActiveCamera())
		{
			for (AActor* Actor : Context->World->GetActors())
			{
				if (!Actor) continue;
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
					{
						Context->World->SetActiveCamera(Cam);
						break;
					}
				}
				if (Context->World->GetActiveCamera()) break;
			}
		}

		// 씬 어디에도 UCameraComponent가 없으면(에디터에서 PerspectiveCamera만 저장된 일반 .Scene)
		// 저장된 에디터 뷰포트 좌표를 사용해 기본 카메라 액터를 한 개 스폰한다.
		if (!Context->World->GetActiveCamera())
		{
			AActor* CamActor = Context->World->SpawnActor<AActor>();
			if (CamActor)
			{
				CamActor->SetFName(FName("DefaultGameCamera"));
				UCameraComponent* Cam = CamActor->AddComponent<UCameraComponent>();
				CamActor->SetRootComponent(Cam);
				if (CamData.bValid)
				{
					Cam->SetRelativeLocation(CamData.Location);
					Cam->SetRelativeRotation(CamData.Rotation);
				}
				else
				{
					// fallback: 위에서 비스듬히 바라보기
					Cam->SetRelativeLocation(FVector(0.0f, -10.0f, 5.0f));
					Cam->SetRelativeRotation(FVector(0.0f, -25.0f, 90.0f));
				}
				Context->World->SetActiveCamera(Cam);
			}
		}
	}
}

bool UGameEngine::LoadScene(const FString& InSceneReference)
{
	if (InSceneReference.empty())
	{
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
			// 같은 상대경로에서 반대 확장자(.scene ↔ .umap) 시도 (Shipping은 .umap만 존재)
			std::filesystem::path Swapped = RawPath;
			Swapped.replace_extension(bHasSceneExtension ? L".umap" : FSceneSaveManager::SceneExtension);
			if (!TrySetChosenPath(SceneDir / Swapped))
			{
				// 파일명만으로 fallback (양 확장자 모두 시도)
				const std::filesystem::path FileName = RawPath.filename();
				if (!TrySetChosenPath(SceneDir / FileName))
				{
					std::filesystem::path FileNameSwapped = FileName;
					FileNameSwapped.replace_extension(bHasSceneExtension ? L".umap" : FSceneSaveManager::SceneExtension);
					if (!TrySetChosenPath(SceneDir / FileNameSwapped))
					{
						return false;
					}
				}
			}
		}
		else
		{
			const std::wstring StemW = FPaths::ToWide(InSceneReference);
			if (!TrySetChosenPath(SceneDir / (StemW + L".umap")))
			{
				if (!TrySetChosenPath(SceneDir / (StemW + FSceneSaveManager::SceneExtension)))
				{
					return false;
				}
			}
		}
	}

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context)
	{
		return false;
	}

	if (UGameViewportClient* ViewportClient = GetGameViewportClient())
	{
		ViewportClient->UnPossess();
		ViewportClient->SetPIEPossessedInputEnabled(false);
	}

	if (Context->World)
	{
		Context->World->EndPlay();
		UObjectManager::Get().DestroyObject(Context->World);
		Context->World = nullptr;
	}

	FPerspectiveCameraData CameraData;
	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());
	if (FSceneSaveManager::IsJsonFile(FilePath))
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, CameraData);
	}
	else if (EndsWithIgnoreCase(FilePath, ".umap"))
	{
		Context->World = UObjectManager::Get().CreateObject<UWorld>();
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
	}
	else
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, CameraData);
	}

	Context->WorldType = EWorldType::Game;
	Context->ContextName = RawPath.stem().empty() ? "GameWorld" : FPaths::ToUtf8(RawPath.stem().wstring());
	Context->ContextHandle = GetActiveWorldHandle();
	SetActiveWorld(Context->ContextHandle);

	if (!Context->World)
	{
		return false;
	}

	Context->World->SetWorldType(EWorldType::Game);
	Context->World->WarmupPickingData();

	if (!Context->World->GetActiveCamera())
	{
		for (AActor* Actor : Context->World->GetActors())
		{
			if (!Actor)
			{
				continue;
			}

			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
				{
					Context->World->SetActiveCamera(Cam);
					break;
				}
			}

			if (Context->World->GetActiveCamera())
			{
				break;
			}
		}
	}

	if (!Context->World->GetActiveCamera())
	{
		AActor* CamActor = Context->World->SpawnActor<AActor>();
		if (CamActor)
		{
			CamActor->SetFName(FName("DefaultGameCamera"));
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
			Context->World->SetActiveCamera(Cam);
		}
	}

	if (!Context->World->HasBegunPlay())
	{
		Context->World->BeginPlay();
	}

	if (UGameViewportClient* ViewportClient = GetGameViewportClient())
	{
		if (UCameraComponent* GameCamera = Context->World->GetActiveCamera())
		{
			ViewportClient->Possess(GameCamera);
		}
	}

	return true;
}
