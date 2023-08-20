#version 450 core

// Input attributes
layout(location=0) in vec3 vertexPos;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in vec3 vertexTangent;
layout(location=3) in vec3 vertexBitangent;
layout(location=4) in vec2 vertexUV;

// Uniform block for transformation matrices
layout(std140, binding=0) uniform TransformationBlock
{
	mat4 viewProjMatrix;      // View projection matrix
	mat4 skyboxProjMatrix;    // Skybox projection matrix
	mat4 rotationMatrix;      // Scene rotation matrix
};

// Output structure for the fragment shader
layout(location=0) out FragmentInput
{
	vec3 worldPos;          // World position for fragment
	vec2 uvCoords;          // Texture coordinates for fragment
	mat3 tangentSpaceMat;   // Tangent space transformation matrix
} fragInput;

void main()
{
	// Transform the vertex position using the scene's rotation matrix
	fragInput.worldPos = vec3(rotationMatrix * vec4(vertexPos, 1.0));
	
	// Adjust texture coordinates for the fragment
	fragInput.uvCoords = vec2(vertexUV.x, 1.0 - vertexUV.y);
	
	// Compute and pass the tangent space matrix for normal mapping
	fragInput.tangentSpaceMat = mat3(rotationMatrix) * mat3(vertexTangent, vertexBitangent, vertexNormal);

	// Compute the final clip-space position of the vertex
	gl_Position = viewProjMatrix * rotationMatrix * vec4(vertexPos, 1.0);
}