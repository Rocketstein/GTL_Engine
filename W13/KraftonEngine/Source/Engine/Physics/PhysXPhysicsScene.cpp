#include "Physics/PhysXPhysicsScene.h"
#include "Physics/PhysXVehicleManager.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Object/Object.h"  // IsAliveObject
#include "Mesh/Static/StaticMesh.h"
#include "Physics/Asset/BodySetup.h"
#include "Physics/Asset/ConstraintSetup.h"
#include "Physics/BodyInstance.h"
#include "Physics/IPhysicsBodySync.h"
#include "Profiling/Stats/Stats.h"
#include "Profiling/Stats/PhysicsStats.h"
#include "Core/ProjectSettings.h"
#include "Core/Logging/Log.h"

// PhysX headers
#include <PxPhysicsAPI.h>
#include <algorithm>
#include <thread>
#include <cmath>
#include <cstring>

using namespace physx;

// ============================================================
// PhysX Error Callback
// ============================================================
class FPhysXErrorCallback : public PxErrorCallback
{
public:
	void reportError(PxErrorCode::Enum code, const char* message,
		const char* file, int line) override
	{
		const char* severity = "Info";
		if (code == PxErrorCode::eABORT || code == PxErrorCode::eOUT_OF_MEMORY)
			severity = "Fatal";
		else if (code == PxErrorCode::eINTERNAL_ERROR || code == PxErrorCode::eINVALID_OPERATION)
			severity = "Error";
		else if (code == PxErrorCode::eINVALID_PARAMETER || code == PxErrorCode::ePERF_WARNING)
			severity = "Warning";
		else if (code == PxErrorCode::eDEBUG_WARNING)
			severity = "Warning";

		UE_LOG("[PhysX %s] %s (%s:%d)", severity, message, file, line);
	}
};

static FPhysXErrorCallback GPhysXErrorCallback;
static PxDefaultAllocator GPhysXAllocator;

// ============================================================
// PhysX Foundation/Physics 싱글턴
// PxCreateFoundation은 프로세스당 1회만 허용 — 복수 Scene에서 공유
// ============================================================
static PxFoundation* GSharedFoundation = nullptr;
static PxPhysics* GSharedPhysics = nullptr;
static PxCooking* GSharedCooking = nullptr;
static PxPvd* GSharedPvd = nullptr;
static PxPvdTransport* GSharedPvdTransport = nullptr;
static bool GSharedExtensionsInitialized = false;
static bool GVehicleSDKInitialized = false;
static int32 GSharedRefCount = 0;

static void ReleaseSharedPvd()
{
	if (GSharedPvd)
	{
		GSharedPvd->disconnect();
		GSharedPvd->release();
		GSharedPvd = nullptr;
	}
	if (GSharedPvdTransport)
	{
		GSharedPvdTransport->release();
		GSharedPvdTransport = nullptr;
	}
}

static PxPvd* CreateSharedPvdIfEnabled(PxFoundation& Foundation)
{
	const auto& PhysicsSettings = FProjectSettings::Get().Physics;
	if (!PhysicsSettings.bEnablePVD)
	{
		return nullptr;
	}

	GSharedPvd = PxCreatePvd(Foundation);
	if (!GSharedPvd)
	{
		UE_LOG("[PhysX] PVD creation failed");
		return nullptr;
	}

	const char* Host = PhysicsSettings.PvdHost.empty() ? "127.0.0.1" : PhysicsSettings.PvdHost.c_str();
	const int Port = static_cast<int>((std::max)(1u, (std::min)(PhysicsSettings.PvdPort, 65535u)));
	const unsigned int TimeoutMs = static_cast<unsigned int>(PhysicsSettings.PvdTimeoutMs);
	GSharedPvdTransport = PxDefaultPvdSocketTransportCreate(Host, Port, TimeoutMs);
	if (!GSharedPvdTransport)
	{
		UE_LOG("[PhysX] PVD socket transport creation failed (%s:%d)", Host, Port);
		ReleaseSharedPvd();
		return nullptr;
	}

	if (!GSharedPvd->connect(*GSharedPvdTransport, PxPvdInstrumentationFlag::eALL))
	{
		UE_LOG("[PhysX] PVD connection failed (%s:%d, timeout=%u ms)", Host, Port, TimeoutMs);
		ReleaseSharedPvd();
		return nullptr;
	}

	UE_LOG("[PhysX] PVD connected (%s:%d)", Host, Port);
	return GSharedPvd;
}

static void AcquireSharedPhysX(PxFoundation*& OutFoundation, PxPhysics*& OutPhysics)
{
	if (GSharedRefCount == 0)
	{
		GSharedFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
		if (GSharedFoundation)
		{
			CreateSharedPvdIfEnabled(*GSharedFoundation);
			GSharedPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *GSharedFoundation, PxTolerancesScale(),
				GSharedPvd != nullptr, GSharedPvd);
			if (GSharedPhysics)
			{
				GSharedExtensionsInitialized = PxInitExtensions(*GSharedPhysics, GSharedPvd);
				UE_LOG("[PhysX] Extensions initialized (PVD=%d, Result=%d)",
					GSharedPvd ? 1 : 0,
					GSharedExtensionsInitialized ? 1 : 0);

				GSharedCooking = PxCreateCooking(
					PX_PHYSICS_VERSION,
					*GSharedFoundation,
					PxCookingParams(GSharedPhysics->getTolerancesScale()));

				if (PxInitVehicleSDK(*GSharedPhysics))
				{
					PxVehicleSetBasisVectors(PxVec3(0.0f, 0.0f, 1.0f), PxVec3(1.0f, 0.0f, 0.0f));
					PxVehicleSetUpdateMode(PxVehicleUpdateMode::eACCELERATION);
					GVehicleSDKInitialized = true;
				}
			}
		}
	}
	++GSharedRefCount;
	OutFoundation = GSharedFoundation;
	OutPhysics = GSharedPhysics;
}

static void ReleaseSharedPhysX()
{
	if (--GSharedRefCount <= 0)
	{
		if (GVehicleSDKInitialized)
		{
			PxCloseVehicleSDK();
			GVehicleSDKInitialized = false;
		}
		if (GSharedCooking)
		{
			GSharedCooking->release();
			GSharedCooking = nullptr;
		}
		if (GSharedExtensionsInitialized)
		{
			PxCloseExtensions();
			GSharedExtensionsInitialized = false;
		}
		if (GSharedPhysics) { GSharedPhysics->release(); GSharedPhysics = nullptr; }
		ReleaseSharedPvd();
		if (GSharedFoundation) { GSharedFoundation->release(); GSharedFoundation = nullptr; }
		GSharedRefCount = 0;
	}
}

static const char* TriangleCookingResultToString(PxTriangleMeshCookingResult::Enum Result)
{
	switch (Result)
	{
	case PxTriangleMeshCookingResult::eSUCCESS:
		return "success";
	case PxTriangleMeshCookingResult::eLARGE_TRIANGLE:
		return "large triangle";
	case PxTriangleMeshCookingResult::eFAILURE:
	default:
		return "failure";
	}
}

bool FPhysXPhysicsScene::CookTriangleMesh(const TArray<FVector>& Vertices, const TArray<int32>& Indices,
	TArray<uint8>& OutCookedData, FString* OutError)
{
	auto SetError = [OutError](const char* Message)
	{
		if (OutError)
		{
			*OutError = Message ? Message : "";
		}
	};

	OutCookedData.clear();
	if (Vertices.size() < 3 || Indices.size() < 3 || (Indices.size() % 3) != 0)
	{
		SetError("Triangle mesh input is incomplete.");
		return false;
	}

	PxFoundation* CookingFoundation = nullptr;
	PxPhysics* CookingPhysics = nullptr;
	AcquireSharedPhysX(CookingFoundation, CookingPhysics);
	if (!CookingFoundation || !CookingPhysics)
	{
		ReleaseSharedPhysX();
		SetError("PhysX Foundation/Physics could not be created for cooking.");
		return false;
	}

	PxCookingParams CookingParams(CookingPhysics->getTolerancesScale());
	CookingParams.meshPreprocessParams |= PxMeshPreprocessingFlag::eWELD_VERTICES;
	CookingParams.meshWeldTolerance = 0.001f;

	PxCooking* Cooking = PxCreateCooking(PX_PHYSICS_VERSION, *CookingFoundation, CookingParams);
	if (!Cooking)
	{
		ReleaseSharedPhysX();
		SetError("PxCreateCooking failed.");
		return false;
	}

	PxTriangleMeshDesc Desc;
	Desc.points.count = static_cast<PxU32>(Vertices.size());
	Desc.points.stride = sizeof(FVector);
	Desc.points.data = Vertices.data();
	Desc.triangles.count = static_cast<PxU32>(Indices.size() / 3);
	Desc.triangles.stride = sizeof(int32) * 3;
	Desc.triangles.data = Indices.data();

	PxTriangleMeshCookingResult::Enum CookResult = PxTriangleMeshCookingResult::eFAILURE;
	PxDefaultMemoryOutputStream OutputStream;
	const bool bCooked = Cooking->cookTriangleMesh(Desc, OutputStream, &CookResult);
	if (bCooked && CookResult != PxTriangleMeshCookingResult::eFAILURE && OutputStream.getSize() > 0)
	{
		const PxU32 Size = OutputStream.getSize();
		OutCookedData.resize(Size);
		std::memcpy(OutCookedData.data(), OutputStream.getData(), Size);
	}
	else
	{
		FString Message = "PxCooking::cookTriangleMesh failed: ";
		Message += TriangleCookingResultToString(CookResult);
		SetError(Message.c_str());
	}

	Cooking->release();
	ReleaseSharedPhysX();

	if (OutCookedData.empty())
	{
		return false;
	}

	SetError("");
	return true;
}

// ============================================================
// PhysX Simulation Event Callback
//
// PhysX 의 onContact / onTrigger 는 Scene->fetchResults(true) 진행 중에 호출되며,
// 그 안에서 직접 게임 측 핸들러(NotifyComponentHit 등)를 호출하면 핸들러가
// World->DestroyActor 같은 scene-mutating 작업을 해서 fetchResults 와 겹쳐 크래쉬한다.
//
// 따라서 콜백은 이벤트를 큐에 적재만 하고, FPhysXPhysicsScene::Tick 의 post-simulate
// 단계 끝에서 DispatchPendingEvents 가 한꺼번에 게임 측 Notify 를 호출한다. 이 시점은
// simulate/fetchResults 외부이므로 핸들러가 자유롭게 actor/component 를 추가/제거해도 안전.
// ============================================================
class FPhysXSimulationCallback : public PxSimulationEventCallback
{
public:
	struct FQueuedHit
	{
		UPrimitiveComponent* Self      = nullptr;  // Notify 가 호출되는 대상
		UPrimitiveComponent* Other     = nullptr;
		FVector              NormalImpulse{0,0,0};
		FHitResult           Hit;
		bool                 bBegin = true;       // false = end
	};

	struct FQueuedTrigger
	{
		UPrimitiveComponent* Self  = nullptr;
		UPrimitiveComponent* Other = nullptr;
		bool                 bBegin = true;        // false = end
	};

	// Block 접촉 → 큐에 적재
	void onContact(const PxContactPairHeader& PairHeader,
		const PxContactPair* Pairs, PxU32 Count) override
	{
		if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0
			|| PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
			return;

		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxContactPair& CP = Pairs[i];
			const bool bBegin = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bEnd = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bEnd) continue;

			auto* CompA = CP.shapes[0] ? static_cast<UPrimitiveComponent*>(CP.shapes[0]->userData) : nullptr;
			auto* CompB = CP.shapes[1] ? static_cast<UPrimitiveComponent*>(CP.shapes[1]->userData) : nullptr;
			if (!CompA || !CompB) continue;

			if (bEnd)
			{
				FQueuedHit A;
				A.Self = CompA;
				A.Other = CompB;
				A.bBegin = false;
				PendingHits.push_back(A);

				FQueuedHit B;
				B.Self = CompB;
				B.Other = CompA;
				B.bBegin = false;
				PendingHits.push_back(B);
				continue;
			}

			// Contact point — 큐 dispatch 시점에 PxContactPair 가 이미 무효이므로 여기서 모두 추출.
			PxContactPairPoint ContactPoints[1];
			PxU32 NumPoints = CP.extractContacts(ContactPoints, 1);

			FVector ContactPos(0, 0, 0);
			FVector ContactNormal(0, 0, 1);
			float Penetration = 0.0f;

			if (NumPoints > 0)
			{
				ContactPos    = FVector(ContactPoints[0].position.x, ContactPoints[0].position.y, ContactPoints[0].position.z);
				ContactNormal = FVector(ContactPoints[0].normal.x,   ContactPoints[0].normal.y,   ContactPoints[0].normal.z);
				Penetration   = ContactPoints[0].separation; // 음수 = 관통
			}

			const FVector NormalImpulse = ContactNormal * Penetration;

			FQueuedHit A;
			A.Self                = CompA;
			A.Other               = CompB;
			A.NormalImpulse       = NormalImpulse;
			A.Hit.bHit            = true;
			A.Hit.HitComponent    = CompB;
			A.Hit.HitActor        = CompB->GetOwner();
			A.Hit.WorldHitLocation= ContactPos;
			A.Hit.ImpactNormal    = ContactNormal;
			A.Hit.WorldNormal     = ContactNormal;
			A.Hit.PenetrationDepth= -Penetration;
			PendingHits.push_back(A);

			FQueuedHit B;
			B.Self                 = CompB;
			B.Other                = CompA;
			B.NormalImpulse        = NormalImpulse * -1.0f;
			B.Hit.bHit             = true;
			B.Hit.HitComponent     = CompA;
			B.Hit.HitActor         = CompA->GetOwner();
			B.Hit.WorldHitLocation = ContactPos;
			B.Hit.ImpactNormal     = ContactNormal * -1.0f;
			B.Hit.WorldNormal      = ContactNormal * -1.0f;
			B.Hit.PenetrationDepth = -Penetration;
			PendingHits.push_back(B);
		}
	}

	// Trigger 진입/이탈 → 큐에 적재
	void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
	{
		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxTriggerPair& TP = Pairs[i];

			if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;

			auto* TriggerComp = TP.triggerShape ? static_cast<UPrimitiveComponent*>(TP.triggerShape->userData) : nullptr;
			auto* OtherComp   = TP.otherShape   ? static_cast<UPrimitiveComponent*>(TP.otherShape->userData)   : nullptr;
			if (!TriggerComp || !OtherComp) continue;

			const bool bBegin = (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bEnd   = (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bEnd) continue;

			if (TriggerComp->GetGenerateOverlapEvents())
			{
				PendingTriggers.push_back({ TriggerComp, OtherComp, bBegin });
			}
			if (OtherComp->GetGenerateOverlapEvents())
			{
				PendingTriggers.push_back({ OtherComp, TriggerComp, bBegin });
			}
		}
	}

	// FPhysXPhysicsScene::Tick 끝에서 호출. simulate/fetchResults 바깥이므로 핸들러가
	// 자유롭게 World->DestroyActor / SpawnActor / RegisterComponent 호출 가능.
	// 핸들러 도중 다른 컴포넌트가 destroy되는 경우 대비해 dispatch 직전에 IsAliveObject
	// 검증 — destroy된 포인터를 만지지 않는다.
	void DispatchPendingEvents()
	{
		// move-out — dispatch 도중 새 이벤트가 큐에 들어오는 일은 없지만, 안전하게 swap 후 처리.
		std::vector<FQueuedHit> HitsToDispatch;
		HitsToDispatch.swap(PendingHits);
		std::vector<FQueuedTrigger> TriggersToDispatch;
		TriggersToDispatch.swap(PendingTriggers);

		for (FQueuedHit& E : HitsToDispatch)
		{
			if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (E.bBegin)
			{
				E.Self->NotifyComponentHit(E.Self, OtherActor, E.Other, E.NormalImpulse, E.Hit);
			}
			else
			{
				E.Self->NotifyComponentEndHit(E.Self, OtherActor, E.Other);
			}
		}

		for (FQueuedTrigger& E : TriggersToDispatch)
		{
			if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (E.bBegin)
			{
				FHitResult DummyHit;
				E.Self->NotifyComponentBeginOverlap(E.Self, OtherActor, E.Other, 0, false, DummyHit);
			}
			else
			{
				E.Self->NotifyComponentEndOverlap(E.Self, OtherActor, E.Other, 0);
			}
		}
	}

	void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
	void onWake(PxActor**, PxU32) override {}
	void onSleep(PxActor**, PxU32) override {}
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

private:
	std::vector<FQueuedHit>     PendingHits;
	std::vector<FQueuedTrigger> PendingTriggers;
};

// ============================================================
// Transform 변환 유틸
// ============================================================
static PxVec3 ToPxVec3(const FVector& V)
{
	return PxVec3(V.X, V.Y, V.Z);
}

static PxQuat ToPxQuat(const FQuat& Q)
{
	return PxQuat(Q.X, Q.Y, Q.Z, Q.W);
}

static FVector ToFVector(const PxVec3& V)
{
	return FVector(V.x, V.y, V.z);
}

static FQuat ToFQuat(const PxQuat& Q)
{
	return FQuat(Q.x, Q.y, Q.z, Q.w);
}

static PxTransform ToPxTransform(const FTransform& T)
{
	return PxTransform(ToPxVec3(T.Location), ToPxQuat(T.Rotation));
}

static FTransform ToFTransform(const PxTransform& T)
{
	return FTransform(ToFVector(T.p), ToFQuat(T.q), FVector(1.0f, 1.0f, 1.0f));
}

static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
	FVector Pos = Comp->GetWorldLocation();
	FQuat Rot = Comp->GetWorldMatrix().ToQuat();
	return PxTransform(ToPxVec3(Pos), ToPxQuat(Rot));
}

// ============================================================
// Collision Filtering
// ============================================================
// filterData 레이아웃:
//   word0 = 자신의 ObjectType (ECollisionChannel)
//   word1 = Block 비트마스크 (해당 채널에 Block 응답인 비트)
//   word2 = Overlap 비트마스크 (해당 채널에 Overlap 응답인 비트)
//   word3 = 소유 액터 UUID — 같은 액터의 두 컴포넌트끼리 충돌을 무시하기 위함
//           (Native 측 O(N²) 루프의 `if (A->GetOwner() == B->GetOwner()) continue;` 가드와 동일 의미)
//           Owner가 없거나 UUID가 0이면 가드 미적용.

static void SetupFilterData(PxShape* Shape, UPrimitiveComponent* Comp)
{
	PxFilterData Filter;
	Filter.word0 = static_cast<PxU32>(Comp->GetCollisionObjectType());
	Filter.word1 = 0;
	Filter.word2 = 0;
	Filter.word3 = Comp->GetOwner() ? Comp->GetOwner()->GetUUID() : 0;

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		ECollisionResponse R = Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch));
		if (R == ECollisionResponse::Block)   Filter.word1 |= (1u << Ch);
		if (R == ECollisionResponse::Overlap) Filter.word2 |= (1u << Ch);
	}

	Shape->setSimulationFilterData(Filter);
	Shape->setQueryFilterData(Filter);
}

static void DisablePvdSceneVisualization(PxScene* Scene);
static void DisablePvdActorVisualization(PxRigidActor* Actor);
static void DisablePvdShapeVisualization(PxShape* Shape);

static void SetupDefaultRawBodyFilterData(PxShape* Shape)
{
	if (!Shape) return;

	DisablePvdShapeVisualization(Shape);

	PxFilterData Filter;
	Filter.word0 = static_cast<PxU32>(ECollisionChannel::WorldDynamic);
	Filter.word1 = 0;
	Filter.word2 = 0;
	Filter.word3 = 0;

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		Filter.word1 |= (1u << Ch);
	}

	Shape->setSimulationFilterData(Filter);
	Shape->setQueryFilterData(Filter);
}

static bool ShouldUseTriggerShape(UPrimitiveComponent* Comp)
{
	if (!Comp)
	{
		return false;
	}

	if (Comp->GetGenerateOverlapEvents())
	{
		return true;
	}

	for (int32 Ch = 0; Ch < static_cast<int32>(ECollisionChannel::ActiveCount); ++Ch)
	{
		if (Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch)) == ECollisionResponse::Block)
		{
			return false;
		}
	}
	return true;
}

static void SetupComponentShape(PxShape* Shape, UPrimitiveComponent* Comp)
{
	if (!Shape || !Comp)
	{
		return;
	}

	DisablePvdShapeVisualization(Shape);

	SetupFilterData(Shape, Comp);

	if (ShouldUseTriggerShape(Comp))
	{
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	Shape->userData = Comp;
}

// 자식 컴포넌트의 RootComp 대비 상대 transform. 현재는 미사용이며, Phase 8 weld(자식 shape 를
// 부모 actor 에 상대 포즈로 붙이기)에서 사용 예정 — 그때까지 보존.
[[maybe_unused]] static PxTransform GetComponentLocalPoseRelativeToRoot(UPrimitiveComponent* Comp, UPrimitiveComponent* RootComp)
{
	if (!Comp || Comp == RootComp || !RootComp)
	{
		return PxTransform(PxIdentity);
	}

	const FVector RootPos = RootComp->GetWorldLocation();
	const FQuat RootRot = RootComp->GetWorldMatrix().ToQuat();
	const FVector CompPos = Comp->GetWorldLocation();
	const FQuat CompRot = Comp->GetWorldMatrix().ToQuat();

	const FQuat InvRootRot = RootRot.Inverse();
	const FVector LocalPos = InvRootRot.RotateVector(CompPos - RootPos);
	const FQuat LocalRot = InvRootRot * CompRot;

	return PxTransform(ToPxVec3(LocalPos), ToPxQuat(LocalRot));
}

// ============================================================================
//  지오메트리 빌더 레이어
//  모든 콜라이더 생성은 아래 두 빌더로 수렴한다:
//    - AddAggregateGeometryShapes : FKAggregateGeom(Sphere/Box/Sphyl) → shape(들)
//    - AttachTriangleMeshShape    : 쿡된 트라이앵글 메시 → shape
//  컴포넌트 바디와 랙돌 본이 모두 AddGeometry 를 통해 이 빌더를 공유하고, 경로별 차이는
//  SetupShape 콜백(필터/트리거/self-collision)으로만 표현한다.
//
//  [PxAggregate 불변식] 빌더는 PxRigidActor* 와 PxMaterial* 만 다루며 PxScene/PxAggregate
//  를 절대 건드리지 않는다(actor-agnostic). 따라서 actor 가 이후 Scene->addActor 로 붙든
//  PxAggregate->addActor 로 묶이든 shape 생성 로직은 그대로다. self-collision 도 빌더가
//  아닌 SetupShape 콜백에서 처리하므로, 향후 PxAggregate(selfCollision=false) 도입 시
//  콜백만 비우면 되고 빌더는 손대지 않는다.
// ============================================================================
template<typename ShapeSetupFunc>
static PxShape* AddAggregateGeometryShapes(
	PxRigidActor* RigidActor,
	PxMaterial* Material,
	const FKAggregateGeom& GeometryData,
	const FVector& Scale,
	const PxTransform& BaseLocalPose,
	ShapeSetupFunc SetupShape)
{
	if (!RigidActor || !Material || GeometryData.GetElementCount() <= 0)
	{
		return nullptr;
	}

	const float AbsScaleX = std::max(std::abs(Scale.X), 0.001f);
	const float AbsScaleY = std::max(std::abs(Scale.Y), 0.001f);
	const float AbsScaleZ = std::max(std::abs(Scale.Z), 0.001f);
	PxShape* FirstShape = nullptr;

	auto ScalePosition = [&](const FVector& P)
	{
		return FVector(P.X * Scale.X, P.Y * Scale.Y, P.Z * Scale.Z);
	};

	auto AttachShape = [&](const PxGeometry& Geometry, const PxTransform& LocalPose)
	{
		PxShape* Shape = PxRigidActorExt::createExclusiveShape(*RigidActor, Geometry, *Material);
		if (!Shape)
		{
			return;
		}

		Shape->setLocalPose(BaseLocalPose * LocalPose);
		SetupShape(Shape);

		if (!FirstShape)
		{
			FirstShape = Shape;
		}
	};

	for (const FKSphereElem& Sphere : GeometryData.SphereElems)
	{
		const float RadiusScale = std::max({ AbsScaleX, AbsScaleY, AbsScaleZ });
		const float Radius = std::max(Sphere.Radius * RadiusScale, 0.001f);
		const PxTransform LocalPose(ToPxVec3(ScalePosition(Sphere.Center)), PxQuat(PxIdentity));
		AttachShape(PxSphereGeometry(Radius), LocalPose);
	}

	for (const FKBoxElem& Box : GeometryData.BoxElems)
	{
		const float HalfX = std::max(Box.HalfExtent.X * AbsScaleX, 0.001f);
		const float HalfY = std::max(Box.HalfExtent.Y * AbsScaleY, 0.001f);
		const float HalfZ = std::max(Box.HalfExtent.Z * AbsScaleZ, 0.001f);
		const PxTransform LocalPose(
			ToPxVec3(ScalePosition(Box.Center)),
			ToPxQuat(Box.Rotation.ToQuaternion()));
		AttachShape(PxBoxGeometry(HalfX, HalfY, HalfZ), LocalPose);
	}

	for (const FKSphylElem& Sphyl : GeometryData.SphylElems)
	{
		const float RadiusScale = std::max(AbsScaleX, AbsScaleZ);
		const float Radius = std::max(Sphyl.Radius * RadiusScale, 0.001f);
		const float HalfHeight = std::max((Sphyl.Length * 0.5f) * AbsScaleY, 0.001f);
		PxTransform LocalPose(
			ToPxVec3(ScalePosition(Sphyl.Center)),
			ToPxQuat(Sphyl.Rotation.ToQuaternion()));
		LocalPose.q = LocalPose.q * PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
		AttachShape(PxCapsuleGeometry(Radius, HalfHeight), LocalPose);
	}

	return FirstShape;
}

// 쿡된 트라이앵글 메시 데이터로 단일 shape 를 만들어 붙인다. 트라이앵글 메시는 정적/키네마틱
// actor 만 백업할 수 있어 월드 정적 지오메트리(StaticMesh) 전용이다.
// AddAggregateGeometryShapes 와 마찬가지로 actor-agnostic — 씬이나 PxAggregate 를 일절
// 건드리지 않으므로, actor 가 이후 어떻게 씬에 추가되든(addActor vs addAggregate) 영향이 없다.
template<typename ShapeSetupFunc>
static PxShape* AttachTriangleMeshShape(
	PxPhysics* Physics,
	PxRigidActor* RigidActor,
	PxMaterial* Material,
	const TArray<uint8>& CookedData,
	const FVector& Scale,
	const PxTransform& BaseLocalPose,
	ShapeSetupFunc SetupShape)
{
	if (!Physics || !RigidActor || !Material || CookedData.empty())
	{
		return nullptr;
	}

	PxDefaultMemoryInputData InputData(const_cast<PxU8*>(CookedData.data()), static_cast<PxU32>(CookedData.size()));
	PxTriangleMesh* TriangleMesh = Physics->createTriangleMesh(InputData);
	if (!TriangleMesh)
	{
		UE_LOG("[PhysX] Failed to create PxTriangleMesh from cooked data");
		return nullptr;
	}

	const PxVec3 MeshScale(
		std::abs(Scale.X) > PX_MESH_SCALE_MIN ? Scale.X : (Scale.X < 0.0f ? -PX_MESH_SCALE_MIN : PX_MESH_SCALE_MIN),
		std::abs(Scale.Y) > PX_MESH_SCALE_MIN ? Scale.Y : (Scale.Y < 0.0f ? -PX_MESH_SCALE_MIN : PX_MESH_SCALE_MIN),
		std::abs(Scale.Z) > PX_MESH_SCALE_MIN ? Scale.Z : (Scale.Z < 0.0f ? -PX_MESH_SCALE_MIN : PX_MESH_SCALE_MIN));

	PxTriangleMeshGeometry TriGeom(TriangleMesh, PxMeshScale(MeshScale));
	PxShape* Shape = PxRigidActorExt::createExclusiveShape(*RigidActor, TriGeom, *Material);
	TriangleMesh->release();   // shape 가 참조를 유지하므로 여기서 release 가능
	if (!Shape)
	{
		return nullptr;
	}

	Shape->setLocalPose(BaseLocalPose);
	SetupShape(Shape);
	return Shape;
}

static bool IsSceneCCDEnabled()
{
	return FProjectSettings::Get().Physics.bEnableCCD;
}

static void SetDynamicCCDEnabled(PxRigidDynamic* Dynamic, bool bEnabled)
{
	if (!Dynamic) return;

	Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, bEnabled && IsSceneCCDEnabled());
}

static void DisablePvdSceneVisualization(PxScene* Scene)
{
	if (!Scene) return;

	Scene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_AABBS, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eACTOR_AXES, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eBODY_AXES, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_SHAPES, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_EDGES, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eCOLLISION_FNORMALS, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 0.0f);
	Scene->setVisualizationParameter(PxVisualizationParameter::eJOINT_LIMITS, 0.0f);
}

static void DisablePvdActorVisualization(PxRigidActor* Actor)
{
	if (Actor)
	{
		Actor->setActorFlag(PxActorFlag::eVISUALIZATION, false);
	}
}

static void DisablePvdShapeVisualization(PxShape* Shape)
{
	if (Shape)
	{
		Shape->setFlag(PxShapeFlag::eVISUALIZATION, false);
	}
}

static bool IsPvdConnected()
{
	return GSharedPvd && GSharedPvd->isConnected(false);
}

static void LogPvdActorState(const char* Context, PxScene* Scene, PxRigidActor* Actor)
{
	if (!IsPvdConnected() || !Actor)
	{
		return;
	}

	const PxU32 SceneActorCount = Scene
		? Scene->getNbActors(PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC)
		: 0;
	const char* ActorType = Actor->is<PxRigidDynamic>() ? "Dynamic" : "Static";
	UE_LOG("[PhysX][PVD] %s Actor=%p Type=%s Shapes=%u SceneActors=%u",
		Context,
		static_cast<void*>(Actor),
		ActorType,
		Actor->getNbShapes(),
		SceneActorCount);
}

// PxFilterShader — 엔진의 채널/응답 매트릭스를 PhysX에서 처리
// 양쪽 모두 상대 채널에 대해 Block이면 물리 충돌, 한쪽이라도 Overlap이면 트리거, 그 외 무시
static PxFilterFlags KraftonFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* /*constantBlock*/, PxU32 /*constantBlockSize*/)
{
	// 같은 액터(같은 owner UUID)의 두 컴포넌트끼리는 충돌 무시.
	// Native 측 O(N²) 루프의 same-owner 가드와 동일 의미. 차량 차체-바퀴처럼
	// 한 액터가 여러 콜라이더를 가질 때 자기끼리 충돌 시뮬레이션되는 문제를 막는다.
	if (filterData0.word3 != 0 && filterData0.word3 == filterData1.word3)
	{
		return PxFilterFlag::eKILL;
	}

	// 트리거 처리 — 한쪽이라도 트리거면 오버랩 통지만
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	PxU32 channelA = filterData0.word0; // A의 ObjectType
	PxU32 channelB = filterData1.word0; // B의 ObjectType

	// A가 B의 채널에 대해 Block인지, B가 A의 채널에 대해 Block인지
	bool bABlocksB = (filterData0.word1 & (1u << channelB)) != 0;
	bool bBBlocksA = (filterData1.word1 & (1u << channelA)) != 0;

	// 양쪽 모두 Block → 물리 충돌 + contact 콜백
	if (bABlocksB && bBBlocksA)
	{
		pairFlags = PxPairFlag::eCONTACT_DEFAULT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_LOST
			| PxPairFlag::eNOTIFY_CONTACT_POINTS;
		if (IsSceneCCDEnabled())
		{
			pairFlags |= PxPairFlag::eDETECT_CCD_CONTACT;
		}
		return PxFilterFlag::eDEFAULT;
	}

	// 한쪽이라도 Overlap → 겹침 감지만 (물리적 밀어내기 없음).
	// 일반적으로 이 케이스는 위 trigger shape 분기에서 이미 처리되지만, 등록 시점에
	// trigger flag로 분류되지 않은 simulation shape pair인데 응답이 Overlap인 경우의
	// 안전망. eSOLVE_CONTACT 명시 제외 + eDETECT_DISCRETE_CONTACT + NOTIFY로 detection만.
	bool bAOverlapsB = (filterData0.word2 & (1u << channelB)) != 0;
	bool bBOverlapsA = (filterData1.word2 & (1u << channelA)) != 0;

	if (bAOverlapsB || bBOverlapsA)
	{
		pairFlags = PxPairFlag::eDETECT_DISCRETE_CONTACT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_LOST;
		return PxFilterFlag::eDEFAULT;
	}

	// Ignore — 쌍 완전히 제거
	return PxFilterFlag::eKILL;
}

// ============================================================
// Lifecycle
// ============================================================

void FPhysXPhysicsScene::Initialize(UWorld* InWorld)
{
	World = InWorld;

	// Foundation / Physics — 프로세스 싱글턴 공유
	AcquireSharedPhysX(Foundation, Physics);
	if (!Foundation || !Physics)
	{
		UE_LOG("[PhysX] Failed to create Foundation or Physics");
		return;
	}

	const auto& PhysicsSettings = FProjectSettings::Get().Physics;
	// WorkerThreadCount==0 → auto: 메인/렌더 스레드 여유분(2)을 빼고 남은 코어를 워커로(최소 1).
	// PhysX simulate 의 솔버/브로드페이즈가 이 워커들로 병렬화된다.
	uint32 WorkerThreadCount = PhysicsSettings.WorkerThreadCount;
	if (WorkerThreadCount == 0)
	{
		const unsigned HardwareConcurrency = std::thread::hardware_concurrency();
		const unsigned AutoWorkers = (HardwareConcurrency > 3) ? (HardwareConcurrency - 2) : 1;
		// 하이퍼스레드까지 다 쓰면 과구독 — PhysX 솔버는 ~16 에서 수확 체감하고, 남는 스레드는
		// 메인/렌더에 양보하는 게 낫다. auto 는 16 으로 캡(명시적 설정은 아래 32 까지 허용).
		WorkerThreadCount = (std::min)(AutoWorkers, 16u);
	}
	WorkerThreadCount = (std::max)(1u, (std::min)(WorkerThreadCount, 32u));

	// CPU Dispatcher
	Dispatcher = PxDefaultCpuDispatcherCreate(static_cast<PxU32>(WorkerThreadCount));

	// Event callback
	EventCallback = new FPhysXSimulationCallback();

	// Scene
	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity = PxVec3(0.0f, 0.0f, -9.81f); // Z-up, m 단위
	SceneDesc.cpuDispatcher = Dispatcher;
	SceneDesc.filterShader = KraftonFilterShader;
	SceneDesc.simulationEventCallback = EventCallback;

	auto SetSceneFlag = [&SceneDesc](PxSceneFlag::Enum Flag, bool bEnabled)
	{
		if (bEnabled)
		{
			SceneDesc.flags |= Flag;
		}
		else
		{
			SceneDesc.flags.clear(Flag);
		}
	};
	SetSceneFlag(PxSceneFlag::eENABLE_CCD, PhysicsSettings.bEnableCCD);
	SetSceneFlag(PxSceneFlag::eENABLE_PCM, PhysicsSettings.bEnablePCM);
	SetSceneFlag(PxSceneFlag::eENABLE_ACTIVE_ACTORS, PhysicsSettings.bEnableActiveActors);

	Scene = Physics->createScene(SceneDesc);

	if (!Scene)
	{
		UE_LOG("[PhysX] Failed to create Scene");
		return;
	}

	if (PxPvdSceneClient* PvdClient = Scene->getScenePvdClient())
	{
		DisablePvdSceneVisualization(Scene);
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, PhysicsSettings.bPvdTransmitContacts);
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, PhysicsSettings.bPvdTransmitConstraints);
		PvdClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, PhysicsSettings.bPvdTransmitSceneQueries);
		UE_LOG("[PhysX] PVD scene flags (Contacts=%d, Constraints=%d, SceneQueries=%d, Visualization=None)",
			PhysicsSettings.bPvdTransmitContacts ? 1 : 0,
			PhysicsSettings.bPvdTransmitConstraints ? 1 : 0,
			PhysicsSettings.bPvdTransmitSceneQueries ? 1 : 0);
	}
	else if (IsPvdConnected())
	{
		UE_LOG("[PhysX] PVD is connected, but Scene PVD client is unavailable.");
	}

	// Default material (static friction, dynamic friction, restitution)
	DefaultMaterial = Physics->createMaterial(0.7f, 0.7f, 0.3f);
	if (!DefaultMaterial)
	{
		UE_LOG("[PhysX] Failed to create default material");
		Scene->release();
		Scene = nullptr;
		return;
	}

	Cooking = GSharedCooking;
	VehicleManager = new FPhysXVehicleManager();
	VehicleManager->Init(Physics, Scene, Cooking, DefaultMaterial);

	UE_LOG("[PhysX] Initialized successfully (Scene=%p, Workers=%u, CCD=%d, PCM=%d, ActiveActors=%d, PVD=%d)",
		Scene,
		WorkerThreadCount,
		PhysicsSettings.bEnableCCD ? 1 : 0,
		PhysicsSettings.bEnablePCM ? 1 : 0,
		PhysicsSettings.bEnableActiveActors ? 1 : 0,
		(GSharedPvd && GSharedPvd->isConnected(true)) ? 1 : 0);
}

void FPhysXPhysicsScene::Shutdown()
{
	for (PxD6Joint* Joint : Owned.Constraints)
	{
		if (Joint)
		{
			Joint->release();
		}
	}
	Owned.Constraints.clear();

	// Raw actor 경로 정리 — 정상 흐름에선 소유자(SkeletalMeshComponent)가 먼저 TermArticulated
	// 로 비우지만, 누수 없이 일괄 정리한다. actor release 가 aggregate/scene 에서 자동 분리한다.
	for (PxRigidActor* Actor : Owned.Actors)
	{
		if (Actor)
		{
			Actor->release();
		}
	}
	Owned.Actors.clear();

	for (PxAggregate* Aggregate : Owned.Aggregates)
	{
		if (Aggregate)
		{
			Aggregate->release();
		}
	}
	Owned.Aggregates.clear();
	BodySyncs.clear();
	// 컴포넌트 바디의 PxActor 는 위 Owned.Actors 루프가 이미 해제했다(InitBody→CreateActor 가
	// Owned.Actors 에 등록). 소유 FBodyInstance 의 핸들을 무효화해, 이후 컴포넌트 파괴 시 TermBody 가
	// 이미 해제된 actor 를 재해제(double free)하지 않도록 한다.
	for (FBodyInstance* Body : ComponentBodies)
	{
		if (Body) Body->ActorHandle = {};
	}
	ComponentBodies.clear();

	if (VehicleManager) { VehicleManager->Release(); delete VehicleManager; VehicleManager = nullptr; }
	if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
	if (Scene) { Scene->release(); Scene = nullptr; }
	if (EventCallback) { delete EventCallback; EventCallback = nullptr; }
	if (Dispatcher) { Dispatcher->release(); Dispatcher = nullptr; }

	// Foundation/Physics는 공유 싱글턴 — release 카운트 감소만
	Foundation = nullptr;
	Physics = nullptr;
	ReleaseSharedPhysX();

	World = nullptr;
	PHYSICS_STATS_RESET();
}

// ============================================================
// Body 관리 — per-component FBodyInstance 레지스트리
//
// UPrimitiveComponent 가 각자 FBodyInstance(=PxRigidActor 1개)를 소유하고, InitBody 가
// 이 씬의 CreateActor/AddGeometry 를 호출해 actor 를 만든다. 컴포넌트는 AddBody 로 자신의
// FBodyInstance 를 등록하고, Start/FinishSimulation 이 그 OwnerComponent 와 트랜스폼을 동기화한다.
// userData: PxActor → AActor(컴포넌트) / FBodyInstance(랙돌), PxShape → UPrimitiveComponent.
// ============================================================

void FPhysXPhysicsScene::AddBody(FBodyInstance* Body)
{
	if (!Body) return;
	if (std::find(ComponentBodies.begin(), ComponentBodies.end(), Body) != ComponentBodies.end()) return;
	ComponentBodies.push_back(Body);
}

void FPhysXPhysicsScene::RemoveBody(FBodyInstance* Body)
{
	ComponentBodies.erase(std::remove(ComponentBodies.begin(), ComponentBodies.end(), Body), ComponentBodies.end());
}

FPhysicsActorHandle FPhysXPhysicsScene::CreateActor(const FActorCreationParams& Params)
{
	if (!Scene || !Physics)
	{
		return {};
	}

	PxRigidActor* NewActor = nullptr;
	const PxTransform InitialPose = ToPxTransform(Params.InitialTM);

	if (Params.bStatic)
	{
		NewActor = Physics->createRigidStatic(InitialPose);
	}
	else
	{
		PxRigidDynamic* NewDynamic = Physics->createRigidDynamic(InitialPose);
		if (NewDynamic)
		{
			NewDynamic->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, true);
			NewDynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !Params.bSimulatePhysics);
			SetDynamicCCDEnabled(NewDynamic, Params.bSimulatePhysics);

			// 랙돌 안정화 (raw 다이내믹 바디 = PhysicsAsset/랙돌 경로):
			//  - maxDepenetrationVelocity: 기본 무한대라, 스폰 시 바닥/월드와 살짝 겹친 바디를
			//    무한 속도로 밀어내 폭발한다. 유한값으로 클램프해 부드럽게 분리(가끔 폭발 차단).
			//  - solverIterationCounts: 기본 (4,1)은 관절 체인이 길면 수렴 부족 → 지터/발산.
			//    (8,2)로 상향해 조인트 안정성 확보.
			//  (값은 스케일에 따라 튜닝 여지 있음.)
			NewDynamic->setMaxDepenetrationVelocity(5.0f);
			NewDynamic->setSolverIterationCounts(8, 2);
		}
		NewActor = NewDynamic;
	}

	if (!NewActor)
	{
		return {};
	}

	if (Params.DebugName)
	{
		NewActor->setName(Params.DebugName);
	}

	NewActor->userData = Params.UserData;
	DisablePvdActorVisualization(NewActor);
	NewActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !Params.bEnableGravity);
	if (Params.bQueryOnly)
	{
		NewActor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, true);
	}

	// aggregate 가 지정되면 그 그룹에 넣는다(addActor 가 씬에도 자동 반영). 아니면 씬에 직접.
	if (PxAggregate* Aggregate = static_cast<PxAggregate*>(Params.Aggregate.Internal))
	{
		Aggregate->addActor(*NewActor);
	}
	else
	{
		Scene->addActor(*NewActor);
	}
	Owned.Actors.push_back(NewActor);   // 씬 멤버로 추적 (해제는 ReleaseActor)

	if (PxRigidDynamic* Dynamic = NewActor->is<PxRigidDynamic>())
	{
		if (Params.bStartAwake)
		{
			Dynamic->wakeUp();
		}
		else if (Params.bSimulatePhysics)
		{
			Dynamic->putToSleep();
		}
	}

	return FPhysicsActorHandle{ NewActor };
}

void FPhysXPhysicsScene::ReleaseActor(FPhysicsActorHandle Actor)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	if (!RigidActor)
	{
		return;
	}

	// 멤버 추적에서 제거
	Owned.Actors.erase(std::remove(Owned.Actors.begin(), Owned.Actors.end(), RigidActor), Owned.Actors.end());

	// aggregate 소속 actor 는 scene->removeActor 가 아니라 release() 가 aggregate/scene 양쪽에서
	// 자동 분리한다. 비소속 actor 만 명시적으로 씬에서 제거한다.
	if (RigidActor->getAggregate() == nullptr)
	{
		if (PxScene* OwningScene = RigidActor->getScene())
		{
			OwningScene->removeActor(*RigidActor);
		}
	}
	RigidActor->release();
}

bool FPhysXPhysicsScene::IsActorValid(FPhysicsActorHandle Actor) const
{
	return Actor.Internal != nullptr;
}

FPhysicsAggregateHandle FPhysXPhysicsScene::CreateAggregate(uint32 MaxActors, bool bSelfCollision)
{
	if (!Scene || !Physics || MaxActors == 0)
	{
		return {};
	}

	// PhysX 4.1: createAggregate(maxSize, enableSelfCollision). maxSize 는 actor 수 상한.
	PxAggregate* Aggregate = Physics->createAggregate(MaxActors, bSelfCollision);
	if (!Aggregate)
	{
		return {};
	}

	// 빈 상태로 씬에 추가해 두면, 이후 CreateActor 의 addActor 가 씬에도 자동 반영된다.
	Scene->addAggregate(*Aggregate);
	Owned.Aggregates.push_back(Aggregate);
	return FPhysicsAggregateHandle{ Aggregate };
}

void FPhysXPhysicsScene::ReleaseAggregate(FPhysicsAggregateHandle Handle)
{
	PxAggregate* Aggregate = static_cast<PxAggregate*>(Handle.Internal);
	if (!Aggregate)
	{
		return;
	}

	Owned.Aggregates.erase(std::remove(Owned.Aggregates.begin(), Owned.Aggregates.end(), Aggregate), Owned.Aggregates.end());

	// 호출 시점엔 소속 actor 들이 이미 ReleaseActor 로 빠져나가 빈 aggregate 다.
	// 씬에서 떼고 release. (release 는 actor 를 파괴하지 않으므로 순서 의존성 없음.)
	if (Aggregate->getScene())
	{
		Scene->removeAggregate(*Aggregate);
	}
	Aggregate->release();
}

bool FPhysXPhysicsScene::AddGeometry(FPhysicsActorHandle Actor, const FGeometryAddParams& Params)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	const bool bHasAggGeom = Params.Geometry != nullptr && Params.Geometry->GetElementCount() > 0;
	const bool bHasTriMesh = Params.CookedTriMesh != nullptr && !Params.CookedTriMesh->empty();
	if (!RigidActor || !DefaultMaterial || (!bHasAggGeom && !bHasTriMesh))
	{
		return false;
	}

	// shape 셋업 공통 람다 — aggregate/trimesh 빌더가 공유한다.
	//   Component 모드: 채널/응답/트리거/owner-UUID 필터 + shape userData = 컴포넌트.
	//   RawBody 모드(기본, 랙돌): block-all 필터 + userData = Params.UserData.
	auto SetupShape = [&](PxShape* Shape)
	{
		if (Params.ShapeSetupMode == EShapeSetupMode::Component && Params.FilterSourceComponent)
		{
			SetupComponentShape(Shape, Params.FilterSourceComponent);
		}
		else
		{
			Shape->userData = Params.UserData;
			SetupDefaultRawBodyFilterData(Shape);
		}
	};

	const PxTransform BaseLocalPose = ToPxTransform(Params.LocalTransform);
	PxShape* FirstShape = bHasTriMesh
		? AttachTriangleMeshShape(Physics, RigidActor, DefaultMaterial, *Params.CookedTriMesh,
			Params.Scale, BaseLocalPose, SetupShape)
		: AddAggregateGeometryShapes(RigidActor, DefaultMaterial, *Params.Geometry,
			Params.Scale, BaseLocalPose, SetupShape);

	if (FirstShape)
	{
		if (PxRigidDynamic* Dynamic = RigidActor->is<PxRigidDynamic>())
		{
			PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, std::max(Dynamic->getMass(), 1.0f));
		}
		LogPvdActorState("Geometry added", Scene, RigidActor);
	}

	return FirstShape != nullptr;
}

void FPhysXPhysicsScene::SetActorGlobalPose(FPhysicsActorHandle Actor, const FTransform& WorldPose)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	if (!RigidActor)
	{
		return;
	}

	RigidActor->setGlobalPose(ToPxTransform(WorldPose));
	if (PxRigidDynamic* Dynamic = RigidActor->is<PxRigidDynamic>())
	{
		Dynamic->wakeUp();
	}
}

FTransform FPhysXPhysicsScene::GetActorGlobalPose(FPhysicsActorHandle Actor) const
{
	const PxRigidActor* RigidActor = static_cast<const PxRigidActor*>(Actor.Internal);
	return RigidActor ? ToFTransform(RigidActor->getGlobalPose()) : FTransform();
}

void FPhysXPhysicsScene::SetActorKinematic(FPhysicsActorHandle Actor, bool bKinematic)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	PxRigidDynamic* Dynamic = RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
	if (!Dynamic)
	{
		return;
	}

	if (bKinematic)
	{
		SetDynamicCCDEnabled(Dynamic, false);
		Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
		Dynamic->wakeUp();
		return;
	}

	// kinematic → dynamic 전환. setKinematicTarget 으로 구동되던 바디는 "타깃 이동량/dt" 의
	// 속도를 갖는데, anim catch-up(예: 직전 래그돌로 바닥에 쓰러진 바디를 한 프레임에 현재
	// 포즈로 끌어당김)이나 점프 중 빠른 추종이면 이 implied velocity 가 폭발적으로 커진다.
	// eKINEMATIC 만 해제하면 그 속도가 그대로 남아 랙돌 솔버가 발산(폭발)한다.
	// → 전환 직전 속도를 읽어 합리적 상한으로 클램프해 다시 설정한다(모멘텀은 보존, 스파이크만 컷).
	// 상한은 정상 이동/낙하 속도(보통 < ~20 m/s)보다 충분히 크고, catch-up 스파이크(수백/s)
	// 보다 작게 잡는다. 필요 시 튜닝.
	constexpr float MaxRagdollLinearSpeed  = 20.0f;   // m/s
	constexpr float MaxRagdollAngularSpeed = 10.0f;   // rad/s

	PxVec3 LinVel = Dynamic->getLinearVelocity();
	PxVec3 AngVel = Dynamic->getAngularVelocity();

	Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
	SetDynamicCCDEnabled(Dynamic, true);

	const float LinLen = LinVel.magnitude();
	if (LinLen > MaxRagdollLinearSpeed)
	{
		LinVel *= (MaxRagdollLinearSpeed / LinLen);
	}
	const float AngLen = AngVel.magnitude();
	if (AngLen > MaxRagdollAngularSpeed)
	{
		AngVel *= (MaxRagdollAngularSpeed / AngLen);
	}
	Dynamic->setLinearVelocity(LinVel);
	Dynamic->setAngularVelocity(AngVel);
	Dynamic->wakeUp();
}

void FPhysXPhysicsScene::SetActorKinematicTarget(FPhysicsActorHandle Actor, const FTransform& WorldPose)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	PxRigidDynamic* Dynamic = RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
	if (!Dynamic)
	{
		return;
	}

	if (!(Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		SetDynamicCCDEnabled(Dynamic, false);
		Dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	}
	Dynamic->setKinematicTarget(ToPxTransform(WorldPose));
}

void FPhysXPhysicsScene::SetActorMass(FPhysicsActorHandle Actor, float Mass)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	PxRigidDynamic* Dynamic = RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
	if (!Dynamic)
	{
		return;
	}

	PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, std::max(Mass, 0.001f));
}

void FPhysXPhysicsScene::SetActorSelfCollisionGroup(FPhysicsActorHandle Actor, uint32 GroupId)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	if (!RigidActor)
	{
		return;
	}

	const PxU32 ShapeCount = RigidActor->getNbShapes();
	if (ShapeCount == 0)
	{
		return;
	}

	std::vector<PxShape*> Shapes(ShapeCount);
	RigidActor->getShapes(Shapes.data(), ShapeCount);
	for (PxShape* Shape : Shapes)
	{
		if (!Shape) continue;
		// 같은 GroupId(=owner UUID) 끼리는 KraftonFilterShader 가 충돌 무시.
		// (word3 비교는 simulation filter data 만 사용하므로 sim filter 만 갱신.)
		PxFilterData Filter = Shape->getSimulationFilterData();
		Filter.word3 = static_cast<PxU32>(GroupId);
		Shape->setSimulationFilterData(Filter);
	}
}

FPhysicsConstraintHandle FPhysXPhysicsScene::CreateConstraint(const FConstraintCreationParams& Params)
{
	PxRigidActor* Actor1 = static_cast<PxRigidActor*>(Params.Actor1.Internal);
	PxRigidActor* Actor2 = static_cast<PxRigidActor*>(Params.Actor2.Internal);
	const FConstraintSetup* Setup = Params.ConstraintSetup;
	if (!Physics || !Actor1 || !Actor2 || !Setup)
	{
		return {};
	}

	PxD6Joint* Joint = PxD6JointCreate(
		*Physics,
		Actor1,
		ToPxTransform(Params.LocalFrame1),
		Actor2,
		ToPxTransform(Params.LocalFrame2));
	if (!Joint)
	{
		return {};
	}

	constexpr float MinAngularLimit = 0.0001f;
	// 저작 한계는 도(degree). 라디안으로 변환 후 [eps, Pi] 로 클램프.
	const auto ClampAngularLimit = [](float Degrees)
	{
		const float Rad = Degrees * (PxPi / 180.0f);
		return std::max(MinAngularLimit, std::min(Rad, PxPi));
	};

	// 축별 모션 enum → PxD6Motion 매핑.
	const auto MapLinear = [](ELinearConstraintMotion M) -> PxD6Motion::Enum
	{
		switch (M)
		{
		case LCM_Free:    return PxD6Motion::eFREE;
		case LCM_Limited: return PxD6Motion::eLIMITED;
		case LCM_Locked:
		default:          return PxD6Motion::eLOCKED;
		}
	};
	const auto MapAngular = [](EAngularConstraintMotion M) -> PxD6Motion::Enum
	{
		switch (M)
		{
		case ACM_Free:    return PxD6Motion::eFREE;
		case ACM_Locked:  return PxD6Motion::eLOCKED;
		case ACM_Limited:
		default:          return PxD6Motion::eLIMITED;
		}
	};

	Joint->setMotion(PxD6Axis::eX, MapLinear(Setup->LinearXMotion));
	Joint->setMotion(PxD6Axis::eY, MapLinear(Setup->LinearYMotion));
	Joint->setMotion(PxD6Axis::eZ, MapLinear(Setup->LinearZMotion));
	Joint->setMotion(PxD6Axis::eTWIST,  MapAngular(Setup->TwistMotion));
	Joint->setMotion(PxD6Axis::eSWING1, MapAngular(Setup->Swing1Motion));
	Joint->setMotion(PxD6Axis::eSWING2, MapAngular(Setup->Swing2Motion));

	// 각도 한계는 해당 축이 Limited 일 때만 적용.
	if (Setup->TwistMotion == ACM_Limited)
	{
		const float TwistLimit = ClampAngularLimit(Setup->TwistLimit);
		Joint->setTwistLimit(PxJointAngularLimitPair(-TwistLimit, TwistLimit));
	}
	if (Setup->Swing1Motion == ACM_Limited || Setup->Swing2Motion == ACM_Limited)
	{
		Joint->setSwingLimit(PxJointLimitCone(
			ClampAngularLimit(Setup->Swing1Limit),
			ClampAngularLimit(Setup->Swing2Limit)));
	}

	// 선형 축 중 하나라도 Limited 면 거리 한계 적용(강성 0 = 강체 한계).
	const bool bAnyLinearLimited =
		(Setup->LinearXMotion == LCM_Limited) ||
		(Setup->LinearYMotion == LCM_Limited) ||
		(Setup->LinearZMotion == LCM_Limited);
	if (bAnyLinearLimited && Setup->LinearLimit > 0.0f)
	{
		Joint->setDistanceLimit(PxJointLinearLimit(Setup->LinearLimit, PxSpring(0.0f, 0.0f)));
	}

	if (Setup->DriveStiffness > 0.0f || Setup->DriveDamping > 0.0f)
	{
		Joint->setDrive(
			PxD6Drive::eSLERP,
			PxD6JointDrive(Setup->DriveStiffness, Setup->DriveDamping, PX_MAX_F32, true));
		Joint->setDrivePosition(PxTransform(PxIdentity));
	}

	Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);

	// Projection — 반복 솔버가 한 프레임에 못 잡은 조인트 분리(특히 kinematic→dynamic 전환 시
	// 본별 상대속도가 클 때)를 매 프레임 기하적으로 도로 끌어다 붙인다. 이게 없으면 한 번 벌어진
	// 바디가 복구되지 않고 누적돼 래그돌이 "터지는" 것처럼 분리된다.
	//   Linear tolerance: 이 거리(m) 이상 벌어지면 snap. Angular: 이 각(rad) 이상 틀어지면 snap.
	// 너무 작게 잡으면 지터가 생길 수 있어 적당히. (필요 시 튜닝)
	constexpr float ProjLinearTolerance  = 0.05f;            // 5 cm
	constexpr float ProjAngularTolerance = 0.2618f;          // ~15도
	Joint->setProjectionLinearTolerance(ProjLinearTolerance);
	Joint->setProjectionAngularTolerance(ProjAngularTolerance);
	Joint->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);

	Joint->userData = Params.UserData;
	Owned.Constraints.push_back(Joint);

	return FPhysicsConstraintHandle{ Joint };
}

void FPhysXPhysicsScene::ReleaseConstraint(FPhysicsConstraintHandle Constraint)
{
	PxD6Joint* Joint = static_cast<PxD6Joint*>(Constraint.Internal);
	if (!Joint)
	{
		return;
	}

	Owned.Constraints.erase(
		std::remove(Owned.Constraints.begin(), Owned.Constraints.end(), Joint),
		Owned.Constraints.end());
	Joint->release();
}

// ============================================================
// Simulation
// ============================================================

// 엔진→PhysX pre-sync teleport 임계값. post-sync 가 매 프레임 엔진=PhysX 로 맞추므로 정상 흐름에선
// 차이 ≈ 0 이지만, round-trip 부동소수 오차로 작은 차이가 매 프레임 생길 수 있어 false-positive
// teleport 를 막도록 충분히 크게 잡는다.
static constexpr float GTeleportPosThresholdSq = 1.0f;   // 1m² (1m 이상 차이 시만 teleport)
static constexpr float GTeleportRotThreshold   = 0.99f;  // ~8° 차이 시만 teleport

// 한 바디 엔진→PhysX 동기화: kinematic=타깃 푸시(setKinematicTarget — 캐릭터 캡슐 등 코드 구동),
// dynamic=임계값 초과 시에만 teleport(velocity 보존, lua spawn 등 외부 변경 흡수), static=setGlobalPose.
static void PreSyncBodyToPhysics(PxRigidActor* Actor, UPrimitiveComponent* Comp)
{
	if (!Actor || !Comp) return;

	const PxTransform NewPose = GetPxTransform(Comp);

	if (PxRigidDynamic* Dynamic = Actor->is<PxRigidDynamic>())
	{
		if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
		{
			Dynamic->setKinematicTarget(NewPose);
		}
		else
		{
			const PxTransform PxPose = Dynamic->getGlobalPose();
			const PxVec3 dp = NewPose.p - PxPose.p;
			const float DistSq = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
			const float QDot = std::abs(
				NewPose.q.x * PxPose.q.x + NewPose.q.y * PxPose.q.y +
				NewPose.q.z * PxPose.q.z + NewPose.q.w * PxPose.q.w);

			if (DistSq > GTeleportPosThresholdSq || QDot < GTeleportRotThreshold)
			{
				Dynamic->setGlobalPose(NewPose);
			}
		}
	}
	else if (Actor->is<PxRigidStatic>())
	{
		Actor->setGlobalPose(NewPose);
	}
}

// 한 바디 PhysX→엔진 동기화: dynamic·비kinematic·비sleeping 일 때만 write-back. kinematic 은 코드가
// 구동하므로 PhysX 가 덮어쓰면 안 되고, sleeping 은 변화 없음.
static void PostSyncBodyFromPhysics(PxRigidActor* Actor, UPrimitiveComponent* Comp)
{
	if (!Actor || !Comp) return;

	PxRigidDynamic* Dynamic = Actor->is<PxRigidDynamic>();
	if (!Dynamic) return;
	if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) return;
	if (Dynamic->isSleeping()) return;

	const PxTransform Pose = Dynamic->getGlobalPose();
	Comp->SetWorldLocation(ToFVector(Pose.p));
	Comp->SetRelativeRotation(ToFQuat(Pose.q));
}

void FPhysXPhysicsScene::Tick(float DeltaTime)
{
	// 편의 래퍼 — 분리 호출이 필요 없는 경로(에디터 프리뷰 등)용.
	StartSimulation(DeltaTime);
	FinishSimulation();
}

void FPhysXPhysicsScene::StartSimulation(float DeltaTime)
{
	CurrentSimDeltaTime = 0.0f;   // simulate 안 하면 FinishSimulation 이 fetchResults 를 건너뜀
	if (!Scene || DeltaTime <= 0.0f) return;

	// 어떤 이유로든 frame hitch (씬 로드 / 큰 OBJ 동기 로딩 / Alt-Tab / OS 스파이크) 가
	// 발생해도 PhysX 가 큰 dt 한 번에 적분해 차량·메테오가 콜리전을 뚫는 tunneling 사고를
	// 막기 위한 클램프. 0.1s 는 60 m/s 차량이 한 step 에 6m 이동 — 충돌 박스 내에서 풀림
	// 가능한 수준이고, 그 이상 hitch 면 게임을 느리게 진행시키더라도 안전이 우선.
	constexpr float MaxPhysicsDeltaTime = 0.1f;
	if (DeltaTime > MaxPhysicsDeltaTime)
	{
		DeltaTime = MaxPhysicsDeltaTime;
	}

	// ── Pre-simulate: Engine → PhysX Transform 동기화 ──
	// 등록된 컴포넌트 바디(per-component FBodyInstance)를 순회. 랙돌 본은 아래 BodySync 가 처리.
	for (FBodyInstance* Body : ComponentBodies)
	{
		if (!Body) continue;
		PreSyncBodyToPhysics(static_cast<PxRigidActor*>(Body->ActorHandle.Internal), Body->OwnerComponent);
	}

	// ── 등록된 sync 핸들러 Pre (랙돌 등): anim → 키네마틱 타깃. 반드시 simulate 이전 ──
	for (IPhysicsBodySync* Sync : BodySyncs)
	{
		if (Sync) Sync->PrePhysicsSimulate(DeltaTime);
	}

	if (VehicleManager)
	{
		VehicleManager->PreTick(DeltaTime);
		VehicleManager->Tick(DeltaTime);
	}

	// ── Simulate (async 윈도우 시작) ──
	// 이후 FinishSimulation 의 fetchResults 까지는 어떤 PxActor 도 건드리면 안 된다.
	// simulate() 는 워커에 디스패치만 하고 곧 반환 — 거의 0 에 가까워야 정상(킥오프 비용).
	CurrentSimDeltaTime = DeltaTime;
	{
		SCOPE_STAT_CAT("Physics.SimulateKick", "Physics");
		Scene->simulate(DeltaTime);
	}
}

void FPhysXPhysicsScene::FinishSimulation()
{
	if (!Scene || CurrentSimDeltaTime <= 0.0f) return;   // StartSimulation 이 simulate 안 함 → skip
	const float DeltaTime = CurrentSimDeltaTime;
	CurrentSimDeltaTime = 0.0f;

	// fetchResults(true) 는 시뮬 완료까지 메인 스레드를 블록한다. 이 시간이 곧 오버랩으로
	// 숨길 수 있는 시간의 상한 — TG_DuringPhysics 에 물리 비의존 작업을 얼마나 넣을지의 기준.
	{
		SCOPE_STAT_CAT("Physics.FetchBlock", "Physics");
		Scene->fetchResults(true);
	}

	// ── Sim 통계 수집 (stat physics 오버레이용) ──
	// getSimulationStatistics 는 fetchResults 완료 이후에만 유효한 직전 step 의 결과를 준다.
	// 타이밍은 위 SCOPE_STAT_CAT 가 FStatManager 스냅샷에 넣으므로 여기서는 카운트만.
#if STATS
	{
		PxSimulationStatistics SimStats;
		Scene->getSimulationStatistics(SimStats);
		PHYSICS_STATS_SET_CONSTRAINTS(SimStats.nbActiveConstraints);
		PHYSICS_STATS_SET_SIMULATING_BODIES(SimStats.nbActiveDynamicBodies);
	}
#endif

	// ── Post-simulate: PhysX → Engine Transform 동기화 ──
	// 컴포넌트 트랜스폼에 적용 → 자식 컴포넌트는 attach로 자동 따라감.
	for (FBodyInstance* Body : ComponentBodies)
	{
		if (!Body) continue;
		PostSyncBodyFromPhysics(static_cast<PxRigidActor*>(Body->ActorHandle.Internal), Body->OwnerComponent);
	}

	// ── 등록된 sync 핸들러 Post (랙돌 등): 시뮬 결과 → 본 포즈. fetchResults 이후 ──
	for (IPhysicsBodySync* Sync : BodySyncs)
	{
		if (Sync) Sync->PostPhysicsSimulate(DeltaTime);
	}

	// ── Dispatch deferred contact/trigger events ──
	// onContact / onTrigger 는 fetchResults 안에서 fire 되므로 거기서 직접 게임 핸들러를
	// 부르면 핸들러의 World->DestroyActor 등이 PhysX scene 변경 타이밍과 겹쳐 크래쉬한다.
	// 그래서 큐에만 적재했고, 이 시점(simulate/fetchResults 외부)에서 한꺼번에 dispatch.
	if (EventCallback)
	{
		EventCallback->DispatchPendingEvents();
	}
}

void FPhysXPhysicsScene::RegisterBodySync(IPhysicsBodySync* Sync)
{
	if (!Sync) return;
	if (std::find(BodySyncs.begin(), BodySyncs.end(), Sync) != BodySyncs.end()) return;
	BodySyncs.push_back(Sync);
}

void FPhysXPhysicsScene::UnregisterBodySync(IPhysicsBodySync* Sync)
{
	BodySyncs.erase(std::remove(BodySyncs.begin(), BodySyncs.end(), Sync), BodySyncs.end());
}

// ============================================================
// Force / Velocity / Mass — handle 경로 (컴포넌트 바디 + 랙돌 본 공용)
// ============================================================

// handle 에서 PxRigidDynamic 추출(static/kinematic-static 이면 nullptr). 모든 handle force/velocity
// API 가 이 게이트를 통과한다 — dynamic 이 아니면 조용히 무시한다(정적 바디에 힘 적용은 무의미).
static PxRigidDynamic* AsRigidDynamic(FPhysicsActorHandle Actor)
{
	PxRigidActor* RigidActor = static_cast<PxRigidActor*>(Actor.Internal);
	return RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
}

void FPhysXPhysicsScene::AddForce(FPhysicsActorHandle Actor, const FVector& Force)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor)) Dyn->addForce(ToPxVec3(Force));
}

void FPhysXPhysicsScene::AddForceAtLocation(FPhysicsActorHandle Actor, const FVector& Force, const FVector& WorldLocation)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor))
		PxRigidBodyExt::addForceAtPos(*Dyn, ToPxVec3(Force), ToPxVec3(WorldLocation));
}

void FPhysXPhysicsScene::AddTorque(FPhysicsActorHandle Actor, const FVector& Torque)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor)) Dyn->addTorque(ToPxVec3(Torque));
}

FVector FPhysXPhysicsScene::GetLinearVelocity(FPhysicsActorHandle Actor) const
{
	PxRigidDynamic* Dyn = AsRigidDynamic(Actor);
	return Dyn ? ToFVector(Dyn->getLinearVelocity()) : FVector(0, 0, 0);
}

void FPhysXPhysicsScene::SetLinearVelocity(FPhysicsActorHandle Actor, const FVector& Vel)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor)) Dyn->setLinearVelocity(ToPxVec3(Vel));
}

FVector FPhysXPhysicsScene::GetAngularVelocity(FPhysicsActorHandle Actor) const
{
	PxRigidDynamic* Dyn = AsRigidDynamic(Actor);
	return Dyn ? ToFVector(Dyn->getAngularVelocity()) : FVector(0, 0, 0);
}

void FPhysXPhysicsScene::SetAngularVelocity(FPhysicsActorHandle Actor, const FVector& Vel)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor)) Dyn->setAngularVelocity(ToPxVec3(Vel));
}

float FPhysXPhysicsScene::GetMass(FPhysicsActorHandle Actor) const
{
	PxRigidDynamic* Dyn = AsRigidDynamic(Actor);
	return Dyn ? Dyn->getMass() : 1.0f;
}

void FPhysXPhysicsScene::SetCenterOfMass(FPhysicsActorHandle Actor, const FVector& LocalOffset)
{
	if (PxRigidDynamic* Dyn = AsRigidDynamic(Actor))
		Dyn->setCMassLocalPose(PxTransform(ToPxVec3(LocalOffset)));
}

FVector FPhysXPhysicsScene::GetCenterOfMass(FPhysicsActorHandle Actor) const
{
	PxRigidDynamic* Dyn = AsRigidDynamic(Actor);
	return Dyn ? ToFVector(Dyn->getCMassLocalPose().p) : FVector(0, 0, 0);
}

// ============================================================
// Raycast
// ============================================================

bool FPhysXPhysicsScene::Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (!Scene) return false;

	// Channel + IgnoreActor 통합 filter.
	// shape의 queryFilterData는 SetupFilterData에서 word0=ObjectType, word1=Block 마스크.
	// 응답이 TraceChannel에 대해 Block(=word1의 해당 비트 set)인 shape만 hit으로 인정.
	// trigger flag가 set된 shape는 PhysX 측 query에서 자동 제외되므로 별도 처리 불필요.
	struct FChannelRaycastFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 TraceBit = 0;

		FChannelRaycastFilter(const AActor* InIgnoreActor, ECollisionChannel InChannel)
			: IgnoreActor(InIgnoreActor)
			, TraceBit(1u << static_cast<PxU32>(InChannel))
		{
		}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (IgnoreActor && Actor && Actor->userData == IgnoreActor)
			{
				return PxQueryHitType::eNONE;
			}

			// shape의 응답이 TraceChannel에 대해 Block인지 확인.
			// (word1[TraceChannel 비트]가 set이면 Block 응답)
			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				if ((ShapeData.word1 & TraceBit) == 0)
				{
					return PxQueryHitType::eNONE;
				}
			}

			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	PxRaycastBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FChannelRaycastFilter FilterCallback(IgnoreActor, TraceChannel);

	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist, Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor && Block.actor->userData)
	{
		OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
	}

	return true;
}

bool FPhysXPhysicsScene::RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	uint32 ObjectTypeMask, const AActor* IgnoreActor) const
{
	if (!Scene || ObjectTypeMask == 0) return false;

	// SetupFilterData (line ~322) 에서 word0 = ObjectType (채널 enum 값) 으로 set.
	// ObjectType 마스크 비트 검사로 hit 후보 필터.
	// Trigger flag shape 는 PhysX 측 query 단계에서 자동 제외.
	struct FObjectTypeRaycastFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 ObjectTypeMask = 0;

		FObjectTypeRaycastFilter(const AActor* InIgnoreActor, PxU32 InMask)
			: IgnoreActor(InIgnoreActor)
			, ObjectTypeMask(InMask)
		{
		}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (IgnoreActor && Actor && Actor->userData == IgnoreActor)
			{
				return PxQueryHitType::eNONE;
			}
			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				const PxU32 ShapeObjectBit = 1u << ShapeData.word0;
				if ((ShapeObjectBit & ObjectTypeMask) == 0)
				{
					return PxQueryHitType::eNONE;
				}
			}
			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	PxRaycastBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FObjectTypeRaycastFilter FilterCallback(IgnoreActor, ObjectTypeMask);

	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist, Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	OutHit.bHit = true;
	OutHit.Distance = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor && Block.actor->userData)
	{
		OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
	}

	return true;
}

// ============================================================
// Sweep
// ============================================================

bool FPhysXPhysicsScene::SweepCapsuleByObjectTypes(const FVector& Start, const FQuat& Rot,
	float Radius, float HalfHeight, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	uint32 ObjectTypeMask, const AActor* IgnoreActor) const
{
	if (!Scene || ObjectTypeMask == 0) return false;

	// PhysX sweep 은 unit direction 을 요구. 길이 0 이면 sweep 의미 없음.
	const float DirLen = Dir.Length();
	if (DirLen <= 1e-6f) return false;
	const FVector UnitDir = Dir * (1.0f / DirLen);

	// 캡슐 지오메트리 — PhysX 캡슐 halfHeight 는 원통부 반길이(반구 제외)다.
	// 컴포넌트 HalfHeight(반구 포함)에서 Radius 를 빼서 환산 — AddAggregateGeometryShapes
	// (FKSphylElem Length = 2*(HalfHeight-Radius)) 와 동일 규약으로 맞춘다.
	const float SafeRadius = std::max(Radius, 0.001f);
	const float CylHalfHeight = std::max(HalfHeight - SafeRadius, 0.001f);
	const PxCapsuleGeometry Capsule(SafeRadius, CylHalfHeight);

	// PhysX 캡슐 장축은 로컬 X. 엔진 캡슐 장축(컴포넌트 로컬 +Z, 중력 -Z 기준 up)에 맞추려면
	// X→Z 정렬 회전을 먼저 적용하고 그 위에 컴포넌트 월드 회전을 곱한다 (장축 = Rot * +Z).
	static const PxQuat AlignXToZ(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f));
	const PxTransform Pose(ToPxVec3(Start), ToPxQuat(Rot) * AlignXToZ);

	// RaycastByObjectTypes 와 동일한 ObjectType 마스크 필터. shape 의 word0(ObjectType) 비트가
	// 마스크에 없으면 제외, IgnoreActor 의 shape 제외. Trigger flag shape 는 PhysX 가 자동 제외.
	struct FObjectTypeSweepFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 ObjectTypeMask = 0;

		FObjectTypeSweepFilter(const AActor* InIgnoreActor, PxU32 InMask)
			: IgnoreActor(InIgnoreActor)
			, ObjectTypeMask(InMask)
		{
		}

		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape, const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (IgnoreActor && Actor && Actor->userData == IgnoreActor)
			{
				return PxQueryHitType::eNONE;
			}
			if (Shape)
			{
				const PxFilterData ShapeData = Shape->getQueryFilterData();
				const PxU32 ShapeObjectBit = 1u << ShapeData.word0;
				if ((ShapeObjectBit & ObjectTypeMask) == 0)
				{
					return PxQueryHitType::eNONE;
				}
			}
			return PxQueryHitType::eBLOCK;
		}

		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{
			return PxQueryHitType::eBLOCK;
		}
	};

	PxSweepBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FObjectTypeSweepFilter FilterCallback(IgnoreActor, ObjectTypeMask);

	// eMTD: 시작 위치에서 이미 겹쳐 있을 때(초기 침투)에도 depenetration 방향/깊이를 얻는다.
	// 없으면 초기 침투 시 normal/position 이 미정의로 반환되어 슬라이드 해소에 쓸 수 없다.
	const PxHitFlags HitFlags = PxHitFlag::eDEFAULT | PxHitFlag::eMTD;
	const bool bStatus = Scene->sweep(Capsule, Pose, ToPxVec3(UnitDir), MaxDist, Hit, HitFlags, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxSweepHit& Block = Hit.block;
	OutHit.bHit = true;

	// 초기 침투면 distance 가 음수(= 침투 깊이)로 온다 — Distance 0, PenetrationDepth 에 깊이.
	if (Block.distance < 0.0f)
	{
		OutHit.Distance = 0.0f;
		OutHit.PenetrationDepth = -Block.distance;
	}
	else
	{
		OutHit.Distance = Block.distance;
		OutHit.PenetrationDepth = 0.0f;
	}
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ImpactNormal = ToFVector(Block.normal);
	OutHit.WorldNormal = OutHit.ImpactNormal;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor && Block.actor->userData)
	{
		OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
	}

	return true;
}
