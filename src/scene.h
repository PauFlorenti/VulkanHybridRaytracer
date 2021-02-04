#pragma once

#include "vk_types.h"
#include "camera.h"
#include "entity.h"

class Scene
{
public:
	std::vector<Object*>		_entities;
	std::vector<Light*>			_lights;
	Camera* _camera;

	unsigned int get_drawable_nodes_size();
	void create_scene();
private:
};