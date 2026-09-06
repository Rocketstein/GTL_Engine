#include "Overlay/DebugOverlayUI.h"
#include "ApplicationCore/InputState.h"
#include "Asset/Manager/AssetCacheManager.h"
#include "Core/Logging/LogMacros.h"
#include "Core/Misc/Paths.h"
#include "Core/Stats/TimingStats.h"
#include "Engine/Asset/AssetObjectManager.h"
#include "Engine/Component/PrimitiveComponent.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineStatics.h"
#include "EngineGlobals.h"
#include "Renderer/D3D11/D3D11RendererModule.h"
#include "Renderer/SceneView.h"
#include "Scene/Scene.h"
#include "Scene/SceneAssetBinder.h"
#include "Scene/Serialization/SceneSerialization.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Viewport/ViewportClient.h"
#include <algorithm>
#include <cctype>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <commdlg.h>
#include <windows.h>

#pragma region Local Helpers

namespace
{
    constexpr float ControlPanelWidth = 420.0f;

    bool ContainsCaseInsensitive(const FString &Text, const char *RawKeyword)
    {
        if (RawKeyword == nullptr || RawKeyword[0] == '\0')
        {
            return true;
        }

        FString Keyword = RawKeyword;
        auto    ToLower = [](unsigned char C) { return static_cast<char>(std::tolower(C)); };
        std::transform(Keyword.begin(), Keyword.end(), Keyword.begin(), ToLower);

        FString LowerText = Text;
        std::transform(LowerText.begin(), LowerText.end(), LowerText.begin(), ToLower);
        return LowerText.find(Keyword) != FString::npos;
    }

    ImVec4 GetLogTextColor(ELogLevel Level)
    {
        switch (Level)
        {
        case ELogLevel::Verbose:
            return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);
        case ELogLevel::Debug:
            return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        case ELogLevel::Info:
            return ImVec4(0.00f, 1.00f, 1.00f, 1.0f);
        case ELogLevel::Warning:
            return ImVec4(1.00f, 1.00f, 0.00f, 1.0f);
        case ELogLevel::Error:
            return ImVec4(1.00f, 0.20f, 0.20f, 1.0f);
        default:
            return ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
        }
    }

    void SeparatorTextWithSpacing(const char *Label, float Top = 6.0f, float Bottom = 6.0f)
    {
        ImGui::Dummy(ImVec2(0.0f, Top));
        ImGui::SeparatorText(Label);
        ImGui::Dummy(ImVec2(0.0f, Bottom));
    }

} // namespace

#pragma endregion

#pragma region FStatsPanel

void FStatsPanel::Render()
{
    if (StatMode == EStatMode::None)
    {
        return;
    }

    ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    const float    Margin = 10.0f;
    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_AlwaysAutoResize;

    const bool bShowLeftFpsPanel = true;
    const bool bRenderRightPanel = bShowRightPanel;

    if (bShowLeftFpsPanel)
    {
        ImGui::SetNextWindowPos(ImVec2(MainViewport->WorkPos.x + Margin, MainViewport->WorkPos.y + Margin),
                                ImGuiCond_Always, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.92f));
        if (ImGui::Begin("FPS Stats Panel", nullptr, Flags))
        {
            if (ViewportClient != nullptr)
            {
                const FViewportPerformanceStats &Perf = ViewportClient->GetPerformanceStats();
                const double Fps = (Perf.LastFrameTimeMs > 0.0) ? (1000.0 / Perf.LastFrameTimeMs) : 0.0;
                UpdateRecentFps(Perf.LastFrameTimeMs);
                const double OneSecondAverageFps =
                    (RecentFrameTimesTotalMs > 0.0)
                        ? (1000.0 * static_cast<double>(RecentFrameTimesMs.size()) /
                           RecentFrameTimesTotalMs)
                        : 0.0;

                ImGui::Text("FPS: %.0f (%.0fms)", Fps, Perf.LastFrameTimeMs);
                ImGui::Text("1s Avg FPS: %.1f", OneSecondAverageFps);
                ImGui::Text("Last Picking: %.6f ms", Perf.LastPickTimeMs);
                ImGui::Text("Total Picking Count: %llu",
                            static_cast<unsigned long long>(Perf.TotalPickCount));
                ImGui::Text("Total Picking Time: %.6f ms", Perf.TotalPickTimeMs);
            }
            else
            {
                ImGui::TextUnformatted("Viewport stats unavailable");
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (bRenderRightPanel)
    {
        ImGui::SetNextWindowPos(
            ImVec2(MainViewport->WorkPos.x + MainViewport->WorkSize.x - Margin,
                   MainViewport->WorkPos.y + Margin),
            ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.92f));

        if (ImGui::Begin("Detail Stats Panel (F3)", nullptr, Flags))
        {
            if (ViewportClient != nullptr)
            {
                ImGui::TextDisabled("Detail Stats Shortcut: F3");
                ImGui::Separator();

                const FViewportPerformanceStats &Perf = ViewportClient->GetPerformanceStats();

                const uint64 CpuTotal = Perf.LastCpuOcclusionCullingTotalObjects;
                const uint64 CpuVisible = Perf.LastCpuOcclusionCullingVisibleObjects;
                const uint64 CpuCulled = Perf.LastCpuOcclusionCullingCulledObjects;
                const double CpuCulledRatioPercent =
                    (CpuTotal > 0)
                        ? (100.0 * static_cast<double>(CpuCulled) / static_cast<double>(CpuTotal))
                        : 0.0;
                const double CpuVisibleRatioPercent =
                    (CpuTotal > 0)
                        ? (100.0 * static_cast<double>(CpuVisible) / static_cast<double>(CpuTotal))
                        : 0.0;

                const uint64 GpuTotal = Perf.LastGpuOcclusionCullingTotalObjects;
                const uint64 GpuVisible = Perf.LastGpuOcclusionCullingVisibleObjects;
                const uint64 GpuCulled = Perf.LastGpuOcclusionCullingCulledObjects;
                const double GpuCulledRatioPercent =
                    (GpuTotal > 0)
                        ? (100.0 * static_cast<double>(GpuCulled) / static_cast<double>(GpuTotal))
                        : 0.0;
                const double GpuVisibleRatioPercent =
                    (GpuTotal > 0)
                        ? (100.0 * static_cast<double>(GpuVisible) / static_cast<double>(GpuTotal))
                        : 0.0;
                const FCullingControlSettings CullingSettings =
                    ViewportClient->GetCullingControlSettings();

                if (ImGui::CollapsingHeader("CPU Culling Stat", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const uint64 WorldObjectCount = Perf.LastFrustumCullingTotalObjects;
                    const uint64 NodeFrustumObjectInput =
                        CullingSettings.bCpuNodeFrustumCulling ? WorldObjectCount : 0;
                    const uint64 NodeOcclusionObjectInput =
                        CullingSettings.bCpuNodeOcclusionCulling
                            ? ((WorldObjectCount > Perf.LastCpuNodeFrustumCulledObjects)
                                   ? (WorldObjectCount - Perf.LastCpuNodeFrustumCulledObjects)
                                   : 0)
                            : 0;
                    const ImVec4 EnabledColor = ImVec4(0.40f, 0.95f, 0.40f, 1.0f);
                    const ImVec4 DisabledColor = ImVec4(1.00f, 0.35f, 0.35f, 1.0f);
                    auto DrawStatusBullet = [&](bool bEnabled, const char *DisabledLabel,
                                                const char *EnabledFormat, auto... EnabledArgs)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, bEnabled ? EnabledColor : DisabledColor);
                        if (!bEnabled)
                        {
                            ImGui::BulletText("%s: Disabled", DisabledLabel);
                        }
                        else
                        {
                            ImGui::BulletText(EnabledFormat, EnabledArgs...);
                        }
                        ImGui::PopStyleColor();
                    };

                    ImGui::TextUnformatted("[CPU Culling Stages]");
                    DrawStatusBullet(CullingSettings.bCpuNodeFrustumCulling, "Node Frustum",
                                     "Node Frustum: nodes tested=%llu, culled=%llu | objects input=%llu, culled=%llu",
                                     static_cast<unsigned long long>(Perf.LastCpuNodeFrustumTested),
                                     static_cast<unsigned long long>(Perf.LastCpuNodeFrustumCulled),
                                     static_cast<unsigned long long>(NodeFrustumObjectInput),
                                     static_cast<unsigned long long>(Perf.LastCpuNodeFrustumCulledObjects));
                    DrawStatusBullet(CullingSettings.bCpuNodeOcclusionCulling, "Node Occlusion",
                                     "Node Occlusion: nodes tested=%llu, culled=%llu | objects input=%llu, culled=%llu",
                                     static_cast<unsigned long long>(Perf.LastCpuNodeOcclusionTested),
                                     static_cast<unsigned long long>(Perf.LastCpuNodeOcclusionCulled),
                                     static_cast<unsigned long long>(NodeOcclusionObjectInput),
                                     static_cast<unsigned long long>(Perf.LastCpuNodeOcclusionCulledObjects));
                    DrawStatusBullet(CullingSettings.bCpuObjectFrustumCulling, "Object Frustum",
                                     "Object Frustum: input=%llu, culled=%llu",
                                     static_cast<unsigned long long>(Perf.LastCpuObjectFrustumInput),
                                     static_cast<unsigned long long>(Perf.LastCpuObjectFrustumCulled));
                    DrawStatusBullet(CullingSettings.bCpuObjectOcclusionCulling &&
                                         Perf.bLastCpuOcclusionPassExecuted, "Object Occlusion",
                                     "Object Occlusion: input=%llu, visible=%llu, culled=%llu",
                                     static_cast<unsigned long long>(CpuTotal),
                                     static_cast<unsigned long long>(CpuVisible),
                                     static_cast<unsigned long long>(CpuCulled));
                    if (Perf.LastCpuOcclusionBufferWidth > 0 && Perf.LastCpuOcclusionBufferHeight > 0)
                    {
                        ImGui::BulletText("CPU Occlusion Buffer: %llux%llu",
                                          static_cast<unsigned long long>(Perf.LastCpuOcclusionBufferWidth),
                                          static_cast<unsigned long long>(Perf.LastCpuOcclusionBufferHeight));
                    }
                    else
                    {
                        ImGui::BulletText("CPU Occlusion Buffer: n/a");
                    }

                    if (!CullingSettings.bCpuDistanceLodCulling)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, DisabledColor);
                        ImGui::BulletText("Distance / LOD: Disabled");
                        ImGui::PopStyleColor();
                    }
                    else if (!Perf.bLastCpuDistanceLodPassImplemented)
                    {
                        ImGui::BulletText("Distance / LOD: not implemented yet (placeholder)");
                    }
                    else
                    {
                        ImGui::BulletText("Distance / LOD: input=%llu, culled=%llu",
                                          static_cast<unsigned long long>(Perf.LastCpuDistanceLodInput),
                                          static_cast<unsigned long long>(Perf.LastCpuDistanceLodCulled));
                    }
                    const uint64 TotalObjects = Perf.LastFrustumCullingTotalObjects;
                    const uint64 VisibleObjects = Perf.LastFrustumCullingVisibleObjects;
                    const uint64 CulledObjects = Perf.LastFrustumCullingCulledObjects;
                    const double FrustumCulledRatioPercent =
                        (TotalObjects > 0)
                            ? (100.0 * static_cast<double>(CulledObjects) /
                               static_cast<double>(TotalObjects))
                            : 0.0;
                    ImGui::BulletText(
                        "Final CPU Culling Result: input=%llu, visible=%llu, culled=%llu (%.1f%%)",
                                      static_cast<unsigned long long>(TotalObjects),
                                      static_cast<unsigned long long>(VisibleObjects),
                                      static_cast<unsigned long long>(CulledObjects),
                                      FrustumCulledRatioPercent);
                    ImGui::Text("CPU Object-Occlusion Visible Ratio: %.1f%%", CpuVisibleRatioPercent);
                    ImGui::Text("CPU Object-Occlusion Ratio: %.1f%%", CpuCulledRatioPercent);
                }

                if (ImGui::CollapsingHeader("GPU Culling Stat", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::TextUnformatted("[GPU Culling Stages]");
                    if (!CullingSettings.bGpuHiZOcclusionCulling || !Perf.bLastGpuHiZOcclusionPassExecuted)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.35f, 0.35f, 1.0f));
                        ImGui::BulletText("GPU Hi-Z Occlusion: Disabled");
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::BulletText("GPU Hi-Z Occlusion: input=%llu, visible=%llu, culled=%llu",
                                          static_cast<unsigned long long>(GpuTotal),
                                          static_cast<unsigned long long>(GpuVisible),
                                          static_cast<unsigned long long>(GpuCulled));
                    }

                    ImGui::Text("GPU Hi-Z Visible Ratio: %.1f%%", GpuVisibleRatioPercent);
                    ImGui::Text("GPU Hi-Z Occlusion Ratio: %.1f%%", GpuCulledRatioPercent);
                }

                if (ImGui::CollapsingHeader("Timing Stat", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const TArray<FTimingStatEntry> Entries = FTimingStats::GetEntries();
                    if (Entries.empty())
                    {
                        ImGui::TextUnformatted("No timing stats recorded yet.");
                    }
                    else
                    {
                        auto FindEntry = [&Entries](const char *Name) -> const FTimingStatEntry *
                        {
                            const auto It =
                                std::find_if(Entries.begin(), Entries.end(),
                                             [Name](const FTimingStatEntry &Entry)
                                             { return Entry.Name == Name; });
                            return (It != Entries.end()) ? &(*It) : nullptr;
                        };

                        const FTimingStatEntry *ExtractEntry =
                            FindEntry("Viewport.Culling.ExtractFrustumPlanes");
                        const FTimingStatEntry *CullEntry =
                            FindEntry("Viewport.Culling.CullAndSortWithBVH8");
                        const FTimingStatEntry *CpuOcclusionEntry =
                            FindEntry("Viewport.Culling.CPUOcclusion");
                        const FTimingStatEntry *BuildEntry =
                            FindEntry("Viewport.Culling.BuildDrawItems");

                        if (ExtractEntry != nullptr || CullEntry != nullptr || CpuOcclusionEntry != nullptr ||
                            BuildEntry != nullptr)
                        {
                            ImGui::TextUnformatted("[Culling Pipeline Breakdown]");
                            if (ExtractEntry != nullptr)
                            {
                                ImGui::Text("Frustum Planes: %.4f ms (avg %.4f)",
                                            ExtractEntry->LastTimeMs, ExtractEntry->AverageTimeMs);
                            }
                            if (CullEntry != nullptr)
                            {
                                ImGui::Text("BVH8 Cull+Sort: %.4f ms (avg %.4f)",
                                            CullEntry->LastTimeMs, CullEntry->AverageTimeMs);
                            }
                            if (CpuOcclusionEntry != nullptr)
                            {
                                ImGui::Text("CPU Occlusion: %.4f ms (avg %.4f)",
                                            CpuOcclusionEntry->LastTimeMs,
                                            CpuOcclusionEntry->AverageTimeMs);
                            }
                            if (BuildEntry != nullptr)
                            {
                                ImGui::Text("Build DrawItems: %.4f ms (avg %.4f)",
                                            BuildEntry->LastTimeMs, BuildEntry->AverageTimeMs);
                            }
                            ImGui::Separator();
                        }

                        TArray<const FTimingStatEntry *> MeaningfulEntries;
                        MeaningfulEntries.reserve(Entries.size());
                        for (const FTimingStatEntry &Entry : Entries)
                        {
                            if (Entry.Name == "Viewport.Draw" || Entry.Name == "Viewport.RenderFrame" ||
                                Entry.Name == "Viewport.Culling.Skipped")
                            {
                                continue;
                            }
                            MeaningfulEntries.push_back(&Entry);
                        }

                        if (MeaningfulEntries.empty())
                        {
                            ImGui::TextUnformatted(
                                "No detailed timings yet. Move camera/scene to populate culling timings.");
                        }
                        else
                        {
                            std::sort(MeaningfulEntries.begin(), MeaningfulEntries.end(),
                                      [](const FTimingStatEntry *A, const FTimingStatEntry *B)
                                      { return A->AverageTimeMs > B->AverageTimeMs; });

                            const size_t MaxCount = std::min<size_t>(MeaningfulEntries.size(), 5);
                            for (size_t Index = 0; Index < MaxCount; ++Index)
                            {
                                const FTimingStatEntry *Entry = MeaningfulEntries[Index];
                                ImGui::Text("%s | Last %.4f ms | Avg %.4f ms | Calls %llu",
                                            Entry->Name.c_str(), Entry->LastTimeMs,
                                            Entry->AverageTimeMs,
                                            static_cast<unsigned long long>(Entry->CallCount));
                            }
                        }
                    }
                }
            }
            else
            {
                ImGui::TextUnformatted("Viewport stats unavailable");
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
    }
}

void FStatsPanel::UpdateRecentFps(double LastFrameTimeMs)
{
    if (LastFrameTimeMs <= 0.0)
    {
        return;
    }

    RecentFrameTimesMs.push_back(LastFrameTimeMs);
    RecentFrameTimesTotalMs += LastFrameTimeMs;

    while (!RecentFrameTimesMs.empty() && RecentFrameTimesTotalMs > 1000.0)
    {
        RecentFrameTimesTotalMs -= RecentFrameTimesMs.front();
        RecentFrameTimesMs.pop_front();
    }
}

#pragma endregion

#pragma region FControlPanel

void FControlPanel::Render(FDebugOverlayUI &Owner)
{
    ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    const float    Margin = 10.0f;
    const float    Width = ControlPanelWidth;
    const float    TopOffset = 150.0f;
    const float    Height = MainViewport->WorkSize.y - TopOffset - Margin;

    ImGui::SetNextWindowPos(
        ImVec2(MainViewport->WorkPos.x + Margin, MainViewport->WorkPos.y + TopOffset),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(Width, Height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("Control Panel (F1)", nullptr, Flags))
    {
        ImGui::End();
        return;
    }

    if (Owner.ViewportClient != nullptr)
    {
        ImGui::PushItemWidth(170.0f);

#pragma region Scene Management
        SeparatorTextWithSpacing("Scene Management");
        ImGui::BeginGroup();
        ImGui::Text("FileName: %s", Owner.SceneName.c_str());

        std::filesystem::path PickedPath;

        const float FullWidth = ImGui::GetContentRegionAvail().x;
        const float ButtonSpacing = ImGui::GetStyle().ItemSpacing.x;
        const float ButtonWidth = (FullWidth - ButtonSpacing) * 0.5f;
        const float ButtonHeight = 0.0f;

        if (ImGui::Button("New Scene", ImVec2(ButtonWidth, ButtonHeight)))
        {
            UE_LOG(DebugOverlay, ELogLevel::Info, "New Scene requested.");

            FScene *RuntimeScene = (Owner.Engine != nullptr) ? Owner.Engine->GetScene() : nullptr;
            if (RuntimeScene == nullptr)
            {
                UE_LOG(DebugOverlay, ELogLevel::Error, "Runtime scene is not available.");
            }
            else
            {
                RuntimeScene->Clear(ESceneMemoryPolicy::ReleaseExcess);
                RuntimeScene->MarkSceneDirty();

                if (Owner.ViewportClient != nullptr)
                {
                    Owner.ViewportClient->FlushGizmo();
                }

                Owner.SceneName = "Untitled";
                Owner.CurrentSceneFilePath.clear();
                UE_LOG(DebugOverlay, ELogLevel::Info, "Created empty scene.");
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Scene", ImVec2(ButtonWidth, ButtonHeight)))
        {
            UE_LOG(DebugOverlay, ELogLevel::Info, "Load Scene requested.");
            if (Owner.OpenSceneFileDialog(false, PickedPath))
            {
                Engine::Scene::Serialization::FSceneCameraData CameraData{};
                FString                                        ErrorMessage;
                std::unique_ptr<FScene>                        LoadedScene =
                    Engine::Scene::Serialization::LoadSceneFromFile(PickedPath, CameraData,
                                                                    &ErrorMessage);

                if (LoadedScene)
                {
                    FScene *RuntimeScene =
                        (Owner.Engine != nullptr) ? Owner.Engine->GetScene() : nullptr;
                    if (RuntimeScene == nullptr)
                    {
                        UE_LOG(DebugOverlay, ELogLevel::Error, "Runtime scene is not available.");
                        ImGui::End();
                        return;
                    }

                    RuntimeScene->MoveFrom(std::move(*LoadedScene),
                                           ESceneMemoryPolicy::ReleaseExcess);

                    if (Owner.ViewportClient != nullptr)
                    {
                        Owner.ViewportClient->SetCameraLocation(CameraData.Location);
                        Owner.ViewportClient->SetCameraRotation(CameraData.Rotation);
                        Owner.ViewportClient->SetPerspectiveCameraParams(
                            CameraData.FOV, CameraData.NearClip, CameraData.FarClip);
                        Owner.ViewportClient->FlushGizmo();
                        UE_LOG(DebugOverlay, ELogLevel::Info,
                               "Applied loaded scene camera. Location=(%.3f, %.3f, %.3f), "
                               "Rotation=(%.3f, %.3f, %.3f), FOV=%.3f, Near=%.3f, Far=%.3f",
                               CameraData.Location.X, CameraData.Location.Y, CameraData.Location.Z,
                               CameraData.Rotation.Pitch, CameraData.Rotation.Yaw,
                               CameraData.Rotation.Roll, CameraData.FOV, CameraData.NearClip,
                               CameraData.FarClip);
                    }

                    Owner.PreloadSceneAssetsForLogging(RuntimeScene);

                    RuntimeScene->MarkSceneDirty();

                    Owner.SceneName = FPaths::Utf8FromPath(PickedPath.filename());
                    Owner.CurrentSceneFilePath = PickedPath;
                    const FString PickedPathUtf8 = FPaths::Utf8FromPath(PickedPath);
                    UE_LOG(DebugOverlay, ELogLevel::Info,
                           "Scene loaded successfully. File=%s, StaticMeshComponents=%zu",
                           PickedPathUtf8.c_str(), RuntimeScene->GetStaticMeshComponents().size());

                    if (RuntimeScene->GetStaticMeshComponents().empty())
                    {
                        UE_LOG(DebugOverlay, ELogLevel::Warning,
                               "Loaded scene is empty. If this is unexpected, verify scene "
                               "serialization implementation and source file contents.");
                    }
                }
                else
                {
                    UE_LOG(DebugOverlay, ELogLevel::Error, "%s", ErrorMessage.c_str());
                }
            }
            else
            {
                UE_LOG(DebugOverlay, ELogLevel::Debug, "Load scene dialog cancelled.");
            }
        }

        if (ImGui::Button("Save Scene", ImVec2(ButtonWidth, ButtonHeight)) &&
            Owner.Engine != nullptr && Owner.Engine->GetScene() != nullptr)
        {
            UE_LOG(DebugOverlay, ELogLevel::Info, "Save Scene requested.");
            if (Owner.CurrentSceneFilePath.empty())
            {
                UE_LOG(DebugOverlay, ELogLevel::Warning,
                       "Save Scene failed: no loaded scene file path. Use Save Scene As first.");
            }
            else
            {
                Engine::Scene::Serialization::FSceneCameraData CameraData{};
                CameraData.Location = Owner.ViewportClient->GetCameraLocation();
                CameraData.Rotation = Owner.ViewportClient->GetCameraRotation();
                CameraData.FOV = Owner.ViewportClient->GetCameraFovDegrees();
                CameraData.NearClip = Owner.ViewportClient->GetCameraNearPlane();
                CameraData.FarClip = Owner.ViewportClient->GetCameraFarPlane();
                FString    ErrorMessage;
                const bool bSaved = Engine::Scene::Serialization::SaveSceneToFile(
                    *Owner.Engine->GetScene(), CameraData, Owner.CurrentSceneFilePath,
                    &ErrorMessage);
                if (bSaved)
                {
                    const FString SavedPathUtf8 = FPaths::Utf8FromPath(Owner.CurrentSceneFilePath);
                    UE_LOG(DebugOverlay, ELogLevel::Info,
                           "Scene saved successfully to currently loaded file: %s",
                           SavedPathUtf8.c_str());
                }
                else
                {
                    const FString SavedPathUtf8 = FPaths::Utf8FromPath(Owner.CurrentSceneFilePath);
                    UE_LOG(DebugOverlay, ELogLevel::Error,
                           "Save Scene failed for currently loaded file: %s, reason: %s",
                           SavedPathUtf8.c_str(), ErrorMessage.c_str());
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Scene As", ImVec2(ButtonWidth, ButtonHeight)) &&
            Owner.Engine != nullptr && Owner.Engine->GetScene() != nullptr)
        {
            UE_LOG(DebugOverlay, ELogLevel::Info, "Save Scene As requested.");
            if (Owner.OpenSceneFileDialog(true, PickedPath))
            {
                Engine::Scene::Serialization::FSceneCameraData CameraData{};
                CameraData.Location = Owner.ViewportClient->GetCameraLocation();
                CameraData.Rotation = Owner.ViewportClient->GetCameraRotation();
                CameraData.FOV = Owner.ViewportClient->GetCameraFovDegrees();
                CameraData.NearClip = Owner.ViewportClient->GetCameraNearPlane();
                CameraData.FarClip = Owner.ViewportClient->GetCameraFarPlane();
                FString    ErrorMessage;
                const bool bSaved = Engine::Scene::Serialization::SaveSceneToFile(
                    *Owner.Engine->GetScene(), CameraData, PickedPath, &ErrorMessage);
                if (bSaved)
                {
                    Owner.SceneName = FPaths::Utf8FromPath(PickedPath.filename());
                    Owner.CurrentSceneFilePath = PickedPath;
                    UE_LOG(DebugOverlay, ELogLevel::Info, "Scene saved successfully as new file.");
                }
                else
                {
                    UE_LOG(DebugOverlay, ELogLevel::Error, "%s", ErrorMessage.c_str());
                }
            }
        }

        ImGui::EndGroup();
        ImGui::Spacing();
#pragma endregion

#pragma region Editor Overlay
        SeparatorTextWithSpacing("Editor Overlay");
        ImGui::BeginGroup();
        float GridSpacing = Owner.ViewportClient->GetGridSpacing();
        if (ImGui::SliderFloat("Grid Spacing", &GridSpacing, 10.0f, 500.0f, "%.1f"))
        {
            Owner.ViewportClient->SetGridSpacing(GridSpacing);
        }

        bool bGizmoHoveringEnabled = Owner.ViewportClient->IsGizmoHoveringEnabled();
        if (ImGui::Checkbox("Gizmo Hovering", &bGizmoHoveringEnabled))
        {
            Owner.ViewportClient->SetGizmoHoveringEnabled(bGizmoHoveringEnabled);
        }

        ImGui::EndGroup();
        ImGui::Spacing();
#pragma endregion

#pragma region Show Flags
        SeparatorTextWithSpacing("Show Flags");
        ImGui::BeginGroup();
        bool bShowGrid = Owner.ViewportClient->IsShowGrid();
        bool bShowWorldAxis = Owner.ViewportClient->IsShowWorldAxis();
        bool bShowGizmo = Owner.ViewportClient->IsShowGizmo();

        if (ImGui::BeginTable("ShowFlagsTable", 2,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Checkbox("Grid", &bShowGrid))
            {
                Owner.ViewportClient->SetShowGrid(bShowGrid);
            }

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("World Axis", &bShowWorldAxis))
            {
                Owner.ViewportClient->SetShowWorldAxis(bShowWorldAxis);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Checkbox("Gizmo", &bShowGizmo))
            {
                Owner.ViewportClient->SetShowGizmo(bShowGizmo);
            }

            ImGui::EndTable();
        }
        ImGui::EndGroup();
        ImGui::Spacing();
#pragma endregion

#pragma region Culling Control
        SeparatorTextWithSpacing("Culling");
        ImGui::BeginGroup();
        FCullingControlSettings CullingSettings = Owner.ViewportClient->GetCullingControlSettings();

        if (ImGui::CollapsingHeader("CPU Culling", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Node Frustum Culling", &CullingSettings.bCpuNodeFrustumCulling);
            ImGui::Checkbox("Node Occlusion Culling", &CullingSettings.bCpuNodeOcclusionCulling);
            ImGui::Checkbox("Object Frustum Culling", &CullingSettings.bCpuObjectFrustumCulling);
            ImGui::Checkbox("Object Occlusion Culling", &CullingSettings.bCpuObjectOcclusionCulling);
            if (!CullingSettings.bCpuNodeFrustumCulling && CullingSettings.bCpuObjectFrustumCulling)
            {
                ImGui::TextDisabled("Node frustum is off: only leaf/object frustum tests are running.");
            }
            ImGui::SliderInt("Node Frustum Min Children",
                             &CullingSettings.CpuNodeFrustumMinChildren, 1, 8);
            ImGui::SliderInt("Node Occlusion Min Children",
                             &CullingSettings.CpuNodeOcclusionMinChildren, 1, 8);
            ImGui::SliderFloat("Occlusion Test Bias",
                               &CullingSettings.CpuOcclusionTestBias, 0.0f, 0.005f, "%.5f");
            ImGui::SliderFloat("Bias / Screen Radius(NDC)",
                               &CullingSettings.CpuOcclusionBiasPerScreenRadiusNdc, 0.0f, 0.01f,
                               "%.4f");
            ImGui::SliderFloat("Max Dynamic Bias",
                               &CullingSettings.CpuOcclusionMaxDynamicBias, 0.0f, 0.005f, "%.5f");
            ImGui::SliderFloat("Near Occlusion Skip Distance",
                               &CullingSettings.CpuNearOcclusionSkipDistance, 0.0f, 5000.0f,
                               "%.0f");
            ImGui::SliderFloat("Min Occluder Coverage",
                               &CullingSettings.CpuMinOccluderScreenCoverage, 0.0f, 0.25f,
                               "%.3f");
            ImGui::SliderFloat("Min Occluder Extent",
                               &CullingSettings.CpuMinOccluderExtent, 0.0f, 300.0f, "%.1f");
            ImGui::SliderFloat("Max Node Occlusion Coverage",
                               &CullingSettings.CpuMaxNodeOcclusionScreenCoverage, 0.0f, 1.0f,
                               "%.2f");
            ImGui::SliderFloat("Occlusion Bounds Extent Scale",
                               &CullingSettings.CpuOcclusionBoundsExtentScale, 0.5f, 1.0f,
                               "%.2f");
            ImGui::SliderFloat("Max Occluder Distance",
                               &CullingSettings.CpuMaxOccluderDistance, 0.0f, 50000.0f,
                               "%.0f");
            ImGui::TextDisabled("Max Occluder Distance: 0 means unlimited.");
            if (ImGui::Checkbox("Distance / LOD Culling", &CullingSettings.bCpuDistanceLodCulling))
            {
                UE_LOG(DebugOverlay, ELogLevel::Info,
                       "CPU Distance / LOD culling toggle changed (placeholder, not implemented).");
            }
            ImGui::TextDisabled("Distance / LOD culling is not implemented yet.");
        }

        if (ImGui::CollapsingHeader("GPU Culling", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("GPU Hi-Z Occlusion Culling", &CullingSettings.bGpuHiZOcclusionCulling);
        }

        Owner.ViewportClient->SetCullingControlSettings(CullingSettings);
        ImGui::EndGroup();
        ImGui::Spacing();
#pragma endregion

#pragma region Camera
        FVector3 CameraLocation = Owner.ViewportClient->GetCameraLocation();
        FRotator CameraRotation = Owner.ViewportClient->GetCameraRotation();
        float    CameraFov = Owner.ViewportClient->GetCameraFovDegrees();
        float    CameraNear = Owner.ViewportClient->GetCameraNearPlane();
        float    CameraFar = Owner.ViewportClient->GetCameraFarPlane();

        SeparatorTextWithSpacing("Camera");
        if (ImGui::DragFloat3("Position", &CameraLocation.X, 1.0f))
        {
            Owner.ViewportClient->SetCameraLocation(CameraLocation);
        }
        if (ImGui::DragFloat3("Rotation", &CameraRotation.Pitch, 0.2f))
        {
            Owner.ViewportClient->SetCameraRotation(CameraRotation);
        }
        if (ImGui::SliderFloat("FOV", &CameraFov, 10.0f, 120.0f, "%.1f"))
        {
            Owner.ViewportClient->SetPerspectiveCameraParams(CameraFov, CameraNear, CameraFar);
        }
        if (ImGui::DragFloat("Near", &CameraNear, 0.1f, 0.01f, CameraFar - 0.01f, "%.2f"))
        {
            Owner.ViewportClient->SetPerspectiveCameraParams(CameraFov, CameraNear, CameraFar);
        }
        if (ImGui::DragFloat("Far", &CameraFar, 10.0f, CameraNear + 1.0f, 500000.0f, "%.1f"))
        {
            Owner.ViewportClient->SetPerspectiveCameraParams(CameraFov, CameraNear, CameraFar);
        }
#pragma endregion

#pragma region Object Transform
        UPrimitiveComponent *SelectedPrimitive = Owner.ViewportClient->GetSelectedPrimitive();
        if (SelectedPrimitive != nullptr)
        {
            FVector3 SelectedLocation = SelectedPrimitive->GetRelativeLocation();
            FRotator SelectedRotation = SelectedPrimitive->GetRelativeRotation();
            SeparatorTextWithSpacing("Object Transform");

            if (ImGui::DragFloat3("Position", &SelectedLocation.X, 0.5f))
            {
                SelectedPrimitive->SetRelativeLocation(SelectedLocation);
            }

            if (ImGui::DragFloat3("Rotation", &SelectedRotation.Pitch, 0.2f))
            {
                SelectedPrimitive->SetRelativeRotation(SelectedRotation);
            }
        }
        else
        {
            SeparatorTextWithSpacing("Object Transform");
            ImGui::TextDisabled("No object selected.");
        }
        ImGui::Spacing();
#pragma endregion
    }

    if (Owner.ViewportClient != nullptr)
    {
        ImGui::PopItemWidth();
    }

    ImGui::End();
}

#pragma endregion

#pragma region FConsolePanel

void FConsolePanel::Render(FDebugOverlayUI &Owner)
{
    ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    if (MainViewport == nullptr)
    {
        return;
    }

    const float  Margin = 10.0f;
    const float  ConsoleReservedWidth = 460.0f;
    const float  GapFromControlPanel = 10.0f;
    static float ConsoleHeight = 420.0f;
    const float  X = MainViewport->WorkPos.x + Margin + ControlPanelWidth + GapFromControlPanel;
    const float  Y = MainViewport->WorkPos.y + MainViewport->WorkSize.y - ConsoleHeight - Margin;
    const float  Width = std::max(480.0f, MainViewport->WorkSize.x - (Margin * 2.0f) -
                                              ConsoleReservedWidth - GapFromControlPanel);
    const float  MaxHeight = std::max(220.0f, MainViewport->WorkSize.y - (Margin * 2.0f));
    ConsoleHeight = std::clamp(ConsoleHeight, 220.0f, MaxHeight);

    ImGui::SetNextWindowPos(ImVec2(X, Y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(Width, ConsoleHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(Width, 220.0f), ImVec2(Width, MaxHeight));
    ImGui::SetNextWindowBgAlpha(0.95f);

    const ImGuiWindowFlags Flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::Begin("Console Panel (F2)", nullptr, Flags))
    {
        ImGui::End();
        return;
    }

    ConsoleHeight = ImGui::GetWindowSize().y;

    const char *LevelLabels[] = {"Verbose", "Debug", "Info", "Warning", "Error"};

#pragma region Header Toolbar
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const float LeftPaneWidth = 260.0f;
    const float SplitterWidth = 12.0f;
    const float RightPaneWidth = std::max(200.0f, AvailableWidth - LeftPaneWidth - SplitterWidth);

    ImGui::BeginChild("ConsoleToolbarLeft", ImVec2(LeftPaneWidth, 72.0f), false,
                      ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button("Clear", ImVec2(80.0f, 0.0f)))
    {
        Owner.LogEntries.clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &Owner.bAutoScroll);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Min Level");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::BeginCombo("##MinLevel", LevelLabels[Owner.SelectedMinimumLevel]))
    {
        for (int32 Index = 0; Index < 5; ++Index)
        {
            const bool bSelected = (Owner.SelectedMinimumLevel == Index);
            if (ImGui::Selectable(LevelLabels[Index], bSelected))
            {
                Owner.SelectedMinimumLevel = Index;
                SetGlobalLogLevel(static_cast<ELogLevel>(Owner.SelectedMinimumLevel));
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("ConsoleToolbarSeparator", ImVec2(SplitterWidth, 72.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    {
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 P0 = ImGui::GetCursorScreenPos();
        const ImVec2 P1 = ImVec2(P0.x + SplitterWidth * 0.5f, P0.y + 72.0f);
        DrawList->AddLine(ImVec2(P1.x, P0.y + 4.0f), ImVec2(P1.x, P1.y - 4.0f),
                          IM_COL32(110, 110, 110, 180), 1.0f);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("ConsoleToolbarRight", ImVec2(RightPaneWidth, 72.0f), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Keyword");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(180.0f, RightPaneWidth - 90.0f));
    ImGui::InputTextWithHint("##LogSearch", "Search keyword...", Owner.SearchBuffer,
                             sizeof(Owner.SearchBuffer));

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Filters");
    ImGui::SameLine();
    for (int32 Index = 0; Index < 5; ++Index)
    {
        ImGui::Checkbox(LevelLabels[Index], &Owner.LevelFilter[Index]);
        if (Index < 4)
        {
            ImGui::SameLine();
        }
    }

    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
#pragma endregion

#pragma region Log Region
    const float FooterHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, -FooterHeight), true,
                      ImGuiWindowFlags_HorizontalScrollbar |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (const FLogEntry &Entry : Owner.LogEntries)
    {
        int32 Index = static_cast<int32>(Entry.Level);
        if (Index < 0)
        {
            Index = 0;
        }
        if (Index > 4)
        {
            Index = 4;
        }
        if (!Owner.LevelFilter[Index])
        {
            continue;
        }
        if (Index < Owner.SelectedMinimumLevel)
        {
            continue;
        }
        if (!ContainsCaseInsensitive(Entry.Message, Owner.SearchBuffer))
        {
            continue;
        }

        ImGui::TextColored(GetLogTextColor(Entry.Level), "%s", Entry.Message.c_str());
    }

    if (Owner.bScrollToBottom && Owner.bAutoScroll)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    Owner.bScrollToBottom = false;

    ImGui::EndChild();
#pragma endregion

#pragma region Command Input
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint(
            "##CommandInput",
            "Command... (stat fps | stat memory | stat timing | stat culling | stat all | stat none)",
            Owner.CommandBuffer, sizeof(Owner.CommandBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        const FString Command = Owner.CommandBuffer;
        Owner.ExecuteConsoleCommand(Command);
        Owner.CommandBuffer[0] = '\0';
    }
#pragma endregion

    ImGui::End();
}

#pragma endregion

#pragma region FDebugOverlayUI Core

void FDebugOverlayUI::Render()
{
    HandleHotkeys();

    StatsPanel.Render();

    if (bShowControlPanel)
    {
        ControlPanel.Render(*this);
    }

    if (bShowConsolePanel)
    {
        ConsolePanel.Render(*this);
    }

    RenderWorldGuides();
}

void FDebugOverlayUI::HandleHotkeys()
{
    if (InputState == nullptr)
    {
        return;
    }

    if (InputState->WasKeyPressed(VK_F1))
    {
        bShowControlPanel = !bShowControlPanel;
    }

    if (InputState->WasKeyPressed(VK_F2))
    {
        bShowConsolePanel = !bShowConsolePanel;
    }

    if (InputState->WasKeyPressed(VK_F3))
    {
        StatsPanel.ToggleRightPanelVisible();
    }
}

void FDebugOverlayUI::Log(ELogLevel Verbosity, const char *Message)
{
    if (LogEntries.size() >= 100)
    {
        LogEntries.pop_front();
    }
    LogEntries.push_back({Verbosity, FString(Message)});
    bScrollToBottom = true;
}

bool FDebugOverlayUI::OpenSceneFileDialog(bool bSaveDialog, std::filesystem::path &OutPath) const
{
    wchar_t       FileBuffer[MAX_PATH] = {};
    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = ::GetActiveWindow();
    Ofn.lpstrFile = FileBuffer;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files\0*.*\0\0";
    Ofn.nFilterIndex = 1;
    Ofn.Flags = OFN_PATHMUSTEXIST | (bSaveDialog ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    Ofn.lpstrDefExt = L"Scene";

    const BOOL bPicked = bSaveDialog ? ::GetSaveFileNameW(&Ofn) : ::GetOpenFileNameW(&Ofn);
    if (bPicked == FALSE)
    {
        UE_LOG(DebugOverlay, ELogLevel::Debug,
               bSaveDialog ? "Save scene dialog cancelled." : "Load scene dialog cancelled.");
        return false;
    }

    OutPath = std::filesystem::path(FileBuffer);
    return true;
}

#pragma endregion

#pragma region World Guide Helpers

namespace
{
    struct FNdcPoint
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    bool ProjectLineToNdc(const FSceneView &SceneView, const FVector3 &WorldA,
                          const FVector3 &WorldB, FNdcPoint &OutA, FNdcPoint &OutB)
    {
        constexpr float NearViewZ = 0.01f;
        FVector4        ViewA = FVector4(WorldA, 1.0f) * SceneView.GetViewMatrix();
        FVector4        ViewB = FVector4(WorldB, 1.0f) * SceneView.GetViewMatrix();

        if (ViewA.Z < NearViewZ && ViewB.Z < NearViewZ)
        {
            return false;
        }

        if (ViewA.Z < NearViewZ || ViewB.Z < NearViewZ)
        {
            FVector4       &Behind = (ViewA.Z < NearViewZ) ? ViewA : ViewB;
            const FVector4 &Front = (ViewA.Z < NearViewZ) ? ViewB : ViewA;
            const float     Denom = (Front.Z - Behind.Z);
            if (std::abs(Denom) <= 1e-6f)
            {
                return false;
            }

            const float T = (NearViewZ - Behind.Z) / Denom;
            Behind = Behind + (Front - Behind) * T;
            Behind.Z = NearViewZ;
        }

        const FVector4 ClipA = ViewA * SceneView.GetProjectionMatrix();
        const FVector4 ClipB = ViewB * SceneView.GetProjectionMatrix();
        if (std::abs(ClipA.W) <= 1e-6f || std::abs(ClipB.W) <= 1e-6f)
        {
            return false;
        }

        OutA.X = ClipA.X / ClipA.W;
        OutA.Y = ClipA.Y / ClipA.W;
        OutA.Z = ClipA.Z / ClipA.W;
        OutB.X = ClipB.X / ClipB.W;
        OutB.Y = ClipB.Y / ClipB.W;
        OutB.Z = ClipB.Z / ClipB.W;
        return true;
    }

    ImVec2 NdcToScreen(const FNdcPoint &Point, const ImVec2 &ScreenPos, const ImVec2 &ScreenSize)
    {
        return ImVec2(ScreenPos.x + ((Point.X * 0.5f) + 0.5f) * ScreenSize.x,
                      ScreenPos.y + ((-Point.Y * 0.5f) + 0.5f) * ScreenSize.y);
    }
} // namespace

#pragma endregion

#pragma region FDebugOverlayUI World Guides

void FDebugOverlayUI::RenderWorldGuides() {}

#pragma endregion

#pragma region FDebugOverlayUI Scene Preload

void FDebugOverlayUI::PreloadSceneAssetsForLogging(FScene *InScene)
{
    if (InScene == nullptr)
    {
        return;
    }

    if (GRenderer == nullptr)
    {
        UE_LOG(DebugOverlay, ELogLevel::Warning,
               "Skipping scene preload after load: renderer is not available.");
        return;
    }

    static Asset::FAssetCacheManager PreviewAssetCacheManager;
    static FAssetObjectManager       PreviewAssetObjectManager;
    PreviewAssetObjectManager.SetAssetCacheManager(&PreviewAssetCacheManager);
    PreviewAssetObjectManager.SetDevice(&GRenderer->GetDevice());

    FSceneAssetBinder::BindScene(InScene, &PreviewAssetObjectManager);

    size_t AttemptedCount = 0;
    size_t SuccessCount = 0;

    for (UStaticMeshComponent *Component : InScene->GetStaticMeshComponents())
    {
        if (Component == nullptr)
        {
            continue;
        }

        const FString &MeshPath = Component->GetStaticMeshPath();
        if (MeshPath.empty())
        {
            UE_LOG(DebugOverlay, ELogLevel::Warning, "Loaded scene component has empty mesh path.");
            continue;
        }

        ++AttemptedCount;
        UStaticMesh *LoadedMesh = Component->GetStaticMesh();
        if (LoadedMesh != nullptr)
        {
            ++SuccessCount;
        }
    }

    UE_LOG(DebugOverlay, ELogLevel::Info, "Loaded scene asset preload result: staticMeshes=%zu/%zu",
           SuccessCount, AttemptedCount);
}

#pragma endregion

#pragma region FDebugOverlayUI Console Commands

void FDebugOverlayUI::ExecuteConsoleCommand(const FString &CommandText)
{
    FString Trimmed = CommandText;
    Trimmed.erase(Trimmed.begin(), std::find_if(Trimmed.begin(), Trimmed.end(), [](unsigned char Ch)
                                                { return !std::isspace(Ch); }));
    Trimmed.erase(std::find_if(Trimmed.rbegin(), Trimmed.rend(),
                               [](unsigned char Ch) { return !std::isspace(Ch); })
                      .base(),
                  Trimmed.end());

    FString Lower = Trimmed;
    std::transform(Lower.begin(), Lower.end(), Lower.begin(),
                   [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
    if (Lower.empty())
    {
        return;
    }

    UE_LOG(DebugOverlay, ELogLevel::Info, "Console command: %s", Trimmed.c_str());
    if (Lower == "stat fps")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::All);
        return;
    }
    if (Lower == "stat memory")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::All);
        return;
    }
    if (Lower == "stat all")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::All);
        return;
    }
    if (Lower == "stat timing")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::All);
        return;
    }
    if (Lower == "stat culling")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::All);
        return;
    }
    if (Lower == "stat timing reset")
    {
        FTimingStats::Reset();
        return;
    }
    if (Lower == "stat none")
    {
        StatsPanel.SetStatMode(FStatsPanel::EStatMode::None);
        return;
    }

    UE_LOG(DebugOverlay, ELogLevel::Warning, "Unknown command: %s", Trimmed.c_str());
}

#pragma endregion
