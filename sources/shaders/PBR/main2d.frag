#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../functions/constants.glsl"

// Buffers ==================================================
layout(set = 1, binding = 0) buffer outputBuffer {
    vec4 color[];
} interference;

layout(set = 1, binding = 1) uniform Misc {
    vec3 viewPosition;
    uint opdSample;
};

// Textures ==================================================
layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D aoMap;
layout(set = 2, binding = 2) uniform sampler2D metallicMap;
layout(set = 2, binding = 3) uniform sampler2D normalMap;
layout(set = 2, binding = 4) uniform sampler2D roughnessMap;

// Inputs ==================================================
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPosition;

// Outputs ==================================================
layout(location = 0) out vec4 outColor;

// Functions ==================================================
#include "../functions/interference.glsl"
#include "../functions/render_function.glsl"
//#include "../functions/pbr.glsl"

void main() {
    
    vec3  N = getNormal();
    float theta1 = getTheta1(N);
    float angleRad = theta1 / PI * 2.0;

    uint idx = getIndex2D(angleRad);
    outColor = interference.color[idx];
    
//    vec4 pbrColor = vec4(pbr(), 1.0);
//    outColor = pbrColor;
//    
//    float metallic  = texture(metallicMap, fragTexCoord).r;
//    if (metallic > 0.5) {
//        outColor = pbrColor * interference.color[idx] * 2.4;
//    }
}
