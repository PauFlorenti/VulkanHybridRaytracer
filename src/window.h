#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>

#include <glm/glm/glm.hpp>

class Window {
public:
	
	SDL_Window* _handle;

	Window();

	bool init(const char* name, const int w, const int h);
	// handles Window events
	void handleEvent(SDL_Event& e, float dt);

	// windows dimensions getters
	int getWidth();
	int getHeight();
	void setWidth(const int w);
	void setHeight(const int h);

	void input_update();

	// window focus
	bool isMinimized();
	bool isFullscreen();
	void clean();

private:

	// Window dimensions
	int _width;
	int _height;

	glm::vec2 _mouse_position;
	glm::vec2 _mouse_delta;
	bool _mouse_locked;

	// Window focus
	bool _minimized;
	bool _fullscreen;
	bool _resized;

	void center_mouse();
};