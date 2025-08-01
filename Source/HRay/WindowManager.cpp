#include "HydraEngine/Base.h"

import HE;
import std;
import ImGui;
import Editor;
import simdjson;

Editor::WindowManager::WindowManager()
{
    scripts.reserve(100);
    descs.reserve(100);
}

Editor::WindowManager::~WindowManager()
{
    for (auto& script : scripts)
    {
        if (script.instance)
        {
            script.instance->OnDestroy();
            script.DestroyScript(&script);
        }
    }
}

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

void Editor::UpdateMenuItems()
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];
        bool s = windowManager.state.contains(desc.title) && windowManager.state.at(desc.title);

        if (ImGui::AutoMenuItem(desc.invokePath.c_str(), nullptr, s, true, ImAutoMenuItemFlags_None))
        {
            if (!script.instance)
            {
                script.instance = script.InstantiateScript();
                script.instance->OnCreate();
                Editor::DeserializeWindow(i);

                windowManager.state[desc.title] = true;
                Editor::Serialize();
            }
            else
            {
                windowManager.state[desc.title] = false;
                Editor::Serialize();
                script.instance->OnDestroy();
                script.DestroyScript(&script);
            }
        }
    }
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
            ImGui::SetNextWindowFocus();
        }

        if (script.instance)
        {
            ImGuiWindowFlags flags = ImGuiWindowFlags_None;
            if (desc.menuBar)
            {
                flags |= ImGuiWindowFlags_MenuBar;
            }

            bool isOpen = true;

            if (desc.focusRequst)
            {
                ImGui::SetNextWindowFocus();
                desc.focusRequst = false;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, desc.padding);
            bool open = ImGui::Begin(std::format("{}  {}", desc.Icon, desc.title).c_str(), &isOpen, flags);
            ImGui::PopStyleVar();

            if (open)
            {
                ImVec2 currentSize = ImGui::GetContentRegionAvail();
                if ((desc.size.x != currentSize.x || desc.size.y != currentSize.y) && desc.size.x > 0 && desc.size.y > 0)
                    script.instance->OnResize(uint32_t(currentSize.x), uint32_t(currentSize.y));
                desc.size = currentSize;
                
                // TODO: Apply this with ImGuiHoveredFlags_RootAndChildWindows only when the mouse enters the window (not on every frame)
                if (ImGui::IsWindowHovered())
                    ImGui::SetWindowFocus();

                bool blockEvent = ImGui::IsAnyItemHovered();

                if (!desc.tooltip.empty()) 
                    ImGui::ToolTip(desc.tooltip.c_str());

                ImGui::ScopedStyle ss(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
                script.instance->OnUpdate(ts);
            }
           
            ImGui::End();

            if (!isOpen)
            {
                windowManager.state[desc.title] = false;
                Editor::Serialize();
                script.instance->OnDestroy();
                script.DestroyScript(&script);
            }
        }
    }
}

void Editor::OpenWindow(const char* windowName)
{
    auto& windowManager = Editor::GetContext().windowManager;
    windowManager.state[windowName] = true;
}

void Editor::FocusWindow(std::string_view windowName)
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        if (script.instance && desc.title == windowName)
        {
            desc.focusRequst = true;
        }
    }
}

void Editor::CloseWindow(std::string_view windowName)
{
    auto& windowManager = Editor::GetContext().windowManager;

    for (int i = 0; i < windowManager.scripts.size(); i++)
    {
        auto& script = windowManager.scripts[i];
        auto& desc = windowManager.descs[i];

        if (script.instance && desc.title == windowName)
        {
            windowManager.state[desc.title] = false;
            Editor::Serialize();
            script.instance->OnDestroy();
            script.DestroyScript(&script);
        }
    }

    Editor::Serialize();
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

    windowManager.state.clear();
}

void Editor::LoadWindowsLayout(WindowsLayout layout)
{
    auto& ctx = Editor::GetContext();

    switch (layout)
    {
    case Editor::WindowsLayout::Default:
    {
        HE::FileSystem::Copy(
            std::filesystem::current_path() / "Resources" / "Layouts" / "Default",
            ctx.project.cacheDir,
            std::filesystem::copy_options::overwrite_existing
        );
        break;
    }
    }

    Editor::DeserializeWindowsState();
    if (std::filesystem::exists(ctx.project.layoutFilePath))
    {
        HE::Jops::SubmitToMainThread([]() {

            auto& ctx = Editor::GetContext();
            ImGui::LoadIniSettingsFromDisk(ctx.project.layoutFilePath.c_str());
        });
    }
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

void Editor::SerializeWindowsState()
{
    auto& ctx = Editor::GetContext();
    auto& windowManager = Editor::GetContext().windowManager;

    std::ofstream file(ctx.project.cacheDir / "winState.json");
    std::ostringstream out;
    out << "{\n";

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

        out << "\t]\n";
    }

    out << "}\n";

    file << out.str();
    file.close();

}

void Editor::DeserializeWindowsState()
{
    HE_PROFILE_FUNCTION();
    
    auto& ctx = Editor::GetContext();
    auto& windowManager = Editor::GetContext().windowManager;

    auto file = (ctx.project.cacheDir / "winState.json").lexically_normal().string();

    if (!std::filesystem::exists(file))
    {
        HE_ERROR("HRAy::Deserialize : Unable to open file for reaading, {}", file);
        return;
    }

    static simdjson::dom::parser parser;
    auto doc = parser.load(file);

    auto windowsState = doc["windowsState"];
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

    ImGui::GetIO().IniFilename = ctx.project.layoutFilePath.c_str();
    if (std::filesystem::exists(ctx.project.layoutFilePath))
    {
        auto path = ctx.project.layoutFilePath;
        HE::Jops::SubmitToMainThread([path]() { ImGui::LoadIniSettingsFromDisk(path.c_str()); });
    }
}