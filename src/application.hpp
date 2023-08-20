#pragma once

#include <memory>
#include "renderer.hpp"

class Application
{
public:
    Application();
    ~Application();

    // Starts the application loop using the provided renderer.
    void run(const std::unique_ptr<RendererInterface>& renderer);

private:
    // GLFW callbacks for handling mouse and keyboard input.
    static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void mouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    // Member variables for storing the state of the application.
    GLFWwindow* m_pWindow;             // GLFW window instance.
    double m_lastCursorX;              // Last x-position of the cursor.
    double m_lastCursorY;              // Last y-position of the cursor.
    CameraSettings m_cameraSettings;  // Camera or viewer's settings.
    SceneSettings m_sceneSettings;    // Scene configuration and light settings.

    // Enum for managing the current mode of input.
    enum class InputMode
    {
        None,             // No specific mode.
        RotatingView,     // User is rotating the view/camera.
        RotatingScene,    // User is rotating the entire scene.
    };

    InputMode m_currentMode; // Current input mode.
};

