#include "Scene/Serialization/SceneSerialization.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Scene/Scene.h"
#include "ThirdParty/nlohmann/json.hpp"
#include <cctype>
#include <fstream>
#include <sstream>

namespace Engine::Scene::Serialization
{
    namespace
    {
        using json = nlohmann::json;

        FString TrimAscii(const FString &InText)
        {
            size_t Start = 0;
            while (Start < InText.size() && std::isspace(static_cast<unsigned char>(InText[Start])))
            {
                ++Start;
            }

            size_t End = InText.size();
            while (End > Start && std::isspace(static_cast<unsigned char>(InText[End - 1])))
            {
                --End;
            }

            return InText.substr(Start, End - Start);
        }

        FString RemoveTrailingCommas(const FString &InJsonText)
        {
            FString Result;
            Result.reserve(InJsonText.size());

            bool bInString = false;
            bool bEscaped = false;

            for (size_t i = 0; i < InJsonText.size(); ++i)
            {
                const char Ch = InJsonText[i];
                Result.push_back(Ch);

                if (bInString)
                {
                    if (bEscaped)
                    {
                        bEscaped = false;
                    }
                    else if (Ch == '\\')
                    {
                        bEscaped = true;
                    }
                    else if (Ch == '"')
                    {
                        bInString = false;
                    }
                    continue;
                }

                if (Ch == '"')
                {
                    bInString = true;
                    continue;
                }

                if (Ch != ',')
                {
                    continue;
                }

                size_t Cursor = i + 1;
                while (Cursor < InJsonText.size() &&
                       std::isspace(static_cast<unsigned char>(InJsonText[Cursor])))
                {
                    ++Cursor;
                }

                if (Cursor < InJsonText.size() &&
                    (InJsonText[Cursor] == '}' || InJsonText[Cursor] == ']'))
                {
                    Result.pop_back();
                }
            }

            return Result;
        }

        bool ReadVector3(const json &Node, FVector3 &OutVector)
        {
            if (!Node.is_array() || Node.size() != 3 || !Node[0].is_number() ||
                !Node[1].is_number() || !Node[2].is_number())
            {
                return false;
            }

            OutVector = FVector3(Node[0].get<float>(), Node[1].get<float>(), Node[2].get<float>());
            return true;
        }

        bool ReadScalarArray(const json &Node, float &OutValue)
        {
            if (!Node.is_array() || Node.empty() || !Node[0].is_number())
            {
                return false;
            }

            OutValue = Node[0].get<float>();
            return true;
        }

        bool ReadCameraData(const json &Root, FSceneCameraData &OutCameraData, FString &OutError)
        {
            const auto CameraIt = Root.find("PerspectiveCamera");
            if (CameraIt == Root.end() || !CameraIt->is_object())
            {
                OutError = "Missing or invalid PerspectiveCamera object.";
                return false;
            }

            const json &CameraNode = *CameraIt;

            FVector3 CameraLocation;
            if (!ReadVector3(CameraNode.value("Location", json()), CameraLocation))
            {
                OutError = "PerspectiveCamera.Location must be an array with 3 numeric values.";
                return false;
            }
            OutCameraData.Location = CameraLocation;

            FVector3 CameraRotation;
            if (!ReadVector3(CameraNode.value("Rotation", json()), CameraRotation))
            {
                OutError = "PerspectiveCamera.Rotation must be an array with 3 numeric values.";
                return false;
            }
            OutCameraData.Rotation = FRotator(CameraRotation.X, CameraRotation.Y, CameraRotation.Z);

            if (!ReadScalarArray(CameraNode.value("FOV", json()), OutCameraData.FOV))
            {
                OutError = "PerspectiveCamera.FOV must be a numeric array.";
                return false;
            }

            if (!ReadScalarArray(CameraNode.value("NearClip", json()), OutCameraData.NearClip))
            {
                OutError = "PerspectiveCamera.NearClip must be a numeric array.";
                return false;
            }

            if (!ReadScalarArray(CameraNode.value("FarClip", json()), OutCameraData.FarClip))
            {
                OutError = "PerspectiveCamera.FarClip must be a numeric array.";
                return false;
            }

            return true;
        }

        bool ReadStaticMeshPrimitive(const json &PrimitiveNode, UStaticMeshComponent &OutComponent,
                                     FString &OutError)
        {
            const FString Type = PrimitiveNode.value("Type", FString());
            if (Type != "StaticMeshComp")
            {
                OutError = "Only Type=StaticMeshComp is currently supported.";
                return false;
            }

            FVector3 Location;
            if (!ReadVector3(PrimitiveNode.value("Location", json()), Location))
            {
                OutError = "Primitive.Location must be an array with 3 numeric values.";
                return false;
            }

            FVector3 Rotation;
            if (!ReadVector3(PrimitiveNode.value("Rotation", json()), Rotation))
            {
                OutError = "Primitive.Rotation must be an array with 3 numeric values.";
                return false;
            }

            FVector3 Scale;
            if (!ReadVector3(PrimitiveNode.value("Scale", json()), Scale))
            {
                OutError = "Primitive.Scale must be an array with 3 numeric values.";
                return false;
            }

            const FString MeshPath = PrimitiveNode.value("ObjStaticMeshAsset", FString());
            if (MeshPath.empty())
            {
                OutError = "Primitive.ObjStaticMeshAsset must be a non-empty string.";
                return false;
            }

            OutComponent.SetStaticMeshPath(MeshPath);
            OutComponent.SetRelativeLocation(Location);
            OutComponent.SetRelativeRotation(FRotator(Rotation.X, Rotation.Y, Rotation.Z));
            OutComponent.SetRelativeScale3D(Scale);
            return true;
        }

    } // namespace

    bool SerializeSceneToJson(const FScene &Scene, const FSceneCameraData &CameraData,
                              FString &OutJson, FString *OutErrorMessage)
    {
        json Root = json::object();
        Root["NextUUID"] = static_cast<int32>(50000 + Scene.GetStaticMeshComponents().size() + 1);

        json CameraJson = json::object();
        CameraJson["Location"] = {CameraData.Location.X, CameraData.Location.Y, CameraData.Location.Z};
        CameraJson["Rotation"] = {CameraData.Rotation.Pitch, CameraData.Rotation.Yaw,
                                  CameraData.Rotation.Roll};
        CameraJson["FOV"] = {CameraData.FOV};
        CameraJson["NearClip"] = {CameraData.NearClip};
        CameraJson["FarClip"] = {CameraData.FarClip};
        Root["PerspectiveCamera"] = std::move(CameraJson);

        json   PrimitivesJson = json::object();
        size_t SerializedPrimitiveCount = 0;
        for (UStaticMeshComponent *Component : Scene.GetStaticMeshComponents())
        {
            if (Component == nullptr)
            {
                UE_LOG(SceneSerialization, ELogLevel::Warning,
                       "Skipping null static mesh component while serializing scene.");
                continue;
            }

            const FVector3 Location = Component->GetRelativeLocation();
            const FRotator Rotation = Component->GetRelativeRotation();
            const FVector3 Scale = Component->GetRelativeScale3D();

            json PrimitiveJson = json::object();
            PrimitiveJson["Type"] = "StaticMeshComp";
            PrimitiveJson["Location"] = {Location.X, Location.Y, Location.Z};
            PrimitiveJson["Rotation"] = {Rotation.Pitch, Rotation.Yaw, Rotation.Roll};
            PrimitiveJson["Scale"] = {Scale.X, Scale.Y, Scale.Z};
            PrimitiveJson["ObjStaticMeshAsset"] = Component->GetStaticMeshPath();

            const FString PrimitiveKey = std::to_string(10 + SerializedPrimitiveCount);
            PrimitivesJson[PrimitiveKey] = std::move(PrimitiveJson);
            ++SerializedPrimitiveCount;
        }

        Root["Primitives"] = std::move(PrimitivesJson);

        try
        {
            OutJson = Root.dump(2);
        }
        catch (const std::exception &Exception)
        {
            UE_LOG(SceneSerialization, ELogLevel::Error,
                   "Failed to serialize scene json text: %s", Exception.what());
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Failed to build scene json text.";
            }
            return false;
        }

        if (OutErrorMessage)
        {
            OutErrorMessage->clear();
        }

        UE_LOG(SceneSerialization, ELogLevel::Info,
               "Scene serialized successfully. StaticMeshComponents=%zu, SerializedPrimitives=%zu",
               Scene.GetStaticMeshComponents().size(), SerializedPrimitiveCount);
        return true;
    }

    bool SaveSceneToFile(const FScene &Scene, const FSceneCameraData &CameraData,
                         const std::filesystem::path &FilePath, FString *OutErrorMessage)
    {
        const FString FilePathUtf8 = FPaths::Utf8FromPath(FilePath);
        UE_LOG(SceneSerialization, ELogLevel::Info, "Saving scene file: %s", FilePathUtf8.c_str());
        FString JsonText;
        if (!SerializeSceneToJson(Scene, CameraData, JsonText, OutErrorMessage))
        {
            UE_LOG(SceneSerialization, ELogLevel::Error, "Failed to serialize scene for file: %s",
                   FilePathUtf8.c_str());
            return false;
        }
        std::ofstream Output(FilePath, std::ios::binary | std::ios::trunc);
        if (!Output.is_open())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Failed to open scene file for writing.";
            }
            UE_LOG(SceneSerialization, ELogLevel::Error,
                   "Failed to open scene file for writing: %s", FilePathUtf8.c_str());
            return false;
        }
        Output.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
        const bool bSaved = Output.good();
        UE_LOG(SceneSerialization, bSaved ? ELogLevel::Info : ELogLevel::Error,
               bSaved ? "Scene save completed: %s" : "Scene save failed while writing: %s",
               FilePathUtf8.c_str());
        return bSaved;
    }

    std::unique_ptr<FScene> DeserializeSceneFromJson(const FString    &JsonSource,
                                                     FSceneCameraData &OutCameraData,
                                                     FString          *OutErrorMessage)
    {
        const FString TrimmedText = TrimAscii(JsonSource);
        if (TrimmedText.empty())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Scene json is empty.";
            }
            return nullptr;
        }

        const FString SanitizedText = RemoveTrailingCommas(TrimmedText);

        json Root;
        try
        {
            Root = json::parse(SanitizedText);
        }
        catch (const std::exception &Exception)
        {
            UE_LOG(SceneSerialization, ELogLevel::Error, "Failed to parse scene json: %s",
                   Exception.what());
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Failed to parse scene json.";
            }
            return nullptr;
        }

        FString ParseError;
        if (!ReadCameraData(Root, OutCameraData, ParseError))
        {
            UE_LOG(SceneSerialization, ELogLevel::Error, "Invalid camera data: %s",
                   ParseError.c_str());
            if (OutErrorMessage)
            {
                *OutErrorMessage = ParseError;
            }
            return nullptr;
        }

        const auto PrimitivesIt = Root.find("Primitives");
        if (PrimitivesIt == Root.end() || !PrimitivesIt->is_object())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Missing or invalid Primitives object.";
            }
            return nullptr;
        }

        std::unique_ptr<FScene> LoadedScene = std::make_unique<FScene>();
        LoadedScene->ReserveStaticMeshComponents(PrimitivesIt->size());

        for (auto It = PrimitivesIt->begin(); It != PrimitivesIt->end(); ++It)
        {
            const FString PrimitiveId = It.key();
            const json   &PrimitiveNode = It.value();

            if (!PrimitiveNode.is_object())
            {
                UE_LOG(SceneSerialization, ELogLevel::Warning,
                       "Skipping primitive '%s': node is not an object.", PrimitiveId.c_str());
                continue;
            }

            auto   *Component = LoadedScene->AllocateStaticMeshComponent();
            FString PrimitiveError;
            if (!ReadStaticMeshPrimitive(PrimitiveNode, *Component, PrimitiveError))
            {
                UE_LOG(SceneSerialization, ELogLevel::Warning, "Skipping primitive '%s': %s",
                       PrimitiveId.c_str(), PrimitiveError.c_str());
                continue;
            }

            LoadedScene->AddStaticMeshComponent(Component);
        }

        UE_LOG(SceneSerialization, ELogLevel::Info,
               "Scene deserialized successfully. StaticMeshComponents=%zu",
               LoadedScene->GetStaticMeshComponents().size());

        if (OutErrorMessage)
        {
            OutErrorMessage->clear();
        }
        return LoadedScene;
    }

    std::unique_ptr<FScene> LoadSceneFromFile(const std::filesystem::path &FilePath,
                                              FSceneCameraData            &OutCameraData,
                                              FString                     *OutErrorMessage)
    {
        const FString FilePathUtf8 = FPaths::Utf8FromPath(FilePath);
        UE_LOG(SceneSerialization, ELogLevel::Info, "Loading scene file: %s", FilePathUtf8.c_str());
        std::ifstream Input(FilePath, std::ios::binary);
        if (!Input.is_open())
        {
            if (OutErrorMessage)
            {
                *OutErrorMessage = "Failed to open scene file for reading.";
            }
            UE_LOG(SceneSerialization, ELogLevel::Error,
                   "Failed to open scene file for reading: %s", FilePathUtf8.c_str());
            return nullptr;
        }

        FString JsonText((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
        UE_LOG(SceneSerialization, ELogLevel::Debug, "Read scene json bytes: %zu", JsonText.size());

        return DeserializeSceneFromJson(JsonText, OutCameraData, OutErrorMessage);
    }
} // namespace Engine::Scene::Serialization
