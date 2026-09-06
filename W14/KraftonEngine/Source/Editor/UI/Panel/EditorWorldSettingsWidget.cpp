#include "Editor/UI/Panel/EditorWorldSettingsWidget.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameMode/GameModeBase.h"
#include "Navigation/NavigationSystem.h"
#include "Object/Reflection/UClass.h"
#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
	void ClampNavigationSettings(FNavigationWorldSettings& Nav)
	{
		Nav.CellSize = std::max(0.25f, Nav.CellSize);
		Nav.MaxSearchNodes = std::max(16, Nav.MaxSearchNodes);
		Nav.AgentRadius = std::max(0.01f, Nav.AgentRadius);
		Nav.AgentHeight = std::max(Nav.AgentRadius * 2.0f, Nav.AgentHeight);
		Nav.AgentStepHeight = std::max(0.0f, Nav.AgentStepHeight);
		Nav.AgentMaxClimbHeight = std::max(0.0f, Nav.AgentMaxClimbHeight);
		Nav.AgentMaxDropHeight = std::max(0.0f, Nav.AgentMaxDropHeight);
		Nav.AgentMaxSlopeDegrees = std::max(0.0f, std::min(89.0f, Nav.AgentMaxSlopeDegrees));
		Nav.ProjectionUp = std::max(0.0f, Nav.ProjectionUp);
		Nav.ProjectionDown = std::max(0.0f, Nav.ProjectionDown);
		Nav.DirectPathSegmentLength = std::max(0.1f, Nav.DirectPathSegmentLength);
		Nav.ObstaclePadding = std::max(0.0f, Nav.ObstaclePadding);
		Nav.RuntimeRebuildDelay = std::max(0.0f, Nav.RuntimeRebuildDelay);
		Nav.RuntimeRebuildMinInterval = std::max(0.0f, Nav.RuntimeRebuildMinInterval);
		Nav.DebugHeightContourInterval = std::max(0.0f, Nav.DebugHeightContourInterval);
		Nav.DebugDrawDuration = std::max(0.0f, Nav.DebugDrawDuration);
		Nav.DebugDrawMaxCells = std::max(0, Nav.DebugDrawMaxCells);
	}
}

void EditorWorldSettingsWidget::Render()
{
	if (!bOpen) return;

	ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("World Settings", &bOpen))
	{
		ImGui::End();
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
	if (!World)
	{
		ImGui::TextDisabled("No active world.");
		ImGui::End();
		return;
	}

	FWorldSettings& WS = World->GetWorldSettings();

	if (ImGui::CollapsingHeader("Game", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// GameMode 클래스 — UClass 레지스트리에서 AGameModeBase 파생만 필터링.
		// 첫 항목 "(Default)" = 빈 문자열 → ProjectSettings fallback.
		TArray<UClass*> GameModeClasses;
		GameModeClasses.push_back(nullptr); // sentinel for "(Default)"
		for (UClass* C : UClass::GetAllClasses())
		{
			if (C && C->IsA(AGameModeBase::StaticClass()))
				GameModeClasses.push_back(C);
		}

		int GMIdx = 0;
		for (int i = 1; i < static_cast<int>(GameModeClasses.size()); ++i)
		{
			if (WS.GameModeClassName == GameModeClasses[i]->GetName())
			{
				GMIdx = i;
				break;
			}
		}

		const char* GMPreview = (GMIdx == 0) ? "(Default)" : GameModeClasses[GMIdx]->GetName();
		if (ImGui::BeginCombo("GameMode Class", GMPreview))
		{
			for (int i = 0; i < static_cast<int>(GameModeClasses.size()); ++i)
			{
				const char* Label = (i == 0) ? "(Default)" : GameModeClasses[i]->GetName();
				bool bSelected = (i == GMIdx);
				if (ImGui::Selectable(Label, bSelected))
				{
					WS.GameModeClassName = (i == 0) ? FString() : FString(GameModeClasses[i]->GetName());
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TextDisabled("Save scene + reload to apply.");
	}

	if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Gravity", WS.Gravity.Data, 0.01f, -100.0f, 100.0f, "%.2f");
		ImGui::TextDisabled("m/s^2, saved with the current scene.");
	}

	if (ImGui::CollapsingHeader("Navigation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		FNavigationWorldSettings& Nav = WS.Navigation;
		bool bChanged = false;

		bChanged |= ImGui::DragFloat("Cell Size", &Nav.CellSize, 0.05f, 0.25f, 50.0f, "%.3f");
		bChanged |= ImGui::DragInt("Max Search Nodes", &Nav.MaxSearchNodes, 64.0f, 16, 131072);
		if (ImGui::TreeNodeEx("Agent / Clearance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bChanged |= ImGui::DragFloat("Agent Radius", &Nav.AgentRadius, 0.01f, 0.01f, 100.0f, "%.3f");
			bChanged |= ImGui::DragFloat("Agent Height", &Nav.AgentHeight, 0.05f, 0.01f, 200.0f, "%.3f");
			bChanged |= ImGui::DragFloat("Agent Step Height", &Nav.AgentStepHeight, 0.01f, 0.0f, 50.0f, "%.3f");
			bChanged |= ImGui::DragFloat("Max Climb Height", &Nav.AgentMaxClimbHeight, 0.01f, 0.0f, 50.0f, "%.3f");
			bChanged |= ImGui::DragFloat("Max Drop Height", &Nav.AgentMaxDropHeight, 0.01f, 0.0f, 50.0f, "%.3f");
			bChanged |= ImGui::DragFloat("Agent Max Slope", &Nav.AgentMaxSlopeDegrees, 1.0f, 0.0f, 89.0f, "%.1f");
			bChanged |= ImGui::DragFloat("Obstacle Padding", &Nav.ObstaclePadding, 0.01f, 0.0f, 10.0f, "%.3f");
			ImGui::TextDisabled("Per-character climb/drop comes from CharacterMovement Max Step/Drop Height at runtime.");
			ImGui::TextDisabled("World values are used for nav build/debug defaults and non-character queries.");
			ImGui::TextDisabled("Blocked cell margin = Agent Radius + Obstacle Padding.");
			ImGui::TreePop();
		}
		bChanged |= ImGui::DragFloat("Projection Up", &Nav.ProjectionUp, 0.1f, 0.0f, 100.0f, "%.2f");
		bChanged |= ImGui::DragFloat("Projection Down", &Nav.ProjectionDown, 0.1f, 0.0f, 100.0f, "%.2f");
		bChanged |= ImGui::DragFloat("Direct Path Segment", &Nav.DirectPathSegmentLength, 0.05f, 0.1f, 100.0f, "%.3f");
		bChanged |= ImGui::Checkbox("Physics Projection Fallback", &Nav.bUsePhysicsProjectionFallback);
		bChanged |= ImGui::Checkbox("Use Built Navigation Data", &Nav.bUseNavigationData);
		bChanged |= ImGui::Checkbox("Auto Rebuild On Path Request", &Nav.bAutoRebuildOnPathRequest);
		bChanged |= ImGui::Checkbox("Allow Runtime Fallback", &Nav.bAllowRuntimeFallback);
		bChanged |= ImGui::Checkbox("Runtime Auto Rebuild", &Nav.bEnableRuntimeAutoRebuild);
		bChanged |= ImGui::DragFloat("Rebuild Delay", &Nav.RuntimeRebuildDelay, 0.01f, 0.0f, 5.0f, "%.2f");
		bChanged |= ImGui::DragFloat("Min Rebuild Interval", &Nav.RuntimeRebuildMinInterval, 0.01f, 0.0f, 10.0f, "%.2f");
		bChanged |= ImGui::Checkbox("Draw Debug On Build", &Nav.bDrawDebugOnBuild);
		bChanged |= ImGui::Checkbox("Draw Blocked Cells", &Nav.bDrawBlockedCells);
		bChanged |= ImGui::Checkbox("Draw Height Colors", &Nav.bDrawHeightColors);
		bChanged |= ImGui::Checkbox("Draw Height Contours", &Nav.bDrawHeightContours);
		bChanged |= ImGui::DragFloat("Height Contour Interval", &Nav.DebugHeightContourInterval, 0.01f, 0.0f, 20.0f, "%.3f");
		bChanged |= ImGui::DragFloat("Debug Draw Duration", &Nav.DebugDrawDuration, 0.1f, 0.0f, 60.0f, "%.1f");
		bChanged |= ImGui::DragInt("Debug Draw Max Cells", &Nav.DebugDrawMaxCells, 100.0f, 0, 200000);

		UNavigationSystem* NavSys = World->GetNavigationSystem();
		if (bChanged)
		{
			ClampNavigationSettings(Nav);
			if (NavSys)
			{
				NavSys->ApplyWorldSettings(Nav);
				NavSys->RequestNavigationRebuild("WorldSettings.Navigation edited", false);
			}
		}

		if (ImGui::Button("Rebuild Now"))
		{
			ClampNavigationSettings(Nav);
			if (NavSys)
			{
				NavSys->ApplyWorldSettings(Nav);
				NavSys->RebuildNavigation();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Draw Debug"))
		{
			if (NavSys)
			{
				NavSys->DebugDrawNavigationData(Nav.DebugDrawDuration);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Invalidate"))
		{
			if (NavSys)
			{
				NavSys->InvalidateNavigationData();
			}
		}

		if (NavSys)
		{
			ImGui::Separator();
			ImGui::TextWrapped("%s", NavSys->GetDebugSummary().c_str());
		}
		else
		{
			ImGui::TextDisabled("NavigationSystem is not initialized.");
		}
	}

	ImGui::End();
}
