#version 460 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 lightColor;
uniform vec3 sunDirection;
uniform vec3 sunLightColor;
uniform float ambientStrength;
uniform bool useShadowMap;
uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform mat4 view;
uniform sampler2DShadow cascadeShadowMaps[4];
uniform mat4 cascadeLightSpaceMatrices[4];
uniform float cascadeSplits[4];
uniform int numCascades;

float calculateShadow(vec3 normal, vec3 lightDir)
{
    if (!useShadowMap) return 0.0;

    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.00001);
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float ndotl = max(dot(normal, lightDir), 0.0);
    float bias = max(0.0035 * (1.0 - ndotl), 0.0008);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

int getCascadeIndex(float viewDepth)
{
    for (int i = 0; i < numCascades; ++i) {
        if (viewDepth < cascadeSplits[i]) return i;
    }
    return max(numCascades - 1, 0);
}

float calculateCascadedShadow(vec3 normal, vec3 lightDir)
{
    if (numCascades <= 0) return 0.0;

    float viewDepth = -(view * vec4(FragPos, 1.0)).z;
    int cascadeIndex = getCascadeIndex(viewDepth);

    vec4 fragPosLightSpace = cascadeLightSpaceMatrices[cascadeIndex] * vec4(FragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.00001);
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float ndotl = max(dot(normal, lightDir), 0.0);
    float bias = max(0.0015 * (1.0 - ndotl), 0.0004);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(cascadeShadowMaps[cascadeIndex], 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(cascadeShadowMaps[cascadeIndex], vec3(projCoords.xy + offset, projCoords.z - bias));
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(Normal);
    vec3 L = normalize(sunDirection);

    float diff = max(dot(N, L), 0.0);
    float shadow = 0.0;
    if (diff > 0.0) {
        shadow = (numCascades > 0) ? calculateCascadedShadow(N, L) : calculateShadow(N, L);
    }

    vec3 ambient = max(ambientStrength, 0.08) * lightColor;
    vec3 direct = (1.0 - shadow) * diff * lightColor * max(sunLightColor, vec3(0.5));

    FragColor = vec4(ambient + direct, 1.0);
}