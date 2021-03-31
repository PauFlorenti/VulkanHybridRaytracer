
#include "scene.h"

unsigned int Scene::get_drawable_nodes_size()
{
	unsigned int count = 0;
	for (Object* o : _entities)
	{
		for (Node* root : o->prefab->_root) {
			count += root->get_number_nodes();
		}
	}

	return count;
}

bool equals(int* a) { return *a > 1; }

void Scene::create_scene()
{
	// Create camera
	_camera = new Camera(glm::vec3(0, 5, 10));

	// Create lights
	// -------------
	Light* light		= new Light();
	light->m_matrix		= glm::translate(glm::mat4(1), glm::vec3(10, 12, -5));
	light->intensity	= 500.0f;
	light->radius = 0.1f;

	Light* light2		= new Light();
	light2->m_matrix	= glm::translate(glm::mat4(1), glm::vec3(-10, 15, -5));
	light2->intensity	= 100.0f;

	_lights.push_back(light);
	//_lights.push_back(light2);

	// Create own Materials
	// --------------------
	Material* m_mirror = new Material();
	m_mirror->shadingModel = 3;
	Material* m_glass = new Material();
	m_glass->diffuseColor = glm::vec4{ 0.7f, 0.7f, 1.0f, 1 };
	m_glass->shadingModel = 4;
	m_glass->ior = 1.125;// 1.2f;
	Material* m_gold = new Material();
	m_gold->diffuseColor = glm::vec4{ 1.0, 0.71, 0.29, 1.0 };
	m_gold->metallicFactor = 0.5f;
	m_gold->roughnessFactor = 0.1f;
	Material* m_red = new Material();
	m_red->diffuseColor = glm::vec4{ 0.9, 0.1, 0.05, 1.0 };
	m_red->metallicFactor = 0.0;
	m_red->roughnessFactor = 0.0;
	Material* m_floor = new Material();
	m_floor->metallicFactor = 0.1f;

	// Create prefabs
	// --------------
	//Prefab* p_sphere	= Prefab::GET("sphere.obj");
	Prefab* p_quad		= Prefab::GET("quad", Mesh::get_quad());
	p_quad->_root[0]->addMaterial(m_floor);
	Prefab* p_mirror	= Prefab::GET("cube", Mesh::get_cube());
	p_mirror->_root[0]->addMaterial(m_mirror);
	Prefab* p_sphere_mirror = Prefab::GET("sphere.obj");
	p_sphere_mirror->_root[0]->addMaterial(m_mirror);
	Prefab* p_glass_sphere = Prefab::GET("sphere.obj");
	p_glass_sphere->_root[0]->addMaterial(m_glass);
	Prefab* p_gold_sphere = Prefab::GET("sphere.obj");
	p_gold_sphere->_root[0]->addMaterial(m_gold);
	Prefab* p_red_sphere = Prefab::GET("sphere.obj");
	p_red_sphere->_root[0]->addMaterial(m_red);
	//Prefab* p_cornell	= Prefab::GET("cornellBox.gltf");
	Prefab* p_helmet	= Prefab::GET("DamagedHelmet.gltf");
	//Prefab* p_lantern	= Prefab::GET("Lantern.gltf");
	//Prefab* p_car		= Prefab::GET("scene.gltf");
	Prefab* p_lucy		= Prefab::GET("lucy", Mesh::GET("lucy.obj"));
	p_lucy->_root[0]->addMaterial(m_glass);
	//Prefab* p_sponza = Prefab::GET("Sponza.gltf");

	// Create entities
	// ---------------
	Object* sphere = new Object();
	sphere->prefab = p_red_sphere;
	sphere->m_matrix = glm::translate(glm::mat4(1), glm::vec3(5, 5, -5));
	sphere->material = Material::_materials[p_red_sphere->_root[0]->_primitives[0]->materialID];

	//Object* sphere2 = new Object();
	//sphere2->prefab = p_red_sphere;
	//sphere2->m_matrix = glm::translate(glm::mat4(1), glm::vec3(-5, 5, -5));
	
	Object* floor = new Object();
	floor->prefab = p_quad;
	floor->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5)) *
		glm::rotate(glm::mat4(1), glm::radians(-90.0f), glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(1), glm::vec3(15));
	floor->material = Material::_materials[p_quad->_root[0]->_primitives[0]->materialID];
	
	//Object* mirror = new Object();
	//mirror->prefab = p_mirror;
	//mirror->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 4, -10)) * 
	//	glm::scale(glm::mat4(1), glm::vec3(8, 4, 1));
	
	Object* cube = new Object();
	cube->prefab = p_mirror;
	cube->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 0, -5));

	Object* helmet = new Object();
	helmet->prefab = p_helmet;
	helmet->m_matrix = glm::translate(glm::mat4(1), glm::vec3(0, 5, -5));
	helmet->material = Material::_materials[p_helmet->_root[0]->_primitives[0]->materialID];

	Object* lucy = new Object();
	lucy->prefab = p_lucy;
	lucy->m_matrix = glm::scale(glm::mat4(1), glm::vec3(0.01));
	lucy->material = Material::_materials[p_lucy->_root[0]->_primitives[0]->materialID];
	
	//Object* sponza = new Object();
	//sponza->prefab = p_sponza;
	
	_entities.push_back(floor);
	_entities.push_back(sphere);
	//_entities.push_back(sphere2);
	//_entities.push_back(duck);
	//_entities.push_back(cornell);
	//_entities.push_back(mirror);
	_entities.push_back(helmet);
	//_entities.push_back(car);
	_entities.push_back(lucy);
	//_entities.push_back(cube);
	//_entities.push_back(lantern);
	//_entities.push_back(sponza);
}