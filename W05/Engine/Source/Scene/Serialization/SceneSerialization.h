#pragma once

#include "Core/Containers/String.h"
#include "Core/Math/Transform/Rotator.h"
#include "Core/Math/Vector3.h"
#include <filesystem>
#include <memory>

class FScene;

namespace Engine::Scene::Serialization
{
    struct FSceneCameraData
    {
        FVector3 Location = FVector3::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        float    FOV = 60.0f;
        float    NearClip = 0.1f;
        float    FarClip = 1000.0f;
    };

    bool SerializeSceneToJson(const FScene &Scene, const FSceneCameraData &CameraData,
                              FString &OutJson, FString *OutErrorMessage = nullptr);

    bool SaveSceneToFile(const FScene &Scene, const FSceneCameraData &CameraData,
                         const std::filesystem::path &FilePath, FString *OutErrorMessage = nullptr);

    std::unique_ptr<FScene> DeserializeSceneFromJson(const FString    &JsonSource,
                                                     FSceneCameraData &OutCameraData,
                                                     FString          *OutErrorMessage = nullptr);

    std::unique_ptr<FScene> LoadSceneFromFile(const std::filesystem::path &FilePath,
                                              FSceneCameraData            &OutCameraData,
                                              FString *OutErrorMessage = nullptr);
} // namespace Engine::Scene::Serialization
