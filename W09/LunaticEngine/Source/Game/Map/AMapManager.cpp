#include "AMapManager.h"
#include "GameFramework/World.h"
#include "Game/Map/MapRandom.h"
#include "Game/GameActors/Obstacle/ObstacleActorBase.h"
#include "Object/Object.h"

IMPLEMENT_CLASS(AMapManager, AActor)

AMapManager::AMapManager()
{
	bTickInEditor = false;
	BuildTemplateLibrary();
}

void AMapManager::BeginPlay() {
	AActor::BeginPlay();
	BuildTemplateLibrary();
}

void AMapManager::EndPlay() {
	AActor::EndPlay();
}

void AMapManager::Tick(float DeltaTime) {
	if (!bEnabled) return;
	if (!Player) return; 
	if (Templates.empty()) return;

	while ((int32)ActiveChunks.size() < TargetChunkCount) {
		if (ActiveChunks.empty()) {
			SpawnNextChunk(true);
		}
		SpawnNextChunk();
	}

	if (!ActiveChunks.empty())
	{
		AMapChunk* Front = ActiveChunks[0];
		FQuat ExitQuat = FQuat::FromRotator(Front->GetExitRotation());
		FVector ToPlayer = Player->GetActorLocation() - Front->GetExitLocation();
		float ExitProgress = ToPlayer.Dot(ExitQuat.GetForwardVector());

		if (ExitProgress > 10.0f)
		{
			DespawnFrontChunk();
			TrySpawnGimmickAtChunkEnd();
		}
	}

}

void AMapManager::BuildTemplateLibrary() {
	constexpr float ChunkWidth = 6.f;	// Recommended multiple of 3
	constexpr float ChunkLength = 20.f; // Recommended multiple of 2
	constexpr float TurnLength = 20.f;
	constexpr float LaneY[3] = { -ChunkWidth / 1.5, 0, ChunkWidth / 1.5f };

	Templates.clear();

	auto AddDefaultDecisionSlots = [](FMapChunkTemplate& Template, float StartX, float EndX, float Step)
	{
		for (float X = StartX; X <= EndX; X += Step)
		{
			FDecisionSlot Slot = {};
			Slot.X = X;
			for (int32 DecisionIndex = 0; DecisionIndex < static_cast<int32>(EObstacleDecision::Count); ++DecisionIndex)
			{
				Slot.AllowedDecisions.push_back(static_cast<EObstacleDecision>(DecisionIndex));
			}
			Template.ObstacleSlotDecisions.push_back(Slot);
		}
	};

	auto AddNonBarrierDecisionSlots = [](FMapChunkTemplate& Template, float StartX, float EndX, float Step)
		{
			for (float X = StartX; X <= EndX; X += Step)
			{
				FDecisionSlot Slot = {};
				Slot.X = X;
				for (int32 DecisionIndex = 0; DecisionIndex < static_cast<int32>(EObstacleDecision::Count); ++DecisionIndex)
				{
					EObstacleDecision Decision = static_cast<EObstacleDecision>(DecisionIndex);
					if (Decision == EObstacleDecision::Pendulum || Decision == EObstacleDecision::MustSlide)
						Slot.AllowedDecisions.push_back(static_cast<EObstacleDecision>(DecisionIndex));
				}
				Template.ObstacleSlotDecisions.push_back(Slot);
			}
		};

	auto AddNonSlideDecisionSlots = [](FMapChunkTemplate& Template, float StartX, float EndX, float Step)
		{
			for (float X = StartX; X <= EndX; X += Step)
			{
				FDecisionSlot Slot = {};
				Slot.X = X;
				for (int32 DecisionIndex = 0; DecisionIndex < static_cast<int32>(EObstacleDecision::Count); ++DecisionIndex)
				{
					EObstacleDecision Decision = static_cast<EObstacleDecision>(DecisionIndex);
					if (Decision != EObstacleDecision::MustSlide)
						Slot.AllowedDecisions.push_back(static_cast<EObstacleDecision>(DecisionIndex));
				}
				Template.ObstacleSlotDecisions.push_back(Slot);
			}
		};

	//-----------------------------------------------------------------
	// Start
	//-----------------------------------------------------------------
	FMapChunkTemplate Start;
	Start.ChunkType = EChunkType::Start;
	Start.Length = 30.f;
	Start.Width  = ChunkWidth;
	Start.ExitOffset = FVector(30.f, 0.f, 0.f);

	// Start Floor infos
	FFloorBlock StartFloor = {};
	StartFloor.LocalPosition = FVector(15.f, 0, 0);
	StartFloor.LocalRotation = FRotator(0, 0, 0);
	StartFloor.Scale		 = FVector(15.f, ChunkWidth, 1);
	Start.FloorBlockInfos.push_back(StartFloor);
	Templates.push_back(Start);
	// No obstacles should spawn in the start region; leave the array blank

	//-----------------------------------------------------------------
	// Straight
	//-----------------------------------------------------------------
	FMapChunkTemplate Straight;
	Straight.ChunkType = EChunkType::Straight;
	Straight.Length = ChunkLength;
	Straight.Width  = ChunkWidth;
	Straight.ExitOffset = FVector(ChunkLength, 0.0f, 0.0f);
	Straight.ObstacleSpawnRate = 0.7f;

	// Straight floor infos
	FFloorBlock StraightFloor = {};
	StraightFloor.LocalPosition = FVector(ChunkLength * 0.5f, 0, 0);
	StraightFloor.LocalRotation = FRotator(0, 0, 0);
	StraightFloor.Scale			= FVector(ChunkLength * 0.5, ChunkWidth, 1);
	Straight.FloorBlockInfos.push_back(StraightFloor);

	constexpr uint8 AllTypes = 0xFF;
	float LeftLaneY  = -ChunkWidth / 1.5;
	float MidLaneY   = 0.f;
	float RightLaneY = ChunkWidth / 1.5f;
	AddDefaultDecisionSlots(Straight, 2.f, Straight.Length - 2.f, 2.f);

	Templates.push_back(Straight);
	Templates.push_back(Straight);
	Templates.push_back(Straight);

	//-----------------------------------------------------------------
	// Left Turn
	//-----------------------------------------------------------------
	//FMapChunkTemplate LeftTurn;
	//LeftTurn.ChunkType = EChunkType::TurnLeft;
	//LeftTurn.Length = ChunkLength;
	//LeftTurn.ExitOffset = FVector(TurnLength, -TurnLength, 0.0f);
	//LeftTurn.ExitRotation = FRotator(0.0f, -90.0f, 0.0f);  // Yaw -90 for left

	//// Left Turn floor
	//FFloorBlock LeftTurnFloorStraight = {};
	//LeftTurnFloorStraight.LocalPosition = FVector(TurnLength * 0.5f, 0, 0);
	//LeftTurnFloorStraight.LocalRotation = FRotator(0, 0, 0);
	//LeftTurnFloorStraight.Scale			= FVector(TurnLength, ChunkWidth, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorStraight);

	//FFloorBlock LeftTurnFloorCorner = {};
	//LeftTurnFloorCorner.LocalPosition = FVector(TurnLength + ChunkWidth * 0.25f, -ChunkWidth * 0.25f, 0);
	//LeftTurnFloorCorner.LocalRotation = FRotator(0, 0, 0);
	//LeftTurnFloorCorner.Scale		 = FVector(ChunkWidth * 0.5f, ChunkWidth * 0.5f, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorCorner);

	//FFloorBlock LeftTurnFloorExit = {};
	//LeftTurnFloorExit.LocalPosition = FVector(TurnLength, -(TurnLength + ChunkWidth * 0.5f) * 0.5f, 0);
	//LeftTurnFloorExit.LocalRotation = FRotator(0, -90, 0);
	//LeftTurnFloorExit.Scale			= FVector(TurnLength - ChunkWidth * 0.5f, ChunkWidth, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorExit);


	//Templates.push_back(LeftTurn);

	//-----------------------------------------------------------------
	// Right Turn
	//-----------------------------------------------------------------
	//FMapChunkTemplate RightTurn;
	//RightTurn.ChunkType = EChunkType::TurnRight;
	//RightTurn.Length = ChunkLength;
	//RightTurn.ExitOffset = FVector(TurnLength, TurnLength, 0.f);
	//RightTurn.ExitRotation = FRotator(0.0f, 90.0f, 0.f);

	//// Right Turn Floor
	//FFloorBlock RightTurnFloorStraight = {};
	//RightTurnFloorStraight.LocalPosition = FVector(TurnLength * 0.5f, 0, 0);
	//RightTurnFloorStraight.LocalRotation = FRotator(0, 0, 0);
	//RightTurnFloorStraight.Scale		 = FVector(TurnLength, ChunkWidth, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorStraight);

	//FFloorBlock RightTurnFloorCorner = {};
	//RightTurnFloorCorner.LocalPosition = FVector(TurnLength + ChunkWidth * 0.25f, ChunkWidth * 0.25f, 0);
	//RightTurnFloorCorner.LocalRotation = FRotator(0, 0, 0);
	//RightTurnFloorCorner.Scale		 = FVector(ChunkWidth * 0.5f, ChunkWidth * 0.5f, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorCorner);

	//FFloorBlock RightTurnFloorExit = {};
	//RightTurnFloorExit.LocalPosition = FVector(TurnLength, (TurnLength + ChunkWidth * 0.5f) * 0.5f, 0);
	//RightTurnFloorExit.LocalRotation = FRotator(0, 90, 0);
	//RightTurnFloorExit.Scale		 = FVector(TurnLength - ChunkWidth * 0.5f, ChunkWidth, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorExit);

	//Templates.push_back(RightTurn);

	//-----------------------------------------------------------------
	// Straight With Hole
	//-----------------------------------------------------------------
	FMapChunkTemplate StraightWithHole;
	StraightWithHole.ChunkType = EChunkType::StraightWithHole;
	StraightWithHole.Length = ChunkLength / 2;
	StraightWithHole.Width  = ChunkWidth;
	StraightWithHole.ExitOffset = FVector(StraightWithHole.Length, 0.f, 0.f);
	StraightWithHole.ObstacleSpawnRate = 0.4f;

	// Straight With Hole Floor
	FFloorBlock StraightWithHoleFloor1 = {};
	StraightWithHoleFloor1.LocalPosition = FVector(StraightWithHole.Length / 8.f, 0, 0);
	StraightWithHoleFloor1.LocalRotation = FRotator(0, 0, 0);
	StraightWithHoleFloor1.Scale		 = FVector(StraightWithHole.Length / 8, ChunkWidth, 1);
	StraightWithHole.FloorBlockInfos.push_back(StraightWithHoleFloor1);

	FFloorBlock StraightWithHoleFloor2 = {};
	StraightWithHoleFloor2.LocalPosition = FVector(StraightWithHole.Length * 0.75f, 0, 0);
	StraightWithHoleFloor2.LocalRotation = FRotator(0, 0, 0);
	StraightWithHoleFloor2.Scale		 = FVector(StraightWithHole.Length / 4, ChunkWidth, 1);
	StraightWithHole.FloorBlockInfos.push_back(StraightWithHoleFloor2);

	// Obstacles
	AddNonSlideDecisionSlots(StraightWithHole, StraightWithHole.Length * 0.5f, StraightWithHole.Length - 2.f, 1.f);
	Templates.push_back(StraightWithHole);

	//-----------------------------------------------------------------
	// Straight With One Lane (Left)
	//-----------------------------------------------------------------
	FMapChunkTemplate StraightWithOneLaneL;
	StraightWithOneLaneL.ChunkType = EChunkType::StraightOneLaneL;
	StraightWithOneLaneL.Length	   = ChunkLength;
	StraightWithOneLaneL.Width	   = ChunkWidth;
	StraightWithOneLaneL.ExitOffset = FVector(StraightWithOneLaneL.Length, 0.f, 0.f);
	StraightWithOneLaneL.ObstacleSpawnRate = 0.2f;

	// StraightWithOneLaneL Floor
	FFloorBlock StraightWithOneLaneL1 = {};
	StraightWithOneLaneL1.LocalPosition = FVector(StraightWithOneLaneL.Length / 8.f, 0, 0);
	StraightWithOneLaneL1.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneL1.Scale			= FVector(StraightWithOneLaneL.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneL.FloorBlockInfos.push_back(StraightWithOneLaneL1);

	FFloorBlock StraightWithOneLaneL2 = {};
	StraightWithOneLaneL2.LocalPosition = FVector(StraightWithOneLaneL.Length / 2.f, LaneY[0], 0);
	StraightWithOneLaneL2.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneL2.Scale			= FVector(StraightWithOneLaneL.Length / 4, 1, 1); 
	StraightWithOneLaneL.FloorBlockInfos.push_back(StraightWithOneLaneL2);

	FFloorBlock StraightWithOneLaneL3 = {};
	StraightWithOneLaneL3.LocalPosition = FVector(StraightWithOneLaneL.Length * 7 / 8.f, 0, 0);
	StraightWithOneLaneL3.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneL3.Scale			= FVector(StraightWithOneLaneL.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneL.FloorBlockInfos.push_back(StraightWithOneLaneL3);

	// Obstacles
	AddNonBarrierDecisionSlots(StraightWithOneLaneL, 2.f, StraightWithOneLaneL.Length - 2.f, 2.f);
	Templates.push_back(StraightWithOneLaneL);

	//-----------------------------------------------------------------
	// Straight With One Lane (Middle)
	//-----------------------------------------------------------------
	FMapChunkTemplate StraightWithOneLaneM;
	StraightWithOneLaneM.ChunkType = EChunkType::StraightOneLaneM;
	StraightWithOneLaneM.Length = ChunkLength;
	StraightWithOneLaneM.Width = ChunkWidth;
	StraightWithOneLaneM.ExitOffset = FVector(StraightWithOneLaneM.Length, 0.f, 0.f);
	StraightWithOneLaneM.ObstacleSpawnRate = 0.2f;

	// StraightWithOneLaneM Floor
	FFloorBlock StraightWithOneLaneM1 = {};
	StraightWithOneLaneM1.LocalPosition = FVector(StraightWithOneLaneM.Length / 8.f, 0, 0);
	StraightWithOneLaneM1.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneM1.Scale = FVector(StraightWithOneLaneM.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneM.FloorBlockInfos.push_back(StraightWithOneLaneM1);

	FFloorBlock StraightWithOneLaneM2 = {};
	StraightWithOneLaneM2.LocalPosition = FVector(StraightWithOneLaneM.Length / 2.f, LaneY[1], 0);
	StraightWithOneLaneM2.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneM2.Scale = FVector(StraightWithOneLaneM.Length / 4, 1, 1);
	StraightWithOneLaneM.FloorBlockInfos.push_back(StraightWithOneLaneM2);

	FFloorBlock StraightWithOneLaneM3 = {};
	StraightWithOneLaneM3.LocalPosition = FVector(StraightWithOneLaneM.Length * 7 / 8.f, 0, 0);
	StraightWithOneLaneM3.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneM3.Scale = FVector(StraightWithOneLaneM.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneM.FloorBlockInfos.push_back(StraightWithOneLaneM3);

	// Obstacles
	AddNonBarrierDecisionSlots(StraightWithOneLaneM, 2.f, StraightWithOneLaneL.Length - 2.f, 2.f);
	Templates.push_back(StraightWithOneLaneM);

	//-----------------------------------------------------------------
	// Straight With One Lane (Right)
	//-----------------------------------------------------------------
	FMapChunkTemplate StraightWithOneLaneR;
	StraightWithOneLaneR.ChunkType = EChunkType::StraightOneLaneR;
	StraightWithOneLaneR.Length = ChunkLength;
	StraightWithOneLaneR.Width = ChunkWidth;
	StraightWithOneLaneR.ExitOffset = FVector(StraightWithOneLaneR.Length, 0.f, 0.f);
	StraightWithOneLaneR.ObstacleSpawnRate = 0.2f;

	// StraightWithOneLaneM Floor
	FFloorBlock StraightWithOneLaneR1 = {};
	StraightWithOneLaneR1.LocalPosition = FVector(StraightWithOneLaneR.Length / 8.f, 0, 0);
	StraightWithOneLaneR1.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneR1.Scale = FVector(StraightWithOneLaneR.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneR.FloorBlockInfos.push_back(StraightWithOneLaneR1);

	FFloorBlock StraightWithOneLaneR2 = {};
	StraightWithOneLaneR2.LocalPosition = FVector(StraightWithOneLaneR.Length / 2.f, LaneY[2], 0);
	StraightWithOneLaneR2.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneR2.Scale = FVector(StraightWithOneLaneR.Length / 4, 1, 1);
	StraightWithOneLaneR.FloorBlockInfos.push_back(StraightWithOneLaneR2);

	FFloorBlock StraightWithOneLaneR3 = {};
	StraightWithOneLaneR3.LocalPosition = FVector(StraightWithOneLaneR.Length * 7 / 8.f, 0, 0);
	StraightWithOneLaneR3.LocalRotation = FRotator(0, 0, 0);
	StraightWithOneLaneR3.Scale = FVector(StraightWithOneLaneR.Length / 8, ChunkWidth, 1);
	StraightWithOneLaneR.FloorBlockInfos.push_back(StraightWithOneLaneR3);

	//Obstacles
	AddNonBarrierDecisionSlots(StraightWithOneLaneR, 2.f, StraightWithOneLaneR.Length - 2.f, 2.f);
	Templates.push_back(StraightWithOneLaneR);
}

void AMapManager::SpawnNextChunk(bool Init)
{
	int32 Idx = SelectNextTemplateIndex();
	const FMapChunkTemplate& T = Init ? Templates[0] : Templates[Idx];

	FVector  SpawnLoc = ActiveChunks.empty() ? FVector(0, 0, 0) : ActiveChunks.back()->GetExitLocation();
	FRotator SpawnRot = ActiveChunks.empty() ? FRotator(0, 0, 0) : ActiveChunks.back()->GetExitRotation();

	AMapChunk* Chunk = GetWorld()->SpawnActor<AMapChunk>();
	Chunk->SetActorLocation(SpawnLoc);
	Chunk->SetActorRotation(SpawnRot);

	// TODO: 장애물 생성 확률은 생존 시간/거리 기반으로 서서히 증가시키되,
	// 플레이 불가능한 수준으로 올라가지 않게 적당한 상한을 둔다.
	Chunk->InitFromTemplate(T, ObstacleSpawnRate);
	if (ChunkBuggedRate < 1.0f) ChunkBuggedRate += 0.05f;
	ActiveChunks.push_back(Chunk);

	//bool bIsTurn = (T.ChunkType == EChunkType::TurnLeft || T.ChunkType == EChunkType::TurnRight);
	//StraightRunLength = bIsTurn ? 0 : StraightRunLength + 1;
}

void AMapManager::DespawnFrontChunk() {
	if (ActiveChunks.empty()) return;
	AMapChunk* Front = ActiveChunks.front();
	GetWorld()->DestroyActor(Front);
	ActiveChunks.erase(ActiveChunks.begin());
}

void AMapManager::TrySpawnGimmickAtChunkEnd()
{
	if (!Player)
	{
		return;
	}

	TArray<AObstacleActorBase*> CandidateObstacles = GatherNearbyObstacleCandidates();
	GimmickManager.TrySpawnRandomGimmick(GetWorld(), CandidateObstacles, GimmickSpawnChance);
}

TArray<AObstacleActorBase*> AMapManager::GatherNearbyObstacleCandidates() const
{
	TArray<AObstacleActorBase*> Candidates;
	if (!Player)
	{
		return Candidates;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const float MaxDistanceSq = GimmickTargetSearchDistance * GimmickTargetSearchDistance;

	for (AMapChunk* Chunk : ActiveChunks)
	{
		if (!Chunk || !IsAliveObject(Chunk))
		{
			continue;
		}

		if (FVector::DistSquared(PlayerLocation, Chunk->GetActorLocation()) > MaxDistanceSq)
		{
			continue;
		}

		for (AObstacleActorBase* Obstacle : Chunk->GetSpawnedObstacles())
		{
			if (Obstacle && IsAliveObject(Obstacle))
			{
				Candidates.push_back(Obstacle);
			}
		}
	}

	return Candidates;
}

void AMapManager::Initialize(AActor* InPlayer)
{
	SetPlayerActor(InPlayer);
	ResetMap();
}

void AMapManager::ResetMap()
{
	UWorld* World = GetWorld();
	for (AMapChunk* Chunk : ActiveChunks)
	{
		if (Chunk && World)
		{
			World->DestroyActor(Chunk);
		}
	}

	ActiveChunks.clear();
	StraightRunLength = 0;
}

void AMapManager::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

void AMapManager::SetPlayerActor(AActor* InPlayer)
{
	Player = InPlayer;
}

int32 AMapManager::SelectNextTemplateIndex()
{
	TArray<int32> Candidates;
	for (int32 i = 1; i < (int32)Templates.size(); ++i)
	{
		EChunkType T = Templates[i].ChunkType;
		//bool bIsTurn = (T == EChunkType::TurnLeft || T == EChunkType::TurnRight);
		//if (bIsTurn && StraightRunLength < MinStraightsBetweenTurns)
		//	continue;
		Candidates.push_back(i);
	}

	return Candidates[MapRandom::Index(static_cast<int32>(Candidates.size()))];
}
