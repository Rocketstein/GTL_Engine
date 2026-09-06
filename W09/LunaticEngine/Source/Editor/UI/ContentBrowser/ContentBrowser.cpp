#include "ContentBrowser.h"

#include "ContentBrowserElement.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Engine/Runtime/Engine.h"
#include "Core/AsciiUtils.h"
#include "Materials/MaterialManager.h"
#include "Texture/Texture2D.h"
#include "WICTextureLoader.h"
#include "Resource/ResourceManager.h"

#include <algorithm>
#include <fstream>
#include <string>

namespace
{
	bool IsParentDirectoryReference(const std::filesystem::path& Path);

	FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	FString MakeContentBrowserDisplayPath(const std::wstring& CurrentPath)
	{
		const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		const std::filesystem::path Path = std::filesystem::path(CurrentPath).lexically_normal();
		const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);

		if (Path == RootPath)
		{
			return "Project Root";
		}

		if (!RelativePath.empty() && !IsParentDirectoryReference(RelativePath))
		{
			return FPaths::ToUtf8(RelativePath.generic_wstring());
		}

		return FPaths::ToUtf8(Path.generic_wstring());
	}

	bool IsParentDirectoryReference(const std::filesystem::path& Path)
	{
		for (const std::filesystem::path& Part : Path)
		{
			if (Part == L"..")
			{
				return true;
			}
		}

		return false;
	}

	FString MakeContentBrowserSettingsPath(const std::wstring& CurrentPath)
	{
		const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		const std::filesystem::path Path = std::filesystem::path(CurrentPath).lexically_normal();
		const std::filesystem::path RelativePath = Path.lexically_relative(RootPath);

		if (!RelativePath.empty() && !IsParentDirectoryReference(RelativePath))
		{
			return FPaths::ToUtf8(RelativePath.generic_wstring());
		}

		return FPaths::ToUtf8(Path.wstring());
	}

	std::filesystem::path GetContentBrowserVirtualRoot();
	std::filesystem::path GetPrimaryContentRoot();
	std::filesystem::path GetScriptRoot();
	bool IsContentBrowserTopLevelPath(const std::filesystem::path& Path);

	std::wstring ResolveContentBrowserSettingsPath(const FString& SavedPath)
	{
		if (SavedPath.empty())
		{
			return GetContentBrowserVirtualRoot().wstring();
		}

		std::filesystem::path Path(FPaths::ToWide(SavedPath));
		if (!Path.is_absolute())
		{
			Path = std::filesystem::path(FPaths::RootDir()) / Path;
		}

		Path = Path.lexically_normal();
		if (std::filesystem::exists(Path) && std::filesystem::is_directory(Path))
		{
			return Path.wstring();
		}

		return GetContentBrowserVirtualRoot().wstring();
	}

	bool IsSubPath(const std::filesystem::path& parent, const std::filesystem::path& child)
	{
		std::filesystem::path p = std::filesystem::weakly_canonical(parent);
		std::filesystem::path c = std::filesystem::weakly_canonical(child);

		auto pIt = p.begin();
		auto cIt = c.begin();

		for (; pIt != p.end() && cIt != c.end(); ++pIt, ++cIt)
		{
			if (*pIt != *cIt)
				return false;
		}

		return pIt == p.end(); // parent ?앷퉴吏 ??留욎븯?쇰㈃ ?ы븿??
	}
	std::filesystem::path GetContentBrowserVirtualRoot()
	{
		return std::filesystem::path(FPaths::RootDir()).lexically_normal();
	}

	std::filesystem::path GetPrimaryContentRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Content").lexically_normal();
	}

	std::filesystem::path GetScriptRoot()
	{
		return (std::filesystem::path(FPaths::RootDir()).parent_path() / L"Scripts").lexically_normal();
	}

	bool IsContentBrowserTopLevelPath(const std::filesystem::path& Path)
	{
		return Path.lexically_normal() == GetContentBrowserVirtualRoot();
	}

	std::string ToLowerUtf8(std::string Value)
	{
		for (char& Character : Value)
		{
			Character = AsciiUtils::ToLower(Character);
		}
		return Value;
	}

	ID3D11ShaderResourceView* GetTexturePreviewSRV(const std::filesystem::path& Path)
	{
		const FString RelativePath = FPaths::ToUtf8(Path.lexically_relative(FPaths::RootDir()).generic_wstring());
		if (UTexture2D* CachedTexture = UTexture2D::LoadFromCached(RelativePath))
		{
			return CachedTexture->GetSRV();
		}

		if (!GEngine)
		{
			return nullptr;
		}

		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (!Device)
		{
			return nullptr;
		}

		if (UTexture2D* Texture = UTexture2D::LoadFromFile(RelativePath, Device))
		{
			return Texture->GetSRV();
		}

		return nullptr;
	}

	ID3D11ShaderResourceView* GetMaterialPreviewSRV(const std::filesystem::path& Path)
	{
		const FString RelativePath = FPaths::ToUtf8(Path.lexically_relative(FPaths::RootDir()).generic_wstring());
		if (UTexture2D* PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(RelativePath))
		{
			return PreviewTexture->GetSRV();
		}

		return nullptr;
	}

	FString TrimAscii(const FString& Value)
	{
		const size_t Start = Value.find_first_not_of(" \t\r\n");
		if (Start == FString::npos)
		{
			return "";
		}

		const size_t End = Value.find_last_not_of(" \t\r\n");
		return Value.substr(Start, End - Start + 1);
	}

	ID3D11ShaderResourceView* GetMtlPreviewSRV(const std::filesystem::path& Path)
	{
		if (!std::filesystem::exists(Path))
		{
			return nullptr;
		}

		std::ifstream File(Path);
		if (!File.is_open())
		{
			return nullptr;
		}

		FString RelativeMtlPath = FPaths::ToUtf8(Path.lexically_relative(FPaths::RootDir()).generic_wstring());
		FString Line;
		while (std::getline(File, Line))
		{
			const FString Trimmed = TrimAscii(Line);
			if (Trimmed.empty() || Trimmed[0] == '#')
			{
				continue;
			}

			if (Trimmed.rfind("map_Kd", 0) != 0)
			{
				continue;
			}

			FString TextureSpec = TrimAscii(Trimmed.substr(6));
			if (TextureSpec.empty())
			{
				continue;
			}

			while (!TextureSpec.empty() && TextureSpec[0] == '-')
			{
				size_t OptionEnd = TextureSpec.find_first_of(" \t");
				if (OptionEnd == FString::npos)
				{
					TextureSpec.clear();
					break;
				}

				const FString Option = TextureSpec.substr(0, OptionEnd);
				TextureSpec = TrimAscii(TextureSpec.substr(OptionEnd + 1));

				int32 ArgsToSkip = 0;
				if (Option == "-s" || Option == "-o" || Option == "-t")
				{
					ArgsToSkip = 3;
				}
				else if (Option == "-mm")
				{
					ArgsToSkip = 2;
				}
				else if (Option == "-bm" || Option == "-boost" || Option == "-texres"
					|| Option == "-blendu" || Option == "-blendv" || Option == "-clamp"
					|| Option == "-cc" || Option == "-imfchan")
				{
					ArgsToSkip = 1;
				}

				for (int32 ArgIndex = 0; ArgIndex < ArgsToSkip && !TextureSpec.empty(); ++ArgIndex)
				{
					size_t ArgEnd = TextureSpec.find_first_of(" \t");
					if (ArgEnd == FString::npos)
					{
						TextureSpec.clear();
						break;
					}
					TextureSpec = TrimAscii(TextureSpec.substr(ArgEnd + 1));
				}
			}

			if (TextureSpec.empty())
			{
				continue;
			}

			const FString ResolvedTexturePath = FPaths::ResolveAssetPath(RelativeMtlPath, TextureSpec);
			const std::filesystem::path AbsoluteTexturePath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(ResolvedTexturePath);
			return GetTexturePreviewSRV(AbsoluteTexturePath.lexically_normal());
		}

		return nullptr;
	}
}

void FEditorContentBrowserWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	FEditorWidget::Initialize(InEditor);
	if (!InDevice) return;

	ICons["Default"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Default"));
	ICons["Directory"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Directory"));
	ICons["FolderClosed"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.FolderClosed"));
	ICons["FolderOpen"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.FolderOpen"));
	ICons[".Scene"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Scene"));
	ICons[".umap"] = ICons[".Scene"];
	ICons[".UMAP"] = ICons[".Scene"];
	ICons[".obj"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Mesh"));
	ICons[".mat"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Material"));
	ICons[".mtl"] = FResourceManager::Get().FindLoadedTexture(GetEditorPathResource("Editor.Icon.ContentBrowser.Material"));

	ContentBrowserContext Context;
	Context.ContentSize = ImVec2(92.0f, 126.0f);
	Context.EditorEngine = InEditor;
	BrowserContext = Context;
	LoadFromSettings();

	Refresh();
}

void FEditorContentBrowserWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	FEditorSettings& Settings = FEditorSettings::Get();
	if (!Settings.UI.bContentBrowser)
	{
		return;
	}

	constexpr const char* PanelIconKey = "Editor.Icon.Panel.ContentBrowser";
	const std::string WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Content Browser", PanelIconKey);
	const bool bIsOpen = ImGui::Begin(WindowTitle.c_str());
	EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
	EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Content Browser", Settings.UI.bContentBrowser, "x##CloseContentBrowser");
	if (!bIsOpen)
	{
		ImGui::End();
		return;
	}
	EditorPanelTitleUtils::ApplyPanelContentTopInset();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 6.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.16f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorAccentColor::WithAlpha(1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.42f, 0.78f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.30f, 0.34f, 1.0f));
	if (ImGui::Button("Refresh##ContentBrowser"))
	{
		Refresh();
		SaveToSettings();
	}
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(3);

	ImGui::SameLine();
	ImGui::TextDisabled("%s", MakeContentBrowserDisplayPath(BrowserContext.CurrentPath).c_str());

	int size = static_cast<int>(BrowserContext.ContentSize.x);
	//ImGui::SliderInt("##slider", &size, 20, 100);
	BrowserContext.ContentSize = ImVec2(static_cast<float>(size), static_cast<float>(size) + 46.0f);

	if (!ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::End();
		return;
	}

	ImGui::TableSetupColumn("Directory", ImGuiTableColumnFlags_WidthFixed, 250.0f);
	ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);

	ImGui::TableNextColumn();
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 6.0f));
		ImGui::BeginChild("DirectoryTree", ImVec2(0, 0), true);
		DrawDirNode(RootNode);
		BrowserContext.PendingRevealPath.clear();
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	ImGui::TableNextColumn();
	{
		ImGui::BeginChild("ContentArea", ImVec2(0, 0), true);
		DrawContents();
		ImGui::EndChild();
	}

	if (BrowserContext.SelectedElement)
		BrowserContext.SelectedElement->RenderDetail();

	ImGui::EndTable();
	ImGui::End();
}

void FEditorContentBrowserWidget::Refresh()
{
	const std::filesystem::path CurrentPath = std::filesystem::path(BrowserContext.CurrentPath).lexically_normal();
	if (!IsContentBrowserTopLevelPath(CurrentPath)
		&& (!std::filesystem::exists(CurrentPath) || !std::filesystem::is_directory(CurrentPath)))
	{
		BrowserContext.CurrentPath = GetContentBrowserVirtualRoot().wstring();
	}

	BrowserContext.PendingRevealPath = BrowserContext.CurrentPath;
	RootNode = BuildDirectoryTree(GetContentBrowserVirtualRoot());
	RefreshContent();

	BrowserContext.bIsNeedRefresh = false;
}

void FEditorContentBrowserWidget::SetIconSize(float Size)
{
	const float ClampedSize = (std::max)(84.0f, (std::min)(Size, 124.0f));
	BrowserContext.ContentSize = ImVec2(ClampedSize, ClampedSize + 34.0f);
}

void FEditorContentBrowserWidget::LoadFromSettings()
{
	BrowserContext.CurrentPath = ResolveContentBrowserSettingsPath(FEditorSettings::Get().ContentBrowserPath);
	BrowserContext.PendingRevealPath = BrowserContext.CurrentPath;
}

void FEditorContentBrowserWidget::SaveToSettings() const
{
	FEditorSettings::Get().ContentBrowserPath = MakeContentBrowserSettingsPath(BrowserContext.CurrentPath);
}

void FEditorContentBrowserWidget::RefreshContent()
{
	CachedBrowserElements.clear();
	TArray<FContentItem> CurrentContents = ReadDirectory(BrowserContext.CurrentPath);
	for (const auto& Content : CurrentContents)
	{
		std::shared_ptr<ContentBrowserElement> element;
		const std::string Extension = ToLowerUtf8(FPaths::ToUtf8(Content.Path.extension()));

		if (Content.bIsDirectory)
		{
			element = std::make_shared<DirectoryElement>();
			element.get()->SetIcon(ICons["Directory"].Get());

		}
		else if (Content.Path.extension() == ".Scene" || Content.Path.extension() == ".umap" || Content.Path.extension() == ".UMAP")
		{
			element = std::make_shared<SceneElement>();
			element.get()->SetIcon(ICons[".Scene"].Get());
		}
		else if (Extension == ".obj")
		{
			element = std::make_shared<ObjectElement>();
			element.get()->SetIcon(ICons[".obj"].Get());
		}
		else if (Extension == ".mat")
		{
			element = std::make_shared<MaterialElement>();
			ID3D11ShaderResourceView* PreviewSRV = GetMaterialPreviewSRV(Content.Path);
			element.get()->SetIcon(PreviewSRV ? PreviewSRV : ICons[".mat"].Get());
		}
		else if (Extension == ".mtl")
		{
			element = std::make_shared<MtlElement>();
			ID3D11ShaderResourceView* PreviewSRV = GetMtlPreviewSRV(Content.Path);
			element.get()->SetIcon(PreviewSRV ? PreviewSRV : ICons[".mtl"].Get());
		}
		else if (Extension == ".uasset")
		{
			element = std::make_shared<UAssetElement>();
			element.get()->SetIcon(ICons["Default"].Get());
		}
		else if (UTexture2D::IsSupportedTextureExtension(Content.Path))
		{
			element = std::make_shared<PNGElement>();
			ID3D11ShaderResourceView* PreviewSRV = GetTexturePreviewSRV(Content.Path);
			element.get()->SetIcon(PreviewSRV ? PreviewSRV : ICons["Default"].Get());
		}
		else
		{
			element = std::make_shared<ContentBrowserElement>();
			element.get()->SetIcon(ICons["Default"].Get());
		}
		
		element.get()->SetContent(Content);
		CachedBrowserElements.push_back(std::move(element));
	}
}

void FEditorContentBrowserWidget::DrawDirNode(FDirNode InNode)
{
	ImGuiTreeNodeFlags Flag =
		(InNode.Children.empty() ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_OpenOnArrow) |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (InNode.Self.Path == BrowserContext.CurrentPath)
	{
		Flag |= ImGuiTreeNodeFlags_Selected;
	}
	if (!BrowserContext.PendingRevealPath.empty() && IsSubPath(InNode.Self.Path, BrowserContext.PendingRevealPath))
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	}

	const std::string NodeId = "##" + FPaths::ToUtf8(InNode.Self.Path.generic_wstring());
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.66f, 0.68f, 1.0f));
	bool bIsOpen = ImGui::TreeNodeEx(NodeId.c_str(), Flag);
	ImGui::PopStyleColor();

	const ImVec2 Min = ImGui::GetItemRectMin();
	const float IconSize = 16.0f;
	const float IconY = Min.y + (ImGui::GetTextLineHeight() - IconSize) * 0.5f;
	const float ArrowWidth = 18.0f;
	const float IconX = Min.x + ArrowWidth + 5.0f;
	const float TextX = IconX + IconSize + 5.0f;
	const float TextY = Min.y + (ImGui::GetTextLineHeight() - ImGui::CalcTextSize(FPaths::ToUtf8(InNode.Self.Name).c_str()).y) * 0.5f;
	ID3D11ShaderResourceView* FolderIcon = bIsOpen ? ICons["FolderOpen"].Get() : ICons["FolderClosed"].Get();
	if (FolderIcon)
	{
		ImGui::GetWindowDrawList()->AddImage(
			reinterpret_cast<ImTextureID>(FolderIcon),
			ImVec2(IconX, IconY),
			ImVec2(IconX + IconSize, IconY + IconSize),
			ImVec2(0.0f, 0.0f),
			ImVec2(1.0f, 1.0f),
			IM_COL32(184, 140, 58, 255));
	}
	const ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
	ImGui::GetWindowDrawList()->AddText(
		ImVec2(TextX, TextY),
		TextColor,
		FPaths::ToUtf8(InNode.Self.Name).c_str());
	if (ImGui::IsItemClicked())
	{
		BrowserContext.CurrentPath = InNode.Self.Path;
		RefreshContent();
	}

	if (!bIsOpen)
	{
		return;
	}

	int32 ChildrenCount = static_cast<int32>(InNode.Children.size());
	for (int i = 0; i < ChildrenCount; i++)
	{
		DrawDirNode(InNode.Children[i]);
	}

	ImGui::TreePop();
}

void FEditorContentBrowserWidget::DrawContents()
{
	int elementCount = static_cast<int>(CachedBrowserElements.size());

	const float contentWidth = ImGui::GetContentRegionAvail().x;
	const float itemWidth = BrowserContext.ContentSize.x;
	const float itemHeight = BrowserContext.ContentSize.y;
	const float MinGap = 10.0f;

	int columnCount = static_cast<int>((contentWidth + MinGap) / (itemWidth + MinGap));
	if (columnCount < 1)
	{
		columnCount = 1;
	}

	float gapSize = MinGap;
	if (columnCount > 1)
	{
		gapSize = (std::max)(MinGap, (contentWidth - itemWidth * columnCount) / (columnCount - 1));
	}

	ImVec2 startPos = ImGui::GetCursorPos();

	for (int i = 0; i < elementCount; ++i)
	{
		int column = i % columnCount;
		int row = i / columnCount;

		float x = startPos.x + column * (itemWidth + gapSize);
		float y = startPos.y + row * (itemHeight + gapSize);

		ImGui::SetCursorPos(ImVec2(x, y));
		CachedBrowserElements[i]->Render(BrowserContext);
	}

	if (BrowserContext.bIsNeedRefresh)
	{
		Refresh();
		SaveToSettings();
	}

	int rowCount = (elementCount + columnCount - 1) / columnCount;
	const float TotalHeight = rowCount * itemHeight + (rowCount > 0 ? (rowCount - 1) * gapSize : 0.0f);
	ImGui::SetCursorPos(startPos);
	ImGui::Dummy(ImVec2((std::max)(itemWidth, contentWidth), TotalHeight));
}

TArray<FContentItem> FEditorContentBrowserWidget::ReadDirectory(std::wstring Path)
{
	TArray<FContentItem> Items;
	const std::filesystem::path CurrentPath = std::filesystem::path(Path).lexically_normal();

	if (IsContentBrowserTopLevelPath(CurrentPath))
	{
		for (const std::filesystem::path& Root : { GetPrimaryContentRoot(), GetScriptRoot() })
		{
			if (!std::filesystem::exists(Root) || !std::filesystem::is_directory(Root))
			{
				continue;
			}

			FContentItem Item;
			Item.Path = Root;
			Item.Name = Root.filename().wstring();
			Item.bIsDirectory = true;
			Items.push_back(Item);
		}

		return Items;
	}

	if (!std::filesystem::exists(CurrentPath) || !std::filesystem::is_directory(CurrentPath))
		return Items;

	for (const auto& Entry : std::filesystem::directory_iterator(CurrentPath))
	{
		std::wstring Name = Entry.path().filename().wstring();
		if (Entry.is_directory())
		{
			if (Name == L"Bin" || Name == L"Build" || Name == L".git" || Name == L".vs")
				continue;
		}

		FContentItem Item;
		Item.Path = Entry.path();
		Item.Name = Name;
		Item.bIsDirectory = Entry.is_directory();

		Items.push_back(Item);
	}

	std::sort(Items.begin(), Items.end(),
		[](const FContentItem& A, const FContentItem& B)
		{
			if (A.bIsDirectory != B.bIsDirectory)
				return A.bIsDirectory > B.bIsDirectory;

			return A.Name < B.Name;
		});

	return Items;
}

FEditorContentBrowserWidget::FDirNode FEditorContentBrowserWidget::BuildDirectoryTree(const std::filesystem::path& DirPath)
{
	FDirNode Node;
	Node.Self.Path = DirPath;
	Node.Self.Name = DirPath.filename().wstring();
	Node.Self.bIsDirectory = true;

	if (IsContentBrowserTopLevelPath(DirPath))
	{
		Node.Self.Name = L"All";
		for (const std::filesystem::path& Root : { GetPrimaryContentRoot(), GetScriptRoot() })
		{
			if (std::filesystem::exists(Root) && std::filesystem::is_directory(Root))
			{
				Node.Children.push_back(BuildDirectoryTree(Root));
			}
		}
		return Node;
	}

	for (const auto& Entry : std::filesystem::directory_iterator(DirPath))
	{
		if (!Entry.is_directory())
			continue;

		std::wstring DirName = Entry.path().filename().wstring();
		if (DirName == L"Bin" || DirName == L"Build" || DirName == L".git" || DirName == L".vs")
			continue;

		Node.Children.push_back(BuildDirectoryTree(Entry.path()));
	}

	if(Node.Self.Name.empty())
		Node.Self.Name = FPaths::ToWide("All");

	return Node;
}
