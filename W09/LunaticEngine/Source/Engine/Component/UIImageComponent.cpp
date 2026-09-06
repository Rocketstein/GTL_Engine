#include "Component/UIImageComponent.h"

#include "Component/CanvasRootComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UUIImageComponent, UBillboardComponent)

namespace
{
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
	const char* GUIImageFitModeNames[] = { "Stretch", "Contain", "Cover" };
	const char* GUIImageContentAlignmentNames[] = { "Center", "Top", "Bottom", "Left", "Right" };
	const char* GUIImageBackgroundFitModeNames[] = { "Stretch", "Contain", "Cover" };
	const char* GUIImageBackgroundContentAlignmentNames[] = { "Center", "Top", "Bottom", "Left", "Right" };

	bool IsEmptyTexturePath(const FString& InTexturePath)
	{
		return InTexturePath.empty() || _stricmp(InTexturePath.c_str(), "None") == 0;
	}

	FVector2 ComputeAlignmentOffset(const FVector2& ContainerSize, const FVector2& ContentSize, EUIImageContentAlignment InAlignment)
	{
		const float DeltaX = (std::max)(0.0f, ContainerSize.X - ContentSize.X);
		const float DeltaY = (std::max)(0.0f, ContainerSize.Y - ContentSize.Y);

		switch (InAlignment)
		{
		case EUIImageContentAlignment::Top:
			return FVector2(DeltaX * 0.5f, 0.0f);
		case EUIImageContentAlignment::Bottom:
			return FVector2(DeltaX * 0.5f, DeltaY);
		case EUIImageContentAlignment::Left:
			return FVector2(0.0f, DeltaY * 0.5f);
		case EUIImageContentAlignment::Right:
			return FVector2(DeltaX, DeltaY * 0.5f);
		case EUIImageContentAlignment::Center:
		default:
			return FVector2(DeltaX * 0.5f, DeltaY * 0.5f);
		}
	}

	FVector2 GetViewportSize2D()
	{
		FVector2 ViewportSize(1920.0f, 1080.0f);
		if (GEngine)
		{
			if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
			{
				if (FViewport* GameViewport = GameViewportClient->GetViewport())
				{
					const float Width = static_cast<float>(GameViewport->GetWidth());
					const float Height = static_cast<float>(GameViewport->GetHeight());
					if (Width > 0.0f && Height > 0.0f)
					{
						ViewportSize.X = Width;
						ViewportSize.Y = Height;
						return ViewportSize;
					}
				}
			}

			if (GIsEditor)
			{
				if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
				{
					if (const FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
					{
						if (const FViewport* EditorViewport = ActiveViewport->GetViewport())
						{
							const float Width = static_cast<float>(EditorViewport->GetWidth());
							const float Height = static_cast<float>(EditorViewport->GetHeight());
							if (Width > 0.0f && Height > 0.0f)
							{
								ViewportSize.X = Width;
								ViewportSize.Y = Height;
								return ViewportSize;
							}
						}
					}
				}
			}

			if (FWindowsWindow* Window = GEngine->GetWindow())
			{
				ViewportSize.X = (std::max)(1.0f, Window->GetWidth());
				ViewportSize.Y = (std::max)(1.0f, Window->GetHeight());
			}
		}

		return ViewportSize;
	}

	bool ResolveParentUILayoutRect(const USceneComponent* StartParent, FVector2& OutPosition, FVector2& OutSize)
	{
		for (const USceneComponent* Current = StartParent; Current != nullptr; Current = Current->GetParent())
		{
			if (const UCanvasRootComponent* CanvasRoot = dynamic_cast<const UCanvasRootComponent*>(Current))
			{
				const FVector& CanvasSize = CanvasRoot->GetCanvasSize();
				OutPosition = CanvasRoot->GetCanvasOrigin();
				OutSize = FVector2((std::max)(1.0f, CanvasSize.X), (std::max)(1.0f, CanvasSize.Y));
				return true;
			}

			if (const UUIImageComponent* ImageParent = dynamic_cast<const UUIImageComponent*>(Current))
			{
				if (ImageParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}

			if (const UUIScreenTextComponent* TextParent = dynamic_cast<const UUIScreenTextComponent*>(Current))
			{
				if (TextParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}
		}

		OutPosition = FVector2(0.0f, 0.0f);
		OutSize = GetViewportSize2D();
		return true;
	}
}

EUIImageFitMode UUIImageComponent::SanitizeFitModeValue(int32 InFitMode)
{
	const int32 MinValue = static_cast<int32>(EUIImageFitMode::Stretch);
	const int32 MaxValue = static_cast<int32>(EUIImageFitMode::Cover);
	return static_cast<EUIImageFitMode>((std::clamp)(InFitMode, MinValue, MaxValue));
}

EUIImageContentAlignment UUIImageComponent::SanitizeContentAlignmentValue(int32 InAlignment)
{
	const int32 MinValue = static_cast<int32>(EUIImageContentAlignment::Center);
	const int32 MaxValue = static_cast<int32>(EUIImageContentAlignment::Right);
	return static_cast<EUIImageContentAlignment>((std::clamp)(InAlignment, MinValue, MaxValue));
}

UUIImageComponent::FResolvedImageDrawParams UUIImageComponent::ResolveImageDrawParams(
	const FVector2& ContainerPosition,
	const FVector2& ContainerSize,
	const UTexture2D* Texture,
	EUIImageFitMode InFitMode,
	EUIImageContentAlignment InAlignment)
{
	FResolvedImageDrawParams Result;
	Result.Position = ContainerPosition;
	Result.Size = ContainerSize;

	if (!Texture || Texture->GetWidth() == 0 || Texture->GetHeight() == 0)
	{
		return Result;
	}

	const float TextureWidth = static_cast<float>(Texture->GetWidth());
	const float TextureHeight = static_cast<float>(Texture->GetHeight());
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f || ContainerSize.X <= 0.0f || ContainerSize.Y <= 0.0f)
	{
		return Result;
	}

	const float WidthScale = ContainerSize.X / TextureWidth;
	const float HeightScale = ContainerSize.Y / TextureHeight;

	switch (InFitMode)
	{
	case EUIImageFitMode::Contain:
	{
		const float Scale = (std::min)(WidthScale, HeightScale);
		const FVector2 DrawSize(TextureWidth * Scale, TextureHeight * Scale);
		Result.Size = DrawSize;
		Result.Position = ContainerPosition + ComputeAlignmentOffset(ContainerSize, DrawSize, InAlignment);
		break;
	}
	case EUIImageFitMode::Cover:
	{
		const float VisibleWidthRatio = (std::min)(1.0f, WidthScale / HeightScale);
		const float VisibleHeightRatio = (std::min)(1.0f, HeightScale / WidthScale);
		const FVector2 VisibleUVSize(VisibleWidthRatio, VisibleHeightRatio);
		const FVector2 UVPadding(1.0f - VisibleUVSize.X, 1.0f - VisibleUVSize.Y);

		switch (InAlignment)
		{
		case EUIImageContentAlignment::Top:
			Result.UVMin = FVector2(UVPadding.X * 0.5f, 0.0f);
			break;
		case EUIImageContentAlignment::Bottom:
			Result.UVMin = FVector2(UVPadding.X * 0.5f, UVPadding.Y);
			break;
		case EUIImageContentAlignment::Left:
			Result.UVMin = FVector2(0.0f, UVPadding.Y * 0.5f);
			break;
		case EUIImageContentAlignment::Right:
			Result.UVMin = FVector2(UVPadding.X, UVPadding.Y * 0.5f);
			break;
		case EUIImageContentAlignment::Center:
		default:
			Result.UVMin = FVector2(UVPadding.X * 0.5f, UVPadding.Y * 0.5f);
			break;
		}

		Result.UVMax = Result.UVMin + VisibleUVSize;
		break;
	}
	case EUIImageFitMode::Stretch:
	default:
		break;
	}

	return Result;
}

UUIImageComponent::UUIImageComponent()
{
	bTickEnable = false;
	TextureSlot.Path.clear();
	SetTexture(nullptr);
	SetCanDeleteFromDetails(true);
}

void UUIImageComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);
	Ar << ScreenPosition;
	Ar << ScreenSize;
	Ar << bUseAnchoredLayout;
	Ar << Anchor;
	Ar << Alignment;
	Ar << AnchorOffset;
	Ar << Tint;
	Ar << BorderThickness;
	Ar << bBottomBorderOnly;
	Ar << BorderColor;
	Ar << ZOrder;
	Ar << FitMode;
	Ar << ContentAlignment;
	Ar << bDrawBackground;
	Ar << BackgroundTextureSlot.Path;
	Ar << BackgroundFitMode;
	Ar << BackgroundContentAlignment;
	Ar << BackgroundTint;
	Ar << bDrawShadow;
	Ar << ShadowOffset;
	Ar << ShadowTint;
	Ar << ShadowTopTint;
	Ar << ShadowBottomTint;
}

void UUIImageComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Screen Position", EPropertyType::Vec3, &ScreenPosition, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Screen Size", EPropertyType::Vec3, &ScreenSize, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Use Anchored Layout", EPropertyType::Bool, &bUseAnchoredLayout });
	OutProps.push_back({ "Anchor", EPropertyType::Vec3, &Anchor, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Alignment", EPropertyType::Vec3, &Alignment, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Anchor Offset", EPropertyType::Vec3, &AnchorOffset, -4096.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Fit Mode", EPropertyType::Enum, &FitMode, 0.0f, 0.0f, 0.1f, GUIImageFitModeNames, static_cast<uint32>(std::size(GUIImageFitModeNames)) });
	OutProps.push_back({ "Content Alignment", EPropertyType::Enum, &ContentAlignment, 0.0f, 0.0f, 0.1f, GUIImageContentAlignmentNames, static_cast<uint32>(std::size(GUIImageContentAlignmentNames)) });
	OutProps.push_back({ "Draw Background", EPropertyType::Bool, &bDrawBackground });
	OutProps.push_back({ "Background Texture", EPropertyType::TextureSlot, &BackgroundTextureSlot });
	OutProps.push_back({ "Background Fit Mode", EPropertyType::Enum, &BackgroundFitMode, 0.0f, 0.0f, 0.1f, GUIImageBackgroundFitModeNames, static_cast<uint32>(std::size(GUIImageBackgroundFitModeNames)) });
	OutProps.push_back({ "Background Content Alignment", EPropertyType::Enum, &BackgroundContentAlignment, 0.0f, 0.0f, 0.1f, GUIImageBackgroundContentAlignmentNames, static_cast<uint32>(std::size(GUIImageBackgroundContentAlignmentNames)) });
	OutProps.push_back({ "Background Tint", EPropertyType::Color4, &BackgroundTint });
	OutProps.push_back({ "Draw Shadow", EPropertyType::Bool, &bDrawShadow });
	OutProps.push_back({ "Shadow Offset", EPropertyType::Vec3, &ShadowOffset, -64.0f, 64.0f, 1.0f });
	OutProps.push_back({ "Shadow Blur", EPropertyType::Float, &ShadowOffset.Z, 0.0f, 32.0f, 0.25f });
	OutProps.push_back({ "Shadow Tint", EPropertyType::Color4, &ShadowTint });
	OutProps.push_back({ "Shadow Top Tint", EPropertyType::Color4, &ShadowTopTint });
	OutProps.push_back({ "Shadow Bottom Tint", EPropertyType::Color4, &ShadowBottomTint });
	OutProps.push_back({ "Tint", EPropertyType::Color4, &Tint });
	OutProps.push_back({ "Border Thickness", EPropertyType::Float, &BorderThickness, 0.0f, 128.0f, 0.1f });
	OutProps.push_back({ "Bottom Border Only", EPropertyType::Bool, &bBottomBorderOnly });
	OutProps.push_back({ "Border Color", EPropertyType::Color4, &BorderColor });
	OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UUIImageComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0 && IsEmptyTexturePath(TextureSlot.Path))
	{
		SetTexture(nullptr);
	}
	else if (strcmp(PropertyName, "Screen Size") == 0)
	{
		SetScreenSize(ScreenSize);
	}
	else if (strcmp(PropertyName, "Border Thickness") == 0)
	{
		SetBorderThickness(BorderThickness);
	}
	else if (strcmp(PropertyName, "Background Texture") == 0)
	{
		if (IsEmptyTexturePath(BackgroundTextureSlot.Path))
		{
			BackgroundTexture = nullptr;
		}
		else
		{
			EnsureBackgroundTextureLoaded();
		}
	}
	else if (strcmp(PropertyName, "Fit Mode") == 0)
	{
		FitMode = static_cast<int32>(SanitizeFitModeValue(FitMode));
	}
	else if (strcmp(PropertyName, "Content Alignment") == 0)
	{
		ContentAlignment = static_cast<int32>(SanitizeContentAlignmentValue(ContentAlignment));
	}
	else if (strcmp(PropertyName, "Background Fit Mode") == 0)
	{
		BackgroundFitMode = static_cast<int32>(SanitizeFitModeValue(BackgroundFitMode));
	}
	else if (strcmp(PropertyName, "Background Content Alignment") == 0)
	{
		BackgroundContentAlignment = static_cast<int32>(SanitizeContentAlignmentValue(BackgroundContentAlignment));
	}
}

void UUIImageComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);

	if (bDrawBackground)
	{
		ID3D11ShaderResourceView* BackgroundSRV = GetBackgroundTextureSRV();
		FVector4 DrawBackgroundTint = BackgroundTint;
		const bool bSolidBackground = (BackgroundSRV == nullptr);
		if (bSolidBackground)
		{
			DrawBackgroundTint.W = (DrawBackgroundTint.W > 0.0f) ? -DrawBackgroundTint.W : DrawBackgroundTint.W;
		}

		const FResolvedImageDrawParams BackgroundDrawParams = ResolveImageDrawParams(
			ResolvedPosition,
			ResolvedSize,
			BackgroundTexture,
			SanitizeFitModeValue(BackgroundFitMode),
			SanitizeContentAlignmentValue(BackgroundContentAlignment));

		Scene.AddScreenQuad(
			BackgroundSRV,
			BackgroundDrawParams.Position,
			BackgroundDrawParams.Size,
			DrawBackgroundTint,
			ZOrder - 1,
			BackgroundDrawParams.UVMin,
			BackgroundDrawParams.UVMax,
			bSolidBackground);
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	const bool bSolidImage = (SRV == nullptr);
	FVector4 DrawTint = Tint;
	if (bSolidImage)
	{
		DrawTint.W = (DrawTint.W > 0.0f) ? -DrawTint.W : DrawTint.W;
	}

	const FResolvedImageDrawParams DrawParams = ResolveImageDrawParams(
		ResolvedPosition,
		ResolvedSize,
		Texture,
		SanitizeFitModeValue(FitMode),
		SanitizeContentAlignmentValue(ContentAlignment));

	if (!bSolidImage && ShouldDrawShadow())
	{
		AddShadowScreenQuad(Scene, SRV, DrawParams.Position, DrawParams.Size, ZOrder - 1, DrawParams.UVMin, DrawParams.UVMax);
	}

	Scene.AddScreenQuad(
		SRV,
		DrawParams.Position,
		DrawParams.Size,
		DrawTint,
		ZOrder,
		DrawParams.UVMin,
		DrawParams.UVMax,
		bSolidImage);

	if (BorderThickness > 0.0f && BorderColor.W > 0.0f)
	{
		const float X = ResolvedPosition.X;
		const float Y = ResolvedPosition.Y;
		const float W = ResolvedSize.X;
		const float H = ResolvedSize.Y;
		const float ClampedThickness = (std::min)(BorderThickness, (std::min)(W, H) * 0.5f);

		Scene.AddScreenQuad(nullptr, FVector2(X, Y + H - ClampedThickness), FVector2(W, ClampedThickness), BorderColor, ZOrder + 1,
			FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), true);
		if (!bBottomBorderOnly)
		{
			Scene.AddScreenQuad(nullptr, FVector2(X, Y), FVector2(W, ClampedThickness), BorderColor, ZOrder + 1,
				FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), true);
			Scene.AddScreenQuad(nullptr, FVector2(X, Y), FVector2(ClampedThickness, H), BorderColor, ZOrder + 1,
				FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), true);
			Scene.AddScreenQuad(nullptr, FVector2(X + W - ClampedThickness, Y), FVector2(ClampedThickness, H), BorderColor, ZOrder + 1,
				FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), true);
		}
	}
}

void UUIImageComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	const float X = ResolvedPosition.X;
	const float Y = ResolvedPosition.Y;
	const float W = ResolvedSize.X;
	const float H = ResolvedSize.Y;
	const int32 OutlineZ = ZOrder + 1000;

	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y - OutlineThickness), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y + H), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X + W, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
}

bool UUIImageComponent::HitTestUIScreenPoint(float X, float Y) const
{
	if (!IsVisible())
	{
		return false;
	}

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	return X >= ResolvedPosition.X
		&& X <= ResolvedPosition.X + ResolvedSize.X
		&& Y >= ResolvedPosition.Y
		&& Y <= ResolvedPosition.Y + ResolvedSize.Y;
}

void UUIImageComponent::SetScreenSize(const FVector& InScreenSize)
{
	ScreenSize.X = (std::max)(1.0f, InScreenSize.X);
	ScreenSize.Y = (std::max)(1.0f, InScreenSize.Y);
	ScreenSize.Z = InScreenSize.Z;

	Width = ScreenSize.X;
	Height = ScreenSize.Y;
}

bool UUIImageComponent::SetTexturePath(const FString& InTexturePath)
{
	TextureSlot.Path = InTexturePath;
	return EnsureTextureLoaded();
}

bool UUIImageComponent::EnsureTextureLoaded()
{
	if (IsEmptyTexturePath(TextureSlot.Path))
	{
		SetTexture(nullptr);
		return true;
	}

	if (ResolveTextureFromPath(TextureSlot.Path))
	{
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	if (UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(TextureSlot.Path, Device))
	{
		SetTexture(LoadedTexture);
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UUIImageComponent::GetResolvedTextureSRV() const
{
	if (Texture && Texture->IsLoaded())
	{
		return Texture->GetSRV();
	}

	return nullptr;
}

bool UUIImageComponent::ShouldDrawShadow() const
{
	return bDrawShadow
		&& ((ShadowTopTint.W * ShadowTint.W) > 0.0f || (ShadowBottomTint.W * ShadowTint.W) > 0.0f);
}

FVector2 UUIImageComponent::GetShadowOffset2D() const
{
	return FVector2(ShadowOffset.X, ShadowOffset.Y);
}

float UUIImageComponent::GetShadowBlurRadius() const
{
	return (std::max)(0.0f, ShadowOffset.Z);
}

FVector4 UUIImageComponent::GetShadowMaskTopColor() const
{
	return FVector4(
		ShadowTopTint.X * ShadowTint.X,
		ShadowTopTint.Y * ShadowTint.Y,
		ShadowTopTint.Z * ShadowTint.Z,
		-((ShadowTopTint.W * ShadowTint.W) + 1.0f));
}

FVector4 UUIImageComponent::GetShadowMaskBottomColor() const
{
	return FVector4(
		ShadowBottomTint.X * ShadowTint.X,
		ShadowBottomTint.Y * ShadowTint.Y,
		ShadowBottomTint.Z * ShadowTint.Z,
		-((ShadowBottomTint.W * ShadowTint.W) + 1.0f));
}

namespace
{
	FVector4 ScaleShadowMaskColor(const FVector4& Color, float Strength)
	{
		const float Alpha = (Color.W < -1.0f) ? (-Color.W - 1.0f) : 0.0f;
		return FVector4(Color.X, Color.Y, Color.Z, -((Alpha * Strength) + 1.0f));
	}
}

void UUIImageComponent::AddShadowScreenQuad(FScene& Scene, ID3D11ShaderResourceView* TextureSRV, const FVector2& Position, const FVector2& Size, int32 InZOrder,
	const FVector2& UVMin, const FVector2& UVMax) const
{
	if (!ShouldDrawShadow())
	{
		return;
	}

	const FVector4 TopColor = GetShadowMaskTopColor();
	const FVector4 BottomColor = GetShadowMaskBottomColor();
	const FVector2 BasePosition = Position + GetShadowOffset2D();
	const float BlurRadius = GetShadowBlurRadius();

	if (BlurRadius <= 0.01f)
	{
		Scene.AddScreenQuad(TextureSRV, BasePosition, Size, TopColor, BottomColor, InZOrder, UVMin, UVMax);
		return;
	}

	Scene.AddScreenQuad(
		TextureSRV,
		BasePosition,
		Size,
		ScaleShadowMaskColor(TopColor, 0.36f),
		ScaleShadowMaskColor(BottomColor, 0.36f),
		InZOrder,
		UVMin,
		UVMax);

	const FVector2 SampleOffsets[] = {
		FVector2(1.0f, 0.0f),
		FVector2(-1.0f, 0.0f),
		FVector2(0.0f, 1.0f),
		FVector2(0.0f, -1.0f),
		FVector2(0.70710678f, 0.70710678f),
		FVector2(-0.70710678f, 0.70710678f),
		FVector2(0.70710678f, -0.70710678f),
		FVector2(-0.70710678f, -0.70710678f),
	};

	for (const FVector2& SampleOffset : SampleOffsets)
	{
		Scene.AddScreenQuad(
			TextureSRV,
			BasePosition + (SampleOffset * BlurRadius),
			Size,
			ScaleShadowMaskColor(TopColor, 0.08f),
			ScaleShadowMaskColor(BottomColor, 0.08f),
			InZOrder,
			UVMin,
			UVMax);
	}
}

bool UUIImageComponent::EnsureBackgroundTextureLoaded() const
{
	if (IsEmptyTexturePath(BackgroundTextureSlot.Path))
	{
		BackgroundTexture = nullptr;
		return true;
	}

	if (BackgroundTexture && BackgroundTexture->IsLoaded())
	{
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	if (UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(BackgroundTextureSlot.Path, Device))
	{
		BackgroundTexture = LoadedTexture;
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UUIImageComponent::GetBackgroundTextureSRV() const
{
	if (BackgroundTexture && BackgroundTexture->IsLoaded())
	{
		return BackgroundTexture->GetSRV();
	}

	if (EnsureBackgroundTextureLoaded() && BackgroundTexture && BackgroundTexture->IsLoaded())
	{
		return BackgroundTexture->GetSRV();
	}

	return nullptr;
}

bool UUIImageComponent::ResolveLayoutRect(FVector2& OutPosition, FVector2& OutSize) const
{
	OutSize = ResolveScreenSize2D();
	OutPosition = ResolveScreenPosition(OutSize);
	return true;
}

FVector2 UUIImageComponent::GetResolvedScreenPosition() const
{
	const FVector2 ResolvedSize = ResolveScreenSize2D();
	return ResolveScreenPosition(ResolvedSize);
}

FVector2 UUIImageComponent::GetResolvedScreenSize() const
{
	return ResolveScreenSize2D();
}

FVector2 UUIImageComponent::ResolveScreenPosition(const FVector2& ElementSize) const
{
	if (!bUseAnchoredLayout)
	{
		return FVector2(ScreenPosition.X, ScreenPosition.Y);
	}

	FVector2 ParentPosition(0.0f, 0.0f);
	FVector2 ParentSize = GetViewportSize2D();
	ResolveParentUILayoutRect(GetParent(), ParentPosition, ParentSize);

	const float ClampedAnchorX = (std::clamp)(Anchor.X, 0.0f, 1.0f);
	const float ClampedAnchorY = (std::clamp)(Anchor.Y, 0.0f, 1.0f);
	const float ClampedAlignmentX = (std::clamp)(Alignment.X, 0.0f, 1.0f);
	const float ClampedAlignmentY = (std::clamp)(Alignment.Y, 0.0f, 1.0f);

	return FVector2(
		ParentPosition.X + ParentSize.X * ClampedAnchorX + AnchorOffset.X - ElementSize.X * ClampedAlignmentX,
		ParentPosition.Y + ParentSize.Y * ClampedAnchorY + AnchorOffset.Y - ElementSize.Y * ClampedAlignmentY);
}

FVector2 UUIImageComponent::ResolveScreenSize2D() const
{
	return FVector2(ScreenSize.X, ScreenSize.Y);
}
