#include "Framework.h"
#include "RenderPipelineBuilder.h"
#include "ComputePipelineBuilder.h"
#include "BindGroup/BindGroup.h"
#include "Buffer/Buffer.h"
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

WGF::Framework WGF::Framework::s_instance;
WGF::Adapter WGF::Adapter::s_instance;
WGF::Device WGF::Device::s_instance;
glm::vec<2, double> WGF::MouseMoveFactory::lastPos;


constexpr uint32_t AGENT_COUNT = 3200000;
constexpr uint32_t WG_COUNT = AGENT_COUNT / 64;

struct Agent
{
	glm::vec2 pos;
	float _pad = 0;
	float dir;
	Agent(glm::vec2& pos, float angle)
		: pos(pos), dir(angle)
	{}
};

struct Settings
{
	float moveSpeed = 50;
	float turnSpeed = 20;
	float sensorAngleDegrees = 30;
	float sensorOffsetDst = 10;
	int sensorSize = 2;
};



constexpr uint32_t BUFFER_SIZE = AGENT_COUNT * sizeof(Agent);

void CreateAgents(std::vector<Agent>& agents, int count)
{
	srand((uint32_t)time(NULL));
	for (size_t i = 0; i < count; i++)
	{
		glm::vec2 pos = { 1000 / 2, 1000 / 2};
		float angle = static_cast<float>(rand() % 360);
		agents.emplace_back(pos, angle);
	}
}


class Application
{
public:
	WGF::RenderPipeline* mainPipeline;
	WGF::BindGroup* mainbindGroup;
	WGF::BindGroup* mainbindGroupSwap;
	WGF::Buffer<glm::vec2>* buffer;

	WGF::ComputePipeline* computePipeline;
	WGF::BindGroup* computebindGroup;
	WGF::BindGroup* computebindGroupSwap;

	WGF::ComputePipeline* agentPipeline;
	WGF::BindGroup* agentBindGroup;
	WGF::BindGroup* agentBindGroupSwap;

	WGF::BindGroup* computeUniforms;
	WGF::Buffer<Settings>* uniformBuffer;
	Settings* settings;

	uint32_t agentCount;

	uint32_t imageWidth;
	uint32_t imageHeight;

	glm::vec2 cursorPos = {500.f, 500.f};

	void OnFrame()
	{
		WGF::Framework::Window()->PollEvents();

		auto surface = WGF::Framework::Window()->GetNextSurfaceTextureView();

		std::swap(mainbindGroup, mainbindGroupSwap);
		auto renderPass = mainPipeline->BeginPass(surface, WGF::Framework::Window()->GetDepthTexView());

		wgpuRenderPassEncoderSetBindGroup(renderPass, 0, mainbindGroup->Get(), 0, nullptr);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, buffer->GetBufferHandle(), 0, buffer->Size() * sizeof(glm::vec2));
		wgpuRenderPassEncoderDraw(renderPass, (uint32_t)buffer->Size(), 1, 0, 0);

		ImGui_ImplWGPU_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		DrawUI();

		ImGui::Render();
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);

		mainPipeline->EndPass();

		wgpuTextureViewRelease(surface);

		ComputePassBlur();
		ComputePassSimulateAgents();
	}

	void ComputePassBlur()
	{
		WGF::Device::CreateEncoder();

		WGPUComputePassDescriptor computePassDesc;
		computePassDesc.timestampWrites = nullptr;
		computePassDesc.label = "compute";
		computePassDesc.nextInChain = nullptr;
		auto computePass = wgpuCommandEncoderBeginComputePass(WGF::Device::GetEncoder(), &computePassDesc);

		wgpuComputePassEncoderSetPipeline(computePass, computePipeline->Get());
		wgpuComputePassEncoderSetBindGroup(computePass, 0, computebindGroup->Get(), 0, nullptr);
		wgpuComputePassEncoderSetBindGroup(computePass, 1, computeUniforms->Get(), 0, nullptr);
		wgpuComputePassEncoderDispatchWorkgroups(computePass, imageWidth / 32, imageHeight, 1);

		wgpuComputePassEncoderEnd(computePass);
		wgpuComputePassEncoderRelease(computePass);

		WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.nextInChain = nullptr;
		cmdBufferDescriptor.label = "Command buffer";
		WGPUCommandBuffer command = wgpuCommandEncoderFinish(WGF::Device::GetEncoder(), &cmdBufferDescriptor);
		WGF::Device::ReleaseEncoder();
		wgpuQueueSubmit(WGF::Device::GetQueue(), 1, &command);
		wgpuCommandBufferRelease(command);

		std::swap(computebindGroup, computebindGroupSwap);
	}

	void ComputePassSimulateAgents()
	{
		WGF::Device::CreateEncoder();

		WGPUComputePassDescriptor computePassDesc;
		computePassDesc.timestampWrites = nullptr;
		computePassDesc.label = "compute";
		computePassDesc.nextInChain = nullptr;
		auto computePass = wgpuCommandEncoderBeginComputePass(WGF::Device::GetEncoder(), &computePassDesc);

		wgpuComputePassEncoderSetPipeline(computePass, agentPipeline->Get());
		wgpuComputePassEncoderSetBindGroup(computePass, 0, agentBindGroup->Get(), 0, nullptr);
		wgpuComputePassEncoderSetBindGroup(computePass, 1, computeUniforms->Get(), 0, nullptr);
		wgpuComputePassEncoderDispatchWorkgroups(computePass, agentCount / 10, 1, 1);

		wgpuComputePassEncoderEnd(computePass);
		wgpuComputePassEncoderRelease(computePass);

		WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
		cmdBufferDescriptor.nextInChain = nullptr;
		cmdBufferDescriptor.label = "Command buffer";
		WGPUCommandBuffer command = wgpuCommandEncoderFinish(WGF::Device::GetEncoder(), &cmdBufferDescriptor);
		WGF::Device::ReleaseEncoder();
		wgpuQueueSubmit(WGF::Device::GetQueue(), 1, &command);
		wgpuCommandBufferRelease(command);

		std::swap(agentBindGroup, agentBindGroupSwap);
	}

	void OnMouseMove( [[ maybe_unused]] WGF::MouseMoveEvent& e)
	{
		e.handled = true;
	}

	void DrawUI()
	{
		ImGui::Begin("Settings");
		ImGui::SliderFloat("Speed", &settings->moveSpeed, 0, 100);
		ImGui::SliderFloat("Turn speed", &settings->turnSpeed, 0, 100);
		ImGui::SliderFloat("Sensor angle", &settings->sensorAngleDegrees, 0, 90);
		ImGui::SliderFloat("Sensor dist", &settings->sensorOffsetDst, 0, 50);
		ImGui::SliderInt("Sensor size", &settings->sensorSize, 0, 5);
		ImGui::End();

		uniformBuffer->Write(settings, 1);
	}
};

int main()
{
	WGF::DeviceLimits limits;
	limits
		.SetTextureLimits(2048, 2048, 512, 8, 8)
		.SetVertexRequiredLimits(8, 2, BUFFER_SIZE, 128)
		.SetInterShaderStageLimits(8, 8)
		.SetBindGroupsLimits(4, 4, 256, 8)
		.SetBindGroupsLimits(4, 4, 256, 8)
		.SetStorageTextureLimits(1)
		.SetStorageBufferLimits(1, BUFFER_SIZE, 0)
		.SetComputeLimits(glm::uvec3(64, 32, 1), 64 * 32, WG_COUNT, 1024)
		.SetSamplersLimits(8, 8);


	WGF::Framework::Init(limits, WGF::WindowParameters(720, 480, "Demo"));
	WGF::Framework::Window()->UseDepth();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	ImGui_ImplWGPU_InitInfo info;
	info.Device = WGF::Device::Get();
	info.RenderTargetFormat = WGF::Framework::Window()->GetTextureFormat();
	info.DepthStencilFormat = WGF::Framework::Window()->GetDepthFormat();
	info.NumFramesInFlight = 3;

	ImGui_ImplGlfw_InitForOther(WGF::Framework::Window()->GetGLFWwin(), true);
	ImGui_ImplWGPU_Init(&info);


	Application app;

	WGF::Texture2D texture;
	texture.GetDescriptor().usage |= WGPUTextureUsage_StorageBinding;
	texture.SetFormat(WGPUTextureFormat_RGBA32Float);
	texture.Init(1024, 1024);
	texture.CreateView();
	WGF::Texture2D texture2;
	texture2.GetDescriptor().usage |= WGPUTextureUsage_StorageBinding;
	texture2.SetFormat(WGPUTextureFormat_RGBA32Float);
	texture2.Init(1024, 1024);
	texture2.CreateView();

	WGF::RenderPipelineBuilder builder;
	builder.SetShaderPath("textureRender.wgsl")
		.SetDepthState(true, WGF::Framework::Window()->GetDepthFormat())
		.AddBindGroup()
		.AddTextureBinding(0, WGPUShaderStage_Vertex | WGPUShaderStage_Fragment, WGPUTextureViewDimension_2D, WGPUTextureSampleType_Float);

	builder.AddBufferLayout(0)
		.AddElement<float>(2);

	glm::vec2 verts[] =
	{
		glm::vec2(0,0),
		glm::vec2(1,0),
		glm::vec2(0,1),
		glm::vec2(0,1),
		glm::vec2(1,0),
		glm::vec2(1,1)
	};

	WGF::Buffer<glm::vec2> vertexBuffer(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, 6);
	vertexBuffer.Write(verts, 6);

	auto pipeline = builder.Build();
	auto bindgroup = WGF::BindGroup(pipeline.GetBindGroupLayout(0));
	bindgroup.AddTextureBinding(0, texture.GetView());
	bindgroup.Init();

	auto bindgroupSwap = WGF::BindGroup(pipeline.GetBindGroupLayout(0));
	bindgroupSwap.AddTextureBinding(0, texture2.GetView());
	bindgroupSwap.Init();

	app.mainbindGroup = &bindgroup;
	app.mainbindGroupSwap = &bindgroupSwap;
	app.mainPipeline = &pipeline;
	app.buffer = &vertexBuffer;

	WGF::ComputePipelineBuilder computebuilder;
	computebuilder.SetShaderPath("compute.wgsl");
	computebuilder.AddBindGroup()
		.AddTextureBinding(0, WGPUShaderStage_Compute, WGPUTextureViewDimension_2D, WGPUTextureSampleType_Float)
		.AddTextureStorage(1, WGPUShaderStage_Compute, WGPUStorageTextureAccess_WriteOnly, WGPUTextureFormat_RGBA32Float, WGPUTextureViewDimension_2D);


	auto computePipeline = computebuilder.Build();
	WGF::BindGroup computeBG(computePipeline.GetBindGroupLayout(0));
	computeBG.AddTextureBinding(0, texture.GetView());
	computeBG.AddTextureBinding(1, texture2.GetView());
	computeBG.Init();

	WGF::BindGroup computeBGSwap(computePipeline.GetBindGroupLayout(0));
	computeBGSwap.AddTextureBinding(0, texture2.GetView());
	computeBGSwap.AddTextureBinding(1, texture.GetView());
	computeBGSwap.Init();

	app.computePipeline = &computePipeline;
	app.computebindGroup = &computeBG;
	app.computebindGroupSwap = &computeBGSwap;
	app.imageHeight = texture2.GetDescriptor().size.height;
	app.imageWidth = texture2.GetDescriptor().size.width;


	WGF::Buffer<Agent> agentBuffer = WGF::Buffer<Agent>(WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage, AGENT_COUNT);
	std::vector<Agent> agents;
	CreateAgents(agents, AGENT_COUNT);
	agentBuffer.Write(agents);


	computebuilder = WGF::ComputePipelineBuilder();
	computebuilder.SetShaderPath("computeAgents.wgsl")
		.AddBindGroup()
		.AddTextureBinding(0, WGPUShaderStage_Compute, WGPUTextureViewDimension_2D, WGPUTextureSampleType_Float)
		.AddTextureStorage(1, WGPUShaderStage_Compute, WGPUStorageTextureAccess_WriteOnly, WGPUTextureFormat_RGBA32Float, WGPUTextureViewDimension_2D)
		.AddStorageBinding(2, WGPUShaderStage_Compute);

	computebuilder
		.AddBindGroup()
		.AddUniformBinding(0, WGPUShaderStage_Compute, sizeof(Settings));

	auto agentPipeline = computebuilder.Build();

	WGF::BindGroup agentBG(agentPipeline.GetBindGroupLayout(0));
	agentBG.AddTextureBinding(0, texture.CreateView());
	agentBG.AddTextureBinding(1, texture2.CreateView());
	agentBG.AddUniformBinding(2, agentBuffer.GetBufferHandle(), 0, AGENT_COUNT * sizeof(Agent));
	agentBG.Init();

	WGF::BindGroup agentBG2(agentPipeline.GetBindGroupLayout(0));
	agentBG2.AddTextureBinding(0, texture2.CreateView());
	agentBG2.AddTextureBinding(1, texture.CreateView());
	agentBG2.AddUniformBinding(2, agentBuffer.GetBufferHandle(), 0, AGENT_COUNT * sizeof(Agent));
	agentBG2.Init();

	Settings settings;

	WGF::Buffer<Settings> uniformBuffer(WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, 1);
	uniformBuffer.Write(&settings, 1);

	WGF::BindGroup uniforms(agentPipeline.GetBindGroupLayout(1));
	uniforms.AddUniformBinding(0, uniformBuffer.GetBufferHandle(), 0, sizeof(Settings));
	uniforms.Init();
	app.computeUniforms = &uniforms;
	app.uniformBuffer = &uniformBuffer;

	app.settings = &settings;
	app.agentCount = WG_COUNT;
	app.agentPipeline = &agentPipeline;
	app.agentBindGroup = &agentBG;
	app.agentBindGroupSwap = &agentBG2;

	//WGF::Framework::Window()->SetMouseMoveCallback(MOUSEMOVE_CALLBACK(Application::OnMouseMove, app));

#if !defined(WEBGPU_BACKEND_WGPU)
#endif

#ifdef __EMSCRIPTEN__

	emscripten_set_main_loop_arg(
		[](void* userData) {
			Application& app = *reinterpret_cast<Application*>(userData);
			app.OnFrame();
		},
		(void*)&app,
		0, true
	);

#else // __EMSCRIPTEN__
	while (!WGF::Framework::Window()->ShouldClose())
	{
		app.OnFrame();
		WGF::Framework::Window()->PresentSurface();
	}

	WGF::Framework::Finish();
#endif // __EMSCRIPTEN__

	return 0;
}