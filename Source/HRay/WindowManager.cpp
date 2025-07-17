#include "HydraEngine/Base.h"

import HE;
import std;
import ImGui;
import Editor;
import simdjson;

void Editor::DistroyWindow(Editor::WindowHandle handle)
{
    auto& ctx = Editor::GetContext();
    auto& script = ctx.windowManager.scripts[handle];

    if (script.instance)
    {
        script.instance->OnDestroy();
        script.DestroyScript(&script);
    }
}

void Editor::SaveWindowsState()
{
    Editor::Serialize();
}

void Editor::UpdateWindows(HE::Timestep ts)
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        bool s = windowManager.state.contains(desc.title) && windowManager.state.at(desc.title);
        if (s && !script.instance)
        {
            script.instance = script.InstantiateScript();
            script.instance->OnCreate();
            
            Editor::DeserializeWindow(i);
        }

        if (ImGui::AutoMenuItem(desc.invokePath.c_str(), nullptr, s, true, ImAutoMenuItemFlags_MainMenuBar))
        {
            if (!script.instance)
            {
                script.instance = script.InstantiateScript();
                script.instance->OnCreate();
                Editor::DeserializeWindow(i);
                
                windowManager.state[desc.title] = true;
                SaveWindowsState();
            }
            else
            {
                windowManager.state[desc.title] = false;
                SaveWindowsState();
                script.instance->OnDestroy();
                script.DestroyScript(&script);
            }
        }

        if (script.instance)
        {
            ImGuiWindowFlags flags = ImGuiWindowFlags_None;
            if (desc.menuBar)
            {
                flags |= ImGuiWindowFlags_MenuBar;
            }

            bool isOpen = true;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, desc.padding);
            
            bool open = ImGui::Begin(desc.title.c_str(), &isOpen, flags);
            if (open)
            {
                ImVec2 currentSize = ImGui::GetContentRegionAvail();
                bool resize = (desc.size.x != currentSize.x || desc.size.y != currentSize.y) && desc.size.x > 0 && desc.size.y > 0;
                if (resize)
                {
                    script.instance->OnResize(uint32_t(currentSize.x), uint32_t(currentSize.y));
                }
                desc.size = { currentSize.x, currentSize.y };

                ImGui::PopStyleVar();
                if (ImGui::IsWindowHovered(/*ImGuiHoveredFlags_RootAndChildWindows*/))
                    ImGui::SetWindowFocus();

                bool blockEvent = ImGui::IsAnyItemHovered();

                if (!desc.tooltip.empty()) ImGui::ToolTip(desc.tooltip.c_str());
                script.instance->OnUpdate(ts);
            }
            else
            {
                ImGui::PopStyleVar();
            }
            ImGui::End();

            if (!isOpen)
            {
                windowManager.state[desc.title] = false;
                SaveWindowsState();
                script.instance->OnDestroy();
                script.DestroyScript(&script);
            }
        }
    }
}

void Editor::CloseAllWindows()
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        if (script.instance)
        {
            windowManager.state[desc.title] = false;
            script.instance->OnDestroy();
            script.DestroyScript(&script);
        }
    }

    SaveWindowsState();
}

Editor::WindowDesc& Editor::GetWindowDesc(Editor::WindowHandle handle)
{
    auto& windowManager = Editor::GetContext().windowManager;

    return windowManager.descs[handle];
}

Editor::WindowDesc& Editor::GetWindowDesc(Editor::Window* window)
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        if (windowManager.scripts[i].instance == window)
        {
            return windowManager.descs[i];
        }
    }

    static Editor::WindowDesc desc;
    return desc;
}

void Editor::SerializeWindows(std::ostringstream& out)
{
    auto& windowManager = Editor::GetContext().windowManager;
    
    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        auto serializable = dynamic_cast<Editor::SerializationCallback*>(script.instance);
        if (serializable && script.instance)
        {
            out << "\t\"" << desc.title << "\" : {\n";
            serializable->Serialize(out);
            out << "\t},\n";
        }
    }

    {
        out << "\t\"windowsState\" : [\n";

        for (int i = 0; const auto & [windowTitle, state] : windowManager.state)
        {
            out << "\t\t{\n";
            out << "\t\t\t\"title\" : \"" << windowTitle << "\",\n";
            out << "\t\t\t\"state\" : " << (state ? "true" : "false") << "\n";

            if (i < windowManager.state.size() - 1)
                out << "\t\t},\n";
            else
                out << "\t\t}\n";
            i++;
        }

        out << "\t],\n";
    }
}

void Editor::DeserializeWindows(simdjson::dom::element element)
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        auto serializable = dynamic_cast<Editor::SerializationCallback*>(script.instance);
        if (serializable && script.instance && !element[desc.title].error())
            serializable->Deserialize(element[desc.title]);
    }

    auto windowsState = element["windowsState"];
    if (!windowsState.error())
    {
        for (auto w : windowsState.get_array())
        {
            std::string title;
            bool state = false;

            if (!w["title"].error())
                title = w["title"].get_c_str().value();

            if (!w["state"].error())
                state = w["state"].get_bool().value();

            windowManager.state[title] = state;
        }
    }
}

void Editor::DeserializeWindow(Editor::WindowHandle handle)
{
    HE_PROFILE_FUNCTION();

    auto& ctx = Editor::GetContext();

    if (!std::filesystem::exists(ctx.project.projectFilePath))
    {
        HE_ERROR("Editor::DeserializeWindow : Unable to open file for reaading, {}", ctx.project.projectFilePath.string());
        return;
    }

    auto& windowManager = Editor::GetContext().windowManager;

    static simdjson::dom::parser parser;
    auto doc = parser.load(ctx.project.projectFilePath.string());

    auto& script = windowManager.scripts[handle];
    auto& desc = windowManager.descs[handle];

    auto serializable = dynamic_cast<Editor::SerializationCallback*>(script.instance);
    if (serializable && script.instance && !doc[desc.title].error())
        serializable->Deserialize(doc[desc.title]);
}