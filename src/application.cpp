#include <stdexcept>
#include <GLFW/glfw3.h>
#include "application.hpp"

namespace 
{
	const int DisplayWidth = 1024;
	const int DisplayHeight = 1024;
	const int AntiAliasingSamples = 16;

	const float DefaultViewDistance = 150.0f;
	const float DefaultViewFOV = 45.0f;
	const float RotationSpeed = 1.0f;
	const float DistanceAdjustSpeed = 4.0f;
}

Application::Application()
	: m_pWindow(nullptr)
	, m_lastCursorX(0.0)
	, m_lastCursorY(0.0)
	, m_currentMode(InputMode::None)
{
	if(!glfwInit()) 
	{
		throw std::runtime_error("Failed to initialize GLFW library");
	}

	m_cameraSettings.distance = DefaultViewDistance;
	m_cameraSettings.fov = DefaultViewFOV;

	// Initialize light directions and radiance for the scene.
	for(int i = 0; i < SceneSettings::MaxLights; i++) 
	{
		m_sceneSettings.lights[i].direction = glm::normalize(glm::vec3{ (i == 1 ? 1.0f : -1.0f), (i == 2 ? -1.0f : 0.0f), 0.0f});
		m_sceneSettings.lights[i].radiance = glm::vec3{1.0f};
	}
}

Application::~Application()
{
	if(m_pWindow) 
	{
		glfwDestroyWindow(m_pWindow);
	}
	glfwTerminate();
}

void Application::run(const std::unique_ptr<RendererInterface>& renderer)
{
	glfwWindowHint(GLFW_RESIZABLE, 0);
	m_pWindow = renderer->initialize(DisplayWidth, DisplayHeight, AntiAliasingSamples);

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetCursorPosCallback(m_pWindow, Application::mousePositionCallback);
	glfwSetMouseButtonCallback(m_pWindow, Application::mouseButtonCallback);
	glfwSetScrollCallback(m_pWindow, Application::mouseScrollCallback);
	glfwSetKeyCallback(m_pWindow, Application::keyCallback);

	renderer->setup();
	while(!glfwWindowShouldClose(m_pWindow)) 
	{
		renderer->render(m_pWindow, m_cameraSettings, m_sceneSettings);
		glfwPollEvents();
	}

	renderer->shutdown();
}

void Application::mousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	Application* self = static_cast<Application*>(glfwGetWindowUserPointer(window));
	if(self->m_currentMode != InputMode::None) 
	{
		const double dx = xpos - self->m_lastCursorX;
		const double dy = ypos - self->m_lastCursorY;

		float adjustment = RotationSpeed * float(dx);
		if(self->m_currentMode == InputMode::RotatingScene) 
		{
			self->m_sceneSettings.yaw += adjustment;
			self->m_sceneSettings.pitch += RotationSpeed * float(dy);
		} 
		else if(self->m_currentMode == InputMode::RotatingView) 
		{
			self->m_cameraSettings.yaw += adjustment;
			self->m_cameraSettings.pitch += RotationSpeed * float(dy);
		}

		self->m_lastCursorX = xpos;
		self->m_lastCursorY = ypos;
	}
}

void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	Application* self = static_cast<Application*>(glfwGetWindowUserPointer(window));

	const InputMode previousMode = self->m_currentMode;

	if(action == GLFW_PRESS && self->m_currentMode == InputMode::None) 
	{
		switch(button) 
		{
			case GLFW_MOUSE_BUTTON_1:
				self->m_currentMode = InputMode::RotatingView;
				break;
			case GLFW_MOUSE_BUTTON_2:
				self->m_currentMode = InputMode::RotatingScene;
				break;
		}
	}
	else if(action == GLFW_RELEASE && (button == GLFW_MOUSE_BUTTON_1 || button == GLFW_MOUSE_BUTTON_2)) 
	{
		self->m_currentMode = InputMode::None;
	}

	// Adjust cursor mode based on input mode.
	if(previousMode != self->m_currentMode) 
	{
		if(self->m_currentMode == InputMode::None) 
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		} 
		else 
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			glfwGetCursorPos(window, &self->m_lastCursorX, &self->m_lastCursorY);
		}
	}
}

void Application::mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	Application* self = static_cast<Application*>(glfwGetWindowUserPointer(window));
	self->m_cameraSettings.distance += DistanceAdjustSpeed * float(-yoffset);
}

void Application::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Application* self = static_cast<Application*>(glfwGetWindowUserPointer(window));

	if(action == GLFW_PRESS) 
	{
		SceneSettings::Light* selectedLight = nullptr;

		switch(key) 
		{
			case GLFW_KEY_F1: selectedLight = &self->m_sceneSettings.lights[0]; break;
			case GLFW_KEY_F2: selectedLight = &self->m_sceneSettings.lights[1]; break;
			case GLFW_KEY_F3: selectedLight = &self->m_sceneSettings.lights[2]; break;
		}

		if(selectedLight) 
		{
			selectedLight->enabled = !selectedLight->enabled;
		}
	}
}
