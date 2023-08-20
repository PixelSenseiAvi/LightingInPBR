#version 450 core

const float PI = 3.141592;
const float TWO_PI = 2 * PI;
const float MIN_EPSILON = 0.001; 

const uint SAMPLE_COUNT = 1024;
const float INV_SAMPLE_COUNT = 1.0 / float(SAMPLE_COUNT);

layout(binding=0, rg16f) restrict writeonly uniform image2D LUT;

// Compute Van der Corput radical inverse
float computeRadicalInverse(uint inputBits)
{
	inputBits = (inputBits << 16u) | (inputBits >> 16u);
	inputBits = ((inputBits & 0x55555555u) << 1u) | ((inputBits & 0xAAAAAAAAu) >> 1u);
	inputBits = ((inputBits & 0x33333333u) << 2u) | ((inputBits & 0xCCCCCCCCu) >> 2u);
	inputBits = ((inputBits & 0x0F0F0F0Fu) << 4u) | ((inputBits & 0xF0F0F0F0u) >> 4u);
	inputBits = ((inputBits & 0x00FF00FFu) << 8u) | ((inputBits & 0xFF00FF00u) >> 8u);
	return float(inputBits) * 2.3283064365386963e-10;
}

vec2 sampleHammersley(uint sampleIndex)
{
	return vec2(sampleIndex * INV_SAMPLE_COUNT, computeRadicalInverse(sampleIndex));
}

vec3 sampleGGXTangentSpace(float randU1, float randU2, float surfaceRoughness)
{
	float alpha = surfaceRoughness * surfaceRoughness;

	float cosTheta = sqrt((1.0 - randU2) / (1.0 + (alpha*alpha - 1.0) * randU2));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	float phi = TWO_PI * randU1;

	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float schlickGGXApproximationSingleTerm(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

float schlickGGXApproximationIBL(float incidentLightAngle, float outgoingLightAngle, float surfaceRoughness)
{
	float remappedRoughness = (surfaceRoughness * surfaceRoughness) / 2.0;
	return schlickGGXApproximationSingleTerm(incidentLightAngle, remappedRoughness) * schlickGGXApproximationSingleTerm(outgoingLightAngle, remappedRoughness);
}

layout(local_size_x=32, local_size_y=32, local_size_z=1) in;
void main(void)
{
	float outgoingLightAngle = gl_GlobalInvocationID.x / float(imageSize(LUT).x);
	float surfaceRoughness = gl_GlobalInvocationID.y / float(imageSize(LUT).y);

	outgoingLightAngle = max(outgoingLightAngle, MIN_EPSILON);

	vec3 outgoingLightDirection = vec3(sqrt(1.0 - outgoingLightAngle*outgoingLightAngle), 0.0, outgoingLightAngle);

	float preIntegratedBRDF_DFG1 = 0;
	float preIntegratedBRDF_DFG2 = 0;

	for(uint i=0; i<SAMPLE_COUNT; ++i) {
		vec2 randVector  = sampleHammersley(i);
		vec3 halfVector = sampleGGXTangentSpace(randVector.x, randVector.y, surfaceRoughness);
		vec3 incidentLightDirection = 2.0 * dot(outgoingLightDirection, halfVector) * halfVector - outgoingLightDirection;

		float incidentLightAngle = incidentLightDirection.z;
		float halfAndOutgoingDotProduct = max(dot(outgoingLightDirection, halfVector), 0.0);

		if(incidentLightAngle > 0.0) {
			float geometryAttenuation = schlickGGXApproximationIBL(incidentLightAngle, outgoingLightAngle, surfaceRoughness);
			float geometryVisibility = geometryAttenuation * halfAndOutgoingDotProduct / (incidentLightAngle * outgoingLightAngle);
			float fresnelSchlick = pow(1.0 - halfAndOutgoingDotProduct, 5);

			preIntegratedBRDF_DFG1 += (1 - fresnelSchlick) * geometryVisibility;
			preIntegratedBRDF_DFG2 += fresnelSchlick * geometryVisibility;
		}
	}

	imageStore(LUT, ivec2(gl_GlobalInvocationID), vec4(preIntegratedBRDF_DFG1, preIntegratedBRDF_DFG2, 0, 0) * INV_SAMPLE_COUNT);
}