#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	if (!ImGui::Begin("Control Panel"))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Camera controls moved to the viewport toolbar.");
	ImGui::TextUnformatted("Use the camera button next to Show.");

	ImGui::End();
}
