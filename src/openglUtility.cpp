#include <stdexcept>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <GLFW/glfw3.h>

#include "mesh.hpp"
#include "image.hpp"
#include "utils.hpp"

#include "openglUtility.hpp"

namespace RendererDetails
{

	/**
	 * @brief Contains transformation related matrices for rendering.
	 */
	struct TransformUB
	{
		glm::mat4 viewProjectionMatrix;
		glm::mat4 skyProjectionMatrix;
		glm::mat4 sceneRotationMatrix;
	};

	/**
	 * @brief Contains shading and light-related information.
	 */
	struct ShadingUB
	{
		struct Light
		{
			glm::vec4 direction;
			glm::vec4 radiance;
		} lights[SceneSettings::MaxLights]; // Assuming SceneSettings::MaxLights is a defined constant

		glm::vec4 eyePosition;
	};

	void SetGLFWWindowHints()
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

#if _DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

		glfwWindowHint(GLFW_DEPTH_BITS, 0);
		glfwWindowHint(GLFW_STENCIL_BITS, 0);
		glfwWindowHint(GLFW_SAMPLES, 0);
	}

	void InitializeOpenGLExtensions()
	{
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			throw std::runtime_error("Failed to initialize OpenGL extensions loader");
		}
	}

	void SetGlobalOpenGLState()
	{
		glEnable(GL_CULL_FACE);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		glFrontFace(GL_CCW);
	}

}

GLFWwindow *Renderer::initialize(int width, int height, int maxSamples)
{
	RendererDetails::SetGLFWWindowHints();

	GLFWwindow *window = glfwCreateWindow(width, height, "Physically Based Rendering - IBL", nullptr, nullptr);
	if (!window)
	{
		throw std::runtime_error("Failed to create OpenGL context");
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(-1);
	RendererDetails::InitializeOpenGLExtensions();

	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_capabilities.maxAnisotropy);

#if _DEBUG
	glDebugMessageCallback(Renderer::logMessage, nullptr);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

	GLint maxSupportedSamples;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSupportedSamples);

	const int samples = glm::min(maxSamples, maxSupportedSamples);
	m_framebuffer = createFrameBuffer(width, height, samples, GL_RGBA16F, GL_DEPTH24_STENCIL8);
	if (samples > 0)
	{
		m_resolveFramebuffer = createFrameBuffer(width, height, 0, GL_RGBA16F, GL_NONE);
	}
	else
	{
		m_resolveFramebuffer = m_framebuffer;
	}

	std::printf("IBL - OpenGL [%s]\n", glGetString(GL_RENDERER));

	return window;
}

void Renderer::shutdown()
{
	if (m_framebuffer.id != m_resolveFramebuffer.id)
	{
		deleteFrameBuffer(m_resolveFramebuffer);
	}

	deleteFrameBuffer(m_framebuffer);
	glDeleteVertexArrays(1, &m_emptyVAO);
	glDeleteBuffers(1, &m_transformUB);
	glDeleteBuffers(1, &m_shadingUB);

	// Assuming these are actual cleanup methods you have defined
	deleteMeshBuffer(m_skybox);
	deleteMeshBuffer(m_pbrModel);

	glDeleteProgram(m_tonemapProgram);
	glDeleteProgram(m_skyboxProgram);
	glDeleteProgram(m_pbrProgram);

	deleteTexture(m_envTexture);
	deleteTexture(m_irmapTexture);
	deleteTexture(m_spBRDF_LUT);
	deleteTexture(m_albedoTexture);
	deleteTexture(m_normalTexture);
	deleteTexture(m_metalnessTexture);
	deleteTexture(m_roughnessTexture);
}

void Renderer::setup()
{
	// Parameters
	static constexpr int kEnvMapSize = 1024;
	static constexpr int kIrradianceMapSize = 32;
	static constexpr int kBRDF_LUT_Size = 256;

	// Set global OpenGL state.
	RendererDetails::SetGlobalOpenGLState();

	// Create empty VAO for rendering full screen triangle.
	glCreateVertexArrays(1, &m_emptyVAO);

	// Create uniform buffers.
	m_transformUB = createUniformBuffer<RendererDetails::TransformUB>();
	m_shadingUB = createUniformBuffer<RendererDetails::ShadingUB>();

	// Load assets & compile/link rendering programs.
	m_tonemapProgram = linkProgram({compileShader("shaders/tonemap.vs", GL_VERTEX_SHADER),
									compileShader("shaders/tonemap.fs", GL_FRAGMENT_SHADER)});

	m_skybox = createMeshBuffer(Mesh::fromFile("data/meshes/skybox.obj"));
	m_skyboxProgram = linkProgram({compileShader("shaders/skybox.vs", GL_VERTEX_SHADER),
								   compileShader("shaders/skybox.fs", GL_FRAGMENT_SHADER)});

	m_pbrModel = createMeshBuffer(Mesh::fromFile("data/meshes/Flaski.fbx"));
	m_pbrProgram = linkProgram({compileShader("shaders/pbr.vs", GL_VERTEX_SHADER),
								compileShader("shaders/pbr.fs", GL_FRAGMENT_SHADER)});

	m_albedoTexture = createTexture(Image::fromFile("data/textures/flaski/Flaski_DefaultMaterial_BaseColor.png", 3), GL_RGB, GL_SRGB8);
	m_normalTexture = createTexture(Image::fromFile("data/textures/flaski/Flaski_DefaultMaterial_Normal.png", 3), GL_RGB, GL_RGB8);
	m_metalnessTexture = createTexture(Image::fromFile("data/textures/flaski/Flaski_DefaultMaterial_Metallic.png", 1), GL_RED, GL_R8);
	m_roughnessTexture = createTexture(Image::fromFile("data/textures/flaski/Flaski_DefaultMaterial_Roughness.png", 1), GL_RED, GL_R8);

	// Unfiltered environment cube map (temporary).
	Texture envTextureUnfiltered = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);

	// Load & convert equirectangular environment map to a cubemap texture.
	{
		GLuint equirectToCubeProgram = linkProgram({compileShader("shaders/equirect2cube.cs", GL_COMPUTE_SHADER)});

		Texture envTextureEquirect = createTexture(Image::fromFile("data/environment.hdr", 3), GL_RGB, GL_RGB16F, 1);

		glUseProgram(equirectToCubeProgram);
		glBindTextureUnit(0, envTextureEquirect.id);
		glBindImageTexture(0, envTextureUnfiltered.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glDispatchCompute(envTextureUnfiltered.width / 32, envTextureUnfiltered.height / 32, 6);

		glDeleteTextures(1, &envTextureEquirect.id);
		glDeleteProgram(equirectToCubeProgram);
	}

	glGenerateTextureMipmap(envTextureUnfiltered.id);

	// Compute pre-filtered specular environment map.
	{
		GLuint spmapProgram = linkProgram({compileShader("shaders/spmap.cs", GL_COMPUTE_SHADER)});

		m_envTexture = createTexture(GL_TEXTURE_CUBE_MAP, kEnvMapSize, kEnvMapSize, GL_RGBA16F);

		// Copy 0th mipmap level into destination environment map.
		glCopyImageSubData(envTextureUnfiltered.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
						   m_envTexture.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0,
						   m_envTexture.width, m_envTexture.height, 6);

		glUseProgram(spmapProgram);
		glBindTextureUnit(0, envTextureUnfiltered.id);

		// Pre-filter rest of the mip chain.
		const float deltaRoughness = 1.0f / glm::max(float(m_envTexture.levels - 1), 1.0f);
		for (int level = 1, size = kEnvMapSize / 2; level <= m_envTexture.levels; ++level, size /= 2)
		{
			const GLuint numGroups = glm::max(1, size / 32);
			glBindImageTexture(0, m_envTexture.id, level, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glProgramUniform1f(spmapProgram, 0, level * deltaRoughness);
			glDispatchCompute(numGroups, numGroups, 6);
		}
		glDeleteProgram(spmapProgram);
	}

	glDeleteTextures(1, &envTextureUnfiltered.id);

	// Compute diffuse irradiance cubemap.
	{
		GLuint irmapProgram = linkProgram({compileShader("shaders/irmap.cs", GL_COMPUTE_SHADER)});

		m_irmapTexture = createTexture(GL_TEXTURE_CUBE_MAP, kIrradianceMapSize, kIrradianceMapSize, GL_RGBA16F, 1);

		glUseProgram(irmapProgram);
		glBindTextureUnit(0, m_envTexture.id);
		glBindImageTexture(0, m_irmapTexture.id, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glDispatchCompute(m_irmapTexture.width / 32, m_irmapTexture.height / 32, 6);
		glDeleteProgram(irmapProgram);
	}

	// Compute Cook-Torrance BRDF 2D LUT for split-sum approximation.
	{
		GLuint spBRDFProgram = linkProgram({compileShader("shaders/spbrdf.cs", GL_COMPUTE_SHADER)});

		m_spBRDF_LUT = createTexture(GL_TEXTURE_2D, kBRDF_LUT_Size, kBRDF_LUT_Size, GL_RG16F, 1);
		glTextureParameteri(m_spBRDF_LUT.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_spBRDF_LUT.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glUseProgram(spBRDFProgram);
		glBindImageTexture(0, m_spBRDF_LUT.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16F);
		glDispatchCompute(m_spBRDF_LUT.width / 32, m_spBRDF_LUT.height / 32, 1);
		glDeleteProgram(spBRDFProgram);
	}

	glFinish();
}

void Renderer::render(GLFWwindow *window, const CameraSettings &view, const SceneSettings &scene)
{
	// 1. PREPARATION:

	// Calculate projection, view, and scene rotation matrices using GLM library functions.
	const glm::mat4 projectionMatrix = glm::perspectiveFov(view.fov, float(m_framebuffer.width), float(m_framebuffer.height), 1.0f, 1000.0f);
	const glm::mat4 viewRotationMatrix = glm::eulerAngleXY(glm::radians(view.pitch), glm::radians(view.yaw));
	const glm::mat4 sceneRotationMatrix = glm::eulerAngleXY(glm::radians(scene.pitch), glm::radians(scene.yaw));
	const glm::mat4 viewMatrix = glm::translate(glm::mat4{1.0f}, {0.0f, 0.0f, -view.distance}) * viewRotationMatrix;
	const glm::vec3 eyePosition = glm::inverse(viewMatrix)[3];

	// 2. UPDATE UNIFORM BUFFERS:

	// Update transformation-related data for shaders.
	{
		RendererDetails::TransformUB transformUniforms;
		transformUniforms.viewProjectionMatrix = projectionMatrix * viewMatrix;
		transformUniforms.skyProjectionMatrix = projectionMatrix * viewRotationMatrix;
		transformUniforms.sceneRotationMatrix = sceneRotationMatrix;
		glNamedBufferSubData(m_transformUB, 0, sizeof(RendererDetails::TransformUB), &transformUniforms);
	}

	// Update shading (lighting and other visual) data for shaders.
	{
		RendererDetails::ShadingUB shadingUniforms;
		shadingUniforms.eyePosition = glm::vec4(eyePosition, 0.0f);
		for (int i = 0; i < SceneSettings::MaxLights; ++i)
		{
			const SceneSettings::Light &light = scene.lights[i];
			shadingUniforms.lights[i].direction = glm::vec4{light.direction, 0.0f};
			shadingUniforms.lights[i].radiance = light.enabled ? glm::vec4{light.radiance, 0.0f} : glm::vec4{};
		}
		glNamedBufferSubData(m_shadingUB, 0, sizeof(RendererDetails::ShadingUB), &shadingUniforms);
	}

	// 3. FRAMEBUFFER SETUP:

	// Set the framebuffer for rendering.
	glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer.id);
	// Clear the depth buffer (not the color buffer since the skybox will be drawn over everything).
	glClear(GL_DEPTH_BUFFER_BIT);

	// Bind the uniform buffers.
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_transformUB);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_shadingUB);

	// 4. RENDERING:

	// Draw the skybox (cube environment map background).
	glDisable(GL_DEPTH_TEST); // Disable depth testing for skybox to ensure it's always rendered behind everything.
	glUseProgram(m_skyboxProgram);
	glBindTextureUnit(0, m_envTexture.id);
	glBindVertexArray(m_skybox.vao);
	glDrawElements(GL_TRIANGLES, m_skybox.numElements, GL_UNSIGNED_INT, 0);

	// Draw the Physically-Based Rendering (PBR) model.
	glEnable(GL_DEPTH_TEST);
	glUseProgram(m_pbrProgram);
	// Bind the various textures (albedo, normal, metalness, roughness, environment map, irradiance map, split-sum BRDF lookup table).
	glBindTextureUnit(0, m_albedoTexture.id);
	glBindTextureUnit(1, m_normalTexture.id);
	glBindTextureUnit(2, m_metalnessTexture.id);
	glBindTextureUnit(3, m_roughnessTexture.id);
	glBindTextureUnit(4, m_envTexture.id);
	glBindTextureUnit(5, m_irmapTexture.id);
	glBindTextureUnit(6, m_spBRDF_LUT.id);
	// Bind vertex array and draw.
	glBindVertexArray(m_pbrModel.vao);
	glDrawElements(GL_TRIANGLES, m_pbrModel.numElements, GL_UNSIGNED_INT, 0);

	// 5. POST-PROCESSING:

	// Handle multisampled framebuffer by resolving it.
	resolveFramebuffer(m_framebuffer, m_resolveFramebuffer);

	// Render a full screen triangle for post-processing operations such as tone mapping.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(m_tonemapProgram);
	glBindTextureUnit(0, m_resolveFramebuffer.colorTarget);
	glBindVertexArray(m_emptyVAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// 6. PRESENT TO SCREEN:

	// Swap the window buffers to display the rendered frame.
	glfwSwapBuffers(window);
}

GLuint Renderer::compileShader(const std::string &filename, GLenum type)
{
	const std::string src = FileUtility::readText(filename);
	if (src.empty())
	{
		throw std::runtime_error("Cannot read shader source file: " + filename);
	}
	const GLchar *srcBufferPtr = src.c_str();

	std::printf("Compiling GLSL shader: %s\n", filename.c_str());

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &srcBufferPtr, nullptr);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE)
	{
		GLsizei infoLogSize;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogSize);
		std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
		glGetShaderInfoLog(shader, infoLogSize, nullptr, infoLog.get());
		throw std::runtime_error(std::string("Shader compilation failed: ") + filename + "\n" + infoLog.get());
	}
	return shader;
}

GLuint Renderer::linkProgram(std::initializer_list<GLuint> shaders)
{
	GLuint program = glCreateProgram();

	for (GLuint shader : shaders)
	{
		glAttachShader(program, shader);
	}
	glLinkProgram(program);
	for (GLuint shader : shaders)
	{
		glDetachShader(program, shader);
		glDeleteShader(shader);
	}

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_TRUE)
	{
		glValidateProgram(program);
		glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
	}
	if (status != GL_TRUE)
	{
		GLsizei infoLogSize;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogSize);
		std::unique_ptr<GLchar[]> infoLog(new GLchar[infoLogSize]);
		glGetProgramInfoLog(program, infoLogSize, nullptr, infoLog.get());
		throw std::runtime_error(std::string("Program link failed\n") + infoLog.get());
	}
	return program;
}

// Private helper function to set up common texture parameters
inline void Renderer::setupTextureParameters(GLuint textureId, int levels) const
{
	glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameterf(textureId, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_capabilities.maxAnisotropy);
}

Texture Renderer::createTexture(GLenum target, int width, int height, GLenum internalformat, int levels) const
{
	Texture texture{
		.width = width,
		.height = height,
		.levels = (levels > 0) ? levels : Utility::numMipmapLevels(width, height)};

	glCreateTextures(target, 1, &texture.id);
	glTextureStorage2D(texture.id, texture.levels, internalformat, width, height);

	setupTextureParameters(texture.id, texture.levels);

	return texture;
}

Texture Renderer::createTexture(const std::shared_ptr<class Image> &image, GLenum format, GLenum internalformat, int levels) const
{
	Texture texture = createTexture(GL_TEXTURE_2D, image->width(), image->height(), internalformat, levels);

	auto uploadTextureData = [&](GLenum type, auto pixels)
	{
		glTextureSubImage2D(texture.id, 0, 0, 0, texture.width, texture.height, format, type, pixels);
	};

	if (image->isHDR())
	{
		uploadTextureData(GL_FLOAT, image->pixels<float>());
	}
	else
	{
		uploadTextureData(GL_UNSIGNED_BYTE, image->pixels<unsigned char>());
	}

	if (texture.levels > 1)
	{
		glGenerateTextureMipmap(texture.id);
	}

	return texture;
}

void Renderer::deleteTexture(Texture &texture)
{
	glDeleteTextures(1, &texture.id);
	std::memset(&texture, 0, sizeof(Texture));
}

// Private helper functions to abstract the repeated framebuffer operations
inline void Renderer::attachMultisampleRenderBuffer(GLuint framebuffer, GLuint &rbo, GLenum attachment, GLenum format, int samples, int width, int height)
{
	glCreateRenderbuffers(1, &rbo);
	glNamedRenderbufferStorageMultisample(rbo, samples, format, width, height);
	glNamedFramebufferRenderbuffer(framebuffer, attachment, GL_RENDERBUFFER, rbo);
}

inline void Renderer::attachTextureBuffer(GLuint framebuffer, GLuint &texture, GLenum attachment, GLenum format, int width, int height)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glTextureStorage2D(texture, 1, format, width, height);
	glNamedFramebufferTexture(framebuffer, attachment, texture, 0);
}

FrameBuffer Renderer::createFrameBuffer(int width, int height, int samples, GLenum colorFormat, GLenum depthstencilFormat)
{
	FrameBuffer fb{
		.width = width,
		.height = height,
		.samples = samples};

	glCreateFramebuffers(1, &fb.id);

	if (colorFormat != GL_NONE)
	{
		if (samples > 0)
		{
			attachMultisampleRenderBuffer(fb.id, fb.colorTarget, GL_COLOR_ATTACHMENT0, colorFormat, samples, width, height);
		}
		else
		{
			attachTextureBuffer(fb.id, fb.colorTarget, GL_COLOR_ATTACHMENT0, colorFormat, width, height);
		}
	}

	if (depthstencilFormat != GL_NONE)
	{
		glCreateRenderbuffers(1, &fb.depthStencilTarget);
		if (samples > 0)
		{
			attachMultisampleRenderBuffer(fb.id, fb.depthStencilTarget, GL_DEPTH_STENCIL_ATTACHMENT, depthstencilFormat, samples, width, height);
		}
		else
		{
			attachTextureBuffer(fb.id, fb.depthStencilTarget, GL_DEPTH_STENCIL_ATTACHMENT, depthstencilFormat, width, height);
		}
	}

	GLenum status = glCheckNamedFramebufferStatus(fb.id, GL_DRAW_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("Framebuffer completeness check failed: " + std::to_string(status));
	}
	return fb;
}

inline std::vector<GLenum> validAttachments(const FrameBuffer &fb)
{
	std::vector<GLenum> attachments;
	if (fb.colorTarget)
		attachments.push_back(GL_COLOR_ATTACHMENT0);
	if (fb.depthStencilTarget)
		attachments.push_back(GL_DEPTH_STENCIL_ATTACHMENT);
	return attachments;
}

void Renderer::resolveFramebuffer(const FrameBuffer &srcfb, const FrameBuffer &dstfb)
{
	if (srcfb.id == dstfb.id)
		return;

	auto attachments = validAttachments(srcfb);
	assert(!attachments.empty());

	glBlitNamedFramebuffer(srcfb.id, dstfb.id, 0, 0, srcfb.width, srcfb.height, 0, 0, dstfb.width, dstfb.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glInvalidateNamedFramebufferData(srcfb.id, static_cast<GLsizei>(attachments.size()), attachments.data());
}

inline void deleteGLObject(GLuint &id, void (*deletionFunction)(GLsizei, const GLuint *))
{
	if (id)
	{
		deletionFunction(1, &id);
		id = 0;
	}
}

void Renderer::deleteFrameBuffer(FrameBuffer &fb)
{
	deleteGLObject(fb.id, glDeleteFramebuffers);
	if (fb.samples == 0)
	{
		deleteGLObject(fb.colorTarget, glDeleteTextures);
	}
	else
	{
		deleteGLObject(fb.colorTarget, glDeleteRenderbuffers);
	}
	deleteGLObject(fb.depthStencilTarget, glDeleteRenderbuffers);
	std::memset(&fb, 0, sizeof(FrameBuffer));
}

inline void createGLBuffer(GLuint &buffer, GLsizeiptr size, const void *data)
{
	glCreateBuffers(1, &buffer);
	glNamedBufferStorage(buffer, size, data, 0);
}

MeshBuffer Renderer::createMeshBuffer(const std::shared_ptr<class Mesh> &mesh)
{
	MeshBuffer buffer;
	buffer.numElements = static_cast<GLuint>(mesh->faces().size()) * 3;

	createGLBuffer(buffer.vbo, mesh->vertices().size() * sizeof(Mesh::Vertex), mesh->vertices().data());
	createGLBuffer(buffer.ibo, mesh->faces().size() * sizeof(Mesh::Face), mesh->faces().data());

	glCreateVertexArrays(1, &buffer.vao);
	glVertexArrayElementBuffer(buffer.vao, buffer.ibo);

	for (int i = 0; i < Mesh::NumAttributes; ++i)
	{
		glVertexArrayVertexBuffer(buffer.vao, i, buffer.vbo, i * sizeof(glm::vec3), sizeof(Mesh::Vertex));
		glEnableVertexArrayAttrib(buffer.vao, i);
		glVertexArrayAttribFormat(buffer.vao, i, i == (Mesh::NumAttributes - 1) ? 2 : 3, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribBinding(buffer.vao, i, i);
	}
	return buffer;
}

void Renderer::deleteMeshBuffer(MeshBuffer &buffer)
{
	deleteGLObject(buffer.vao, glDeleteVertexArrays);
	deleteGLObject(buffer.vbo, glDeleteBuffers);
	deleteGLObject(buffer.ibo, glDeleteBuffers);
	std::memset(&buffer, 0, sizeof(MeshBuffer));
}

GLuint Renderer::createUniformBuffer(const void *data, size_t size)
{
	GLuint ubo;
	glCreateBuffers(1, &ubo);
	glNamedBufferStorage(ubo, size, data, GL_DYNAMIC_STORAGE_BIT);
	return ubo;
}

#if _DEBUG
void Renderer::logMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	// Log message if the severity is not just a notification.
	if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		std::fprintf(stderr, "GL: %s\n", message);
	}
}
#endif
