
#include "vk_engine.h"

#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>
#include "vk_textures.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <iostream>
#include <fstream>
#include <array>

#define VK_CHECK(x)												\
	do															\
	{															\
		VkResult err = x;										\
		if (err)												\
		{														\
			std::cout << "Vulkan EROR: " << err << std::endl;	\
			abort();											\
		}														\
	} while (0);												\
																					
void VulkanEngine::init()
{

	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(
		"Vulkan Pinut",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);

	// load core vulkan structures
	init_vulkan();

	// create the swapchain
	init_swapchain();

	// create commands
	init_commands();

	// init fences and sempahores
	init_sync_structures();

	// Load images and meshes for the engine
	load_images();

	load_meshes();

	init_scene();

	// init render pass
	init_default_renderpass();

	init_offscreen_renderpass();

	// create framebuffers
	init_framebuffers();

	init_offscreen_framebuffers();

	init_descriptors();

	init_deferred_descriptors();

	init_deferred_pipelines();

	setup_descriptors();

	init_imgui();

	build_previous_command_buffers();

	mouse_locked = false;
	SDL_ShowCursor(!mouse_locked);

	// Everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {

		for (auto& frames : _frames)
			vkWaitForFences(_device, 1, &frames._renderFence, VK_TRUE, 1000000000);

		_mainDeletionQueue.flush();

		//vmaDestroyAllocator(_allocator);

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw_deferred()
{
	ImGui::Render();

	// Wait until the gpu has finished rendering the last frame. Timeout 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, VK_TRUE, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, get_current_frame()._presentSemaphore, VK_NULL_HANDLE, &_indexSwapchainImage);

	// First pass
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.pWaitDstStageMask	= waitStages;
	submit.waitSemaphoreCount	= 1;
	submit.pWaitSemaphores		= &get_current_frame()._presentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores	= &_offscreenSemaphore;
	submit.commandBufferCount	= 1;
	submit.pCommandBuffers		= &_offscreenComandBuffer;

	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, VK_NULL_HANDLE));

	build_deferred_command_buffer();
	
	// Second pass
	submit.pWaitSemaphores		= &_offscreenSemaphore;
	submit.pSignalSemaphores	= &get_current_frame()._renderSemaphore;
	submit.pCommandBuffers		= &get_current_frame()._mainCommandBuffer;
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext				= nullptr;
	presentInfo.swapchainCount		= 1;
	presentInfo.pSwapchains			= &_swapchain;
	presentInfo.waitSemaphoreCount	= 1;
	presentInfo.pWaitSemaphores		= &get_current_frame()._renderSemaphore;
	presentInfo.pImageIndices		= &_indexSwapchainImage;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));
	_frameNumber++;
}

void VulkanEngine::run()
{
	SDL_Event e;

	double lastFrame = 0.0f;
	while (!_bQuit)
	{
		double currentTime = SDL_GetTicks();
		float dt = float(currentTime - lastFrame);
		lastFrame = currentTime;
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT) _bQuit = true;
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_SPACE) {

				}
				if (e.key.keysym.sym == SDLK_w) {
					_camera->processKeyboard(FORWARD, dt);
				}
				if (e.key.keysym.sym == SDLK_a) {
					_camera->processKeyboard(LEFT, dt);
				}
				if (e.key.keysym.sym == SDLK_s) {
					_camera->processKeyboard(BACKWARD, dt);
				}
				if (e.key.keysym.sym == SDLK_d) {
					_camera->processKeyboard(RIGHT, dt);
				}
				if (e.key.keysym.sym == SDLK_LSHIFT) {
					//_camera->move(glm::vec3(0, -1, 0), dt);
					_camera->processKeyboard(DOWN, dt);
				}
				if (e.key.keysym.sym == SDLK_SPACE) {
					_camera->processKeyboard(UP, dt);
					//_camera->move(glm::vec3(0, 1, 0), dt);
				}
				if (e.key.keysym.sym == SDLK_ESCAPE) _bQuit = true;
			}
			if (e.type == SDL_MOUSEBUTTONDOWN) {
				if(e.button.button == SDL_BUTTON_MIDDLE)
					mouse_locked = !mouse_locked;
			}
			if (e.type == SDL_MOUSEMOTION) {
				if (mouse_locked)
					_camera->rotate(mouse_delta.x, mouse_delta.y);
			}
		}

		renderGUI();

		input_update();
		update(dt);
		draw_deferred();
	}
}

void VulkanEngine::update(const float dt)
{

	glm::mat4 view			= _camera->getView();
	glm::mat4 projection	= glm::perspective(glm::radians(70.0f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	// Fill the GPU camera data sturct
	GPUCameraData cameraData;
	cameraData.view				= view;
	cameraData.projection		= projection;
	cameraData.viewprojection	= projection * view;

	// Copy camera info to the buffer
	void* data;
	vmaMapMemory(_allocator, _cameraBuffer._allocation, &data);
	memcpy(data, &cameraData.viewprojection, sizeof(glm::mat4));
	vmaUnmapMemory(_allocator, _cameraBuffer._allocation);

	// Copy scene data to the buffer
	float framed = (_frameNumber / 120.f);
	_sceneParameters.ambientColor = { sin(framed), 0, cos(framed), 1 };

	char* sceneData;
	vmaMapMemory(_allocator, _sceneBuffer._allocation, (void**)&sceneData);

	//int frameIndex = _frameNumber % FRAME_OVERLAP;
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)); // *frameIndex;

	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, _sceneBuffer._allocation);

	// Copy matrices to the buffer
	void* objData;
	vmaMapMemory(_allocator, _objectBuffer._allocation, &objData);

	GPUObjectData* objectSSBO = (GPUObjectData*)objData;

	for (int i = 0; i < _renderables.size(); i++)
	{
		Object *object = _renderables[i];
		objectSSBO[i].modelMatrix = object->m_matrix;
	}

	vmaUnmapMemory(_allocator, _objectBuffer._allocation);

	void* lightData;
	vmaMapMemory(_allocator, get_current_frame()._lightBuffer._allocation, &lightData);
	uboLight* lightUBO = (uboLight*)lightData;
	for (int i = 0; i < _lights.size(); i++)
	{
		_lights[i]->update();
		Light *l = _lights[i];
		lightUBO[i].color = glm::vec4(l->color.x, l->color.y, l->color.z, l->intensity);
		lightUBO[i].position = glm::vec4(l->position.x, l->position.y, l->position.z, l->maxDistance);
	}
	vmaUnmapMemory(_allocator, get_current_frame()._lightBuffer._allocation);

}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material mat = {};
	mat.pipeline		= pipeline;
	mat.pipelineLayout	= layout;
	_materials[name]	= mat;

	return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
	// Search for the material, return null if not found
	auto it = _materials.find(name);
	if (it == _materials.end())
		return nullptr;
	else
		return &(*it).second;
}

Mesh* VulkanEngine::get_mesh(const std::string& name)
{
	auto it = _meshes.find(name);
	if (it == _meshes.end())
		return nullptr;
	else
		return &(*it).second;
}

Texture* VulkanEngine::get_texture(const std::string& name)
{
	auto it = _textures.find(name);
	if (it == _textures.end()) {
		return nullptr;
	}
	else
		return &(*it).second;
}

int VulkanEngine::get_textureId(const std::string& name)
{
	int i = 0;
	for (auto it : _textures) {
		if (it.first == name)
			break;
		i++;
	}
	return i;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext._commandPool, 1);

	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo submit = vkinit::submit_info(&cmd);

	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, VK_TRUE, 1000000000);
	vkResetFences(_device, 1, &_uploadContext._uploadFence);

	vkResetCommandPool(_device, _uploadContext._commandPool, 0);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;
	
	VmaAllocationCreateInfo vmaallocinfo = {};
	vmaallocinfo.usage = memoryUsage;

	AllocatedBuffer newBuffer;

	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocinfo, 
		&newBuffer._buffer, 
		&newBuffer._allocation, 
		nullptr));

	return newBuffer;

}

// PRIVATE ----------------------------------------

void VulkanEngine::init_vulkan()
{
	vkb::InstanceBuilder builder;
	auto system_info_ret = vkb::SystemInfo::get_system_info();
	auto system_info = system_info_ret.value();
	
	auto inst_ret = builder.set_app_name("Vulkan Pinut")
		.request_validation_layers(true)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.enable_extension("VK_KHR_get_physical_device_properties2")
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	vk::PhysicalDeviceRayTracingFeaturesKHR raytracingFeature;

	std::vector<const char*> required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
		// VkRay
		"VK_KHR_acceleration_structure",
		"VK_KHR_ray_tracing_pipeline",
		VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
	};

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		.add_required_extensions(required_device_extensions)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	uint32_t count;
	vkEnumerateDeviceExtensionProperties(physicalDevice.physical_device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> props(count);
	vkEnumerateDeviceExtensionProperties(physicalDevice.physical_device, nullptr, &count, props.data());

	vkb::Device vkbDevice = deviceBuilder.build().value();

	_device = vkbDevice.device;
	_gpu	= physicalDevice.physical_device;

	_graphicsQueue			= vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily	= vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	// Initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice	= _gpu;
	allocatorInfo.device			= _device;
	allocatorInfo.instance			= _instance;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	vkGetPhysicalDeviceProperties(_gpu, &_gpuProperties);
	std::cout << "The GPU has a minimum buffer alignment of " << _gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyAllocator(_allocator);
		});
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{ _gpu, _device, _surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	// Store swapchain and related images
	_swapchain				= vkbSwapchain.swapchain;
	_swapchainImages		= vkbSwapchain.get_images().value();
	_swapchainImageViews	= vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		});

	VkExtent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	_depthFormat = VK_FORMAT_D32_SFLOAT;
	VkImageCreateInfo depth_info = vkinit::image_create_info(
		_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);
	
	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &depth_info, &dimg_allocinfo, 
		&_depthImage._image, &_depthImage._allocation, nullptr);

	VkImageViewCreateInfo dview_info = vkinit::image_view_create_info(
		_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _depthImageView, nullptr);
		vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
	});
}

void VulkanEngine::init_commands()
{
	
	// Create a command pool for commands to be submitted to the graphics queue
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		// Allocate the default command buffer that will be used for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
		});
	}

	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool));
	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_commandPool));

	VkCommandBufferAllocateInfo cmdDeferredAllocInfo = vkinit::command_buffer_allocate_info(_commandPool);
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdDeferredAllocInfo, &_offscreenComandBuffer));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
		vkDestroyCommandPool(_device, _commandPool, nullptr);
	});
}

void VulkanEngine::init_default_renderpass()
{
	VkAttachmentDescription color_attachment = {};
	color_attachment.format		= _swapchainImageFormat;
	color_attachment.samples	= VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp		= VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp	= VK_ATTACHMENT_STORE_OP_STORE;

	// Do not care about stencil at the moment
	color_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// We do not know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// After the render pass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	// Attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.format			= _depthFormat;
	depth_attachment.samples		= VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.flags			= 0;
	depth_attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDependency, 2> dependencies{};
	dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass		= 0;
	dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass		= 0;
	dependencies[1].dstSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.pNext				= nullptr;
	render_pass_info.attachmentCount	= 2;
	render_pass_info.pAttachments		= &attachments[0];
	render_pass_info.subpassCount		= 1;
	render_pass_info.pSubpasses			= &subpass;
	render_pass_info.dependencyCount	= 2;
	render_pass_info.pDependencies		= dependencies.data();

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
	});

}

void VulkanEngine::init_offscreen_renderpass()
{
	Texture position, normal, albedo, depth;
	create_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &position);
	create_attachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &normal);
	create_attachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &albedo);
	create_attachment(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &depth);

	_deferredTextures.push_back(position);
	_deferredTextures.push_back(normal);
	_deferredTextures.push_back(albedo);
	_deferredTextures.push_back(depth);

	std::array<VkAttachmentDescription, 4> attachmentDescs = {};

	// Init attachment properties
	for (uint32_t i = 0; i < 4; i++)
	{
		attachmentDescs[i].samples			= VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[i].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[i].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[i].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[i].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		if (i == 3)
		{
			attachmentDescs[i].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			attachmentDescs[i].initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
	}

	// Formats
	attachmentDescs[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachmentDescs[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachmentDescs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachmentDescs[3].format = _depthFormat;

	std::vector<VkAttachmentReference> colorReferences;
	colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	VkAttachmentReference depthReference;
	depthReference.attachment = 3;
	depthReference.layout	  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.pColorAttachments		= colorReferences.data();
	subpass.colorAttachmentCount	= static_cast<uint32_t>(colorReferences.size());
	subpass.pDepthStencilAttachment = &depthReference;

	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass		= 0;
	dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass		= 0;
	dependencies[1].dstSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext			= nullptr;
	renderPassInfo.pAttachments		= attachmentDescs.data();
	renderPassInfo.attachmentCount	= static_cast<uint32_t>(attachmentDescs.size());
	renderPassInfo.subpassCount		= 1;
	renderPassInfo.pSubpasses		= &subpass;
	renderPassInfo.dependencyCount	= 2;
	renderPassInfo.pDependencies	= dependencies.data();

	VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_offscreenRenderPass));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, _offscreenRenderPass, nullptr);
		for (int i = 0; i < _deferredTextures.size(); i++) {
			vkDestroyImageView(_device, _deferredTextures[i].imageView, nullptr);
			vmaDestroyImage(_allocator, _deferredTextures[i].image._image, _deferredTextures[i].image._allocation);
		}
	});
}

void VulkanEngine::init_framebuffers()
{
	VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_renderPass, _windowExtent);

	// Grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	for (int i = 0; i < swapchain_imagecount; i++)
	{
		VkImageView attachments[2];
		attachments[0] = _swapchainImageViews[i];
		attachments[1] = _depthImageView;

		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments	= attachments;
		VK_CHECK(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		});
	}
}

void VulkanEngine::init_offscreen_framebuffers()
{
	std::array<VkImageView, 4> attachments;
	attachments[0] = _deferredTextures.at(0).imageView;	// Position
	attachments[1] = _deferredTextures.at(1).imageView;	// Normal
	attachments[2] = _deferredTextures.at(2).imageView;	// Color	
	attachments[3] = _deferredTextures.at(3).imageView;	// Depth
	
	VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_offscreenRenderPass, _windowExtent);
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments	= attachments.data();

	VK_CHECK(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_offscreenFramebuffer));

	VkSamplerCreateInfo sampler = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	sampler.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.mipLodBias		= 0.0f;
	sampler.maxAnisotropy	= 1.0f;
	sampler.minLod			= 0.0f;
	sampler.maxLod			= 1.0f;
	sampler.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK(vkCreateSampler(_device, &sampler, nullptr, &_offscreenSampler));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyFramebuffer(_device, _offscreenFramebuffer, nullptr);
		vkDestroySampler(_device, _offscreenSampler, nullptr);
	});

}

void VulkanEngine::init_sync_structures()
{
	// Create syncronization structures

	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_offscreenSemaphore));

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
		});

		// We do not need any flags for the sempahores

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
		});
	}

	VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();

	VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
		vkDestroySemaphore(_device, _offscreenSemaphore, nullptr);
	});
}

void VulkanEngine::init_descriptors()
{
	std::vector<VkDescriptorPoolSize> sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext			= nullptr;
	pool_info.maxSets		= 10;
	pool_info.flags			= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes	= sizes.data();

	vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptorPool);
	
	// Set = 0
	// binding camera data at 0
	VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	// binding scene data at 1
	VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

	VkDescriptorSetLayoutBinding bindings[] = { cameraBind, sceneBind };

	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.pNext = nullptr;

	setInfo.bindingCount = 2;
	setInfo.pBindings	 = bindings;
	setInfo.flags		 = 0;

	vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_offscreenDescriptorSetLayout);
	
	// Set = 1
	// binding matrices at 0
	VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

	VkDescriptorSetLayoutCreateInfo set2Info = {};
	set2Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set2Info.pNext = nullptr;

	set2Info.bindingCount = 1;
	set2Info.pBindings	  = &objectBind;
	set2Info.flags		  = 0;

	vkCreateDescriptorSetLayout(_device, &set2Info, nullptr, &_objectDescriptorSetLayout);
	
	// Set = 2
	// binding single texture at 0
	uint32_t nText = _textures.size();
	VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, nText);
	
	VkDescriptorSetLayoutCreateInfo set3Info = {};
	set3Info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set3Info.pNext			= nullptr;
	set3Info.bindingCount	= 1;
	set3Info.pBindings		= &textureBind;
	set3Info.flags			= 0;
	
	vkCreateDescriptorSetLayout(_device, &set3Info, nullptr, &_textureDescriptorSetLayout);

	_cameraBuffer = create_buffer(sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	const size_t sceneBufferSize = pad_uniform_buffer_size(sizeof(GPUSceneData));
	_sceneBuffer = create_buffer(sceneBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	const int MAX_OBJECTS = 10000;
	_objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool		= _descriptorPool;
	allocInfo.descriptorSetCount	= 1;
	allocInfo.pSetLayouts			= &_offscreenDescriptorSetLayout;

	vkAllocateDescriptorSets(_device, &allocInfo, &_offscreenDescriptorSet);

	VkDescriptorSetAllocateInfo objectAllocInfo = {};
	objectAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	objectAllocInfo.pNext = nullptr;

	objectAllocInfo.descriptorPool		= _descriptorPool;
	objectAllocInfo.descriptorSetCount	= 1;
	objectAllocInfo.pSetLayouts			= &_objectDescriptorSetLayout;

	vkAllocateDescriptorSets(_device, &objectAllocInfo, &_objectDescriptorSet);

	VkDescriptorBufferInfo cameraInfo = {};
	cameraInfo.buffer	= _cameraBuffer._buffer;
	cameraInfo.offset	= 0;
	cameraInfo.range	= sizeof(glm::mat4);

	VkDescriptorBufferInfo sceneInfo = {};
	sceneInfo.buffer	= _sceneBuffer._buffer;
	sceneInfo.offset	= 0;
	sceneInfo.range		= VK_WHOLE_SIZE;

	VkDescriptorBufferInfo objectInfo = {};
	objectInfo.buffer	= _objectBuffer._buffer;
	objectInfo.offset	= 0;
	objectInfo.range	= sizeof(GPUObjectData) * MAX_OBJECTS;

	VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _offscreenDescriptorSet, &cameraInfo, 0);
	VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _offscreenDescriptorSet, &sceneInfo, 1);
	VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _objectDescriptorSet, &objectInfo, 0);

	VkWriteDescriptorSet writes[] = { cameraWrite, sceneWrite, objectWrite };

	vkUpdateDescriptorSets(_device, 3, writes, 0, nullptr);

	// Textures descriptor ---

	VkDescriptorSetAllocateInfo textureAllocInfo = {};
	textureAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	textureAllocInfo.pNext = nullptr;
	textureAllocInfo.descriptorSetCount = 1;
	textureAllocInfo.pSetLayouts = &_textureDescriptorSetLayout;
	textureAllocInfo.descriptorPool = _descriptorPool;

	VK_CHECK(vkAllocateDescriptorSets(_device, &textureAllocInfo, &_textureDescriptorSet));

	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
	VkSampler sampler;
	vkCreateSampler(_device, &samplerInfo, nullptr, &sampler);

	std::vector<VkDescriptorImageInfo> imageInfos;
	for (auto const& texture : _textures)
	{
		VkDescriptorImageInfo imageBufferInfo = {};
		imageBufferInfo.sampler = sampler;
		imageBufferInfo.imageView = texture.second.imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageInfos.push_back(imageBufferInfo);
	}

	VkWriteDescriptorSet write = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _textureDescriptorSet, imageInfos.data(), 0, _textures.size());

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(_device, _offscreenDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _objectDescriptorSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _textureDescriptorSetLayout, nullptr);
		vmaDestroyBuffer(_allocator, _cameraBuffer._buffer, _cameraBuffer._allocation);
		vmaDestroyBuffer(_allocator, _sceneBuffer._buffer, _sceneBuffer._allocation);
		vmaDestroyBuffer(_allocator, _objectBuffer._buffer, _objectBuffer._allocation);
		vkDestroySampler(_device, sampler, nullptr);
	});
}

void VulkanEngine::init_deferred_pipelines()
{
	VkShaderModule offscreenVertexShader;
	if (!load_shader_module("data/shaders/basic.vert.spv", &offscreenVertexShader)) {
		std::cout << "Could not load geometry vertex shader!" << std::endl;
	}
	VkShaderModule offscreenFragmentShader;
	if (!load_shader_module("data/shaders/geometry_shader.frag.spv", &offscreenFragmentShader)) {
		std::cout << "Could not load geometry fragment shader!" << std::endl;
	}
	VkShaderModule quadVertexShader;
	if (!load_shader_module("data/shaders/quad.vert.spv", &quadVertexShader)) {
		std::cout << "Could not load deferred vertex shader!" << std::endl;
	}
	VkShaderModule deferredFragmentShader;
	if (!load_shader_module("data/shaders/testQuad.frag.spv", &deferredFragmentShader)) {
		std::cout << "Could not load deferred fragment shader!" << std::endl;
	}

	PipelineBuilder pipBuilder;
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, offscreenVertexShader));
	pipBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, offscreenFragmentShader));

	VkDescriptorSetLayout offscreenSetLayouts[] = { _offscreenDescriptorSetLayout, _objectDescriptorSetLayout, _textureDescriptorSetLayout };

	VkPushConstantRange push_constant;
	push_constant.offset		= 0;
	push_constant.size			= sizeof(int);
	push_constant.stageFlags	= VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo offscreenPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	offscreenPipelineLayoutInfo.setLayoutCount			= 3;
	offscreenPipelineLayoutInfo.pSetLayouts				= offscreenSetLayouts;
	offscreenPipelineLayoutInfo.pushConstantRangeCount	= 1;
	offscreenPipelineLayoutInfo.pPushConstantRanges		= &push_constant;

	VK_CHECK(vkCreatePipelineLayout(_device, &offscreenPipelineLayoutInfo, nullptr, &_offscreenPipelineLayout));

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();
	pipBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	pipBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipBuilder._vertexInputInfo.pVertexAttributeDescriptions	= vertexDescription.attributes.data();
	pipBuilder._vertexInputInfo.vertexBindingDescriptionCount	= vertexDescription.bindings.size();
	pipBuilder._vertexInputInfo.pVertexBindingDescriptions		= vertexDescription.bindings.data();

	pipBuilder._pipelineLayout = _offscreenPipelineLayout;

	pipBuilder._inputAssembly		= vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipBuilder._rasterizer			= vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipBuilder._depthStencil		= vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	pipBuilder._viewport.x			= 0.0f;
	pipBuilder._viewport.y			= 0.0f;
	pipBuilder._viewport.maxDepth	= 1.0f;
	pipBuilder._viewport.minDepth	= 0.0f;
	pipBuilder._viewport.width		= (float)_windowExtent.width;
	pipBuilder._viewport.height		= (float)_windowExtent.height;
	pipBuilder._scissor.offset		= { 0, 0 };
	pipBuilder._scissor.extent		= _windowExtent;

	std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates = {
		vkinit::color_blend_attachment_state(
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE), 
		vkinit::color_blend_attachment_state(0xf, VK_FALSE),
		vkinit::color_blend_attachment_state(0xf, VK_FALSE)
	};

	VkPipelineColorBlendStateCreateInfo colorBlendInfo = vkinit::color_blend_state_create_info(static_cast<uint32_t>(blendAttachmentStates.size()), blendAttachmentStates.data());

	pipBuilder._colorBlendStateInfo = colorBlendInfo;
	pipBuilder._multisampling = vkinit::multisample_state_create_info();

	_offscreenPipeline = pipBuilder.build_pipeline(_device, _offscreenRenderPass);

	create_material(_offscreenPipeline, _offscreenPipelineLayout, "offscreen");

	// Second pipeline -----------------------------------------------------------------------------

	VkPushConstantRange push_constant_final;
	push_constant_final.offset = 0;
	push_constant_final.size = sizeof(pushConstants);
	push_constant_final.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayout finalSetLayout[] = { _deferredSetLayout };

	VkPipelineLayoutCreateInfo deferredPipelineLayoutInfo = vkinit::pipeline_layout_create_info();
	deferredPipelineLayoutInfo.setLayoutCount			= 1;
	deferredPipelineLayoutInfo.pSetLayouts				= &_deferredSetLayout;
	deferredPipelineLayoutInfo.pushConstantRangeCount	= 1;
	deferredPipelineLayoutInfo.pPushConstantRanges		= &push_constant_final;

	VK_CHECK(vkCreatePipelineLayout(_device, &deferredPipelineLayoutInfo, nullptr, &_finalPipelineLayout));

	pipBuilder._colorBlendStateInfo = vkinit::color_blend_state_create_info(1, &vkinit::color_blend_attachment_state(0xf, VK_FALSE));

	pipBuilder._shaderStages.clear();
	pipBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, quadVertexShader));
	pipBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, deferredFragmentShader));
	pipBuilder._pipelineLayout = _finalPipelineLayout;

	_finalPipeline = pipBuilder.build_pipeline(_device, _renderPass);

	vkDestroyShaderModule(_device, offscreenVertexShader, nullptr);
	vkDestroyShaderModule(_device, offscreenFragmentShader, nullptr);
	vkDestroyShaderModule(_device, quadVertexShader, nullptr);
	vkDestroyShaderModule(_device, deferredFragmentShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _offscreenPipelineLayout, nullptr);
		vkDestroyPipelineLayout(_device, _finalPipelineLayout, nullptr);
		vkDestroyPipeline(_device, _offscreenPipeline, nullptr);
		vkDestroyPipeline(_device, _finalPipeline, nullptr);
	});
}

void VulkanEngine::init_scene()
{
	_camera = new Camera(glm::vec3(0, 40, 5));

	Light *light = new Light();
	light->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 55, 0));
	Light* light2 = new Light();
	light2->m_matrix = glm::translate(glm::mat4(1), glm::vec3(10, 50, 0));
	light2->color = glm::vec3(1, 0, 0);
	Light* light3 = new Light();
	light3->m_matrix = glm::translate(glm::mat4(1), glm::vec3(-10, 50, 0));
	light3->color = glm::vec3(0, 0, 1);

	_lights.push_back(light);
	_lights.push_back(light2);
	_lights.push_back(light3);

	
	Object* monkey = new Object();
	monkey->mesh = get_mesh("monkey");
	monkey->material = get_material("offscreen");
	glm::mat4 translation = glm::translate(glm::mat4(1.f), glm::vec3(0, 40, -3));
	glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.8));
	monkey->m_matrix = translation * scale;
	
	Object* empire = new Object();
	empire->mesh = get_mesh("empire");
	empire->material = get_material("offscreen");
	empire->id = get_textureId("empire");

	Object* quad = new Object();
	quad->mesh = get_mesh("quad");
	quad->material = get_material("offscreen");
	quad->m_matrix = glm::translate(glm::mat4(1), glm::vec3(5, 40, -5));

	Object* tri = new Object();
	tri->mesh = get_mesh("triangle");
	tri->material = get_material("offscreen");
	tri->m_matrix = glm::translate(glm::mat4(1), glm::vec3(-5, 40, -5));
	tri->id = get_textureId("black");
	

	Object* cube = new Object();
	cube->mesh = get_mesh("cube");
	cube->material = get_material("offscreen");
	cube->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 40, -5));

	Object* sphere = new Object();
	sphere->mesh = get_mesh("sphere");
	sphere->material = get_material("offscreen");
	sphere->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 50, -5));

	_renderables.push_back(empire);
	_renderables.push_back(monkey);
	_renderables.push_back(tri);
	_renderables.push_back(quad);
	_renderables.push_back(cube);
	_renderables.push_back(sphere);
}

void VulkanEngine::setup_descriptors()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext					= nullptr;
		allocInfo.descriptorPool		= _descriptorPool;
		allocInfo.descriptorSetCount	= 1;
		allocInfo.pSetLayouts			= &_deferredSetLayout;

		vkAllocateDescriptorSets(_device, &allocInfo, &_frames[i].deferredDescriptorSet);

		VkDescriptorImageInfo texDescriptorPosition = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Position
		VkDescriptorImageInfo texDescriptorNormal = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Normal
		VkDescriptorImageInfo texDescriptorAlbedo = vkinit::descriptor_image_create_info(
			_offscreenSampler, _deferredTextures[2].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	// Albedo

		int nLights = _lights.size();
		_frames[i]._lightBuffer = create_buffer(sizeof(uboLight) * nLights, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		VkDescriptorBufferInfo lightBuffer;
		lightBuffer.buffer	= _frames[i]._lightBuffer._buffer;
		lightBuffer.offset	= 0;
		lightBuffer.range	= sizeof(uboLight) * nLights;

		std::vector<VkWriteDescriptorSet> writes = {
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorPosition, 0),
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorNormal, 1),
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _frames[i].deferredDescriptorSet, &texDescriptorAlbedo, 2),
			vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].deferredDescriptorSet, &lightBuffer, 3)
		};

		vkUpdateDescriptorSets(_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, _frames[i]._lightBuffer._buffer, _frames[i]._lightBuffer._allocation);
		});
	}
}

void VulkanEngine::init_deferred_descriptors()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
	};

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),	// Position
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),	// Normals
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),	// Albedo
		vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3)
	};

	VkDescriptorSetLayoutCreateInfo setInfo = {};
	setInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.pNext			= nullptr;
	setInfo.bindingCount	= static_cast<uint32_t>(setLayoutBindings.size());
	setInfo.pBindings		= setLayoutBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_deferredSetLayout));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _deferredSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _deferredLightSetLayout, nullptr);
	});

}

void VulkanEngine::init_imgui()
{
	// Create descriptor pool for IMGUI
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext			= nullptr;
	pool_info.maxSets		= 1000;
	pool_info.flags			= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes	= pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// Initialize imgui library
	ImGui::CreateContext();
	
	ImGui_ImplSDL2_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = _instance;
	initInfo.PhysicalDevice = _gpu;
	initInfo.Device = _device;
	initInfo.Queue = _graphicsQueue;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;

	ImGui_ImplVulkan_Init(&initInfo, _renderPass);

	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});

	ImGui_ImplVulkan_DestroyFontUploadObjects();

	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}

void VulkanEngine::build_deferred_command_buffer()
{

	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color		= { 0.0f, 1.0f, 0.0f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass					= _renderPass;
	renderPassBeginInfo.renderArea.extent.width		= _windowExtent.width;
	renderPassBeginInfo.renderArea.extent.height	= _windowExtent.height;
	renderPassBeginInfo.clearValueCount				= static_cast<uint32_t>(clearValues.size());
	renderPassBeginInfo.pClearValues				= clearValues.data();
	renderPassBeginInfo.framebuffer					= _framebuffers[_indexSwapchainImage];

	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	vkBeginCommandBuffer(get_current_frame()._mainCommandBuffer, &cmdBufInfo);

	vkCmdBeginRenderPass(get_current_frame()._mainCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(get_current_frame()._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _finalPipeline);

	VkDeviceSize offset = { 0 };

	Mesh* quad = get_mesh("quad");

	vkCmdPushConstants(get_current_frame()._mainCommandBuffer, _finalPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &_constants);

	vkCmdBindDescriptorSets(get_current_frame()._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _finalPipelineLayout, 0, 1, &get_current_frame().deferredDescriptorSet, 0, nullptr);
	//vkCmdBindDescriptorSets(get_current_frame()._mainCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _finalPipelineLayout, 1, 1, &get_current_frame().deferredLightDescriptorSet, 0, nullptr);
	vkCmdBindVertexBuffers(get_current_frame()._mainCommandBuffer, 0, 1, &quad->_vertexBuffer._buffer, &offset);
	vkCmdBindIndexBuffer(get_current_frame()._mainCommandBuffer, quad->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(get_current_frame()._mainCommandBuffer, static_cast<uint32_t>(quad->_indices.size()), 1, 0, 0, 1);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), get_current_frame()._mainCommandBuffer);

	vkCmdEndRenderPass(get_current_frame()._mainCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(get_current_frame()._mainCommandBuffer));
}

void VulkanEngine::build_previous_command_buffers()
{
	if (_offscreenComandBuffer == VK_NULL_HANDLE)
	{
		VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(_commandPool);
		VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_offscreenComandBuffer));
	}

	VkCommandBufferBeginInfo cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

	std::array<VkClearValue, 4> clearValues;
	clearValues[0].color		= { 0.2f, 0.2f, 0.2f, 1.0f };
	clearValues[1].color		= { 0.0f, 0.5f, 0.0f, 1.0f };
	clearValues[2].color		= { 0.0f, 0.0f, 0.0f, 1.0f };
	clearValues[3].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass					= _offscreenRenderPass;
	renderPassBeginInfo.framebuffer					= _offscreenFramebuffer;
	renderPassBeginInfo.renderArea.extent.width		= _windowExtent.width;
	renderPassBeginInfo.renderArea.extent.height	= _windowExtent.height;
	renderPassBeginInfo.clearValueCount				= static_cast<uint32_t>(clearValues.size());;
	renderPassBeginInfo.pClearValues				= clearValues.data();

	VK_CHECK(vkBeginCommandBuffer(_offscreenComandBuffer, &cmdBufInfo));

	vkCmdBeginRenderPass(_offscreenComandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	// Set = 0 Camera data descriptor
	uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData));
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 0, 1, &_offscreenDescriptorSet, 1, &uniform_offset);
	// Set = 1 Object data descriptor
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 1, 1, &_objectDescriptorSet, 0, nullptr);
	// Set = 2 Texture data descriptor
	vkCmdBindDescriptorSets(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipelineLayout, 2, 1, &_textureDescriptorSet, 0, nullptr);

	Mesh* lastMesh = nullptr;

	for (size_t i = 0; i < _renderables.size(); i++)
	{
		Object *object = _renderables[i];

		vkCmdBindPipeline(_offscreenComandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _offscreenPipeline);

		VkDeviceSize offset = { 0 };

		int constant = object->id;
		vkCmdPushConstants(_offscreenComandBuffer, _offscreenPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &constant);

		if (lastMesh != object->mesh) {
			vkCmdBindVertexBuffers(_offscreenComandBuffer, 0, 1, &object->mesh->_vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(_offscreenComandBuffer, object->mesh->_indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);
			lastMesh = object->mesh;
		}
		vkCmdDrawIndexed(_offscreenComandBuffer, static_cast<uint32_t>(object->mesh->_indices.size()), _renderables.size(), 0, 0, i);
		//vkCmdDraw(_offscreenComandBuffer, static_cast<uint32_t>(object->mesh->_vertices.size()), _renderables.size(), 0, i);
 	}

	vkCmdEndRenderPass(_offscreenComandBuffer);
	VK_CHECK(vkEndCommandBuffer(_offscreenComandBuffer));
}

void VulkanEngine::load_meshes()
{
	Mesh _triangleMesh;
	Mesh _monkeyMesh;
	Mesh _lucy;
	Mesh _lostEmpire{};
	Mesh _quad{};
	Mesh _cube;
	Mesh _sphere;

	_triangleMesh.get_triangle();
	_quad.get_quad();
	//_cube.get_cube();

	_monkeyMesh.load_from_obj("data/meshes/monkey_smooth.obj");
	_lostEmpire.load_from_obj("data/meshes/lost_empire.obj");
	//_lucy.load_from_obj("data/meshes/lucy.obj");
	_cube.load_from_obj("data/meshes/default_cube.obj");
	_sphere.load_from_obj("data/meshes/sphere.obj");

	upload_mesh(_triangleMesh);
	upload_mesh(_quad);
	upload_mesh(_monkeyMesh);
	upload_mesh(_lostEmpire);
	upload_mesh(_cube);
	upload_mesh(_sphere);

	_meshes["triangle"] = _triangleMesh;
	_meshes["quad"]		= _quad;
	_meshes["monkey"]	= _monkeyMesh;
	_meshes["empire"]	= _lostEmpire;
	_meshes["cube"]		= _cube;
	_meshes["sphere"]	= _sphere;
}

void VulkanEngine::load_images()
{
	Texture white;
	vkutil::load_image_from_file(*this, "data/textures/whiteTexture.png", white.image);
	Texture black;
	vkutil::load_image_from_file(*this, "data/textures/blackTexture.png", black.image);
	Texture lostEmpire;
	vkutil::load_image_from_file(*this, "data/textures/lost_empire-RGBA.png", lostEmpire.image);

	VkImageViewCreateInfo whiteImageInfo = vkinit::image_view_create_info(VK_FORMAT_R8G8B8A8_UNORM, white.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
	VkImageViewCreateInfo blackImageInfo = vkinit::image_view_create_info(VK_FORMAT_R8G8B8A8_UNORM, black.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
	VkImageViewCreateInfo imageInfo = vkinit::image_view_create_info(VK_FORMAT_R8G8B8A8_UNORM, lostEmpire.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(_device, &whiteImageInfo, nullptr, &white.imageView);
	vkCreateImageView(_device, &blackImageInfo, nullptr, &black.imageView);
	vkCreateImageView(_device, &imageInfo, nullptr, &lostEmpire.imageView);

	_textures["white"]  = white;
	_textures["black"]	= black;
	_textures["empire"] = lostEmpire;

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, white.imageView, nullptr);
		vkDestroyImageView(_device, black.imageView, nullptr);
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
	});
}

void VulkanEngine::upload_mesh(Mesh& mesh)
{
	create_vertex_buffer(mesh);
	create_index_buffer(mesh);	
}

void VulkanEngine::create_attachment(VkFormat format, VkImageUsageFlagBits usage, Texture* texture)
{
	VkImageAspectFlags aspectMask = 0;
	VkImageLayout imageLayout;

	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // | VK_IMAGE_ASPECT_STENCIL_BIT;
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	assert(aspectMask > 0);

	VkExtent3D extent = { _windowExtent.width, _windowExtent.height, 1 };
	VkImageCreateInfo imageInfo = vkinit::image_create_info(format, usage | VK_IMAGE_USAGE_SAMPLED_BIT, extent);

	VmaAllocationCreateInfo memAlloc = {};
	memAlloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	memAlloc.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &imageInfo, &memAlloc, 
		&texture->image._image, &texture->image._allocation, nullptr);

	VkImageViewCreateInfo imageView = vkinit::image_view_create_info(format, texture->image._image, aspectMask);

	VK_CHECK(vkCreateImageView(_device, &imageView, nullptr, &texture->imageView));
}

void VulkanEngine::create_vertex_buffer(Mesh& mesh)
{
	const size_t bufferSize = mesh._vertices.size() * sizeof(Vertex);

	//allocate staging buffer
	VkBufferCreateInfo stagingBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// let the VMA library know that this data should be on CPU RAM
	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;

	VK_CHECK(vmaCreateBuffer(_allocator, &stagingBufferInfo, &vmaAllocInfo,
		&stagingBuffer._buffer,
		&stagingBuffer._allocation,
		nullptr));

	// Copy Vertex data
	void* data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	VkBufferCreateInfo vertexBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaAllocInfo,
		&mesh._vertexBuffer._buffer,
		&mesh._vertexBuffer._allocation,
		nullptr));

	// Copy vertex data
	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._vertexBuffer._buffer, 1, &copy);
		});

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation);
		});

	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

void VulkanEngine::create_index_buffer(Mesh& mesh)
{
	const size_t bufferSize = mesh._indices.size() * sizeof(uint32_t);
	VkBufferCreateInfo stagingBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer;

	VK_CHECK(vmaCreateBuffer(_allocator, &stagingBufferInfo, &vmaAllocInfo,
		&stagingBuffer._buffer,
		&stagingBuffer._allocation,
		nullptr));

	void* data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, mesh._indices.data(), mesh._indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VkBufferCreateInfo indexBufferInfo = vkinit::buffer_create_info(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	VK_CHECK(vmaCreateBuffer(_allocator, &indexBufferInfo, &vmaAllocInfo,
		&mesh._indexBuffer._buffer,
		&mesh._indexBuffer._allocation,
		nullptr));

	// Copy index data
	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._indexBuffer._buffer, 1, &copy);
		});

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyBuffer(_allocator, mesh._indexBuffer._buffer, mesh._indexBuffer._allocation);
		});

	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
	// open file in binary mode with cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}

	// Find up the size by looking up the location of the cursor
	// Since it is at the end, it gives the number of bytes
	size_t fileSize = (size_t)file.tellg();
	//std::vector<char> buffer(fileSize);

	// spirv expects the buffer to be on uint32, make sure to reserve a int vector big enough for the entire file
	std::vector<uint32_t> buffer((fileSize / sizeof(uint32_t)));

	// Put the cursor back to the beginning
	file.seekg(0);

	// Load the entire file to the buffer
	file.read((char*)buffer.data(), fileSize);

	// The file is loaded to the buffer, we can now close it
	file.close();

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	// CodeSize is in bytes, multiply the ints of the buffer by the size of int
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	// Check if creation goes well
	VkShaderModule module;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &module) != VK_SUCCESS)
		return false;

	*outShaderModule = module;
	return true;

}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
	// Calculate required alignment based on minimun device offset alignment
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

// Input functions
// TODO: Maybe create a header file with all input functions and parameters?

void VulkanEngine::input_update()
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	mouse_delta = glm::vec2(mouse_position.x - x, mouse_position.y - y);
	mouse_position = glm::vec2(x, y);

	SDL_ShowCursor(!mouse_locked);

	ImGui::SetMouseCursor(mouse_locked ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow);

	if (mouse_locked)
		center_mouse();
}

void VulkanEngine::center_mouse()
{
	int window_width, window_height;
	SDL_GetWindowSize(_window, &window_width, &window_height);
	int center_x = (int)glm::floor(window_width * 0.5);
	int center_y = (int)glm::floor(window_height * 0.5);

	SDL_WarpMouseInWindow(_window, center_x, center_y);
	mouse_position = glm::vec2((float)center_x, (float)center_y);
}

void VulkanEngine::renderGUI()
{
	// Imgui new frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(_window);

	ImGui::NewFrame();

	ImGui::Begin("Debug window");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	for (auto& light : _lights)
	{
		if (ImGui::TreeNode(&light, "Light")) {
			if (ImGui::Button("Select"))
				gizmoEntity = light;
			ImGui::SliderFloat3("Position", &((glm::vec3)light->m_matrix[3])[0], -200, 200);
			ImGui::ColorEdit3("Color", &light->color.x);
			ImGui::SliderFloat("Intensity", &light->intensity, 0, 1000);
			ImGui::SliderFloat("Max Distance", &light->maxDistance, 0, 500);
			ImGui::TreePop();
		}
	}
	for (auto& entity : _renderables)
	{
		if (ImGui::TreeNode(&entity, "Entity")) {
			if (ImGui::Button("Select"))
				gizmoEntity = entity;
			ImGui::SliderFloat3("Position", &((glm::vec3)entity->m_matrix[3])[0], -200, 200);
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (!gizmoEntity)
		return;

	glm::mat4& matrix = gizmoEntity->m_matrix;

	ImGuizmo::BeginFrame();

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, &matrix[0][0]);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	glm::vec3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.f / 900.f, 0.1f, 200.0f);
	ImGuizmo::Manipulate(&_camera->getView()[0][0], &projection[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &matrix[0][0], NULL, useSnap ? &snap.x : NULL);

	ImGui::EndFrame();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports	= &_viewport;
	viewportState.scissorCount	= 1;
	viewportState.pScissors		= &_scissor;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount				= _shaderStages.size();
	pipelineInfo.pStages				= _shaderStages.data();
	pipelineInfo.pVertexInputState		= &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &_inputAssembly;
	pipelineInfo.pViewportState			= &viewportState;
	pipelineInfo.pRasterizationState	= &_rasterizer;
	pipelineInfo.pMultisampleState		= &_multisampling;
	pipelineInfo.pColorBlendState		= &_colorBlendStateInfo;
	pipelineInfo.layout					= _pipelineLayout;
	pipelineInfo.renderPass				= pass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState =	&_depthStencil;

	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cout << "Failed to create pipeline!" << std::endl;
		return VK_NULL_HANDLE;
	}

	return newPipeline;

}

VkPipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(
	bool bDepthTest,
	bool bDepthWrite,
	VkCompareOp compareOp)
{
	VkPipelineDepthStencilStateCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.pNext = nullptr;

	info.depthTestEnable		= bDepthTest ? VK_TRUE : VK_FALSE;
	info.depthWriteEnable		= bDepthWrite ? VK_TRUE : VK_FALSE;
	info.depthCompareOp			= bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
	info.depthBoundsTestEnable	= VK_FALSE;
	info.minDepthBounds			= 0.0f;
	info.maxDepthBounds			= 1.0f;
	info.stencilTestEnable		= VK_FALSE;

	return info;
}
