#include "Editor/UI/EditorMainPanel.h"
//#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
// Merge icons into default tool font
#include "ImGui/IconsFontAwesome4.h"
#include "Engine/EngineServices/EngineServices.h"
#include "Engine/Render/Resource/RenderResourceManager.h"
#include "Render/Renderer/Renderer.h"
#include "World/Gizmo/GizmoManager.h"
#include "World/Primitives/Primitives.h"
#include "FileManager/SceneSaveManager.h"
#include "Core/Common.h"
#include "Engine/Core/InputSystem.h"
#include "Classes/ASpotlight.h"
#include "World/Primitives/SubUV/SubUV.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorMainPanel::Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine, FEditorViewportClient* InViewport)
{
	Viewport = InViewport;
	EditorEngine = InEditorEngine;
	SelectedPrimitiveType = static_cast<int>(EPrimitiveType::EPT_Cube);
	Renderer = &InRenderer;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)InHWindow);
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	//폰트 아이콘
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();
	ImFontConfig config;
	config.MergeMode = true;
	config.GlyphOffset = ImVec2(0.0f, 2.0f);  // ← 이거 추가 (양수 = 아래로)
	config.GlyphMinAdvanceX = 13.0f; // Use if you want to make the icon monospaced
	static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	io.Fonts->AddFontFromFileTTF("ImGui/fontawesome-webfont.ttf", 13.0f, &config, icon_ranges);
	io.Fonts->AddFontFromFileTTF("ImGui/NanumGothic.otf", 13.0f, &config, io.Fonts->GetGlyphRangesKorean());
}

void FEditorMainPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	// Control Tab
	RenderControlTab(DeltaTime, ViewOutput);

	// Console
	ConsoleInstance.Draw("Console", &bShowConsole);

	// Render Object
	RenderObjectManager(ViewOutput);

	// Picked Object
	RenderPickedObjectWindow(ViewOutput.Object);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderControlTab(float DeltaTime, FViewOutput& ViewOutput)
{
	auto& SceneManager = EditorEngine->GetWorld()->GetSceneManager();
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	// 매 프레임마다 위치/크기 강제 고정
	float PanelWidth = std::clamp(ImGui::GetIO().DisplaySize.x * 0.3f, 200.0f, 400.0f);

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(PanelWidth, ImGui::GetIO().DisplaySize.y * 0.3f), ImGuiCond_Always);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel", nullptr,
		ImGuiWindowFlags_NoMove |        // 드래그 이동 금지
		ImGuiWindowFlags_NoCollapse      // 접기 금지 (선택)
	);

	if (ImGui::BeginTabBar("Jungle Control Panel"))
	{
		// FPS & Memory
		if (ImGui::BeginTabItem("Memory"))
		{
			ImGui::SeparatorText("Memory");

			ImGui::Text("FPS : %.1f", EditorEngine->GetMainLoopFPS());
			ImGui::Text("Memory Allocated : %d", EngineStatics::GetTotalAllocationBytes());
			ImGui::Text("Times Allocated : %d", EngineStatics::GetTotalAllocationCount());

			ImGui::EndTabItem();
		}

		// Scene
		if (ImGui::BeginTabItem("Scene"))
		{
			ImGui::SeparatorText("Spawn");

			ImGui::Combo("Primitive", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

			if (ImGui::Button("Spawn Primitive Actor"))
			{
				for (int i = 0; i < NumberOfSpawnedActors; i++) {
					switch ((EPrimitiveType)SelectedPrimitiveType)
					{
					case EPrimitiveType::EPT_Cube:
						EditorEngine->SpawnNewPrimitiveActor<UCubeComponent>(CurSpawnPoint);
						break;
					case EPrimitiveType::EPT_Sphere:
						EditorEngine->SpawnNewPrimitiveActor<USphereComponent>(CurSpawnPoint);
						break;
					case EPrimitiveType::EPT_Plane:
						EditorEngine->SpawnNewPrimitiveActor<UPlaneComponent>(CurSpawnPoint);
						break;
					case EPrimitiveType::EPT_SubUV:
						EditorEngine->SpawnNewPrimitiveActor<USubUVComponent>(CurSpawnPoint);
						break;
					case EPrimitiveType::EPT_Text:
						EditorEngine->SpawnNewPrimitiveActor<UTextComponent>(CurSpawnPoint);
						break;
					}
				}

				NumberOfSpawnedActors = 1;
			}

			ImGui::Combo("Utility", &SelectedUtilityActorType, UtilTypes, IM_ARRAYSIZE(UtilTypes));
			if (ImGui::Button("Spawn Utility Actor"))
			{
				for (int i = 0; i < NumberOfSpawnedActors; i++) {
					switch ((EUtilType)SelectedUtilityActorType)
					{
					case EUtilType::EUT_Spotlight:
						EditorEngine->SpawnNewUtilActor<ASpotlight>(CurSpawnPoint);
						break;
					}
				}

				NumberOfSpawnedActors = 1;
			}

			ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

			ImGui::SeparatorText("Scene");

			if (ImGui::Button("New Scene")) {
				Viewport->GetGizmoManager().SetVisibility(false);
				ViewOutput.Object = nullptr;
				EditorEngine->NewScene();
				NewSceneNotificationTimer = common::constants::imgui::NotificationTimer;
			}
			if (NewSceneNotificationTimer > 0.0f) {
				NewSceneNotificationTimer -= DeltaTime;
				ImGui::SameLine();
				ImGui::Text("New scene created");
			}

	if (ImGui::Button("Save Scene")) {
		FSceneSaveManager::SaveSceneAsJSON(EditorEngine->GetWorld()->GetActiveScene());
		SceneSaveNotificationTimer = common::constants::imgui::NotificationTimer;
	}
	if (SceneSaveNotificationTimer > 0.0f) {
		SceneSaveNotificationTimer -= DeltaTime;
		ImGui::Text("Scene saved");
	}

	if (ImGui::Button("Load Scene")) {
		UScene* Scene = FSceneSaveManager::LoadSceneFromJSON(EditorEngine->GetWorld());
		if (Scene) {
			Viewport->GetGizmoManager().SetVisibility(false);
			ViewOutput.Object = nullptr;
			SceneManager.AddScene(Scene);
			SceneManager.SetActiveScene(Scene);
			Viewport->ResetViewport();
			EditorEngine->ResetViewportScene();
			Sleep(50);	// Safeguard
			SceneLoadNotificationTimer = common::constants::imgui::NotificationTimer;
		}
	}

			if (ImGui::Button("Delete Current Scene")) {
				if (SceneManager.GetScenes().size() > 1) {
					auto Scenes = SceneManager.GetScenes();
					Viewport->GetGizmoManager().SetVisibility(false);
					ViewOutput.Object = nullptr;
					uint32 RemovedIndex = SceneManager.RemoveActiveScene();
					UScene* NewActiveScene = Scenes[RemovedIndex - 1];
					SceneManager.SetActiveScene(NewActiveScene);
					Viewport->ResetViewport();
					EditorEngine->ResetViewportScene();
				}
			}

			if (SceneLoadNotificationTimer > 0.0f) {
				SceneLoadNotificationTimer -= DeltaTime;
				ImGui::Text("Scene loaded");
			}

			ImGui::EndTabItem();
		}

		// Camera
		if (ImGui::BeginTabItem("Camera"))
		{
			ImGui::SeparatorText("Camera");

			FCameraState& CameraState = Viewport->GetCamera()->GetCameraState();
			ImGui::Checkbox("Orthographic", &(CameraState.bIsOrthogonal));

			float CameraFOV_Deg = CameraState.FOV * (RAD_TO_DEG);
			float OrthoWidth = CameraState.OrthoWidth;

			if (!CameraState.bIsOrthogonal) {
				if (ImGui::DragFloat("Camera FOV",
					&CameraFOV_Deg,
					0.5f,          // speed in degrees
					1.0f,          // min deg
					90.0f))        // max deg
				{
					CameraState.FOV = CameraFOV_Deg * (DEG_TO_RAD);
				}
			}
			else {
				float OrthoWidth = CameraState.OrthoWidth;
				if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
				{
					CameraState.OrthoWidth = Clamp(OrthoWidth, 0.1f, 1000.0f);
				}
			}
			UCamera* Camera = Viewport->GetCamera();
			FVector CamPos = Camera->GetWorldLocation();
			float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
			if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
			{
				Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
			}
			FVector CamRot = Camera->GetRelativeRotation();
			float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
			if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
			{
				Camera->SetRelativeRotation(FVector(Clamp(CameraRotation[0], CamRot.X, CamRot.X), CameraRotation[1], CameraRotation[2]));
			}

			ImGui::SeparatorText("Camera Sensitivity");

			float CamMoveSpeed = FUISettingInitializer::GetViewCamMoveSpeed();
			if (ImGui::SliderFloat("Camera Movement Sensitivity", &CamMoveSpeed, 5.f, 30.f)) {
				FUISettingInitializer::SetViewCamMoveSpeed(CamMoveSpeed);
			}

			float CamRotSpeed = FUISettingInitializer::GetViewCamRotSpeed();
			if (ImGui::SliderFloat("Camera Rotation Sensitivity", &CamRotSpeed, 0.05f, 0.6f)) {
				FUISettingInitializer::SetViewCamRotSpeed(CamRotSpeed);
			}

			ImGui::SeparatorText("Grid");

			int GridSize = static_cast<int>(FUISettingInitializer::GetGridSize());
			if (ImGui::DragInt("Grid Size", &GridSize, 1, 1, 100))
			{
				FUISettingInitializer::SetGridSize(static_cast<uint32>(GridSize));
			}

			ImGui::EndTabItem();
		}

		// Gizmo
		if (ImGui::BeginTabItem("Gizmo"))
		{
			ImGui::SeparatorText("Gizmo");

			// Space Select Button(World or Local)
			static int SelectedSpace = 0;
			if (ImGui::RadioButton("World", &SelectedSpace, 0))
			{
				Viewport->GetGizmoManager().SetWorldSpace(true);
				std::cout << "Switched to World Space\n";
			}

			ImGui::SameLine();

			if (ImGui::RadioButton("Local", &SelectedSpace, 1))
			{
				Viewport->GetGizmoManager().SetWorldSpace(false);
				std::cout << "Switched to Local Space\n";
			}

			ImGui::SeparatorText("Tools");

			if (ImGui::Button("Translate")) Viewport->GetGizmoManager().SetTranslateMode();
			ImGui::SameLine();
			if (ImGui::Button("Rotate")) Viewport->GetGizmoManager().SetRotateMode();
			ImGui::SameLine();
			if (ImGui::Button("Scale")) Viewport->GetGizmoManager().SetScaleMode();

			ImGui::EndTabItem();
		}

		// View Mode
		if (ImGui::BeginTabItem("View"))
		{
			ImGui::SeparatorText("View Mode");

			static int ViewMode = 0;
			if (ImGui::RadioButton(ICON_FA_ADJUST" Lit", &ViewMode, 0)) {
				Renderer->viewMode = EViewModeIndex::VMI_Unlit;
			}

			if (ImGui::RadioButton(ICON_FA_CIRCLE" Unlit", &ViewMode, 1)) {
				Renderer->viewMode = EViewModeIndex::VMI_Unlit;
			}

			if (ImGui::RadioButton(ICON_FA_CODEPEN" Wireframe", &ViewMode, 2)) {
				Renderer->viewMode = EViewModeIndex::VMI_Wireframe;
			}

			ImGui::SeparatorText("Common Show Flag");

			ImGui::CheckboxFlags(ICON_FA_CUBES" Primitives", (int*)&Renderer->showFlag, (uint64)EEngineShowFlags::SF_Primitives);
			ImGui::CheckboxFlags(ICON_FA_FONT " BillboardText", (int*)&Renderer->showFlag, (uint64)EEngineShowFlags::SF_BillboardText);

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void FEditorMainPanel::RenderObjectManager(FViewOutput& ViewOutput) {
	// 매 프레임마다 위치/크기 강제 고정
	float PanelWidth = std::clamp(ImGui::GetIO().DisplaySize.x * 0.25f, 200.0f, 350.0f);

	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(PanelWidth, ImGui::GetIO().DisplaySize.y * 0.3f), ImGuiCond_Always);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);

	ImGui::Begin("Scene Manager", nullptr,
		ImGuiWindowFlags_NoMove |        // 드래그 이동 금지
		ImGuiWindowFlags_NoCollapse      // 접기 금지 (선택)
	);

	auto& SceneManager = EditorEngine->GetWorld()->GetSceneManager();

	// Loaded scenes dropdown
	if (ImGui::CollapsingHeader("Scenes")) {
		const TArray<UScene*>& Scenes = SceneManager.GetScenes();
		for (int i = 0; i < Scenes.size(); i++) {
			UScene* Scene = Scenes[i];
			if (!Scene) continue;

			std::string Label = "Scene_" + std::to_string(Scene->UUID);

			bool bSelected = (SelectedSceneIndex == i);
			if (ImGui::Selectable(Label.c_str(), bSelected)) {
				SelectedSceneIndex = i;
				SceneManager.SetActiveScene(Scene);
				Viewport->ResetViewport();
				EditorEngine->ResetViewportScene();
			}
		}
	}

	// Load actors present in the current scene
	TArray<AActor*> PrimitiveActors;
	TArray<AActor*> NonPrimitiveActors;
	const TArray<AActor*>& Actors = SceneManager.GetActiveScene()->GetActors();
	for (AActor* Actor : Actors) {
		if (!Actor || Actor->bPendingKill) continue;

		// We define Primitive actor = has a UPrimitiveComponent as its root
		if (Actor->GetRootComponent() && Actor->GetRootComponent()->IsA<UPrimitiveComponent>()) {
			PrimitiveActors.push_back(Actor);
		}
		else {
			NonPrimitiveActors.push_back(Actor);
		}
	}

	// Primitive Actors dropdown
	if (ImGui::CollapsingHeader("Scene Actors")) {
		for (int i = 0; i < PrimitiveActors.size(); i++) {
			AActor* Actor = PrimitiveActors[i];
			FString Label = Actor->Name.GetFString();

			if (Label.length() > 0)
			{
				ImGui::Indent();
				if (ImGui::CollapsingHeader(Label.c_str())) {
					for (auto* Comps : Actor->GetComponents()) {
						bool bSelected = (SelectedComponentIndex == Comps->UUID);
						FString CompLabel = Comps->Name.GetFString();
						if (ImGui::Selectable(CompLabel.c_str(), bSelected)) {
							SelectedComponentIndex = Comps->UUID;
							Viewport->GetGizmoManager().SetTarget(Comps);
							ViewOutput.Object = Comps;
							ViewOutput.ObjectPicked = Comps->GetTypeInfo()->name;
						}

						if (bSelected)
						{
							// Text 컴포넌트면 입력창 표시
							if (Comps->IsA<UTextComponent>()) {
								UTextComponent* TextComp = static_cast<UTextComponent*>(Comps);
								DrawTextComponentUI(TextComp);
							}
						}
					}
				}
			}
			ImGui::Unindent();
		}
	}

	// Non-Primitive Actors Dropdown
	if (ImGui::CollapsingHeader("Non-Scene Actors")) {
		for (int i = 0; i < NonPrimitiveActors.size(); i++) {
			AActor* Actor = NonPrimitiveActors[i];
			std::string Label = "Actor_" + std::to_string(Actor->UUID);

			bool bSelected = (SelectedActorIndex == Actor->UUID);
			if (ImGui::Selectable(Label.c_str(), bSelected)) {
				SelectedActorIndex = Actor->UUID;
				Viewport->GetGizmoManager().Deactivate();
			}
		}
	}

	ImGui::End();
}

void FEditorMainPanel::RenderPickedObjectWindow(UObject*& ObjectPicked) {
	// 매 프레임마다 위치/크기 강제 고정
	float PanelWidth = std::clamp(ImGui::GetIO().DisplaySize.x * 0.25f, 200.0f, 350.0f);

	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y * 0.3f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(PanelWidth, ImGui::GetIO().DisplaySize.y * 0.7f), ImGuiCond_Always);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);

	ImGui::Begin("Picked Object", nullptr,
		ImGuiWindowFlags_NoMove |        // 드래그 이동 금지
		ImGuiWindowFlags_NoCollapse      // 접기 금지 (선택));
	);

	if(!ObjectPicked) {
		ImGui::End();
		return;
	}
	if (ObjectPicked) {
		ImGui::Text("Class: %s", ObjectPicked->GetTypeInfo()->name);
		ImGui::Text("Object Size: %d", sizeof(*ObjectPicked));

		auto ObjectName = ObjectPicked->Name.GetFString();
		auto ObjectUUID = ObjectPicked->UUID;
		static std::unordered_map<uint32, std::array<char, 256>> FNameBuffers;
		auto& Buffer = FNameBuffers[ObjectUUID];
		// Seed buffer if empty
		if (Buffer[0] == '\0')
		{
			strncpy_s(Buffer.data(), Buffer.size(), ObjectName.c_str(), _TRUNCATE);
		}
		FString Current = ObjectPicked->Name.GetFString();
		FString InputLabel = "##NameInput" + std::to_string(ObjectUUID);
		if (ImGui::InputText(InputLabel.c_str(), Buffer.data(), Buffer.size())) {
			ObjectPicked->SetNewName(FString(Buffer.data()));
		}
	}
	if (ObjectPicked->IsA<USceneComponent>()) {
		ImGui::Text("Transform");
		ImGui::Separator();

		USceneComponent* SceneComp = ObjectPicked->Cast<USceneComponent>();
		FVector Pos = SceneComp->GetWorldLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		FVector Rot = SceneComp->GetRelativeRotation();
		float RotArray[3] = { Rot.X, Rot.Y, Rot.Z };

		FVector Scale = SceneComp->GetRelativeScale();
		float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };


		FGizmoManager& Gizmo = Viewport->GetGizmoManager();
		if (ImGui::DragFloat3("Location", PosArray, 0.1f))
		{
			Gizmo.SetTargetLocation(FVector(PosArray[0], PosArray[1], PosArray[2]));
		}
		if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
		{
			Gizmo.SetTargetRotation(FVector(RotArray[0], RotArray[1], RotArray[2]));
		}
		if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
		{
			Gizmo.SetTargetScale(FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]));
		}

		if (SceneComp->GetOwningActor()) { RenderPickedActorWindow(SceneComp->GetOwningActor()); }

		// Sub UV
		if (ObjectPicked->IsA< USubUVComponent>()) {
			ImGui::SeparatorText("Sub UV");
			RenderPicekdSubUVWindow(static_cast<USubUVComponent*>(ObjectPicked));
		}

		// Text 컴포넌트면 입력창 표시
		if (ObjectPicked->IsA<UTextComponent>()) {
			SEPARATOR();
			DrawTextComponentUI(static_cast<UTextComponent*>(ObjectPicked));
		}

		SEPARATOR();

		if (ImGui::Button("Remove Object") && ObjectPicked) {
			ObjectPicked->bPendingKill = true;
			if (ObjectPicked->IsA<USceneComponent>()) {
				USceneComponent* SceneComp = ObjectPicked->Cast<USceneComponent>();
				if (SceneComp->GetOwningActor()) {
					// TODO:: Do this recursively
					FObjectManager::Get().DestroyObject(SceneComp->GetOwningActor());
				}
			}
			Viewport->GetGizmoManager().SetVisibility(false);
			Viewport->GetGizmoManager().Deactivate();
			ObjectPicked = nullptr;
		}
	}

	ImGui::End();
}

void FEditorMainPanel::RenderPickedActorWindow(AActor* Actor) {
	ImGui::SeparatorText("Owning Actor");
	auto ActorName = Actor->Name.GetFString();
	auto ActorUUID = Actor->UUID;
	static std::unordered_map<uint32, std::array<char, 256>> FNameBuffers;
	auto& Buffer = FNameBuffers[ActorUUID];
	// Seed buffer if empty
	if (Buffer[0] == '\0')
	{
		strncpy_s(Buffer.data(), Buffer.size(), ActorName.c_str(), _TRUNCATE);
	}
	FString Current = Actor->Name.GetFString();
	FString InputLabel = "##NameInput" + std::to_string(ActorUUID);
	if (ImGui::InputText(InputLabel.c_str(), Buffer.data(), Buffer.size())) {
		Actor->SetNewName(FString(Buffer.data()));
	}

	if (Actor->IsA<ASpotlight>()) {
		ImGui::SeparatorText("Spotlight");
		ASpotlight* SpotlightActor = ASpotlight::Cast(Actor);
		USpotlightComponent* Spotlight = SpotlightActor->GetSpotlightComp();
		float SpotlightRot[2] = { Spotlight->GetYaw(), Spotlight->GetPitch()};
		if (ImGui::DragFloat2("Spotlight Rotation", SpotlightRot, 0.1f)) {
			SpotlightRot[0] = Clamp(SpotlightRot[0], -80.0f, 80.0f); SpotlightRot[1] = Clamp(SpotlightRot[1], -80.0f, 80.0f);
			Spotlight->SetYaw(SpotlightRot[0]); Spotlight->SetPitch(SpotlightRot[1]);
		}

		int NumVertices = Spotlight->GetNumVertices();
		if (ImGui::DragInt("No. Vertices", &NumVertices, 1)) { 
			NumVertices = Clamp(NumVertices, 0, 500);
			if (NumVertices >= 0) {
				Spotlight->SetNumVertex(NumVertices);
			}
		}

		float SpotlightHeight = Spotlight->GetConeHeight();
		if (ImGui::DragFloat("Spotlight Height", &SpotlightHeight, 0.1f)) {SpotlightHeight =  Clamp(SpotlightHeight, 0.1f, 500.0f); Spotlight->SetConeHeight(SpotlightHeight); }

		float SpotlightRadius = Spotlight->GetRadius();
		if (ImGui::DragFloat("Spotlight Radius", &SpotlightRadius, 0.1f)) { SpotlightRadius = Clamp(SpotlightRadius, 0.1f, 100.0f); Spotlight->SetConeRadius(SpotlightRadius); }

		// Color
		FVector4 RayColor = Spotlight->GetColor();
		float Color[4] = { RayColor.X, RayColor.Y, RayColor.Z, RayColor.W };
		if (ImGui::ColorEdit4("Ray Color", Color)) {
			FVector4 NewColor(Color[0], Color[1], Color[2], Color[3]);
			Spotlight->SetColor(NewColor);
		}
	}
}

void FEditorMainPanel::RenderPicekdSubUVWindow(USubUVComponent* SubUVComp)
{
	std::wstring wFileName = L"Texture File: " + SubUVComp->GetFileName();

	//wchar_t Wide[256];
	//MultiByteToWideChar(CP_UTF8, 0, Buffer.data(), -1, Wide, 256);
	//std::wstring WideStr(Wide);


	char strMultibyte[256] = { 0, };
	WideCharToMultiByte(CP_UTF8, 0, wFileName.c_str(), -1, strMultibyte, wFileName.size()*sizeof(WCHAR), NULL, NULL);

	ImGui::Text(strMultibyte);

	if (ImGui::Button("Load New Texture")) {
		std::wstring newFilePath = FEngineServices::GetResourceManager()->LoadResourceFilePath();

		std::replace(newFilePath.begin(), newFilePath.end(), '/', '\\');
		std::wstring FileDestinationW(newFilePath.begin(), newFilePath.end());
		SubUVComp->ReloadTextureResource(FileDestinationW);
	}

	float offsetLeft, offsetRight, offsetUP, offsetBottom;
	offsetLeft = SubUVComp->OffsetLeftX; offsetRight = SubUVComp->OffsetRightX; offsetUP = SubUVComp->OffsetUpY; offsetBottom = SubUVComp->OffsetBottomY;

	int playRate = SubUVComp->PlayRate;
	
	ImGui::Checkbox("is Loop", &(SubUVComp->bLoop));

	if (ImGui::DragInt("Play Rate", &playRate, 1, 60)) {
		SubUVComp->PlayRate = playRate;
	}

	if (ImGui::DragFloat("Offset Left", &offsetLeft,
		0.01f,          // speed in degrees
		0.0f,          // min deg
		1.0f))        // max deg
	{
		SubUVComp->OffsetLeftX = offsetLeft;
	}
;

	if (ImGui::DragFloat("Offset Right", &offsetRight,0.01f,0.0f,1.0f))
		SubUVComp->OffsetRightX = offsetRight;


	if (ImGui::DragFloat("Offset Up", &offsetUP, 0.01f, 0.0f, 1.0f))
		SubUVComp->OffsetUpY = offsetUP;


	if (ImGui::DragFloat("Offset Bottom", &offsetBottom, 0.01f, 0.0f, 1.0f))
		SubUVComp->OffsetBottomY = offsetBottom;

	int rows, columns;
	rows = SubUVComp->CellRows; columns = SubUVComp->CellColumns;

	if (ImGui::DragInt("Rows", &rows, 1, 1, 10))
		SubUVComp->CellRows = rows;


	if (ImGui::DragInt("Columns", &columns, 1, 1, 10))
		SubUVComp->CellColumns = columns;

}

void FEditorMainPanel::DrawTextComponentUI(UTextComponent* TextComp)
{
	// UUID별로 버퍼 관리
	static std::unordered_map<uint32, std::array<char, 256>> TextBuffers;
	auto& Buffer = TextBuffers[TextComp->UUID];

	// 현재 텍스트로 초기화
	std::wstring Current = TextComp->GetText();
	WideCharToMultiByte(CP_UTF8, 0, Current.c_str(), -1, Buffer.data(), 256, nullptr, nullptr);

	ImGui::Text("Text");
	FString InputLabel = "##TextInput" + std::to_string(TextComp->UUID);
	if (ImGui::InputTextMultiline(InputLabel.c_str(), Buffer.data(), Buffer.size(), ImVec2(-1, 50))) {
		wchar_t Wide[256];
		MultiByteToWideChar(CP_UTF8, 0, Buffer.data(), -1, Wide, 256);
		std::wstring WideStr(Wide);
		TextComp->SetText(WideStr);
	}

	// 정렬
	ImGui::Combo("H Align", &SelectedAlignment, TextAlignmentTypes, IM_ARRAYSIZE(TextAlignmentTypes));
	ETextAlignment Alignment = ETextAlignment::Center;
	switch (SelectedAlignment)
	{
	case 0:
		// left
		Alignment = ETextAlignment::Left;
		TextComp->SetTextAlignment(Alignment);
		break;
	case 1:
		// Center
		Alignment = ETextAlignment::Center;
		TextComp->SetTextAlignment(Alignment);
		break;
	case 2:
		// Center
		Alignment = ETextAlignment::Right;
		TextComp->SetTextAlignment(Alignment);
		break;
	default:
		break;
	}

	// Color
	FVector4 Color = TextComp->GetColor();
	float ColorArr[4] = { Color.X, Color.Y, Color.Z, Color.W };
	FString ColorLabel = "Color##" + std::to_string(TextComp->UUID);
	if (ImGui::ColorEdit4(ColorLabel.c_str(), ColorArr)) {
		FVector4 NewColor(ColorArr[0], ColorArr[1], ColorArr[2], ColorArr[3]);
		TextComp->SetTextColor(NewColor);
	}

	// Text Scale
	FVector CurrScale = TextComp->GetTextScale();
	float Width, Height;
	Width = CurrScale.Y;
	Height = CurrScale.Z;
	if (ImGui::DragFloat("Width", &Width, 0.01f, 0.1f, 5.0f))
	{
		CurrScale.Y = Width;
		TextComp->SetTextScale(CurrScale);
	}

	if (ImGui::DragFloat("Height", &Height, 0.01f, 0.1f, 5.0f))
	{
		CurrScale.Z = Height;
		TextComp->SetTextScale(CurrScale);
	}	
}

void FEditorMainPanel::Update()
{
	ImGuiIO& io = ImGui::GetIO();

	InputSystem::GuiInputState.bUsingMouse = io.WantCaptureMouse; 
	InputSystem::GuiInputState.bUsingKeyboard = io.WantCaptureKeyboard;
}
