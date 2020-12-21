#pragma once
#include <vk_types.h>
#include <glm/glm/glm.hpp>

class Entity;

struct FrameData
{
	VkSemaphore		_renderSemaphore;
	VkSemaphore		_presentSemaphore;
	VkFence			_renderFence;

	VkCommandPool	_commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkDescriptorSet deferredDescriptorSet;
	VkDescriptorSet deferredLightDescriptorSet;
	AllocatedBuffer _lightBuffer;
};

struct pushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct Texture {
	AllocatedImage  image;
	VkImageView		imageView;
};

struct AccelerationStructure {
	VkAccelerationStructureKHR	handle;
	uint64_t					deviceAddress = 0;
	VkBuffer buffer;
	VkDeviceMemory memory;
	//AllocatedBuffer				buffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class Renderer {

public:

	Renderer();

	// Auxiliar pointer to engine variables
	VkDevice*		device;
	VkSwapchainKHR* swapchain;
	int*			frameNumber;
	Entity*			gizmoEntity;

	FrameData		_frames[FRAME_OVERLAP];
	pushConstants	_constants;


	// RASTERIZER VARIABLES -----------------------
	VkRenderPass				_forwardRenderPass;
	VkCommandPool				_commandPool;
	VkDescriptorPool			_descriptorPool;

	// Forward stuff
	VkPipelineLayout			_forwardPipelineLayout;
	VkPipeline					_forwardPipeline;

	// Onscreen stuff
	VkRenderPass				_renderPass;
	std::vector<VkFramebuffer>	_framebuffers;
	VkDescriptorSetLayout		_deferredSetLayout;
	VkPipelineLayout			_finalPipelineLayout;
	VkPipeline					_finalPipeline;

	std::vector<Texture>		_deferredTextures;

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

	// RAYTRACING VARIABLES ------------------------
	VkDescriptorPool			_rtDescriptorPool;
	VkDescriptorSetLayout		_rtDescriptorSetLayout;
	VkDescriptorSet				_rtDescriptorSet;
	Texture						_rtImage;
	VkPipeline					_rtPipeline;
	VkPipelineLayout			_rtPipelineLayout;

	AllocatedBuffer				_rtVertexBuffer;
	AllocatedBuffer				_rtIndexBuffer;

	AccelerationStructure		_bottomLevelAS;
	AccelerationStructure		_topLevelAS;

	AllocatedBuffer				raygenShaderBindingTable;
	AllocatedBuffer				missShaderBindingTable;
	AllocatedBuffer				hitShaderBindingTable;

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

	PFN_vkCreateAccelerationStructureKHR				vkCreateAccelerationStructureKHR;
	PFN_vkGetAccelerationStructureBuildSizesKHR			vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkGetAccelerationStructureDeviceAddressKHR		vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkBuildAccelerationStructuresKHR				vkBuildAccelerationStructuresKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR				vkCmdBuildAccelerationStructuresKHR;
	PFN_vkGetRayTracingShaderGroupHandlesKHR			vkGetRayTracingShaderGroupHandlesKHR;
	PFN_vkCreateRayTracingPipelinesKHR					vkCreateRayTracingPipelinesKHR;
	PFN_vkCmdTraceRaysKHR								vkCmdTraceRaysKHR;
	PFN_vkDestroyAccelerationStructureKHR				vkDestroyAccelerationStructureKHR;

	void rasterize();

	void render();

	void raytrace();

	void render_gui();

	void init_commands();

	void init_render_pass();

	void init_forward_render_pass();

	void init_offscreen_render_pass();

	FrameData& get_current_frame();

	void create_storage_image();
private:

	void init_framebuffers();

	void init_offscreen_framebuffers();

	void init_sync_structures();

	void init_descriptors();

	void setup_descriptors();

	void init_deferred_descriptors();

	void init_forward_pipeline();

	void init_deferred_pipelines();

	void build_forward_command_buffer();

	void build_previous_command_buffer();
	
	void build_deferred_command_buffer();

	// VKRay

	void create_bottom_acceleration_structure();

	void create_top_acceleration_structure();

	void create_acceleration_structure(AccelerationStructure& accelerationStructure, 
		VkAccelerationStructureTypeKHR type, 
		VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

	void create_rt_descriptors();

	void create_shader_binding_table();

	void init_raytracing_pipeline();

	void build_raytracing_command_buffers();

};