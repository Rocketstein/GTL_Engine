#pragma once
#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/Viewport/Asset/MeshEditorViewportClient.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"
#include "Asset/AssetRegistry.h"
#include "Physics/PhysicsHandles.h"

struct FSkeletalMesh;
struct ImDrawList;
struct ImVec2;
class UAnimSequence;
class UAnimMontage;
class UAnimSingleNodeInstance;
class UPhysicsAsset;
class UBodySetup;

enum class EMeshEditorTab : uint8 { Skeleton, Mesh, Animation, Physics };

struct FAnimationTabState
{
	UAnimSequence* CurrentSequence    = nullptr;
	UAnimMontage*  CurrentMontage     = nullptr;
	int32          SelectedAnimIndex     = -1;
	int32          SelectedMontageIndex  = -1;
	bool           bMontageSelected      = false;     // true 면 좌측 패널이 montage 표시
	// 타임라인에서 선택된 Notify entry 인덱스 (현재 시퀀스의 DataModel->Notifies 기준).
	// -1 = 미선택. 시퀀스/몽타주 전환 시 -1 reset 필요.
	// 유효 시 좌상단 AssetDetails 패널이 시퀀스 정보 대신 Notify 의 UPROPERTY 편집 UI 를 그림.
	int32         SelectedNotifyIndex     = -1;
	int32         SelectedMorphCurveIndex = -1;
	int32         SelectedMorphKeyIndex   = -1;
	TArray<float> MorphPreviewWeights;
	TArray<uint8> MorphPreviewOverrideMask;
	bool          bMorphPreviewOverrideEnabled = false;

	// Animation tab asset browser cache.
	// Render 중 매 프레임 ListAnimationsForSkeleton() -> LoadAnimation() -> Serialize() 되는 것을 막는다.
	TArray<FAssetListItem> CachedAnimationFiles;
	TArray<FAssetListItem> CachedMontageFiles;
	FSkeletonBinding       CachedAnimationListBinding;
	bool                   bAnimationListDirty = true;

	float         AnimListWidth                = 200.0f;
	float         AnimDetailsWidth             = 280.0f;

	FFbxAnimationImportDialogState AnimationImportDialog;
};

class FMeshEditorWidget : public FAssetEditorWidget
{
public:
	FMeshEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

	bool IsMouseOverViewport() const { return IsOpen() && ViewportClient.IsMouseOverViewport(); }

	FMeshEditorViewportClient* GetViewportClient() { return &ViewportClient; }

	static void RecordImportDurationForAsset(const FString& AssetPath, double Seconds);
	static void ClearImportDurationForAsset(const FString& AssetPath);

private:
	// Tab bar
	void RenderTabBar();

	// Per-tab layouts
	void RenderSkeletonLayout();
	void RenderMeshLayout();
	void RenderAnimationLayout(float TotalHeight);
	void RenderPhysicsLayout();

	// Physics tab helpers (PhysicsAsset 저작: 발제 2-2)
	void           RenderPhysicsDetails();
	void           RenderPhysicsBulkEdit();   // 다중선택 시 바디/조인트 일괄 편집 패널
	UPhysicsAsset* EnsurePhysicsAssetForCurrentSkeleton();
	void           AddBodyToSelectedBone();
	void           RemoveBodyAtSelectedBone();
	void           GenerateConstraintForSelectedBone();
	int32          FindPhysicsBodyIndexForBone(int32 BoneIndex) const;
	int32          FindPhysicsConstraintIndexForChild(const FName& ChildBone) const;
	FName          GetPhysicsBoneName(int32 BoneIndex) const;
	void           RenderPhysicsBoneTree(const FSkeletalMesh* Asset, int32 Index);

	// Shared helpers
	void RenderViewportPanel(ImVec2 Size);
	void RenderBoneTree(const FSkeletalMesh* Asset, int32 Index);
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;

	// Animation tab helpers
	void ApplyAnimationToComponent();
	void ResetMorphPreviewOverrides();
	void EnsureMorphPreviewOverrideSize();
	void ApplyMorphPreviewOverrides(TArray<float>& InOutMorphWeights) const;
	void RefreshAnimationPreviewPose();
	void MarkAnimationListDirty();
	const TArray<FAssetListItem>& GetCachedAnimationFilesForCurrentSkeleton();
	const TArray<FAssetListItem>& GetCachedMontageFilesForCurrentSkeleton();

private:
	FMeshEditorViewportClient ViewportClient;

	// Tab state
	EMeshEditorTab     ActiveTab = EMeshEditorTab::Skeleton;
	FAnimationTabState AnimTabState;

	// Skeleton tab state
	int32 SelectedBoneIndex = -1;
	float HierarchyWidth    = 250.0f;
	float DetailsWidth      = 300.0f;

	// Physics tab state
	// EditedObject 는 항상 USkeletalMesh 로 유지하고, 편집 중인 PhysicsAsset 은 따로 보유.
	// (UPhysicsAsset 을 열면 그 스켈레톤에 호환되는 메시를 EditedObject 로 해석)
	UPhysicsAsset* CurrentPhysicsAsset     = nullptr;
	int32          SelectedConstraintIndex = -1;
	bool           bSimulating             = false;

	// 본 트리 다중선택(일괄편집용). SelectedBoneIndex 는 primary(디테일/뷰포트 하이라이트).
	TArray<int32>  SelectedBoneIndices;
	// 일괄편집 템플릿(헤더 의존 최소화 위해 plain 값; .cpp 에서 enum 캐스트).
	int32 BulkTwistMotion = 0, BulkSwing1Motion = 0, BulkSwing2Motion = 0;  // 0=Limited,1=Locked,2=Free
	float BulkTwistLimit = 45.f, BulkSwing1Limit = 30.f, BulkSwing2Limit = 30.f;
	int32 BulkBodyPhysicsType = 0;   // 0=Default,1=Kinematic,2=Simulated
	// 시뮬 중 랙돌이 쌓일 정적 바닥(프리뷰 물리 씬). Stop 시 해제.
	FPhysicsActorHandle PreviewGroundHandle;

	uint32  InstanceId;
	FName   PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	FString LastCreatedClothAssetPath;
	FString LastClothAssetError;

	bool bPendingClose = false;
};
