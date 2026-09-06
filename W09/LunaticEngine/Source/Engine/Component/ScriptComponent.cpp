#include "Component/ScriptComponent.h"

#include "Core/Log.h"
#include "Core/PropertyTypes.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"
#include "Runtime/Engine.h"
#include "Scripting/LuaScriptRuntime.h"
#include "Serialization/Archive.h"
#include "Shape/ShapeComponent.h"
#include "PrimitiveComponent.h"

#include <shellapi.h>

#include <cstring>
#include <filesystem>

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

#pragma region Path Helper
namespace
{
	// 스크립트 자동 생성 시 장면/액터 이름을 조합할 때 쓰는 fallback 이름.
	constexpr const char* DefaultSceneName = "DefaultScene";
	constexpr const char* DefaultActorName = "LuaActor";

	FString GetFileStem(const FString& PathString)
	{
		const std::filesystem::path Path(FPaths::ToWide(PathString));
		return FPaths::ToUtf8(Path.stem().wstring());
	}

	bool LooksLikeGeneratedSceneName(const FString& SceneName)
	{
		// 에디터나 런타임이 임시로 만든 이름은 실제 파일명 후보로 쓰지 않는다.
		return SceneName.empty()
			|| SceneName.rfind("ULevel_", 0) == 0
			|| SceneName.rfind("UWorld_", 0) == 0;
	}

	FString SanitizeScriptFileName(FString Name)
	{
		// 운영체제 파일명에 쓸 수 없는 문자만 최소한으로 치환한다.
		if (Name.empty())
		{
			return DefaultActorName;
		}

		for (char& Character : Name)
		{
			switch (Character)
			{
			case '<':
			case '>':
			case ':':
			case '"':
			case '/':
			case '\\':
			case '|':
			case '?':
			case '*':
			case ' ':
				Character = '_';
				break;
			default:
				break;
			}
		}

		return Name;
	}

	FString GetSceneNameForScript(const AActor* OwnerActor)
	{
		// 1순위는 실제 저장된 레벨 파일명이고,
		// 그게 없으면 Level/World 이름으로 fallback한다.
		if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->HasCurrentLevelFilePath())
			{
				const FString SceneStem = GetFileStem(EditorEngine->GetCurrentLevelFilePath());
				if (!SceneStem.empty())
				{
					return SceneStem;
				}
			}
		}

		if (OwnerActor)
		{
			if (const ULevel* Level = OwnerActor->GetLevel())
			{
				const FString LevelName = Level->GetFName().ToString();
				if (!LooksLikeGeneratedSceneName(LevelName))
				{
					return LevelName;
				}
			}

			if (const UWorld* World = OwnerActor->GetWorld())
			{
				const FString WorldName = World->GetFName().ToString();
				if (!LooksLikeGeneratedSceneName(WorldName))
				{
					return WorldName;
				}
			}
		}

		return DefaultSceneName;
	}

	FString GetActorNameForScript(const AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return DefaultActorName;
		}

		const FString ActorName = OwnerActor->GetFName().ToString();
		return ActorName.empty() ? FString(DefaultActorName) : ActorName;
	}

	FString BuildDefaultScriptPath(const AActor* OwnerActor)
	{
		// 새 스크립트 생성 시 장면/액터 이름 기반 기본 파일명을 만들되,
		// 최종 표기는 항상 FScriptPaths 정책을 거쳐 Scripts/... 형태로 맞춘다.
		const FString SceneName = SanitizeScriptFileName(GetSceneNameForScript(OwnerActor));
		const FString ActorName = SanitizeScriptFileName(GetActorNameForScript(OwnerActor));
		const FString FileName = SceneName + "_" + ActorName + ".lua";
		return FScriptPaths::NormalizeScriptPath(FileName);
	}
}
#pragma endregion

UScriptComponent::~UScriptComponent()
{
	UnbindOwnerShapeCollisionEvents();
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG_CATEGORY(ScriptComponent, Debug, "[ScriptComponent] BeginPlay for actor: %s, script: %s", 
		GetOwner() ? GetOwner()->GetFName().ToString().c_str() : "None", ScriptPath.c_str());

	ClearScriptError();

	if (ScriptPath.empty())
	{
		SetScriptError("ScriptPath is empty.");
		return;
	}

	// Play 중 hot-reload 대상 선별은 Runtime이 담당하므로
	// 실제 로딩 전에 먼저 등록해도 문제없다.
	UpdateRuntimeRegistration();

	if (!LoadScript())
	{
		UE_LOG_CATEGORY(ScriptComponent, Warning, "[ScriptComponent] LoadScript failed for %s", ScriptPath.c_str());
		return;
	}

	// 로드가 끝난 후 delegate를 바인딩
	BindOwnerShapeCollisionEvents();

	// 델리게이트 바인드가 끝난 뒤 Lua BeginPlay를 호출해 스크립트 초기화 로직을 실행한다.
	if (!ScriptInstance.CallBeginPlay())
	{
		UE_LOG_CATEGORY(ScriptComponent, Warning, "[ScriptComponent] Lua BeginPlay call failed for %s", ScriptPath.c_str());
		RefreshScriptErrorState();

		// 실패한 경우 델리게이트 바인딩 모두 해제
		UnbindOwnerShapeCollisionEvents();

		ScriptInstance.StopAllCoroutines();
		bLoaded = false;

		return;
	}
	UE_LOG_CATEGORY(ScriptComponent, Debug, "[ScriptComponent] Lua BeginPlay successful for %s", ScriptPath.c_str());
	RefreshScriptErrorState();
}

void UScriptComponent::EndPlay()
{
	UnbindOwnerShapeCollisionEvents();

	// EndPlay 이후에는 더 이상 이 component를 hot-reload 대상으로 순회하면 안 된다.
	if (!ScriptPath.empty())
	{
		FLuaScriptRuntime::Get().UnregisterScriptComponent(this);
	}

	if (bLoaded)
	{
		ScriptInstance.CallEndPlay();
	}

	ScriptInstance.StopAllCoroutines();
	ScriptInstance.Shutdown();
	bLoaded = false;
	RefreshScriptErrorState();

	Super::EndPlay();
}

void UScriptComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// ScriptPath는 component 상태의 일부이므로 저장/로드 시 그대로 직렬화한다.
	Ar << ScriptPath;

	if (Ar.IsLoading())
	{
		ScriptPath = FScriptPaths::NormalizeScriptPath(ScriptPath);
		ScriptInstance.SetScriptPath(ScriptPath);
	}
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);
	OutProps.push_back({ "ScriptPath", EPropertyType::String, &ScriptPath });
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "ScriptPath") == 0)
	{
		// 에디터에서 입력된 경로도 런타임과 같은 정규화 정책을 타게 만든다.
		SetScriptPath(ScriptPath);

		if (GetOwner() && GetOwner()->HasActorBegunPlay())
		{
			// 플레이 중 경로가 바뀌면 즉시 새 파일 기준으로 다시 로드한다.
			ReloadScript();
		}
	}
}

void UScriptComponent::SetScriptPath(const FString& InPath)
{
	// 직렬화/에디터 입력/코드 호출 경로를 모두 같은 내부 표기로 맞춘다.
	ScriptPath = FScriptPaths::NormalizeScriptPath(InPath);
	ScriptInstance.SetScriptPath(ScriptPath);

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasActorBegunPlay())
	{
		// 플레이 도중 경로가 바뀌면 hot-reload 대상 매핑도 즉시 갱신되어야 한다.
		UpdateRuntimeRegistration();
	}
}

bool UScriptComponent::CreateScript()
{
	// 자동 생성 파일도 수동 입력 경로와 같은 정책을 타게 해서
	// 내부 저장 경로와 실제 파일 위치가 어긋나지 않게 유지한다.
	const std::filesystem::path ScriptsDir = std::filesystem::path(FPaths::ScriptsDir()).lexically_normal();
	std::error_code ErrorCode;
	std::filesystem::create_directories(ScriptsDir, ErrorCode);
	if (ErrorCode)
	{
		SetScriptError("CreateScript failed: could not create Scripts directory. " + ErrorCode.message());
		return false;
	}

	const std::filesystem::path TemplatePath = ScriptsDir / L"template.lua";
	if (!std::filesystem::exists(TemplatePath))
	{
		SetScriptError("CreateScript failed: Scripts/template.lua was not found.");
		return false;
	}

	const FString RelativePathString = ScriptPath.empty()
		? BuildDefaultScriptPath(GetOwner())
		: FScriptPaths::NormalizeScriptPath(ScriptPath);

	// 저장값은 상대 경로로 유지하고, 실제 복사 대상만 절대 경로로 바꿔 쓴다.
	const std::filesystem::path AbsoluteGeneratedPath = FScriptPaths::ResolveScriptPath(RelativePathString);

	ErrorCode.clear();

	std::filesystem::create_directories(AbsoluteGeneratedPath.parent_path(), ErrorCode);
	if (ErrorCode)
	{
		SetScriptError("CreateScript failed: could not create script directory. " + ErrorCode.message());
		return false;
	}

	if (std::filesystem::exists(AbsoluteGeneratedPath))
	{
		// 이미 파일이 있으면 덮어쓰지 않고 그 경로만 현재 component에 연결한다.
		UE_LOG_CATEGORY(ScriptComponent, Debug, "[ScriptComponent] CreateScript: existing script reused: %s", RelativePathString.c_str());
		SetScriptPath(RelativePathString);
		ClearScriptError();
		if (GetOwner() && GetOwner()->HasActorBegunPlay())
		{
			return ReloadScript();
		}
		return true;
	}

	ErrorCode.clear();
	const bool bCopied = std::filesystem::copy_file(
		TemplatePath,
		AbsoluteGeneratedPath,
		std::filesystem::copy_options::none,
		ErrorCode);

	if (!bCopied)
	{
		SetScriptError("CreateScript failed: " + ErrorCode.message());
		return false;
	}

	SetScriptPath(RelativePathString);
	ClearScriptError();

	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		return ReloadScript();
	}

	return true;
}

bool UScriptComponent::LoadScript()
{
	// 일반 로드는 항상 기존 Lua 상태를 비우고 처음부터 다시 시작한다.
	ScriptInstance.Shutdown();
	bLoaded = false;

	if (ScriptPath.empty())
	{
		SetScriptError("LoadScript failed: ScriptPath is empty.");
		return false;
	}

	if (!ScriptInstance.Initialize(this))
	{
		SetScriptError(ScriptInstance.GetLastError());
		return false;
	}

	// ScriptInstance도 같은 canonical 경로를 갖고 있어야 이후 reload 비교가 단순해진다.
	ScriptInstance.SetScriptPath(ScriptPath);
	if (!ScriptInstance.LoadFromFile(ScriptPath))
	{
		SetScriptError(ScriptInstance.GetLastError());
		return false;
	}

	bLoaded = true;
	RefreshScriptErrorState();
	return true;
}

bool UScriptComponent::ReloadScript()
{
	// 수동 reload와 파일 변경 기반 hot-reload가 모두 이 경로를 사용한다.
	// 이미 owner가 연결된 경우에는 그 상태를 재사용하고, 아니면 다시 초기화한다.
	UnbindOwnerShapeCollisionEvents();
	bLoaded = false;

	if (ScriptPath.empty())
	{
		SetScriptError("ReloadScript failed: ScriptPath is empty.");
		return false;
	}

	const std::filesystem::path AbsoluteScriptPath = FScriptPaths::ResolveScriptPath(ScriptPath);
	if (!std::filesystem::exists(AbsoluteScriptPath))
	{
		SetScriptError("ReloadScript failed: script file does not exist: " + FPaths::ToUtf8(AbsoluteScriptPath.generic_wstring()));
		return false;
	}

	if (ScriptInstance.GetOwnerComponent() != this)
	{
		if (!ScriptInstance.Initialize(this))
		{
			SetScriptError(ScriptInstance.GetLastError());
			return false;
		}

		ScriptInstance.SetScriptPath(ScriptPath);
	}

	ScriptInstance.StopAllCoroutines();

	// 실제 environment 재구성은 ScriptInstance가 알고 있으므로
	// component는 reload 진입과 후처리만 담당한다.
	const bool bReloaded = ScriptInstance.Reload();
	bLoaded = bReloaded;
	if (!bReloaded)
	{
		if (ScriptInstance.HasError())
		{
			SetScriptError(ScriptInstance.GetLastError());
		}
		else
		{
			SetScriptError("ReloadScript failed: unknown Lua reload error.");
		}
		return false;
	}

	bool bBeginPlaySucceeded = true;
	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		// 현재 엔진 구조에서는 reload 후 BeginPlay를 다시 호출해
		// 스크립트가 자신의 초기 상태를 다시 세팅할 기회를 준다.
		bBeginPlaySucceeded = ScriptInstance.CallBeginPlay();
	}

	RefreshScriptErrorState();

	// BeginPlay가 실패한 경우 Load를 다시 수행한다
	if (!bBeginPlaySucceeded)
	{
		ScriptInstance.StopAllCoroutines();
		UnbindOwnerShapeCollisionEvents();
		bLoaded = false;
	}
	else if (bReloaded && GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		BindOwnerShapeCollisionEvents();
	}
	return bReloaded && bBeginPlaySucceeded;
}

bool UScriptComponent::OpenScript()
{
	// 파일을 직접 편집할 수 있도록 절대 경로로 변환한 뒤 OS에 위임한다.
	if (ScriptPath.empty())
	{
		SetScriptError("OpenScript failed: ScriptPath is empty.");
		return false;
	}

	const std::filesystem::path AbsoluteScriptPath = FScriptPaths::ResolveScriptPath(ScriptPath);
	if (!std::filesystem::exists(AbsoluteScriptPath))
	{
		SetScriptError("OpenScript failed: script file does not exist: " + FPaths::ToUtf8(AbsoluteScriptPath.generic_wstring()));
		return false;
	}

	const std::wstring WidePath = AbsoluteScriptPath.wstring();

	const HINSTANCE Result = ShellExecuteW(
		nullptr,
		L"open",
		WidePath.c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL);

	if (reinterpret_cast<INT_PTR>(Result) <= 32)
	{
		SetScriptError("OpenScript failed: ShellExecuteW returned " + std::to_string(reinterpret_cast<INT_PTR>(Result)) + ".");
		return false;
	}

	ClearScriptError();
	return true;
}

bool UScriptComponent::CallScriptFunction(const FString& FunctionName)
{
	if (FunctionName.empty())
	{
		return false;
	}

	if (!ScriptInstance.CallLuaFunction(FunctionName))
	{
		RefreshScriptErrorState();
		return false;
	}

	RefreshScriptErrorState();
	return true;
}

void UScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 로드되지 않은 스크립트는 Tick과 coroutine 갱신을 모두 건너뛴다.
	if (!bLoaded)
	{
		return;
	}

	// 일반 Tick 함수와 coroutine resume을 같은 프레임에서 순서대로 처리한다.
	// BeginPlay와 마찬가지로 호출을 실패한 경우 다시 load를 수행한다.
	if (!ScriptInstance.CallTick(DeltaTime))
	{
		RefreshScriptErrorState();

		ScriptInstance.StopAllCoroutines();
		bLoaded = false;

		return;
	}
	ScriptInstance.TickCoroutines(DeltaTime);

	if (ScriptInstance.HasError())
	{
		RefreshScriptErrorState();
		ScriptInstance.StopAllCoroutines();
		bLoaded = false;
		return;
	}

	RefreshScriptErrorState();
}

void UScriptComponent::BindOwnerShapeCollisionEvents()
{
	UnbindOwnerShapeCollisionEvents();

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG_CATEGORY(ScriptComponent, Warning, "Owner Actor isn't exist");
		return;
	}

	const TArray<UPrimitiveComponent*>& PrimitiveComponents =
		OwnerActor->GetPrimitiveComponents();

	// OwnerActor의 PrimitiveComponent들을 순회
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || !IsAliveObject(PrimitiveComponent))
		{
			continue;
		}

		// ShapeComponent가 있다면 Delegate Binding
		UShapeComponent* ShapeComponent = Cast<UShapeComponent>(PrimitiveComponent);
		if (!ShapeComponent)
		{
			continue;
		}

		FShapeCollisionBinding Binding;
		Binding.ShapeComponent = ShapeComponent;

		Binding.BeginOverlapHandle = ShapeComponent->OnComponentBeginOverlap.AddDynamic(
				this,
				&UScriptComponent::OnShapeBeginOverlap);

		Binding.EndOverlapHandle =
			ShapeComponent->OnComponentEndOverlap.AddDynamic(
				this,
				&UScriptComponent::OnShapeEndOverlap);

		Binding.HitHandle =
			ShapeComponent->OnComponentHit.AddDynamic(
				this,
				&UScriptComponent::OnShapeHit);

		ShapeCollisionBindings.push_back(Binding);
	}
}

void UScriptComponent::UnbindOwnerShapeCollisionEvents()
{
	// 캐싱된 ShapeCollisionBinding들을 순회하면서 Delegate Bind 해제
	for (FShapeCollisionBinding& Binding : ShapeCollisionBindings)
	{
		if (!Binding.ShapeComponent || !IsAliveObject(Binding.ShapeComponent))
		{
			continue;
		}

		if (Binding.BeginOverlapHandle.IsValid())
		{
			Binding.ShapeComponent->OnComponentBeginOverlap.Remove(
				Binding.BeginOverlapHandle);

			Binding.BeginOverlapHandle.Reset();
		}

		if (Binding.EndOverlapHandle.IsValid())
		{
			Binding.ShapeComponent->OnComponentEndOverlap.Remove(
				Binding.EndOverlapHandle);

			Binding.EndOverlapHandle.Reset();
		}

		if (Binding.HitHandle.IsValid())
		{
			Binding.ShapeComponent->OnComponentHit.Remove(Binding.HitHandle);
			Binding.HitHandle.Reset();
		}
	}

	ShapeCollisionBindings.clear();
}

void UScriptComponent::OnShapeBeginOverlap(const FComponentOverlapEvent& Event)
{
	if (!bLoaded)
	{
		return;
	}

	if (!ScriptInstance.CallLuaOverlapEvent(
		"OnBeginOverlap",
		Event.OtherActor,
		Event.OtherComponent,
		Event.OverlappedComponent))
	{
		RefreshScriptErrorState();
		return;
	}

	RefreshScriptErrorState();
}

void UScriptComponent::OnShapeEndOverlap(const FComponentOverlapEvent& Event)
{
	if (!bLoaded)
	{
		return;
	}

	if (!ScriptInstance.CallLuaOverlapEvent(
		"OnEndOverlap",
		Event.OtherActor,
		Event.OtherComponent,
		Event.OverlappedComponent))
	{
		RefreshScriptErrorState();
		return;
	}

	RefreshScriptErrorState();
}

void UScriptComponent::OnShapeHit(const FComponentHitEvent& Event)
{
	if (!bLoaded)
	{
		return;
	}

	if (!ScriptInstance.CallLuaHitEvent(
		"OnHit",
		Event.OtherActor,
		Event.OtherComponent,
		Event.HitComponent,
		Event.Hit.ImpactLocation,
		Event.Hit.ImpactNormal))
	{
		RefreshScriptErrorState();
		return;
	}

	RefreshScriptErrorState();
}

void UScriptComponent::ClearScriptError()
{
	bHasScriptError = false;
	LastScriptError.clear();
}

void UScriptComponent::SetScriptError(const FString& ErrorMessage)
{
	bHasScriptError = true;
	LastScriptError = ErrorMessage;
	UE_LOG_CATEGORY(ScriptComponent, Error, "%s", ErrorMessage.c_str());
}

void UScriptComponent::RefreshScriptErrorState()
{
	// 실제 Lua 실행 에러는 ScriptInstance가 쥐고 있으므로
	// component는 그 상태를 UI/로그용으로 반영만 한다.
	if (ScriptInstance.HasError())
	{
		SetScriptError(ScriptInstance.GetLastError());
		return;
	}

	ClearScriptError();
}

void UScriptComponent::UpdateRuntimeRegistration()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasActorBegunPlay() || ScriptPath.empty())
	{
		// 아직 플레이 전이거나 경로가 없으면 watcher 대상에서 뺀다.
		FLuaScriptRuntime::Get().UnregisterScriptComponent(this);
		return;
	}

	// 등록은 로딩 성공 여부와 별개로 유지한다.
	// 그래야 파일이 나중에 생성되거나 수정됐을 때도 같은 경로 기준으로 reload 시도 가능하다.
	FLuaScriptRuntime::Get().RegisterScriptComponent(this);
}
