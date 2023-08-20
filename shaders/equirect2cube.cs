#version 450 core

// Constants
const float PI = 3.141592;
const float TwoPI = 2 * PI;

// Sampler for the equirectangular input texture.
layout(binding=0) uniform sampler2D equirectangularTexture;

// Image cube to store the output from the conversion.
layout(binding=0, rgba16f) restrict writeonly uniform imageCube cubeMapTexture;

// Calculate normalized sampling direction vector based on current fragment coordinates (gl_GlobalInvocationID.xyz)
vec3 calculateSamplingVector()
{
    vec2 textureSizeRatio = gl_GlobalInvocationID.xy / vec2(imageSize(cubeMapTexture));
    vec2 uvCoordinates = 2.0 * vec2(textureSizeRatio.x, 1.0 - textureSizeRatio.y) - vec2(1.0);

    vec3 directionVector;

	// Determine direction vector based on which face of the cubemap we're drawing.
    if(gl_GlobalInvocationID.z == 0)      directionVector = vec3(1.0,  uvCoordinates.y, -uvCoordinates.x);
    else if(gl_GlobalInvocationID.z == 1) directionVector = vec3(-1.0, uvCoordinates.y,  uvCoordinates.x);
    else if(gl_GlobalInvocationID.z == 2) directionVector = vec3(uvCoordinates.x, 1.0, -uvCoordinates.y);
    else if(gl_GlobalInvocationID.z == 3) directionVector = vec3(uvCoordinates.x, -1.0, uvCoordinates.y);
    else if(gl_GlobalInvocationID.z == 4) directionVector = vec3(uvCoordinates.x, uvCoordinates.y, 1.0);
    else if(gl_GlobalInvocationID.z == 5) directionVector = vec3(-uvCoordinates.x, uvCoordinates.y, -1.0);

    return normalize(directionVector);
}

layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
void main(void)
{
	// Get the sampling direction vector based on the current fragment.
	vec3 sampleDirection = calculateSamplingVector();

	// Convert the Cartesian direction vector to spherical coordinates.
	float phi   = atan(sampleDirection.z, sampleDirection.x);
	float theta = acos(sampleDirection.y);

	// Sample the equirectangular texture based on the calculated spherical coordinates.
	vec4 sampledColor = texture(equirectangularTexture, vec2(phi / TwoPI, theta / PI));

	// Write the sampled color to the output cubemap.
	imageStore(cubeMapTexture, ivec3(gl_GlobalInvocationID), sampledColor);
}