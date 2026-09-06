#include "ContentBrowserElement.h"
#include "Editor/UI/EditorFileUtils.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/AssetEditor/AssetEditorWidget.h"
#include "Platform/Paths.h"
#include "Core/Notification.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjImporter.h"

#include <algorithm>
#include <vector>

namespace
{
	std::vector<FString> WrapTextLines(const FString& Text, float MaxWidth, int MaxLines)
	{
		std::vector<FString> Lines;
		if (Text.empty() || MaxLines <= 0)
		{
			return Lines;
		}

		FString CurrentLine;
		for (char Character : Text)
		{
			FString Candidate = CurrentLine;
			Candidate.push_back(Character);

			if (!CurrentLine.empty() && ImGui::CalcTextSize(Candidate.c_str()).x > MaxWidth)
			{
				Lines.push_back(CurrentLine);
				CurrentLine.clear();
				if (static_cast<int>(Lines.size()) >= MaxLines)
				{
					break;
				}
			}

			if (static_cast<int>(Lines.size()) >= MaxLines)
			{
				break;
			}

			CurrentLine.push_back(Character);
		}

		if (static_cast<int>(Lines.size()) < MaxLines && !CurrentLine.empty())
		{
			Lines.push_back(CurrentLine);
		}

		if (Lines.empty())
		{
			Lines.push_back(Text);
		}

		if (static_cast<int>(Lines.size()) > MaxLines)
		{
			Lines.resize(MaxLines);
		}

		size_t UsedCharacters = 0;
		for (const FString& Line : Lines)
		{
			UsedCharacters += Line.size();
		}

		FString& LastLine = Lines.back();
		while (!LastLine.empty() && ImGui::CalcTextSize((LastLine + "...").c_str()).x > MaxWidth)
		{
			LastLine.pop_back();
		}

		if (UsedCharacters < Text.size())
		{
			LastLine += "...";
		}

		return Lines;
	}

	std::filesystem::path GetProtectedContentRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Content").lexically_normal();
	}

	std::filesystem::path GetProtectedScriptsRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()).parent_path() / L"Scripts").lexically_normal();
	}

	bool IsProtectedContentBrowserPath(const std::filesystem::path& InPath)
	{
		const std::filesystem::path NormalizedPath = InPath.lexically_normal();
		return NormalizedPath == GetProtectedContentRoot() || NormalizedPath == GetProtectedScriptsRoot();
	}
}

bool ContentBrowserElement::RenderSelectSpace(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement.get() == this;

	ImGui::InvisibleButton("##Element", Context.ContentSize);
	bool bIsClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImFont* font = ImGui::GetFont();
	const float fontSize = ImGui::GetFontSize();
	const float Padding = 8.0f;
	const float TextSpacing = 5.0f;
	const float WrapWidth = (Max.x - Min.x) - Padding * 2.0f;
	const FString DisplayName = GetDisplayName();
	const FString Subtitle = GetSubtitleText();
	const ImVec2 SubtitleTextSize = ImGui::CalcTextSize(Subtitle.c_str());
	const float CardRounding = 6.0f;
	const ImU32 BorderColor = bIsSelected ? EditorAccentColor::ToU32() : IM_COL32(62, 62, 66, 255);
	const ImU32 FillColor = bIsSelected ? EditorAccentColor::ToU32(48) : IM_COL32(36, 36, 39, 255);
	const ImU32 ThumbnailFill = IM_COL32(30, 30, 33, 255);
	DrawList->AddRectFilled(Min, Max, FillColor, CardRounding);
	DrawList->AddRect(Min, Max, BorderColor, CardRounding, 0, bIsSelected ? 1.5f : 1.0f);

	const float ThumbnailHeight = (std::min)(54.0f, (Max.y - Min.y) * 0.48f);
	ImVec2 ThumbnailMin(Min.x + Padding, Min.y + Padding);
	ImVec2 ThumbnailMax(Max.x - Padding, ThumbnailMin.y + ThumbnailHeight);
	DrawList->AddRectFilled(ThumbnailMin, ThumbnailMax, ThumbnailFill, 5.0f);
	DrawList->AddRect(ThumbnailMin, ThumbnailMax, IM_COL32(52, 52, 56, 255), 5.0f);

	const float IconSize = IsTexturePreview() ? ThumbnailHeight - 2.0f : (std::min)(34.0f, ThumbnailHeight - 12.0f);
	ImVec2 ImageMin(
		ThumbnailMin.x + ((ThumbnailMax.x - ThumbnailMin.x) - IconSize) * 0.5f,
		ThumbnailMin.y + ((ThumbnailMax.y - ThumbnailMin.y) - IconSize) * 0.5f);
	ImVec2 ImageMax(ImageMin.x + IconSize, ImageMin.y + IconSize);
	if (Icon)
	{
		DrawList->AddImage(Icon, ImageMin, ImageMax, ImVec2(0, 0), ImVec2(1, 1), GetIconTint());
	}

	const float NameTop = ThumbnailMax.y + TextSpacing;
	const auto Lines = WrapTextLines(DisplayName, WrapWidth, 2);
	for (int LineIndex = 0; LineIndex < static_cast<int>(Lines.size()); ++LineIndex)
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(Lines[LineIndex].c_str());
		const float TextX = Min.x + ((Max.x - Min.x) - TextSize.x) * 0.5f;
		const float TextY = NameTop + LineIndex * (fontSize + 2.0f);
		DrawList->AddText(font, fontSize, ImVec2(TextX, TextY), ImGui::GetColorU32(ImGuiCol_Text), Lines[LineIndex].c_str());
	}

	if (!Subtitle.empty())
	{
		const float SubtitleX = Min.x + ((Max.x - Min.x) - SubtitleTextSize.x) * 0.5f;
		const float SubtitleY = Max.y - Padding - SubtitleTextSize.y;
		DrawList->AddText(ImVec2(SubtitleX, SubtitleY), IM_COL32(155, 155, 160, 255), Subtitle.c_str());
	}
	ImGui::PopID();

	return bIsClicked;
}

void ContentBrowserElement::Render(ContentBrowserContext& Context)
{
	if (RenderSelectSpace(Context))
	{
		Context.SelectedElement = shared_from_this();
		bIsSelected = true;
		OnLeftClicked(Context);
	}

	bool bDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (bDoubleClicked)
	{
		OnDoubleLeftClicked(Context);
	}

	if (ImGui::BeginPopupContextItem())
	{
		DrawContextMenu(Context);
		ImGui::EndPopup();
	}

	if (ImGui::BeginDragDropSource())
	{
		RenderSelectSpace(Context);
		ImGui::SetDragDropPayload(GetDragItemType(), &ContentItem, sizeof(ContentItem));
		OnDrag(Context);
		ImGui::EndDragDropSource();
	}
}

FString ContentBrowserElement::GetDisplayName() const
{
	if (ContentItem.bIsDirectory)
	{
		return FPaths::ToUtf8(ContentItem.Name);
	}

	return FPaths::ToUtf8(ContentItem.Path.stem().wstring());
}

FString ContentBrowserElement::GetSubtitleText() const
{
	if (ContentItem.bIsDirectory)
	{
		return "Folder";
	}

	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension().wstring());
	if (!Extension.empty() && Extension.front() == '.')
	{
		Extension.erase(Extension.begin());
	}

	return Extension;
}

bool ContentBrowserElement::CanDelete() const
{
	return !IsProtectedContentBrowserPath(ContentItem.Path);
}

void ContentBrowserElement::DrawContextMenu(ContentBrowserContext& Context)
{
	const bool bCanOpen = std::filesystem::exists(ContentItem.Path);
	if (ImGui::MenuItem("Open", nullptr, false, bCanOpen))
	{
		OnDoubleLeftClicked(Context);
	}

	if (ImGui::MenuItem("Show in Explorer", nullptr, false, bCanOpen))
	{
		if (!FEditorFileUtils::RevealInExplorer(ContentItem.Path))
		{
			FNotificationManager::Get().AddNotification("Failed to reveal path in Explorer.", ENotificationType::Error, 5.0f);
		}
	}

	if (ImGui::MenuItem("Delete", nullptr, false, bCanOpen && CanDelete()))
	{
		if (FEditorFileUtils::DeletePath(ContentItem.Path))
		{
			if (Context.SelectedElement.get() == this)
			{
				Context.SelectedElement.reset();
			}
			Context.bIsNeedRefresh = true;
			FNotificationManager::Get().AddNotification("Deleted: " + GetDisplayName(), ENotificationType::Success, 3.0f);
		}
		else
		{
			FNotificationManager::Get().AddNotification("Failed to delete: " + GetDisplayName(), ENotificationType::Error, 5.0f);
		}
	}
}

void ContentBrowserElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	(void)Context;
	if (!FEditorFileUtils::OpenPath(ContentItem.Path))
	{
		FNotificationManager::Get().AddNotification("Failed to open: " + GetDisplayName(), ENotificationType::Error, 5.0f);
	}
}

void DirectoryElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	Context.CurrentPath = ContentItem.Path;
	Context.PendingRevealPath = ContentItem.Path;
	Context.bIsNeedRefresh = true;
}

#include "Serialization/SceneSaveManager.h"
#include "Editor/EditorEngine.h"
void SceneElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	std::filesystem::path ScenePath = ContentItem.Path;
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());
	UEditorEngine* EditorEngine = Context.EditorEngine;
	EditorEngine->LoadSceneFromPath(FilePath);
}

void MaterialElement::OnLeftClicked(ContentBrowserContext& Context)
{
	MaterialInspector = { ContentItem.Path };
}

void MaterialElement::RenderDetail()
{
	MaterialInspector.Render();
}

void MtlElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	TArray<FString> GeneratedMatPaths;
	const FString RelativeMtlPath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	if (FObjImporter::ImportMtl(RelativeMtlPath, &GeneratedMatPaths))
	{
		FMaterialManager::Get().ScanMaterialAssets();
		Context.bIsNeedRefresh = true;

		FString Message = "Imported MTL: " + RelativeMtlPath;
		if (!GeneratedMatPaths.empty())
		{
			Message += " (" + std::to_string(GeneratedMatPaths.size()) + " mat)";
		}
		FNotificationManager::Get().AddNotification(Message, ENotificationType::Success, 3.0f);
	}
	else
	{
		FNotificationManager::Get().AddNotification("Failed to import MTL: " + RelativeMtlPath, ENotificationType::Error, 5.0f);
	}
}


void UAssetElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	(void)Context;
	if (!FAssetEditorWidget::OpenAssetFile(ContentItem.Path))
	{
		FNotificationManager::Get().AddNotification("Failed to open asset: " + GetDisplayName(), ENotificationType::Error, 5.0f);
	}
}
