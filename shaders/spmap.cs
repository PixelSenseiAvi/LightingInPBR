#version 450 core

const float PI = 3.141592;
const float TwoPI = 2 * PI;
const float Epsilon = 0.00001;

const uint SAMPLE_COUNT = 1024;
const float InvNumSamples = 1.0 / float(SAMPLE_COUNT);

const int MIP_LEVEL_COUNT = 1;
layout(binding=0) uniform samplerCube envMap;
layout(binding=0, rgba16f) restrict writeonly uniform imageCube prefilteredEnvMap[MIP_LEVEL_COUNT];

layout(location=0) uniform float roughnessValue;

#define CURRENT_MIP_LEVEL     0
#define ROUGHNESS roughnessValue

float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 sampleHammersley(uint i)
{
	return vec2(i * InvNumSamples, radicalInverse_VdC(i));
}

vec3 sampleGGX(float u1, float u2, float roughness)
{
	float alpha = roughness * roughness;

	float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta); // Trig. identity
	float phi = TwoPI * u1;

	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float ndfGGX(float cosHalfVector, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosHalfVector * cosHalfVector) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

vec3 getSamplingVector()
{
    vec2 st = gl_GlobalInvocationID.xy/vec2(imageSize(prefilteredEnvMap[CURRENT_MIP_LEVEL]));
    vec2 uv = 2.0 * vec2(st.x, 1.0-st.y) - vec2(1.0);

    vec3 ret;
    if(gl_GlobalInvocationID.z == 0)      ret = vec3(1.0,  uv.y, -uv.x);
    else if(gl_GlobalInvocationID.z == 1) ret = vec3(-1.0, uv.y,  uv.x);
    else if(gl_GlobalInvocationID.z == 2) ret = vec3(uv.x, 1.0, -uv.y);
    else if(gl_GlobalInvocationID.z == 3) ret = vec3(uv.x, -1.0, uv.y);
    else if(gl_GlobalInvocationID.z == 4) ret = vec3(uv.x, uv.y, 1.0);
    else if(gl_GlobalInvocationID.z == 5) ret = vec3(-uv.x, uv.y, -1.0);
    return normalize(ret);
}

void computeBasisVectors(const vec3 N, out vec3 S, out vec3 T)
{
	T = cross(N, vec3(0.0, 1.0, 0.0));
	T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

	T = normalize(T);
	S = normalize(cross(N, T));
}

vec3 tangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
void main(void)
{
	ivec2 outputSize = imageSize(prefilteredEnvMap[CURRENT_MIP_LEVEL]);
	if(gl_GlobalInvocationID.x >= outputSize.x || gl_GlobalInvocationID.y >= outputSize.y) {
		return;
	}
	
	vec2 inputSize = vec2(textureSize(envMap, 0));
	float wt = 4.0 * PI / (6 * inputSize.x * inputSize.y);
	
	vec3 normal = getSamplingVector();
	vec3 viewDir = normal;
	
	vec3 tangent, bitangent;
	computeBasisVectors(normal, tangent, bitangent);

	vec3 accumulatedColor = vec3(0);
	float totalWeight = 0;

	for(uint i=0; i<SAMPLE_COUNT; ++i) {
		vec2 u = sampleHammersley(i);
		vec3 halfVector = tangentToWorld(sampleGGX(u.x, u.y, ROUGHNESS), normal, tangent, bitangent);

		vec3 lightDir = 2.0 * dot(viewDir, halfVector) * halfVector - viewDir;

		float cosLightDir = dot(normal, lightDir);
		if(cosLightDir > 0.0) {
			
			float cosHalfVector = max(dot(normal, halfVector), 0.0);

			float pdf = ndfGGX(cosHalfVector, ROUGHNESS) * 0.25;

			float ws = 1.0 / (SAMPLE_COUNT * pdf);

			float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

			accumulatedColor  += textureLod(envMap, lightDir, mipLevel).rgb * cosLightDir;
			totalWeight += cosLightDir;
		}
	}
	accumulatedColor /= totalWeight;

	imageStore(prefilteredEnvMap[CURRENT_MIP_LEVEL], ivec3(gl_GlobalInvocationID), vec4(accumulatedColor, 1.0));
}
