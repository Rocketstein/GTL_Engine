#include "Component/UIBackgroundComponent.h"

#include "Component/CanvasRootComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UUIBackgroundComponent, UUIImageComponent)

namespace
{
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
}

UUIBackgroundComponent::UUIBackgroundComponent()
{
	SetScreenPosition(FVector(0.0f, 0.0f, 0.0f));
	SetScreenSize(FVector(1920.0f, 1080.0f, 0.0f));
	SetAnchoredLayoutEnabled(false);
	SetZOrder(-10000);
}

void UUIBackgroundComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Tint", EPropertyType::Color4, &Tint });
	OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UUIBackgroundComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0 && (TextureSlot.Path.empty() || TextureSlot.Path == "None"))
	{
		SetTexturePath("");
	}
}

void UUIBackgroundComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	const bool bSolidBackground = (SRV == nullptr);
	FVector4 DrawTint = Tint;
	if (bSolidBackground)
	{
		DrawTint.W = (DrawTint.W > 0.0f) ? -DrawTint.W : DrawTint.W;
	}

	FVector2 BackgroundPosition(0.0f, 0.0f);
	FVector2 BackgroundSize = GetViewportSize2D();
	ResolveBackgroundRect(BackgroundPosition, BackgroundSize);
	Scene.AddScreenQuad(SRV, BackgroundPosition, BackgroundSize, DrawTint, ZOrder, FVector2(0.0f, 0.0f), FVector2(1.0f, 1.0f), bSolidBackground);
}

void UUIBackgroundComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	FVector2 Position(0.0f, 0.0f);
	FVector2 Size = GetViewportSize2D();
	ResolveBackgroundRect(Position, Size);
	const int32 OutlineZ = ZOrder + 1000;

	Scene.AddScreenQuad(nullptr, FVector2(Position.X - OutlineThickness, Position.Y - OutlineThickness), FVector2(Size.X + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(Position.X - OutlineThickness, Position.Y + Size.Y), FVector2(Size.X + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(Position.X - OutlineThickness, Position.Y), FVector2(OutlineThickness, Size.Y), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(Position.X + Size.X, Position.Y), FVector2(OutlineThickness, Size.Y), SelectedUIOutlineColor, OutlineZ);
}

bool UUIBackgroundComponent::HitTestUIScreenPoint(float X, float Y) const
{
	if (!IsVisible())
	{
		return false;
	}

	FVector2 Position(0.0f, 0.0f);
	FVector2 Size = GetViewportSize2D();
	ResolveBackgroundRect(Position, Size);
	return X >= Position.X && X <= Position.X + Size.X && Y >= Position.Y && Y <= Position.Y + Size.Y;
}

bool UUIBackgroundComponent::ResolveBackgroundRect(FVector2& OutPosition, FVector2& OutSize) const
{
	for (const USceneComponent* Current = GetParent(); Current != nullptr; Current = Current->GetParent())
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

FVector2 UUIBackgroundComponent::GetViewportSize2D() const
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
