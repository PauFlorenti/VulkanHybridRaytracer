#pragma once

#include <vk_types.h>
#include <vector>
#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <vk_mesh.h>

#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>

#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_impl_vulkan.h>

#include "entity.h"

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();
		}
		deletors.clear();
	}
};

struct pushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct Texture {
	AllocatedImage  image;
	VkImageView		imageView;
};

struct GPUCameraData
{
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 viewprojection;
};

struct GPUSceneData {
	glm::vec4 fogColor;
	glm::vec4 fogDistances;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection;
	glm::vec4 sunlightColor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;
};

struct uboLight {
	glm::vec4 position;	// w used for maxDistance
	glm::vec4 color;	// w used for intensity;
};

struct FrameData
{
	VkSemaphore _renderSemaphore, _presentSemaphore;
	VkFence _renderFence;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkDescriptorSet deferredDescriptorSet;
	VkDescriptorSet deferredLightDescriptorSet;
	AllocatedBuffer _lightBuffer;
};

//TODO solve problem when creating the commands. We may want 2 FRAME_OVERLAP but it has to take
//into account the number of swapchain images
constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool _bQuit{ false };
	uint32_t _indexSwapchainImage{ 0 };

	VkExtent2D _windowExtent{ 1700, 900 };

	struct SDL_Window* _window{ nullptr };

	DeletionQueue _mainDeletionQueue;

	// Device stuff
	VkInstance					_instance;
	VkDebugUtilsMessengerEXT	_debug_messenger;
	VkPhysicalDevice			_gpu;
	VkDevice					_device;
	VkSurfaceKHR				_surface;
	VkPhysicalDeviceProperties  _gpuProperties;

	// Swapchain stuff
	VkSwapchainKHR				_swapchain;
	VkFormat					_swapchainImageFormat;
	std::vector<VkImage>		_swapchainImages;
	std::vector<VkImageView>	_swapchainImageViews;

	VkImageView					_depthImageView;
	AllocatedImage				_depthImage;
	VkFormat					_depthFormat;

	// Command stuff
	VkQueue						_graphicsQueue;
	uint32_t					_graphicsQueueFamily;

	// RenderPass stuff (final in deferred)
	VkRenderPass				_renderPass;
	std::vector<VkFramebuffer>	_framebuffers;

	VkDescriptorPool			_descriptorPool;

	VkDescriptorSetLayout		_deferredSetLayout;
	VkDescriptorSetLayout		_deferredLightSetLayout;

	// Offscreen stuff
	VkFramebuffer				_offscreenFramebuffer;
	VkRenderPass				_offscreenRenderPass;
	VkDescriptorSetLayout		_offscreenDescriptorSetLayout;
	VkDescriptorSet				_offscreenDescriptorSet;
	VkDescriptorSetLayout		_objectDescriptorSetLayout;
	VkDescriptorSet				_objectDescriptorSet;
	VkDescriptorSetLayout		_textureDescriptorSetLayout;
	VkDescriptorSet				_textureDescriptorSet;
	VkCommandBuffer				_offscreenComandBuffer;
	VkSampler					_offscreenSampler;
	VkSemaphore					_offscreenSemaphore;
	VkPipelineLayout			_offscreenPipelineLayout;
	VkPipeline					_offscreenPipeline;

	// Deferred
	VkPipelineLayout			_finalPipelineLayout;
	VkPipeline					_finalPipeline;
	// Textures used as attachments from the first pass
	std::vector<Texture>		_deferredTextures;

	pushConstants _constants;

	VkCommandPool _commandPool;
	// Set 0 is a Global set - updated once per frame
	AllocatedBuffer				_cameraBuffer;	// Buffer to hold all information from camera to the shader
	GPUSceneData				_sceneParameters;
	AllocatedBuffer				_sceneBuffer;	// Buffer to hold all information involving the scene

	// Set 1 is a per pass set - updated per pass
	// Buffer to hold all matrices information from the scene
	AllocatedBuffer				_objectBuffer;

	// Allocator
	VmaAllocator _allocator;
	FrameData _frames[FRAME_OVERLAP];

	// Scene stuff
	UploadContext _uploadContext;

	std::vector<Object*> _renderables;
	std::vector<Light*> _lights;
	Entity* gizmoEntity;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh>	  _meshes;
	std::unordered_map<std::string, Texture>  _textures;

	Camera* _camera;
	bool mouse_locked;

	// Main functions
	void init();

	void cleanup();

	void draw_deferred();

	void run();

	void update(const float dt);

	Material* create_material(
		VkPipeline pipeline, 
		VkPipelineLayout layout, 
		const std::string& name);

	Material* get_material(const std::string& name);

	Mesh* get_mesh(const std::string& name);

	Texture* get_texture(const std::string& name);

	int get_textureId(const std::string& name);

	void immediate_submit(std::function<void(VkCommandBuffer)>&& function);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

private:

	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_offscreen_renderpass();
	
	void init_framebuffers();

	void init_offscreen_framebuffers();

	void init_sync_structures();

	void init_deferred_pipelines();

	void load_meshes();

	void load_images();

	void init_scene();

	void init_descriptors();

	void setup_descriptors();

	void init_deferred_descriptors();

	void init_imgui();

	void build_previous_command_buffers();

	void build_deferred_command_buffer();

	void upload_mesh(Mesh& mesh);

	void create_attachment(VkFormat format, VkImageUsageFlagBits usage, Texture* texture);

	void create_vertex_buffer(Mesh& mesh);

	void create_index_buffer(Mesh& mesh);

	// Loads a shader module from a SPIR-V file
	bool load_shader_module(
		const char* filePath, 
		VkShaderModule* outShaderModule);

	FrameData& get_current_frame();

	size_t pad_uniform_buffer_size(size_t originalSize);

	void renderGUI();

	// Input functions
	glm::vec2 mouse_position = { _windowExtent.width * 0.5, _windowExtent.height * 0.5 };
	glm::vec2 mouse_delta;

	void input_update();
	void center_mouse();

	void renderGizmo();
};

class PipelineBuilder{
public:
	std::vector<VkPipelineShaderStageCreateInfo>	_shaderStages;
	VkPipelineVertexInputStateCreateInfo			_vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo			_inputAssembly;
	VkViewport										_viewport;
	VkRect2D										_scissor;
	VkPipelineRasterizationStateCreateInfo			_rasterizer;
	VkPipelineColorBlendStateCreateInfo				_colorBlendStateInfo;
	VkPipelineMultisampleStateCreateInfo			_multisampling;
	VkPipelineLayout								_pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo			_depthStencil;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};