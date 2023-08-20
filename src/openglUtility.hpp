#pragma once
#include <string>
#include <glad/glad.h>
#include "renderer.hpp"

/**
 * @brief Represents a buffer for storing mesh data.
 */
struct MeshBuffer
{
    GLuint vbo = 0, ibo = 0, vao = 0;
    GLuint numElements = 0;
};

/**
 * @brief Represents a framebuffer for rendering.
 */
struct FrameBuffer
{
    GLuint id = 0;
    GLuint colorTarget = 0;
    GLuint depthStencilTarget = 0;
    int width = 0, height = 0;
    int samples = 0;
};

/**
 * @brief Represents a texture.
 */
struct Texture
{
    GLuint id = 0;
    int width = 0, height = 0;
    int levels = 0;
};

/**
 * @brief The main renderer class.
 */
class Renderer final : public RendererInterface
{
public:
    GLFWwindow* initialize(int width, int height, int maxSamples) override;
    void loadGLExtensions();
    void determineMultisampling(int maxSamples, int width, int height);
    void shutdown() override;
    void setup() override;
    void render(GLFWwindow* window, const CameraSettings& view, const SceneSettings& scene) override;

//Cleaner functions
private:
    void cleanFramebuffers();
    void cleanVAOsAndBuffers();
    void cleanMeshBuffers();
    void cleanPrograms();
    void cleanTextures();

private:
    // Shader utility functions
    static GLuint compileShader(const std::string& filename, GLenum type);
    static GLuint linkProgram(std::initializer_list<GLuint> shaders);

    void setupTextureParameters(GLuint textureId, int levels) const;

    // Texture utility functions
    Texture createTexture(GLenum target, int width, int height, GLenum internalformat, int levels = 0) const;
    Texture createTexture(const std::shared_ptr<class Image>& image, GLenum format, GLenum internalformat, int levels = 0) const;
    static void deleteTexture(Texture& texture);

    static void attachMultisampleRenderBuffer(GLuint framebuffer, GLuint &rbo, GLenum attachment, GLenum format, int samples, int width, int height);
    static void attachTextureBuffer(GLuint framebuffer, GLuint &texture, GLenum attachment, GLenum format, int width, int height);

    // Framebuffer utility functions
    static FrameBuffer createFrameBuffer(int width, int height, int samples, GLenum colorFormat, GLenum depthstencilFormat);
    static void resolveFramebuffer(const FrameBuffer& srcfb, const FrameBuffer& dstfb);
    static void deleteFrameBuffer(FrameBuffer& fb);

    // MeshBuffer utility functions
    static MeshBuffer createMeshBuffer(const std::shared_ptr<class Mesh>& mesh);
    static void deleteMeshBuffer(MeshBuffer& buffer);

    // Uniform buffer utility functions
    static GLuint createUniformBuffer(const void* data, size_t size);
    template<typename T> 
    GLuint createUniformBuffer(const T* data = nullptr)
    {
        return createUniformBuffer(data, sizeof(T));
    }

    // Debugging utility
#if _DEBUG
    static void logMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
#endif

    // Renderer capabilities
    struct 
    {
        float maxAnisotropy = 1.0f;
    } m_capabilities;

    // Renderer state and assets
    FrameBuffer m_framebuffer, m_resolveFramebuffer;
    MeshBuffer m_skybox, m_pbrModel;
    GLuint m_emptyVAO;
    GLuint m_tonemapProgram, m_skyboxProgram, m_pbrProgram;
    Texture m_envTexture, m_irmapTexture, m_spBRDF_LUT, m_albedoTexture, m_normalTexture, m_metalnessTexture, m_roughnessTexture;
    GLuint m_transformUB, m_shadingUB;
};
