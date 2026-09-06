#pragma once

#include "Editor/UI/EditorWidget.h"
#include <filesystem>
#include <functional>

class UAssetData;
class UCameraModifierStackAssetData;
struct FAssetBezierCurve;
struct FCameraShakeModifierAssetDesc;

class FAssetEditorWidget final : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Shutdown();
	void Render(float DeltaTime) override;

	bool OpenAssetWithDialog(void* OwnerWindowHandle = nullptr);
	bool CreateCameraShakeAsset();
	bool OpenAssetFromPath(const std::filesystem::path& FilePath);
	bool SaveCurrentAsset();
	bool HasOpenAsset() const { return EditingAsset != nullptr; }
	bool IsOpen() const { return bOpen; }
	bool IsCapturingInput() const { return bCapturingInput; }

	static bool OpenAssetFile(const std::filesystem::path& FilePath);

private:
	void DrawToolbar();
	void DrawAssetContents();
	void DrawDetails();
	bool DrawLabeledField(const char* Label, const std::function<bool()>& DrawField);
	bool DrawCurveControlPointRow(const char* Label, float& XValue, float& YValue);
	bool DrawCurveEditor(const char* Label, FAssetBezierCurve& Curve);
	bool PromptForSavePath(void* OwnerWindowHandle = nullptr);
	void DrawCameraModifierStackAssetContents(UCameraModifierStackAssetData& Asset);
	void DrawCameraModifierStackAssetDetails(UCameraModifierStackAssetData& Asset);
	bool DrawCameraShakeDetails(FCameraShakeModifierAssetDesc& Desc);
	FCameraShakeModifierAssetDesc* FindSelectedCameraShake(UCameraModifierStackAssetData& Asset);
	void CloseCurrentAsset();
	void MarkDirty() { bDirty = true; }

private:
	UAssetData* EditingAsset = nullptr;
	std::filesystem::path EditingAssetPath;
	uint64 SelectedEditorId = 0;
	bool bOpen = false;
	bool bDirty = false;
	bool bCapturingInput = false;
};
