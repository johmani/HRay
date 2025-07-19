
import HE;
import std;
import Math;
import ImGui;
import Assets;
import Editor;


//////////////////////////////////////////////////////////////////////////
// OrbitCamera
//////////////////////////////////////////////////////////////////////////

Editor::OrbitCamera::OrbitCamera(float fov, float aspectRatio, float near, float far)
{
    view.fov = fov;
    view.near = near;
    view.far = far;
    view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);

    UpdateView();
}

float Editor::OrbitCamera::ZoomSpeed(float distance)
{
    float speed = Math::exp(distance / 5.0f) - 1.0f;
    speed = Math::min(Math::max(speed, 0.3f), 100.0f);
    return speed;
}

void Editor::OrbitCamera::OnUpdate(HE::Timestep ts)
{
    bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (!isWindowFocused)
    {
        UpdateView();
        return;
    }

    auto mp = ImGui::GetMousePos();
    Math::float2 mouse{ mp.x, mp.y };
    Math::float2 delta = (mouse - initialMousePosition) * 0.001f;
    initialMousePosition = mouse;

    if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
    {
        focalPoint -= transform.GetRight() * delta.x * distance;
        focalPoint += transform.GetUp() * delta.y * distance;
    }

    else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
    {
        distance -= delta.y * ZoomSpeed(distance);
        distance = distance <= 0.0f ? 0.0f : distance;
    }
    else if (ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
    {
        float yaw = -delta.x * rotationSpeed;
        float pitch = -delta.y * rotationSpeed;

        Math::quat yawQuat = Math::angleAxis(yaw, Math::float3(0.0f, 1.0f, 0.0f));

        transform.rotation = yawQuat * transform.rotation;
        transform.rotation = Math::angleAxis(pitch, transform.GetRight()) * transform.rotation;

        transform.rotation = Math::normalize(transform.rotation);
    }

    if (isWindowFocused && Math::abs(ImGui::GetIO().MouseWheel) > 0.0f)
    {
        distance -= ImGui::GetIO().MouseWheel * 0.5f;
        UpdateView();
    }

    if (!overrideTransform)
        transform.position = focalPoint + transform.GetForward() * distance;

    UpdateView();
}

void Editor::OrbitCamera::SetViewportSize(uint32_t width, uint32_t height)
{
    // Update Projection
    if (height == 0) return;
    float aspectRatio = float(width) / float(height);
    view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
}

void Editor::OrbitCamera::Focus(Math::float3 position, float pDistance)
{
    focalPoint = position;
    distance = pDistance;

    transform.position = focalPoint + transform.GetForward() * distance;

    UpdateView();
}

void Editor::OrbitCamera::UpdateView()
{
    view.view = Math::inverse(transform.ToMat());
    view.viewProjection = view.projection * view.view;
}

//////////////////////////////////////////////////////////////////////////
// FlyCamera
//////////////////////////////////////////////////////////////////////////

Editor::FlyCamera::FlyCamera(float fov, float aspectRatio, float near, float far, float speed)
{
    view.fov = fov;
    view.near = near;
    view.far = far;
    transform.position = transform.position + transform.GetForward() * 10.0f;
    view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
    speed = speed;
    UpdateView();
}

void Editor::FlyCamera::OnUpdate(HE::Timestep ts)
{
    using ImGuiMouseButton_::ImGuiMouseButton_Right;

    bool isWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (!isWindowFocused)
    {
        UpdateView();
        return;
    }

    auto mp = ImGui::GetMousePos();
    Math::float2 mouse{ mp.x, mp.y };
    Math::float2 delta = (mouse - initialMousePosition);
    initialMousePosition = mouse;

    float mouseSensitivity = 0.1f;
    float mult = 1;

    if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
        mult = 4;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
    
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyDown(ImGuiKey_W))
            transform.position -= transform.GetForward() * Math::float3(ts * speed * mult);
        if (ImGui::IsKeyDown(ImGuiKey_S))
            transform.position += transform.GetForward() * Math::float3(ts * speed * mult);

        if (ImGui::IsKeyDown(ImGuiKey_D))
            transform.position += transform.GetRight() * Math::float3(ts * speed * mult);
        if (ImGui::IsKeyDown(ImGuiKey_A))
            transform.position -= transform.GetRight() * Math::float3(ts * speed * mult);

        if (ImGui::IsKeyDown(ImGuiKey_E))
            transform.position += transform.GetUp() * Math::float3(ts * speed * mult);
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            transform.position -= transform.GetUp() * Math::float3(ts * speed * mult);

        {
            float yaw = -delta.x * ts * mouseSensitivity;
            float pitch = -delta.y * ts * mouseSensitivity;

            Math::quat yawQuat = Math::angleAxis(yaw, Math::float3(0.0f, 1.0f, 0.0f));

            transform.rotation = yawQuat * transform.rotation;
            transform.rotation = Math::angleAxis(pitch, transform.GetRight()) * transform.rotation;

            transform.rotation = Math::normalize(transform.rotation);
        }
    }

    if (Math::abs(ImGui::GetIO().MouseWheel) > 0.0f)
    {
        speed = Math::max(1.0f, speed + ImGui::GetIO().MouseWheel * 2.0f);
    }

    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        transform.position -= delta.y * ts * transform.GetForward();

    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        transform.position -= delta.x * ts * transform.GetRight();
        transform.position += delta.y * ts * transform.GetUp();
    }

    UpdateView();
}

void Editor::FlyCamera::Focus(Math::float3 position, float distance)
{
    transform.position = position + transform.GetForward() * distance;
    UpdateView();
}

void Editor::FlyCamera::SetViewportSize(uint32_t width, uint32_t height)
{
    // Update Projection
    if (height == 0) return;
    float aspectRatio = float(width) / float(height);
    view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
}

void Editor::FlyCamera::UpdateView()
{
    view.view = Math::inverse(transform.ToMat());
    view.viewProjection = view.projection * view.view;
}
