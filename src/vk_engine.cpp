
#include "vk_engine.h"

#include <VkBootstrap.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>
#include "vk_textures.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <fstream>
#include <array>

VulkanEngine* VulkanEngine::engine = nullptr;

VulkanEngine::VulkanEngine()
{
	engine = this;
}

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

	init_upload_commands();

	renderer = new Renderer();

	// Load images and meshes for the engine
	load_images();

	load_meshes();

	init_scene();

	renderer->init();

	init_imgui();

	mouse_locked = false;
	SDL_ShowCursor(!mouse_locked);

	// Everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized) {

		for (auto& frames : renderer->_frames)
			vkWaitForFences(_device, 1, &frames._renderFence, VK_TRUE, 1000000000);

		_mainDeletionQueue.flush();

		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyDevice(_device, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
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
					_camera->processKeyboard(DOWN, dt);
				}
				if (e.key.keysym.sym == SDLK_SPACE) {
					_camera->processKeyboard(UP, dt);
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

		input_update();
		update(dt);
		renderer->render_gui();
		renderer->render();
		_frameNumber++;
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
	vmaMapMemory(_allocator, renderer->get_current_frame()._lightBuffer._allocation, &lightData);
	uboLight* lightUBO = (uboLight*)lightData;
	for (int i = 0; i < _lights.size(); i++)
	{
		_lights[i]->update();
		Light *l = _lights[i];
		lightUBO[i].color = glm::vec4(l->color.x, l->color.y, l->color.z, l->intensity);
		lightUBO[i].position = glm::vec4(l->position.x, l->position.y, l->position.z, l->maxDistance);
	}
	vmaUnmapMemory(_allocator, renderer->get_current_frame()._lightBuffer._allocation);

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
		//"VK_KHR_acceleration_structure",
		//"VK_KHR_ray_tracing_pipeline",
		//VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		//VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		//VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		//VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
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

void VulkanEngine::init_upload_commands()
{
	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);

	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool));

	VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();

	VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence));

	VulkanEngine::engine->_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
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
	monkey->mesh		= get_mesh("monkey");
	monkey->material	= get_material("offscreen");
	glm::mat4 translation = glm::translate(glm::mat4(1.f), glm::vec3(0, 40, -3));
	glm::mat4 scale = glm::scale(glm::mat4(1.f), glm::vec3(0.8));
	monkey->m_matrix = translation * scale;
	
	Object* empire = new Object();
	empire->mesh		= get_mesh("empire");
	empire->material	= get_material("offscreen");
	empire->id			= get_textureId("empire");

	Object* quad = new Object();
	quad->mesh		= get_mesh("quad");
	quad->material	= get_material("offscreen");
	quad->m_matrix	= glm::translate(glm::mat4(1), glm::vec3(5, 40, -5));

	Object* tri = new Object();
	tri->mesh		= get_mesh("triangle");
	tri->material	= get_material("offscreen");
	tri->m_matrix	= glm::translate(glm::mat4(1), glm::vec3(-5, 40, -5));
	tri->id			= get_textureId("black");

	Object* cube = new Object();
	cube->mesh		= get_mesh("cube");
	cube->material	= get_material("offscreen");
	cube->m_matrix	= glm::translate(glm::mat4(1), glm::vec3(0, 40, -5));

	Object* sphere = new Object();
	sphere->mesh		= get_mesh("sphere");
	sphere->material	= get_material("offscreen");
	sphere->m_matrix	= glm::translate(glm::mat4(1), glm::vec3(0, 50, -5));

	_renderables.push_back(empire);
	_renderables.push_back(monkey);
	_renderables.push_back(tri);
	_renderables.push_back(quad);
	_renderables.push_back(cube);
	_renderables.push_back(sphere);
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

	ImGui_ImplVulkan_Init(&initInfo, renderer->_renderPass);

	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});

	ImGui_ImplVulkan_DestroyFontUploadObjects();

	_mainDeletionQueue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
	});
}

void VulkanEngine::load_meshes()
{
	Mesh _triangleMesh;
	Mesh _monkeyMesh;
	Mesh _lucy;
	Mesh _lostEmpire{};
	Mesh* _quad{};
	Mesh _cube;
	Mesh _sphere;

	_triangleMesh.get_triangle();
	_quad = Mesh::get_quad();
	//_cube.get_cube();

	_monkeyMesh.load_from_obj("data/meshes/monkey_smooth.obj");
	_lostEmpire.load_from_obj("data/meshes/lost_empire.obj");
	//_lucy.load_from_obj("data/meshes/lucy.obj");
	_cube.load_from_obj("data/meshes/default_cube.obj");
	_sphere.load_from_obj("data/meshes/sphere.obj");

	upload_mesh(_triangleMesh);
	upload_mesh(*_quad);
	upload_mesh(_monkeyMesh);
	upload_mesh(_lostEmpire);
	upload_mesh(_cube);
	upload_mesh(_sphere);

	_meshes["triangle"] = _triangleMesh;
	_meshes["quad"]		= *_quad;
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
