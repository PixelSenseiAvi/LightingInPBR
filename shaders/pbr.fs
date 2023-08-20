#version 450 core

const float PI_CONST = 3.141592;
const float EPSILON_CONST = 0.00001;
const int LIGHT_COUNT = 3;

const vec3 DIELECTRIC_FRESNEL = vec3(0.04);

struct LightSource {
	vec3 direction;
	vec3 intensity;
};

layout(location=0) in FragmentInput
{
	vec3 worldPos;
	vec2 uvCoords;          
	mat3 tangentSpaceMat;
} fragIn;


layout(location=0) out vec4 finalColor;

layout(std140, binding=1) uniform ShadingData
{
	LightSource lights[LIGHT_COUNT];
	vec3 viewerPos;
};

layout(binding=0) uniform sampler2D albedoTex;
layout(binding=1) uniform sampler2D normalMapTex;
layout(binding=2) uniform sampler2D metalnessTex;
layout(binding=3) uniform sampler2D roughnessTex;
layout(binding=4) uniform samplerCube specReflectionTex;
layout(binding=5) uniform samplerCube diffuseIrradianceTex;
layout(binding=6) uniform sampler2D specularBRDF_LUT_Tex;

// GGX/Towbridge-Reitz normal distribution function.
float calcNDF(float cosHalfway, float surfaceRoughness)
{
	float alpha = surfaceRoughness * surfaceRoughness;
	float denom = (cosHalfway * cosHalfway) * (alpha * alpha - 1.0) + 1.0;
	return alpha * alpha / (PI_CONST * denom * denom);
}

float calcG1Schlick(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

float calcGASchlick(float cosIncoming, float cosOutgoing, float surfaceRoughness)
{
	float r = surfaceRoughness + 1.0;
	float k = (r * r) / 8.0;
	return calcG1Schlick(cosIncoming, k) * calcG1Schlick(cosOutgoing, k);
}

vec3 calcFresnelSchlick(vec3 F0, float cosTheta)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
	vec3 surfaceAlbedo = texture(albedoTex, fragIn.uvCoords).rgb;
	float metalVal = texture(metalnessTex, fragIn.uvCoords).r;
	float surfaceRoughness = texture(roughnessTex, fragIn.uvCoords).r;
	vec3 outgoingDir = normalize(viewerPos - fragIn.worldPos);
	vec3 fragmentNormal = normalize(2.0 * texture(normalMapTex, fragIn.uvCoords).rgb - 1.0);
	fragmentNormal = normalize(fragIn.tangentSpaceMat * fragmentNormal);
	float cosOutgoing = max(0.0, dot(fragmentNormal, outgoingDir));
	vec3 reflectedDir = 2.0 * cosOutgoing * fragmentNormal - outgoingDir;
	vec3 F0 = mix(DIELECTRIC_FRESNEL, surfaceAlbedo, metalVal);
	vec3 directLightResult = vec3(0);
	
	for(int i=0; i<LIGHT_COUNT; ++i)
	{
		vec3 incomingDir = -lights[i].direction;
		vec3 lightIntensity = lights[i].intensity;
		vec3 halfwayDir = normalize(incomingDir + outgoingDir);
		float cosIncoming = max(0.0, dot(fragmentNormal, incomingDir));
		float cosHalfway = max(0.0, dot(fragmentNormal, halfwayDir));
		vec3 fresnel = calcFresnelSchlick(F0, max(0.0, dot(halfwayDir, outgoingDir)));
		float D = calcNDF(cosHalfway, surfaceRoughness);
		float G = calcGASchlick(cosIncoming, cosOutgoing, surfaceRoughness);
		vec3 diffuseFactor = mix(vec3(1.0) - fresnel, vec3(0.0), metalVal);
		vec3 diffuseBRDF = diffuseFactor * surfaceAlbedo;
		vec3 specularBRDF = (fresnel * D * G) / max(EPSILON_CONST, 4.0 * cosIncoming * cosOutgoing);
		directLightResult += (diffuseBRDF + specularBRDF) * lightIntensity * cosIncoming;
	}
	
	vec3 ambientResult;
	{
		vec3 diffuseIrradiance = texture(diffuseIrradianceTex, fragmentNormal).rgb;
		vec3 fresnel = calcFresnelSchlick(F0, cosOutgoing);
		vec3 diffuseFactor = mix(vec3(1.0) - fresnel, vec3(0.0), metalVal);
		vec3 diffuseIBL = diffuseFactor * surfaceAlbedo * diffuseIrradiance;
		int reflectionTexLevels = textureQueryLevels(specReflectionTex);
		vec3 specIrradiance = textureLod(specReflectionTex, reflectedDir, surfaceRoughness * reflectionTexLevels).rgb;
		vec2 specularBRDF = texture(specularBRDF_LUT_Tex, vec2(cosOutgoing, surfaceRoughness)).rg;
		vec3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specIrradiance;
		ambientResult = diffuseIBL + specularIBL;
	}
	
	finalColor = vec4(directLightResult + ambientResult, 1.0);
}