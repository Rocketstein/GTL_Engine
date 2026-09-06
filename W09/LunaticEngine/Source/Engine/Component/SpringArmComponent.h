#pragma once

#include "Component/SceneComponent.h"

class UPrimitiveComponent;

class USpringArmComponent : public USceneComponent
{
  public:
    DECLARE_CLASS(USpringArmComponent, USceneComponent)

    // 기본 tick 가능 상태와 spring arm 기본 파라미터를 초기화한다.
    USpringArmComponent();

    // 플레이 시작 시 lag 상태를 초기화하고 첫 카메라 위치를 계산한다.
    void BeginPlay() override;
    // 매 프레임 spring arm 끝의 실제 카메라 위치를 갱신한다.
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction &ThisTickFunction) override;
    // 에디터에서 조절할 spring arm 프로퍼티를 노출한다.
    void GetEditableProperties(TArray<FPropertyDescriptor> &OutProps) override;
    // 에디터에서 값이 바뀐 뒤 범위를 보정하고 상태를 재계산한다.
    void PostEditProperty(const char *PropertyName) override;
    // spring arm 설정값을 직렬화하고 로드 시 캐시를 초기화한다.
    void Serialize(FArchive &Ar) override;

    // 현재 spring arm이 따를 목표 회전을 반환한다.
    FRotator GetTargetRotation() const;
    // 충돌 보정 전, 원래 의도했던 카메라 위치를 반환한다.
    FVector  GetUnfixedCameraPosition() const { return UnfixedCameraPosition; }
    // 현재 프레임에 충돌 보정이 적용됐는지 반환한다.
    bool     IsCollisionFixApplied() const { return bIsCollisionFixApplied; }

    // target과 camera 사이 거리(arm 길이)를 설정한다.
    void SetTargetArmLength(float InTargetArmLength);
    // 부모 기준 target 피벗 오프셋을 설정한다.
    void SetTargetOffset(const FVector &InTargetOffset);
    // 카메라 끝점에 추가로 더할 소켓 오프셋을 설정한다.
    void SetSocketOffset(const FVector &InSocketOffset);
    // 충돌 검사에 사용할 probe 반경을 설정한다.
    void SetProbeSize(float InProbeSize);
    // 카메라 충돌 테스트 사용 여부를 설정한다.
    void SetDoCollisionTest(bool bInDoCollisionTest) { bDoCollisionTest = bInDoCollisionTest; }
    // 카메라 위치 lag 사용 여부를 설정한다.
    void SetEnableCameraLag(bool bInEnableCameraLag);
    // 카메라가 목표 위치를 따라가는 lag 속도를 설정한다.
    void SetCameraLagSpeed(float InCameraLagSpeed);

  protected:
    // 충돌 시 hit 위치와 원래 desired 위치를 섞을 수 있는 확장 포인트다.
    FVector BlendLocations(const FVector &DesiredArmLocation, const FVector &TraceHitLocation, bool bHitSomething, float DeltaTime) const;
    // target 계산 -> desired 위치 계산 -> sweep -> 충돌 보정 -> lag 적용 순서로 카메라 끝점을 갱신한다.
    void    UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime);
    // 현재 엔진에는 shape sweep API가 없어서, probe 반경만큼 팽창한 AABB로 경로를 검사한다.
    bool    SweepCameraPath(const FVector &Start, const FVector &End, float Radius, FVector &OutHitLocation) const;
    // 자기 자신이나 비활성 primitive처럼 무시해야 할 충돌 대상을 판정한다.
    bool    ShouldIgnoreCollision(const UPrimitiveComponent *Primitive) const;
    // lag와 충돌 상태 캐시를 초기화한다.
    void    ResetLagState();
    // details 패널에서 tick enable 프로퍼티를 노출한다.
    bool    ShouldExposeTickEnabledProperty() const override { return true; }

  private:
    float   TargetArmLength = 8.0f;
    FVector TargetOffset = FVector(0.0f, 0.0f, 5.0f);
    FVector SocketOffset = FVector::ZeroVector;
    float   ProbeSize = 0.75f;
    float   CameraLagSpeed = 10.0f;
    bool    bDoCollisionTest = true;
    bool    bEnableCameraLag = true;

    FVector PreviousCameraLocation = FVector::ZeroVector;
    FVector UnfixedCameraPosition = FVector::ZeroVector;
    bool    bHasPreviousCameraLocation = false;
    bool    bIsCollisionFixApplied = false;
};
