#version 450 core

// Constants
const float PI = 3.141592;
const float TwoPI = 2.0 * PI;
const float Epsilon = 0.00001;
const uint NumSamples = 64 * 1024;
const float InvNumSamples = 1.0 / float(NumSamples);

// Texture layout
layout(binding=0) uniform samplerCube inputTexture;
layout(binding=0, rgba16f) restrict writeonly uniform imageCube outputTexture;

// Helper function to compute the Van der Corput sequence for low-discrepancy sequences
float computeVanDerCorput(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

// Helper function to generate Hammersley samples for Monte Carlo integration
vec2 generateHammersleySample(uint i) {
    return vec2(i * InvNumSamples, computeVanDerCorput(i));
}

// Helper function to sample a point on the hemisphere
vec3 sampleHemispherePoint(float u1, float u2) {
    const float u1p = sqrt(max(0.0, 1.0 - u1 * u1));
    return vec3(cos(TwoPI * u2) * u1p, sin(TwoPI * u2) * u1p, u1);
}

// Helper function to compute the normalized sampling direction vector
vec3 computeSamplingDirectionVector() {
    vec2 st = gl_GlobalInvocationID.xy / vec2(imageSize(outputTexture));
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - vec2(1.0);
    vec3 ret;

    if (gl_GlobalInvocationID.z == 0)
        ret = vec3(1.0, uv.y, -uv.x);
    else if (gl_GlobalInvocationID.z == 1) 
        ret = vec3(-1.0, uv.y, uv.x);
    else if (gl_GlobalInvocationID.z == 2) 
        ret = vec3(uv.x, 1.0, -uv.y);
    else if (gl_GlobalInvocationID.z == 3) 
        ret = vec3(uv.x, -1.0, uv.y);
    else if (gl_GlobalInvocationID.z == 4) 
        ret = vec3(uv.x, uv.y, 1.0);
    else if (gl_GlobalInvocationID.z == 5) 
        ret = vec3(-uv.x, uv.y, -1.0);

    return normalize(ret);
}

// Helper function to compute the orthonormal basis vectors
void computeBasisVectors(const vec3 normal, out vec3 tangent, out vec3 bitangent) {
    bitangent = cross(normal, vec3(0.0, 1.0, 0.0));
    bitangent = mix(cross(normal, vec3(1.0, 0.0, 0.0)), bitangent, step(Epsilon, dot(bitangent, bitangent)));
    bitangent = normalize(bitangent);
    tangent = normalize(cross(normal, bitangent));
}

// Helper function to convert a point from tangent/shading space to world space
vec3 convertToWorldSpace(const vec3 v, const vec3 normal, const vec3 tangent, const vec3 bitangent) {
    return tangent * v.x + bitangent * v.y + normal * v.z;
}

void main(void) {
    vec3 normal = computeSamplingDirectionVector();
    vec3 tangent, bitangent;
    computeBasisVectors(normal, tangent, bitangent);

    vec3 irradiance = vec3(0.0);
    for(uint i=0; i<NumSamples; ++i) {
        vec2 sample = generateHammersleySample(i);
        vec3 hemisphereSample = convertToWorldSpace(sampleHemispherePoint(sample.x, sample.y), normal, tangent, bitangent);
        float cosTheta = max(0.0, dot(hemisphereSample, normal));
        irradiance += 2.0 * textureLod(inputTexture, hemisphereSample, 0).rgb * cosTheta;
    }
    irradiance /= vec3(NumSamples);

    imageStore(outputTexture, ivec3(gl_GlobalInvocationID), vec4(irradiance, 1.0));
}
