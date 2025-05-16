#include "HydraEngine/Base.h"
#include <format>

import HE;
import std;
import Math;
import nvrhi;

using namespace HE;

class HRay : public Layer
{
public:
	nvrhi::DeviceHandle device;
	nvrhi::CommandListHandle commandList;

	virtual void OnAttach() override
	{
		device = RHI::GetDevice();
		commandList = device->createCommandList();

		Plugins::LoadPluginsInDirectory("Plugins");
	}

	virtual void OnUpdate(const FrameInfo& info) override
	{

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

	ApplicationContext* ctx = new ApplicationContext(desc);
	Application::PushLayer(new HRay());

	return ctx;
}

#include "HydraEngine/EntryPoint.h"