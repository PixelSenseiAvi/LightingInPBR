#version 450 core

// Input from the vertex shader: local position of a vertex.
layout(location=0) in vec3 vertexLocalPos;

// Output color of the fragment.
layout(location=0) out vec4 fragColor;

// Sampler for the environment/skybox cube texture.
layout(binding=0) uniform samplerCube skyboxTexture;

void main()
{
	// Normalize the local position to get a direction vector for sampling the cube map.
	vec3 directionVec = normalize(vertexLocalPos);

	// Sample the skybox texture using the direction vector.
	// The third argument '0' ensures we're sampling the highest resolution mip level.
	fragColor = textureLod(skyboxTexture, directionVec, 0);
}
