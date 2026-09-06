#include "Viewport/ViewportClient.h"
#include "Core/Math/MathUtility.h"
#include "Core/Math/Matrix.h"
#include "Engine/EngineStatics.h"
#include "Renderer/D3D11/D3D11RendererModule.h"
#include "Scene/Scene.h"
#include "Scene/SceneDrawSort.h"
#include "Scene/SAH-BVH8/SceneRenderer.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Core/Platform/PlatformTime.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/MathUtility.h"
#include "Core/Platform/PlatformTime.h"
#include "Core/Stats/TimingStats.h"
#include "Core/Logging/LogMacros.h"
#include "Engine/EngineStatics.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Viewport/Gizmo.h"
#include "Picker.h"
#include <algorithm>
#include <windows.h>

namespace
{
    constexpr int32 ClickThresholdPixels = 4;
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float DegreesToRadians = Pi / 180.0f;

    void ExtractFrustumPlanes(const FMatrix &VP, FPlane OutPlanes[6])
    {
        // 엔진의 FMatrix 행/열 접근자에 맞춰 .M[][] 또는 .m[][] 으로 수정해서 사용하세요.
        OutPlanes[0] = {VP.M[0][3] + VP.M[0][0], VP.M[1][3] + VP.M[1][0], VP.M[2][3] + VP.M[2][0],
                        VP.M[3][3] + VP.M[3][0]}; // Left
        OutPlanes[1] = {VP.M[0][3] - VP.M[0][0], VP.M[1][3] - VP.M[1][0], VP.M[2][3] - VP.M[2][0],
                        VP.M[3][3] - VP.M[3][0]}; // Right
        OutPlanes[2] = {VP.M[0][3] + VP.M[0][1], VP.M[1][3] + VP.M[1][1], VP.M[2][3] + VP.M[2][1],
                        VP.M[3][3] + VP.M[3][1]}; // Bottom
        OutPlanes[3] = {VP.M[0][3] - VP.M[0][1], VP.M[1][3] - VP.M[1][1], VP.M[2][3] - VP.M[2][1],
                        VP.M[3][3] - VP.M[3][1]};                        // Top
        OutPlanes[4] = {VP.M[0][2], VP.M[1][2], VP.M[2][2], VP.M[3][2]}; // Near
        OutPlanes[5] = {VP.M[0][3] - VP.M[0][2], VP.M[1][3] - VP.M[1][2], VP.M[2][3] - VP.M[2][2],
                        VP.M[3][3] - VP.M[3][2]}; // Far

        // 정규화 (Normalization)
        for (int i = 0; i < 6; ++i)
        {
            float Length =
                std::sqrt(OutPlanes[i].nx * OutPlanes[i].nx + OutPlanes[i].ny * OutPlanes[i].ny +
                          OutPlanes[i].nz * OutPlanes[i].nz);
            OutPlanes[i].nx /= Length;
            OutPlanes[i].ny /= Length;
            OutPlanes[i].nz /= Length;
            OutPlanes[i].d /= Length;
        }
    }


} // namespace

FViewportClient::FViewportClient()
{
    Gizmo = new UGizmo();
    Gizmo->Activate();
    Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
    bGizmoPickMeshUploaded = false;

    Picker = new FPicker();
}

FViewportClient::~FViewportClient()
{
    if (Gizmo)
    {
        delete Gizmo;
        Gizmo = nullptr;
    }
}

void FViewportClient::Tick(float DeltaTime)
{
    PerformanceStats.LastFrameTimeMs = static_cast<double>(DeltaTime) * 1000.0;
    const bool bWindowFocused =
        (OwnerWindow != nullptr) && (::GetForegroundWindow() == OwnerWindow);

    if (bWindowFocused)
    {
        const bool bMouseCapturedByImGui =
            ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;

        // 1. �̺�Ʈ �ý����� ��ȸ�Ͽ� ���콺 ���¿� ��ġ�� ���� �����ɴϴ� (Polling)
        bRightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        bMiddleMouseDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        const bool bCurrentLeftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        POINT CursorPos;
        GetCursorPos(&CursorPos);
        ScreenToClient(OwnerWindow, &CursorPos);

        // ���콺 ��ȭ��(Delta) ���
        const int32 DeltaX = CursorPos.x - LastMouseX;
        const int32 DeltaY = CursorPos.y - LastMouseY;
        LastMouseX = CursorPos.x;
        LastMouseY = CursorPos.y;

        const bool bSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (bSpaceDown && !bSpacePrevDown)
            Gizmo->SetNextMode();
        bSpacePrevDown = bSpaceDown;

        if (bRightMouseDown)
        {
            // ȸ�� ����
            CurrentCameraInput.YawDelta += static_cast<float>(DeltaX);
            CurrentCameraInput.PitchDelta += static_cast<float>(DeltaY);

            // WASD �̵� ����
            bool        bCameraMoved = false;
            const float SpeedMultiplier =
                ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) ? 4.0f : 1.0f;
            if ((GetAsyncKeyState('W') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveForward += SpeedMultiplier;
                bCameraMoved = true;
            }
            if ((GetAsyncKeyState('S') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveForward -= SpeedMultiplier;
                bCameraMoved = true;
            }
            if ((GetAsyncKeyState('D') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveRight += SpeedMultiplier;
                bCameraMoved = true;
            }
            if ((GetAsyncKeyState('A') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveRight -= SpeedMultiplier;
                bCameraMoved = true;
            }
            if ((GetAsyncKeyState('E') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveUpWorld += SpeedMultiplier;
                bCameraMoved = true;
            }
            if ((GetAsyncKeyState('Q') & 0x8000) != 0)
            {
                CurrentCameraInput.MoveUpWorld -= SpeedMultiplier;
                bCameraMoved = true;
            }

            if (bCameraMoved)
            {
                Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
            }
        }
        // 3. �� Ŭ�� ó�� (�д�)
        else if (bMiddleMouseDown)
        {
            CurrentCameraInput.PanX += static_cast<float>(DeltaX);
            CurrentCameraInput.PanY += static_cast<float>(DeltaY);
        }

        // 4. �� ��ũ�� ó�� (��)
        if (PendingWheelDelta != 0.0f)
        {
            CurrentCameraInput.Zoom = PendingWheelDelta;
            PendingWheelDelta = 0.0f;
        }

        // 5. ��Ŭ�� ó�� (��ŷ�� ���� Ŭ��/�巡�� ����)
        if (bCurrentLeftMouseDown)
        {
            if (!bLeftMouseDown)
            {
                // LMB press — start gizmo drag if an axis is hovered
                MouseDownX = CursorPos.x;
                MouseDownY = CursorPos.y;

                if (!bMouseCapturedByImGui && Gizmo && Gizmo->IsHovered())
                {
                    Gizmo->SetHolding(true);
                    Gizmo->DragStart();
                }
            }

            if (bLeftMouseDown && Gizmo && Gizmo->IsHolding())
            {
                const Geometry::FRay DragRay = Geometry::FRay::BuildRay(
                    CursorPos.x, CursorPos.y, SceneView.GetViewProjectionMatrix(),
                    SceneView.GetCameraWidth(), SceneView.GetCameraHeight());
                Gizmo->UpdateDrag(DragRay);
            }
        }
        else if (!bCurrentLeftMouseDown && bLeftMouseDown)
        {
            if (Gizmo && Gizmo->IsHolding())
            {
                Gizmo->DragEnd();
                Gizmo->SetHolding(false);

                if (Scene)
                {
                    Scene->MarkSceneDirty();
                }
            }
            else if (!bMouseCapturedByImGui &&
                     GetReleaseType(CursorPos.x, CursorPos.y) == EPointerReleaseType::Click)
            {
                ProcessClick(CursorPos.x, CursorPos.y);
            }
        }
        bLeftMouseDown = bCurrentLeftMouseDown;

        // Gizmo hover — only when the mouse moved and no drag/camera interaction is active
        if (!bMouseCapturedByImGui && IsShowGizmo() && Gizmo && Gizmo->IsHoveringEnabled() &&
            !bRightMouseDown && !bMiddleMouseDown && !Gizmo->IsHolding() &&
            (CursorPos.x != LastHoverMouseX || CursorPos.y != LastHoverMouseY))
        {
            LastHoverMouseX = CursorPos.x;
            LastHoverMouseY = CursorPos.y;
            const Geometry::FRay Ray = Geometry::FRay::BuildRay(
                CursorPos.x, CursorPos.y, SceneView.GetViewProjectionMatrix(),
                SceneView.GetCameraWidth(), SceneView.GetCameraHeight());
            Gizmo->UpdateHoveredAxis(Gizmo->PickAxis(Ray));
        }
    }

    // 6. ������ �Է��� ��Ʈ�ѷ��� �����Ͽ� ī�޶� ����
    // ����
    CameraController.Tick(DeltaTime, CurrentCameraInput);
    CurrentCameraInput.ResetFrameInput();

    if (Scene != nullptr)
        Scene->Tick(DeltaTime);
}

//void FViewportClient::Draw(float Width, float Height)
//{
//    if (Renderer == nullptr)
//    {
//        return;
//    }
//
//    SceneView = BuildSceneView(Width, Height);
//    if (Gizmo)
//    {
//        SceneView.SetGizmo(Gizmo);
//        Renderer->GetGizmoRenderer().UploadPickMesh(Gizmo);
//    }
//
//    // Raycast for gizmo hover
//    if (Gizmo && (LastMouseX != LastHoverMouseX || LastMouseY != LastHoverMouseY))
//    {
//        LastHoverMouseX = LastMouseX;
//        LastHoverMouseY = LastMouseY;
//        const Geometry::FRay Ray = Geometry::FRay::BuildRay(
//            LastMouseX, LastMouseY,
//            SceneView.GetViewProjectionMatrix(),
//            Width, Height);
//        Gizmo->UpdateHoveredAxis(Gizmo->PickAxis(Ray));
//    }
//
//    Renderer->SetScene(Scene);
//    Renderer->SetShowGrid(IsShowGrid());
//    Renderer->SetShowWorldAxis(IsShowWorldAxis());
//    Renderer->SetGridSpacing(GetGridSpacing());
//    Renderer->SetGridRenderMode(GridRenderMode);
//    Renderer->RenderFrame(&SceneView);
//}

void FViewportClient::Draw(float Width, float Height)
{
    SCOPED_TIMING_STAT("Viewport.Draw");

    if (Renderer == nullptr)
    {
        return;
    }

    SceneView = BuildSceneView(Width, Height);
    SceneView.SetShowGizmo(IsShowGizmo());
    if (Gizmo)
    {
        SceneView.SetGizmo(Gizmo);
        Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
        if (IsShowGizmo())
        {
            if (!bGizmoPickMeshUploaded)
            {
                Renderer->GetGizmoRenderer().UploadPickMesh(Gizmo);
                bGizmoPickMeshUploaded = true;
                UE_LOG(Viewport, ELogLevel::Debug,
                       "Gizmo pick mesh uploaded once and cached for runtime.");
            }
        }
    }

    if (Scene != nullptr)
    {
        // 1. 기존 그리기 대기열 초기화
        Scene->GetDrawItems().clear();
        PerformanceStats.LastFrustumCullingTotalObjects =
            static_cast<uint64>(Scene->GetStaticMeshComponents().size());
        PerformanceStats.LastFrustumCullingVisibleObjects = 0;
        PerformanceStats.LastFrustumCullingCulledObjects = PerformanceStats.LastFrustumCullingTotalObjects;
        auto &DrawItems = Scene->GetDrawItems();
        DrawItems.clear();

        // BVH8 노드가 준비되어 있으면 컬링 경로 사용
        if (Scene->GetRootNodeIndex() >= 0 && !Scene->GetBVH8Nodes().empty())
        {
            FPlane FrustumPlanes[6];
            {
                SCOPED_TIMING_STAT("Viewport.Culling.ExtractFrustumPlanes");
                ExtractFrustumPlanes(SceneView.GetViewProjectionMatrix(), FrustumPlanes);
            }

            const bool bAnyCpuCullingEnabled =
                CullingControlSettings.bCpuNodeFrustumCulling ||
                CullingControlSettings.bCpuObjectFrustumCulling ||
                CullingControlSettings.bCpuNodeOcclusionCulling ||
                CullingControlSettings.bCpuObjectOcclusionCulling ||
                CullingControlSettings.bCpuDistanceLodCulling;

            if (bAnyCpuCullingEnabled)
            {
                std::vector<FVisibleObject> VisibleQueue;
                FCpuOcclusionCullStats      CpuOcclusionStats{};
                FCpuCullingOptions          CpuCullingOptions{};
                CpuCullingOptions.bEnableNodeFrustumCulling =
                    CullingControlSettings.bCpuNodeFrustumCulling;
                CpuCullingOptions.bEnableObjectFrustumCulling =
                    CullingControlSettings.bCpuObjectFrustumCulling;
                CpuCullingOptions.bEnableNodeOcclusionCulling =
                    CullingControlSettings.bCpuNodeOcclusionCulling;
                CpuCullingOptions.bEnableObjectOcclusionCulling =
                    CullingControlSettings.bCpuObjectOcclusionCulling;
                CpuCullingOptions.bEnableDistanceLodCulling =
                    CullingControlSettings.bCpuDistanceLodCulling;
                CpuCullingOptions.MinChildrenForNodeFrustumTest = static_cast<uint32>(
                    (std::max)(1, CullingControlSettings.CpuNodeFrustumMinChildren));
                CpuCullingOptions.MinChildrenForNodeOcclusionTest = static_cast<uint32>(
                    (std::max)(1, CullingControlSettings.CpuNodeOcclusionMinChildren));
                CpuCullingOptions.OcclusionTestBias =
                    (std::max)(0.0f, CullingControlSettings.CpuOcclusionTestBias);
                CpuCullingOptions.OcclusionScreenRadiusBiasScale =
                    (std::max)(0.0f, CullingControlSettings.CpuOcclusionBiasPerScreenRadiusNdc);
                CpuCullingOptions.MaxDynamicOcclusionBias =
                    (std::max)(0.0f, CullingControlSettings.CpuOcclusionMaxDynamicBias);
                CpuCullingOptions.NearOcclusionSkipDistance =
                    (std::max)(0.0f, CullingControlSettings.CpuNearOcclusionSkipDistance);
                CpuCullingOptions.MinOccluderScreenCoverage =
                    (std::max)(0.0f, CullingControlSettings.CpuMinOccluderScreenCoverage);
                CpuCullingOptions.MinOccluderExtent =
                    (std::max)(0.0f, CullingControlSettings.CpuMinOccluderExtent);
                CpuCullingOptions.MaxNodeOcclusionScreenCoverage =
                    (std::max)(0.0f, CullingControlSettings.CpuMaxNodeOcclusionScreenCoverage);
                CpuCullingOptions.OcclusionBoundsExtentScale =
                    (std::clamp)(CullingControlSettings.CpuOcclusionBoundsExtentScale, 0.5f, 1.0f);
                CpuCullingOptions.MaxOccluderDistance =
                    (std::max)(0.0f, CullingControlSettings.CpuMaxOccluderDistance);

                // AVX2 BVH8 컬링 수행
                {
                    SCOPED_TIMING_STAT("Viewport.Culling.CullAndSortWithBVH8");
                    CullAndSortWithBVH8(&Scene->GetSceneDataSoA(), Scene->GetBVH8Nodes().data(),
                                        Scene->GetRootNodeIndex(), SceneView.GetEyePosition(),
                                        FrustumPlanes, SceneView.GetViewProjectionMatrix(), VisibleQueue,
                                        &CpuOcclusionStats, &CpuCullingOptions);
                }

                // 통과한(Visible) 오브젝트를 VisibleSet으로 구성한 뒤,
                // SceneSort::MakeOpaqueSortKey(material/mesh/front-to-back)를 사용해 DrawItems를 생성한다.
                {
                    SCOPED_TIMING_STAT("Viewport.Culling.BuildDrawItems");
                    TArray<FSceneVisibleItem> &VisibleSet = Scene->GetVisibleSet();
                    VisibleSet.clear();
                    VisibleSet.reserve(VisibleQueue.size());

                    for (const FVisibleObject &VisObj : VisibleQueue)
                    {
                        if (VisObj.ObjectID < 0)
                        {
                            continue;
                        }
                        FSceneVisibleItem Item{};
                        Item.ComponentIndex = static_cast<uint32>(VisObj.ObjectID);
                        VisibleSet.push_back(Item);
                    }

                    Scene->BuildDrawItemsFromVisibleSet();
                }

                PerformanceStats.LastFrustumCullingVisibleObjects = static_cast<uint64>(VisibleQueue.size());
                const uint64 TotalObjects = PerformanceStats.LastFrustumCullingTotalObjects;
                const uint64 VisibleObjects = PerformanceStats.LastFrustumCullingVisibleObjects;
                PerformanceStats.LastFrustumCullingCulledObjects =
                    (TotalObjects > VisibleObjects) ? (TotalObjects - VisibleObjects) : 0;

                PerformanceStats.LastCpuOcclusionCullingTotalObjects =
                    static_cast<uint64>(CpuOcclusionStats.InputCandidates);
                PerformanceStats.LastCpuOcclusionCullingVisibleObjects =
                    static_cast<uint64>(CpuOcclusionStats.VisibleObjects);
                PerformanceStats.LastCpuOcclusionCullingCulledObjects =
                    static_cast<uint64>(CpuOcclusionStats.CulledObjects);
                PerformanceStats.LastCpuNodeFrustumTested =
                    static_cast<uint64>(CpuOcclusionStats.NodeFrustumTested);
                PerformanceStats.LastCpuNodeFrustumCulled =
                    static_cast<uint64>(CpuOcclusionStats.NodeFrustumCulled);
                PerformanceStats.LastCpuNodeFrustumTestedObjects =
                    static_cast<uint64>(CpuOcclusionStats.NodeFrustumTestedObjects);
                PerformanceStats.LastCpuNodeFrustumCulledObjects =
                    static_cast<uint64>(CpuOcclusionStats.NodeFrustumCulledObjects);
                PerformanceStats.LastCpuObjectFrustumInput =
                    static_cast<uint64>(CpuOcclusionStats.ObjectFrustumInput);
                PerformanceStats.LastCpuObjectFrustumCulled =
                    static_cast<uint64>(CpuOcclusionStats.ObjectFrustumCulled);
                PerformanceStats.LastCpuNodeOcclusionTested =
                    static_cast<uint64>(CpuOcclusionStats.NodeOcclusionTested);
                PerformanceStats.LastCpuNodeOcclusionCulled =
                    static_cast<uint64>(CpuOcclusionStats.NodeOcclusionCulled);
                PerformanceStats.LastCpuNodeOcclusionTestedObjects =
                    static_cast<uint64>(CpuOcclusionStats.NodeOcclusionTestedObjects);
                PerformanceStats.LastCpuNodeOcclusionCulledObjects =
                    static_cast<uint64>(CpuOcclusionStats.NodeOcclusionCulledObjects);
                PerformanceStats.LastCpuDistanceLodInput =
                    static_cast<uint64>(CpuOcclusionStats.DistanceLodInput);
                PerformanceStats.LastCpuDistanceLodCulled =
                    static_cast<uint64>(CpuOcclusionStats.DistanceLodCulled);
                PerformanceStats.bLastCpuOcclusionPassExecuted =
                    CullingControlSettings.bCpuObjectOcclusionCulling;
                PerformanceStats.bLastCpuDistanceLodPassImplemented = false;
            }
            else
            {
                SCOPED_TIMING_STAT("Viewport.Culling.Skipped");

                const auto &StaticMeshComponents = Scene->GetStaticMeshComponents();
                {
                    SCOPED_TIMING_STAT("Viewport.Culling.BuildDrawItems");
                    TArray<FSceneVisibleItem> &VisibleSet = Scene->GetVisibleSet();
                    VisibleSet.clear();
                    VisibleSet.reserve(StaticMeshComponents.size());

                    for (uint32 i = 0; i < static_cast<uint32>(StaticMeshComponents.size()); ++i)
                    {
                        if (StaticMeshComponents[i] == nullptr) continue;
                        FSceneVisibleItem Item{};
                        Item.ComponentIndex = i;
                        VisibleSet.push_back(Item);
                    }

                    Scene->BuildDrawItemsFromVisibleSet();
                }

                PerformanceStats.LastFrustumCullingVisibleObjects = static_cast<uint64>(DrawItems.size());
                const uint64 TotalObjects = PerformanceStats.LastFrustumCullingTotalObjects;
                const uint64 VisibleObjects = PerformanceStats.LastFrustumCullingVisibleObjects;
                PerformanceStats.LastFrustumCullingCulledObjects =
                    (TotalObjects > VisibleObjects) ? (TotalObjects - VisibleObjects) : 0;
                PerformanceStats.LastCpuOcclusionCullingTotalObjects = 0;
                PerformanceStats.LastCpuOcclusionCullingVisibleObjects = 0;
                PerformanceStats.LastCpuOcclusionCullingCulledObjects = 0;
                PerformanceStats.LastCpuNodeFrustumTested = 0;
                PerformanceStats.LastCpuNodeFrustumCulled = 0;
                PerformanceStats.LastCpuNodeFrustumTestedObjects = 0;
                PerformanceStats.LastCpuNodeFrustumCulledObjects = 0;
                PerformanceStats.LastCpuObjectFrustumInput = 0;
                PerformanceStats.LastCpuObjectFrustumCulled = 0;
                PerformanceStats.LastCpuNodeOcclusionTested = 0;
                PerformanceStats.LastCpuNodeOcclusionCulled = 0;
                PerformanceStats.LastCpuNodeOcclusionTestedObjects = 0;
                PerformanceStats.LastCpuNodeOcclusionCulledObjects = 0;
                PerformanceStats.LastCpuDistanceLodInput = 0;
                PerformanceStats.LastCpuDistanceLodCulled = 0;
                PerformanceStats.bLastCpuOcclusionPassExecuted = false;
                PerformanceStats.bLastCpuDistanceLodPassImplemented = false;
            }
        }
        else
        {
            // BVH가 아직 준비되지 않았거나 단일 오브젝트 씬 등으로 비어 있는 경우:
            // 폴백으로 모든 컴포넌트를 그려서 장면이 비어 보이지 않도록 한다.
            SCOPED_TIMING_STAT("Viewport.Culling.Skipped");

            const auto &StaticMeshComponents = Scene->GetStaticMeshComponents();
            {
                SCOPED_TIMING_STAT("Viewport.Culling.BuildDrawItems");
                TArray<FSceneVisibleItem> &VisibleSet = Scene->GetVisibleSet();
                VisibleSet.clear();
                VisibleSet.reserve(StaticMeshComponents.size());

                for (uint32 i = 0; i < static_cast<uint32>(StaticMeshComponents.size()); ++i)
                {
                    if (StaticMeshComponents[i] == nullptr) continue;
                    FSceneVisibleItem Item{};
                    Item.ComponentIndex = i;
                    VisibleSet.push_back(Item);
                }

                Scene->BuildDrawItemsFromVisibleSet();
            }

            PerformanceStats.LastFrustumCullingVisibleObjects = static_cast<uint64>(DrawItems.size());
            const uint64 TotalObjects = PerformanceStats.LastFrustumCullingTotalObjects;
            const uint64 VisibleObjects = PerformanceStats.LastFrustumCullingVisibleObjects;
            PerformanceStats.LastFrustumCullingCulledObjects =
                (TotalObjects > VisibleObjects) ? (TotalObjects - VisibleObjects) : 0;
            PerformanceStats.LastCpuOcclusionCullingTotalObjects = 0;
            PerformanceStats.LastCpuOcclusionCullingVisibleObjects = 0;
            PerformanceStats.LastCpuOcclusionCullingCulledObjects = 0;
            PerformanceStats.LastCpuNodeFrustumTested = 0;
            PerformanceStats.LastCpuNodeFrustumCulled = 0;
            PerformanceStats.LastCpuNodeFrustumTestedObjects = 0;
            PerformanceStats.LastCpuNodeFrustumCulledObjects = 0;
            PerformanceStats.LastCpuObjectFrustumInput = 0;
            PerformanceStats.LastCpuObjectFrustumCulled = 0;
            PerformanceStats.LastCpuNodeOcclusionTested = 0;
            PerformanceStats.LastCpuNodeOcclusionCulled = 0;
            PerformanceStats.LastCpuNodeOcclusionTestedObjects = 0;
            PerformanceStats.LastCpuNodeOcclusionCulledObjects = 0;
            PerformanceStats.LastCpuDistanceLodInput = 0;
            PerformanceStats.LastCpuDistanceLodCulled = 0;
            PerformanceStats.LastCpuOcclusionBufferWidth = 0;
            PerformanceStats.LastCpuOcclusionBufferHeight = 0;
            PerformanceStats.bLastCpuOcclusionPassExecuted = false;
            PerformanceStats.bLastCpuDistanceLodPassImplemented = false;
        }
    }

    {
        SCOPED_TIMING_STAT("Viewport.RenderFrame");
        Renderer->SetScene(Scene);
        Renderer->SetShowGrid(IsShowGrid());
        Renderer->SetShowWorldAxis(IsShowWorldAxis());
        Renderer->SetShowSelectedAABB(IsShowSelectedAABB());
        Renderer->SetGridSpacing(GetGridSpacing());
        GridRenderMode = EGridRenderMode::CpuLineBatch;
        Renderer->SetGridRenderMode(EGridRenderMode::CpuLineBatch);
        FGpuCullingSettings GpuCullingSettings{};
        GpuCullingSettings.bEnableGpuFrustumCulling = CullingControlSettings.bGpuFrustumCulling;
        GpuCullingSettings.bEnableGpuHiZOcclusionCulling =
            CullingControlSettings.bGpuHiZOcclusionCulling;
        GpuCullingSettings.bEnableGpuInstanceCulling = CullingControlSettings.bGpuInstanceCulling;
        GpuCullingSettings.bEnableGpuLodSelection = CullingControlSettings.bGpuLodSelection;
        Renderer->SetGpuCullingSettings(GpuCullingSettings);
        Renderer->RenderFrame(&SceneView);

        const FDepthCullStats DepthCullStats = Renderer->GetLastDepthCullStats();
        PerformanceStats.LastGpuOcclusionCullingTotalObjects = DepthCullStats.TotalObjects;
        PerformanceStats.LastGpuOcclusionCullingVisibleObjects = DepthCullStats.VisibleObjects;
        PerformanceStats.LastGpuOcclusionCullingCulledObjects = DepthCullStats.OccludedObjects;
        const size_t DrawItemCount = (Scene != nullptr) ? Scene->GetDrawItems().size() : 0;
        if (DepthCullStats.TotalObjects == 0 && DrawItemCount > 0)
        {
            PerformanceStats.LastGpuOcclusionCullingTotalObjects = static_cast<uint64>(DrawItemCount);
            PerformanceStats.LastGpuOcclusionCullingVisibleObjects =
                PerformanceStats.LastGpuOcclusionCullingTotalObjects;
            PerformanceStats.LastGpuOcclusionCullingCulledObjects = 0;
        }

        PerformanceStats.LastGpuFrustumCullingTotalObjects = 0;
        PerformanceStats.LastGpuFrustumCullingVisibleObjects = 0;
        PerformanceStats.LastGpuFrustumCullingCulledObjects = 0;
        PerformanceStats.LastGpuInstanceCullingTotalObjects = 0;
        PerformanceStats.LastGpuInstanceCullingVisibleObjects = 0;
        PerformanceStats.LastGpuInstanceCullingCulledObjects = 0;
        PerformanceStats.LastGpuLodSelectionTotalObjects = 0;
        PerformanceStats.LastGpuLodSelectionCulledObjects = 0;
        PerformanceStats.bLastGpuFrustumPassImplemented = false;
        PerformanceStats.bLastGpuInstancePassImplemented = false;
        PerformanceStats.bLastGpuLodPassImplemented = false;
        PerformanceStats.bLastGpuHiZOcclusionPassExecuted =
            CullingControlSettings.bGpuHiZOcclusionCulling;
    }
}

void FViewportClient::ResetInputState()
{
    bLeftMouseDown = false;
    PendingWheelDelta = 0.0f;
}

bool FViewportClient::InputKey(EKey Key, EInputEvent Event)
{
    if (Key == EKey::LeftMouseButton)
    {
        bLeftMouseDown = (Event != EInputEvent::Released);
        if (Event == EInputEvent::Pressed)
        {
            MouseDownX = LastMouseX;
            MouseDownY = LastMouseY;
        }
        return true;
    }

    if (Key == EKey::RightMouseButton)
    {
        bRightMouseDown = (Event != EInputEvent::Released);
        return true;
    }

    if (Key == EKey::MiddleMouseButton)
    {
        bMiddleMouseDown = (Event != EInputEvent::Released);
        return true;
    }
    return false;
}

bool FViewportClient::InputAxis(EKey Key, float Delta)
{
    if (Key == EKey::MouseWheelAxis)
    {
        PendingWheelDelta += Delta;
        return true;
    }
    return false;
}

bool FViewportClient::MouseMove(int32 X, int32 Y)
{
    if (OwnerWindow != nullptr && ::GetForegroundWindow() != OwnerWindow)
    {
        LastMouseX = X;
        LastMouseY = Y;
        return false;
    }

    const int32 DeltaX = X - LastMouseX;
    const int32 DeltaY = Y - LastMouseY;
    LastMouseX = X;
    LastMouseY = Y;

    if (bRightMouseDown || bMiddleMouseDown)
    {
        CapturedMouseMove(DeltaX, DeltaY);
    }
    return true;
}

bool FViewportClient::CapturedMouseMove(int32 DeltaX, int32 DeltaY)
{
    if (bRightMouseDown)
    {
        CurrentCameraInput.YawDelta += static_cast<float>(DeltaX);
        CurrentCameraInput.PitchDelta += static_cast<float>(DeltaY);
        return true;
    }

    if (bMiddleMouseDown)
    {
        CurrentCameraInput.PanX += static_cast<float>(DeltaX);
        CurrentCameraInput.PanY += static_cast<float>(DeltaY);
        return true;
    }
    return false;
}

void FViewportClient::ProcessClick(int32 X, int32 Y)
{
    UE_LOG(Viewport, ELogLevel::Debug,
           "ProcessClick start: screen=(%d,%d), camera=(%.2f,%.2f,%.2f), pickCount=%llu", X, Y,
           SceneView.GetEyePosition().X, SceneView.GetEyePosition().Y, SceneView.GetEyePosition().Z,
           static_cast<unsigned long long>(PerformanceStats.TotalPickCount));

    const Geometry::FRay WorldRay = Geometry::FRay::BuildRay(
        X, Y, SceneView.GetViewProjectionMatrix(), SceneView.GetCameraWidth(),
        SceneView.GetCameraHeight());
    UE_LOG(Viewport, ELogLevel::Debug,
           "Picking ray: origin=(%.4f,%.4f,%.4f), dir=(%.4f,%.4f,%.4f)",
           WorldRay.Origin.X, WorldRay.Origin.Y, WorldRay.Origin.Z, WorldRay.Direction.X,
           WorldRay.Direction.Y, WorldRay.Direction.Z);

    static const TStatId GViewportProcessClickStatId("Viewport.ProcessClick");
    FScopeCycleCounter   ScopeCounter(GViewportProcessClickStatId);
    ++PerformanceStats.TotalPickCount;

    FRayResult   PickResult;
    bool         bHit = Picker->RayCastWithBVH(WorldRay, PickResult);

    const uint64 ProcessClickCycles = ScopeCounter.Finish();
    PerformanceStats.LastPickTimeMs = FPlatformTime::ToMilliseconds(ProcessClickCycles);
    PerformanceStats.TotalPickTimeMs += PerformanceStats.LastPickTimeMs;

    if (bHit && PickResult.HitComponent != nullptr)
    {
        Gizmo->SetTarget(PickResult.HitComponent);
        Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
        UE_LOG(Viewport, ELogLevel::Info,
               "Picking hit: clicked object UUID=%u, type=%s, address=%p",
               PickResult.HitComponent->UUID, PickResult.HitComponent->GetTypeName(),
               PickResult.HitComponent);
    }
    else
    {
        if (Gizmo != nullptr)
        {
            Gizmo->Flush();
        }
        UE_LOG(Viewport, ELogLevel::Info,
               "Picking miss: no object selected at screen=(%d,%d)", X, Y);
    }

    UE_LOG(Viewport, ELogLevel::Debug,
           "ProcessClick end: hit=%s, pickTime=%.4f ms, totalPickTime=%.4f ms",
           (bHit && PickResult.HitComponent != nullptr) ? "true" : "false",
           PerformanceStats.LastPickTimeMs, PerformanceStats.TotalPickTimeMs);
    LastMouseX = X;
    LastMouseY = Y;
}

EPointerReleaseType FViewportClient::GetReleaseType(int32 X, int32 Y) const
{
    const int32 Dx = X - MouseDownX;
    const int32 Dy = Y - MouseDownY;
    const int32 DistanceSq = Dx * Dx + Dy * Dy;
    return (DistanceSq <= ClickThresholdPixels * ClickThresholdPixels) ? EPointerReleaseType::Click
                                                                       : EPointerReleaseType::Drag;
}

FSceneView FViewportClient::BuildSceneView(float Width, float Height) const
{
    FSceneView Result;
    Result.SetCameraSize(Width, Height);

    const FViewportCameraTransform &Transform = CameraController.GetTransform();
    const FVector3                  Eye = Transform.GetLocation();
    const FVector3                  Forward = Transform.GetRotation().Vector().GetSafeNormal();
    const FVector3 Target = Eye + ((Forward.IsNearlyZero()) ? FVector3::ForwardVector : Forward);

    Result.SetEyePosition(Eye);
    Result.SetViewMatrix(FMatrix::MakeViewLookAtLH(Eye, Target, FVector3::UpVector));

    const float SafeHeight = (Height > 1.0f) ? Height : 1.0f;
    const float AspectRatio = ((Width > 1.0f) ? Width : 1.0f) / SafeHeight;
    if (CameraState.ProjectionMode == EProjectionMode::Orthographic)
    {
        Result.SetProjectionMatrix(FMatrix::MakeOrthographicLH(
            CameraState.OrthoWidth, CameraState.OrthoWidth / AspectRatio, CameraState.NearPlane,
            CameraState.FarPlane));
    }
    else
    {
        Result.SetProjectionMatrix(FMatrix::MakePerspectiveFovLH(
            CameraState.VerticalFovDegrees * DegreesToRadians, AspectRatio, CameraState.NearPlane,
            CameraState.FarPlane));
    }

    Result.UpdateViewProjectionMatrix();
    return Result;
}

void FViewportClient::SetShowGrid(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_Grid);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowGrid() const { return IsFlagSet(ShowFlags, EShowFlags::SF_Grid); }

void FViewportClient::SetShowPrimitives(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_Primitives);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowPrimitives() const
{
    return IsFlagSet(ShowFlags, EShowFlags::SF_Primitives);
}

void FViewportClient::SetShowLines(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_Lines);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowLines() const { return IsFlagSet(ShowFlags, EShowFlags::SF_Lines); }

void FViewportClient::SetGridSpacing(float InSpacing)
{
    UEngineStatics::GridSpacing = FMath::Clamp(InSpacing, 10.0f, 2000.0f);
}

float FViewportClient::GetGridSpacing() const { return UEngineStatics::GridSpacing; }

void FViewportClient::SetShowWorldAxis(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_WorldAxis);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowWorldAxis() const
{
    return IsFlagSet(ShowFlags, EShowFlags::SF_WorldAxis);
}

void FViewportClient::SetShowGizmo(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_Gizmo);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowGizmo() const { return IsFlagSet(ShowFlags, EShowFlags::SF_Gizmo); }

void FViewportClient::SetShowSelectedAABB(bool bEnabled)
{
    const uint32 Current = static_cast<uint32>(ShowFlags);
    const uint32 Bit = static_cast<uint32>(EShowFlags::SF_SelectedAABB);
    ShowFlags = static_cast<EShowFlags>(bEnabled ? (Current | Bit) : (Current & ~Bit));
}

bool FViewportClient::IsShowSelectedAABB() const
{
    return IsFlagSet(ShowFlags, EShowFlags::SF_SelectedAABB);
}

void FViewportClient::SetPerspectiveCameraParams(float InFovDegrees, float InNearPlane,
                                                 float InFarPlane)
{
    CameraState.ProjectionMode = EProjectionMode::Perspective;
    CameraState.VerticalFovDegrees = FMath::Clamp(InFovDegrees, 10.0f, 120.0f);
    CameraState.NearPlane = FMath::Clamp(InNearPlane, 0.01f, 100000.0f);
    CameraState.FarPlane = FMath::Clamp(InFarPlane, CameraState.NearPlane + 1.0f, 500000.0f);
}

void FViewportClient::FlushGizmo() {
    Gizmo->Flush();
    Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
    bGizmoPickMeshUploaded = false;
}

void FViewportClient::SetGizmoHoveringEnabled(bool bEnabled)
{
    if (Gizmo)
    {
        Gizmo->SetHoveringEnabled(bEnabled);
        if (bEnabled)
        {
            LastHoverMouseX = INT_MIN;
            LastHoverMouseY = INT_MIN;
        }
    }
}

bool FViewportClient::IsGizmoHoveringEnabled() const
{
    return (Gizmo != nullptr) ? Gizmo->IsHoveringEnabled() : false;
}

void FViewportClient::SetGizmoMaxScreenSpaceScale(float InMaxScale)
{
    if (Gizmo)
    {
        Gizmo->SetMaxScreenSpaceScale(InMaxScale);
        Gizmo->ApplyScreenSpaceScaling(CameraController.GetTransform().GetLocation());
    }
}

float FViewportClient::GetGizmoMaxScreenSpaceScale() const
{
    return (Gizmo != nullptr) ? Gizmo->GetMaxScreenSpaceScale() : 0.0f;
}
