#pragma once

#include "Camera/Shakes/CameraShakeAssetManager.h"
#include "Editor/UI/EditorPanel.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"

#include <array>

class UCameraComponent;
class UCameraShakeBase;

class FEditorCameraShakePanel : public FEditorPanel
{
public:
    void Initialize(UEditorEngine* InEditorEngine) override;
    void Render(float DeltaTime) override;

    bool OpenAsset(const FString& Path);
    void StopPreview();

private:
    struct FPreviewState
    {
        UCameraShakeBase* ShakeInstance = nullptr;
        UCameraComponent* Camera = nullptr;
        FVector BaseLocation = FVector::ZeroVector;
        FRotator BaseRotation = FRotator::ZeroRotator;
        float BaseFOVRadians = 0.f;
        bool bActive = false;
    };

    void RefreshAssets();
    void NewAsset();
    bool SaveCurrent();
    bool SaveCurrentAs();
    void DrawAssetList();
    void DrawEditor();
    void DrawPatternControls();
    void DrawOscillatorTable();
    bool DrawOscillatorRow(const char* Label,
                           FCameraShakeAssetOscillator& Oscillator,
                           bool bDrawInitialOffset,
                           const char* ChannelTooltip,
                           const char* AmplitudeTooltip,
                           const char* FrequencyTooltip);
    void DrawSequenceCurveEditor();
    bool DrawSequenceGraph(FSequenceCameraShakeCurve& Curve);
    bool DrawSequenceKeyDetails(FSequenceCameraShakeCurve& Curve);
    void EnsureDefaultSequenceCurves();
    void NormalizeSequenceCurve(FSequenceCameraShakeCurve& Curve);
    void FitSequenceCurveView();
    FSequenceCameraShakeCurve& GetSelectedSequenceCurve();
    void StartPreview();
    void UpdatePreview(float DeltaTime);
    void MarkDirty();
    FString MakeDisplayPath() const;

private:
    enum class ESequenceEditorDragTarget : uint8
    {
        None,
        Key,
        ArriveHandle,
        LeaveHandle
    };

    TArray<FCameraShakeAssetListItem> AssetList;
    FCameraShakeAssetDefinition CurrentDefinition;
    FString CurrentPath;
    FString StatusMessage;
    bool bDirty = false;
    bool bRefreshRequested = true;
    FPreviewState Preview;

    int32 SequenceSelectedChannel = 0;
    int32 SequenceSelectedKey = -1;
    float SequenceViewTimeMin = 0.f;
    float SequenceViewTimeMax = 1.f;
    float SequenceViewValueMin = -1.f;
    float SequenceViewValueMax = 1.f;
    ESequenceEditorDragTarget SequenceDragTarget = ESequenceEditorDragTarget::None;
    int32 SequenceDragKey = -1;
};
