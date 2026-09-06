#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Camera/Shakes/SequenceCameraShakePattern.h"

class UCameraShakeBase;
class UObject;

enum class ECameraShakeAssetPatternType : uint8
{
    WaveOscillator,
    PerlinNoise,
    Sequence,
};

enum class ECameraShakeAssetWaveInitialOffsetType : uint8
{
    Random,
    Zero,
};

struct FCameraShakeAssetOscillator
{
    float Amplitude = 0.f;
    float Frequency = 1.f;
    ECameraShakeAssetWaveInitialOffsetType InitialOffsetType = ECameraShakeAssetWaveInitialOffsetType::Random;
};

struct FCameraShakeAssetDefinition
{
    int32 Version = 1;
    FString ShakeClass = "UCameraShakeBase";
    bool bSingleInstance = false;

    ECameraShakeAssetPatternType PatternType = ECameraShakeAssetPatternType::WaveOscillator;
    FString RootPatternClass = "UWaveOscillatorCameraShakePattern";

    float Duration = 0.6f;
    float BlendInTime = 0.05f;
    float BlendOutTime = 0.15f;
    float PlayRate = 1.f;

    float LocationAmplitudeMultiplier = 1.f;
    float LocationFrequencyMultiplier = 1.f;
    float RotationAmplitudeMultiplier = 1.f;
    float RotationFrequencyMultiplier = 1.f;

    FCameraShakeAssetOscillator X { 20.f, 18.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator Y { 10.f, 22.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator Z { 6.f, 16.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator Pitch { 1.5f, 20.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator Yaw { 1.f, 18.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator Roll { 1.f, 24.f, ECameraShakeAssetWaveInitialOffsetType::Random };
    FCameraShakeAssetOscillator FOV { 0.f, 1.f, ECameraShakeAssetWaveInitialOffsetType::Zero };

    FSequenceCameraShakeCurves Curves;
};

struct FCameraShakeAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

class FCameraShakeAssetManager : public TSingleton<FCameraShakeAssetManager>
{
    friend class TSingleton<FCameraShakeAssetManager>;

public:
    TOptional<FCameraShakeAssetDefinition> LoadAsset(const FString& Path) const;
    bool SaveAsset(const FString& Path, const FCameraShakeAssetDefinition& Definition) const;
    TArray<FCameraShakeAssetListItem> ScanAssets() const;

    UCameraShakeBase* CreateShakeInstance(const FString& Path, UObject* Outer = nullptr) const;
    UCameraShakeBase* CreateShakeInstance(const FCameraShakeAssetDefinition& Definition, UObject* Outer = nullptr, const FString& SourcePath = FString()) const;

    FString NormalizeAssetPath(const FString& Path) const;
    static FString GetDefaultAssetDirectory();

private:
    FCameraShakeAssetManager() = default;

    TOptional<FCameraShakeAssetDefinition> ParseDefinition(const FString& Path, const FString& Content) const;
};
