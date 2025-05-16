module;

#include "HydraEngine/Base.h"

export module Editor;

import HE;
import Math;
import ImGui;

export namespace Editor {

	struct CameraTransform
	{
		Math::float3 position = { 0.0f, 0.0f, 0.0f };
		Math::quat rotation = { 1.0f, 0.0f, 0.0f, 0.0f }; //Math::quat(Math::radians(Math::float3(-45.0f, -45.0f, 0.0f)));

		Math::float3 GetRight() { return Math::rotate(rotation, Math::float3(1.0f, 0.0f, 0.0f)); }
		Math::float3 GetUp() { return Math::rotate(rotation, Math::float3(0.0f, 1.0f, 0.0f)); }
		Math::float3 GetForward() { return Math::rotate(rotation, Math::float3(0.0f, 0.0f, 1.0f)); }
		Math::float4x4 ToMat() const { return Math::translate(Math::float4x4(1.0f), position) * Math::toMat4(Math::quat(rotation)); }
	};

	struct View
	{
		Math::float4x4 projection;
		Math::float4x4 viewProjection;
		Math::float4x4 view;
		float fov;
		float near;
		float far;
	};

	class EditorCamera
	{
	public:
		CameraTransform transform;
		View view;

		virtual void OnUpdate(HE::Timestep ts) {}
		virtual void SetViewportSize(uint32_t width, uint32_t height) {}
		virtual void Focus(Math::float3 position, float distance) {}
		virtual void UpdateView() {}
	};

	class OrbitCamera : public EditorCamera
	{
	public:
		float rotationSpeed = 4.0f;

		OrbitCamera() = default;
		OrbitCamera(float fov, float aspectRatio, float near, float far)
		{
			view.fov = fov;
			view.near = near;
			view.far = far;
			view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);

			UpdateView();
		}

		float ZoomSpeed(float distance)
		{
			float speed = Math::exp(distance / 5.0f) - 1.0f;
			speed = Math::min(Math::max(speed, 0.3f), 100.0f);
			return speed;
		}

		void OnUpdate(HE::Timestep ts) override
		{
			bool isIsWindowHovered = ImGui::IsWindowHovered();

			auto mp = ImGui::GetMousePos();
			Math::float2 mouse { mp.x, mp.y };
			Math::float2 delta = (mouse - m_InitialMousePosition) * 0.001f;
			m_InitialMousePosition = mouse;

			if (ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
			{
				m_FocalPoint -= transform.GetRight() * delta.x * m_Distance;
				m_FocalPoint += transform.GetUp() * delta.y * m_Distance;
			}

			else if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiKey_MouseMiddle))
			{
				m_Distance -= delta.y * ZoomSpeed(m_Distance);
				m_Distance = m_Distance <= 0.0f ? 0.0f : m_Distance;
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

			if (isIsWindowHovered && Math::abs(ImGui::GetIO().MouseWheel) > 0.0f)
			{
				m_Distance -= ImGui::GetIO().MouseWheel * 0.5f;
				UpdateView();
			}

			transform.position = m_FocalPoint + transform.GetForward() * m_Distance;
			UpdateView();
		}

		virtual void SetViewportSize(uint32_t width, uint32_t height) override 
		{ 
			// Update Projection
			if (height == 0) return;
			float aspectRatio = float(width) / float(height);
			view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
		}

		virtual void Focus(Math::float3 position, float distance) override
		{
			m_FocalPoint = position;
			m_Distance = distance;

			transform.position = m_FocalPoint + transform.GetForward() * m_Distance;

			UpdateView();
		}

		virtual void UpdateView() override
		{
			view.view = Math::inverse(transform.ToMat());
			view.viewProjection = view.projection * view.view;
		}

	private:
		Math::float3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		Math::float2 m_InitialMousePosition = { 0.0f, 0.0f };
		float m_Distance = 5.0f;
	};

	class FlyCamera : public EditorCamera
	{
	public:

		float speed = 10.0f;

		FlyCamera() = default;
		FlyCamera(float fov, float aspectRatio, float near, float far, float speed = 10.0f)
		{
			view.fov = fov;
			view.near = near;
			view.far = far;
			transform.position = transform.position + transform.GetForward() * 10.0f;
			view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
			speed = speed;
			UpdateView();
		}

		void OnUpdate(HE::Timestep ts) override
		{
			using ImGuiMouseButton_::ImGuiMouseButton_Right;

			bool isIsWindowHovered = ImGui::IsWindowHovered();

			auto mp = ImGui::GetMousePos();
			Math::float2 mouse{ mp.x, mp.y };
			Math::float2 delta = (mouse - m_InitialMousePosition);
			m_InitialMousePosition = mouse;

			float mouseSensitivity = 0.1f;
			float mult = 1;

			if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
				mult = 4;

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				HE::Input::SetCursorMode(HE::Cursor::Mode::Disabled);

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
				HE::Input::SetCursorMode(HE::Cursor::Mode::Normal);

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

		virtual void Focus(Math::float3 position, float distance) override
		{
			transform.position = position + transform.GetForward() * distance;
			UpdateView();
		}

		virtual void SetViewportSize(uint32_t width, uint32_t height) override
		{
			// Update Projection
			if (height == 0) return;
			float aspectRatio = float(width) / float(height);
			view.projection = Math::perspective(Math::radians(view.fov), aspectRatio, view.near, view.far);
		}

		virtual void UpdateView() override
		{
			view.view = Math::inverse(transform.ToMat());
			view.viewProjection = view.projection * view.view;
		}

	private:
			Math::float2 m_InitialMousePosition = { 0.0f, 0.0f };
	};
}