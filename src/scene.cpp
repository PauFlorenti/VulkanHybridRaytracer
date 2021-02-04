
#include "scene.h"

unsigned int Scene::get_drawable_nodes_size()
{
	unsigned int count = 0;
	for (Object* o : _entities)
	{
		count += o->prefab->_root->get_number_nodes();
	}

	return count;
}

bool equals(int* a) { return *a > 1; }

void Scene::create_scene()
{
	// Create camera
	_camera = new Camera(glm::vec3(0, 5, 10));

	// Create lights
	Light* light = new Light();
	light->m_matrix = glm::translate(glm::mat4(1), glm::vec3(5, 15, 0));
	light->intensity = 100.0f;

	_lights.push_back(light);

	// Materials
	Material* m_asphalt = new Material();
	m_asphalt->diffuseTexture = Texture::get_id("asphalt.png");
	Material* m_mirror = new Material();
	m_mirror->shadingModel = 3;
	Material* m_glass = new Material();
	m_glass->shadingModel = 4;
	m_glass->ior = 1.53f;

	// Create prefabs
	Prefab* p_duck		= Prefab::GET("duck.gltf");
	Prefab* p_sphere	= Prefab::GET("sphere.obj");
	Prefab* p_quad		= Prefab::GET("quad", Mesh::get_quad());
	p_quad->_root->addMaterial(m_asphalt);
	Prefab* p_mirror	= Prefab::GET("cube", Mesh::get_cube());
	p_mirror->_root->addMaterial(m_mirror);
	Prefab* p_glass_sphere = Prefab::GET("sphere.obj");
	p_glass_sphere->_root->addMaterial(m_glass);
	Prefab* p_cornell	= Prefab::GET("Avocado.gltf");
	Prefab* p_helmet	= Prefab::GET("DamagedHelmet.gltf");
	Prefab* p_lantern	= Prefab::GET("Lantern.gltf");

	// Create entities

	Object* sphere = new Object();
	sphere->prefab = p_glass_sphere;
	sphere->m_matrix = glm::translate(glm::mat4(1), glm::vec3(5, 5, -5));

	Object* duck = new Object();
	duck->prefab = p_duck;
	duck->m_matrix = glm::translate(glm::mat4(1), glm::vec3(10, 0, -20));

	Object* floor = new Object();
	floor->prefab = p_quad;
	floor->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5)) *
		glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1), glm::vec3(50));

	Object* mirror = new Object();
	mirror->prefab = p_mirror;
	mirror->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 5, -10)) * 
		glm::scale(glm::mat4(1), glm::vec3(5, 5, 1));

	Object* helmet = new Object();
	helmet->prefab = p_lantern;
	helmet->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5)) *
		glm::scale(glm::mat4(1), glm::vec3(0.5));

	Object* cornell = new Object();
	cornell->prefab = p_cornell;
	cornell->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 5, -10)) *
		glm::scale(glm::mat4(1), glm::vec3(50));
	
	_entities.push_back(sphere);
	_entities.push_back(duck);
	_entities.push_back(floor);
	_entities.push_back(mirror);
	_entities.push_back(helmet);

}