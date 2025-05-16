module;

#include "HydraEngine/Base.h"

export module Utils;

import HE;
import std;
import nvrhi;
import ImGui;

export namespace Utils {

	template<typename T> T Align(T size, T alignment)
	{
		return (size + alignment - 1) & ~(alignment - 1);
	}

	nvrhi::TextureHandle LoadTexture(const std::filesystem::path filePath, nvrhi::IDevice* device, nvrhi::ICommandList* commandList)
	{
		HE::Image image(filePath);
		nvrhi::TextureDesc desc;
		desc.width = image.GetWidth();
		desc.height = image.GetHeight();
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = filePath.string();

		auto texture = device->createTexture(desc);
		commandList->beginTrackingTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
		commandList->writeTexture(texture, 0, 0, image.GetData(), desc.width * 4);
		commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
		commandList->commitBarriers();

		return texture;
	}

	nvrhi::TextureHandle LoadTexture(HE::Buffer buffer, nvrhi::IDevice* device, nvrhi::ICommandList* commandList, const std::string& name = {})
	{
		HE::Image image(buffer);

		nvrhi::TextureDesc desc;
		desc.width = image.GetWidth();
		desc.height = image.GetHeight();
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = name;

		nvrhi::TextureHandle texture = device->createTexture(desc);
		commandList->beginTrackingTextureState(texture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
		commandList->writeTexture(texture, 0, 0, image.GetData(), desc.width * 4);
		commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
		commandList->commitBarriers();

		return texture;
	}

	bool BeginMainMenuBar(bool customTitlebar, ImTextureRef icon, ImTextureRef close, ImTextureRef min, ImTextureRef max, ImTextureRef res)
	{
		if (customTitlebar)
		{
			float dpi = ImGui::GetWindowDpiScale();
			float scale = ImGui::GetIO().FontGlobalScale * dpi;
			ImGuiStyle& style = ImGui::GetStyle();
			static float yframePadding = 8.0f * dpi;
			bool isMaximized = HE::Application::GetWindow().IsMaximize();
			bool isFullScreen = HE::Application::GetWindow().IsFullScreen();
			auto& title = HE::Application::GetApplicationDesc().windowDesc.title;
			bool isIconClicked = false;

			{
				ImGui::ScopedStyle fbs(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::ScopedStyle wbs(ImGuiStyleVar_WindowBorderSize, 0.0f);
				ImGui::ScopedStyle wr(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f * dpi));
				ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2{ 0, yframePadding });
				ImGui::ScopedColor scWindowBg(ImGuiCol_WindowBg, ImVec4{ 1.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor scMenuBarBg(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 1.0f, 0.0f, 0.0f });
				ImGui::BeginMainMenuBar();
			}

			const ImVec2 windowPadding = style.WindowPadding;
			const ImVec2 titlebarMin = ImGui::GetCursorScreenPos() - ImVec2(windowPadding.x, 0);
			const ImVec2 titlebarMax = ImGui::GetCursorScreenPos() + ImGui::GetWindowSize() - ImVec2(windowPadding.x, 0);
			auto* fgDrawList = ImGui::GetForegroundDrawList();
			auto* bgDrawList = ImGui::GetBackgroundDrawList();

			ImGui::SetCursorPos(titlebarMin);

			//fgDrawList->AddRect(titlebarMin, titlebarMax, ImColor32(255, 0, 0, 255));
			//fgDrawList->AddRect(
			//	ImVec2{ titlebarMax.x / 2 - ImGui::CalcTextSize(title.c_str()).x, titlebarMin.y },
			//	ImVec2{ titlebarMax.x / 2 + ImGui::CalcTextSize(title.c_str()).x, titlebarMax.y },
			//	ImColor32(255, 0, 0, 255)
			//);

			bgDrawList->AddRectFilledMultiColor(
				titlebarMin,
				titlebarMax,
				ImColor32(50, 50, 70, 255),
				ImColor32(50, 50, 50, 255),
				ImColor32(50, 50, 50, 255),
				ImColor32(50, 50, 50, 255)
			);

			bgDrawList->AddRectFilledMultiColor(
				titlebarMin,
				ImGui::GetCursorScreenPos() + ImGui::GetWindowSize() - ImVec2(ImGui::GetWindowSize().x * 3 / 4, 0),
				ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f)),
				ImColor32(50, 50, 50, 0),
				ImColor32(50, 50, 50, 0),
				ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f))
			);

			if (!ImGui::IsAnyItemHovered() && !HE::Application::GetWindow().IsFullScreen() && ImGui::IsWindowHovered())
				HE::Application::GetWindow().SetTitleBarState(true);
			else
				HE::Application::GetWindow().SetTitleBarState(false);

			float ySpace = ImGui::GetContentRegionAvail().y;

			// Text
			{
				ImGui::ScopedFont sf(1, 20);
				ImGui::ScopedStyle fr(ImGuiStyleVar_FramePadding, ImVec2{ 8.0f , 8.0f });

				ImVec2 cursorPos = ImGui::GetCursorPos();

				float size = ImGui::CalcTextSize(title.c_str()).x;
				float avail = ImGui::GetContentRegionAvail().x;

				float off = (avail - size) * 0.5f;
				if (off > 0.0f)
					ImGui::ShiftCursorX(off);

				//ImGui::ShiftCursorY(-2 * scale);
				ImGui::TextUnformatted(title.c_str());
				//ImGui::ShiftCursorY(2 * scale);

				ImGui::SetCursorPos(cursorPos);
			}

			// icon
			{
				ImVec2 cursorPos = ImGui::GetCursorPos();

				ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

				if (ImGui::ImageButton("icon", icon, { ySpace, ySpace }))
				{
					isIconClicked = true;
				}

				ImGui::SetCursorPos(cursorPos);
				ImGui::SameLine();

			}

			// buttons
			if(!isFullScreen)
			{
				ImGui::ScopedStyle fr(ImGuiStyleVar_FrameRounding, 0.0f);
				ImGui::ScopedStyle is(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f));
				ImGui::ScopedStyle iis(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0.0f));

				ImVec2 cursorPos = ImGui::GetCursorPos();
				float size = ySpace * 3 + style.FramePadding.x * 6.0f;
				float avail = ImGui::GetContentRegionAvail().x;

				float off = (avail - size);
				if (off > 0.0f)
					ImGui::ShiftCursorX(off);

				{
					ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
					if (ImGui::ImageButton("min", min, { ySpace, ySpace })) HE::Application::GetWindow().MinimizeWindow();

					ImGui::SameLine();
					if (ImGui::ImageButton("max_res", isMaximized ? res : max, { ySpace, ySpace }))
						isMaximized ? HE::Application::GetWindow().RestoreWindow() : HE::Application::GetWindow().MaximizeWindow();
				}

				{
					ImGui::SameLine();
					ImGui::ScopedColor sc0(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
					ImGui::ScopedColor sc1(ImGuiCol_ButtonActive, ImVec4{ 1.0f, 0.3f, 0.2f, 1.0f });
					ImGui::ScopedColor sc2(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.3f, 0.2f, 1.0f });
					if (ImGui::ImageButton("close", close, { ySpace, ySpace })) HE::Application::Shutdown();
				}

				ImGui::SetCursorPos(cursorPos);
			}

			return isIconClicked;
		}
		else
		{
			ImGui::BeginMainMenuBar();
		}

		return false;
	}

	void EndMainMenuBar()
	{
		ImGui::EndMainMenuBar();
	}
}