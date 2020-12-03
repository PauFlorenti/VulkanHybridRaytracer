#pragma once
#include <vk_types.h>
#include <glm/glm/glm.hpp>

class Entity;

struct FrameData
{
	VkSemaphore _renderSemaphore;
	VkSemaphore _presentSemaphore;
	VkFence		_renderFence;

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

constexpr unsigned int FRAME_OVERLAP = 2;

class Renderer {

public:

	Renderer();

	VkDevice*		device;
	VkSwapchainKHR* swapchain;
	int*			frameNumber;
	Entity*			gizmoEntity;

	FrameData _frames[FRAME_OVERLAP];

	VkCommandPool		_commandPool;
	VkDescriptorPool	_descriptorPool;

	pushConstants		_constants;

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

	void render();

	void render_gui();

	void init();

	void init_commands();

	void init_render_pass();

	void init_offscreen_render_pass();

	FrameData& get_current_frame();
private:

	void init_framebuffers();

	void init_offscreen_framebuffers();

	void init_sync_structures();

	void init_descriptors();

	void setup_descriptors();

	void init_deferred_descriptors();

	void init_deferred_pipelines();

	void build_previous_command_buffer();
	
	void build_deferred_command_buffer();
};