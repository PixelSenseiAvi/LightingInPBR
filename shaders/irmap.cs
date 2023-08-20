#version 450 core

// Constants
const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const uint SAMPLE_COUNT = 64 * 1024;
const float SAMPLE_COUNT_INV = 1.0 / float(SAMPLE_COUNT);
const float EPSILON = 0.00001;

// Texture layout
layout(binding=0) uniform samplerCube envMap;              // Input environment map (cubemap)
layout(binding=0, rgba16f) restrict writeonly uniform imageCube irradianceMap;  // Output irradiance map (cubemap)
layout(local_size_x=32, local_size_y=32, local_size_z=1) in;

// Computes the Van Der Corput sequence for quasi-random (low-discrepancy) sampling.
float VanDerCorput(uint index) {
    // Bit manipulation to create the sequence
    index = (index << 16u) | (index >> 16u);
    index = ((index & 0x55555555u) << 1u) | ((index & 0xAAAAAAAAu) >> 1u);
    index = ((index & 0x33333333u) << 2u) | ((index & 0xCCCCCCCCu) >> 2u);
    index = ((index & 0x0F0F0F0Fu) << 4u) | ((index & 0xF0F0F0F0u) >> 4u);
    index = ((index & 0x00FF00FFu) << 8u) | ((index & 0xFF00FF00u) >> 8u);
    return float(index) * 2.3283064365386963e-10;
}

// Generates Hammersley quasi-random samples for Monte Carlo integration.
vec2 HammersleySample(uint index) {
    return vec2(index * SAMPLE_COUNT_INV, VanDerCorput(index));
}

// Samples a point on the unit hemisphere using spherical coordinates.
vec3 SampleHemisphere(float u, float v) {
    const float rad = sqrt(max(0.0, 1.0 - u * u));
    return vec3(cos(TWO_PI * v) * rad, sin(TWO_PI * v) * rad, u);
}

// Determines the normalized direction for each texel of the output cubemap.
vec3 GetSampleDirection() {
    // Normalize coordinates to the range [0, 1]
    vec2 normalizedCoords = gl_GlobalInvocationID.xy / vec2(imageSize(irradianceMap));
    vec2 uv = 2.0 * vec2(normalizedCoords.x, 1.0 - normalizedCoords.y) - vec2(1.0);
    vec3 direction;

    // Convert 2D UV to 3D direction for each face of the cubemap

    if (gl_GlobalInvocationID.z == 0)
        direction = vec3(1.0, uv.y, -uv.x);
    else if (gl_GlobalInvocationID.z == 1) 
        direction = vec3(-1.0, uv.y, uv.x);
    else if (gl_GlobalInvocationID.z == 2) 
        direction = vec3(uv.x, 1.0, -uv.y);
    else if (gl_GlobalInvocationID.z == 3) 
        direction = vec3(uv.x, -1.0, uv.y);
    else if (gl_GlobalInvocationID.z == 4) 
        direction = vec3(uv.x, uv.y, 1.0);
    else if (gl_GlobalInvocationID.z == 5) 
        direction = vec3(-uv.x, uv.y, -1.0);

    return normalize(direction);
}

// Constructs an orthonormal basis for a given normal vector.
void OrthonormalBasis(const vec3 n, out vec3 t, out vec3 b) {
    // Create a tangent that's orthogonal to the normal
    b = abs(n.y) < EPSILON ? cross(n, vec3(1.0, 0.0, 0.0)) : cross(n, vec3(0.0, 1.0, 0.0));
    b = normalize(b);
    t = normalize(cross(n, b)); // Compute the third orthonormal vector
}

// Transforms a sample point from tangent space to world space.
vec3 ToWorldSpace(const vec3 v, const vec3 n, const vec3 t, const vec3 b) {
    return t * v.x + b * v.y + n * v.z;
}

void main(void) {
    // Get the direction for the current texel
    vec3 n = GetSampleDirection();
    vec3 t, b;
    OrthonormalBasis(n, t, b); // Generate the tangent and bitangent for the current direction

    vec3 result = vec3(0.0);
    for(uint i = 0; i < SAMPLE_COUNT; ++i) {
        // Generate a sample direction in the hemisphere around the current direction
        vec2 sampleUV = HammersleySample(i);
        vec3 hemisphereSample = ToWorldSpace(SampleHemisphere(sampleUV.x, sampleUV.y), n, t, b);

        // Weight by the cosine of the angle between the sample direction and the normal
        float weight = max(0.0, dot(hemisphereSample, n));

        // Accumulate the weighted environment map sample
        result += 2.0 * textureLod(envMap, hemisphereSample, 0).rgb * weight;
    }

    // Average the accumulated samples
    result /= vec3(SAMPLE_COUNT);

    // Store the computed irradiance value for the current texel
    imageStore(irradianceMap, ivec3(gl_GlobalInvocationID), vec4(result, 1.0));
}
