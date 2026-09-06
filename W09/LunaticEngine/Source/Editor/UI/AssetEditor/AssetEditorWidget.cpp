#include "Editor/UI/AssetEditor/AssetEditorWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/AssetEditor/ImGuiBezierWidget.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorFileUtils.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/Notification.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <functional>

namespace
{
	FAssetEditorWidget* GActiveAssetEditorWidget = nullptr;
	constexpr ImVec4 AssetEditorBlue = ImVec4(0.20f, 0.48f, 0.90f, 1.0f);
	constexpr ImVec4 AssetEditorBlueHover = ImVec4(0.26f, 0.58f, 0.98f, 1.0f);
	constexpr ImVec4 AssetEditorBlueActive = ImVec4(0.14f, 0.39f, 0.80f, 1.0f);
	constexpr ImVec4 AssetEditorBlueSoft = ImVec4(0.18f, 0.36f, 0.62f, 0.38f);
	constexpr ImVec4 AssetEditorBorder = ImVec4(0.34f, 0.36f, 0.40f, 0.96f);
	constexpr ImVec4 AssetEditorButtonBg = ImVec4(0.12f, 0.16f, 0.23f, 1.0f);

    constexpr ImVec4 AssetEditorHeaderBg = ImVec4(0.15f, 0.19f, 0.27f, 1.0f);
	constexpr ImVec4 AssetEditorVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
	constexpr ImVec4 AssetEditorVectorFieldBg = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
	constexpr ImVec4 AssetEditorVectorFieldHoverBg = ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f);
	constexpr ImVec4 AssetEditorVectorFieldActiveBg = ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f);
	constexpr float AssetEditorVectorLabelWidth = 56.0f;
	constexpr float AssetEditorPropertyLabelWidth = 136.0f;

	bool DrawAngularActionButton(const char* Label, const ImVec2& Size = ImVec2(0.0f, 0.0f))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, AssetEditorButtonBg);
		ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBlueSoft);
		const bool bPressed = ImGui::Button(Label, Size);
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
		return bPressed;
	}

	void PushAssetEditorVectorFieldStyle()
	{
		ImGui::PushStyleColor(ImGuiCol_FrameBg, AssetEditorVectorFieldBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, AssetEditorVectorFieldHoverBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, AssetEditorVectorFieldActiveBg);
		ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBorder);
	}

	void PopAssetEditorVectorFieldStyle()
	{
		ImGui::PopStyleColor(4);
	}

	FString GetFileNameUtf8(const std::filesystem::path& Path)
	{
		if (Path.empty())
		{
			return "Untitled";
		}
		return FPaths::ToUtf8(Path.filename().wstring());
	}

	bool IsWindowInHierarchy(const ImGuiWindow* Window, const ImGuiWindow* ExpectedRoot)
	{
		const ImGuiWindow* Current = Window;
		while (Current)
		{
			if (Current == ExpectedRoot)
			{
				return true;
			}
			Current = Current->ParentWindow;
		}
		return false;
	}

	bool IsCurrentAssetEditorCapturingInput()
	{
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
		{
			return true;
		}

		ImGuiContext& Context = *GImGui;
		ImGuiWindow* CurrentWindow = Context.CurrentWindow;
		if (!CurrentWindow)
		{
			return false;
		}

		ImGuiWindow* RootWindow = CurrentWindow->RootWindow ? CurrentWindow->RootWindow : CurrentWindow;
		if (ImGui::GetIO().WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			return true;
		}

		if (Context.ActiveIdWindow && IsWindowInHierarchy(Context.ActiveIdWindow, RootWindow))
		{
			return true;
		}

		return false;
	}

	void DrawStringInput(const char* Label, FString& Value, size_t BufferSize = 256)
	{
		TArray<char> Buffer;
		Buffer.resize(BufferSize);
		std::fill(Buffer.begin(), Buffer.end(), '\0');
		std::snprintf(Buffer.data(), BufferSize, "%s", Value.c_str());
		if (ImGui::InputText(Label, Buffer.data(), Buffer.size()))
		{
			Value = Buffer.data();
		}
	}

	bool DrawVector3Input(const char* Label, FVector& Value)
	{
		float Values[3] = { Value.X, Value.Y, Value.Z };
		if (ImGui::DragFloat3(Label, Values, 0.01f))
		{
			Value.X = Values[0];
			Value.Y = Values[1];
			Value.Z = Values[2];
			return true;
		}
		return false;
	}

	bool DrawRotatorInput(const char* Label, FRotator& Value)
	{
		float Values[3] = { Value.Pitch, Value.Yaw, Value.Roll };
		if (ImGui::DragFloat3(Label, Values, 0.01f))
		{
			Value.Pitch = Values[0];
			Value.Yaw = Values[1];
			Value.Roll = Values[2];
			return true;
		}
		return false;
	}

	FCameraShakeModifierAssetDesc MakeDefaultCameraShakeDesc(int32 Index)
	{
		FCameraShakeModifierAssetDesc Desc;
		Desc.EditorId = GenerateAssetEditorId();
		Desc.Name = "CameraShake_" + std::to_string(Index);
		FAssetBezierCurve EaseOut;
		EaseOut.ControlPoints[0] = 0.20f;
		EaseOut.ControlPoints[1] = 1.00f;
		EaseOut.ControlPoints[2] = 0.85f;
		EaseOut.ControlPoints[3] = 0.00f;
		Desc.Curves.CopyFrom(EaseOut);
		return Desc;
	}
}

void FAssetEditorWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	GActiveAssetEditorWidget = this;
}

void FAssetEditorWidget::Shutdown()
{
	if (GActiveAssetEditorWidget == this)
	{
		GActiveAssetEditorWidget = nullptr;
	}
	bCapturingInput = false;
	CloseCurrentAsset();
}

bool FAssetEditorWidget::OpenAssetFile(const std::filesystem::path& FilePath)
{
	if (!GActiveAssetEditorWidget)
	{
		FNotificationManager::Get().AddNotification("Asset Editor is not initialized.", ENotificationType::Error, 5.0f);
		return false;
	}
	return GActiveAssetEditorWidget->OpenAssetFromPath(FilePath);
}

bool FAssetEditorWidget::OpenAssetWithDialog(void* OwnerWindowHandle)
{
	const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
		.Filter = L"Asset Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
		.Title = L"Open Asset",
		.InitialDirectory = FPaths::AssetDir().c_str(),
		.OwnerWindowHandle = OwnerWindowHandle,
		.bFileMustExist = true,
		.bPathMustExist = true,
		.bPromptOverwrite = false,
		.bReturnRelativeToProjectRoot = false,
	});

	if (SelectedPath.empty())
	{
		return false;
	}

	return OpenAssetFromPath(std::filesystem::path(FPaths::ToWide(SelectedPath)));
}

bool FAssetEditorWidget::CreateCameraShakeAsset()
{
	CloseCurrentAsset();
	auto* NewAsset = UObjectManager::Get().CreateObject<UCameraModifierStackAssetData>();
	NewAsset->CameraShakes.push_back(MakeDefaultCameraShakeDesc(0));

	EditingAsset = NewAsset;
	EditingAssetPath.clear();
	SelectedEditorId = NewAsset->CameraShakes.front().EditorId;
	bOpen = true;
	bDirty = true;
	FNotificationManager::Get().AddNotification("Created new asset", ENotificationType::Success, 2.5f);
	return true;
}

bool FAssetEditorWidget::OpenAssetFromPath(const std::filesystem::path& FilePath)
{
	FString Error;
	UAssetData* LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(FilePath, &Error);
	if (!LoadedAsset)
	{
		FNotificationManager::Get().AddNotification(Error.empty() ? "Failed to load asset." : Error, ENotificationType::Error, 5.0f);
		return false;
	}

	CloseCurrentAsset();
	EditingAsset = LoadedAsset;
	EditingAssetPath = FilePath.lexically_normal();
	SelectedEditorId = 0;
	bOpen = true;
	bDirty = false;

	if (UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(EditingAsset))
	{
		StackAsset->EnsureValidEditorIds();
		if (!StackAsset->CameraShakes.empty())
		{
			SelectedEditorId = StackAsset->CameraShakes.front().EditorId;
		}
	}

	FNotificationManager::Get().AddNotification("Opened asset: " + GetFileNameUtf8(EditingAssetPath), ENotificationType::Success, 3.0f);
	return true;
}

bool FAssetEditorWidget::SaveCurrentAsset()
{
	if (!EditingAsset)
	{
		FNotificationManager::Get().AddNotification("No asset is open.", ENotificationType::Error, 3.0f);
		return false;
	}

	if (EditingAssetPath.empty() && !PromptForSavePath(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr))
	{
		return false;
	}

	FString Error;
	if (!FAssetFileSerializer::SaveAssetToFile(EditingAssetPath, EditingAsset, &Error))
	{
		FNotificationManager::Get().AddNotification(Error.empty() ? "Failed to save asset." : Error, ENotificationType::Error, 5.0f);
		return false;
	}

	bDirty = false;
	FNotificationManager::Get().AddNotification("Saved asset: " + GetFileNameUtf8(EditingAssetPath), ENotificationType::Success, 3.0f);
	return true;
}

bool FAssetEditorWidget::PromptForSavePath(void* OwnerWindowHandle)
{
	const FString SelectedPath = FEditorFileUtils::SaveFileDialog({
		.Filter = L"Asset Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
		.Title = L"Save Asset",
		.DefaultExtension = L"uasset",
		.InitialDirectory = FPaths::AssetDir().c_str(),
		.DefaultFileName = L"CameraShakeStack.uasset",
		.OwnerWindowHandle = OwnerWindowHandle,
		.bFileMustExist = false,
		.bPathMustExist = true,
		.bPromptOverwrite = true,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return false;
	}

	EditingAssetPath = std::filesystem::path(FPaths::ToWide(SelectedPath)).lexically_normal();
	return true;
}

void FAssetEditorWidget::CloseCurrentAsset()
{
	if (EditingAsset)
	{
		UObjectManager::Get().DestroyObject(EditingAsset);
		EditingAsset = nullptr;
	}
	EditingAssetPath.clear();
	SelectedEditorId = 0;
	bDirty = false;
}

void FAssetEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!bOpen)
	{
		bCapturingInput = false;
		return;
	}

	FString WindowTitle = "Asset Editor";
	if (EditingAsset)
	{
		WindowTitle += " - ";
		WindowTitle += GetFileNameUtf8(EditingAssetPath);
		if (bDirty)
		{
			WindowTitle += " *";
		}
	}

	bool bWindowOpen = bOpen;
	ImGui::SetNextWindowSize(ImVec2(980.0f, 620.0f), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoCollapse;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, AssetEditorBlueSoft);
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, AssetEditorBlueSoft);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AssetEditorBlueHover);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, AssetEditorBlueActive);
	ImGui::PushStyleColor(ImGuiCol_Header, AssetEditorHeaderBg);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, AssetEditorBlueHover);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, AssetEditorBlueActive);
	ImGui::PushStyleColor(ImGuiCol_CheckMark, AssetEditorBlue);
	ImGui::PushStyleColor(ImGuiCol_SliderGrab, AssetEditorBlueHover);
	ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, AssetEditorBlueActive);
	ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, AssetEditorBlueHover);
	ImGui::PushStyleColor(ImGuiCol_SeparatorActive, AssetEditorBlueActive);
	ImGui::PushStyleColor(ImGuiCol_PlotLines, AssetEditorBlueHover);
	ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBorder);
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		ImGui::PopStyleColor(14);
		ImGui::PopStyleVar(2);
		bOpen = bWindowOpen;
		bCapturingInput = bOpen;
		return;
	}
	bOpen = bWindowOpen;
	bCapturingInput = IsCurrentAssetEditorCapturingInput();

	DrawToolbar();
	ImGui::Separator();

	if (!EditingAsset)
	{
		ImGui::TextDisabled("Open a .uasset file from File > Open... or the Content Browser.");
		ImGui::End();
		ImGui::PopStyleColor(14);
		ImGui::PopStyleVar(2);
		return;
	}

	if (ImGui::BeginTable("AssetEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Asset Contents", ImGuiTableColumnFlags_WidthFixed, 280.0f);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextColumn();
		DrawAssetContents();

		ImGui::TableNextColumn();
		DrawDetails();

		ImGui::EndTable();
	}

	ImGui::End();
	ImGui::PopStyleColor(14);
	ImGui::PopStyleVar(2);
}

void FAssetEditorWidget::DrawToolbar()
{
	if (DrawAngularActionButton("New##AssetEditorNewAsset"))
	{
		CreateCameraShakeAsset();
	}
	ImGui::SameLine();
	if (DrawAngularActionButton("Open...##AssetEditorOpen"))
	{
		OpenAssetWithDialog(nullptr);
	}
	ImGui::SameLine();
	if (DrawAngularActionButton("Save##AssetEditorSave"))
	{
		SaveCurrentAsset();
	}
	ImGui::SameLine();
	if (EditingAsset)
	{
		ImGui::TextDisabled("%s", EditingAsset->GetClass()->GetName());
	}
}

void FAssetEditorWidget::DrawAssetContents()
{
	ImGui::BeginChild("Asset Contents", ImVec2(0, 0), true);
	ImGui::TextUnformatted("Asset Contents");
	ImGui::Separator();

	if (UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(EditingAsset))
	{
		DrawCameraModifierStackAssetContents(*StackAsset);
	}
	else
	{
		ImGui::TextDisabled("Unsupported asset root.");
	}

	ImGui::EndChild();
}

void FAssetEditorWidget::DrawDetails()
{
	ImGui::BeginChild("Details", ImVec2(0, 0), true);
	ImGui::TextUnformatted("Details");
	ImGui::Separator();

	if (UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(EditingAsset))
	{
		DrawCameraModifierStackAssetDetails(*StackAsset);
	}
	else
	{
		ImGui::TextDisabled("No Details panel is registered for this asset type.");
	}

	ImGui::EndChild();
}

bool FAssetEditorWidget::DrawLabeledField(const char* Label, const std::function<bool()>& DrawField)
{
	const float RowStartX = ImGui::GetCursorPosX();
	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float LabelTextWidth = ImGui::CalcTextSize(Label).x;
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float DesiredLabelWidth = (std::max)(AssetEditorPropertyLabelWidth, LabelTextWidth + Style.ItemSpacing.x + Style.FramePadding.x * 2.0f);
	const float MaxLabelWidth = (std::max)(AssetEditorPropertyLabelWidth, TotalWidth * 0.48f);
	const float LabelColumnWidth = (std::min)(DesiredLabelWidth, MaxLabelWidth);

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(Label);
	ImGui::SameLine(RowStartX + LabelColumnWidth);

	const float FieldWidth = TotalWidth - LabelColumnWidth;
	if (FieldWidth > 0.0f)
	{
		ImGui::SetNextItemWidth(FieldWidth);
	}

	return DrawField();
}

bool FAssetEditorWidget::DrawCurveEditor(const char* Label, FAssetBezierCurve& Curve)
{
	bool bChanged = false;
	ImGui::PushID(Label);
	if (!ImGui::CollapsingHeader(Label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PopID();
		return false;
	}
	if (DrawAngularActionButton("Linear"))
	{
		Curve.ResetLinear();
		bChanged = true;
	}
	ImGui::SameLine();
	if (DrawAngularActionButton("Ease Out"))
	{
		Curve.ControlPoints[0] = 0.20f;
		Curve.ControlPoints[1] = 1.00f;
		Curve.ControlPoints[2] = 0.85f;
		Curve.ControlPoints[3] = 0.00f;
		bChanged = true;
	}

	bChanged |= DrawCurveControlPointRow("P1", Curve.ControlPoints[0], Curve.ControlPoints[1]);
	bChanged |= DrawCurveControlPointRow("P2", Curve.ControlPoints[2], Curve.ControlPoints[3]);
	bChanged |= ImGui::Bezier("##Bezier", Curve.ControlPoints) != 0;
	const float PreviewTime = 0.5f;
	ImGui::TextDisabled("Preview t=%.2f value=%.3f", PreviewTime, Curve.Evaluate(PreviewTime));
	ImGui::PopID();
	return bChanged;
}

bool FAssetEditorWidget::DrawCurveControlPointRow(const char* Label, float& XValue, float& YValue)
{
	bool bChanged = false;
	ImGui::PushID(Label);
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, AssetEditorVectorLabelColor);
	ImGui::TextUnformatted(Label);
	ImGui::PopStyleColor();
	ImGui::SameLine(AssetEditorVectorLabelWidth);

	const float TotalWidth = ImGui::GetContentRegionAvail().x;
	const float BarWidth = 3.0f;
	const float FieldSpacing = 3.0f;
	const float InterFieldSpacing = ImGui::GetStyle().ItemSpacing.x;
	const float FieldWidth = (std::max)(48.0f, (TotalWidth - InterFieldSpacing - (BarWidth + FieldSpacing) * 2.0f) * 0.5f);
	const ImVec4 AxisColors[2] = { ImVec4(0.85f, 0.22f, 0.22f, 1.0f), ImVec4(0.36f, 0.74f, 0.25f, 1.0f) };
	float* Values[2] = { &XValue, &YValue };
	const char* AxisIds[2] = { "##X", "##Y" };

	for (int Axis = 0; Axis < 2; ++Axis)
	{
		if (Axis > 0)
		{
			ImGui::SameLine();
		}

		const ImVec2 Start = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(
			Start,
			ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
			ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]),
			2.0f);
		ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + FieldSpacing, Start.y));
		PushAssetEditorVectorFieldStyle();
		ImGui::SetNextItemWidth(FieldWidth);
		bChanged |= ImGui::DragFloat(AxisIds[Axis], Values[Axis], 0.01f, -1.0f, 2.0f, "%.3f");
		PopAssetEditorVectorFieldStyle();
	}

	ImGui::PopID();
	return bChanged;
}

void FAssetEditorWidget::DrawCameraModifierStackAssetContents(UCameraModifierStackAssetData& Asset)
{
	if (DrawAngularActionButton("Add##AssetEditorAddEntry"))
	{
		ImGui::OpenPopup("AddAssetEntryPopup");
	}

	if (ImGui::BeginPopup("AddAssetEntryPopup"))
	{
		if (ImGui::Selectable("Camera Shake"))
		{
			Asset.CameraShakes.push_back(MakeDefaultCameraShakeDesc(static_cast<int32>(Asset.CameraShakes.size())));
			SelectedEditorId = Asset.CameraShakes.back().EditorId;
			MarkDirty();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	const bool bCanDelete = FindSelectedCameraShake(Asset) != nullptr;
	if (!bCanDelete)
	{
		ImGui::BeginDisabled();
	}
	if (DrawAngularActionButton("Delete"))
	{
		Asset.CameraShakes.erase(
			std::remove_if(
				Asset.CameraShakes.begin(),
				Asset.CameraShakes.end(),
				[this](const FCameraShakeModifierAssetDesc& Desc)
				{
					return Desc.EditorId == SelectedEditorId;
				}),
			Asset.CameraShakes.end());
		SelectedEditorId = Asset.CameraShakes.empty() ? 0 : Asset.CameraShakes.front().EditorId;
		MarkDirty();
	}
	if (!bCanDelete)
	{
		ImGui::EndDisabled();
	}

	ImGui::Separator();

	for (int32 Index = 0; Index < static_cast<int32>(Asset.CameraShakes.size()); ++Index)
	{
		FCameraShakeModifierAssetDesc& Desc = Asset.CameraShakes[Index];
		ImGui::PushID(static_cast<int>(Desc.EditorId & 0x7fffffff));
		const bool bSelected = (SelectedEditorId == Desc.EditorId);
		FString Label = Desc.Name.empty() ? ("CameraShake_" + std::to_string(Index)) : Desc.Name;
		Label += "##CameraShakeItem";
		if (ImGui::Selectable(Label.c_str(), bSelected))
		{
			SelectedEditorId = Desc.EditorId;
		}
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Duplicate"))
			{
				FCameraShakeModifierAssetDesc Copy = Desc;
				Copy.EditorId = GenerateAssetEditorId();
				Copy.Name += "_Copy";
				Asset.CameraShakes.insert(Asset.CameraShakes.begin() + Index + 1, Copy);
				SelectedEditorId = Copy.EditorId;
				MarkDirty();
			}
			if (ImGui::MenuItem("Delete"))
			{
				Asset.CameraShakes.erase(Asset.CameraShakes.begin() + Index);
				SelectedEditorId = Asset.CameraShakes.empty() ? 0 : Asset.CameraShakes.front().EditorId;
				MarkDirty();
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}
}

void FAssetEditorWidget::DrawCameraModifierStackAssetDetails(UCameraModifierStackAssetData& Asset)
{
	FCameraShakeModifierAssetDesc* Desc = FindSelectedCameraShake(Asset);
	if (!Desc)
	{
		ImGui::TextDisabled("Select an item from Asset Contents.");
		return;
	}

	if (DrawCameraShakeDetails(*Desc))
	{
		MarkDirty();
	}
}

FCameraShakeModifierAssetDesc* FAssetEditorWidget::FindSelectedCameraShake(UCameraModifierStackAssetData& Asset)
{
	for (FCameraShakeModifierAssetDesc& Desc : Asset.CameraShakes)
	{
		if (Desc.EditorId == SelectedEditorId)
		{
			return &Desc;
		}
	}
	return nullptr;
}

bool FAssetEditorWidget::DrawCameraShakeDetails(FCameraShakeModifierAssetDesc& Desc)
{
	bool bChanged = false;

	if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bChanged |= DrawLabeledField("Name", [&]()
		{
			const FString OldName = Desc.Name;
			DrawStringInput("##Name", Desc.Name);
			return OldName != Desc.Name;
		});
		DrawLabeledField("EditorId", [&]()
		{
			ImGui::TextDisabled("%llu", static_cast<unsigned long long>(Desc.EditorId));
			return false;
		});
	}

	if (ImGui::CollapsingHeader("Common", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int Priority = static_cast<int>(Desc.Common.Priority);
		bChanged |= DrawLabeledField("Priority", [&]()
		{
			if (ImGui::DragInt("##Priority", &Priority, 1.0f, 0, 255))
			{
				Desc.Common.Priority = static_cast<uint8>(Priority);
				return true;
			}
			return false;
		});
		bChanged |= DrawLabeledField("Alpha In Time", [&]() { return ImGui::DragFloat("##AlphaInTime", &Desc.Common.AlphaInTime, 0.01f, 0.0f, 10.0f); });
		bChanged |= DrawLabeledField("Alpha Out Time", [&]() { return ImGui::DragFloat("##AlphaOutTime", &Desc.Common.AlphaOutTime, 0.01f, 0.0f, 10.0f); });
		bChanged |= DrawLabeledField("Start Disabled", [&]() { return ImGui::Checkbox("##StartDisabled", &Desc.Common.bStartDisabled); });
	}

	if (ImGui::CollapsingHeader("Camera Shake", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bChanged |= DrawLabeledField("Duration", [&]() { return ImGui::DragFloat("##Duration", &Desc.Duration, 0.01f, 0.0f, 30.0f); });
		bChanged |= DrawLabeledField("Intensity", [&]() { return ImGui::DragFloat("##Intensity", &Desc.Intensity, 0.01f, 0.0f, 20.0f); });
		bChanged |= DrawLabeledField("Frequency", [&]() { return ImGui::DragFloat("##Frequency", &Desc.Frequency, 0.10f, 0.0f, 200.0f); });
		bChanged |= DrawLabeledField("Location Amplitude", [&]() { return DrawVector3Input("##LocationAmplitude", Desc.LocationAmplitude); });
		bChanged |= DrawLabeledField("Rotation Amplitude", [&]() { return DrawRotatorInput("##RotationAmplitude", Desc.RotationAmplitude); });
		bChanged |= DrawLabeledField("Use Curves", [&]() { return ImGui::Checkbox("##UseCurves", &Desc.bUseCurves); });
	}

	if (ImGui::CollapsingHeader("Curve Editor", ImGuiTreeNodeFlags_DefaultOpen) && Desc.bUseCurves)
	{
		if (DrawAngularActionButton("All Linear"))
		{
			Desc.Curves.ResetLinear();
			bChanged = true;
		}
		ImGui::SameLine();
		if (DrawAngularActionButton("All Ease Out"))
		{
			FAssetBezierCurve EaseOut;
			EaseOut.ControlPoints[0] = 0.20f;
			EaseOut.ControlPoints[1] = 1.00f;
			EaseOut.ControlPoints[2] = 0.85f;
			EaseOut.ControlPoints[3] = 0.00f;
			Desc.Curves.CopyFrom(EaseOut);
			bChanged = true;
		}

		bChanged |= DrawCurveEditor("Translation X", Desc.Curves.TranslationX);
		bChanged |= DrawCurveEditor("Translation Y", Desc.Curves.TranslationY);
		bChanged |= DrawCurveEditor("Translation Z", Desc.Curves.TranslationZ);
		bChanged |= DrawCurveEditor("Rotation X", Desc.Curves.RotationX);
		bChanged |= DrawCurveEditor("Rotation Y", Desc.Curves.RotationY);
		bChanged |= DrawCurveEditor("Rotation Z", Desc.Curves.RotationZ);
	}

	return bChanged;
}
