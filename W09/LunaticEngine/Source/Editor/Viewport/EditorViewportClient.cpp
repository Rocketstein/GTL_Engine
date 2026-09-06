#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Settings/EditorSettings.h"
#include "Core/ProjectSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "Math/Vector.h"
#include "Math/MathUtils.h"

#include "Component/GizmoComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/MeshComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/UI/EditorAccentColor.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "Viewport/GameViewportClient.h"
#include "ImGui/imgui.h"
#include "Component/Light/LightComponentBase.h"

#include <cfloat>

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

namespace
{
	void SyncPIEViewportRect(FViewport* Viewport, const FRect& ViewportScreenRect)
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->IsPlayingInEditor())
			{
				if (UGameViewportClient* GameViewportClient = EditorEngine->GetGameViewportClient())
				{
					if (GameViewportClient->GetViewport() == Viewport)
					{
						GameViewportClient->SetViewport(Viewport);
						GameViewportClient->SetCursorClipRect(ViewportScreenRect);
					}
				}
			}
		}
	}

	struct FCameraBookmark
	{
		FVector Location;
		FRotator Rotation;
		bool bValid = false;
	};
	static FCameraBookmark GCameraBookmarks[10];

	enum class EUIScreenGizmoAxis : int32
	{
		None = 0,
		X = 1,
		Y = 2,
		XY = 3
	};

	bool IsUIComponentSelectable(const UActorComponent* Component)
	{
		return Component
			&& !Component->IsHiddenInComponentTree()
			&& !Component->IsEditorOnlyComponent()
			&& Component->SupportsUIScreenPicking();
	}

	USceneComponent* FindTopmostUIComponentAt(UWorld* World, float X, float Y)
	{
		if (!World)
		{
			return nullptr;
		}

		USceneComponent* BestComponent = nullptr;
		int32 BestZOrder = INT_MIN;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!IsUIComponentSelectable(Component))
				{
					continue;
				}

				USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
				if (!SceneComponent || !Component->HitTestUIScreenPoint(X, Y))
				{
					continue;
				}

				const int32 ZOrder = Component->GetUIScreenPickingZOrder();
				if (!BestComponent || ZOrder >= BestZOrder)
				{
					BestComponent = SceneComponent;
					BestZOrder = ZOrder;
				}
			}
		}

		return BestComponent;
	}

	bool IsUIScreenTransformableComponent(const USceneComponent* Component)
	{
		return Cast<UUIImageComponent>(Component) || Cast<UUIScreenTextComponent>(Component);
	}

	bool GetUIScreenTextBounds(const UUIScreenTextComponent* TextComponent, float& OutX, float& OutY, float& OutWidth, float& OutHeight)
	{
		return TextComponent && TextComponent->GetResolvedScreenBounds(OutX, OutY, OutWidth, OutHeight);
	}

	bool GetUIScreenComponentBounds(const USceneComponent* Component, float& OutX, float& OutY, float& OutWidth, float& OutHeight)
	{
		if (const UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(Component))
		{
			const FVector2 ResolvedPosition = ImageComponent->GetResolvedScreenPosition();
			const FVector2 ResolvedSize = ImageComponent->GetResolvedScreenSize();
			OutX = ResolvedPosition.X;
			OutY = ResolvedPosition.Y;
			OutWidth = (std::max)(1.0f, ResolvedSize.X);
			OutHeight = (std::max)(1.0f, ResolvedSize.Y);
			return true;
		}

		if (const UUIScreenTextComponent* TextComponent = Cast<UUIScreenTextComponent>(Component))
		{
			return GetUIScreenTextBounds(TextComponent, OutX, OutY, OutWidth, OutHeight);
		}

		return false;
	}

	bool GetUIScreenComponentPosition(const USceneComponent* Component, FVector& OutPosition)
	{
		if (const UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(Component))
		{
			const FVector2 ResolvedPosition = ImageComponent->GetResolvedScreenPosition();
			OutPosition = FVector(ResolvedPosition.X, ResolvedPosition.Y, ImageComponent->GetScreenPosition().Z);
			return true;
		}

		if (const UUIScreenTextComponent* TextComponent = Cast<UUIScreenTextComponent>(Component))
		{
			float X = 0.0f;
			float Y = 0.0f;
			float Width = 0.0f;
			float Height = 0.0f;
			if (!TextComponent->GetResolvedScreenBounds(X, Y, Width, Height))
			{
				return false;
			}

			OutPosition = FVector(X, Y, TextComponent->GetScreenPosition().Z);
			return true;
		}

		return false;
	}

	bool SetUIScreenComponentPosition(USceneComponent* Component, const FVector& InPosition)
	{
		if (UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(Component))
		{
			if (ImageComponent->IsAnchoredLayoutEnabled())
			{
				const FVector2 CurrentResolvedPosition = ImageComponent->GetResolvedScreenPosition();
				FVector AnchorOffset = ImageComponent->GetAnchorOffset();
				AnchorOffset.X += InPosition.X - CurrentResolvedPosition.X;
				AnchorOffset.Y += InPosition.Y - CurrentResolvedPosition.Y;
				ImageComponent->SetAnchorOffset(AnchorOffset);
				return true;
			}

			ImageComponent->SetScreenPosition(InPosition);
			return true;
		}

		if (UUIScreenTextComponent* TextComponent = Cast<UUIScreenTextComponent>(Component))
		{
			if (TextComponent->IsAnchoredLayoutEnabled())
			{
				float CurrentX = 0.0f;
				float CurrentY = 0.0f;
				float CurrentWidth = 0.0f;
				float CurrentHeight = 0.0f;
				if (!TextComponent->GetResolvedScreenBounds(CurrentX, CurrentY, CurrentWidth, CurrentHeight))
				{
					return false;
				}

				FVector AnchorOffset = TextComponent->GetAnchorOffset();
				AnchorOffset.X += InPosition.X - CurrentX;
				AnchorOffset.Y += InPosition.Y - CurrentY;
				TextComponent->SetAnchorOffset(AnchorOffset);
				return true;
			}

			TextComponent->SetScreenPosition(InPosition);
			return true;
		}

		return false;
	}

	bool TryConvertMouseToViewportPixel(
		const ImVec2& MousePos,
		const FRect& ViewportScreenRect,
		const FViewport* Viewport,
		float FallbackWidth,
		float FallbackHeight,
		float& OutViewportX,
		float& OutViewportY)
	{
		if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
		{
			return false;
		}

		const float LocalX = MousePos.x - ViewportScreenRect.X;
		const float LocalY = MousePos.y - ViewportScreenRect.Y;
		if (LocalX < 0.0f || LocalY < 0.0f || LocalX >= ViewportScreenRect.Width || LocalY >= ViewportScreenRect.Height)
		{
			return false;
		}

		const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
		const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
		if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
		{
			return false;
		}

		const float ScaleX = TargetWidth / ViewportScreenRect.Width;
		const float ScaleY = TargetHeight / ViewportScreenRect.Height;
		OutViewportX = LocalX * ScaleX;
		OutViewportY = LocalY * ScaleY;
		return true;
	}

	bool TryConvertViewportPixelToScreenPoint(
		float ViewportX,
		float ViewportY,
		const FRect& ViewportScreenRect,
		const FViewport* Viewport,
		float FallbackWidth,
		float FallbackHeight,
		float& OutScreenX,
		float& OutScreenY)
	{
		if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
		{
			return false;
		}

		const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
		const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
		if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
		{
			return false;
		}

		const float ScaleX = ViewportScreenRect.Width / TargetWidth;
		const float ScaleY = ViewportScreenRect.Height / TargetHeight;
		OutScreenX = ViewportScreenRect.X + ViewportX * ScaleX;
		OutScreenY = ViewportScreenRect.Y + ViewportY * ScaleY;
		return true;
	}

	bool ProjectWorldToViewport(
		const FMatrix& ViewProjection,
		const FVector& WorldPosition,
		float ViewportWidth,
		float ViewportHeight,
		float& OutScreenX,
		float& OutScreenY,
		float& OutDepth)
	{
		const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
		OutScreenX = (ClipSpace.X * 0.5f + 0.5f) * ViewportWidth;
		OutScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * ViewportHeight;
		OutDepth = ClipSpace.Z;
		return std::isfinite(OutScreenX) && std::isfinite(OutScreenY) && std::isfinite(OutDepth);
	}

	bool EditorRaycastAllVisiblePrimitives(UWorld* World, const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor)
	{
		FRayHitResult BestHit{};
		AActor* BestActor = nullptr;

		if (!World)
		{
			return false;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				FRayHitResult CandidateHit{};
				if (!Primitive->LineTraceComponent(Ray, CandidateHit))
				{
					float AABBTMin = 0.0f;
					float AABBTMax = 0.0f;
					const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
					if (!Bounds.IsValid() || !FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, AABBTMin, AABBTMax))
					{
						continue;
					}

					CandidateHit.HitComponent = Primitive;
					CandidateHit.Distance = AABBTMin >= 0.0f ? AABBTMin : AABBTMax;
					CandidateHit.WorldHitLocation = Ray.Origin + Ray.Direction * CandidateHit.Distance;
					CandidateHit.bHit = true;
				}

				if (CandidateHit.Distance < BestHit.Distance)
				{
					BestHit = CandidateHit;
					BestActor = Actor;
				}
			}
		}

		if (!BestActor)
		{
			return false;
		}

		OutHitResult = BestHit;
		OutActor = BestActor;
		return true;
	}

	void BuildBoundingBoxCorners(const FBoundingBox& Bounds, FVector OutCorners[8])
	{
		const FVector& Min = Bounds.Min;
		const FVector& Max = Bounds.Max;
		OutCorners[0] = FVector(Min.X, Min.Y, Min.Z);
		OutCorners[1] = FVector(Max.X, Min.Y, Min.Z);
		OutCorners[2] = FVector(Min.X, Max.Y, Min.Z);
		OutCorners[3] = FVector(Max.X, Max.Y, Min.Z);
		OutCorners[4] = FVector(Min.X, Min.Y, Max.Z);
		OutCorners[5] = FVector(Max.X, Min.Y, Max.Z);
		OutCorners[6] = FVector(Min.X, Max.Y, Max.Z);
		OutCorners[7] = FVector(Max.X, Max.Y, Max.Z);
	}

	AActor* FindScreenSpacePrimitiveAt(
		UWorld* World,
		const UCameraComponent* Camera,
		float MouseViewportX,
		float MouseViewportY,
		float ViewportWidth,
		float ViewportHeight,
		UPrimitiveComponent*& OutPrimitive)
	{
		OutPrimitive = nullptr;
		if (!World || !Camera || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
		{
			return nullptr;
		}

		const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
		AActor* BestActor = nullptr;
		UPrimitiveComponent* BestPrimitive = nullptr;
		float BestArea = FLT_MAX;
		float BestDepth = FLT_MAX;
		float BestScore = FLT_MAX;
		constexpr float ScreenPickPadding = 12.0f;
		constexpr float CenterPickRadiusSq = 24.0f * 24.0f;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || !Primitive->IsVisible())
				{
					continue;
				}

				const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
				if (!Bounds.IsValid())
				{
					continue;
				}

				FVector Corners[8];
				BuildBoundingBoxCorners(Bounds, Corners);

				float MinX = FLT_MAX;
				float MinY = FLT_MAX;
				float MaxX = -FLT_MAX;
				float MaxY = -FLT_MAX;
				float MinDepth = FLT_MAX;
				bool bProjectedAny = false;

				for (const FVector& Corner : Corners)
				{
					float ScreenX = 0.0f;
					float ScreenY = 0.0f;
					float Depth = 0.0f;
					if (!ProjectWorldToViewport(ViewProjection, Corner, ViewportWidth, ViewportHeight, ScreenX, ScreenY, Depth))
					{
						continue;
					}

					MinX = (std::min)(MinX, ScreenX);
					MinY = (std::min)(MinY, ScreenY);
					MaxX = (std::max)(MaxX, ScreenX);
					MaxY = (std::max)(MaxY, ScreenY);
					MinDepth = (std::min)(MinDepth, Depth);
					bProjectedAny = true;
				}

				if (!bProjectedAny)
				{
					continue;
				}

				const float ExpandedMinX = MinX - ScreenPickPadding;
				const float ExpandedMaxX = MaxX + ScreenPickPadding;
				const float ExpandedMinY = MinY - ScreenPickPadding;
				const float ExpandedMaxY = MaxY + ScreenPickPadding;
				if (MouseViewportX < ExpandedMinX || MouseViewportX > ExpandedMaxX || MouseViewportY < ExpandedMinY || MouseViewportY > ExpandedMaxY)
				{
					continue;
				}

				const float Area = (std::max)(1.0f, MaxX - MinX) * (std::max)(1.0f, MaxY - MinY);
				const float ClampedX = (std::max)(MinX, (std::min)(MouseViewportX, MaxX));
				const float ClampedY = (std::max)(MinY, (std::min)(MouseViewportY, MaxY));
				const float DistanceSq = (MouseViewportX - ClampedX) * (MouseViewportX - ClampedX)
					+ (MouseViewportY - ClampedY) * (MouseViewportY - ClampedY);
				if (!BestActor
					|| DistanceSq < BestScore
					|| (std::abs(DistanceSq - BestScore) < 1.0f && (Area < BestArea || (std::abs(Area - BestArea) < 1.0f && MinDepth < BestDepth))))
				{
					BestActor = Actor;
					BestPrimitive = Primitive;
					BestScore = DistanceSq;
					BestArea = Area;
					BestDepth = MinDepth;
				}
			}
		}

		if (!BestActor)
		{
			for (AActor* Actor : World->GetActors())
			{
				if (!Actor || !Actor->IsVisible())
				{
					continue;
				}

				float ScreenX = 0.0f;
				float ScreenY = 0.0f;
				float Depth = 0.0f;
				if (!ProjectWorldToViewport(ViewProjection, Actor->GetActorLocation(), ViewportWidth, ViewportHeight, ScreenX, ScreenY, Depth))
				{
					continue;
				}

				const float DistanceSq = (MouseViewportX - ScreenX) * (MouseViewportX - ScreenX)
					+ (MouseViewportY - ScreenY) * (MouseViewportY - ScreenY);
				if (DistanceSq > CenterPickRadiusSq)
				{
					continue;
				}

				BestActor = Actor;
				BestScore = DistanceSq;
				BestDepth = Depth;
				for (UActorComponent* Component : Actor->GetComponents())
				{
					UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
					if (Primitive && Primitive->IsVisible())
					{
						BestPrimitive = Primitive;
						break;
					}
				}
				break;
			}
		}

		OutPrimitive = BestPrimitive;
		return BestActor;
	}
}

FEditorViewportClient::FEditorViewportClient()
{
	SetupInput();
}

FEditorViewportClient::~FEditorViewportClient()
{
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	if (EditorMappingContext)
	{
		for (auto& Mapping : EditorMappingContext->Mappings)
		{
			for (auto* Trigger : Mapping.Triggers) delete Trigger;
			for (auto* Modifier : Mapping.Modifiers) delete Modifier;
		}
		delete EditorMappingContext;
	}

	delete ActionEditorMove;
	delete ActionEditorRotate;
	delete ActionEditorPan;
	delete ActionEditorZoom;
	delete ActionEditorOrbit;
	delete ActionEditorFocus;
	delete ActionEditorDelete;
	delete ActionEditorDuplicate;
	delete ActionEditorToggleGizmoMode;
	delete ActionEditorToggleCoordSystem;
	delete ActionEditorEscape;
	delete ActionEditorTogglePIE;
	delete ActionEditorDecreaseSnap;
	delete ActionEditorIncreaseSnap;
	delete ActionEditorVertexSnap;
	delete ActionEditorSnapToFloor;
	delete ActionEditorSetBookmark;
	delete ActionEditorJumpToBookmark;
	delete ActionEditorSetViewportPerspective;
	delete ActionEditorSetViewportTop;
	delete ActionEditorSetViewportFront;
	delete ActionEditorSetViewportRight;
	delete ActionEditorToggleGridSnap;
	delete ActionEditorToggleRotationSnap;
	delete ActionEditorToggleScaleSnap;
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FEditorViewportClient::SetupInput()
{
	// 1. Create Actions
	ActionEditorMove = new FInputAction("IA_EditorMove", EInputActionValueType::Axis3D);
	ActionEditorRotate = new FInputAction("IA_EditorRotate", EInputActionValueType::Axis2D);
	ActionEditorPan = new FInputAction("IA_EditorPan", EInputActionValueType::Axis2D);
	ActionEditorZoom = new FInputAction("IA_EditorZoom", EInputActionValueType::Float);
	ActionEditorOrbit = new FInputAction("IA_EditorOrbit", EInputActionValueType::Axis2D);

	ActionEditorFocus = new FInputAction("IA_EditorFocus", EInputActionValueType::Bool);
	ActionEditorDelete = new FInputAction("IA_EditorDelete", EInputActionValueType::Bool);
	ActionEditorDuplicate = new FInputAction("IA_EditorDuplicate", EInputActionValueType::Bool);
	ActionEditorToggleGizmoMode = new FInputAction("IA_EditorToggleGizmoMode", EInputActionValueType::Bool);
	ActionEditorToggleCoordSystem = new FInputAction("IA_EditorToggleCoordSystem", EInputActionValueType::Bool);
	ActionEditorEscape = new FInputAction("IA_EditorEscape", EInputActionValueType::Bool);
	ActionEditorTogglePIE = new FInputAction("IA_EditorTogglePIE", EInputActionValueType::Bool);
	
	ActionEditorDecreaseSnap = new FInputAction("IA_EditorDecreaseSnap", EInputActionValueType::Bool);
	ActionEditorIncreaseSnap = new FInputAction("IA_EditorIncreaseSnap", EInputActionValueType::Bool);
	ActionEditorVertexSnap = new FInputAction("IA_EditorVertexSnap", EInputActionValueType::Bool);
	ActionEditorSnapToFloor = new FInputAction("IA_EditorSnapToFloor", EInputActionValueType::Bool);
	ActionEditorSetBookmark = new FInputAction("IA_EditorSetBookmark", EInputActionValueType::Float);
	ActionEditorJumpToBookmark = new FInputAction("IA_EditorJumpToBookmark", EInputActionValueType::Float);

	ActionEditorSetViewportPerspective = new FInputAction("IA_SetViewportPerspective", EInputActionValueType::Bool);
	ActionEditorSetViewportTop = new FInputAction("IA_SetViewportTop", EInputActionValueType::Bool);
	ActionEditorSetViewportFront = new FInputAction("IA_SetViewportFront", EInputActionValueType::Bool);
	ActionEditorSetViewportRight = new FInputAction("IA_SetViewportRight", EInputActionValueType::Bool);

	ActionEditorToggleGridSnap = new FInputAction("IA_ToggleGridSnap", EInputActionValueType::Bool);
	ActionEditorToggleRotationSnap = new FInputAction("IA_ToggleRotationSnap", EInputActionValueType::Bool);
	ActionEditorToggleScaleSnap = new FInputAction("IA_ToggleScaleSnap", EInputActionValueType::Bool);

	// 2. Create Mapping Context
	EditorMappingContext = new FInputMappingContext();
	EditorMappingContext->ContextName = "IMC_Editor";

	// Move: WASD QE
	EditorMappingContext->AddMapping(ActionEditorMove, 'W');
	EditorMappingContext->AddMapping(ActionEditorMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'D').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorMove, 'A').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'E').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->AddMapping(ActionEditorMove, 'Q').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

	// Rotate: Arrows + Right Mouse
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_UP).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_DOWN).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_LEFT).Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_RIGHT);
	
	// Mouse Rotate 
	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Pan: Middle Mouse
	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Zoom: Wheel
	EditorMappingContext->AddMapping(ActionEditorZoom, static_cast<int32>(EInputKey::MouseWheel));

	// Orbit: Alt + Left Mouse
	EditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// --- Shortcuts ---
	EditorMappingContext->AddMapping(ActionEditorFocus, 'F');
	EditorMappingContext->AddMapping(ActionEditorDelete, VK_DELETE);
	EditorMappingContext->AddMapping(ActionEditorDuplicate, 'D');
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, VK_SPACE);
	EditorMappingContext->AddMapping(ActionEditorToggleCoordSystem, 'X');
	EditorMappingContext->AddMapping(ActionEditorEscape, VK_ESCAPE);
	EditorMappingContext->AddMapping(ActionEditorTogglePIE, VK_F8);
	
	EditorMappingContext->AddMapping(ActionEditorDecreaseSnap, VK_OEM_4); // [
	EditorMappingContext->AddMapping(ActionEditorIncreaseSnap, VK_OEM_6); // ]
	EditorMappingContext->AddMapping(ActionEditorVertexSnap, 'V');
	EditorMappingContext->AddMapping(ActionEditorSnapToFloor, VK_END);

	// Viewport Type Shortcuts
	EditorMappingContext->AddMapping(ActionEditorSetViewportPerspective, 'G'); // Alt+G
	EditorMappingContext->AddMapping(ActionEditorSetViewportTop, 'J');		   // Alt+J
	EditorMappingContext->AddMapping(ActionEditorSetViewportFront, 'H');	   // Alt+H
	EditorMappingContext->AddMapping(ActionEditorSetViewportRight, 'K');	   // Alt+K

	// Snap Toggles
	EditorMappingContext->AddMapping(ActionEditorToggleGridSnap, 'G');		// Shift+G
	EditorMappingContext->AddMapping(ActionEditorToggleRotationSnap, 'R');	// Shift+R
	EditorMappingContext->AddMapping(ActionEditorToggleScaleSnap, 'S');		// Shift+S

	// Bookmarks 0-9
	for (int32 i = 0; i < 10; ++i)
	{
		int32 Key = '0' + i;
		EditorMappingContext->AddMapping(ActionEditorSetBookmark, Key).Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
		EditorMappingContext->AddMapping(ActionEditorJumpToBookmark, Key).Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
	}

	// UE Style Gizmo Shortcuts (QWER)
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'Q'); // Select
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'W'); // Translate
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'E'); // Rotate
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'R'); // Scale

	// Game View Toggle (G)
	FInputAction* ActionEditorToggleGameView = new FInputAction("IA_EditorToggleGameView", EInputActionValueType::Bool);
	EditorMappingContext->AddMapping(ActionEditorToggleGameView, 'G');
	EnhancedInputManager.BindAction(ActionEditorToggleGameView, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (!FInputManager::Get().IsKeyDown(VK_MENU) && !FInputManager::Get().IsKeyDown(VK_SHIFT)) RenderOptions.bGameView = !RenderOptions.bGameView;
	});

	// 3. Add Context
	EnhancedInputManager.AddMappingContext(EditorMappingContext, 0);

	// 4. Bind Actions
	EnhancedInputManager.BindAction(ActionEditorMove, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorMove(V); });
	EnhancedInputManager.BindAction(ActionEditorRotate, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorRotate(V); });
	EnhancedInputManager.BindAction(ActionEditorPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorPan(V); });
	EnhancedInputManager.BindAction(ActionEditorZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorZoom(V); });
	EnhancedInputManager.BindAction(ActionEditorOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorOrbit(V); });

	EnhancedInputManager.BindAction(ActionEditorFocus, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorFocus(V); });
	EnhancedInputManager.BindAction(ActionEditorDelete, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorDelete(V); });
	EnhancedInputManager.BindAction(ActionEditorDuplicate, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorDuplicate(V); });
	EnhancedInputManager.BindAction(ActionEditorToggleGizmoMode, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorToggleGizmoMode(V); });
	EnhancedInputManager.BindAction(ActionEditorToggleCoordSystem, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorToggleCoordSystem(V); });
	EnhancedInputManager.BindAction(ActionEditorEscape, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorEscape(V); });
	EnhancedInputManager.BindAction(ActionEditorTogglePIE, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorTogglePIE(V); });
	
	EnhancedInputManager.BindAction(ActionEditorDecreaseSnap, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		FEditorSettings& S = FEditorSettings::Get();
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) S.RotationSnapSize = (std::max)(1.0f, S.RotationSnapSize - 5.0f);
		else S.TranslationSnapSize = (std::max)(0.1f, S.TranslationSnapSize / 2.0f);
	});
	EnhancedInputManager.BindAction(ActionEditorIncreaseSnap, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		FEditorSettings& S = FEditorSettings::Get();
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) S.RotationSnapSize = (std::min)(90.0f, S.RotationSnapSize + 5.0f);
		else S.TranslationSnapSize = (std::min)(1000.0f, S.TranslationSnapSize * 2.0f);
	});

	EnhancedInputManager.BindAction(ActionEditorSnapToFloor, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (SelectionManager) {
			UWorld* W = GetWorld();
			for (AActor* Actor : SelectionManager->GetSelectedActors()) {
				if (!Actor) continue;
				FRay DownRay(Actor->GetActorLocation(), FVector(0, 0, -1));
				FRayHitResult Hit; AActor* HitActor = nullptr;
				if (W && W->RaycastPrimitives(DownRay, Hit, HitActor)) {
					Actor->SetActorLocation(DownRay.Origin + DownRay.Direction * Hit.Distance);
				}
			}
			if (Gizmo) Gizmo->UpdateGizmoTransform();
		}
	});

	EnhancedInputManager.BindAction(ActionEditorSetBookmark, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_CONTROL) && Camera) {
			int32 Index = static_cast<int32>(V.Get());
			if (Index >= 0 && Index < 10) GCameraBookmarks[Index] = { Camera->GetWorldLocation(), Camera->GetRelativeRotation(), true };
		}
	});

	EnhancedInputManager.BindAction(ActionEditorJumpToBookmark, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (!FInputManager::Get().IsKeyDown(VK_CONTROL) && Camera) {
			int32 Index = static_cast<int32>(V.Get());
			if (Index >= 0 && Index < 10 && GCameraBookmarks[Index].bValid) {
				const auto& BM = GCameraBookmarks[Index];
				FocusStartLoc = Camera->GetWorldLocation(); FocusStartRot = Camera->GetRelativeRotation();
				FocusEndLoc = BM.Location; FocusEndRot = BM.Rotation;
				bIsFocusAnimating = true; FocusAnimTimer = 0.0f;
			}
		}
	});

	EnhancedInputManager.BindAction(ActionEditorSetViewportPerspective, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Perspective);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportTop, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Top);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportFront, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Front);
	});
	EnhancedInputManager.BindAction(ActionEditorSetViewportRight, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_MENU)) SetViewportType(ELevelViewportType::Right);
	});

	EnhancedInputManager.BindAction(ActionEditorToggleGridSnap, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableTranslationSnap = !FEditorSettings::Get().bEnableTranslationSnap;
	});
	EnhancedInputManager.BindAction(ActionEditorToggleRotationSnap, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableRotationSnap = !FEditorSettings::Get().bEnableRotationSnap;
	});
	EnhancedInputManager.BindAction(ActionEditorToggleScaleSnap, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		if (FInputManager::Get().IsKeyDown(VK_SHIFT)) FEditorSettings::Get().bEnableScaleSnap = !FEditorSettings::Get().bEnableScaleSnap;
	});
}

void FEditorViewportClient::OnEditorMove(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsKeyDown(VK_CONTROL)) return;
	if (FInputManager::Get().IsMouseButtonDown(VK_RBUTTON))
	{
		EditorMoveAccumulator = EditorMoveAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnEditorRotate(const FInputActionValue& Value)
{
	FInputManager& Input = FInputManager::Get();
	if (Input.IsMouseButtonDown(VK_RBUTTON))
	{
		EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
	}
	else
	{
		FVector KeyboardRotate(0, 0, 0);
		if (Input.IsKeyDown(VK_RIGHT)) KeyboardRotate.X += 1.0f;
		if (Input.IsKeyDown(VK_LEFT)) KeyboardRotate.X -= 1.0f;
		if (Input.IsKeyDown(VK_UP)) KeyboardRotate.Y += 1.0f;
		if (Input.IsKeyDown(VK_DOWN)) KeyboardRotate.Y -= 1.0f;
		if (!KeyboardRotate.IsNearlyZero())
		{
			EditorRotateAccumulator = EditorRotateAccumulator + KeyboardRotate;
		}
	}
}

void FEditorViewportClient::OnEditorPan(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsMouseButtonDown(VK_MBUTTON))
	{
		EditorPanAccumulator = EditorPanAccumulator + Value.GetVector();
	}
	else if (FInputManager::Get().IsKeyDown(VK_MENU) && FInputManager::Get().IsMouseButtonDown(VK_MBUTTON)) // Alt + MMB
	{
		EditorPanAccumulator = EditorPanAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnEditorZoom(const FInputActionValue& Value)
{
	FInputManager& Input = FInputManager::Get();
	if (Input.IsMouseButtonDown(VK_RBUTTON))
	{
		float& Speed = FEditorSettings::Get().CameraSpeed;
		Speed = Clamp(Speed + Value.Get() * 2.0f, 1.0f, 100.0f);
		return;
	}
	EditorZoomAccumulator += Value.Get();
}

void FEditorViewportClient::OnEditorOrbit(const FInputActionValue& Value)
{
	FInputManager& Input = FInputManager::Get();
	if (Input.IsKeyDown(VK_MENU))
	{
		// Alt + LMB = Selection Orbit
		if (Input.IsMouseButtonDown(VK_LBUTTON))
		{
			if (SelectionManager && SelectionManager->GetPrimarySelection() && Camera)
			{
				FVector Pivot = SelectionManager->GetPrimarySelection()->GetActorLocation();
				FVector CameraPos = Camera->GetWorldLocation();
				float Dist = FVector::Distance(CameraPos, Pivot);
				const float Sensitivity = 0.25f;
				FRotator Rotation = Camera->GetRelativeRotation();
				Rotation.Yaw += Value.GetVector().X * Sensitivity;
				Rotation.Pitch = Clamp(Rotation.Pitch + Value.GetVector().Y * Sensitivity, -89.0f, 89.0f);
				FVector NewPos = Pivot - Rotation.ToVector() * (Dist > 0.1f ? Dist : 5.0f);
				Camera->SetWorldLocation(NewPos);
				Camera->SetRelativeRotation(Rotation);
				SyncCameraSmoothingTarget();
			}
			else
			{
				EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
			}
		}
		// Alt + RMB = Scrub Zoom
		else if (Input.IsMouseButtonDown(VK_RBUTTON))
		{
			if (Camera)
			{
				float ScrubSpeed = 0.05f;
				TargetLocation += Camera->GetForwardVector() * (Value.GetVector().X * ScrubSpeed);
			}
		}
	}
}

void FEditorViewportClient::OnEditorFocus(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsMouseButtonDown(VK_RBUTTON)) return;
	if (SelectionManager && Camera)
	{
		AActor* Selected = SelectionManager->GetPrimarySelection();
		if (Selected)
		{
			FVector TargetLoc = Selected->GetActorLocation();
			FVector CameraForward = Camera->GetForwardVector();
			FVector OriginalLoc = Camera->GetWorldLocation();
			FRotator OriginalRot = Camera->GetRelativeRotation();

			float FocusDistance = 5.0f;
			if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Selected->GetRootComponent()))
			{
				FVector Extent = RootPrim->GetWorldBoundingBox().GetExtent();
				float MaxDim = (std::max)({ Extent.X, Extent.Y, Extent.Z });
				FocusDistance = (std::max)(5.0f, MaxDim * 2.5f);
			}

			FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;
			Camera->SetWorldLocation(NewCameraLoc);
			Camera->LookAt(TargetLoc);
			FRotator TargetRot = Camera->GetRelativeRotation();
			Camera->SetWorldLocation(OriginalLoc);
			Camera->SetRelativeRotation(OriginalRot);
			bIsFocusAnimating = true;
			FocusAnimTimer = 0.0f;
			FocusStartLoc = OriginalLoc;
			FocusStartRot = OriginalRot;
			FocusEndLoc = NewCameraLoc;
			FocusEndRot = TargetRot;
		}
	}
}

void FEditorViewportClient::OnEditorDelete(const FInputActionValue& Value)
{
	if (SelectionManager) SelectionManager->DeleteSelectedActors();
}

void FEditorViewportClient::OnEditorDuplicate(const FInputActionValue& Value)
{
	if (SelectionManager && FInputManager::Get().IsKeyDown(VK_CONTROL))
	{
		const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
		if (!ToDuplicate.empty())
		{
			const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
			TArray<AActor*> NewSelection;
			int32 DuplicateIndex = 0;
			for (AActor* Src : ToDuplicate)
			{
				if (!Src) continue;
				AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
				if (Dup)
				{
					Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
					NewSelection.push_back(Dup);
					++DuplicateIndex;
				}
			}
			SelectionManager->ClearSelection();
			for (AActor* Actor : NewSelection) SelectionManager->ToggleSelect(Actor);
			if (GEngine && Cast<UEditorEngine>(GEngine)->GetGizmo())
				Cast<UEditorEngine>(GEngine)->GetGizmo()->UpdateGizmoTransform();
		}
	}
}

void FEditorViewportClient::OnEditorToggleGizmoMode(const FInputActionValue& Value)
{
	FInputManager& Input = FInputManager::Get();
	if (Input.IsMouseButtonDown(VK_RBUTTON)) return;

	if (Gizmo)
	{
		if (Input.IsKeyPressed('W')) Gizmo->UpdateGizmoMode(EGizmoMode::Translate);
		else if (Input.IsKeyPressed('E')) Gizmo->UpdateGizmoMode(EGizmoMode::Rotate);
		else if (Input.IsKeyPressed('R')) Gizmo->UpdateGizmoMode(EGizmoMode::Scale);
		else if (Input.IsKeyPressed('Q')) Gizmo->UpdateGizmoMode(EGizmoMode::Select);
		else Gizmo->SetNextMode(); 

		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			EditorEngine->ApplyTransformSettingsToGizmo();
	}
}

void FEditorViewportClient::OnEditorToggleCoordSystem(const FInputActionValue& Value)
{
	if (!FInputManager::Get().IsKeyDown(VK_CONTROL))
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			EditorEngine->ToggleCoordSystem();
	}
}

void FEditorViewportClient::OnEditorEscape(const FInputActionValue& Value)
{
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor()) EditorEngine->RequestEndPlayMap();
	}
}

void FEditorViewportClient::OnEditorTogglePIE(const FInputActionValue& Value)
{
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor()) EditorEngine->TogglePIEControlMode();
	}
}

void FEditorViewportClient::SetLightViewOverride(ULightComponentBase* Light)
{
	LightViewOverride = Light;
	PointLightFaceIndex = 0;
	if (Light && SelectionManager) SelectionManager->ClearSelection();
}

void FEditorViewportClient::ClearLightViewOverride()
{
	LightViewOverride = nullptr;
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings) return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
	SyncCameraSmoothingTarget();
}

bool FEditorViewportClient::FocusActor(AActor* Actor)
{
	if (!Actor || !Camera)
	{
		return false;
	}

	const FVector TargetLoc = Actor->GetActorLocation();
	const FVector CameraForward = Camera->GetForwardVector();

	const FVector OriginalLoc = Camera->GetWorldLocation();
	const FRotator OriginalRot = Camera->GetRelativeRotation();

	constexpr float FocusDistance = 5.0f;
	const FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;

	Camera->SetWorldLocation(NewCameraLoc);
	Camera->LookAt(TargetLoc);
	const FRotator TargetRot = Camera->GetRelativeRotation();

	Camera->SetWorldLocation(OriginalLoc);
	Camera->SetRelativeRotation(OriginalRot);

	bIsFocusAnimating = true;
	FocusAnimTimer = 0.0f;
	FocusStartLoc = OriginalLoc;
	FocusStartRot = OriginalRot;
	FocusEndLoc = NewCameraLoc;
	FocusEndRot = TargetRot;
	return true;
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera) return;
	RenderOptions.ViewportType = NewType;
	if (NewType == ELevelViewportType::Perspective) { Camera->SetOrthographic(false); SyncCameraSmoothingTarget(); return; }
	if (NewType == ELevelViewportType::FreeOrthographic) { Camera->SetOrthographic(true); SyncCameraSmoothingTarget(); return; }
	Camera->SetOrthographic(true);
	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0); FVector Rotation = FVector(0, 0, 0);
	switch (NewType)
	{
	case ELevelViewportType::Top: Position = FVector(0, 0, OrthoDistance); Rotation = FVector(0, 90.0f, 0); break;
	case ELevelViewportType::Bottom: Position = FVector(0, 0, -OrthoDistance); Rotation = FVector(0, -90.0f, 0); break;
	case ELevelViewportType::Front: Position = FVector(OrthoDistance, 0, 0); Rotation = FVector(0, 0, 180.0f); break;
	case ELevelViewportType::Back: Position = FVector(-OrthoDistance, 0, 0); Rotation = FVector(0, 0, 0.0f); break;
	case ELevelViewportType::Left: Position = FVector(0, -OrthoDistance, 0); Rotation = FVector(0, 0, 90.0f); break;
	case ELevelViewportType::Right: Position = FVector(0, OrthoDistance, 0); Rotation = FVector(0, 0, -90.0f); break;
	default: break;
	}
	Camera->SetRelativeLocation(Position); Camera->SetRelativeRotation(Rotation); SyncCameraSmoothingTarget();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f) WindowWidth = InWidth;
	if (InHeight > 0.0f) WindowHeight = InHeight;
	if (Camera) Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (!bIsActive) return;
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			FInputManager& Input = FInputManager::Get();
			if (Input.IsKeyPressed(VK_ESCAPE)) { EditorEngine->RequestEndPlayMap(); return; }
			if (Input.IsKeyPressed(VK_F8)) { EditorEngine->TogglePIEControlMode(); }

			// 매 Tick마다 SetDrivingCamera(EditorViewportCamera)를 하지 말 것
			if (EditorEngine->IsPIEPossessedMode())
			{
				if (UGameViewportClient* GameViewportClient = EditorEngine->GetGameViewportClient())
				{
					GameViewportClient->SetViewport(Viewport);
					GameViewportClient->SetCursorClipRect(ViewportScreenRect);

					if (!GameViewportClient->HasPossessedTarget())
					{
						if (UWorld* World = EditorEngine->GetWorld())
						{
							GameViewportClient->Possess(World->GetActiveCamera());
						}

						if (!GameViewportClient->HasPossessedTarget())
						{
							GameViewportClient->Possess(Camera);
						}
					}

					GameViewportClient->ProcessPIEInput(DeltaTime);
				}
				return;
			}
		}
	}
	SyncCameraSmoothingTarget();
	if (bIsFocusAnimating && Camera)
	{
		FocusAnimTimer += DeltaTime;
		float Alpha = FocusAnimTimer / FocusAnimDuration;
		if (Alpha >= 1.0f) { Alpha = 1.0f; bIsFocusAnimating = false; }
		float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
		FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;
		FQuat StartQuat = FocusStartRot.ToQuaternion();
		FQuat EndQuat = FocusEndRot.ToQuaternion();
		FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);
		Camera->SetWorldLocation(NewLoc);
		Camera->SetRelativeRotation(FRotator::FromQuaternion(BlendedQuat));
		TargetLocation = NewLoc; LastAppliedCameraLocation = NewLoc; bLastAppliedCameraLocationInitialized = true;
	}
	else { ApplySmoothedCameraLocation(DeltaTime); }
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::SyncCameraSmoothingTarget()
{
	if (!Camera) { bTargetLocationInitialized = false; bLastAppliedCameraLocationInitialized = false; return; }
	const FVector CurrentLocation = Camera->GetWorldLocation();
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized && FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;
	if (!bTargetLocationInitialized || bCameraMovedExternally) { TargetLocation = CurrentLocation; bTargetLocationInitialized = true; }
	LastAppliedCameraLocation = CurrentLocation; bLastAppliedCameraLocationInitialized = true;
}

void FEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	if (!Camera) return;
	const FVector CurrentLocation = Camera->GetWorldLocation();
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	Camera->SetWorldLocation(NewLocation);
	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FEditorViewportClient::TickEditorShortcuts()
{
	UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
	if (!EditorEngine)
	{
		return;
	}

	if (EditorEngine->IsPlayingInEditor() && FInputManager::Get().IsKeyPressed(VK_ESCAPE))
	{
		EditorEngine->RequestEndPlayMap();
	}

	const bool bAllowKeyboardInput = !FInputManager::Get().IsGuiUsingKeyboard() && !ImGui::GetIO().WantTextInput;
	if (!bAllowKeyboardInput)
	{
		return;
	}

	if (SelectionManager && FInputManager::Get().IsKeyPressed(VK_DELETE))
	{
		EditorEngine->BeginTrackedSceneChange();
		SelectionManager->DeleteSelectedActors();
		EditorEngine->CommitTrackedSceneChange();
		return;
	}

	if (!FInputManager::Get().IsKeyDown(VK_CONTROL) && FInputManager::Get().IsKeyPressed('X'))
	{
		EditorEngine->ToggleCoordSystem();
		return;
	}

	if (SelectionManager && FInputManager::Get().IsKeyPressed('F'))
	{
		AActor* Selected = SelectionManager->GetPrimarySelection();
		FocusActor(Selected);
	}

	if (SelectionManager && FInputManager::Get().IsKeyDown(VK_CONTROL) && FInputManager::Get().IsKeyPressed('D'))
	{
		const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
		if (!ToDuplicate.empty())
		{
			EditorEngine->BeginTrackedSceneChange();
			const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
			TArray<AActor*> NewSelection;
			int32 DuplicateIndex = 0;
			for (AActor* Src : ToDuplicate)
			{
				if (!Src) continue;
				AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
				if (Dup)
				{
					Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
					NewSelection.push_back(Dup);
					++DuplicateIndex;
				}
			}
			SelectionManager->ClearSelection();
			for (AActor* Actor : NewSelection)
			{
				SelectionManager->ToggleSelect(Actor);
			}
			if (EditorEngine->GetGizmo())
			{
				EditorEngine->GetGizmo()->UpdateGizmoTransform();
			}
			EditorEngine->CommitTrackedSceneChange();
		}
	}
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera) return;
	if (IsViewingFromLight()) return;
	FInputManager& Input = FInputManager::Get();
	EditorMoveAccumulator = FVector::ZeroVector;
	EditorRotateAccumulator = FVector::ZeroVector;
	EditorPanAccumulator = FVector::ZeroVector;
	EditorZoomAccumulator = 0.0f;
	bool bForceInput = bIsHovered || bIsActive || Input.IsMouseButtonDown(VK_RBUTTON);
	EnhancedInputManager.ProcessInput(&Input, DeltaTime, bForceInput);
	const FMinimalViewInfo& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;
	if (!bIsOrtho)
	{
		FVector DeltaMove = (Camera->GetForwardVector() * EditorMoveAccumulator.X + Camera->GetRightVector() * EditorMoveAccumulator.Y) * (CameraSpeed * DeltaTime);
		DeltaMove.Z += EditorMoveAccumulator.Z * (CameraSpeed * DeltaTime);
		TargetLocation += DeltaMove;
		if (!EditorPanAccumulator.IsNearlyZero())
		{
			FVector PanDelta = (Camera->GetRightVector() * (-EditorPanAccumulator.X * PanMouseScale * 0.15f)) + (Camera->GetUpVector() * (EditorPanAccumulator.Y * PanMouseScale * 0.15f));
			TargetLocation += PanDelta;
		}
		if (!EditorRotateAccumulator.IsNearlyZero())
		{
			const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
			const float MouseRotationSpeed = 0.15f * RotateSensitivity;
			const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
			float Yaw = 0.0f, Pitch = 0.0f;
			if (!Input.IsMouseButtonDown(VK_RBUTTON)) { Yaw = EditorRotateAccumulator.X * AngleVelocity * DeltaTime; Pitch = EditorRotateAccumulator.Y * AngleVelocity * DeltaTime; }
			else { Yaw = EditorRotateAccumulator.X * MouseRotationSpeed; Pitch = EditorRotateAccumulator.Y * MouseRotationSpeed; }
			Camera->Rotate(Yaw, Pitch);
		}
	}
	else
	{
		if (!EditorRotateAccumulator.IsNearlyZero() && Input.IsMouseButtonDown(VK_RBUTTON))
		{
			float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
			Camera->MoveLocal(FVector(0, -EditorRotateAccumulator.Y * PanScale, EditorRotateAccumulator.Z * PanScale));
		}
	}
}

// Find closest vertex in the world to a ray
static FVector FindClosestVertex(UWorld* World, const FRay& Ray, float MaxDistance = 10.0f)
{
	if (!World) return FVector::ZeroVector;
	FVector BestVertex = FVector::ZeroVector;
	float BestDistSq = MaxDistance * MaxDistance;
	bool bFound = false;
	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>()) continue;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (UMeshComponent* MeshComp = Comp->IsA<UMeshComponent>() ? static_cast<UMeshComponent*>(Comp) : nullptr)
			{
				FMeshDataView View = MeshComp->GetMeshDataView();
				if (!View.IsValid()) continue;
				FMatrix WorldMat = MeshComp->GetWorldMatrix();
				for (uint32 i = 0; i < View.VertexCount; ++i)
				{
					FVector LocalPos = View.GetPosition(i);
					FVector WorldPos = WorldMat.TransformPositionWithW(LocalPos); // Use TransformPositionWithW
					FVector W = WorldPos - Ray.Origin;
					float Proj = W.Dot(Ray.Direction);
					FVector ClosestP = Ray.Origin + Ray.Direction * (std::max)(0.0f, Proj);
					float DistSq = (WorldPos - ClosestP).Dot(WorldPos - ClosestP);
					if (DistSq < BestDistSq) { BestDistSq = DistSq; BestVertex = WorldPos; bFound = true; }
				}
			}
		}
	}
	return bFound ? BestVertex : FVector::ZeroVector;
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime; if (!Camera || !Gizmo || !GetWorld()) return;
	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	uint32 CursorViewportX = 0;
	uint32 CursorViewportY = 0;
	const bool bCursorInViewport = GetCursorViewportPosition(CursorViewportX, CursorViewportY);
	if (FInputManager::Get().IsGuiUsingMouse() && !bCursorInViewport && !Gizmo->IsHolding() && !bIsMarqueeSelecting)
	{
		return;
	}

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;
	if (std::abs(EditorZoomAccumulator) > 1e-6f)
	{
		if (Camera->IsOrthogonal()) { float NewWidth = Camera->GetOrthoWidth() - EditorZoomAccumulator * ZoomSpeed * DeltaTime; Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f)); }
		else { TargetLocation += Camera->GetForwardVector() * (EditorZoomAccumulator * ZoomSpeed * 0.015f); }
	}
	FInputManager& Input = FInputManager::Get();
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	HoveredUIScreenGizmoAxis = HasUIScreenTranslateGizmo() ? HitTestUIScreenTranslateGizmo(MousePos) : 0;
	float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	float LocalMouseX = 0.0f;
	float LocalMouseY = 0.0f;
	if (!TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, LocalMouseX, LocalMouseY))
	{
		LocalMouseX = MousePos.x - ViewportScreenRect.X;
		LocalMouseY = MousePos.y - ViewportScreenRect.Y;
	}
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FRayHitResult HitResult;
	bool bGizmoHit = FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);
	if (Input.IsKeyPressed(FInputManager::MOUSE_LEFT) && bIsHovered)
	{
		if (Input.IsKeyDown(VK_CONTROL) && Input.IsKeyDown(VK_MENU)) { bIsMarqueeSelecting = true; MarqueeStartPos = FVector(MousePos.x, MousePos.y, 0); MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0); }
		else 
		{ 
			if (Input.IsKeyDown(VK_MENU) && bGizmoHit && SelectionManager && !SelectionManager->IsEmpty())
			{
				const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
				TArray<AActor*> NewSelection;
				for (AActor* Src : ToDuplicate) { if (AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr))) NewSelection.push_back(Dup); }
				SelectionManager->ClearSelection();
				for (AActor* Actor : NewSelection) SelectionManager->ToggleSelect(Actor);
			}
			HandleDragStart(Ray); 
		}
	}
	else if (Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
	{
		if (bIsMarqueeSelecting) { MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0); }
		else if (bDraggingUIScreenGizmo)
		{
			UpdateUIScreenTranslateDrag(MousePos);
		}
		else 
		{ 
			if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding()) Gizmo->SetHolding(true); 
			if (Gizmo->IsHolding()) 
			{
				if (Input.IsKeyDown('V')) { FVector BestV = FindClosestVertex(GetWorld(), Ray, 2.0f); if (!BestV.IsNearlyZero()) { Gizmo->SetTargetLocation(BestV); TargetLocation = BestV; } }
				Gizmo->UpdateDrag(Ray); 
			}
		}
	}
	else if (Input.IsKeyReleased(FInputManager::MOUSE_LEFT))
	{
		if (bDraggingUIScreenGizmo)
		{
			EndUIScreenTranslateDrag(true);
		}
		else if (bIsMarqueeSelecting)
		{
			bIsMarqueeSelecting = false;
			float MinX = (std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X); float MaxX = (std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X);
			float MinY = (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y); float MaxY = (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
			if (std::abs(MaxX - MinX) > 2.0f || std::abs(MaxY - MinY) > 2.0f)
			{
				UWorld* World = GetWorld();
				if (World && SelectionManager)
				{
					if (!Input.IsKeyDown(VK_CONTROL)) SelectionManager->ClearSelection();
					FMatrix VP = Camera->GetViewProjectionMatrix();
					for (AActor* Actor : World->GetActors())
					{
						if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>()) continue;
						FVector WorldPos = Actor->GetActorLocation(); FVector ClipSpace = VP.TransformPositionWithW(WorldPos);
						float ScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X; float ScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;
						if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY) SelectionManager->ToggleSelect(Actor);
					}
				}
			}
		}
		else
		{
			Gizmo->DragEnd();
			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			{
				EditorEngine->CommitTrackedTransformChange();
			}
		}
	}
	else if (Input.IsKeyReleased(VK_LBUTTON))
	{
		if (bDraggingUIScreenGizmo)
		{
			EndUIScreenTranslateDrag(true);
		}
		Gizmo->SetPressedOnHandle(false);
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->CommitTrackedTransformChange();
		}
		bIsMarqueeSelecting = false;
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FInputManager& Input = FInputManager::Get(); if (!bIsHovered) return;
	if (BeginUIScreenTranslateDrag(ImGui::GetIO().MousePos))
	{
		return;
	}
	FScopeCycleCounter PickCounter; FRayHitResult HitResult{};
	if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult))
	{
		if (SelectionManager)
		{
			for (AActor* Actor : SelectionManager->GetSelectedActors())
			{
				if (Actor && Actor->IsActorMovementLocked())
				{
					Gizmo->SetPressedOnHandle(false);
					return;
				}
			}
		}
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->BeginTrackedTransformChange();
		}
		Gizmo->SetPressedOnHandle(true);
	}
	else
	{
		uint32 CursorX = 0;
		uint32 CursorY = 0;
		USceneComponent* HitUIComponent = GetCursorViewportPosition(CursorX, CursorY)
			? FindTopmostUIComponentAt(GetWorld(), static_cast<float>(CursorX), static_cast<float>(CursorY))
			: nullptr;
		if (HitUIComponent)
		{
			AActor* HitActor = HitUIComponent->GetOwner();
			const bool bCtrlHeld = Input.IsKeyDown(VK_CONTROL);
			if (HitActor)
			{
				if (bCtrlHeld)
				{
					const bool bWasSelected = SelectionManager->IsSelected(HitActor);
					SelectionManager->ToggleSelect(HitActor);
					if (!bWasSelected)
					{
						SelectionManager->SelectComponent(HitUIComponent);
					}
				}
				else
				{
					if (SelectionManager->GetPrimarySelection() != HitActor)
					{
						SelectionManager->Select(HitActor);
					}
					SelectionManager->SelectComponent(HitUIComponent);
				}
			}
		}
		else
		{
			AActor* BestActor = nullptr;
			if (UWorld* W = GetWorld())
			{
				W->RaycastPrimitives(Ray, HitResult, BestActor);
				if (!BestActor)
				{
					EditorRaycastAllVisiblePrimitives(W, Ray, HitResult, BestActor);
				}
				if (!BestActor && Camera)
				{
					const ImVec2 MousePos = ImGui::GetIO().MousePos;
					float LocalMouseX = 0.0f;
					float LocalMouseY = 0.0f;
					if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, LocalMouseX, LocalMouseY))
					{
						UPrimitiveComponent* ScreenHitPrimitive = nullptr;
						const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
						const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
						BestActor = FindScreenSpacePrimitiveAt(W, Camera, LocalMouseX, LocalMouseY, VPWidth, VPHeight, ScreenHitPrimitive);
						if (ScreenHitPrimitive)
						{
							HitResult.HitComponent = ScreenHitPrimitive;
						}
					}
				}
			}
			bool bCtrlHeld = Input.IsKeyDown(VK_CONTROL);
			if (BestActor == nullptr) { if (!bCtrlHeld) SelectionManager->ClearSelection(); }
			else
			{
				if (bCtrlHeld) SelectionManager->ToggleSelect(BestActor);
				else { if (SelectionManager->GetPrimarySelection() == BestActor) { if (HitResult.HitComponent) SelectionManager->SelectComponent(HitResult.HitComponent); } else SelectionManager->Select(BestActor); }
			}
		}
	}
	if (OverlayStatSystem) { const uint64 PickCycles = PickCounter.Finish(); const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles); OverlayStatSystem->RecordPickingAttempt(ElapsedMs); }
}

bool FEditorViewportClient::HasUIScreenTranslateGizmo() const
{
	if (!SelectionManager || !Gizmo || Gizmo->GetMode() != EGizmoMode::Translate)
	{
		return false;
	}

	if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			return false;
		}
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	return SelectedComponent && IsUIScreenTransformableComponent(SelectedComponent);
}

int32 FEditorViewportClient::HitTestUIScreenTranslateGizmo(const ImVec2& MousePos) const
{
	if (!HasUIScreenTranslateGizmo())
	{
		return static_cast<int32>(EUIScreenGizmoAxis::None);
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
	{
		return static_cast<int32>(EUIScreenGizmoAxis::None);
	}

	float CenterX = X + Width * 0.5f;
	float CenterY = Y + Height * 0.5f;
	if (!TryConvertViewportPixelToScreenPoint(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, CenterX, CenterY))
	{
		CenterX += ViewportScreenRect.X;
		CenterY += ViewportScreenRect.Y;
	}
	const ImVec2 Local(MousePos.x - CenterX, MousePos.y - CenterY);
	const float CenterHalf = 8.0f;
	const float AxisThickness = 6.0f;
	const float AxisLength = 48.0f;

	if (std::abs(Local.x) <= CenterHalf && std::abs(Local.y) <= CenterHalf)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::XY);
	}

	if (Local.x >= CenterHalf && Local.x <= AxisLength && std::abs(Local.y) <= AxisThickness)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::X);
	}

	if (Local.y >= CenterHalf && Local.y <= AxisLength && std::abs(Local.x) <= AxisThickness)
	{
		return static_cast<int32>(EUIScreenGizmoAxis::Y);
	}

	return static_cast<int32>(EUIScreenGizmoAxis::None);
}

bool FEditorViewportClient::BeginUIScreenTranslateDrag(const ImVec2& MousePos)
{
	if (!HasUIScreenTranslateGizmo())
	{
		return false;
	}

	const int32 HitAxis = HitTestUIScreenTranslateGizmo(MousePos);
	if (HitAxis == static_cast<int32>(EUIScreenGizmoAxis::None))
	{
		return false;
	}

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		EditorEngine->BeginTrackedTransformChange();
	}

	ActiveUIScreenGizmoAxis = HitAxis;
	float ViewportMouseX = 0.0f;
	float ViewportMouseY = 0.0f;
	if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportMouseX, ViewportMouseY))
	{
		LastUIScreenGizmoMousePos = ImVec2(ViewportMouseX, ViewportMouseY);
	}
	else
	{
		LastUIScreenGizmoMousePos = MousePos;
	}
	bDraggingUIScreenGizmo = true;
	return true;
}

void FEditorViewportClient::UpdateUIScreenTranslateDrag(const ImVec2& MousePos)
{
	if (!bDraggingUIScreenGizmo || !SelectionManager)
	{
		return;
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!IsUIScreenTransformableComponent(SelectedComponent))
	{
		return;
	}

	FVector ScreenPosition;
	if (!GetUIScreenComponentPosition(SelectedComponent, ScreenPosition))
	{
		return;
	}

	float ViewportMouseX = 0.0f;
	float ViewportMouseY = 0.0f;
	ImVec2 CurrentMouseInViewport = MousePos;
	if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportMouseX, ViewportMouseY))
	{
		CurrentMouseInViewport = ImVec2(ViewportMouseX, ViewportMouseY);
	}

	const ImVec2 Delta(CurrentMouseInViewport.x - LastUIScreenGizmoMousePos.x, CurrentMouseInViewport.y - LastUIScreenGizmoMousePos.y);
	switch (static_cast<EUIScreenGizmoAxis>(ActiveUIScreenGizmoAxis))
	{
	case EUIScreenGizmoAxis::X:
		ScreenPosition.X += Delta.x;
		break;
	case EUIScreenGizmoAxis::Y:
		ScreenPosition.Y += Delta.y;
		break;
	case EUIScreenGizmoAxis::XY:
		ScreenPosition.X += Delta.x;
		ScreenPosition.Y += Delta.y;
		break;
	default:
		return;
	}

	SetUIScreenComponentPosition(SelectedComponent, ScreenPosition);
	LastUIScreenGizmoMousePos = CurrentMouseInViewport;
}

void FEditorViewportClient::EndUIScreenTranslateDrag(bool bCommitChange)
{
	if (!bDraggingUIScreenGizmo)
	{
		return;
	}

	bDraggingUIScreenGizmo = false;
	ActiveUIScreenGizmoAxis = static_cast<int32>(EUIScreenGizmoAxis::None);
	if (bCommitChange)
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			EditorEngine->CommitTrackedTransformChange();
		}
	}
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;
	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;

	if (!Viewport)
	{
		return;
	}

	if (FProjectSettings::Get().Game.bLockWindowResolution)
	{
		const uint32 TargetW = (std::max)(320u, FProjectSettings::Get().Game.WindowWidth);
		const uint32 TargetH = (std::max)(240u, FProjectSettings::Get().Game.WindowHeight);
		if (Camera)
		{
			Camera->OnResize(static_cast<int32>(TargetW), static_cast<int32>(TargetH));
		}
		if (Viewport->GetWidth() != TargetW || Viewport->GetHeight() != TargetH)
		{
			Viewport->RequestResize(TargetW, TargetH);
		}

		if (R.Width > 0.0f && R.Height > 0.0f)
		{
			const float Scale = (std::min)(R.Width / static_cast<float>(TargetW), R.Height / static_cast<float>(TargetH));
			const float DrawW = static_cast<float>(TargetW) * Scale;
			const float DrawH = static_cast<float>(TargetH) * Scale;
			ViewportScreenRect.X = R.X + (R.Width - DrawW) * 0.5f;
			ViewportScreenRect.Y = R.Y + (R.Height - DrawH) * 0.5f;
			ViewportScreenRect.Width = DrawW;
			ViewportScreenRect.Height = DrawH;
		}
		SyncPIEViewportRect(Viewport, ViewportScreenRect);
		return;
	}

	uint32 SlotW = static_cast<uint32>(R.Width);
	uint32 SlotH = static_cast<uint32>(R.Height);
	if (Camera && SlotW > 0 && SlotH > 0)
	{
		Camera->OnResize(static_cast<int32>(SlotW), static_cast<int32>(SlotH));
	}
	if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
	{
		Viewport->RequestResize(SlotW, SlotH);
	}
	SyncPIEViewportRect(Viewport, ViewportScreenRect);
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV()) return;
	const FRect& R = ViewportScreenRect; if (R.Width <= 0 || R.Height <= 0) return;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y); ImVec2 Max(R.X + R.Width, R.Y + R.Height);
	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
	if (bIsActiveViewport)
	{
		constexpr float ActiveBorderThickness = 4.0f;
		const float BorderInset = ActiveBorderThickness * 0.5f;
		ImU32 BorderColor = IM_COL32(255, 165, 0, 220);
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->IsPlayingInEditor())
			{
				BorderColor = EditorEngine->IsGamePaused()
					? EditorAccentColor::ToU32()
					: IM_COL32(52, 199, 89, 255);
			}
		}
		if (R.Width > ActiveBorderThickness && R.Height > ActiveBorderThickness)
		{
			DrawList->AddRect(
				ImVec2(Min.x + BorderInset, Min.y + BorderInset),
				ImVec2(Max.x - BorderInset, Max.y - BorderInset),
				BorderColor,
				0.0f,
				0,
				ActiveBorderThickness);
		}
	}
	if (bIsMarqueeSelecting)
	{
		ImDrawList* ForegroundDrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		ImVec2 RectMin((std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ImVec2 RectMax((std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ForegroundDrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 5.0f);
	}

	DrawUIScreenTranslateGizmo();
}

void FEditorViewportClient::DrawUIScreenTranslateGizmo()
{
	if (!HasUIScreenTranslateGizmo())
	{
		return;
	}

	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
	{
		return;
	}

	float CenterX = X + Width * 0.5f;
	float CenterY = Y + Height * 0.5f;
	if (!TryConvertViewportPixelToScreenPoint(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, CenterX, CenterY))
	{
		CenterX += ViewportScreenRect.X;
		CenterY += ViewportScreenRect.Y;
	}
	const float AxisLength = 48.0f;
	const float CenterHalf = 8.0f;
	const float Thickness = 3.0f;
	const bool bHoverX = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
	const bool bHoverY = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
	const bool bHoverXY = HoveredUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
	const bool bActiveX = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
	const bool bActiveY = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
	const bool bActiveXY = ActiveUIScreenGizmoAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
	const ImU32 XColor = (bHoverX || bActiveX) ? IM_COL32(255, 120, 120, 255) : IM_COL32(230, 70, 70, 255);
	const ImU32 YColor = (bHoverY || bActiveY) ? IM_COL32(120, 255, 160, 255) : IM_COL32(70, 210, 110, 255);
	const ImU32 CenterColor = (bHoverXY || bActiveXY) ? IM_COL32(120, 190, 255, 255) : IM_COL32(26, 138, 245, 255);

	ImDrawList* DrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	const ImVec2 Center(CenterX, CenterY);
	DrawList->AddLine(Center, ImVec2(CenterX + AxisLength, CenterY), XColor, Thickness);
	DrawList->AddTriangleFilled(ImVec2(CenterX + AxisLength + 8.0f, CenterY), ImVec2(CenterX + AxisLength - 4.0f, CenterY - 6.0f), ImVec2(CenterX + AxisLength - 4.0f, CenterY + 6.0f), XColor);
	DrawList->AddLine(Center, ImVec2(CenterX, CenterY + AxisLength), YColor, Thickness);
	DrawList->AddTriangleFilled(ImVec2(CenterX, CenterY + AxisLength + 8.0f), ImVec2(CenterX - 6.0f, CenterY + AxisLength - 4.0f), ImVec2(CenterX + 6.0f, CenterY + AxisLength - 4.0f), YColor);
	DrawList->AddRectFilled(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf), ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), CenterColor, 2.0f);
	DrawList->AddRect(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf), ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32& OutX, uint32& OutY) const
{
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	if (!bIsHovered && !bIsActive)
	{
		return false;
	}

	float ViewportX = 0.0f;
	float ViewportY = 0.0f;
	if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportX, ViewportY))
	{
		OutX = static_cast<uint32>(ViewportX);
		OutY = static_cast<uint32>(ViewportY);
		return true;
	}

	return false;
}

