#include "HydraEngine/Base.h"
#include <format>
#include "Embeded/icon.h"

import HE;
import std;
import Math;
import nvrhi;
import ImGui;
import Utils;

using namespace HE;

class HRay : public Layer
{
public:
	nvrhi::DeviceHandle device;
	nvrhi::CommandListHandle commandList;
    nvrhi::TextureHandle icon, close, min, max, res;

    bool enableTitlebar = true;

	virtual void OnAttach() override
	{
		device = RHI::GetDevice();
		commandList = device->createCommandList();

		Plugins::LoadPluginsInDirectory("Plugins");

        commandList->open();

        // icons
        {
            HE_PROFILE_SCOPE("Load Icons");

            icon = Utils::LoadTexture(Application::GetApplicationDesc().windowDesc.iconFilePath, device, commandList);
            close = Utils::LoadTexture(Buffer(g_icon_close, sizeof(g_icon_close)), device, commandList);
            min = Utils::LoadTexture(Buffer(g_icon_minimize, sizeof(g_icon_minimize)), device, commandList);
            max = Utils::LoadTexture(Buffer(g_icon_maximize, sizeof(g_icon_maximize)), device, commandList);
            res = Utils::LoadTexture(Buffer(g_icon_restore, sizeof(g_icon_restore)), device, commandList);
        }

        commandList->close();
        device->executeCommandList(commandList);
	}

	virtual void OnUpdate(const FrameInfo& info) override
	{
		// UI
		{

            HE_PROFILE_SCOPE_NC("UI", 0xFF0000FF);

            float dpi = ImGui::GetWindowDpiScale();
            float scale = ImGui::GetIO().FontGlobalScale * dpi;
            auto& style = ImGui::GetStyle();
            style.FrameRounding = Math::max(3.0f * scale, 1.0f);
            ImGui::ScopedStyle wb(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);

            if (enableTitlebar)
            {
                bool customTitlebar = Application::GetApplicationDesc().windowDesc.customTitlebar;
                bool isIconClicked = Utils::BeginMainMenuBar(customTitlebar, icon.Get(), close.Get(), min.Get(), max.Get(), res.Get());

                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Restart", "Left Shift + ESC"))
                        Application::Restart();

                    if (ImGui::MenuItem("Exit", "ESC"))
                        Application::Shutdown();

                    if (ImGui::MenuItem("Full Screen", "F", Application::GetWindow().IsFullScreen()))
                        Application::GetWindow().ToggleScreenState();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Window"))
                {
                    if (ImGui::MenuItem("Title Bar", "Left Shift + T", enableTitlebar))
                        enableTitlebar = enableTitlebar ? false : true;

                    ImGui::EndMenu();
                }

                Utils::EndMainMenuBar();
            }
		}

        {
            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::Escape))
                Application::Restart();

            if (Input::IsKeyPressed(Key::Escape))
                Application::Shutdown();

            if (Input::IsKeyReleased(Key::F))
                Application::GetWindow().ToggleScreenState();

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyPressed(Key::T))
                enableTitlebar = enableTitlebar ? false : true;

            if (Input::IsKeyDown(Key::LeftShift) && Input::IsKeyReleased(Key::M))
            {
                if (!Application::GetWindow().IsMaximize())
                    Application::GetWindow().MaximizeWindow();
                else
                    Application::GetWindow().RestoreWindow();
            }

            if (Input::IsKeyPressed(Key::H))
            {
                if (Input::GetCursorMode() == Cursor::Mode::Disabled)
                    Input::SetCursorMode(Cursor::Mode::Normal);
                else
                    Input::SetCursorMode(Cursor::Mode::Disabled);
            }
        }
	}

	virtual void OnBegin(const FrameInfo& info) override
	{
		commandList->open();
		nvrhi::utils::ClearColorAttachment(commandList, info.fb, 0, nvrhi::Color(0.1f));
	}

	virtual void OnEnd(const FrameInfo& info) override
	{
		commandList->close();
		device->executeCommandList(commandList);
	}
};

HE::ApplicationContext* HE::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationDesc desc;

	desc.deviceDesc.api = {
		nvrhi::GraphicsAPI::D3D12,
		nvrhi::GraphicsAPI::VULKAN,
	};

	desc.windowDesc.title = "HRay";
    desc.windowDesc.iconFilePath = "Resources/Icons/Hydra.png";
    desc.windowDesc.customTitlebar = true;
    desc.windowDesc.minWidth = 960;
    desc.windowDesc.minHeight = 540;

	ApplicationContext* ctx = new ApplicationContext(desc);
	Application::PushLayer(new HRay());

	return ctx;
}

#include "HydraEngine/EntryPoint.h"