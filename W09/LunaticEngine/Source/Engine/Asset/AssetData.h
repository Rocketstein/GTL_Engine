#pragma once

#include "Core/CoreTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/Object.h"

class FArchive;

class UAssetData : public UObject
{
  public:
    DECLARE_CLASS(UAssetData, UObject)

    void Serialize(FArchive &Ar) override;
};

struct FCameraModifierCommonAssetDesc
{
    uint8 Priority = 128;
    float AlphaInTime = 0.05f;
    float AlphaOutTime = 0.10f;
    bool  bStartDisabled = false;
};

struct FAssetBezierCurve
{
    float ControlPoints[4] = {0.25f, 0.0f, 0.75f, 1.0f};

    float Evaluate(float NormalizedTime) const;
    void  ResetLinear();
    void  Serialize(FArchive &Ar);
};

struct FCameraShakePatternCurves
{
    FAssetBezierCurve TranslationX;
    FAssetBezierCurve TranslationY;
    FAssetBezierCurve TranslationZ;
    FAssetBezierCurve RotationX;
    FAssetBezierCurve RotationY;
    FAssetBezierCurve RotationZ;

    void ResetLinear();
    void CopyFrom(const FAssetBezierCurve &SourceCurve);
    void Serialize(FArchive &Ar);
};

struct FCameraShakeModifierAssetDesc
{
    uint64                         EditorId = 0;
    FString                        Name = "CameraShake";
    FCameraModifierCommonAssetDesc Common;
    float                          Duration = 0.5f;
    float                          Intensity = 1.0f;
    float                          Frequency = 20.0f;
    FVector                        LocationAmplitude = FVector(0.0f, 0.0f, 5.0f);
    FRotator                       RotationAmplitude = FRotator(1.0f, 1.0f, 0.0f);
    bool                           bUseCurves = true;
    FCameraShakePatternCurves      Curves;
};

class UCameraModifierStackAssetData : public UAssetData
{
  public:
    DECLARE_CLASS(UCameraModifierStackAssetData, UAssetData)

    TArray<FCameraShakeModifierAssetDesc> CameraShakes;

    void Serialize(FArchive &Ar) override;
    void EnsureValidEditorIds();
};

void   SerializeCameraModifierCommonAssetDesc(FArchive &Ar, FCameraModifierCommonAssetDesc &Desc);
void   SerializeCameraShakeModifierAssetDesc(FArchive &Ar, FCameraShakeModifierAssetDesc &Desc);
uint64 GenerateAssetEditorId();
