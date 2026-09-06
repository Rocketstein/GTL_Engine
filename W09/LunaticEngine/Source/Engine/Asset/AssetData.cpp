#include "Engine/Asset/AssetData.h"

#include "Engine/Asset/AssetFileSerializer.h"
#include "Object/ObjectFactory.h"
#include "Engine/Serialization/Archive.h"
#include <algorithm>
#include <chrono>

IMPLEMENT_CLASS(UAssetData, UObject)
IMPLEMENT_CLASS(UCameraModifierStackAssetData, UAssetData)

namespace
{
    float Clamp01(float Value) { return (std::max)(0.0f, (std::min)(Value, 1.0f)); }

    float CubicBezier1D(float P0, float P1, float P2, float P3, float T)
    {
        const float U = 1.0f - T;
        return U * U * U * P0 + 3.0f * U * U * T * P1 + 3.0f * U * T * T * P2 + T * T * T * P3;
    }
} // namespace

uint64 GenerateAssetEditorId()
{
    static uint64 Counter = 1;
    const uint64  TimeSeed = static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return TimeSeed ^ (Counter++ << 32);
}

void UAssetData::Serialize(FArchive &Ar) { UObject::Serialize(Ar); }

float FAssetBezierCurve::Evaluate(float NormalizedTime) const
{
    const float TargetX = Clamp01(NormalizedTime);

    float Low = 0.0f;
    float High = 1.0f;
    for (int32 Iteration = 0; Iteration < 16; ++Iteration)
    {
        const float Mid = (Low + High) * 0.5f;
        const float X = CubicBezier1D(0.0f, ControlPoints[0], ControlPoints[2], 1.0f, Mid);
        if (X < TargetX)
        {
            Low = Mid;
        }
        else
        {
            High = Mid;
        }
    }

    const float T = (Low + High) * 0.5f;
    return CubicBezier1D(0.0f, ControlPoints[1], ControlPoints[3], 1.0f, T);
}

void FAssetBezierCurve::ResetLinear()
{
    ControlPoints[0] = 0.0f;
    ControlPoints[1] = 0.0f;
    ControlPoints[2] = 1.0f;
    ControlPoints[3] = 1.0f;
}

void FAssetBezierCurve::Serialize(FArchive &Ar)
{
    for (float &Value : ControlPoints)
    {
        Ar << Value;
    }
}

void FCameraShakePatternCurves::ResetLinear()
{
    TranslationX.ResetLinear();
    TranslationY.ResetLinear();
    TranslationZ.ResetLinear();
    RotationX.ResetLinear();
    RotationY.ResetLinear();
    RotationZ.ResetLinear();
}

void FCameraShakePatternCurves::CopyFrom(const FAssetBezierCurve &SourceCurve)
{
    TranslationX = SourceCurve;
    TranslationY = SourceCurve;
    TranslationZ = SourceCurve;
    RotationX = SourceCurve;
    RotationY = SourceCurve;
    RotationZ = SourceCurve;
}

void FCameraShakePatternCurves::Serialize(FArchive &Ar)
{
    TranslationX.Serialize(Ar);
    TranslationY.Serialize(Ar);
    TranslationZ.Serialize(Ar);
    RotationX.Serialize(Ar);
    RotationY.Serialize(Ar);
    RotationZ.Serialize(Ar);
}

void SerializeCameraModifierCommonAssetDesc(FArchive &Ar, FCameraModifierCommonAssetDesc &Desc)
{
    Ar << Desc.Priority;
    Ar << Desc.AlphaInTime;
    Ar << Desc.AlphaOutTime;
    Ar << Desc.bStartDisabled;
}

void SerializeCameraShakeModifierAssetDesc(FArchive &Ar, FCameraShakeModifierAssetDesc &Desc)
{
    Ar << Desc.EditorId;
    Ar << Desc.Name;

    SerializeCameraModifierCommonAssetDesc(Ar, Desc.Common);

    Ar << Desc.Duration;
    Ar << Desc.Intensity;
    Ar << Desc.Frequency;
    Ar << Desc.LocationAmplitude;
    Ar << Desc.RotationAmplitude;

    const uint32 AssetVersion = FAssetFileSerializer::GetCurrentAssetSerializationVersion();
    if (Ar.IsLoading() && AssetVersion < 2)
    {
        bool              bUseIntensityCurve = true;
        FAssetBezierCurve IntensityOverTime;
        Ar << bUseIntensityCurve;
        IntensityOverTime.Serialize(Ar);

        Desc.bUseCurves = bUseIntensityCurve;
        Desc.Curves.CopyFrom(IntensityOverTime);
    }
    else
    {
        Ar << Desc.bUseCurves;
        Desc.Curves.Serialize(Ar);
    }
}

void UCameraModifierStackAssetData::Serialize(FArchive &Ar)
{
    UAssetData::Serialize(Ar);

    uint32 CameraShakeCount = static_cast<uint32>(CameraShakes.size());
    Ar << CameraShakeCount;

    if (Ar.IsLoading())
    {
        CameraShakes.resize(CameraShakeCount);
    }

    for (uint32 Index = 0; Index < CameraShakeCount; ++Index)
    {
        SerializeCameraShakeModifierAssetDesc(Ar, CameraShakes[Index]);
    }

    if (Ar.IsLoading())
    {
        EnsureValidEditorIds();
    }
}

void UCameraModifierStackAssetData::EnsureValidEditorIds()
{
    for (FCameraShakeModifierAssetDesc &Desc : CameraShakes)
    {
        if (Desc.EditorId == 0)
        {
            Desc.EditorId = GenerateAssetEditorId();
        }
        if (Desc.Name.empty())
        {
            Desc.Name = "CameraShake";
        }
    }
}
