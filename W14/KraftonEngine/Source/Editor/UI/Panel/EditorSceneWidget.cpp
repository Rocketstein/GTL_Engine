#include "Editor/UI/Panel/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/Level/LevelEditorViewportClient.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "Object/FName.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats/Stats.h"

#include <cctype>
#include <cstring>

namespace
{
	bool IsBlankRenameName(const FString& Name)
	{
		for (const char Ch : Name)
		{
			if (!std::isspace(static_cast<unsigned char>(Ch)))
			{
				return false;
			}
		}

		return true;
	}
}

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorSceneWidget::Render(float DeltaTime)
{
	if (!EditorEngine)
	{
		return;
	}

	(void)DeltaTime;
	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Outliner");

	// 씬 파일 작업은 상단 메뉴로 옮기고, Scene Manager는 액터 목록만 유지한다.
	RenderActorOutliner();
	HandleSceneManagerShortcuts();
	RenderRenamePopup();

	ImGui::End();
}

void FEditorSceneWidget::HandleSceneManagerShortcuts()
{
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		return;
	}

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	InputSystem& Input = InputSystem::Get();

	if (Input.GetKeyDown(VK_DELETE))
	{
		Selection.DeleteSelectedActors();
		return;
	}

	if (Input.GetKeyDown(VK_F2))
	{
		const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
		if (SelectedActors.size() == 1)
		{
			BeginRenameActor(SelectedActors[0]);
		}
		return;
	}

	if (Input.GetKeyDown('F'))
	{
		if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
		{
			ActiveViewport->FocusOnPrimarySelection();
		}
	}
}

void FEditorSceneWidget::RenderActorOutliner()
{
	SCOPE_STAT_CAT("SceneWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World) return;

	TArray<AActor*> Actors = World->GetActors();

	// null이 아닌 유효 Actor 인덱스만 수집 (Clipper는 연속 인덱스 필요)
	ValidActorIndices.clear();
	ValidActorIndices.reserve(Actors.size());
	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		if (Actors[i]) ValidActorIndices.push_back(i);
	}

	ImGui::Text("Actors (%d)", static_cast<int32>(ValidActorIndices.size()));
	ImGui::Separator();

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();

	ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

	ImGuiListClipper Clipper;
	Clipper.Begin(static_cast<int>(ValidActorIndices.size()));
	while (Clipper.Step())
	{
		for (int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row)
		{
			AActor* Actor = Actors[ValidActorIndices[Row]];

			const FString& StoredName = Actor->GetFName().ToString();
			const char* DisplayName = StoredName.empty()
				? Actor->GetClass()->GetName()
				: StoredName.c_str();

			bool bIsSelected = Selection.IsSelected(Actor);
			ImGui::PushID(Actor);
			if (ImGui::Selectable(DisplayName, bIsSelected))
			{
				if (ImGui::GetIO().KeyShift)
				{
					Selection.SelectRange(Actor, Actors);
				}
				else if (ImGui::GetIO().KeyCtrl)
				{
					Selection.ToggleSelect(Actor);
				}
				else
				{
					Selection.Select(Actor);
				}
			}
			if (ImGui::BeginPopupContextItem("ActorContext"))
			{
				if (ImGui::MenuItem("Rename"))
				{
					BeginRenameActor(Actor);
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
	}

	ImGui::EndChild();
}

void FEditorSceneWidget::BeginRenameActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	RenameTargetActor = TargetActor;
	RenameErrorText.clear();

	const FString CurrentName = TargetActor->GetFName().ToString();
	strncpy_s(RenameBuffer, sizeof(RenameBuffer), CurrentName.c_str(), _TRUNCATE);

	bRenamePopupRequested = true;
	bFocusRenameInputNextFrame = true;
}

void FEditorSceneWidget::RenderRenamePopup()
{
	if (bRenamePopupRequested)
	{
		ImGui::OpenPopup("Rename Actor");
		bRenamePopupRequested = false;
	}

	if (ImGui::BeginPopupModal("Rename Actor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (!IsValid(RenameTargetActor.Get()))
		{
			RenameTargetActor = nullptr;
			RenameBuffer[0] = '\0';
			RenameErrorText.clear();
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}

		ImGui::TextUnformatted("Actor Name");
		ImGui::SetNextItemWidth(320.0f);

		if (bFocusRenameInputNextFrame)
		{
			ImGui::SetKeyboardFocusHere();
			bFocusRenameInputNextFrame = false;
		}

		const bool bSubmit = ImGui::InputText("##ActorRenameInput", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

		if (!RenameErrorText.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", RenameErrorText.c_str());
		}

		const bool bApply = bSubmit || ImGui::Button("OK");
		ImGui::SameLine();
		const bool bCancel = ImGui::Button("Cancel");

		if (bApply)
		{
			const FString NewName(RenameBuffer);
			if (TryRenameActor(RenameTargetActor.Get(), NewName))
			{
				RenameTargetActor = nullptr;
				RenameBuffer[0] = '\0';
				RenameErrorText.clear();
				ImGui::CloseCurrentPopup();
			}
		}
		else if (bCancel)
		{
			RenameTargetActor = nullptr;
			RenameBuffer[0] = '\0';
			RenameErrorText.clear();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

bool FEditorSceneWidget::TryRenameActor(AActor* TargetActor, const FString& NewName)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (IsBlankRenameName(NewName))
	{
		RenameErrorText = "Name cannot be empty.";
		return false;
	}

	const FString CurrentName = TargetActor->GetFName().ToString();
	if (NewName == CurrentName)
	{
		return true;
	}

	UWorld* World = EditorEngine ? EditorEngine->GetWorld() : nullptr;
	if (World)
	{
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || Actor == TargetActor)
			{
				continue;
			}

			if (Actor->GetFName().ToString() == NewName)
			{
				RenameErrorText = "An actor with this name already exists.";
				return false;
			}
		}
	}

	TargetActor->SetFName(FName(NewName));
	return true;
}
