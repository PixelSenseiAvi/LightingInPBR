#version 450 core

// Define uniform block for transformation matrices.
layout(std140, binding=0) uniform TransformUniforms
{
	mat4 viewProjMat;          // View-Projection matrix for the main scene.
	mat4 skyboxProjMat;        // Projection matrix dedicated for the skybox.
	mat4 sceneRotationMat;     // Rotation matrix for the scene.
};

// Input vertex attribute: position of a vertex.
layout(location=0) in vec3 inputPosition;
// Output to the fragment shader: local position of a vertex.
layout(location=0) out vec3 outLocalPosition;

void main()
{
	// Pass the input position to the fragment shader.
	outLocalPosition = inputPosition;

	// Compute the vertex position in the screen space.
	// We only apply the skybox projection because the skybox is centered on the camera.
	gl_Position = skyboxProjMat * vec4(inputPosition, 1.0);
}
