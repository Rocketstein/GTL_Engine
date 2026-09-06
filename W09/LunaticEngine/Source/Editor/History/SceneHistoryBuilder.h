#pragma once

#include "Editor/History/SceneHistoryTypes.h"

class UEditorEngine;

class FSceneHistoryBuilder
{
public:
	static FTrackedSceneSnapshot CaptureSnapshot(const UEditorEngine& EditorEngine);
	static bool HasMeaningfulDelta(const FTrackedSceneSnapshot& Before, const FTrackedSceneSnapshot& After);
	static FTrackedSceneChange BuildChange(const FTrackedSceneSnapshot& Before, const FTrackedSceneSnapshot& After);
	static TArray<uint32> GetChangedActorUUIDs(const FTrackedSceneChange& Change, bool bSelectAfterChange);
};
