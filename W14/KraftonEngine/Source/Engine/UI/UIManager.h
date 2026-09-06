#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/GarbageCollection.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Render/Types/RenderTypes.h"

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

#include <chrono>

class APlayerController;
class UUserWidget;
struct FFrameContext;
struct FPassContext;
struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11RasterizerState;
struct ID3D11ShaderResourceView;

class FRmlSystemInterface final : public Rml::SystemInterface
{
public:
	double GetElapsedTime() override;
	void JoinPath(Rml::String& TranslatedPath, const Rml::String& DocumentPath, const Rml::String& Path) override;
	bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override;

private:
	std::chrono::steady_clock::time_point StartTime = std::chrono::steady_clock::now();
};

// 한글 경로 호환 — RmlUi 기본 FileInterface 는 fopen(UTF-8 bytes) 로 동작하는데, Windows
// fopen 은 인자를 ANSI 코드페이지(예: CP949) 로 해석해 한글 경로의 UTF-8 바이트를 깨뜨린다.
// 이 인터페이스는 항상 wide(_wfopen) 로 열어 한글 경로 디렉토리에서도 RML / 폰트 / CSS 가
// 정상 로드되게 한다.
class FRmlFileInterfaceWide final : public Rml::FileInterface
{
public:
	Rml::FileHandle Open(const Rml::String& Path) override;
	void Close(Rml::FileHandle FileHandle) override;
	size_t Read(void* Buffer, size_t Size, Rml::FileHandle FileHandle) override;
	bool Seek(Rml::FileHandle FileHandle, long Offset, int Origin) override;
	size_t Tell(Rml::FileHandle FileHandle) override;
};

struct FUIInputCaptureState
{
	bool bWantsMouse = false;
	bool bWantsKeyboard = false;
	bool bWantsTextInput = false;

	bool bBlocksGameInput = false;
	bool bBlocksGameKeyboard = false;
	bool bBlocksGameMouseLook = false;
};

class FRmlRenderInterfaceD3D11 final : public Rml::RenderInterface
{
public:
	explicit FRmlRenderInterfaceD3D11(ID3D11Device* InDevice);
	~FRmlRenderInterfaceD3D11() override;

	void BeginFrame(const FPassContext& InCtx);
	void EndFrame();

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle GeometryHandle, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle GeometryHandle) override;
	Rml::TextureHandle LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
	void ReleaseTexture(Rml::TextureHandle Texture) override;
	void EnableScissorRegion(bool Enable) override;
	void SetScissorRegion(Rml::Rectanglei Region) override;
	void SetTransform(const Rml::Matrix4f* Transform) override;

private:
	void CreateConstantBuffer();
	void CreateWhiteTexture();
	void ReleaseWhiteTexture();

private:
	ID3D11Device* Device = nullptr;
	ID3D11Buffer* PerFrameCB = nullptr;
	ID3D11ShaderResourceView* WhiteTextureSRV = nullptr;
	ID3D11RasterizerState* ScissorRasterizerState = nullptr;
	bool bScissorEnabled = false;
	Rml::Matrix4f CurrentTransform;
	const FPassContext* Ctx = nullptr;
};

class UUIManager : public TSingleton<UUIManager>, public FGCObject
{
	friend class TSingleton<UUIManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Shutdown();

	UUserWidget* CreateWidget(APlayerController* OwningPlayer, const FString& DocumentPath);
	void AddToViewport(UUserWidget* Widget, int32 ZOrder);
	void RemoveFromViewport(UUserWidget* Widget);
	bool ReloadDocument(UUserWidget* Widget);
	bool ReloadStyleSheet(UUserWidget* Widget);
	void ReloadAllStyleSheets();
	// PIE end / TransitionToScene 같은 라이프사이클 경계 — viewport 만 비우고 widget UObject
	// 는 유지 (Lua 가 캐시한 핸들이 valid 한 채로 다음 세션에 재사용되도록).
	void ClearViewport();
	// 엔진 shutdown 전용 — 모든 widget UObject 파괴.
	void DestroyAllWidgets();

	void Render(const FPassContext& Ctx);
	bool HasViewportWidgets() const { return !ViewportWidgets.empty(); }
	const char* GetReferencerName() const override { return "UUIManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

		// viewport 에 올라온 widget들의 입력 요구/게임 입력 차단 정책을 합산한다.
		FUIInputCaptureState GetViewportInputCaptureState() const;
		bool AnyViewportWidgetWantsMouse() const;

private:
	UUIManager() = default;
	~UUIManager() = default;

	bool LoadDocument(UUserWidget* Widget);
	void CloseDocument(UUserWidget* Widget);
	void CompactInvalidWidgets();
	void ProcessInput(const FFrameContext& Frame);
	void RemoveFromViewportImmediate(UUserWidget* Widget);
	void FlushDeferredViewportRemovals();
	void StartUIHotReloadWatcher();
	void StopUIHotReloadWatcher();
	void HandleUIFileChanges(const TSet<FString>& ChangedPaths);

private:
	TArray<UUserWidget*> ViewportWidgets;
	TArray<UUserWidget*> CreatedWidgets;
	TArray<UUserWidget*> PendingRemoveWidgets;
	TArray<UUserWidget*> PendingReloadWidgets;

	ID3D11Device* CachedDevice = nullptr;
	FRmlSystemInterface* SystemInterface = nullptr;
	FRmlFileInterfaceWide* FileInterface = nullptr;
	FRmlRenderInterfaceD3D11* RenderInterface = nullptr;
	Rml::Context* RmlContext = nullptr;
	FWatchID UIWatchID = 0;
	FSubscriptionID UIWatchSub = 0;
	bool bRmlInitialized = false;
	bool bDispatchingRmlEvents = false;
};
