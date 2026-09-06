#include "Editor/UI/EditorSkeletalMeshViewerPanel.h"

#include "Core/CoreGlobals.h"
#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/SkeletalMeshViewer.h"
#include "Mesh/Skeleton.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/SkeletalMeshActor.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <commdlg.h>
#include <filesystem>
#include <string>

namespace
{
FQuat ExtractRotationNoScale(const FMatrix& Matrix)
{
    FMatrix RotationMatrix = Matrix;

    for (int32 Row = 0; Row < 3; ++Row)
    {
        FVector Axis(
            RotationMatrix.M[Row][0],
            RotationMatrix.M[Row][1],
            RotationMatrix.M[Row][2]);

        if (Axis.LengthSquared() > FMath::Epsilon * FMath::Epsilon)
        {
            Axis.Normalize();
            RotationMatrix.M[Row][0] = Axis.X;
            RotationMatrix.M[Row][1] = Axis.Y;
            RotationMatrix.M[Row][2] = Axis.Z;
        }
    }

    RotationMatrix.M[0][3] = 0.0f;
    RotationMatrix.M[1][3] = 0.0f;
    RotationMatrix.M[2][3] = 0.0f;
    RotationMatrix.M[3][0] = 0.0f;
    RotationMatrix.M[3][1] = 0.0f;
    RotationMatrix.M[3][2] = 0.0f;
    RotationMatrix.M[3][3] = 1.0f;

    return RotationMatrix.ToQuat();
}

FTransform MakeTransformFromMatrixForEditCache(const FMatrix& Matrix)
{
    return FTransform(
        Matrix.GetLocation(),
        ExtractRotationNoScale(Matrix),
        Matrix.GetScale());
}
}

void FEditorSkeletalMeshViewerPanel::Release()
{
    Owner = nullptr;
    BoneLocalTransforms.clear();
}

void FEditorSkeletalMeshViewerPanel::Render(float DeltaTime)
{
    if (!Owner) return;

    bool bOpen = Owner->IsOpen();
    const std::string WindowName = "SkeletalMesh Viewer##" + std::to_string(Owner->GetViewerId());
    if (!ImGui::Begin(WindowName.c_str(), &bOpen))
    {
        Owner->SetOpen(bOpen);
        ImGui::End();
        return;
    }
    Owner->SetOpen(bOpen);

    RenderToolbar();

    ImGui::Separator();

    const float RightPanelWidth = 320.0f;
    ImVec2 Available = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("PreviewViewport", ImVec2(Available.x - RightPanelWidth, Available.y), true);
    RenderPreviewViewport(DeltaTime);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(RightPanelWidth, Available.y), true, ImGuiWindowFlags_HorizontalScrollbar);
    RenderBoneHierarchyTree();
    ImGui::Separator();
    RenderInspector();
    ImGui::EndChild();

    ImGui::End();
}

void FEditorSkeletalMeshViewerPanel::RenderToolbar()
{
	FSkeletalMeshViewerState& State = Owner->GetState();

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

	const auto RenderToggleButton = [](const char* Label, bool& bValue) -> bool
	{
		const ImVec4 ActiveColor(0.18f, 0.42f, 0.78f, 1.0f);
		const ImVec4 ActiveHoveredColor(0.24f, 0.50f, 0.88f, 1.0f);
		const ImVec4 InactiveColor(0.18f, 0.18f, 0.20f, 1.0f);
		const ImVec4 InactiveHoveredColor(0.26f, 0.26f, 0.29f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_Button, bValue ? ActiveColor : InactiveColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bValue ? ActiveHoveredColor : InactiveHoveredColor);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ActiveColor);

		const bool bClicked = ImGui::Button(Label);
		if (bClicked)
		{
			bValue = !bValue;
		}

		ImGui::PopStyleColor(3);
		return bClicked;
	};

	ImGui::TextUnformatted("View");
	ImGui::SameLine();

	RenderToggleButton("Mesh", State.bShowMesh);
	ImGui::SameLine();

	RenderToggleButton("Skeleton", State.bShowSkeleton);
    ImGui::SameLine();

    ImGui::Text("ViewMode");
    ImGui::SameLine();
	auto OpenToolbarPopup =
        [&](const char* ButtonLabel, const char* PopupId, const std::function<void()>& Body)
    {
        ImGui::SameLine();

        if (ImGui::Button(ButtonLabel))
        {
            ImGui::OpenPopup(PopupId);
        }

        if (ImGui::BeginPopup(PopupId))
        {
            Body();
            ImGui::EndPopup();
        }
    };

    OpenToolbarPopup("ViewMode", "ViewModePopup", [&]()
                     {
        int32 CurrentMode = static_cast<int32>(State.ViewMode);

        ImGui::RadioButton("Lit_Lambert", &CurrentMode, static_cast<int32>(EViewMode::Lit_Lambert));
        ImGui::RadioButton("Lit_Phong", &CurrentMode, static_cast<int32>(EViewMode::Lit_Phong));
        ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
        ImGui::RadioButton("WorldNormal", &CurrentMode, static_cast<int32>(EViewMode::WorldNormal));
        ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
        ImGui::RadioButton("SceneDepth", &CurrentMode, static_cast<int32>(EViewMode::SceneDepth));

        State.ViewMode = static_cast<EViewMode>(CurrentMode);
	 });

	ImGui::TextUnformatted("Gizmo");
    ImGui::SameLine();
    if (UGizmoComponent* Gizmo = Owner->GetBoneGizmo())
    {
        const EGizmoMode Mode = Gizmo->GetMode();
        if (ImGui::Button(Mode == EGizmoMode::Translate ? "[Move]" : "Move"))
        {
            Gizmo->SetTranslateMode();
        }
        ImGui::SameLine();
        if (ImGui::Button(Mode == EGizmoMode::Rotate ? "[Rotate]" : "Rotate"))
        {
            Gizmo->SetRotateMode();
        }
        ImGui::SameLine();
        if (ImGui::Button(Mode == EGizmoMode::Scale ? "[Scale]" : "Scale"))
        {
            Gizmo->SetScaleMode();
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("Space");
        ImGui::SameLine();
        int32 CurrentSpace = Gizmo->IsWorldSpace() ? 1 : 0;
        if (ImGui::Button(CurrentSpace == 1 ? "[World]" : "World"))
        {
            Gizmo->SetWorldSpace(true);
        }
        ImGui::SameLine();
        if (ImGui::Button(CurrentSpace == 0 ? "[Local]" : "Local"))
        {
            Gizmo->SetWorldSpace(false);
        }
    }

	USkeletalMesh* ActiveMesh = State.ActiveMesh;
	const bool bCanSpawn = ActiveMesh != nullptr && EditorEngine && !EditorEngine->IsPlayingInEditor();

	ImGui::BeginDisabled(!bCanSpawn);
	if (ImGui::Button("Spawn in Scene"))
	{
		UWorld* EditorWorld = nullptr;
		for (FWorldContext& WorldContext : EditorEngine->GetWorldList())
		{
			if (WorldContext.WorldType == EWorldType::Editor)
			{
				EditorWorld = WorldContext.World;
				break;
			}
		}

		if (EditorWorld)
		{
			ASkeletalMeshActor* Actor = EditorWorld->SpawnActor<ASkeletalMeshActor>();
			Actor->InitDefaultComponents();

			USkeletalMeshComponent* NewComp = Actor->GetSkeletalMeshComponent();
			NewComp->SetSkeletalMesh(ActiveMesh);

			USkeletalMeshComponent* PreviewComp = GetPreviewMeshComponent();
			if (PreviewComp)
			{
				const int32 NumBones = PreviewComp->GetNumBones();
				for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
				{
					NewComp->SetBoneLocalMatrix(BoneIndex, PreviewComp->GetBoneLocalMatrix(BoneIndex));
				}
				NewComp->RefreshEditedDisplayPose();
			}

			Actor->SetActorLocation(FVector::ZeroVector);
			EditorWorld->InsertActorToOctree(Actor);
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	if (RenderToggleButton("FBX Local Bones", State.bUseFbxLocalSkeleton))
	{
        if (USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent())
        {
            MeshComp->SetSkeletalDebugPoseMode(
                State.bUseFbxLocalSkeleton
                    ? ESkeletalDebugPoseMode::FbxLocalPose
                    : ESkeletalDebugPoseMode::SkinBindPose);
        }
	}

	const ImGuiStyle& Style = ImGui::GetStyle();
	const float SaveWidth = ImGui::CalcTextSize("Save").x + Style.FramePadding.x * 2.0f;
	const float ResetWidth = ImGui::CalcTextSize("Reset Pose").x + Style.FramePadding.x * 2.0f;
	const float ActionWidth = SaveWidth + ResetWidth + Style.ItemSpacing.x;
	const float ActionStartX = ImGui::GetWindowContentRegionMax().x - ActionWidth;

	if (ImGui::GetCursorPosX() < ActionStartX)
	{
		ImGui::SameLine(ActionStartX);
	}
	else
	{
		ImGui::SameLine();
	}

	//Save랑 Reset은 기능없음
    if(ImGui::Button("Save"))
    {
    }
	ImGui::SameLine();
	if(ImGui::Button("Reset Pose")){
        GetPreviewMeshComponent()->ResetToReferencePose();
        RebuildBoneLocalTransformCache(GetPreviewMeshComponent());
	}

	ImGui::PopStyleVar(2);
}

void FEditorSkeletalMeshViewerPanel::SetSkelMesh()
{
    BoneLocalTransforms.clear();
}

USkeletalMeshComponent* FEditorSkeletalMeshViewerPanel::GetPreviewMeshComponent()
{
    return Owner ? Owner->GetPreviewMeshComponent() : nullptr;
}

bool FEditorSkeletalMeshViewerPanel::GetCachedBoneLocalTransform(int32 BoneIndex, FTransform& OutTransform)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    EnsureBoneLocalTransformCache(MeshComp);

    if (!MeshComp || BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneLocalTransforms.size()))
    {
        return false;
    }

    OutTransform = BoneLocalTransforms[BoneIndex];
    return true;
}

bool FEditorSkeletalMeshViewerPanel::SetCachedBoneLocalTransform(
    int32 BoneIndex,
    const FTransform& NewTransform,
    bool bApplyToComponent)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    EnsureBoneLocalTransformCache(MeshComp);

    if (!MeshComp || BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneLocalTransforms.size()))
    {
        return false;
    }

    BoneLocalTransforms[BoneIndex] = NewTransform;

    if (bApplyToComponent)
    {
        MeshComp->SetBoneLocalMatrix(BoneIndex, NewTransform.ToMatrix());
    }

    return true;
}

FQuat FEditorSkeletalMeshViewerPanel::GetCachedBoneComponentRotation(int32 BoneIndex)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    EnsureBoneLocalTransformCache(MeshComp);
    return GetCachedGlobalRotation(MeshComp, BoneIndex);
}

FVector FEditorSkeletalMeshViewerPanel::GetCachedBoneComponentScale(int32 BoneIndex)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    EnsureBoneLocalTransformCache(MeshComp);
    return GetCachedGlobalScale(MeshComp, BoneIndex);
}

void FEditorSkeletalMeshViewerPanel::EnsureBoneLocalTransformCache(USkeletalMeshComponent* MeshComp)
{
    if (!MeshComp)
    {
        BoneLocalTransforms.clear();
        return;
    }

    const int32 BoneCount = MeshComp->GetNumBones();
    if (BoneCount <= 0)
    {
        BoneLocalTransforms.clear();
        return;
    }

    if (static_cast<int32>(BoneLocalTransforms.size()) != BoneCount)
    {
        RebuildBoneLocalTransformCache(MeshComp);
    }
}

void FEditorSkeletalMeshViewerPanel::RebuildBoneLocalTransformCache(USkeletalMeshComponent* MeshComp)
{
    BoneLocalTransforms.clear();

    if (!MeshComp)
    {
        return;
    }

    const int32 BoneCount = MeshComp->GetNumBones();
    BoneLocalTransforms.reserve(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        BoneLocalTransforms.push_back(
            MakeTransformFromMatrixForEditCache(MeshComp->GetBoneLocalMatrix(BoneIndex)));
    }
}

FQuat FEditorSkeletalMeshViewerPanel::GetCachedGlobalRotation(USkeletalMeshComponent* MeshComp, int32 BoneIndex) const
{
    FQuat GlobalRotation = FQuat::Identity;

    if (!MeshComp || BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneLocalTransforms.size()))
    {
        return GlobalRotation;
    }

    TArray<int32> Chain;
    for (int32 CurrentIndex = BoneIndex; CurrentIndex >= 0;)
    {
        Chain.push_back(CurrentIndex);

        const FBoneInfo* Bone = MeshComp->GetBoneInfo(CurrentIndex);
        if (!Bone)
        {
            break;
        }

        CurrentIndex = Bone->ParentIndex;
    }

    for (auto It = Chain.rbegin(); It != Chain.rend(); ++It)
    {
        const int32 CurrentIndex = *It;
        if (CurrentIndex >= 0 && CurrentIndex < static_cast<int32>(BoneLocalTransforms.size()))
        {
            GlobalRotation = BoneLocalTransforms[CurrentIndex].Rotation * GlobalRotation;
        }
    }

    return GlobalRotation.GetNormalized();
}

FVector FEditorSkeletalMeshViewerPanel::GetCachedGlobalScale(USkeletalMeshComponent* MeshComp, int32 BoneIndex) const
{
    FVector GlobalScale(1.0f, 1.0f, 1.0f);

    if (!MeshComp || BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneLocalTransforms.size()))
    {
        return GlobalScale;
    }

    for (int32 CurrentIndex = BoneIndex; CurrentIndex >= 0;)
    {
        if (CurrentIndex < static_cast<int32>(BoneLocalTransforms.size()))
        {
            const FVector LocalScale = BoneLocalTransforms[CurrentIndex].Scale;
            GlobalScale.X *= LocalScale.X;
            GlobalScale.Y *= LocalScale.Y;
            GlobalScale.Z *= LocalScale.Z;
        }

        const FBoneInfo* Bone = MeshComp->GetBoneInfo(CurrentIndex);
        if (!Bone)
        {
            break;
        }

        CurrentIndex = Bone->ParentIndex;
    }

    return GlobalScale;
}

void FEditorSkeletalMeshViewerPanel::RenderPreviewViewport(float DeltaTime)
{
    if (!Owner)
    {
        return;
    }

    auto& VC = Owner->GetPreviewViewportClient();

    ImVec2 Pos = ImGui::GetCursorScreenPos();
    ImVec2 Size = ImGui::GetContentRegionAvail();

    FSkeletalMeshViewerState& State = Owner->GetState();
    FPreviewViewportOptions Options;
    Options.bShowMesh = State.bShowMesh;
    Options.bShowSkeleton = State.bShowSkeleton;
    Options.SelectedBoneIndex = State.SelectedBoneIndex;

    VC.SetPreviewOptions(Options);
    VC.SetViewportRect(Pos.x, Pos.y, Size.x, Size.y);
    VC.Draw(VC.GetViewport(), DeltaTime);
    VC.RenderViewportImage();
}

// TODO: 이름 수정이 가능하게 변경
void FEditorSkeletalMeshViewerPanel::RenderBoneHierarchyTree()
{
    ImGui::Text("Bone Hierarchy");

    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    const int32 BoneCount = MeshComp ? MeshComp->GetNumBones() : 0;
    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("No skeleton.");
        return;
    }

    ImGui::Text("Bones: %d", BoneCount);
    ImGui::Separator();

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FBoneInfo* Bone = MeshComp->GetBoneInfo(BoneIndex);
        if (Bone && Bone->ParentIndex < 0)
        {
            RenderBoneNode(BoneIndex);
        }
    }
}

void FEditorSkeletalMeshViewerPanel::RenderBoneNode(uint32 BoneIndex)
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    if (!MeshComp || BoneIndex >= static_cast<uint32>(MeshComp->GetNumBones()))
        return;

    const TArray<TArray<uint32>>& BoneHierarchy = Owner->GetState().BoneHierarchy;
    if (BoneIndex >= BoneHierarchy.size())
        return;

    const FBoneInfo* Bone = MeshComp->GetBoneInfo(static_cast<int32>(BoneIndex));
    if (!Bone)
        return;

    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

    const bool bSelected = (SelectedBoneIndex == BoneIndex);
    const bool bLeaf = BoneHierarchy[BoneIndex].empty();

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (bSelected)
        Flags |= ImGuiTreeNodeFlags_Selected;

    if (bLeaf)
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (Bone->ParentIndex < 0)
        Flags |= ImGuiTreeNodeFlags_DefaultOpen;

    ImGui::PushID(BoneIndex);

    const bool bOpen = ImGui::TreeNodeEx(
        Bone->Name.ToString().c_str(),
        Flags);

    if (ImGui::IsItemClicked())
    {
        SelectedBoneIndex = static_cast<int32>(BoneIndex);
        Owner->GetState().SelectedBoneIndex = SelectedBoneIndex;
    }

    if (!bLeaf && bOpen)
    {
        for (int32 ChildIndex : BoneHierarchy[BoneIndex])
        {
            RenderBoneNode(ChildIndex);
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void FEditorSkeletalMeshViewerPanel::RenderInspector()
{
    USkeletalMeshComponent* MeshComp = GetPreviewMeshComponent();
    const int32 BoneCount = MeshComp ? MeshComp->GetNumBones() : 0;

    int32& SelectedBoneIndex = Owner->GetState().SelectedBoneIndex;

    if (!MeshComp || SelectedBoneIndex < 0 || SelectedBoneIndex >= BoneCount)
    {
        ImGui::TextDisabled("No selected Bone");
        return;
    }

    const FBoneInfo* Bone = MeshComp->GetBoneInfo(SelectedBoneIndex);
    if (!Bone)
    {
        ImGui::TextDisabled("No selected Bone");
        return;
    }


    ImGui::Text("Index: %d", SelectedBoneIndex);
    ImGui::Text("Name: %s", Bone->Name.ToString().c_str());

    if (Bone->ParentIndex >= 0 && Bone->ParentIndex < BoneCount)
    {
        if (Bone->ParentIndex < static_cast<int32>(MeshComp->GetNumBones()))
        {
            const FBoneInfo* ParentBone = MeshComp->GetBoneInfo(Bone->ParentIndex);
            ImGui::Text("Parent: %d", Bone->ParentIndex);
            ImGui::Text("Parent Name: %s", ParentBone->Name.ToString().c_str());
		}
    }

    ImGui::SeparatorText("Local Transform");

    // 컴포넌트가 가진 현재 Local Matrix를 직접 읽는다.
    const FMatrix& LocalMatrix = MeshComp->GetBoneLocalMatrix(SelectedBoneIndex);

    // 기존에 네가 쓰던 Matrix -> FTransform 변환 함수 사용.
    FTransform LocalTransform = MakeTransformFromMatrixForEditCache(LocalMatrix);

    const FRotator LocalRotation = LocalTransform.GetRotator();

    float Location[3] = {
        LocalTransform.Location.X,
        LocalTransform.Location.Y,
        LocalTransform.Location.Z
    };

    float Rotation[3] = {
        LocalRotation.Pitch,
        LocalRotation.Yaw,
        LocalRotation.Roll
    };

    float Scale[3] = {
        LocalTransform.Scale.X,
        LocalTransform.Scale.Y,
        LocalTransform.Scale.Z
    };

    bool bChanged = false;
    bool bEditFinished = false;

    bChanged |= ImGui::DragFloat3("Local Location", Location, 0.1f);
    bEditFinished |= ImGui::IsItemDeactivatedAfterEdit();

    bChanged |= ImGui::DragFloat3("Local Rotation", Rotation, 0.1f);
    bEditFinished |= ImGui::IsItemDeactivatedAfterEdit();

    bChanged |= ImGui::DragFloat3("Local Scale", Scale, 0.01f);
    bEditFinished |= ImGui::IsItemDeactivatedAfterEdit();

    if (bChanged)
    {
        LocalTransform.Location = FVector(Location[0], Location[1], Location[2]);
        LocalTransform.Rotation =
            FRotator(Rotation[0], Rotation[1], Rotation[2])
                .GetClamped()
                .ToQuaternion();
        LocalTransform.Scale = FVector(Scale[0], Scale[1], Scale[2]);

        MeshComp->SetBoneLocalMatrix(
            SelectedBoneIndex,
            LocalTransform.ToMatrix());
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Global Transform");

    // 수정 후 값을 다시 읽어야 한다.
    const FMatrix& GlobalMatrix = MeshComp->GetBoneComponentMatrix(SelectedBoneIndex);

    const FTransform GlobalTransform =
        MakeTransformFromMatrixForEditCache(GlobalMatrix);

    const FVector GlobalLocation = GlobalTransform.Location;
    const FVector GlobalRotation = GlobalTransform.GetRotator().ToVector();
    const FVector GlobalScale = GlobalTransform.Scale;

    ImGui::Text("Global Location: %.3f, %.3f, %.3f",
                GlobalLocation.X,
                GlobalLocation.Y,
                GlobalLocation.Z);

    ImGui::Text("Global Rotation: %.3f, %.3f, %.3f",
                GlobalRotation.X,
                GlobalRotation.Y,
                GlobalRotation.Z);

    ImGui::Text("Global Scale: %.3f, %.3f, %.3f",
                GlobalScale.X,
                GlobalScale.Y,
                GlobalScale.Z);
}
