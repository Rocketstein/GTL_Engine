#include "Editor/UI/EditorCameraShakePanel.h"

#include "Camera/Shakes/CameraShakeBase.h"
#include "Component/CameraComponent.h"
#include "Core/Logging/LogMacros.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Classes/Camera/CameraManager.h"
#include "Math/MathUtils.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <commdlg.h>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace
{
constexpr float LeftPaneWidth = 260.0f;
constexpr float PropertyLabelWidth = 190.0f;
constexpr float CurveGraphHeight = 280.0f;
constexpr float CurveHitRadius = 9.0f;
constexpr float CurveHandleRadius = 5.0f;

const char* PatternLabels[] = {
    "Wave Oscillator",
    "Perlin Noise",
    "Sequence Curve",
};

const char* InitialOffsetLabels[] = {
    "Random",
    "Zero",
};

FString PatternClassFromType(ECameraShakeAssetPatternType Type)
{
    switch (Type)
    {
    case ECameraShakeAssetPatternType::PerlinNoise:
        return "UPerlinNoiseCameraShakePattern";
    case ECameraShakeAssetPatternType::Sequence:
        return "USequenceCameraShakePattern";
    case ECameraShakeAssetPatternType::WaveOscillator:
    default:
        return "UWaveOscillatorCameraShakePattern";
    }
}

int PatternIndexFromType(ECameraShakeAssetPatternType Type)
{
    switch (Type)
    {
    case ECameraShakeAssetPatternType::PerlinNoise:
        return 1;
    case ECameraShakeAssetPatternType::Sequence:
        return 2;
    case ECameraShakeAssetPatternType::WaveOscillator:
    default:
        return 0;
    }
}

ECameraShakeAssetPatternType PatternTypeFromIndex(int Index)
{
    switch (Index)
    {
    case 1:
        return ECameraShakeAssetPatternType::PerlinNoise;
    case 2:
        return ECameraShakeAssetPatternType::Sequence;
    case 0:
    default:
        return ECameraShakeAssetPatternType::WaveOscillator;
    }
}

void ClampNonNegative(float& Value)
{
    if (Value < 0.f)
    {
        Value = 0.f;
    }
}

float ClampToRange(float Value, float MinValue, float MaxValue)
{
    if (MaxValue < MinValue)
    {
        std::swap(MinValue, MaxValue);
    }
    return FMath::Clamp(Value, MinValue, MaxValue);
}

float DistanceSquared(const ImVec2& A, const ImVec2& B)
{
    const float DX = A.x - B.x;
    const float DY = A.y - B.y;
    return DX * DX + DY * DY;
}

bool IsLocationSequenceChannel(int32 ChannelIndex)
{
    return ChannelIndex >= 0 && ChannelIndex <= 2;
}

bool IsRotationSequenceChannel(int32 ChannelIndex)
{
    return ChannelIndex >= 3 && ChannelIndex <= 5;
}

const char* SequenceChannelUnitLabel(int32 ChannelIndex)
{
    if (IsLocationSequenceChannel(ChannelIndex))
    {
        return "engine units";
    }
    return "degrees";
}

ImU32 SequenceChannelColor(int32 ChannelIndex)
{
    static const ImU32 Colors[] = {
        IM_COL32(229, 75, 75, 255),
        IM_COL32(75, 180, 96, 255),
        IM_COL32(78, 132, 230, 255),
        IM_COL32(229, 170, 62, 255),
        IM_COL32(93, 194, 205, 255),
        IM_COL32(194, 104, 222, 255),
        IM_COL32(235, 235, 120, 255),
    };
    return Colors[(std::clamp)(ChannelIndex, 0, static_cast<int32>(SequenceCameraShakeChannelCount) - 1)];
}

FSequenceCameraShakeKey MakeSequenceKey(float Time, float Value, float Duration)
{
    FSequenceCameraShakeKey Key;
    Key.Time = (std::max)(Time, 0.f);
    Key.Value = Value;
    const float HandleTime = (std::max)(Duration, 0.3f) / 3.f;
    Key.ArriveTime = -HandleTime;
    Key.LeaveTime = HandleTime;
    return Key;
}

const char* TooltipSequencePlayRate =
    "Scales sequence timeline playback. 1.0 uses asset time, 0.5 plays half speed, and 2.0 plays double speed.";

const char* TooltipSequenceCurve =
    "Time/value camera offset curve. Location channels use engine units; rotation and FOV use degrees.";

void DrawPropertyTooltip(const char* Description)
{
    if (Description == nullptr || Description[0] == '\0')
    {
        return;
    }

    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        return;
    }

    if (ImGui::BeginTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 34.0f);
        ImGui::TextUnformatted(Description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void DrawLabelWithTooltip(const char* Label, const char* Tooltip)
{
    ImGui::TextUnformatted(Label);
    DrawPropertyTooltip(Tooltip);
}

constexpr const char* TooltipPatternType =
    "쉐이크 결과를 만드는 계산 방식입니다.\n"
    "Wave Oscillator는 규칙적인 사인파 흔들림이고, Perlin Noise는 불규칙하게 이어지는 노이즈 흔들림입니다.";

constexpr const char* TooltipSingleInstance =
    "켜면 같은 클래스/asset 경로의 쉐이크가 이미 재생 중일 때 새로 겹치지 않고 기존 쉐이크를 다시 시작합니다.";

constexpr const char* TooltipDuration =
    "쉐이크가 재생되는 전체 시간(초)입니다.\n"
    "0이면 Simple 패턴에서 무한 재생으로 처리됩니다.";

constexpr const char* TooltipBlendIn =
    "시작 후 이 시간 동안 흔들림 세기가 0에서 100%로 올라갑니다.";

constexpr const char* TooltipBlendOut =
    "끝나기 전 또는 Stop 시 이 시간 동안 흔들림 세기가 100%에서 0으로 내려갑니다.";

constexpr const char* TooltipLocationAmplitude =
    "X/Y/Z 위치 Amplitude에 곱해지는 전체 배율입니다.\n"
    "0이면 위치 이동 흔들림이 사라지고, 2면 위치 이동 폭이 두 배가 됩니다.";

constexpr const char* TooltipLocationFrequency =
    "X/Y/Z 위치 Frequency에 곱해지는 전체 배율입니다.\n"
    "값이 높을수록 위치 흔들림이 더 빠르게 반복됩니다.";

constexpr const char* TooltipRotationAmplitude =
    "Pitch/Yaw/Roll 회전 Amplitude에 곱해지는 전체 배율입니다.\n"
    "회전 흔들림 각도 크기를 한 번에 키우거나 줄일 때 사용합니다.";

constexpr const char* TooltipRotationFrequency =
    "Pitch/Yaw/Roll 회전 Frequency에 곱해지는 전체 배율입니다.\n"
    "값이 높을수록 회전 흔들림이 더 빠르게 반복됩니다.";

constexpr const char* TooltipInitialOffset =
    "Wave Oscillator의 시작 위상입니다.\n"
    "Random은 재생마다 다른 시작점, Zero는 항상 같은 시작점에서 시작합니다.";

constexpr const char* TooltipXChannel =
    "카메라 로컬 X축 위치 오프셋입니다.\n"
    "CameraLocal 재생 기준으로 앞/뒤 방향 흔들림에 반영됩니다.";

constexpr const char* TooltipYChannel =
    "카메라 로컬 Y축 위치 오프셋입니다.\n"
    "CameraLocal 재생 기준으로 좌/우 방향 흔들림에 반영됩니다.";

constexpr const char* TooltipZChannel =
    "카메라 로컬 Z축 위치 오프셋입니다.\n"
    "CameraLocal 재생 기준으로 위/아래 방향 흔들림에 반영됩니다.";

constexpr const char* TooltipLocationAxisAmplitude =
    "해당 축 위치 흔들림의 기본 크기입니다.\n"
    "최종 위치 오프셋은 이 값에 Location Amplitude와 재생 스케일/블렌드가 곱해져 적용됩니다.";

constexpr const char* TooltipLocationAxisFrequency =
    "해당 축 위치 흔들림의 속도입니다.\n"
    "Wave는 초당 반복 수처럼 동작하고, Perlin은 노이즈를 읽는 속도로 동작합니다. Location Frequency 배율이 추가로 곱해집니다.";

constexpr const char* TooltipPitchChannel =
    "Pitch 회전 오프셋입니다.\n"
    "카메라가 위/아래로 고개를 드는 각도에 반영됩니다.";

constexpr const char* TooltipYawChannel =
    "Yaw 회전 오프셋입니다.\n"
    "카메라가 좌/우로 방향을 트는 각도에 반영됩니다.";

constexpr const char* TooltipRollChannel =
    "Roll 회전 오프셋입니다.\n"
    "카메라가 좌/우로 기울어지는 각도에 반영됩니다.";

constexpr const char* TooltipRotationAxisAmplitude =
    "해당 회전 흔들림의 기본 각도 크기(deg)입니다.\n"
    "최종 회전은 이 값에 Rotation Amplitude와 재생 스케일/블렌드가 곱해져 적용됩니다.";

constexpr const char* TooltipRotationAxisFrequency =
    "해당 회전 흔들림의 속도입니다.\n"
    "Wave는 초당 반복 수처럼 동작하고, Perlin은 노이즈를 읽는 속도로 동작합니다. Rotation Frequency 배율이 추가로 곱해집니다.";

constexpr const char* TooltipFOVChannel =
    "FOV 오프셋입니다.\n"
    "카메라 시야각(deg)에 더해져 줌 인/아웃처럼 보입니다.";

constexpr const char* TooltipFOVAmplitude =
    "FOV 흔들림의 기본 크기(deg)입니다.\n"
    "Location/Rotation 배율은 적용되지 않고, 재생 스케일/블렌드만 곱해집니다.";

constexpr const char* TooltipFOVFrequency =
    "FOV 흔들림의 속도입니다.\n"
    "Wave는 초당 반복 수처럼 동작하고, Perlin은 노이즈를 읽는 속도로 동작합니다. Location/Rotation Frequency 배율은 적용되지 않습니다.";

bool DrawFloatPropertyRow(const char* Label, float& Value, float Speed, float Min, float Max, const char* Format, const char* Tooltip)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawLabelWithTooltip(Label, Tooltip);

    ImGui::TableNextColumn();
    ImGui::PushID(Label);
    ImGui::SetNextItemWidth(-1.0f);
    const bool bChanged = ImGui::DragFloat("##Value", &Value, Speed, Min, Max, Format);
    DrawPropertyTooltip(Tooltip);
    ImGui::PopID();
    return bChanged;
}

FString EnsureShakeExtension(const FString& Path)
{
    std::filesystem::path FilePath = FPaths::ToPath(Path);
    if (FilePath.extension().empty())
    {
        FilePath.replace_extension(L".shake");
    }
    return FPaths::FromPath(FilePath);
}

FString OpenCameraShakeSaveDialog(const FString& CurrentPath)
{
    wchar_t FileName[MAX_PATH] = L"NewCameraShake.shake";
    if (!CurrentPath.empty())
    {
        const std::filesystem::path FullPath = FPaths::ToPath(FPaths::RootDir()) / FPaths::ToPath(CurrentPath);
        const std::wstring WidePath = FullPath.lexically_normal().wstring();
        wcsncpy_s(FileName, MAX_PATH, WidePath.c_str(), _TRUNCATE);
    }

    const std::filesystem::path InitialDir = FPaths::ToPath(FCameraShakeAssetManager::GetDefaultAssetDirectory());

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(OPENFILENAMEW);
    Ofn.lpstrFilter = L"Camera Shake (*.shake)\0*.shake\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FileName;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrDefExt = L"shake";
    Ofn.lpstrInitialDir = InitialDir.c_str();
    Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&Ofn))
    {
        return FString();
    }

    return EnsureShakeExtension(FPaths::FromWide(FileName));
}
} // namespace

void FEditorCameraShakePanel::Initialize(UEditorEngine* InEditorEngine)
{
    FEditorPanel::Initialize(InEditorEngine);
    RefreshAssets();
}

void FEditorCameraShakePanel::Render(float DeltaTime)
{
    UpdatePreview(DeltaTime);

    bool bOpen = FEditorSettings::Get().UI.bCameraShakeEditor;
    ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_Once);
    if (!ImGui::Begin("Camera Shake Editor", &bOpen))
    {
        ImGui::End();
        FEditorSettings::Get().UI.bCameraShakeEditor = bOpen;
        if (!bOpen)
        {
            StopPreview();
        }
        return;
    }

    if (bRefreshRequested)
    {
        RefreshAssets();
        bRefreshRequested = false;
    }

    DrawAssetList();
    ImGui::SameLine();
    DrawEditor();

    ImGui::End();

    FEditorSettings::Get().UI.bCameraShakeEditor = bOpen;
    if (!bOpen)
    {
        StopPreview();
    }
}

bool FEditorCameraShakePanel::OpenAsset(const FString& Path)
{
    StopPreview();

    TOptional<FCameraShakeAssetDefinition> Definition = FCameraShakeAssetManager::Get().LoadAsset(Path);
    if (!Definition.has_value())
    {
        StatusMessage = "Failed to load asset.";
        return false;
    }

    CurrentDefinition = Definition.value();
    CurrentPath = FCameraShakeAssetManager::Get().NormalizeAssetPath(Path);
    SequenceSelectedChannel = 0;
    SequenceSelectedKey = -1;
    for (FSequenceCameraShakeCurve& Curve : CurrentDefinition.Curves)
    {
        NormalizeSequenceCurve(Curve);
    }
    FitSequenceCurveView();
    bDirty = false;
    StatusMessage = "Loaded " + CurrentPath;
    FEditorSettings::Get().UI.bCameraShakeEditor = true;
    bRefreshRequested = true;
    return true;
}

void FEditorCameraShakePanel::StopPreview()
{
    if (Preview.bActive && Preview.Camera)
    {
        Preview.Camera->SetWorldLocation(Preview.BaseLocation);
        Preview.Camera->SetRelativeRotation(Preview.BaseRotation);

        FCameraState CameraState = Preview.Camera->GetCameraState();
        CameraState.FOV = Preview.BaseFOVRadians;
        Preview.Camera->SetCameraState(CameraState);
    }

    if (Preview.ShakeInstance)
    {
        Preview.ShakeInstance->StopShake(true);
        Preview.ShakeInstance->TeardownShake();
        UObjectManager::Get().DestroyObject(Preview.ShakeInstance);
    }

    Preview = FPreviewState();
}

void FEditorCameraShakePanel::RefreshAssets()
{
    AssetList = FCameraShakeAssetManager::Get().ScanAssets();
}

void FEditorCameraShakePanel::NewAsset()
{
    StopPreview();
    CurrentDefinition = FCameraShakeAssetDefinition();
    CurrentPath.clear();
    bDirty = true;
    SequenceSelectedChannel = 0;
    SequenceSelectedKey = -1;
    FitSequenceCurveView();
    StatusMessage = "New unsaved camera shake.";
}

bool FEditorCameraShakePanel::SaveCurrent()
{
    if (CurrentPath.empty())
    {
        return SaveCurrentAs();
    }

    CurrentDefinition.RootPatternClass = PatternClassFromType(CurrentDefinition.PatternType);
    if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
    {
        for (FSequenceCameraShakeCurve& Curve : CurrentDefinition.Curves)
        {
            NormalizeSequenceCurve(Curve);
        }
    }
    if (!FCameraShakeAssetManager::Get().SaveAsset(CurrentPath, CurrentDefinition))
    {
        StatusMessage = "Save failed.";
        return false;
    }

    CurrentPath = FCameraShakeAssetManager::Get().NormalizeAssetPath(CurrentPath);
    bDirty = false;
    StatusMessage = "Saved " + CurrentPath;
    RefreshAssets();
    return true;
}

bool FEditorCameraShakePanel::SaveCurrentAs()
{
    const FString NewPath = OpenCameraShakeSaveDialog(CurrentPath);
    if (NewPath.empty())
    {
        return false;
    }

    CurrentPath = FCameraShakeAssetManager::Get().NormalizeAssetPath(NewPath);
    return SaveCurrent();
}

void FEditorCameraShakePanel::DrawAssetList()
{
    if (ImGui::BeginChild("CameraShakeAssets", ImVec2(LeftPaneWidth, 0.0f), true))
    {
        if (ImGui::Button("New", ImVec2(56.0f, 0.0f)))
        {
            NewAsset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save", ImVec2(58.0f, 0.0f)))
        {
            SaveCurrent();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As", ImVec2(76.0f, 0.0f)))
        {
            SaveCurrentAs();
        }
        if (ImGui::Button("Refresh", ImVec2(-1.0f, 0.0f)))
        {
            RefreshAssets();
        }

        ImGui::Separator();

        if (AssetList.empty())
        {
            ImGui::TextDisabled("No .shake assets.");
        }
        else
        {
            for (const FCameraShakeAssetListItem& Item : AssetList)
            {
                const bool bSelected = CurrentPath == Item.FullPath;
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    OpenAsset(Item.FullPath);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", Item.FullPath.c_str());
                }
            }
        }
    }
    ImGui::EndChild();
}

void FEditorCameraShakePanel::DrawEditor()
{
    if (ImGui::BeginChild("CameraShakeEditorBody", ImVec2(0.0f, 0.0f), false))
    {
        const FString DisplayPath = MakeDisplayPath();
        ImGui::TextUnformatted(DisplayPath.c_str());
        if (bDirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.74f, 0.24f, 1.0f), "modified");
        }

        if (!StatusMessage.empty())
        {
            ImGui::TextDisabled("%s", StatusMessage.c_str());
        }

        ImGui::Separator();
        DrawPatternControls();
        ImGui::Separator();
        if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
        {
            DrawSequenceCurveEditor();
        }
        else
        {
            DrawOscillatorTable();
        }
        ImGui::Separator();

        const char* PreviewLabel = Preview.bActive ? "Stop Preview" : "Preview";
        if (ImGui::Button(PreviewLabel, ImVec2(130.0f, 0.0f)))
        {
            if (Preview.bActive)
            {
                StopPreview();
            }
            else
            {
                StartPreview();
            }
        }
    }
    ImGui::EndChild();
}

void FEditorCameraShakePanel::DrawPatternControls()
{
    ImGui::TextUnformatted("Pattern");
    ImGui::Spacing();

    int PatternIndex = PatternIndexFromType(CurrentDefinition.PatternType);
    if (ImGui::BeginTable("CameraShakePatternTable", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, PropertyLabelWidth);
        ImGui::TableSetupColumn("Value");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawLabelWithTooltip("Pattern Type", TooltipPatternType);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##PatternType", &PatternIndex, PatternLabels, IM_ARRAYSIZE(PatternLabels)))
        {
            StopPreview();
            CurrentDefinition.PatternType = PatternTypeFromIndex(PatternIndex);
            CurrentDefinition.RootPatternClass = PatternClassFromType(CurrentDefinition.PatternType);
            if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
            {
                EnsureDefaultSequenceCurves();
                FitSequenceCurveView();
            }
            MarkDirty();
        }
        DrawPropertyTooltip(TooltipPatternType);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        DrawLabelWithTooltip("Single Instance", TooltipSingleInstance);
        ImGui::TableNextColumn();
        bool bSingleInstance = CurrentDefinition.bSingleInstance;
        if (ImGui::Checkbox("##SingleInstance", &bSingleInstance))
        {
            CurrentDefinition.bSingleInstance = bSingleInstance;
            MarkDirty();
        }
        DrawPropertyTooltip(TooltipSingleInstance);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Timing");
    if (ImGui::BeginTable("CameraShakeTimingTable", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, PropertyLabelWidth);
        ImGui::TableSetupColumn("Value");

        if (DrawFloatPropertyRow("Duration", CurrentDefinition.Duration, 0.01f, 0.0f, 60.0f, "%.3f", TooltipDuration))
        {
            ClampNonNegative(CurrentDefinition.Duration);
            if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
            {
                for (FSequenceCameraShakeCurve& Curve : CurrentDefinition.Curves)
                {
                    NormalizeSequenceCurve(Curve);
                }
                FitSequenceCurveView();
            }
            MarkDirty();
        }
        if (DrawFloatPropertyRow("Blend In Time", CurrentDefinition.BlendInTime, 0.01f, 0.0f, 10.0f, "%.3f", TooltipBlendIn))
        {
            ClampNonNegative(CurrentDefinition.BlendInTime);
            MarkDirty();
        }
        if (DrawFloatPropertyRow("Blend Out Time", CurrentDefinition.BlendOutTime, 0.01f, 0.0f, 10.0f, "%.3f", TooltipBlendOut))
        {
            ClampNonNegative(CurrentDefinition.BlendOutTime);
            MarkDirty();
        }
        if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence
            && DrawFloatPropertyRow("Play Rate", CurrentDefinition.PlayRate, 0.01f, 0.0f, 20.0f, "%.3f", TooltipSequencePlayRate))
        {
            ClampNonNegative(CurrentDefinition.PlayRate);
            MarkDirty();
        }

        ImGui::EndTable();
    }

    if (CurrentDefinition.PatternType == ECameraShakeAssetPatternType::Sequence)
    {
        return;
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Multipliers");
    if (ImGui::BeginTable("CameraShakeMultiplierTable", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, PropertyLabelWidth);
        ImGui::TableSetupColumn("Value");

        if (DrawFloatPropertyRow("Location Amplitude", CurrentDefinition.LocationAmplitudeMultiplier, 0.01f, 0.0f, 100.0f, "%.3f", TooltipLocationAmplitude))
        {
            MarkDirty();
        }
        if (DrawFloatPropertyRow("Location Frequency", CurrentDefinition.LocationFrequencyMultiplier, 0.01f, 0.0f, 100.0f, "%.3f", TooltipLocationFrequency))
        {
            ClampNonNegative(CurrentDefinition.LocationFrequencyMultiplier);
            MarkDirty();
        }
        if (DrawFloatPropertyRow("Rotation Amplitude", CurrentDefinition.RotationAmplitudeMultiplier, 0.01f, 0.0f, 100.0f, "%.3f", TooltipRotationAmplitude))
        {
            MarkDirty();
        }
        if (DrawFloatPropertyRow("Rotation Frequency", CurrentDefinition.RotationFrequencyMultiplier, 0.01f, 0.0f, 100.0f, "%.3f", TooltipRotationFrequency))
        {
            ClampNonNegative(CurrentDefinition.RotationFrequencyMultiplier);
            MarkDirty();
        }

        ImGui::EndTable();
    }
}

void FEditorCameraShakePanel::DrawOscillatorTable()
{
    const bool bDrawInitialOffset = CurrentDefinition.PatternType == ECameraShakeAssetPatternType::WaveOscillator;

    if (!ImGui::BeginTable("CameraShakeOscillators", bDrawInitialOffset ? 4 : 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        return;
    }

    ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn("Amplitude");
    ImGui::TableSetupColumn("Frequency");
    if (bDrawInitialOffset)
    {
        ImGui::TableSetupColumn("Initial Offset", ImGuiTableColumnFlags_WidthFixed, 132.0f);
    }
    ImGui::TableHeadersRow();

    DrawOscillatorRow("X", CurrentDefinition.X, bDrawInitialOffset,
                      TooltipXChannel, TooltipLocationAxisAmplitude, TooltipLocationAxisFrequency);
    DrawOscillatorRow("Y", CurrentDefinition.Y, bDrawInitialOffset,
                      TooltipYChannel, TooltipLocationAxisAmplitude, TooltipLocationAxisFrequency);
    DrawOscillatorRow("Z", CurrentDefinition.Z, bDrawInitialOffset,
                      TooltipZChannel, TooltipLocationAxisAmplitude, TooltipLocationAxisFrequency);
    DrawOscillatorRow("Pitch", CurrentDefinition.Pitch, bDrawInitialOffset,
                      TooltipPitchChannel, TooltipRotationAxisAmplitude, TooltipRotationAxisFrequency);
    DrawOscillatorRow("Yaw", CurrentDefinition.Yaw, bDrawInitialOffset,
                      TooltipYawChannel, TooltipRotationAxisAmplitude, TooltipRotationAxisFrequency);
    DrawOscillatorRow("Roll", CurrentDefinition.Roll, bDrawInitialOffset,
                      TooltipRollChannel, TooltipRotationAxisAmplitude, TooltipRotationAxisFrequency);
    DrawOscillatorRow("FOV", CurrentDefinition.FOV, bDrawInitialOffset,
                      TooltipFOVChannel, TooltipFOVAmplitude, TooltipFOVFrequency);

    ImGui::EndTable();
}

bool FEditorCameraShakePanel::DrawOscillatorRow(const char* Label,
                                                FCameraShakeAssetOscillator& Oscillator,
                                                bool bDrawInitialOffset,
                                                const char* ChannelTooltip,
                                                const char* AmplitudeTooltip,
                                                const char* FrequencyTooltip)
{
    bool bChanged = false;
    ImGui::PushID(Label);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawLabelWithTooltip(Label, ChannelTooltip);

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat("##Amplitude", &Oscillator.Amplitude, 0.05f, -10000.0f, 10000.0f, "%.3f"))
    {
        bChanged = true;
    }
    DrawPropertyTooltip(AmplitudeTooltip);

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat("##Frequency", &Oscillator.Frequency, 0.05f, 0.0f, 10000.0f, "%.3f"))
    {
        ClampNonNegative(Oscillator.Frequency);
        bChanged = true;
    }
    DrawPropertyTooltip(FrequencyTooltip);

    if (bDrawInitialOffset)
    {
        ImGui::TableNextColumn();
        int InitialOffsetIndex = Oscillator.InitialOffsetType == ECameraShakeAssetWaveInitialOffsetType::Zero ? 1 : 0;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##InitialOffset", &InitialOffsetIndex, InitialOffsetLabels, IM_ARRAYSIZE(InitialOffsetLabels)))
        {
            Oscillator.InitialOffsetType = InitialOffsetIndex == 1
                                               ? ECameraShakeAssetWaveInitialOffsetType::Zero
                                               : ECameraShakeAssetWaveInitialOffsetType::Random;
            bChanged = true;
        }
        DrawPropertyTooltip(TooltipInitialOffset);
    }

    if (bChanged)
    {
        MarkDirty();
    }

    ImGui::PopID();
    return bChanged;
}

void FEditorCameraShakePanel::DrawSequenceCurveEditor()
{
    ImGui::TextUnformatted("Sequence Curves");
    DrawPropertyTooltip(TooltipSequenceCurve);
    ImGui::Spacing();

    if (ImGui::BeginTabBar("CameraShakeSequenceChannels"))
    {
        for (size_t ChannelIndex = 0; ChannelIndex < SequenceCameraShakeChannelCount; ++ChannelIndex)
        {
            const ESequenceCameraShakeChannel Channel = static_cast<ESequenceCameraShakeChannel>(ChannelIndex);
            if (ImGui::BeginTabItem(GetSequenceCameraShakeChannelName(Channel)))
            {
                if (SequenceSelectedChannel != static_cast<int32>(ChannelIndex))
                {
                    SequenceSelectedChannel = static_cast<int32>(ChannelIndex);
                    SequenceSelectedKey = -1;
                    SequenceDragTarget = ESequenceEditorDragTarget::None;
                    SequenceDragKey = -1;
                    FitSequenceCurveView();
                }

                FSequenceCameraShakeCurve& Curve = GetSelectedSequenceCurve();
                ImGui::TextDisabled("%s", SequenceChannelUnitLabel(SequenceSelectedChannel));

                ImGui::SameLine();
                if (ImGui::Button("Add Key"))
                {
                    const float Time = CurrentDefinition.Duration > 0.f
                                           ? CurrentDefinition.Duration * 0.5f
                                           : (SequenceViewTimeMin + SequenceViewTimeMax) * 0.5f;
                    const float Value = USequenceCameraShakePattern::EvaluateCurve(Curve, Time);
                    Curve.push_back(MakeSequenceKey(Time, Value, (std::max)(CurrentDefinition.Duration, 1.f)));
                    NormalizeSequenceCurve(Curve);
                    SequenceSelectedKey = static_cast<int32>(Curve.size()) - 1;
                    MarkDirty();
                }

                ImGui::SameLine();
                const bool bCanDelete = SequenceSelectedKey >= 0 && SequenceSelectedKey < static_cast<int32>(Curve.size());
                if (!bCanDelete)
                {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Delete Key") && bCanDelete)
                {
                    Curve.erase(Curve.begin() + SequenceSelectedKey);
                    SequenceSelectedKey = (std::min)(SequenceSelectedKey, static_cast<int32>(Curve.size()) - 1);
                    SequenceDragTarget = ESequenceEditorDragTarget::None;
                    MarkDirty();
                }
                if (!bCanDelete)
                {
                    ImGui::EndDisabled();
                }

                ImGui::SameLine();
                if (ImGui::Button("Fit View"))
                {
                    FitSequenceCurveView();
                }

                bool bChanged = DrawSequenceGraph(Curve);
                bChanged |= DrawSequenceKeyDetails(Curve);
                if (bChanged)
                {
                    MarkDirty();
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

bool FEditorCameraShakePanel::DrawSequenceGraph(FSequenceCameraShakeCurve& Curve)
{
    bool bChanged = false;
    const float ViewTimeRange = (std::max)(SequenceViewTimeMax - SequenceViewTimeMin, 0.001f);
    const float ViewValueRange = (std::max)(SequenceViewValueMax - SequenceViewValueMin, 0.001f);

    ImVec2 Available = ImGui::GetContentRegionAvail();
    const ImVec2 CanvasSize((std::max)(Available.x, 320.0f), CurveGraphHeight);
    ImGui::InvisibleButton("SequenceCurveGraph", CanvasSize, ImGuiButtonFlags_MouseButtonLeft);
    const bool bHovered = ImGui::IsItemHovered();
    const bool bActive = ImGui::IsItemActive();
    const ImVec2 CanvasMin = ImGui::GetItemRectMin();
    const ImVec2 CanvasMax = ImGui::GetItemRectMax();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    auto ToScreen = [&](float Time, float Value) -> ImVec2
    {
        const float X = CanvasMin.x + ((Time - SequenceViewTimeMin) / ViewTimeRange) * CanvasSize.x;
        const float Y = CanvasMax.y - ((Value - SequenceViewValueMin) / ViewValueRange) * CanvasSize.y;
        return ImVec2(X, Y);
    };

    auto FromScreen = [&](ImVec2 Position) -> ImVec2
    {
        const float Time = SequenceViewTimeMin + ((Position.x - CanvasMin.x) / CanvasSize.x) * ViewTimeRange;
        const float Value = SequenceViewValueMin + ((CanvasMax.y - Position.y) / CanvasSize.y) * ViewValueRange;
        return ImVec2(Time, Value);
    };

    DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(28, 30, 34, 255));
    DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(84, 90, 98, 255));

    for (int GridIndex = 1; GridIndex < 4; ++GridIndex)
    {
        const float X = CanvasMin.x + CanvasSize.x * (static_cast<float>(GridIndex) / 4.f);
        const float Y = CanvasMin.y + CanvasSize.y * (static_cast<float>(GridIndex) / 4.f);
        DrawList->AddLine(ImVec2(X, CanvasMin.y), ImVec2(X, CanvasMax.y), IM_COL32(58, 62, 68, 255));
        DrawList->AddLine(ImVec2(CanvasMin.x, Y), ImVec2(CanvasMax.x, Y), IM_COL32(58, 62, 68, 255));
    }

    if (SequenceViewValueMin <= 0.f && SequenceViewValueMax >= 0.f)
    {
        const ImVec2 A = ToScreen(SequenceViewTimeMin, 0.f);
        const ImVec2 B = ToScreen(SequenceViewTimeMax, 0.f);
        DrawList->AddLine(A, B, IM_COL32(112, 118, 126, 255), 1.5f);
    }

    const ImU32 CurveColor = SequenceChannelColor(SequenceSelectedChannel);
    if (!Curve.empty())
    {
        const int SampleCount = 128;
        ImVec2 Prev = ToScreen(SequenceViewTimeMin, USequenceCameraShakePattern::EvaluateCurve(Curve, SequenceViewTimeMin));
        for (int SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
        {
            const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
            const float Time = SequenceViewTimeMin + ViewTimeRange * Alpha;
            const ImVec2 Next = ToScreen(Time, USequenceCameraShakePattern::EvaluateCurve(Curve, Time));
            DrawList->AddLine(Prev, Next, CurveColor, 2.0f);
            Prev = Next;
        }
    }

    for (size_t KeyIndex = 0; KeyIndex < Curve.size(); ++KeyIndex)
    {
        const FSequenceCameraShakeKey& Key = Curve[KeyIndex];
        const ImVec2 KeyPos = ToScreen(Key.Time, Key.Value);
        const bool bSelected = SequenceSelectedKey == static_cast<int32>(KeyIndex);

        if (KeyIndex > 0)
        {
            const ImVec2 ArrivePos = ToScreen(Key.Time + Key.ArriveTime, Key.Value + Key.ArriveValue);
            DrawList->AddLine(ArrivePos, KeyPos, IM_COL32(154, 158, 166, 255), 1.0f);
            DrawList->AddCircleFilled(ArrivePos, CurveHandleRadius, IM_COL32(154, 158, 166, 255));
        }

        if (KeyIndex + 1 < Curve.size())
        {
            const ImVec2 LeavePos = ToScreen(Key.Time + Key.LeaveTime, Key.Value + Key.LeaveValue);
            DrawList->AddLine(KeyPos, LeavePos, IM_COL32(154, 158, 166, 255), 1.0f);
            DrawList->AddCircleFilled(LeavePos, CurveHandleRadius, IM_COL32(154, 158, 166, 255));
        }

        DrawList->AddCircleFilled(KeyPos, bSelected ? 6.5f : 5.0f, bSelected ? IM_COL32(255, 255, 255, 255) : CurveColor);
        DrawList->AddCircle(KeyPos, bSelected ? 7.5f : 6.0f, IM_COL32(22, 24, 28, 255), 0, 1.5f);
    }

    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 CurvePos = FromScreen(MousePos);
        Curve.push_back(MakeSequenceKey(ClampToRange(CurvePos.x, 0.f, (std::max)(CurrentDefinition.Duration, SequenceViewTimeMax)),
                                        CurvePos.y,
                                        (std::max)(CurrentDefinition.Duration, 1.f)));
        NormalizeSequenceCurve(Curve);
        SequenceSelectedKey = static_cast<int32>(Curve.size()) - 1;
        SequenceDragTarget = ESequenceEditorDragTarget::None;
        bChanged = true;
    }
    else if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        float BestDistance = CurveHitRadius * CurveHitRadius;
        SequenceDragTarget = ESequenceEditorDragTarget::None;
        SequenceDragKey = -1;

        for (size_t KeyIndex = 0; KeyIndex < Curve.size(); ++KeyIndex)
        {
            const FSequenceCameraShakeKey& Key = Curve[KeyIndex];
            if (KeyIndex > 0)
            {
                const float Distance = DistanceSquared(MousePos, ToScreen(Key.Time + Key.ArriveTime, Key.Value + Key.ArriveValue));
                if (Distance <= BestDistance)
                {
                    BestDistance = Distance;
                    SequenceDragTarget = ESequenceEditorDragTarget::ArriveHandle;
                    SequenceDragKey = static_cast<int32>(KeyIndex);
                }
            }
            if (KeyIndex + 1 < Curve.size())
            {
                const float Distance = DistanceSquared(MousePos, ToScreen(Key.Time + Key.LeaveTime, Key.Value + Key.LeaveValue));
                if (Distance <= BestDistance)
                {
                    BestDistance = Distance;
                    SequenceDragTarget = ESequenceEditorDragTarget::LeaveHandle;
                    SequenceDragKey = static_cast<int32>(KeyIndex);
                }
            }
        }

        for (size_t KeyIndex = 0; KeyIndex < Curve.size(); ++KeyIndex)
        {
            const FSequenceCameraShakeKey& Key = Curve[KeyIndex];
            const float Distance = DistanceSquared(MousePos, ToScreen(Key.Time, Key.Value));
            if (Distance <= BestDistance)
            {
                BestDistance = Distance;
                SequenceDragTarget = ESequenceEditorDragTarget::Key;
                SequenceDragKey = static_cast<int32>(KeyIndex);
            }
        }

        SequenceSelectedKey = SequenceDragKey;
    }

    if (bActive
        && SequenceDragTarget != ESequenceEditorDragTarget::None
        && SequenceDragKey >= 0
        && SequenceDragKey < static_cast<int32>(Curve.size()))
    {
        FSequenceCameraShakeKey& Key = Curve[SequenceDragKey];
        const ImVec2 CurvePos = FromScreen(MousePos);
        const float DurationLimit = CurrentDefinition.Duration > 0.f ? CurrentDefinition.Duration : (std::max)(SequenceViewTimeMax, 1.f);

        if (SequenceDragTarget == ESequenceEditorDragTarget::Key)
        {
            const float MinTime = SequenceDragKey > 0 ? Curve[SequenceDragKey - 1].Time + 0.001f : 0.f;
            const float MaxTime = SequenceDragKey + 1 < static_cast<int32>(Curve.size())
                                      ? Curve[SequenceDragKey + 1].Time - 0.001f
                                      : DurationLimit;
            Key.Time = ClampToRange(CurvePos.x, MinTime, (std::max)(MinTime, MaxTime));
            Key.Value = CurvePos.y;
        }
        else if (SequenceDragTarget == ESequenceEditorDragTarget::ArriveHandle)
        {
            const float PrevSpan = SequenceDragKey > 0 ? Key.Time - Curve[SequenceDragKey - 1].Time : 0.f;
            Key.ArriveTime = ClampToRange(CurvePos.x - Key.Time, -PrevSpan, 0.f);
            Key.ArriveValue = CurvePos.y - Key.Value;
        }
        else if (SequenceDragTarget == ESequenceEditorDragTarget::LeaveHandle)
        {
            const float NextSpan = SequenceDragKey + 1 < static_cast<int32>(Curve.size()) ? Curve[SequenceDragKey + 1].Time - Key.Time : 0.f;
            Key.LeaveTime = ClampToRange(CurvePos.x - Key.Time, 0.f, NextSpan);
            Key.LeaveValue = CurvePos.y - Key.Value;
        }

        NormalizeSequenceCurve(Curve);
        bChanged = true;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        SequenceDragTarget = ESequenceEditorDragTarget::None;
        SequenceDragKey = -1;
    }

    return bChanged;
}

bool FEditorCameraShakePanel::DrawSequenceKeyDetails(FSequenceCameraShakeCurve& Curve)
{
    if (SequenceSelectedKey < 0 || SequenceSelectedKey >= static_cast<int32>(Curve.size()))
    {
        ImGui::TextDisabled("No key selected.");
        return false;
    }

    bool bChanged = false;
    FSequenceCameraShakeKey& Key = Curve[SequenceSelectedKey];

    ImGui::SeparatorText("Selected Key");
    if (ImGui::BeginTable("CameraShakeSequenceKeyDetails", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, PropertyLabelWidth);
        ImGui::TableSetupColumn("Value");

        auto DrawKeyFloat = [&](const char* Label, float& Value, float Speed, float MinValue, float MaxValue, const char* Tooltip) -> bool
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            DrawLabelWithTooltip(Label, Tooltip);
            ImGui::TableNextColumn();
            ImGui::PushID(Label);
            ImGui::SetNextItemWidth(-1.0f);
            const bool bValueChanged = ImGui::DragFloat("##Value", &Value, Speed, MinValue, MaxValue, "%.3f");
            DrawPropertyTooltip(Tooltip);
            ImGui::PopID();
            return bValueChanged;
        };

        const float DurationLimit = CurrentDefinition.Duration > 0.f ? CurrentDefinition.Duration : (std::max)(SequenceViewTimeMax, 1.f);
        const float MinKeyTime = SequenceSelectedKey > 0 ? Curve[SequenceSelectedKey - 1].Time + 0.001f : 0.f;
        const float MaxKeyTime = SequenceSelectedKey + 1 < static_cast<int32>(Curve.size())
                                     ? Curve[SequenceSelectedKey + 1].Time - 0.001f
                                     : DurationLimit;

        if (DrawKeyFloat("Time", Key.Time, 0.01f, MinKeyTime, (std::max)(MinKeyTime, MaxKeyTime), "Key time in seconds."))
        {
            Key.Time = ClampToRange(Key.Time, MinKeyTime, (std::max)(MinKeyTime, MaxKeyTime));
            bChanged = true;
        }
        if (DrawKeyFloat("Value", Key.Value, 0.05f, -10000.0f, 10000.0f, SequenceChannelUnitLabel(SequenceSelectedChannel)))
        {
            bChanged = true;
        }

        const float PrevSpan = SequenceSelectedKey > 0 ? Key.Time - Curve[SequenceSelectedKey - 1].Time : 0.f;
        const float NextSpan = SequenceSelectedKey + 1 < static_cast<int32>(Curve.size()) ? Curve[SequenceSelectedKey + 1].Time - Key.Time : 0.f;
        if (DrawKeyFloat("Arrive Time", Key.ArriveTime, 0.01f, -PrevSpan, 0.f, "Incoming Bezier handle time offset."))
        {
            Key.ArriveTime = ClampToRange(Key.ArriveTime, -PrevSpan, 0.f);
            bChanged = true;
        }
        if (DrawKeyFloat("Arrive Value", Key.ArriveValue, 0.05f, -10000.0f, 10000.0f, "Incoming Bezier handle value offset."))
        {
            bChanged = true;
        }
        if (DrawKeyFloat("Leave Time", Key.LeaveTime, 0.01f, 0.f, NextSpan, "Outgoing Bezier handle time offset."))
        {
            Key.LeaveTime = ClampToRange(Key.LeaveTime, 0.f, NextSpan);
            bChanged = true;
        }
        if (DrawKeyFloat("Leave Value", Key.LeaveValue, 0.05f, -10000.0f, 10000.0f, "Outgoing Bezier handle value offset."))
        {
            bChanged = true;
        }

        ImGui::EndTable();
    }

    if (bChanged)
    {
        NormalizeSequenceCurve(Curve);
    }
    return bChanged;
}

void FEditorCameraShakePanel::EnsureDefaultSequenceCurves()
{
    const float EndTime = CurrentDefinition.Duration > 0.f ? CurrentDefinition.Duration : 1.f;
    for (FSequenceCameraShakeCurve& Curve : CurrentDefinition.Curves)
    {
        if (!Curve.empty())
        {
            NormalizeSequenceCurve(Curve);
            continue;
        }

        Curve.push_back(MakeSequenceKey(0.f, 0.f, EndTime));
        Curve.push_back(MakeSequenceKey(EndTime, 0.f, EndTime));
        Curve.front().ArriveTime = 0.f;
        Curve.back().LeaveTime = 0.f;
        NormalizeSequenceCurve(Curve);
    }
}

void FEditorCameraShakePanel::NormalizeSequenceCurve(FSequenceCameraShakeCurve& Curve)
{
    USequenceCameraShakePattern::NormalizeCurve(Curve, CurrentDefinition.Duration);
    if (SequenceSelectedKey >= static_cast<int32>(Curve.size()))
    {
        SequenceSelectedKey = static_cast<int32>(Curve.size()) - 1;
    }
    if (Curve.empty())
    {
        SequenceSelectedKey = -1;
    }
}

void FEditorCameraShakePanel::FitSequenceCurveView()
{
    FSequenceCameraShakeCurve& Curve = GetSelectedSequenceCurve();
    SequenceViewTimeMin = 0.f;
    SequenceViewTimeMax = (std::max)(CurrentDefinition.Duration, 1.f);

    float MinValue = 0.f;
    float MaxValue = 0.f;
    bool bHasValue = false;

    for (const FSequenceCameraShakeKey& Key : Curve)
    {
        const float Values[] = {
            Key.Value,
            Key.Value + Key.ArriveValue,
            Key.Value + Key.LeaveValue,
        };
        for (float Value : Values)
        {
            if (!bHasValue)
            {
                MinValue = Value;
                MaxValue = Value;
                bHasValue = true;
            }
            else
            {
                MinValue = (std::min)(MinValue, Value);
                MaxValue = (std::max)(MaxValue, Value);
            }
        }
        SequenceViewTimeMax = (std::max)(SequenceViewTimeMax, Key.Time);
    }

    if (!bHasValue)
    {
        MinValue = -1.f;
        MaxValue = 1.f;
    }
    if (std::abs(MaxValue - MinValue) < 0.001f)
    {
        MinValue -= 1.f;
        MaxValue += 1.f;
    }

    const float Padding = (MaxValue - MinValue) * 0.15f;
    SequenceViewValueMin = MinValue - Padding;
    SequenceViewValueMax = MaxValue + Padding;
}

FSequenceCameraShakeCurve& FEditorCameraShakePanel::GetSelectedSequenceCurve()
{
    SequenceSelectedChannel = (std::clamp)(SequenceSelectedChannel, 0, static_cast<int32>(SequenceCameraShakeChannelCount) - 1);
    return CurrentDefinition.Curves[static_cast<size_t>(SequenceSelectedChannel)];
}

void FEditorCameraShakePanel::StartPreview()
{
    StopPreview();

    if (!EditorEngine)
    {
        StatusMessage = "Preview failed: editor engine is missing.";
        return;
    }

    FLevelEditorViewportClient* ViewportClient = EditorEngine->GetActiveViewport();
    if (!ViewportClient)
    {
        const TArray<FLevelEditorViewportClient*>& Viewports = EditorEngine->GetLevelViewportClients();
        if (!Viewports.empty())
        {
            ViewportClient = Viewports.front();
        }
    }

    UCameraComponent* Camera = ViewportClient ? ViewportClient->GetCamera() : nullptr;
    if (!Camera)
    {
        StatusMessage = "Preview failed: active viewport camera is missing.";
        return;
    }

    CurrentDefinition.RootPatternClass = PatternClassFromType(CurrentDefinition.PatternType);
    UCameraShakeBase* ShakeInstance = FCameraShakeAssetManager::Get().CreateShakeInstance(CurrentDefinition, nullptr, CurrentPath);
    if (!ShakeInstance)
    {
        StatusMessage = "Preview failed: shake instance could not be created.";
        return;
    }

    Preview.Camera = Camera;
    Preview.ShakeInstance = ShakeInstance;
    Preview.BaseLocation = Camera->GetWorldLocation();
    Preview.BaseRotation = Camera->GetRelativeRotation();
    Preview.BaseFOVRadians = Camera->GetFOV();
    Preview.bActive = true;

    FCameraShakeBaseStartParams StartParams;
    StartParams.ShakeScale = 1.f;
    StartParams.PlaySpace = ECameraShakePlaySpace::CameraLocal;
    ShakeInstance->StartShake(StartParams);
    StatusMessage = "Previewing on active viewport.";
}

void FEditorCameraShakePanel::UpdatePreview(float DeltaTime)
{
    if (!Preview.bActive)
    {
        return;
    }

    if (!Preview.Camera || !Preview.ShakeInstance)
    {
        StopPreview();
        return;
    }

    FMinimalViewInfo POV;
    POV.Location = Preview.BaseLocation;
    POV.Rotation = Preview.BaseRotation;
    POV.FOV = Preview.BaseFOVRadians * RAD_TO_DEG;
    POV.AspectRatio = Preview.Camera->GetAspectRatio();
    POV.NearZ = Preview.Camera->GetNearPlane();
    POV.FarZ = Preview.Camera->GetFarPlane();
    POV.bOrthographic = Preview.Camera->IsOrthogonal();
    POV.OrthoWidth = Preview.Camera->GetOrthoWidth();

    Preview.ShakeInstance->UpdateAndApplyCameraShake(DeltaTime, 1.f, POV);

    Preview.Camera->SetWorldLocation(POV.Location);
    Preview.Camera->SetRelativeRotation(POV.Rotation);

    FCameraState CameraState = Preview.Camera->GetCameraState();
    CameraState.FOV = POV.FOV * DEG_TO_RAD;
    Preview.Camera->SetCameraState(CameraState);

    if (Preview.ShakeInstance->IsFinished())
    {
        StopPreview();
    }
}

void FEditorCameraShakePanel::MarkDirty()
{
    bDirty = true;
}

FString FEditorCameraShakePanel::MakeDisplayPath() const
{
    if (CurrentPath.empty())
    {
        return "Unsaved Camera Shake";
    }
    return CurrentPath;
}
