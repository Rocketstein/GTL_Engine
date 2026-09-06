#include "AMapChunk.h"
#include "GameFramework/World.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Game/GameActors/Obstacle/SimpleObstacleActor.h"
#include "Game/GameActors/Obstacle/BarrierObstacleActor.h"
#include "Game/GameActors/Obstacle/MustJumpObstacleActor.h"
#include "Game/GameActors/Obstacle/MustSlideObstacleActor.h"
#include "Game/GameActors/Obstacle/PendulumObstacleActor.h"
#include "Game/GameActors/Items/CrashDumpItemActor.h"
#include "Game/GameActors/Items/ItemActorBase.h"
#include "Game/GameActors/Items/LogFragmentItemActor.h"
#include "Game/Map/MapRandom.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(AMapChunk, AActor)

namespace {  
	constexpr const char* BuggedMaterialKey = "Default.Material.BasicShape";
	constexpr const char* NormalMaterialKey = "Sample.Material.BlueGrid";

	static FString SelectChunkMaterialPath(float ChunkBuggedRate)
	{
		const char* MaterialKey = MapRandom::Chance(ChunkBuggedRate)
			? BuggedMaterialKey
			: NormalMaterialKey;

		return FResourceManager::Get().ResolvePath(FName(MaterialKey));
	}
}

void AMapChunk::BeginPlay() {
	Super::BeginPlay();
}

void AMapChunk::EndPlay() {
	/*
	* 이 코드외에서 절대로 Destroy 처리를 하지 않는다.
	*/
	for (auto* Obstacle : SpawnedObstacles) {
		if (Obstacle && Obstacle->GetWorld() && Obstacle->GetWorld()->HasBegunPlay()) {
			Obstacle->GetWorld()->DestroyActor(Obstacle);
		}
	}

	SpawnedObstacles.clear();

	// Item은 Collision만 비활성화 하고 삭제는 Endplay에서만 한다.
	for (auto* Item : SpawnedItems) {
		if (Item && Item->GetWorld() && Item->GetWorld()->HasBegunPlay()) {
			Item->GetWorld()->DestroyActor(Item);
		}
	}

	SpawnedItems.clear();

	AActor::EndPlay();
}

void AMapChunk::InitFromTemplate(const FMapChunkTemplate& InTemplate, float InChunkBuggedRate) {
	Template         = InTemplate;
	ObstacleFillRate = InTemplate.ObstacleSpawnRate;
	ChunkBuggedRate = InChunkBuggedRate;
	BuildFloor();
	SpawnObstacle();
}

FVector AMapChunk::GetExitLocation() const
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	return GetActorLocation() + WorldQuat.RotateVector(Template.ExitOffset);
}

FRotator AMapChunk::GetExitRotation() const
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	FQuat ExitQuat = FQuat::FromRotator(Template.ExitRotation);
	return (WorldQuat * ExitQuat).ToRotator();
}


static AObstacleActorBase* SpawnObstacleOfType(UWorld* World, EObstacleType Type)
{
	if (!World)
	{
		return nullptr;
	}

	// Using SimpleObstacleActor as placeholder
	switch (Type)
	{
	case EObstacleType::Barrier: return World->SpawnActor<ABarrierObstacleActor>();
	case EObstacleType::LowBar:	 return World->SpawnActor<AMustJumpObstacleActor>();
	case EObstacleType::HighBar: return World->SpawnActor<AMustSlideObstacleActor>();
	case EObstacleType::Pendulum: return World->SpawnActor<APendulumObstacleActor>();
	case EObstacleType::Misc:    return World->SpawnActor<ASimpleObstacleActor>();
	default:                     return nullptr;
	}
}

static AObstacleActorBase* SpawnObstacleAt(UWorld* World, EObstacleType Type, const FVector& Location)
{
	AObstacleActorBase* Obstacle = SpawnObstacleOfType(World, Type);
	if (!Obstacle)
	{
		return nullptr;
	}

	Obstacle->InitDefaultComponents("");
	Obstacle->SetTag("Obstacle");
	// Lua PlayerController가 obstacle:GetDamage()로 읽을 기본 피해량입니다.
	// 기본 장애물은 1, 고위험으로 취급할 수 있는 Wireball/Misc는 2를 줄 수 있게 연결점만 둡니다.
	// TODO: 마감 이후 장애물별 Damage 테이블을 Lua Config로 옮기면 여기 하드코딩은 제거하면 됨.
	Obstacle->SetDamage(Type == EObstacleType::Misc ? 2 : 1);
	Obstacle->SetActorLocation(Location);
	World->InsertActorToOctree(Obstacle);

	if (!Obstacle->HasActorBegunPlay())
	{
		Obstacle->BeginPlay();
	}

	return Obstacle;
}

static AItemActorBase* SpawnItemOfType(UWorld* World, bool bCrashDump)
{
	if (!World)
	{
		return nullptr;
	}

	if (bCrashDump)
	{
		return World->SpawnActor<ACrashDumpItemActor>();
	}

	return World->SpawnActor<ALogFragmentItemActor>();
}

static AItemActorBase* SpawnItemAt(UWorld* World, bool bCrashDump, const FVector& Location)
{
	AItemActorBase* Item = SpawnItemOfType(World, bCrashDump);
	if (!Item)
	{
		return nullptr;
	}

	Item->SetActorLocation(Location);
	World->InsertActorToOctree(Item);

	if (!Item->HasActorBegunPlay())
	{
		Item->BeginPlay();
	}

	return Item;
}

static bool IsLaneBlockedByObstacle(EObstacleDecision Decision, int32 LaneIndex)
{
	switch (Decision)
	{
	case SingleBarrierLeft:
		return LaneIndex == 0;

	case SingleBarrierMiddle:
		return LaneIndex == 1;

	case SingleBarrierRight:
		return LaneIndex == 2;

	case DoubleBarrierLeft:
		return LaneIndex == 0 || LaneIndex == 1;

	case DoubleBarrierRight:
		return LaneIndex == 1 || LaneIndex == 2;

	//case MustJump:
	case MustSlide:
		return LaneIndex == 1;

	default:
		return false;
	}
}

static int32 PickOpenLane(EObstacleDecision Decision)
{
	TArray<int32> OpenLanes;

	for (int32 LaneIndex = 0; LaneIndex < 3; ++LaneIndex)
	{
		if (!IsLaneBlockedByObstacle(Decision, LaneIndex))
		{
			OpenLanes.push_back(LaneIndex);
		}
	}

	if (OpenLanes.empty())
	{
		return -1;
	}

	return OpenLanes[MapRandom::Index(static_cast<int32>(OpenLanes.size()))];
}

static FString GetMeshPath(const char* MeshKey)
{
	if (const FMeshResource* MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
	{
		return MeshResource->Path;
	}

	return "";
}

static void ApplyBasicShapeMaterial(UStaticMeshComponent* MeshComponent, UStaticMesh* Mesh, const FString& MaterialPath)
{
	if (!MeshComponent || !Mesh)
	{
		return;
	}

	UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
	if (!Material)
	{
		return;
	}

	int32 MaterialCount = static_cast<int32>(Mesh->GetStaticMaterials().size());
	if (MaterialCount == 0 && Mesh->GetStaticMeshAsset() &&
		(!Mesh->GetStaticMeshAsset()->Sections.empty() || !Mesh->GetStaticMeshAsset()->Indices.empty()))
	{
		MaterialCount = 1;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		MeshComponent->SetMaterial(MaterialIndex, Material);
	}
}

static void ApplyCubeMesh(UStaticMeshComponent* MeshComponent, const FString& MaterialPath)
{
	if (!MeshComponent || !GEngine)
	{
		return;
	}

	const FString MeshPath = GetMeshPath("Default.Mesh.BasicShape.Cube");
	if (MeshPath.empty())
	{
		return;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
	MeshComponent->SetStaticMesh(Mesh);
	ApplyBasicShapeMaterial(MeshComponent, Mesh, MaterialPath);
}

void AMapChunk::SpawnObstacle()
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	const float LaneY[3] = { -Template.Width / 1.5f, 0.0f, Template.Width / 1.5f };
	constexpr float ObstacleZ = 1.0f;

	for (const FDecisionSlot& DecisionSlot : Template.ObstacleSlotDecisions) {
		if (!MapRandom::Chance(ObstacleFillRate)) continue;
		if (DecisionSlot.AllowedDecisions.empty()) continue;
		if (bWasObstacleSpawned) continue;
		bWasObstacleSpawned = true;

		const int32 DecisionIndex = MapRandom::Index(static_cast<int32>(DecisionSlot.AllowedDecisions.size()));
		EObstacleDecision Decision = DecisionSlot.AllowedDecisions[DecisionIndex];

		auto WorldPositionForLane = [&](int32 LaneIndex)
		{
			return GetActorLocation() + WorldQuat.RotateVector(FVector(DecisionSlot.X, LaneY[LaneIndex], ObstacleZ + 1.f));
		};

		switch (Decision) {
		case (SingleBarrierLeft):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(0)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (SingleBarrierMiddle):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (SingleBarrierRight):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(2)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (DoubleBarrierLeft):
		{
			if (AObstacleActorBase* Obs0 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(0)))
				SpawnedObstacles.push_back(Obs0);
			if (AObstacleActorBase* Obs1 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs1);
			break;
		}
		case (DoubleBarrierRight):
		{
			if (AObstacleActorBase* Obs0 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(2)))
				SpawnedObstacles.push_back(Obs0);
			if (AObstacleActorBase* Obs1 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs1);
			break;
		}
		//case (MustJump):
		//{
		//	if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::LowBar, WorldPositionForLane(1))) {
		//		SpawnedObstacles.push_back(Obs);
		//	}
		//	break;
		//}
		case (MustSlide):
		{
			FVector SpawnLoc = WorldPositionForLane(1);
			SpawnLoc.Z = SpawnLoc.Z + 0.35f;
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::HighBar, SpawnLoc)) {
				SpawnedObstacles.push_back(Obs);
			}
			break;
		}
		case (Pendulum):
		{
			FVector SpawnLoc = WorldPositionForLane(1);
			SpawnLoc.Z = SpawnLoc.Z + 1.f;
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Pendulum, SpawnLoc)) {
				SpawnedObstacles.push_back(Obs);
			}
			break;
		}
		default: {
			break;
		}
		}

		SpawnItemForSlot(DecisionSlot, Decision);
	}
}

void AMapChunk::SpawnItemForSlot(const FDecisionSlot& DecisionSlot, EObstacleDecision Decision)
{
	if (!GetWorld())
	{
		return;
	}

	// 아이템은 모든 슬롯에 나오지 않도록 확률로 제한한다.
	if (!MapRandom::Chance(0.60f))
	{
		return;
	}
	// Item이 호출될 Index를 결정한다.
	const int32 LaneIndex = PickOpenLane(Decision);
	if (LaneIndex < 0)
	{
		return;
	}

	// Item이 Spawn될 위치 지정
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());

	const float LaneY[3] =
	{
		-Template.Width / 1.5f,
		0.0f,
		Template.Width / 1.5f
	};

	constexpr float ItemZ = 2.5f;

	// 장애물과 정확히 같은 X에 놓지 않는다.
	const float ItemX = DecisionSlot.X - 3.0f;

	const FVector LocalPosition(ItemX, LaneY[LaneIndex], ItemZ);
	const FVector WorldPosition = GetActorLocation() + WorldQuat.RotateVector(LocalPosition);

	// Crash Dump는 희귀 아이템(이지만 지금은 테스트를 위해 30%로 고정)
	// TODO: 변수로 관리하게 고려
	const bool bCrashDump = MapRandom::Chance(0.30f);

	if (AItemActorBase* Item = SpawnItemAt(GetWorld(), bCrashDump, WorldPosition))
	{
		SpawnedItems.push_back(Item);
	}
}

void AMapChunk::BuildFloor() {
	for (UStaticMeshComponent* FloorMesh : FloorMeshes)
	{
		RemoveComponent(FloorMesh);
	}
	FloorMeshes.clear();

	const FString ChunkMaterialPath = SelectChunkMaterialPath(ChunkBuggedRate);

	for (const FFloorBlock& BlockInfo : Template.FloorBlockInfos) {
		UStaticMeshComponent* Block = AddComponent<UStaticMeshComponent>();
		if (GetRootComponent())
		{
			Block->AttachToComponent(GetRootComponent());
		}

		ApplyCubeMesh(Block, ChunkMaterialPath);
		FVector BlockPos = BlockInfo.LocalPosition;
		Block->SetRelativeLocation(FVector(BlockPos.X, BlockPos.Y, BlockPos.Z));
		Block->SetRelativeRotation(BlockInfo.LocalRotation);
		Block->SetRelativeScale(BlockInfo.Scale);

		// PlayerController의 FindGround()는 collision enabled primitive만 바닥 후보로 본다.
		// Floor는 장애물처럼 데미지를 주는 대상은 아니지만, Player가 "딛고 설 수 있는 지형"이다.
		// 따라서 collision을 명시적으로 켜고, overlap도 켜서 디버그 중 상태를 쉽게 확인할 수 있게 한다.
		// Mesh/Transform/Scale을 모두 적용한 뒤 bounds를 dirty 처리해야 다음 ground query에서
		// 최신 world AABB, 특히 바닥 상단 Z값이 정확히 계산된다.
		Block->SetCollisionEnabled(true);
		Block->SetGenerateOverlapEvents(true);
		Block->MarkWorldBoundsDirty();

		FloorMeshes.push_back(Block);
	}

	// AddComponent는 Actor의 spatial partition/octree 상태를 자동으로 완전히 갱신하지 않는다.
	// FloorMesh를 런타임에 붙인 뒤 Actor를 다시 octree에 넣어야 렌더링/피킹/충돌 broad-phase에서
	// 새 floor component들이 안정적으로 관찰된다.
	if (UWorld* World = GetWorld())
	{
		World->InsertActorToOctree(this);
	}
}
