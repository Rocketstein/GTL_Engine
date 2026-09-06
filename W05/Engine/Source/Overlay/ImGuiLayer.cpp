#include "Overlay/ImGuiLayer.h"
#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"
#include <filesystem>

bool FImGuiLayer::Initialize(HWND InHwnd, ID3D11Device *InDevice,
                             ID3D11DeviceContext *InDeviceContext)
{
    if (bInitialized)
    {
        return true;
    }

    if (InHwnd == nullptr || InDevice == nullptr || InDeviceContext == nullptr)
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiStyle &Style = ImGui::GetStyle();
    Style.Colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.20f, 0.30f, 0.54f);
    Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.34f, 0.26f, 0.46f, 0.68f);
    Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.42f, 0.30f, 0.58f, 0.74f);
    Style.Colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.16f, 0.22f, 1.0f);
    Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.30f, 0.22f, 0.42f, 1.0f);
    Style.Colors[ImGuiCol_Tab] = ImVec4(0.22f, 0.18f, 0.30f, 0.86f);
    Style.Colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.29f, 0.60f, 0.90f);
    Style.Colors[ImGuiCol_TabActive] = ImVec4(0.34f, 0.25f, 0.54f, 0.96f);
    Style.Colors[ImGuiCol_Header] = ImVec4(0.36f, 0.22f, 0.62f, 0.50f);
    Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.30f, 0.78f, 0.80f);
    Style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.56f, 0.34f, 0.84f, 1.0f);
    Style.Colors[ImGuiCol_Button] = ImVec4(0.45f, 0.28f, 0.75f, 0.60f);
    Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.55f, 0.35f, 0.85f, 1.0f);
    Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.38f, 0.90f, 1.0f);
    Style.Colors[ImGuiCol_CheckMark] = ImVec4(0.75f, 0.55f, 1.0f, 1.0f);
    Style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.60f, 0.40f, 0.90f, 0.80f);
    Style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.48f, 1.0f, 1.0f);
    Style.Colors[ImGuiCol_Separator] = ImVec4(0.60f, 0.40f, 0.90f, 0.50f);
    Style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.30f, 0.80f, 0.30f);
    Style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.40f, 0.90f, 0.70f);
    Style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.50f, 1.0f, 0.95f);
    Style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.70f, 0.45f, 1.0f, 0.85f);

    ImGuiIO &Io = ImGui::GetIO();
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const float FontSize = 16.0f;
    bool        bFontLoaded = false;
    const char *PreferredFonts[] = {
        "Data/Fonts/CascadiaCode.ttf",
        "Data/Fonts/CascadiaMono.ttf",
        "Data/fonts/CascadiaCode.ttf",
        "Data/fonts/CascadiaMono.ttf",
        "C:/Windows/Fonts/CascadiaCode.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
    };
    for (const char *FontPath : PreferredFonts)
    {
        if (!std::filesystem::exists(FontPath))
        {
            continue;
        }

        if (Io.Fonts->AddFontFromFileTTF(FontPath, FontSize) != nullptr)
        {
            bFontLoaded = true;
            break;
        }
    }

    if (!bFontLoaded)
    {
        Io.Fonts->AddFontDefault();
    }
    Io.FontGlobalScale = 1.08f;

    if (!ImGui_ImplWin32_Init(InHwnd))
    {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX11_Init(InDevice, InDeviceContext))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    bInitialized = true;
    return true;
}

void FImGuiLayer::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    bInitialized = false;
}

void FImGuiLayer::BeginFrame()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void FImGuiLayer::EndFrame()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui::EndFrame();
    ImGui::Render();
}

void FImGuiLayer::Render()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
