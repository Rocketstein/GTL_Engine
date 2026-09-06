#include "Editor/UI/Asset/Rml/RmlWidgetEditorWidget.h"

#include "UI/RmlWidgetAsset.h"

#include "Editor/EditorEngine.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	constexpr float DefaultElementWidth = 80.0f;
	constexpr float DefaultElementHeight = 28.0f;

	FString TrimCopy(const FString& Value)
	{
		size_t Begin = 0;
		while (Begin < Value.size() && std::isspace(static_cast<unsigned char>(Value[Begin])))
		{
			++Begin;
		}

		size_t End = Value.size();
		while (End > Begin && std::isspace(static_cast<unsigned char>(Value[End - 1])))
		{
			--End;
		}

		return Value.substr(Begin, End - Begin);
	}

	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});
		return Value;
	}

	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path Result(FPaths::ToWide(Path));
		if (!Result.is_absolute())
		{
			Result = std::filesystem::path(FPaths::RootDir()) / Result;
		}
		return Result.lexically_normal();
	}

	FString ReadTextFile(const FString& Path)
	{
		std::ifstream File(ResolveProjectPath(Path), std::ios::binary);
		if (!File.is_open())
		{
			return FString();
		}

		std::ostringstream Buffer;
		Buffer << File.rdbuf();
		return Buffer.str();
	}

	bool WriteTextFile(const FString& Path, const FString& Text)
	{
		std::ofstream File(ResolveProjectPath(Path), std::ios::binary | std::ios::trunc);
		if (!File.is_open())
		{
			return false;
		}
		File.write(Text.data(), static_cast<std::streamsize>(Text.size()));
		return File.good();
	}

	bool HasSuffix(const FString& Value, const char* Suffix)
	{
		const size_t SuffixLen = std::strlen(Suffix);
		return Value.size() >= SuffixLen && Value.compare(Value.size() - SuffixLen, SuffixLen, Suffix) == 0;
	}

	bool ParseCssNumber(const FString& Value, float& OutValue, FString* OutUnit = nullptr)
	{
		const FString Trimmed = TrimCopy(Value);
		if (Trimmed.empty())
		{
			return false;
		}

		char* EndPtr = nullptr;
		const float Parsed = std::strtof(Trimmed.c_str(), &EndPtr);
		if (EndPtr == Trimmed.c_str())
		{
			return false;
		}

		OutValue = Parsed;
		if (OutUnit)
		{
			*OutUnit = TrimCopy(FString(EndPtr));
		}
		return true;
	}

	float ParseCssDimension(const FString& Value, float ParentSize, float DefaultValue = 0.0f)
	{
		float Number = 0.0f;
		FString Unit;
		if (!ParseCssNumber(Value, Number, &Unit))
		{
			return DefaultValue;
		}

		if (Unit == "%")
		{
			return ParentSize * Number / 100.0f;
		}

		return Number;
	}

	FString FormatCssNumber(float Value, const FString& Unit)
	{
		char Buffer[64] = {};
		const char* EffectiveUnit = Unit.empty() ? "px" : Unit.c_str();
		std::snprintf(Buffer, sizeof(Buffer), "%.0f%s", Value, EffectiveUnit);
		return Buffer;
	}

	FString ExtractAttributeValue(const FString& TagText, const char* AttributeName)
	{
		const FString LowerTag = ToLowerCopy(TagText);
		const FString LowerName = ToLowerCopy(AttributeName);

		size_t Pos = 0;
		while ((Pos = LowerTag.find(LowerName, Pos)) != FString::npos)
		{
			const bool bBoundaryBefore = Pos == 0 ||
				(!std::isalnum(static_cast<unsigned char>(LowerTag[Pos - 1])) && LowerTag[Pos - 1] != '-' && LowerTag[Pos - 1] != '_');
			const size_t AfterName = Pos + LowerName.size();
			const bool bBoundaryAfter = AfterName >= LowerTag.size() ||
				(!std::isalnum(static_cast<unsigned char>(LowerTag[AfterName])) && LowerTag[AfterName] != '-' && LowerTag[AfterName] != '_');
			if (!bBoundaryBefore || !bBoundaryAfter)
			{
				Pos = AfterName;
				continue;
			}

			const size_t EqualPos = TagText.find('=', AfterName);
			if (EqualPos == FString::npos)
			{
				return FString();
			}

			const size_t QuoteStart = TagText.find_first_of("\"'", EqualPos + 1);
			if (QuoteStart == FString::npos)
			{
				return FString();
			}

			const char Quote = TagText[QuoteStart];
			const size_t QuoteEnd = TagText.find(Quote, QuoteStart + 1);
			if (QuoteEnd == FString::npos)
			{
				return FString();
			}

			return TagText.substr(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
		}

		return FString();
	}

	bool IsIdentCharacter(char Ch)
	{
		return std::isalnum(static_cast<unsigned char>(Ch)) || Ch == '-' || Ch == '_';
	}

	ImU32 ColorForElementId(const FString& Id, bool bSelected)
	{
		uint32 Hash = 2166136261u;
		for (char Ch : Id)
		{
			Hash ^= static_cast<uint8>(Ch);
			Hash *= 16777619u;
		}

		const int R = 80 + static_cast<int>(Hash & 0x7f);
		const int G = 80 + static_cast<int>((Hash >> 8) & 0x7f);
		const int B = 80 + static_cast<int>((Hash >> 16) & 0x7f);
		const int A = bSelected ? 90 : 42;
		return IM_COL32(R, G, B, A);
	}
}

bool FRmlWidgetEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<URmlWidgetAsset>();
}

void FRmlWidgetEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	FAssetEditorWidget::Open(Object);
	SelectedElementId.clear();
	DraggingElementId.clear();
	bDragging = false;
	Reload();
}

FString FRmlWidgetEditorWidget::GetDocumentTitle() const
{
	if (const URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject))
	{
		std::filesystem::path Path(FPaths::ToWide(Asset->GetSourcePath()));
		FString Title = FPaths::ToUtf8(Path.filename().wstring());
		if (IsDirty())
		{
			Title += " *";
		}
		return Title;
	}
	return "Rml Widget";
}

FString FRmlWidgetEditorWidget::GetDocumentPayloadId() const
{
	if (const URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject))
	{
		return Asset->GetSourcePath();
	}
	return FAssetEditorWidget::GetDocumentPayloadId();
}

EEditorDocumentTabKind FRmlWidgetEditorWidget::GetDocumentTabKind() const
{
	return EEditorDocumentTabKind::RmlWidgetEditor;
}

void FRmlWidgetEditorWidget::Reload()
{
	URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject);
	if (!Asset)
	{
		return;
	}

	RawRmlText = Asset->GetDocumentPath().empty() ? FString() : ReadTextFile(Asset->GetDocumentPath());
	RawStyleText = ReadTextFile(Asset->GetStyleSheetPath());
	ParseRml();
	ParseStyleSheet();
	RebuildElementTree();
	SetAllElementVisibility(true);
	ComputedRectCache.clear();
	ClearDirty();

	StatusMessage = "Loaded ";
	StatusMessage += Asset->GetStyleSheetPath();
}

bool FRmlWidgetEditorWidget::SaveStyleSheet()
{
	URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject);
	if (!Asset)
	{
		return false;
	}

	if (!WriteTextFile(Asset->GetStyleSheetPath(), RawStyleText))
	{
		StatusMessage = "Save failed: ";
		StatusMessage += Asset->GetStyleSheetPath();
		return false;
	}

	ClearDirty();
	StatusMessage = "Saved ";
	StatusMessage += Asset->GetStyleSheetPath();
	if (LivePreview)
	{
		FString LiveStatus;
		if (LivePreview->ReloadDocument(&LiveStatus))
		{
			StatusMessage = LiveStatus;
		}
	}
	return true;
}

void FRmlWidgetEditorWidget::ParseRml()
{
	Elements.clear();
	ElementIndexById.clear();

	TArray<FString> ParentStack;
	size_t Pos = 0;
	while ((Pos = RawRmlText.find('<', Pos)) != FString::npos)
	{
		if (Pos + 1 >= RawRmlText.size())
		{
			break;
		}

		const bool bClosing = RawRmlText[Pos + 1] == '/';
		const bool bSpecial = RawRmlText[Pos + 1] == '!' || RawRmlText[Pos + 1] == '?';
		const size_t End = RawRmlText.find('>', Pos + 1);
		if (End == FString::npos)
		{
			break;
		}

		if (bClosing)
		{
			if (!ParentStack.empty())
			{
				ParentStack.pop_back();
			}
			Pos = End + 1;
			continue;
		}

		if (bSpecial)
		{
			Pos = End + 1;
			continue;
		}

		const FString TagText = RawRmlText.substr(Pos, End - Pos + 1);
		const FString Id = ExtractAttributeValue(TagText, "id");
		const FString CurrentParent = ParentStack.empty() ? FString() : ParentStack.back();
		FString StackValue = CurrentParent;

		if (!Id.empty())
		{
			FElementNode Node;
			Node.Id = Id;
			Node.ParentId = CurrentParent;
			ElementIndexById[Id] = static_cast<int32>(Elements.size());
			Elements.push_back(std::move(Node));
			StackValue = Id;
		}

		const FString TrimmedTag = TrimCopy(TagText);
		const bool bSelfClosing = TrimmedTag.size() >= 2 && TrimmedTag[TrimmedTag.size() - 2] == '/';
		if (!bSelfClosing)
		{
			ParentStack.push_back(StackValue);
		}

		Pos = End + 1;
	}
}

void FRmlWidgetEditorWidget::ParseStyleSheet()
{
	StyleRules.clear();

	size_t Pos = 0;
	while ((Pos = RawStyleText.find('#', Pos)) != FString::npos)
	{
		const size_t IdStart = Pos + 1;
		size_t IdEnd = IdStart;
		while (IdEnd < RawStyleText.size() && IsIdentCharacter(RawStyleText[IdEnd]))
		{
			++IdEnd;
		}

		if (IdEnd == IdStart)
		{
			Pos = IdStart;
			continue;
		}

		const size_t BraceOpen = RawStyleText.find('{', IdEnd);
		if (BraceOpen == FString::npos)
		{
			break;
		}

		const FString Between = RawStyleText.substr(IdEnd, BraceOpen - IdEnd);
		if (Between.find(';') != FString::npos || Between.find('}') != FString::npos)
		{
			Pos = IdEnd;
			continue;
		}

		if (Between.find(',') != FString::npos)
		{
			Pos = BraceOpen + 1;
			continue;
		}

		const size_t BraceClose = RawStyleText.find('}', BraceOpen + 1);
		if (BraceClose == FString::npos)
		{
			break;
		}

		FStyleRule Rule;
		Rule.Id = RawStyleText.substr(IdStart, IdEnd - IdStart);
		Rule.BlockStart = BraceOpen;
		Rule.BlockEnd = BraceClose;

		size_t PropPos = BraceOpen + 1;
		while (PropPos < BraceClose)
		{
			while (PropPos < BraceClose && std::isspace(static_cast<unsigned char>(RawStyleText[PropPos])))
			{
				++PropPos;
			}

			if (PropPos + 1 < BraceClose && RawStyleText[PropPos] == '/' && RawStyleText[PropPos + 1] == '*')
			{
				const size_t CommentEnd = RawStyleText.find("*/", PropPos + 2);
				if (CommentEnd == FString::npos || CommentEnd >= BraceClose)
				{
					break;
				}
				PropPos = CommentEnd + 2;
				continue;
			}

			const size_t Colon = RawStyleText.find(':', PropPos);
			if (Colon == FString::npos || Colon >= BraceClose)
			{
				break;
			}

			const size_t Semi = RawStyleText.find(';', Colon + 1);
			if (Semi == FString::npos || Semi > BraceClose)
			{
				PropPos = Colon + 1;
				continue;
			}

			FString Name = TrimCopy(RawStyleText.substr(PropPos, Colon - PropPos));
			if (!Name.empty() && Name.find('\n') == FString::npos && Name.find('\r') == FString::npos)
			{
				size_t ValueStart = Colon + 1;
				while (ValueStart < Semi && std::isspace(static_cast<unsigned char>(RawStyleText[ValueStart])))
				{
					++ValueStart;
				}

				size_t ValueEnd = Semi;
				while (ValueEnd > ValueStart && std::isspace(static_cast<unsigned char>(RawStyleText[ValueEnd - 1])))
				{
					--ValueEnd;
				}

				FStyleProperty Property;
				Property.Name = std::move(Name);
				Property.Value = RawStyleText.substr(ValueStart, ValueEnd - ValueStart);
				Property.ValueStart = ValueStart;
				Property.ValueEnd = ValueEnd;
				Rule.Properties.push_back(std::move(Property));
			}

			PropPos = Semi + 1;
		}

		StyleRules.push_back(std::move(Rule));
		Pos = BraceClose + 1;
	}
}

void FRmlWidgetEditorWidget::RebuildElementTree()
{
	for (FElementNode& Element : Elements)
	{
		Element.Children.clear();
	}

	for (FElementNode& Element : Elements)
	{
		if (Element.ParentId.empty())
		{
			continue;
		}

		auto ParentIt = ElementIndexById.find(Element.ParentId);
		if (ParentIt != ElementIndexById.end())
		{
			Elements[ParentIt->second].Children.push_back(Element.Id);
		}
	}
}

const FRmlWidgetEditorWidget::FStyleRule* FRmlWidgetEditorWidget::FindStyleRule(const FString& ElementId) const
{
	for (auto It = StyleRules.rbegin(); It != StyleRules.rend(); ++It)
	{
		if (It->Id == ElementId)
		{
			return &(*It);
		}
	}
	return nullptr;
}

FRmlWidgetEditorWidget::FStyleRule* FRmlWidgetEditorWidget::FindStyleRule(const FString& ElementId)
{
	for (auto It = StyleRules.rbegin(); It != StyleRules.rend(); ++It)
	{
		if (It->Id == ElementId)
		{
			return &(*It);
		}
	}
	return nullptr;
}

const FRmlWidgetEditorWidget::FStyleProperty* FRmlWidgetEditorWidget::FindProperty(const FStyleRule& Rule, const FString& PropertyName) const
{
	for (const FStyleProperty& Property : Rule.Properties)
	{
		if (Property.Name == PropertyName)
		{
			return &Property;
		}
	}
	return nullptr;
}

const FRmlWidgetEditorWidget::FStyleProperty* FRmlWidgetEditorWidget::FindCascadedProperty(const FString& ElementId, const FString& PropertyName) const
{
	const FStyleProperty* Result = nullptr;
	for (const FStyleRule& Rule : StyleRules)
	{
		if (Rule.Id != ElementId)
		{
			continue;
		}

		if (const FStyleProperty* Property = FindProperty(Rule, PropertyName))
		{
			Result = Property;
		}
	}
	return Result;
}

bool FRmlWidgetEditorWidget::SetStyleProperty(const FString& ElementId, const FString& PropertyName, const FString& Value)
{
	if (ElementId.empty() || PropertyName.empty())
	{
		return false;
	}

	if (FStyleRule* Rule = FindStyleRule(ElementId))
	{
		for (const FStyleProperty& Property : Rule->Properties)
		{
			if (Property.Name == PropertyName)
			{
				RawStyleText.replace(Property.ValueStart, Property.ValueEnd - Property.ValueStart, Value);
				ParseStyleSheet();
				ComputedRectCache.clear();
				MarkDirty();
				return true;
			}
		}

		FString Insertion = "\n    ";
		Insertion += PropertyName;
		Insertion += ": ";
		Insertion += Value;
		Insertion += ";";
		RawStyleText.insert(Rule->BlockEnd, Insertion);
		ParseStyleSheet();
		ComputedRectCache.clear();
		MarkDirty();
		return true;
	}

	FString NewRule = "\n\n#";
	NewRule += ElementId;
	NewRule += " {\n    ";
	NewRule += PropertyName;
	NewRule += ": ";
	NewRule += Value;
	NewRule += ";\n}\n";
	RawStyleText += NewRule;
	ParseStyleSheet();
	ComputedRectCache.clear();
	MarkDirty();
	return true;
}

FRmlWidgetEditorWidget::FComputedRect FRmlWidgetEditorWidget::ComputeElementRect(const FString& ElementId)
{
	TSet<FString> Visiting;
	return ComputeElementRectRecursive(ElementId, Visiting);
}

FRmlWidgetEditorWidget::FComputedRect FRmlWidgetEditorWidget::ComputeElementRectRecursive(const FString& ElementId, TSet<FString>& Visiting)
{
	if (auto It = ComputedRectCache.find(ElementId); It != ComputedRectCache.end())
	{
		return It->second;
	}

	FComputedRect Result;
	if (Visiting.find(ElementId) != Visiting.end())
	{
		return Result;
	}
	Visiting.insert(ElementId);

	auto ElementIt = ElementIndexById.find(ElementId);
	if (ElementIt == ElementIndexById.end())
	{
		return Result;
	}

	const FElementNode& Element = Elements[ElementIt->second];
	const bool bIsRoot = Element.ParentId.empty() || ElementId == "hud-screen";
	FComputedRect ParentRect;
	ParentRect.X = 0.0f;
	ParentRect.Y = 0.0f;
	ParentRect.Width = PreviewWidth;
	ParentRect.Height = PreviewHeight;
	ParentRect.bValid = true;

	if (!bIsRoot)
	{
		ParentRect = ComputeElementRectRecursive(Element.ParentId, Visiting);
		if (!ParentRect.bValid)
		{
			ParentRect.X = 0.0f;
			ParentRect.Y = 0.0f;
			ParentRect.Width = PreviewWidth;
			ParentRect.Height = PreviewHeight;
			ParentRect.bValid = true;
		}
	}

	auto GetValue = [this, &ElementId](const char* Name) -> const FString*
	{
		if (const FStyleProperty* Property = FindCascadedProperty(ElementId, Name))
		{
			return &Property->Value;
		}
		return nullptr;
	};

	if (ElementId == "hud-screen")
	{
		Result = ParentRect;
		Result.bValid = true;
		ComputedRectCache[ElementId] = Result;
		return Result;
	}

	float Width = DefaultElementWidth;
	float Height = DefaultElementHeight;
	if (const FString* WidthValue = GetValue("width"))
	{
		Width = ParseCssDimension(*WidthValue, ParentRect.Width, Width);
	}
	if (const FString* HeightValue = GetValue("height"))
	{
		Height = ParseCssDimension(*HeightValue, ParentRect.Height, Height);
	}

	float LocalX = 0.0f;
	float LocalY = 0.0f;
	if (const FString* LeftValue = GetValue("left"))
	{
		LocalX = ParseCssDimension(*LeftValue, ParentRect.Width, 0.0f);
	}
	else if (const FString* RightValue = GetValue("right"))
	{
		LocalX = ParentRect.Width - ParseCssDimension(*RightValue, ParentRect.Width, 0.0f) - Width;
	}

	if (const FString* TopValue = GetValue("top"))
	{
		LocalY = ParseCssDimension(*TopValue, ParentRect.Height, 0.0f);
	}
	else if (const FString* BottomValue = GetValue("bottom"))
	{
		LocalY = ParentRect.Height - ParseCssDimension(*BottomValue, ParentRect.Height, 0.0f) - Height;
	}

	if (const FString* MarginLeft = GetValue("margin-left"))
	{
		LocalX += ParseCssDimension(*MarginLeft, ParentRect.Width, 0.0f);
	}

	Result.X = ParentRect.X + LocalX;
	Result.Y = ParentRect.Y + LocalY;
	Result.Width = (std::max)(1.0f, Width);
	Result.Height = (std::max)(1.0f, Height);
	Result.bValid = true;

	ComputedRectCache[ElementId] = Result;
	return Result;
}

void FRmlWidgetEditorWidget::DrawElementRecursive(const FString& ElementId, const ImVec2& CanvasMin, float Zoom, ImDrawList* DrawList, bool bFilled)
{
	const FComputedRect Rect = ComputeElementRect(ElementId);
	if (!Rect.bValid)
	{
		return;
	}

	auto ElementIt = ElementIndexById.find(ElementId);
	if (ElementIt == ElementIndexById.end())
	{
		return;
	}

	const bool bIsRoot = ElementId == "hud-screen";
	const bool bSelected = ElementId == SelectedElementId;
	if (!bIsRoot && IsElementVisible(ElementId))
	{
		const ImVec2 Min(CanvasMin.x + Rect.X * Zoom, CanvasMin.y + Rect.Y * Zoom);
		const ImVec2 Max(CanvasMin.x + (Rect.X + Rect.Width) * Zoom, CanvasMin.y + (Rect.Y + Rect.Height) * Zoom);
		const ImU32 FillColor = ColorForElementId(ElementId, bSelected);
		const ImU32 BorderColor = bSelected ? IM_COL32(255, 210, 70, 255) : IM_COL32(140, 150, 165, 170);
		if (bFilled)
		{
			DrawList->AddRectFilled(Min, Max, FillColor, 2.0f);
		}
		DrawList->AddRect(Min, Max, BorderColor, 2.0f, 0, bSelected ? 2.5f : 1.0f);
		DrawList->AddText(ImVec2(Min.x + 4.0f, Min.y + 3.0f), bFilled ? IM_COL32(235, 238, 245, 230) : IM_COL32(255, 232, 120, 245), ElementId.c_str());
	}

	const FElementNode& Element = Elements[ElementIt->second];
	for (const FString& ChildId : Element.Children)
	{
		DrawElementRecursive(ChildId, CanvasMin, Zoom, DrawList, bFilled);
	}
}

FString FRmlWidgetEditorWidget::PickElementAt(const ImVec2& MouseCanvasPosition)
{
	FString Picked;
	for (const FElementNode& Element : Elements)
	{
		if (Element.Id == "hud-screen")
		{
			continue;
		}

		if (!IsElementVisible(Element.Id))
		{
			continue;
		}

		const FComputedRect Rect = ComputeElementRect(Element.Id);
		if (!Rect.bValid)
		{
			continue;
		}

		if (MouseCanvasPosition.x >= Rect.X && MouseCanvasPosition.x <= Rect.X + Rect.Width &&
			MouseCanvasPosition.y >= Rect.Y && MouseCanvasPosition.y <= Rect.Y + Rect.Height)
		{
			Picked = Element.Id;
		}
	}

	return Picked;
}

void FRmlWidgetEditorWidget::ApplyDragDelta(const FString& ElementId, float DeltaX, float DeltaY)
{
	if (ElementId.empty() || ElementId == "hud-screen")
	{
		return;
	}

	auto ElementIt = ElementIndexById.find(ElementId);
	if (ElementIt == ElementIndexById.end())
	{
		return;
	}

	const FElementNode& Element = Elements[ElementIt->second];
	FComputedRect ParentRect;
	ParentRect.Width = PreviewWidth;
	ParentRect.Height = PreviewHeight;
	ParentRect.bValid = true;
	if (!Element.ParentId.empty())
	{
		ParentRect = ComputeElementRect(Element.ParentId);
	}

	const FComputedRect Rect = ComputeElementRect(ElementId);
	const FStyleProperty* Left = FindCascadedProperty(ElementId, "left");
	const FStyleProperty* Right = FindCascadedProperty(ElementId, "right");
	const FStyleProperty* Top = FindCascadedProperty(ElementId, "top");
	const FStyleProperty* Bottom = FindCascadedProperty(ElementId, "bottom");
	const FStyleProperty* MarginLeft = FindCascadedProperty(ElementId, "margin-left");

	const float LocalX = Rect.X - ParentRect.X;
	const float LocalY = Rect.Y - ParentRect.Y;
	const float MarginLeftPixels = MarginLeft ? ParseCssDimension(MarginLeft->Value, ParentRect.Width, 0.0f) : 0.0f;

	if (Right && !Left)
	{
		float RightValue = ParentRect.Width - (LocalX + DeltaX) - Rect.Width;
		SetStyleProperty(ElementId, "right", FormatCssNumber((std::max)(0.0f, RightValue), "px"));
	}
	else if (Left && HasSuffix(TrimCopy(Left->Value), "%"))
	{
		const float Percent = ParentRect.Width > 0.0f ? ((LocalX + DeltaX - MarginLeftPixels) / ParentRect.Width) * 100.0f : 0.0f;
		char Buffer[64] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.2f%%", Percent);
		SetStyleProperty(ElementId, "left", Buffer);
	}
	else
	{
		SetStyleProperty(ElementId, "left", FormatCssNumber(LocalX + DeltaX - MarginLeftPixels, "px"));
	}

	if (Bottom && !Top)
	{
		float BottomValue = ParentRect.Height - (LocalY + DeltaY) - Rect.Height;
		SetStyleProperty(ElementId, "bottom", FormatCssNumber((std::max)(0.0f, BottomValue), "px"));
	}
	else
	{
		SetStyleProperty(ElementId, "top", FormatCssNumber(LocalY + DeltaY, "px"));
	}
}

void FRmlWidgetEditorWidget::RenderToolbar(URmlWidgetAsset* Asset)
{
	if (ImGui::Button("Save"))
	{
		SaveStyleSheet();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		Reload();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("Zoom", &PreviewZoom, 0.01f, 0.25f, 2.0f, "%.2f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("W", &PreviewWidth, 1.0f, 320.0f, 7680.0f, "%.0f");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragFloat("H", &PreviewHeight, 1.0f, 180.0f, 4320.0f, "%.0f");

	ImGui::SameLine();
	if (ImGui::RadioButton("Wireframe", !bLivePreviewMode))
	{
		bLivePreviewMode = false;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Live", bLivePreviewMode))
	{
		bLivePreviewMode = true;
	}
	if (bLivePreviewMode)
	{
		ImGui::SameLine();
		ImGui::Checkbox("Overlay", &bShowLiveOverlay);
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s%s", Asset->GetStyleSheetPath().c_str(), IsDirty() ? " *" : "");
	if (!StatusMessage.empty())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("| %s", StatusMessage.c_str());
	}
}

void FRmlWidgetEditorWidget::RenderElementList()
{
	ImGui::TextUnformatted("Elements");
	ImGui::SameLine();
	if (ImGui::SmallButton("All"))
	{
		SetAllElementVisibility(true);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("None"))
	{
		SetAllElementVisibility(false);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Selected Only") && !SelectedElementId.empty())
	{
		SetAllElementVisibility(false);
		if (SelectedElementId != "hud-screen")
		{
			VisibleElementIds.insert(SelectedElementId);
		}
	}

	ImGui::Separator();
	for (const FElementNode& Element : Elements)
	{
		ImGui::PushID(Element.Id.c_str());

		if (Element.Id == "hud-screen")
		{
			ImGui::BeginDisabled();
			bool bRootVisible = true;
			ImGui::Checkbox("##visible", &bRootVisible);
			ImGui::EndDisabled();
		}
		else
		{
			bool bVisible = IsElementVisible(Element.Id);
			if (ImGui::Checkbox("##visible", &bVisible))
			{
				if (bVisible)
				{
					VisibleElementIds.insert(Element.Id);
				}
				else
				{
					VisibleElementIds.erase(Element.Id);
				}
			}
		}

		ImGui::SameLine();
		const bool bSelected = Element.Id == SelectedElementId;
		if (ImGui::Selectable(Element.Id.c_str(), bSelected))
		{
			SelectedElementId = Element.Id;
		}

		ImGui::PopID();
	}
}

bool FRmlWidgetEditorWidget::IsElementVisible(const FString& ElementId) const
{
	return ElementId == "hud-screen" || VisibleElementIds.find(ElementId) != VisibleElementIds.end();
}

void FRmlWidgetEditorWidget::SetAllElementVisibility(bool bVisible)
{
	VisibleElementIds.clear();
	if (!bVisible)
	{
		return;
	}

	for (const FElementNode& Element : Elements)
	{
		if (Element.Id != "hud-screen")
		{
			VisibleElementIds.insert(Element.Id);
		}
	}
}

void FRmlWidgetEditorWidget::RenderPreviewCanvas()
{
	if (bLivePreviewMode)
	{
		RenderLivePreviewCanvas();
	}
	else
	{
		RenderWireframePreviewCanvas();
	}
}

void FRmlWidgetEditorWidget::RenderWireframePreviewCanvas()
{
	ComputedRectCache.clear();

	const ImVec2 CanvasSize(PreviewWidth * PreviewZoom, PreviewHeight * PreviewZoom);
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
	ImGui::InvisibleButton("##RmlPreviewCanvas", CanvasSize);
	const bool bHovered = ImGui::IsItemHovered();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(42, 43, 45, 255));
	DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(190, 150, 36, 255), 0.0f, 0, 2.0f);

	for (int Grid = 1; Grid < 8; ++Grid)
	{
		const float X = CanvasMin.x + CanvasSize.x * static_cast<float>(Grid) / 8.0f;
		const float Y = CanvasMin.y + CanvasSize.y * static_cast<float>(Grid) / 8.0f;
		DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(70, 72, 76, 120));
		DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(70, 72, 76, 120));
	}

	if (ElementIndexById.find("hud-screen") != ElementIndexById.end())
	{
		DrawElementRecursive("hud-screen", CanvasMin, PreviewZoom, DrawList, true);
	}
	else
	{
		for (const FElementNode& Element : Elements)
		{
			if (Element.ParentId.empty())
			{
				DrawElementRecursive(Element.Id, CanvasMin, PreviewZoom, DrawList, true);
			}
		}
	}

	HandlePreviewInput(CanvasMin, PreviewZoom, bHovered);
}

void FRmlWidgetEditorWidget::HandlePreviewInput(const ImVec2& CanvasMin, float Zoom, bool bHovered)
{
	const ImGuiIO& IO = ImGui::GetIO();
	const ImVec2 MouseCanvasPosition(
		(IO.MousePos.x - CanvasMin.x) / Zoom,
		(IO.MousePos.y - CanvasMin.y) / Zoom);

	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const FString Picked = PickElementAt(MouseCanvasPosition);
		if (!Picked.empty())
		{
			SelectedElementId = Picked;
			DraggingElementId = Picked;
			bDragging = true;
		}
	}

	if (bDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		ApplyDragDelta(DraggingElementId, IO.MouseDelta.x / Zoom, IO.MouseDelta.y / Zoom);
	}

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		bDragging = false;
		DraggingElementId.clear();
	}

	if (bHovered)
	{
		const FString HoveredElement = PickElementAt(MouseCanvasPosition);
		if (!HoveredElement.empty())
		{
			ImGui::SetTooltip("%s", HoveredElement.c_str());
		}
	}
}

void FRmlWidgetEditorWidget::RenderLivePreviewCanvas()
{
	ComputedRectCache.clear();

	const ImVec2 CanvasSize(PreviewWidth * PreviewZoom, PreviewHeight * PreviewZoom);
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);

	URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject);
	ID3D11ShaderResourceView* PreviewSRV = nullptr;
	FString LiveStatus;

	if (Asset && EditorEngine)
	{
		FRenderer& Renderer = EditorEngine->GetRenderer();
		if (!LivePreview)
		{
			LivePreview = std::make_unique<FRmlLivePreview>();
		}

		if (!LivePreview->IsInitialized())
		{
			LivePreview->Initialize(Renderer.GetFD3DDevice().GetDevice());
		}

		const FString& DocumentPath = Asset->GetDocumentPath();
		if (LivePreview->IsInitialized())
		{
			if (!LivePreview->IsDocumentLoaded() || LivePreview->GetLoadedDocumentPath() != DocumentPath)
			{
				LivePreview->LoadDocument(DocumentPath, &LiveStatus);
			}

			if (LivePreview->IsDocumentLoaded())
			{
				PreviewSRV = LivePreview->Render(
					Renderer,
					static_cast<int32>(PreviewWidth),
					static_cast<int32>(PreviewHeight),
					&LiveStatus);
			}
		}
	}

	if (PreviewSRV)
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(PreviewSRV), CanvasSize);
	}
	else
	{
		ImGui::InvisibleButton("##RmlLivePreviewMissing", CanvasSize);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(24, 24, 28, 255));
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(190, 150, 36, 255), 0.0f, 0, 2.0f);
		const FString Message = LiveStatus.empty() ? "Live preview is not available." : LiveStatus;
		DrawList->AddText(ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 12.0f), IM_COL32(235, 238, 245, 230), Message.c_str());
	}

	const bool bHovered = ImGui::IsItemHovered();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(190, 150, 36, 255), 0.0f, 0, 2.0f);

	if (bShowLiveOverlay)
	{
		if (ElementIndexById.find("hud-screen") != ElementIndexById.end())
		{
			DrawElementRecursive("hud-screen", CanvasMin, PreviewZoom, DrawList, false);
		}
		else
		{
			for (const FElementNode& Element : Elements)
			{
				if (Element.ParentId.empty())
				{
					DrawElementRecursive(Element.Id, CanvasMin, PreviewZoom, DrawList, false);
				}
			}
		}

		HandlePreviewInput(CanvasMin, PreviewZoom, bHovered);
	}

	if (IsDirty())
	{
		DrawList->AddText(
			ImVec2(CanvasMin.x + 12.0f, CanvasMax.y - ImGui::GetFontSize() - 10.0f),
			IM_COL32(255, 218, 110, 245),
			"Live preview shows the last saved RCSS. Press Save to refresh.");
	}
}

void FRmlWidgetEditorWidget::RenderStylePropertyFloat(const char* Label, const char* PropertyName)
{
	if (SelectedElementId.empty())
	{
		return;
	}

	const FStyleProperty* Property = FindCascadedProperty(SelectedElementId, PropertyName);
	if (!Property)
	{
		FString ButtonLabel = "Add ";
		ButtonLabel += Label;
		if (ImGui::SmallButton(ButtonLabel.c_str()))
		{
			SetStyleProperty(SelectedElementId, PropertyName, "0px");
		}
		return;
	}

	float Value = 0.0f;
	FString Unit;
	if (!ParseCssNumber(Property->Value, Value, &Unit))
	{
		ImGui::Text("%s: %s", Label, Property->Value.c_str());
		return;
	}

	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::DragFloat(Label, &Value, 1.0f, -10000.0f, 10000.0f, "%.0f"))
	{
		SetStyleProperty(SelectedElementId, PropertyName, FormatCssNumber(Value, Unit));
	}
}

void FRmlWidgetEditorWidget::RenderStylePropertyText(const char* Label, const char* PropertyName)
{
	(void)Label;
	(void)PropertyName;
}

void FRmlWidgetEditorWidget::RenderDetailsPanel()
{
	ImGui::TextUnformatted("Details");
	ImGui::Separator();

	if (SelectedElementId.empty())
	{
		ImGui::TextDisabled("No element selected.");
		return;
	}

	ImGui::Text("ID: %s", SelectedElementId.c_str());
	if (auto ElementIt = ElementIndexById.find(SelectedElementId); ElementIt != ElementIndexById.end())
	{
		const FElementNode& Element = Elements[ElementIt->second];
		ImGui::TextDisabled("Parent: %s", Element.ParentId.empty() ? "(root)" : Element.ParentId.c_str());
	}

	if (SelectedElementId != "hud-screen")
	{
		bool bVisible = IsElementVisible(SelectedElementId);
		if (ImGui::Checkbox("Visible in preview", &bVisible))
		{
			if (bVisible)
			{
				VisibleElementIds.insert(SelectedElementId);
			}
			else
			{
				VisibleElementIds.erase(SelectedElementId);
			}
		}
	}

	const FComputedRect Rect = ComputeElementRect(SelectedElementId);
	if (Rect.bValid)
	{
		ImGui::TextDisabled("Rect: %.0f, %.0f  %.0f x %.0f", Rect.X, Rect.Y, Rect.Width, Rect.Height);
	}

	ImGui::Separator();
	const FStyleRule* Rule = FindStyleRule(SelectedElementId);
	const FStyleProperty* Position = FindCascadedProperty(SelectedElementId, "position");
	const char* PositionLabels[] = { "absolute", "relative", "static" };
	int PositionIndex = 0;
	if (Position)
	{
		const FString Lower = ToLowerCopy(TrimCopy(Position->Value));
		if (Lower == "relative") PositionIndex = 1;
		else if (Lower == "static") PositionIndex = 2;
	}
	if (ImGui::Combo("position", &PositionIndex, PositionLabels, IM_ARRAYSIZE(PositionLabels)))
	{
		SetStyleProperty(SelectedElementId, "position", PositionLabels[PositionIndex]);
	}

	RenderStylePropertyFloat("left", "left");
	RenderStylePropertyFloat("top", "top");
	RenderStylePropertyFloat("right", "right");
	RenderStylePropertyFloat("bottom", "bottom");
	RenderStylePropertyFloat("width", "width");
	RenderStylePropertyFloat("height", "height");
	RenderStylePropertyFloat("margin-left", "margin-left");

	ImGui::Separator();
	ImGui::TextUnformatted("Raw Style (last matching rule)");
	if (!Rule)
	{
		ImGui::TextDisabled("No #id rule. Editing a property will create one.");
		return;
	}

	if (ImGui::BeginTable("RmlRawStyleTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		for (const FStyleProperty& Property : Rule->Properties)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(Property.Name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(Property.Value.c_str());
		}
		ImGui::EndTable();
	}
}

void FRmlWidgetEditorWidget::Render(float DeltaTime)
{
	RenderDocument(DeltaTime);
}

void FRmlWidgetEditorWidget::RenderDocument(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	URmlWidgetAsset* Asset = Cast<URmlWidgetAsset>(EditedObject);
	if (!Asset)
	{
		Close();
		return;
	}

	if (ConsumeFocusRequest())
	{
		ImGui::SetKeyboardFocusHere();
	}

	RenderToolbar(Asset);
	ImGui::Separator();

	if (ImGui::BeginTable("RmlWidgetEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Elements", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 300.0f);

		ImGui::TableNextColumn();
		ImGui::BeginChild("##RmlElements", ImVec2(0.0f, 0.0f), false);
		RenderElementList();
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("##RmlPreview", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
		RenderPreviewCanvas();
		ImGui::EndChild();

		ImGui::TableNextColumn();
		ImGui::BeginChild("##RmlDetails", ImVec2(0.0f, 0.0f), false);
		RenderDetailsPanel();
		ImGui::EndChild();

		ImGui::EndTable();
	}
}
