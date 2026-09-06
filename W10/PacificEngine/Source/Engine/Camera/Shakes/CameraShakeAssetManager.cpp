#include "Camera/Shakes/CameraShakeAssetManager.h"

#include "Camera/Shakes/CameraShakeBase.h"
#include "Camera/Shakes/CameraShakePattern.h"
#include "Camera/Shakes/PerlinNoiseCameraShakePattern.h"
#include "Camera/Shakes/SequenceCameraShakePattern.h"
#include "Camera/Shakes/SimpleCameraShakePattern.h"
#include "Camera/Shakes/WaveOscillatorCameraShakePattern.h"
#include "Core/Logging/LogMacros.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "SimpleJSON/json.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{
namespace ShakeKeys
{
constexpr const char* Version = "Version";
constexpr const char* ShakeClass = "ShakeClass";
constexpr const char* bSingleInstance = "bSingleInstance";
constexpr const char* RootPattern = "RootPattern";
constexpr const char* Class = "Class";
constexpr const char* Duration = "Duration";
constexpr const char* BlendInTime = "BlendInTime";
constexpr const char* BlendOutTime = "BlendOutTime";
constexpr const char* PlayRate = "PlayRate";
constexpr const char* Curves = "Curves";
constexpr const char* Time = "Time";
constexpr const char* Value = "Value";
constexpr const char* ArriveTime = "ArriveTime";
constexpr const char* ArriveValue = "ArriveValue";
constexpr const char* LeaveTime = "LeaveTime";
constexpr const char* LeaveValue = "LeaveValue";
constexpr const char* LocationAmplitudeMultiplier = "LocationAmplitudeMultiplier";
constexpr const char* LocationFrequencyMultiplier = "LocationFrequencyMultiplier";
constexpr const char* RotationAmplitudeMultiplier = "RotationAmplitudeMultiplier";
constexpr const char* RotationFrequencyMultiplier = "RotationFrequencyMultiplier";
constexpr const char* Amplitude = "Amplitude";
constexpr const char* Frequency = "Frequency";
constexpr const char* InitialOffsetType = "InitialOffsetType";
constexpr const char* X = "X";
constexpr const char* Y = "Y";
constexpr const char* Z = "Z";
constexpr const char* Pitch = "Pitch";
constexpr const char* Yaw = "Yaw";
constexpr const char* Roll = "Roll";
constexpr const char* FOV = "FOV";
} // namespace ShakeKeys

constexpr const char* WavePatternClassName = "UWaveOscillatorCameraShakePattern";
constexpr const char* PerlinPatternClassName = "UPerlinNoiseCameraShakePattern";
constexpr const char* SequencePatternClassName = "USequenceCameraShakePattern";

FString ToLowerAscii(FString Value)
{
    std::transform(Value.begin(), Value.end(), Value.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    return Value;
}

float ReadFloat(const json::JSON& Object, const char* Key, float DefaultValue)
{
    if (!Object.hasKey(Key))
    {
        return DefaultValue;
    }

    const json::JSON& Value = Object.at(Key);
    if (Value.JSONType() == json::JSON::Class::Floating)
    {
        return static_cast<float>(Value.ToFloat());
    }
    if (Value.JSONType() == json::JSON::Class::Integral)
    {
        return static_cast<float>(Value.ToInt());
    }
    return DefaultValue;
}

int32 ReadInt(const json::JSON& Object, const char* Key, int32 DefaultValue)
{
    if (!Object.hasKey(Key))
    {
        return DefaultValue;
    }

    const json::JSON& Value = Object.at(Key);
    if (Value.JSONType() == json::JSON::Class::Integral)
    {
        return static_cast<int32>(Value.ToInt());
    }
    if (Value.JSONType() == json::JSON::Class::Floating)
    {
        return static_cast<int32>(Value.ToFloat());
    }
    return DefaultValue;
}

bool ReadBool(const json::JSON& Object, const char* Key, bool DefaultValue)
{
    if (!Object.hasKey(Key))
    {
        return DefaultValue;
    }

    const json::JSON& Value = Object.at(Key);
    return Value.JSONType() == json::JSON::Class::Boolean ? Value.ToBool() : DefaultValue;
}

FString ReadString(const json::JSON& Object, const char* Key, const FString& DefaultValue)
{
    if (!Object.hasKey(Key))
    {
        return DefaultValue;
    }

    const json::JSON& Value = Object.at(Key);
    return Value.JSONType() == json::JSON::Class::String ? Value.ToString() : DefaultValue;
}

ECameraShakeAssetWaveInitialOffsetType ParseInitialOffsetType(const FString& Value)
{
    return Value == "Zero" ? ECameraShakeAssetWaveInitialOffsetType::Zero : ECameraShakeAssetWaveInitialOffsetType::Random;
}

const char* ToString(ECameraShakeAssetWaveInitialOffsetType Value)
{
    return Value == ECameraShakeAssetWaveInitialOffsetType::Zero ? "Zero" : "Random";
}

ECameraShakeAssetPatternType PatternTypeFromClass(const FString& ClassName)
{
    if (ClassName == PerlinPatternClassName)
    {
        return ECameraShakeAssetPatternType::PerlinNoise;
    }
    if (ClassName == SequencePatternClassName)
    {
        return ECameraShakeAssetPatternType::Sequence;
    }
    return ECameraShakeAssetPatternType::WaveOscillator;
}

const char* PatternClassFromType(ECameraShakeAssetPatternType Type)
{
    switch (Type)
    {
    case ECameraShakeAssetPatternType::PerlinNoise:
        return PerlinPatternClassName;
    case ECameraShakeAssetPatternType::Sequence:
        return SequencePatternClassName;
    case ECameraShakeAssetPatternType::WaveOscillator:
    default:
        return WavePatternClassName;
    }
}

bool IsSupportedPatternClass(const FString& ClassName)
{
    return ClassName == WavePatternClassName
        || ClassName == PerlinPatternClassName
        || ClassName == SequencePatternClassName;
}

void ReadOscillator(const json::JSON& RootPattern, const char* Key, FCameraShakeAssetOscillator& InOutOscillator)
{
    if (!RootPattern.hasKey(Key))
    {
        return;
    }

    const json::JSON& Object = RootPattern.at(Key);
    if (Object.JSONType() != json::JSON::Class::Object)
    {
        return;
    }

    InOutOscillator.Amplitude = ReadFloat(Object, ShakeKeys::Amplitude, InOutOscillator.Amplitude);
    InOutOscillator.Frequency = ReadFloat(Object, ShakeKeys::Frequency, InOutOscillator.Frequency);
    InOutOscillator.InitialOffsetType = ParseInitialOffsetType(ReadString(Object, ShakeKeys::InitialOffsetType, ToString(InOutOscillator.InitialOffsetType)));
}

json::JSON WriteOscillator(const FCameraShakeAssetOscillator& Oscillator)
{
    json::JSON Object = json::Object();
    Object[ShakeKeys::Amplitude] = Oscillator.Amplitude;
    Object[ShakeKeys::Frequency] = Oscillator.Frequency;
    Object[ShakeKeys::InitialOffsetType] = ToString(Oscillator.InitialOffsetType);
    return Object;
}

bool ReadSequenceKey(const json::JSON& Object, FSequenceCameraShakeKey& OutKey)
{
    if (Object.JSONType() != json::JSON::Class::Object)
    {
        return false;
    }

    OutKey.Time = ReadFloat(Object, ShakeKeys::Time, OutKey.Time);
    OutKey.Value = ReadFloat(Object, ShakeKeys::Value, OutKey.Value);
    OutKey.ArriveTime = ReadFloat(Object, ShakeKeys::ArriveTime, OutKey.ArriveTime);
    OutKey.ArriveValue = ReadFloat(Object, ShakeKeys::ArriveValue, OutKey.ArriveValue);
    OutKey.LeaveTime = ReadFloat(Object, ShakeKeys::LeaveTime, OutKey.LeaveTime);
    OutKey.LeaveValue = ReadFloat(Object, ShakeKeys::LeaveValue, OutKey.LeaveValue);
    return true;
}

void ReadSequenceCurve(const json::JSON& CurvesObject,
                       const char* ChannelName,
                       FSequenceCameraShakeCurve& OutCurve,
                       float Duration,
                       const FString& Path)
{
    OutCurve.clear();
    if (!CurvesObject.hasKey(ChannelName))
    {
        UE_LOG(CameraShake, Warning, "Sequence camera shake curve '%s' is missing in %s.", ChannelName, Path.c_str());
        return;
    }

    const json::JSON& CurveArray = CurvesObject.at(ChannelName);
    if (CurveArray.JSONType() != json::JSON::Class::Array)
    {
        UE_LOG(CameraShake, Warning, "Sequence camera shake curve '%s' must be an array in %s.", ChannelName, Path.c_str());
        return;
    }

    for (int Index = 0; Index < CurveArray.length(); ++Index)
    {
        FSequenceCameraShakeKey Key;
        if (!ReadSequenceKey(CurveArray.at(static_cast<unsigned>(Index)), Key))
        {
            UE_LOG(CameraShake, Warning, "Skipping malformed sequence key %d in curve '%s' from %s.", Index, ChannelName, Path.c_str());
            continue;
        }
        OutCurve.push_back(Key);
    }

    USequenceCameraShakePattern::NormalizeCurve(OutCurve, Duration);
}

json::JSON WriteSequenceCurve(const FSequenceCameraShakeCurve& Curve)
{
    json::JSON Array = json::Array();
    for (size_t Index = 0; Index < Curve.size(); ++Index)
    {
        const FSequenceCameraShakeKey& Key = Curve[Index];
        json::JSON Object = json::Object();
        Object[ShakeKeys::Time] = Key.Time;
        Object[ShakeKeys::Value] = Key.Value;
        Object[ShakeKeys::ArriveTime] = Key.ArriveTime;
        Object[ShakeKeys::ArriveValue] = Key.ArriveValue;
        Object[ShakeKeys::LeaveTime] = Key.LeaveTime;
        Object[ShakeKeys::LeaveValue] = Key.LeaveValue;
        Array[static_cast<unsigned>(Index)] = Object;
    }
    return Array;
}

void ReadSequenceCurves(const json::JSON& Pattern, FCameraShakeAssetDefinition& Definition, const FString& Path)
{
    if (!Pattern.hasKey(ShakeKeys::Curves) || Pattern.at(ShakeKeys::Curves).JSONType() != json::JSON::Class::Object)
    {
        UE_LOG(CameraShake, Warning, "Sequence camera shake asset missing Curves object: %s", Path.c_str());
        return;
    }

    const json::JSON& CurvesObject = Pattern.at(ShakeKeys::Curves);
    for (size_t ChannelIndex = 0; ChannelIndex < SequenceCameraShakeChannelCount; ++ChannelIndex)
    {
        const ESequenceCameraShakeChannel Channel = static_cast<ESequenceCameraShakeChannel>(ChannelIndex);
        ReadSequenceCurve(CurvesObject,
                          GetSequenceCameraShakeChannelName(Channel),
                          Definition.Curves[ChannelIndex],
                          Definition.Duration,
                          Path);
    }
}

json::JSON WriteSequenceCurves(const FSequenceCameraShakeCurves& Curves)
{
    json::JSON Object = json::Object();
    for (size_t ChannelIndex = 0; ChannelIndex < SequenceCameraShakeChannelCount; ++ChannelIndex)
    {
        const ESequenceCameraShakeChannel Channel = static_cast<ESequenceCameraShakeChannel>(ChannelIndex);
        Object[GetSequenceCameraShakeChannelName(Channel)] = WriteSequenceCurve(Curves[ChannelIndex]);
    }
    return Object;
}

std::filesystem::path ResolveFullPath(const FString& Path)
{
    std::filesystem::path FilePath = FPaths::ToPath(Path);
    if (!FilePath.is_absolute())
    {
        FilePath = FPaths::ToPath(FPaths::RootDir()) / FilePath;
    }
    return FilePath.lexically_normal();
}

void ApplySimplePatternFields(USimpleCameraShakePattern* Pattern, const FCameraShakeAssetDefinition& Definition)
{
    if (!Pattern)
    {
        return;
    }

    Pattern->Duration = Definition.Duration;
    Pattern->BlendInTime = Definition.BlendInTime;
    Pattern->BlendOutTime = Definition.BlendOutTime;
}

void ApplyWaveOscillator(FWaveOscillator& Target, const FCameraShakeAssetOscillator& Source)
{
    Target.Amplitude = Source.Amplitude;
    Target.Frequency = Source.Frequency;
    Target.InitialOffsetType = Source.InitialOffsetType == ECameraShakeAssetWaveInitialOffsetType::Zero
                                   ? EInitialWaveOscillatorOffsetType::Zero
                                   : EInitialWaveOscillatorOffsetType::Random;
}

void ApplyPerlinShaker(FPerlinNoiseShaker& Target, const FCameraShakeAssetOscillator& Source)
{
    Target.Amplitude = Source.Amplitude;
    Target.Frequency = Source.Frequency;
}
} // namespace

TOptional<FCameraShakeAssetDefinition> FCameraShakeAssetManager::LoadAsset(const FString& Path) const
{
    const FString NormalizedPath = NormalizeAssetPath(Path);
    if (NormalizedPath.empty())
    {
        UE_LOG(CameraShake, Warning, "Camera shake asset path is empty.");
        return TOptional<FCameraShakeAssetDefinition>();
    }

    const std::filesystem::path FullPath = ResolveFullPath(NormalizedPath);
    std::error_code Ec;
    if (!std::filesystem::exists(FullPath, Ec) || !std::filesystem::is_regular_file(FullPath, Ec))
    {
        UE_LOG(CameraShake, Warning, "Camera shake asset not found: %s", NormalizedPath.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    std::ifstream File(FullPath, std::ios::binary);
    if (!File.is_open())
    {
        UE_LOG(CameraShake, Warning, "Failed to open camera shake asset: %s", NormalizedPath.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    std::stringstream Buffer;
    Buffer << File.rdbuf();
    return ParseDefinition(NormalizedPath, Buffer.str());
}

bool FCameraShakeAssetManager::SaveAsset(const FString& Path, const FCameraShakeAssetDefinition& Definition) const
{
    const FString NormalizedPath = NormalizeAssetPath(Path);
    if (NormalizedPath.empty())
    {
        UE_LOG(CameraShake, Warning, "Cannot save camera shake asset with empty path.");
        return false;
    }

    FCameraShakeAssetDefinition SaveDefinition = Definition;
    SaveDefinition.Version = 1;
    SaveDefinition.ShakeClass = SaveDefinition.ShakeClass.empty() ? "UCameraShakeBase" : SaveDefinition.ShakeClass;
    SaveDefinition.RootPatternClass = PatternClassFromType(SaveDefinition.PatternType);

    if (!IsSupportedPatternClass(SaveDefinition.RootPatternClass))
    {
        UE_LOG(CameraShake, Warning, "Cannot save unsupported camera shake pattern: %s", SaveDefinition.RootPatternClass.c_str());
        return false;
    }

    json::JSON Root = json::Object();
    Root[ShakeKeys::Version] = SaveDefinition.Version;
    Root[ShakeKeys::ShakeClass] = SaveDefinition.ShakeClass;
    Root[ShakeKeys::bSingleInstance] = SaveDefinition.bSingleInstance;

    json::JSON Pattern = json::Object();
    Pattern[ShakeKeys::Class] = SaveDefinition.RootPatternClass;
    Pattern[ShakeKeys::Duration] = SaveDefinition.Duration;
    Pattern[ShakeKeys::BlendInTime] = SaveDefinition.BlendInTime;
    Pattern[ShakeKeys::BlendOutTime] = SaveDefinition.BlendOutTime;
    if (SaveDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
    {
        Pattern[ShakeKeys::PlayRate] = (std::max)(SaveDefinition.PlayRate, 0.f);
        FSequenceCameraShakeCurves Curves = SaveDefinition.Curves;
        for (FSequenceCameraShakeCurve& Curve : Curves)
        {
            USequenceCameraShakePattern::NormalizeCurve(Curve, SaveDefinition.Duration);
        }
        Pattern[ShakeKeys::Curves] = WriteSequenceCurves(Curves);
    }
    else
    {
        Pattern[ShakeKeys::LocationAmplitudeMultiplier] = SaveDefinition.LocationAmplitudeMultiplier;
        Pattern[ShakeKeys::LocationFrequencyMultiplier] = SaveDefinition.LocationFrequencyMultiplier;
        Pattern[ShakeKeys::RotationAmplitudeMultiplier] = SaveDefinition.RotationAmplitudeMultiplier;
        Pattern[ShakeKeys::RotationFrequencyMultiplier] = SaveDefinition.RotationFrequencyMultiplier;
        Pattern[ShakeKeys::X] = WriteOscillator(SaveDefinition.X);
        Pattern[ShakeKeys::Y] = WriteOscillator(SaveDefinition.Y);
        Pattern[ShakeKeys::Z] = WriteOscillator(SaveDefinition.Z);
        Pattern[ShakeKeys::Pitch] = WriteOscillator(SaveDefinition.Pitch);
        Pattern[ShakeKeys::Yaw] = WriteOscillator(SaveDefinition.Yaw);
        Pattern[ShakeKeys::Roll] = WriteOscillator(SaveDefinition.Roll);
        Pattern[ShakeKeys::FOV] = WriteOscillator(SaveDefinition.FOV);
    }
    Root[ShakeKeys::RootPattern] = Pattern;

    const std::filesystem::path FullPath = ResolveFullPath(NormalizedPath);
    std::error_code Ec;
    std::filesystem::create_directories(FullPath.parent_path(), Ec);
    if (Ec)
    {
        UE_LOG(CameraShake, Warning, "Failed to create camera shake asset directory: %s", FPaths::FromPath(FullPath.parent_path()).c_str());
        return false;
    }

    std::ofstream File(FullPath, std::ios::binary | std::ios::trunc);
    if (!File.is_open())
    {
        UE_LOG(CameraShake, Warning, "Failed to save camera shake asset: %s", NormalizedPath.c_str());
        return false;
    }

    File << Root;
    const bool bGood = File.good();
    if (!bGood)
    {
        UE_LOG(CameraShake, Warning, "Failed while writing camera shake asset: %s", NormalizedPath.c_str());
    }
    return bGood;
}

TArray<FCameraShakeAssetListItem> FCameraShakeAssetManager::ScanAssets() const
{
    TArray<FCameraShakeAssetListItem> Result;

    const std::filesystem::path ShakeRoot = FPaths::ToPath(GetDefaultAssetDirectory());
    std::error_code Ec;
    if (!std::filesystem::exists(ShakeRoot, Ec) || !std::filesystem::is_directory(ShakeRoot, Ec))
    {
        return Result;
    }

    const std::filesystem::path ProjectRoot = FPaths::ToPath(FPaths::RootDir()).lexically_normal();
    for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ShakeRoot, Ec))
    {
        if (Ec)
        {
            break;
        }

        if (!Entry.is_regular_file(Ec))
        {
            continue;
        }

        const std::filesystem::path Path = Entry.path().lexically_normal();
        if (ToLowerAscii(FPaths::FromPath(Path.extension())) != ".shake")
        {
            continue;
        }

        FCameraShakeAssetListItem Item;
        Item.DisplayName = FPaths::FromPath(Path.stem());
        Item.FullPath = FPaths::FromPath(Path.lexically_relative(ProjectRoot));
        Result.push_back(std::move(Item));
    }

    std::sort(Result.begin(), Result.end(), [](const FCameraShakeAssetListItem& A, const FCameraShakeAssetListItem& B)
              {
                  return ToLowerAscii(A.DisplayName) < ToLowerAscii(B.DisplayName);
              });

    return Result;
}

UCameraShakeBase* FCameraShakeAssetManager::CreateShakeInstance(const FString& Path, UObject* Outer) const
{
    TOptional<FCameraShakeAssetDefinition> Definition = LoadAsset(Path);
    if (!Definition.has_value())
    {
        return nullptr;
    }

    return CreateShakeInstance(Definition.value(), Outer, NormalizeAssetPath(Path));
}

UCameraShakeBase* FCameraShakeAssetManager::CreateShakeInstance(const FCameraShakeAssetDefinition& Definition, UObject* Outer, const FString& SourcePath) const
{
    if (!IsSupportedPatternClass(Definition.RootPatternClass))
    {
        UE_LOG(CameraShake, Warning, "Unsupported camera shake pattern class: %s", Definition.RootPatternClass.c_str());
        return nullptr;
    }

    const FString ShakeClassName = Definition.ShakeClass.empty() ? FString("UCameraShakeBase") : Definition.ShakeClass;
    UObject* ShakeObject = FObjectFactory::Get().Create(ShakeClassName, Outer);
    UCameraShakeBase* ShakeInstance = Cast<UCameraShakeBase>(ShakeObject);
    if (!ShakeInstance)
    {
        if (ShakeObject)
        {
            UObjectManager::Get().DestroyObject(ShakeObject);
        }
        UE_LOG(CameraShake, Warning, "Failed to create camera shake class: %s", Definition.ShakeClass.c_str());
        return nullptr;
    }

    UObject* PatternObject = FObjectFactory::Get().Create(Definition.RootPatternClass, ShakeInstance);
    UCameraShakePattern* RootPattern = Cast<UCameraShakePattern>(PatternObject);
    if (!RootPattern)
    {
        if (PatternObject)
        {
            UObjectManager::Get().DestroyObject(PatternObject);
        }
        UObjectManager::Get().DestroyObject(ShakeInstance);
        UE_LOG(CameraShake, Warning, "Failed to create camera shake pattern class: %s", Definition.RootPatternClass.c_str());
        return nullptr;
    }

    ApplySimplePatternFields(Cast<USimpleCameraShakePattern>(RootPattern), Definition);

    if (USequenceCameraShakePattern* Sequence = Cast<USequenceCameraShakePattern>(RootPattern))
    {
        Sequence->PlayRate = (std::max)(Definition.PlayRate, 0.f);
        Sequence->Curves = Definition.Curves;
        for (FSequenceCameraShakeCurve& Curve : Sequence->Curves)
        {
            USequenceCameraShakePattern::NormalizeCurve(Curve, Definition.Duration);
        }
    }
    else if (UWaveOscillatorCameraShakePattern* Wave = Cast<UWaveOscillatorCameraShakePattern>(RootPattern))
    {
        Wave->LocationAmplitudeMultiplier = Definition.LocationAmplitudeMultiplier;
        Wave->LocationFrequencyMultiplier = Definition.LocationFrequencyMultiplier;
        Wave->RotationAmplitudeMultiplier = Definition.RotationAmplitudeMultiplier;
        Wave->RotationFrequencyMultiplier = Definition.RotationFrequencyMultiplier;
        ApplyWaveOscillator(Wave->X, Definition.X);
        ApplyWaveOscillator(Wave->Y, Definition.Y);
        ApplyWaveOscillator(Wave->Z, Definition.Z);
        ApplyWaveOscillator(Wave->Pitch, Definition.Pitch);
        ApplyWaveOscillator(Wave->Yaw, Definition.Yaw);
        ApplyWaveOscillator(Wave->Roll, Definition.Roll);
        ApplyWaveOscillator(Wave->FOV, Definition.FOV);
    }
    else if (UPerlinNoiseCameraShakePattern* Perlin = Cast<UPerlinNoiseCameraShakePattern>(RootPattern))
    {
        Perlin->LocationAmplitudeMultiplier = Definition.LocationAmplitudeMultiplier;
        Perlin->LocationFrequencyMultiplier = Definition.LocationFrequencyMultiplier;
        Perlin->RotationAmplitudeMultiplier = Definition.RotationAmplitudeMultiplier;
        Perlin->RotationFrequencyMultiplier = Definition.RotationFrequencyMultiplier;
        ApplyPerlinShaker(Perlin->X, Definition.X);
        ApplyPerlinShaker(Perlin->Y, Definition.Y);
        ApplyPerlinShaker(Perlin->Z, Definition.Z);
        ApplyPerlinShaker(Perlin->Pitch, Definition.Pitch);
        ApplyPerlinShaker(Perlin->Yaw, Definition.Yaw);
        ApplyPerlinShaker(Perlin->Roll, Definition.Roll);
        ApplyPerlinShaker(Perlin->FOV, Definition.FOV);
    }

    ShakeInstance->bSingleInstance = Definition.bSingleInstance;
    ShakeInstance->SetSourceAssetPath(SourcePath);
    ShakeInstance->SetRootShakePattern(RootPattern);
    return ShakeInstance;
}

FString FCameraShakeAssetManager::NormalizeAssetPath(const FString& Path) const
{
    if (Path.empty())
    {
        return FString();
    }

    std::filesystem::path FullPath = ResolveFullPath(Path);

    std::error_code Ec;
    std::filesystem::path Canonical = std::filesystem::weakly_canonical(FullPath, Ec);
    if (!Ec)
    {
        FullPath = Canonical;
    }

    std::filesystem::path Result = FullPath;
    if (Result.extension().empty())
    {
        Result.replace_extension(L".shake");
    }

    return FPaths::MakeRelativeToRoot(Result);
}

FString FCameraShakeAssetManager::GetDefaultAssetDirectory()
{
    return FPaths::FromPath(FPaths::ToPath(FPaths::ContentDir()) / L"CameraShakes");
}

TOptional<FCameraShakeAssetDefinition> FCameraShakeAssetManager::ParseDefinition(const FString& Path, const FString& Content) const
{
    if (Content.empty())
    {
        UE_LOG(CameraShake, Warning, "Camera shake asset is empty: %s", Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    json::JSON Root;
    try
    {
        Root = json::JSON::Load(Content);
    }
    catch (const std::exception& Exception)
    {
        UE_LOG(CameraShake, Warning, "Malformed camera shake JSON: %s (%s)", Path.c_str(), Exception.what());
        return TOptional<FCameraShakeAssetDefinition>();
    }
    catch (...)
    {
        UE_LOG(CameraShake, Warning, "Malformed camera shake JSON: %s", Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    if (Root.JSONType() != json::JSON::Class::Object)
    {
        UE_LOG(CameraShake, Warning, "Camera shake asset root must be an object: %s", Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    FCameraShakeAssetDefinition Definition;
    Definition.Version = ReadInt(Root, ShakeKeys::Version, Definition.Version);
    if (Definition.Version != 1)
    {
        UE_LOG(CameraShake, Warning, "Unsupported camera shake asset version %d: %s", Definition.Version, Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    Definition.ShakeClass = ReadString(Root, ShakeKeys::ShakeClass, Definition.ShakeClass);
    Definition.bSingleInstance = ReadBool(Root, ShakeKeys::bSingleInstance, Definition.bSingleInstance);

    if (!Root.hasKey(ShakeKeys::RootPattern) || Root.at(ShakeKeys::RootPattern).JSONType() != json::JSON::Class::Object)
    {
        UE_LOG(CameraShake, Warning, "Camera shake asset missing RootPattern object: %s", Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }

    const json::JSON& Pattern = Root.at(ShakeKeys::RootPattern);
    Definition.RootPatternClass = ReadString(Pattern, ShakeKeys::Class, Definition.RootPatternClass);
    if (!IsSupportedPatternClass(Definition.RootPatternClass))
    {
        UE_LOG(CameraShake, Warning, "Unsupported camera shake pattern class '%s' in %s", Definition.RootPatternClass.c_str(), Path.c_str());
        return TOptional<FCameraShakeAssetDefinition>();
    }
    Definition.PatternType = PatternTypeFromClass(Definition.RootPatternClass);

    Definition.Duration = ReadFloat(Pattern, ShakeKeys::Duration, Definition.Duration);
    Definition.BlendInTime = ReadFloat(Pattern, ShakeKeys::BlendInTime, Definition.BlendInTime);
    Definition.BlendOutTime = ReadFloat(Pattern, ShakeKeys::BlendOutTime, Definition.BlendOutTime);
    Definition.PlayRate = (std::max)(ReadFloat(Pattern, ShakeKeys::PlayRate, Definition.PlayRate), 0.f);

    if (Definition.PatternType == ECameraShakeAssetPatternType::Sequence)
    {
        ReadSequenceCurves(Pattern, Definition, Path);
    }
    else
    {
        Definition.LocationAmplitudeMultiplier = ReadFloat(Pattern, ShakeKeys::LocationAmplitudeMultiplier, Definition.LocationAmplitudeMultiplier);
        Definition.LocationFrequencyMultiplier = ReadFloat(Pattern, ShakeKeys::LocationFrequencyMultiplier, Definition.LocationFrequencyMultiplier);
        Definition.RotationAmplitudeMultiplier = ReadFloat(Pattern, ShakeKeys::RotationAmplitudeMultiplier, Definition.RotationAmplitudeMultiplier);
        Definition.RotationFrequencyMultiplier = ReadFloat(Pattern, ShakeKeys::RotationFrequencyMultiplier, Definition.RotationFrequencyMultiplier);

        ReadOscillator(Pattern, ShakeKeys::X, Definition.X);
        ReadOscillator(Pattern, ShakeKeys::Y, Definition.Y);
        ReadOscillator(Pattern, ShakeKeys::Z, Definition.Z);
        ReadOscillator(Pattern, ShakeKeys::Pitch, Definition.Pitch);
        ReadOscillator(Pattern, ShakeKeys::Yaw, Definition.Yaw);
        ReadOscillator(Pattern, ShakeKeys::Roll, Definition.Roll);
        ReadOscillator(Pattern, ShakeKeys::FOV, Definition.FOV);
    }

    return Definition;
}
