#include "Component/UWindowPanelComponent.h"

#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Platform/Paths.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

IMPLEMENT_CLASS(UNineSlicePanelComponent, UUIImageComponent)
IMPLEMENT_CLASS(UWindowPanelComponent, UNineSlicePanelComponent)
HIDE_FROM_COMPONENT_LIST(UWindowPanelComponent)

namespace
{
	bool IsTexturePathNone(const FString& InTexturePath)
	{
		return !InTexturePath.empty() && _stricmp(InTexturePath.c_str(), "None") == 0;
	}

	void FitBordersToTarget(float TargetSize, float& StartSize, float& EndSize)
	{
		StartSize = (std::max)(0.0f, StartSize);
		EndSize = (std::max)(0.0f, EndSize);

		const float Total = StartSize + EndSize;
		if (Total > TargetSize && Total > 0.0f)
		{
			const float Scale = TargetSize / Total;
			StartSize *= Scale;
			EndSize *= Scale;
		}
	}

	float ReadJsonFloat(const json::JSON& Value, float Fallback = 0.0f)
	{
		bool bFloat = false;
		const double FloatValue = Value.ToFloat(bFloat);
		if (bFloat)
		{
			return static_cast<float>(FloatValue);
		}

		bool bInt = false;
		const long IntValue = Value.ToInt(bInt);
		if (bInt)
		{
			return static_cast<float>(IntValue);
		}

		return Fallback;
	}

	float ReadJsonObjectFloat(const json::JSON& Object, const char* Key, float Fallback = 0.0f)
	{
		if (!Key || !Object.hasKey(Key))
		{
			return Fallback;
		}

		return ReadJsonFloat(Object.at(Key), Fallback);
	}

	bool LoadJsonFile(const FString& JsonPath, json::JSON& OutJson)
	{
		std::ifstream File(std::filesystem::path(FPaths::ToWide(JsonPath)));
		if (!File.is_open())
		{
			return false;
		}

		const FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		OutJson = json::JSON::Load(Content);
		return true;
	}
}

UNineSlicePanelComponent::UNineSlicePanelComponent()
{
	Slice = FVector4(8.0f, 24.0f, 8.0f, 8.0f);
}

void UNineSlicePanelComponent::Serialize(FArchive& Ar)
{
	UUIImageComponent::Serialize(Ar);
	Ar << StyleJsonPath;
	Ar << AtlasRegion;
	Ar << Slice;
	Ar << bDrawBorder;
	Ar << bDrawCenter;
	Ar << TopLeftTextureSlot.Path;
	Ar << TopTextureSlot.Path;
	Ar << TopRightTextureSlot.Path;
	Ar << LeftTextureSlot.Path;
	Ar << CenterTextureSlot.Path;
	Ar << RightTextureSlot.Path;
	Ar << BottomLeftTextureSlot.Path;
	Ar << BottomTextureSlot.Path;
	Ar << BottomRightTextureSlot.Path;

	if (Ar.IsLoading())
	{
		if (!StyleJsonPath.empty())
		{
			LoadStyleFromJson();
		}
		ClampSlice();
	}
}

void UNineSlicePanelComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UUIImageComponent::GetEditableProperties(OutProps);

	OutProps.erase(
		std::remove_if(
			OutProps.begin(),
			OutProps.end(),
			[](const FPropertyDescriptor& Prop)
			{
				return Prop.Name == "Texture";
			}),
		OutProps.end());

	OutProps.push_back({ "Style Json", EPropertyType::String, &StyleJsonPath });
	OutProps.push_back({ "Draw Border", EPropertyType::Bool, &bDrawBorder });
	OutProps.push_back({ "Draw Center", EPropertyType::Bool, &bDrawCenter });
}

void UNineSlicePanelComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Style Json") == 0)
	{
		LoadStyleFromJson();
		ClampSlice();
		MarkRenderStateDirty();
	}

	if (strcmp(PropertyName, "Slice") == 0
		|| strcmp(PropertyName, "Nine Slice Border") == 0
		|| strcmp(PropertyName, "Atlas Region") == 0
		|| strcmp(PropertyName, "Texture") == 0)
	{
		ClampSlice();
	}

	if (strcmp(PropertyName, "Top Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopLeftTextureSlot, TopLeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Top Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopTextureSlot, TopTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Top Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopRightTextureSlot, TopRightTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(LeftTextureSlot, LeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Center Texture") == 0)
	{
		ResolveOptionalTextureSlot(CenterTextureSlot, CenterTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(RightTextureSlot, RightTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomLeftTextureSlot, BottomLeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomTextureSlot, BottomTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomRightTextureSlot, BottomRightTexture);
		MarkRenderStateDirty();
	}
}

bool UNineSlicePanelComponent::LoadStyleFromJson()
{
	if (StyleJsonPath.empty())
	{
		return true;
	}

	const std::filesystem::path NormalizedJsonPath = std::filesystem::path(FPaths::ToWide(StyleJsonPath)).lexically_normal();
	StyleJsonPath = FPaths::ToUtf8(NormalizedJsonPath.wstring());

	json::JSON Root;
	if (!LoadJsonFile(StyleJsonPath, Root))
	{
		return false;
	}

	if (Root.hasKey("image"))
	{
		const FString ImagePath = Root["image"].ToString();
		if (!ImagePath.empty())
		{
			const std::filesystem::path BaseDir = NormalizedJsonPath.parent_path();
			const std::filesystem::path ImageFsPath = std::filesystem::path(FPaths::ToWide(ImagePath));
			const std::filesystem::path ResolvedImagePath = ImageFsPath.is_absolute()
				? ImageFsPath.lexically_normal()
				: (BaseDir / ImageFsPath).lexically_normal();
			SetTexturePath(FPaths::ToUtf8(ResolvedImagePath.wstring()));
		}
	}

	if (Root.hasKey("region"))
	{
		const json::JSON Region = Root["region"];
		AtlasRegion.X = ReadJsonObjectFloat(Region, "x", 0.0f);
		AtlasRegion.Y = ReadJsonObjectFloat(Region, "y", 0.0f);
		AtlasRegion.Z = ReadJsonObjectFloat(Region, "w", 0.0f);
		AtlasRegion.W = ReadJsonObjectFloat(Region, "h", 0.0f);
	}
	else
	{
		AtlasRegion = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	if (Root.hasKey("border"))
	{
		const json::JSON Border = Root["border"];
		Slice.X = ReadJsonObjectFloat(Border, "left", Slice.X);
		Slice.Y = ReadJsonObjectFloat(Border, "top", Slice.Y);
		Slice.Z = ReadJsonObjectFloat(Border, "right", Slice.Z);
		Slice.W = ReadJsonObjectFloat(Border, "bottom", Slice.W);
	}

	return true;
}

void UNineSlicePanelComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	UTexture2D* Texture2D = GetTexture();
	if (!SRV || !Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return;
	}

	const float RegionX = (std::clamp)(AtlasRegion.X, 0.0f, TextureWidth);
	const float RegionY = (std::clamp)(AtlasRegion.Y, 0.0f, TextureHeight);
	const float RegionWidth = AtlasRegion.Z > 0.0f ? (std::clamp)(AtlasRegion.Z, 0.0f, TextureWidth - RegionX) : (TextureWidth - RegionX);
	const float RegionHeight = AtlasRegion.W > 0.0f ? (std::clamp)(AtlasRegion.W, 0.0f, TextureHeight - RegionY) : (TextureHeight - RegionY);
	if (RegionWidth <= 0.0f || RegionHeight <= 0.0f)
	{
		return;
	}

	float SliceLeft = (std::clamp)(Slice.X, 0.0f, RegionWidth);
	float SliceTop = (std::clamp)(Slice.Y, 0.0f, RegionHeight);
	float SliceRight = (std::clamp)(Slice.Z, 0.0f, RegionWidth - SliceLeft);
	float SliceBottom = (std::clamp)(Slice.W, 0.0f, RegionHeight - SliceTop);

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	float LeftWidth = SliceLeft;
	float RightWidth = SliceRight;
	float TopHeight = SliceTop;
	float BottomHeight = SliceBottom;

	FitBordersToTarget(ResolvedSize.X, LeftWidth, RightWidth);
	FitBordersToTarget(ResolvedSize.Y, TopHeight, BottomHeight);

	const float CenterWidth = (std::max)(0.0f, ResolvedSize.X - LeftWidth - RightWidth);
	const float CenterHeight = (std::max)(0.0f, ResolvedSize.Y - TopHeight - BottomHeight);

	const float X0 = ResolvedPosition.X;
	const float X1 = X0 + LeftWidth;
	const float X2 = X0 + ResolvedSize.X - RightWidth;
	const float Y0 = ResolvedPosition.Y;
	const float Y1 = Y0 + TopHeight;
	const float Y2 = Y0 + ResolvedSize.Y - BottomHeight;

	const float U0 = RegionX / TextureWidth;
	const float U1 = (RegionX + SliceLeft) / TextureWidth;
	const float U2 = (RegionX + RegionWidth - SliceRight) / TextureWidth;
	const float U3 = (RegionX + RegionWidth) / TextureWidth;
	const float V0 = RegionY / TextureHeight;
	const float V1 = (RegionY + SliceTop) / TextureHeight;
	const float V2 = (RegionY + RegionHeight - SliceBottom) / TextureHeight;
	const float V3 = (RegionY + RegionHeight) / TextureHeight;

	if (bDrawBorder)
	{
		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopLeftTextureSlot, TopLeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y0, LeftWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(TopLeftTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X0, Y0, LeftWidth, TopHeight, U0, V0, U1, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopTextureSlot, TopTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y0, CenterWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(TopTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X1, Y0, CenterWidth, TopHeight, U1, V0, U2, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopRightTextureSlot, TopRightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y0, RightWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(TopRightTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X2, Y0, RightWidth, TopHeight, U2, V0, U3, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(LeftTextureSlot, LeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y1, LeftWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(LeftTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X0, Y1, LeftWidth, CenterHeight, U0, V1, U1, V2);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(RightTextureSlot, RightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y1, RightWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(RightTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X2, Y1, RightWidth, CenterHeight, U2, V1, U3, V2);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomLeftTextureSlot, BottomLeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y2, LeftWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(BottomLeftTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X0, Y2, LeftWidth, BottomHeight, U0, V2, U1, V3);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomTextureSlot, BottomTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y2, CenterWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(BottomTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X1, Y2, CenterWidth, BottomHeight, U1, V2, U2, V3);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomRightTextureSlot, BottomRightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y2, RightWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(BottomRightTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X2, Y2, RightWidth, BottomHeight, U2, V2, U3, V3);
	}

	if (bDrawCenter)
	{
		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(CenterTextureSlot, CenterTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y1, CenterWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else if (IsTexturePathNone(CenterTextureSlot.Path))
		{
		}
		else
			AddPanelQuad(Scene, SRV, X1, Y1, CenterWidth, CenterHeight, U1, V1, U2, V2);
	}
}

void UNineSlicePanelComponent::ClampSlice()
{
	Slice.X = (std::max)(0.0f, Slice.X);
	Slice.Y = (std::max)(0.0f, Slice.Y);
	Slice.Z = (std::max)(0.0f, Slice.Z);
	Slice.W = (std::max)(0.0f, Slice.W);
	AtlasRegion.X = (std::max)(0.0f, AtlasRegion.X);
	AtlasRegion.Y = (std::max)(0.0f, AtlasRegion.Y);
	AtlasRegion.Z = (std::max)(0.0f, AtlasRegion.Z);
	AtlasRegion.W = (std::max)(0.0f, AtlasRegion.W);

	UTexture2D* Texture2D = GetTexture();
	if (!Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	AtlasRegion.X = (std::clamp)(AtlasRegion.X, 0.0f, TextureWidth);
	AtlasRegion.Y = (std::clamp)(AtlasRegion.Y, 0.0f, TextureHeight);

	const float RegionWidth = AtlasRegion.Z > 0.0f ? (std::clamp)(AtlasRegion.Z, 0.0f, TextureWidth - AtlasRegion.X) : (TextureWidth - AtlasRegion.X);
	const float RegionHeight = AtlasRegion.W > 0.0f ? (std::clamp)(AtlasRegion.W, 0.0f, TextureHeight - AtlasRegion.Y) : (TextureHeight - AtlasRegion.Y);
	AtlasRegion.Z = RegionWidth;
	AtlasRegion.W = RegionHeight;

	Slice.X = (std::clamp)(Slice.X, 0.0f, RegionWidth);
	Slice.Y = (std::clamp)(Slice.Y, 0.0f, RegionHeight);
	Slice.Z = (std::clamp)(Slice.Z, 0.0f, RegionWidth - Slice.X);
	Slice.W = (std::clamp)(Slice.W, 0.0f, RegionHeight - Slice.Y);
}

bool UNineSlicePanelComponent::ResolveOptionalTextureSlot(FTextureSlot& Slot, UTexture2D*& LoadedTexture)
{
	if (Slot.Path.empty())
	{
		LoadedTexture = nullptr;
		return true;
	}

	if (IsTexturePathNone(Slot.Path))
	{
		Slot.Path = "None";
		LoadedTexture = nullptr;
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	const FString ResolvedPath = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Slot.Path)).lexically_normal().wstring());
	if (UTexture2D* Texture2D = UTexture2D::LoadFromFile(ResolvedPath, Device))
	{
		Slot.Path = ResolvedPath;
		LoadedTexture = Texture2D;
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UNineSlicePanelComponent::GetOptionalTextureSRV(const FTextureSlot& Slot, UTexture2D* LoadedTexture) const
{
	if (Slot.Path.empty() || IsTexturePathNone(Slot.Path) || !LoadedTexture || !LoadedTexture->IsLoaded())
	{
		return nullptr;
	}

	return LoadedTexture->GetSRV();
}

void UNineSlicePanelComponent::AddPanelQuad(FScene& Scene, ID3D11ShaderResourceView* SRV, float X, float Y, float Width, float Height,
	float U0, float V0, float U1, float V1) const
{
	if (Width <= 0.0f || Height <= 0.0f)
	{
		return;
	}

	if (ShouldDrawShadow())
	{
		AddShadowScreenQuad(
			Scene,
			SRV,
			FVector2(X, Y),
			FVector2(Width, Height),
			ZOrder - 1,
			FVector2(U0, V0),
			FVector2(U1, V1));
	}

	Scene.AddScreenQuad(
		SRV,
		FVector2(X, Y),
		FVector2(Width, Height),
		Tint,
		ZOrder,
		FVector2(U0, V0),
		FVector2(U1, V1));
}
