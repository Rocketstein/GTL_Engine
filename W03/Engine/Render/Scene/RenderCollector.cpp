#include "RenderCollector.h"
#include "Engine/Scene/Camera.h"
#include "World.h"
#include "World/Gizmo/GizmoComponent.h"
#include "Classes/ASpotlight.h"

FMeshBufferManager FRenderCollector::MeshBufferManager;

void FRenderCollector::Collect(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	if (!Context.Camera || !Context.Scene)
	{
		return;
	}

	//	Must be the active camera
	FMatrix View = Context.Camera->GetViewMatrix();
	FMatrix Projection = Context.Camera->GetProjectionMatrix();

	//	Draw from Editor (Gizmo, Axis, etc.)
	CollectFromEditor(Context, View, Projection, RenderBus);

	//	Draw from World
	//	Iterate through GUObjects, not context.scene. This is for extra encapsulation
	for (const auto& ObjPtr : GUObjectArray)
	{
		auto* Object = ObjPtr;
		if (Object && Object->IsA<AActor>() && !Object->bPendingKill)
		{
			auto* Actor = Object->Cast<AActor>();
			if (Actor->GetOwnerUUID() == Context.Scene->UUID)
			{
				CollectFromActor(Actor, Context, RenderBus);
			}
		}
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	// Iterate through the components of the actor and retrieve their render properties
	for (auto* Comp : Actor->GetComponents())
	{
		if (!Comp || Comp->bPendingKill) continue;
		if (!Comp->IsA<UPrimitiveComponent>()) continue;

		UPrimitiveComponent* Primitive = static_cast<UPrimitiveComponent*>(Comp);
		CollectFromComponent(Primitive, Context, RenderBus);
	}

	if (Actor->IsA<ASpotlight>()) {
		ASpotlight::Cast(Actor)->RenderLines(RenderBus.GetBatchedLine());
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Primitive;
	Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());	// ?
	Cmd.TransformConstants = FTransformConstants{ primitiveComponent->GetWorldMatrix(), Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix() };

	FRenderCommand FontCmd = {};
	FontCmd.Type = ERenderCommandType::Font;
	FontCmd.TextConstants.OrientationType = EOrientationType::Billboard;
	FontCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Quad);
	FontCmd.TextConstants.Text = L"윢윩앏있띻\nUUID:" + std::to_wstring(primitiveComponent->UUID);

	float TopZ = primitiveComponent->GetBoundingBox().WorldAABBMaxLocation.Z;
	FontCmd.TextConstants.TextPosition = primitiveComponent->GetBoundingBox().WorldAABBMaxLocation * 0.5f	// AABB 평균: 오브젝트 중앙
		+ primitiveComponent->GetBoundingBox().WorldAABBMinLocation * 0.5f;
	FontCmd.TextConstants.TextPosition.Z = TopZ + 1.0f;

	FontCmd.TextConstants.TextScale = { 1.0f, 0.8f, 0.7f };
	FontCmd.TextConstants.TextColor = FVector4(1, 1, 1, 1);
	RenderBus.AddTextCommand(FontCmd);

	if (primitiveComponent->GetRenderCommand(Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix(), Cmd))
	{
		if (Cmd.Type != ERenderCommandType::Primitive && Cmd.Type != ERenderCommandType::TextPrimitive)//여기 SubUV{
		{
			Cmd.UVMeshBuffer = &MeshBufferManager.GetUVMeshBuffer(primitiveComponent->GetPrimitiveType());
			RenderBus.AddSubUVCompoenetCommand(Cmd);
		}
		else {
			RenderBus.AddComponentCommand(Cmd);
		}

		FBatchedLine* BatchLine = RenderBus.GetBatchedLine();

		for (uint64 i = 0; i < (Context.SelectedComponent)->size(); ++i)
		{
			if (primitiveComponent == (*Context.SelectedComponent)[i])
			{
				FBoundingBox AABB = primitiveComponent->GetBoundingBox();
				BatchLine->AddAABB(AABB.WorldAABBMinLocation, AABB.WorldAABBMaxLocation);

				FRenderCommand OutlineCmd = Cmd;
				OutlineCmd.Type = ERenderCommandType::SelectionOutline;
				OutlineCmd.OutlineConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
				OutlineCmd.OutlineConstants.OutlineInvScale = FVector(1.0f / primitiveComponent->GetRelativeScale().X,
					1.0f / primitiveComponent->GetRelativeScale().Y, 1.0f / primitiveComponent->GetRelativeScale().Z);
				OutlineCmd.OutlineConstants.OutlineOffset = 0.03;

				if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Plane)
				{
					OutlineCmd.OutlineConstants.PrimitiveType = 0u;
				}
				else
				{
					//	Plane은 Outline이 제대로 안나오는 이슈가 있어서, 일단 Cube로 대체하여 그립니다.
					OutlineCmd.OutlineConstants.PrimitiveType = 1u;
				}

				RenderBus.AddOutlineCommand(OutlineCmd);
			}
		}

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextComponent* TextComponent = static_cast<UTextComponent*>(primitiveComponent);
			FRenderCommand TextCmd = Cmd;
			TextCmd.Type = ERenderCommandType::TextPrimitive;
			TextCmd.TextConstants.OrientationType = EOrientationType::Fixed;
			TextCmd.TextConstants.Alignment = TextComponent->GetAlignment();
			TextCmd.TextConstants.Text = TextComponent->GetText();
			TextCmd.TextConstants.TextPosition = primitiveComponent->GetWorldLocation();
			TextCmd.TextConstants.TextRotation = primitiveComponent->GetRelativeRotation();
			TextCmd.TextConstants.TextScale = primitiveComponent->GetScaleVector() * TextComponent->GetTextScale();
			TextCmd.TextConstants.TextColor = TextComponent->GetColor();
			RenderBus.AddTextCommand(TextCmd);
		}
	}
}

void FRenderCollector::CollectFromEditor(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	//	Gizmo
	UGizmoComponent* Gizmo = Context.Gizmo;

	RenderBus.SetCachedView(ViewMat);
	RenderBus.SetCachedProjection(ProjMat);

	FBatchedLine* BatchLine = RenderBus.GetBatchedLine();
	BatchLine->AddGrid(Context.GridSize);

	FRenderCommand BatchedLineCommand = {};

	FVector CamWorldPos = Context.Camera->GetWorldLocation();
	BatchedLineCommand.Type = ERenderCommandType::BatchedLine;
	BatchedLineCommand.MeshBuffer = BatchLine->GetBatchedLineBuffer();

	BatchedLineCommand.BatchedLineConstants = FBatchedLineConstants{
		FMatrix::Identity, ViewMat, ProjMat,
		FVector4(CamWorldPos.X, CamWorldPos.Y, CamWorldPos.Z, 1.f)
	};

	RenderBus.AddBatcedLineCommand(BatchedLineCommand);


	if (Gizmo && Gizmo->IsVisible())
	{
		FRenderCommand Cmd1 = {};
		Cmd1.Type = ERenderCommandType::Gizmo;
		Cmd1.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
		Cmd1.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), ViewMat, ProjMat };
		Cmd1.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd1.GizmoConstants.bIsInnerGizmo = 1;
		Cmd1.GizmoConstants.bClicking = Gizmo->IsHolding() ? 1 : 0;
		Cmd1.GizmoConstants.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? static_cast<uint32>(Gizmo->GetSelectedAxis()) : 0xffffffffu;
		Cmd1.GizmoConstants.HoveredAxisOpacity = 0.55f;

		RenderBus.AddDepthLessCommand(Cmd1);

		//	선택되지 않은 경우에 Outer를 그림
		if (!Gizmo->IsHolding())
		{
			FRenderCommand Cmd2 = {};
			Cmd2.Type = ERenderCommandType::Gizmo;
			Cmd2.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
			Cmd2.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), ViewMat, ProjMat };
			Cmd2.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			Cmd2.GizmoConstants.bIsInnerGizmo = 0;
			Cmd2.GizmoConstants.bClicking = Gizmo->IsHolding() ? 1 : 0;
			Cmd2.GizmoConstants.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? static_cast<uint32>(Gizmo->GetSelectedAxis()) : 0xffffffffu;
			Cmd2.GizmoConstants.HoveredAxisOpacity = 0.55f;

			RenderBus.AddDepthLessCommand(Cmd2);
		}
	}

	//	Cursor Overlay (null checking +)
	if (Context.CursorOverlayState && Context.CursorOverlayState->bVisible)
	{
		FRenderCommand OverlayCmd = {};
		OverlayCmd.Type = ERenderCommandType::Overlay;
		OverlayCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_MouseOverlay);

		OverlayCmd.OverlayConstants.CenterScreen.X = Context.CursorOverlayState->ScreenX;
		OverlayCmd.OverlayConstants.CenterScreen.Y = Context.CursorOverlayState->ScreenY;
		OverlayCmd.OverlayConstants.ViewportSize.X = static_cast<float>(Context.ViewportWidth);
		OverlayCmd.OverlayConstants.ViewportSize.Y = static_cast<float>(Context.ViewportHeight);
		OverlayCmd.OverlayConstants.Radius = Context.CursorOverlayState->CurrentRadius;
		OverlayCmd.OverlayConstants.Color = Context.CursorOverlayState->Color;

		RenderBus.AddOverlayCommand(OverlayCmd);
	}
}
