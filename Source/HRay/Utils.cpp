#include "HydraEngine/Base.h"

import Editor;
import HE;
import ImGui;
import Assets;


bool Editor::BeginMainMenuBar(bool customTitlebar, ImTextureRef icon, ImTextureRef close, ImTextureRef min, ImTextureRef max, ImTextureRef res)
{
    if (customTitlebar)
    {
        auto& io = ImGui::GetIO();
        float dpiScale = ImGui::GetWindowDpiScale();
        float scale = ImGui::GetIO().FontGlobalScale * dpiScale;
        ImGuiStyle& style = ImGui::GetStyle();
        static float yframePadding = 8.0f * dpiScale;
        bool isMaximized = HE::Application::GetWindow().IsMaximize();
        auto& title = HE::Application::GetWindow().desc.title;
        bool isIconClicked = false;

        {
            ImGui::ScopedStyle fbs(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::ScopedStyle wbs(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::ScopedStyle wr(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f * dpiScale));
            ImGui::ScopedStyle fp(ImGuiStyleVar_FramePadding, ImVec2{ 0, yframePadding });
            ImGui::ScopedColor scWindowBg(ImGuiCol_WindowBg, ImVec4{ 1.0f, 0.0f, 0.0f, 0.0f });
            ImGui::ScopedColor scMenuBarBg(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 1.0f, 0.0f, 0.0f });
            ImGui::BeginMainMenuBar();
        }

        const ImVec2 windowPadding = style.WindowPadding;
        const ImVec2 titlebarMin = ImGui::GetCursorPos() - ImVec2(windowPadding.x, 0);
        const ImVec2 titlebarMax = ImGui::GetCursorPos() + ImGui::GetWindowSize() - ImVec2(windowPadding.x, 0);
        auto* fgDrawList = ImGui::GetForegroundDrawList();
        auto* bgDrawList = ImGui::GetBackgroundDrawList();

        ImGui::SetCursorPos(titlebarMin);

        ImVec2 screenPos = ImGui::GetCursorScreenPos();

        //fgDrawList->AddRect(screenPos + titlebarMin, screenPos + titlebarMax, ImColor32(255, 0, 0, 255));
        //fgDrawList->AddRect(
        //    ImVec2{ screenPos.x + titlebarMax.x / 2 - ImGui::CalcTextSize(title.c_str()).x, screenPos.y + titlebarMin.y },
        //    ImVec2{ screenPos.x + titlebarMax.x / 2 + ImGui::CalcTextSize(title.c_str()).x, screenPos.y + titlebarMax.y },
        //    ImColor32(255, 0, 0, 255)
        //);

        bgDrawList->AddRectFilledMultiColor(
            screenPos + titlebarMin,
            screenPos + titlebarMax,
            ImColorU32(50, 50, 70, 255),
            ImColorU32(50, 50, 50, 255),
            ImColorU32(50, 50, 50, 255),
            ImColorU32(50, 50, 50, 255)
        );

        bgDrawList->AddRectFilledMultiColor(
            screenPos + titlebarMin,
            screenPos + ImGui::GetCursorPos() + ImGui::GetWindowSize() - ImVec2(ImGui::GetWindowSize().x * 3 / 4, 0),
            ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f)),
            ImColorU32(50, 50, 50, 0),
            ImColorU32(50, 50, 50, 0),
            ImGui::GetColorU32(ImVec4(0.278431f, 0.447059f, 0.701961f, 1.00f))
        );

        if (!ImGui::IsAnyItemHovered() && !HE::Application::GetWindow().IsFullScreen() && ImGui::IsWindowHovered())
        {
            HE::Application::GetWindow().isTitleBarHit = true;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
        else
        {
            HE::Application::GetWindow().isTitleBarHit = false;
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }

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

            ImGui::TextUnformatted(title.c_str());

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
                if (ImGui::ImageButton("min", min, { ySpace, ySpace })) HE::Application::GetWindow().Minimize();

                ImGui::SameLine();
                if (ImGui::ImageButton("max_res", isMaximized ? res : max, { ySpace, ySpace }))
                    isMaximized ? HE::Application::GetWindow().Restore() : HE::Application::GetWindow().Maximize();
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

void Editor::EndMainMenuBar()
{
    ImGui::EndMainMenuBar();
}

void Editor::BeginChildView(const char* str_id, Editor::Corner& corner)
{
    auto& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();

    {
        bool isHiddenTabBar = ImGui::GetCurrentContext()->CurrentWindow->DockNode ? ImGui::GetCurrentContext()->CurrentWindow->DockNode->IsHiddenTabBar() : false;
        float padding = 5 * io.FontGlobalScale;
       
        ImVec2 offset = padding;

        if (corner == Editor::Corner::TopLeft || corner == Editor::Corner::TopRight)
        {
           offset = offset + (isHiddenTabBar ? 0 : ImVec2(0, ImGui::GetFrameHeight() - 2));       // TabBar
           offset = offset + ImVec2(0, ImGui::GetCurrentContext()->CurrentWindow->MenuBarHeight); // MenuBarHeight
        }

        ImVec2 work_pos = ImGui::GetCurrentContext()->CurrentWindow->Pos;
        ImVec2 work_size = ImGui::GetCurrentContext()->CurrentWindow->Size;
        ImVec2 window_pos, window_pos_pivot;

        window_pos.x = ((int)corner & 1) ? (work_pos.x + work_size.x - offset.x) : (work_pos.x + offset.x);
        window_pos.y = ((int)corner & 2) ? (work_pos.y + work_size.y - offset.y) : (work_pos.y + offset.y);
        window_pos_pivot.x = ((int)corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = ((int)corner & 2) ? 1.0f : 0.0f;

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    static ImGuiChildFlags flags = ImGuiChildFlags_ResizeY | ImGuiChildFlags_ResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    ImGui::BeginChild(str_id, ImVec2(0, 0), flags, ImGuiWindowFlags_NoScrollbar);

    {
        ImGui::ScopedStyle wp(ImGuiStyleVar_WindowPadding, ImVec2{ 8, 8 });
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Top-left", nullptr, corner == Editor::Corner::TopLeft)) { corner = Editor::Corner::TopLeft; }
            if (ImGui::MenuItem("Top-right", nullptr, corner == Editor::Corner::TopRight)) { corner = Editor::Corner::TopRight; }
            if (ImGui::MenuItem("Bottom-left", nullptr, corner == Editor::Corner::ButtomLeft)) { corner = Editor::Corner::ButtomLeft; }
            if (ImGui::MenuItem("Bottom-right", nullptr, corner == Editor::Corner::ButtomRight)) { corner = Editor::Corner::ButtomRight; }

            ImGui::EndPopup();
        }
    }
}

void Editor::EndChildView()
{
    ImGui::EndChild();
}

std::string Editor::IncrementString(const std::string& str)
{
    std::string input = str;
    uint32_t number = 0;
    uint32_t multiplier = 1;

    for (size_t i = input.length() - 1; i > 0; --i)
    {
        auto c = input[i];
        if (std::isdigit(c))
        {
            number += (input[i] - '0') * multiplier;
            multiplier *= 10;
        }
        else if (number != 0)
        {
            // If a non-digit character is encountered after we've started
            // accumulating the number, break the loop.
            break;
        }
    }

    std::string strNumber = std::to_string(number + 1);

    std::string result = input;
    size_t pos = result.find_last_of("0123456789"); // Finding the position of the last digit
    if (pos != std::string::npos)
    {
        while (pos > 0 && std::isdigit(result[pos - 1]))
            pos--; // Move back if it's a multi-digit number

        result.replace(pos, result.length() - pos, strNumber); // Replace the number
    }
    else
    {
        result += " " + strNumber;
    }

    return result;
}
