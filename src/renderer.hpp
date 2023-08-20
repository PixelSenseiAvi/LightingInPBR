#pragma once

#include <glm/mat4x4.hpp>

// Forward declaration of GLFW's window structure.
struct GLFWwindow;

// Settings related to camera position and view.
struct CameraSettings
{
    float pitch = 0.0f;     // Up-down angle.
    float yaw = 0.0f;       // Left-right angle.
    float distance;         // Distance from the target.
    float fov;              // Field of view.
};

// Settings for the scene including lights.
struct SceneSettings
{
    float pitch = 0.0f;    // Scene's pitch orientation.
    float yaw = 0.0f;      // Scene's yaw orientation.

    // Represents a light source in the scene.
    struct Light 
    {
        glm::vec3 direction;  // Direction of the light.
        glm::vec3 radiance;   // Intensity and color of the light.
        bool enabled = false; // Flag to check if light is on.
    };

    static const int MaxLights = 3; // Maximum number of lights.
    Light lights[MaxLights];
};

// Interface defining the core methods a renderer should implement.
class RendererInterface
{
public:
    virtual ~RendererInterface() = default;

    // Initializes the renderer with given parameters.
    virtual GLFWwindow* initialize(int width, int height, int maxSamples) = 0;

    // Shuts down the renderer.
    virtual void shutdown() = 0;

    // Sets up necessary data or parameters for the renderer.
    virtual void setup() = 0;

    // Renders the scene using given view and scene settings.
    virtual void render(GLFWwindow* window, const CameraSettings& view, const SceneSettings& scene) = 0;
};