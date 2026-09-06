#include "Component/SpringArmComponent.h"

#include <cmath>

#include "Collision/Octree.h"
#include "Collision/RayUtils.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

namespace
{
    // 로컬 오프셋을 spring arm 회전 기준 월드 오프셋으로 변환한다.
    FVector RotateOffset(const FRotator &Rotation, const FVector &Offset)
    {
        return Rotation.GetForwardVector() * Offset.X + Rotation.GetRightVector() * Offset.Y + Rotation.GetUpVector() * Offset.Z;
    }

    // 지수 감쇠형 lag alpha다. Speed가 클수록 목표 지점을 더 빠르게 따라간다.
    float ComputeLagAlpha(float Speed, float DeltaTime)
    {
        if (Speed <= 0.0f)
        {
            return 1.0f;
        }

        return Clamp(1.0f - expf(-Speed * DeltaTime), 0.0f, 1.0f);
    }
} // namespace

IMPLEMENT_CLASS(USpringArmComponent, USceneComponent)

// 기본 tick 가능 상태와 spring arm 기본 파라미터를 초기화한다.
USpringArmComponent::USpringArmComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.bTickEnabled = true;
}

// 플레이 시작 시 lag 상태를 초기화하고 첫 카메라 위치를 계산한다.
void USpringArmComponent::BeginPlay()
{
    USceneComponent::BeginPlay();
    SetComponentTickEnabled(true);
    ResetLagState();
    UpdateDesiredArmLocation(bDoCollisionTest, false, false, 0.0f);
}

// 매 프레임 spring arm 끝의 실제 카메라 위치를 갱신한다.
void USpringArmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction &ThisTickFunction)
{
    USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // SpringArm은 매 프레임 카메라 붐 끝의 실제 월드 위치를 다시 계산한다.
    UpdateDesiredArmLocation(bDoCollisionTest, bEnableCameraLag, false, DeltaTime);
}

// 에디터에서 조절할 spring arm 프로퍼티를 노출한다.
void USpringArmComponent::GetEditableProperties(TArray<FPropertyDescriptor> &OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({"Target Arm Length", EPropertyType::Float, &TargetArmLength, 0.0f, 2048.0f, 0.1f});
    OutProps.push_back({"Target Offset", EPropertyType::Vec3, &TargetOffset, 0.0f, 0.0f, 0.1f});
    OutProps.push_back({"Socket Offset", EPropertyType::Vec3, &SocketOffset, 0.0f, 0.0f, 0.1f});
    OutProps.push_back({"Probe Size", EPropertyType::Float, &ProbeSize, 0.0f, 256.0f, 0.1f});
    OutProps.push_back({"Do Collision Test", EPropertyType::Bool, &bDoCollisionTest});
    OutProps.push_back({"Enable Camera Lag", EPropertyType::Bool, &bEnableCameraLag});
    OutProps.push_back({"Camera Lag Speed", EPropertyType::Float, &CameraLagSpeed, 0.0f, 64.0f, 0.1f});
}

// 에디터에서 값이 바뀐 뒤 범위를 보정하고 상태를 재계산한다.
void USpringArmComponent::PostEditProperty(const char *PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Target Arm Length") == 0)
    {
        TargetArmLength = (std::max)(0.0f, TargetArmLength);
    }
    else if (strcmp(PropertyName, "Probe Size") == 0)
    {
        ProbeSize = (std::max)(0.0f, ProbeSize);
    }
    else if (strcmp(PropertyName, "Camera Lag Speed") == 0)
    {
        CameraLagSpeed = (std::max)(0.0f, CameraLagSpeed);
    }

    ResetLagState();
    UpdateDesiredArmLocation(bDoCollisionTest, false, false, 0.0f);
}

// spring arm 설정값을 직렬화하고 로드 시 캐시를 초기화한다.
void USpringArmComponent::Serialize(FArchive &Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << TargetArmLength;
    Ar << TargetOffset;
    Ar << SocketOffset;
    Ar << ProbeSize;
    Ar << CameraLagSpeed;
    Ar << bDoCollisionTest;
    Ar << bEnableCameraLag;

    if (Ar.IsLoading())
    {
        ResetLagState();
    }
}

// 현재 spring arm이 따를 목표 회전을 반환한다.
FRotator USpringArmComponent::GetTargetRotation() const { return GetComponentRotation(); }

// target과 camera 사이 거리(arm 길이)를 설정한다.
void USpringArmComponent::SetTargetArmLength(float InTargetArmLength)
{
    TargetArmLength = (std::max)(0.0f, InTargetArmLength);
    ResetLagState();
}

// 부모 기준 target 피벗 오프셋을 설정한다.
void USpringArmComponent::SetTargetOffset(const FVector &InTargetOffset)
{
    TargetOffset = InTargetOffset;
    ResetLagState();
}

// 카메라 끝점에 추가로 더할 소켓 오프셋을 설정한다.
void USpringArmComponent::SetSocketOffset(const FVector &InSocketOffset)
{
    SocketOffset = InSocketOffset;
    ResetLagState();
}

// 충돌 검사에 사용할 probe 반경을 설정한다.
void USpringArmComponent::SetProbeSize(float InProbeSize) { ProbeSize = (std::max)(0.0f, InProbeSize); }

// 카메라 위치 lag 사용 여부를 설정한다.
void USpringArmComponent::SetEnableCameraLag(bool bInEnableCameraLag)
{
    bEnableCameraLag = bInEnableCameraLag;
    ResetLagState();
}

// 카메라가 목표 위치를 따라가는 lag 속도를 설정한다.
void USpringArmComponent::SetCameraLagSpeed(float InCameraLagSpeed) { CameraLagSpeed = (std::max)(0.0f, InCameraLagSpeed); }

// 충돌 시 hit 위치와 원래 desired 위치를 섞을 수 있는 기본 블렌딩 동작이다.
FVector USpringArmComponent::BlendLocations(const FVector &DesiredArmLocation, const FVector &TraceHitLocation, bool bHitSomething,
                                            float DeltaTime) const
{
    (void)DeltaTime;
    return bHitSomething ? TraceHitLocation : DesiredArmLocation;
}

// target 계산 -> desired 위치 계산 -> sweep -> 충돌 보정 -> lag 적용 순서로 카메라 끝점을 갱신한다.
void USpringArmComponent::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime)
{
    (void)bDoRotationLag;

    USceneComponent *ParentComponent = GetParent();
    if (!ParentComponent && GetOwner())
    {
        ParentComponent = GetOwner()->GetRootComponent();
    }

    const FVector  PivotLocation = ParentComponent ? ParentComponent->GetWorldLocation() : GetWorldLocation();
    const FRotator TargetRotation = GetTargetRotation();
    // 1. 부모 기준 target 위치를 계산한다.
    const FVector  TargetLocation = PivotLocation + RotateOffset(TargetRotation, TargetOffset);

    // 2. target에서 arm length만큼 뒤로 뺀 desired camera 위치를 만든다.
    FVector DesiredArmLocation = TargetLocation - TargetRotation.GetForwardVector() * TargetArmLength;
    DesiredArmLocation += RotateOffset(TargetRotation, SocketOffset);
    UnfixedCameraPosition = DesiredArmLocation;

    FVector FinalCameraLocation = DesiredArmLocation;
    bIsCollisionFixApplied = false;

    if (bDoTrace && bDoCollisionTest)
    {
        FVector TraceHitLocation = DesiredArmLocation;
        // 3. target -> desired 경로 사이 장애물을 검사한다.
        if (SweepCameraPath(TargetLocation, DesiredArmLocation, ProbeSize, TraceHitLocation))
        {
            // 4. 충돌 시 카메라를 hit 지점 앞으로 당겨서 벽 관통을 막는다.
            FinalCameraLocation = BlendLocations(DesiredArmLocation, TraceHitLocation, true, DeltaTime);
            bIsCollisionFixApplied = true;
        }
    }

    if (bDoLocationLag)
    {
        if (!bHasPreviousCameraLocation)
        {
            PreviousCameraLocation = FinalCameraLocation;
            bHasPreviousCameraLocation = true;
        }
        else
        {
            // 5. 최종 위치를 바로 쓰지 않고 lag를 적용해 부드럽게 따라간다.
            const float LagAlpha = ComputeLagAlpha(CameraLagSpeed, DeltaTime);
            FinalCameraLocation = FVector::Lerp(PreviousCameraLocation, FinalCameraLocation, LagAlpha);
            PreviousCameraLocation = FinalCameraLocation;
        }
    }
    else
    {
        PreviousCameraLocation = FinalCameraLocation;
        bHasPreviousCameraLocation = true;
    }

    SetWorldLocation(FinalCameraLocation);
}

// 현재 엔진에는 shape sweep API가 없어서, probe 반경만큼 팽창한 AABB로 경로를 검사한다.
bool USpringArmComponent::SweepCameraPath(const FVector &Start, const FVector &End, float Radius, FVector &OutHitLocation) const
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector SweepDelta = End - Start;
    const float   SweepLength = SweepDelta.Length();
    if (SweepLength <= 1.0e-4f)
    {
        OutHitLocation = End;
        return false;
    }

    const FVector SweepDirection = SweepDelta / SweepLength;
    const FVector Expand(Radius, Radius, Radius);

    FBoundingBox QueryBounds(FVector((std::min)(Start.X, End.X), (std::min)(Start.Y, End.Y), (std::min)(Start.Z, End.Z)) - Expand,
                             FVector((std::max)(Start.X, End.X), (std::max)(Start.Y, End.Y), (std::max)(Start.Z, End.Z)) + Expand);

    TArray<UPrimitiveComponent *> Candidates;
    if (const FOctree *Octree = World->GetOctree())
    {
        // 매 프레임 전체 primitive를 훑지 않도록 sweep 구간을 감싸는 AABB로 후보를 먼저 줄인다.
        Octree->QueryAABB(QueryBounds, Candidates);
    }
    else
    {
        for (AActor *Actor : World->GetActors())
        {
            if (!Actor)
            {
                continue;
            }

            for (UPrimitiveComponent *Primitive : Actor->GetPrimitiveComponents())
            {
                if (Primitive)
                {
                    Candidates.push_back(Primitive);
                }
            }
        }
    }

    const FRay SweepRay{Start, SweepDirection};
    float      ClosestHitDistance = SweepLength;
    bool       bHitSomething = false;

    for (UPrimitiveComponent *Primitive : Candidates)
    {
        if (ShouldIgnoreCollision(Primitive))
        {
            continue;
        }

        // Probe sphere sweep을 단순화하기 위해 primitive bounds를 probe 반경만큼 확장해 검사한다.
        FBoundingBox ExpandedBounds = Primitive->GetWorldBoundingBox();
        ExpandedBounds.Min -= Expand;
        ExpandedBounds.Max += Expand;

        float HitTMin = 0.0f;
        float HitTMax = 0.0f;
        if (!FRayUtils::IntersectRayAABB(SweepRay, ExpandedBounds.Min, ExpandedBounds.Max, HitTMin, HitTMax))
        {
            continue;
        }

        if (HitTMax < 0.0f || HitTMin > SweepLength)
        {
            continue;
        }

        const float HitDistance = Clamp(HitTMin, 0.0f, SweepLength);
        if (HitDistance < ClosestHitDistance)
        {
            ClosestHitDistance = HitDistance;
            bHitSomething = true;
        }
    }

    if (!bHitSomething)
    {
        return false;
    }

    const float CameraCollisionOffset = 0.05f;
    OutHitLocation = Start + SweepDirection * Clamp(ClosestHitDistance - CameraCollisionOffset, 0.0f, SweepLength);
    return true;
}

// 자기 자신이나 비활성 primitive처럼 무시해야 할 충돌 대상을 판정한다.
bool USpringArmComponent::ShouldIgnoreCollision(const UPrimitiveComponent *Primitive) const
{
    if (!Primitive || !Primitive->IsVisible() || !Primitive->IsCollisionEnabled())
    {
        return true;
    }

    return Primitive->GetOwner() == GetOwner();
}

// lag와 충돌 상태 캐시를 초기화한다.
void USpringArmComponent::ResetLagState()
{
    PreviousCameraLocation = FVector::ZeroVector;
    UnfixedCameraPosition = FVector::ZeroVector;
    bHasPreviousCameraLocation = false;
    bIsCollisionFixApplied = false;
}
