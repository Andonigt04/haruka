#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gEmissive;
uniform sampler2D ssao;
uniform samplerCube pointShadowMap;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform float farPlane;

struct Light {
    vec3 position;
    vec3 color;
};
uniform Light lights[32];
uniform int numLights;

// Muestras para PCF en cubemap
vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float ShadowCalculation(vec3 FragPos)
{
    vec3 fragToLight = FragPos - lightPos;
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    float bias = 0.05;
    int samples = 20;
    float diskRadius = 0.01;

    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(pointShadowMap, normalize(fragToLight + sampleOffsetDirections[i] * diskRadius)).r;
        closestDepth *= farPlane;
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
    return shadow;
}

void main()
{
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal  = normalize(texture(gNormal, TexCoords).rgb);
    vec3 Albedo  = texture(gAlbedoSpec, TexCoords).rgb;
    float Spec   = texture(gAlbedoSpec, TexCoords).a;
    vec3 Emissive = texture(gEmissive, TexCoords).rgb;
    float AO = 1.0; // fuerza AO para evitar negro

    vec3 viewDir = normalize(viewPos - FragPos);

    // ===== DIRECT LIGHTING (Point Lights) =====
    vec3 lighting = vec3(0.0);
    
    // Ambient
    vec3 ambient = 0.1 * Albedo * AO;
    lighting += ambient;
    
    // shadow normal
    float shadow = ShadowCalculation(FragPos);

    for(int i = 0; i < numLights; ++i)
    {
        vec3 lightDir = normalize(lights[i].position - FragPos);
        float diff = max(dot(Normal, lightDir), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 32.0) * Spec;

        float distance = length(lights[i].position - FragPos);
        float attenuation = 1.0 / (distance * distance + 0.001);

        vec3 radiance = lights[i].color * attenuation;

        float shadowFactor = (i == 0) ? (1.0 - shadow) : 1.0;
        lighting += (diff * Albedo + spec * vec3(0.5)) * radiance * shadowFactor;
    }
    
    lighting += Emissive * 0.5;
    FragColor = vec4(lighting, 1.0);
}