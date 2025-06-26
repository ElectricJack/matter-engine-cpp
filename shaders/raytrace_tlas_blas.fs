#version 330 core

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

// Camera uniforms
uniform vec3  cameraPos;
uniform vec3  cameraTarget;
uniform vec3  cameraUp;
uniform float cameraFovy;
uniform vec2  screenSize;

// Lighting uniforms (ported from raytracer.cl)
vec3 lightPos = vec3(3.0, 10.0, 2.0);
vec3 lightColor = vec3(150.0, 150.0, 120.0);
vec3 ambient = vec3(0.2, 0.2, 0.4);

// Sky texture (placeholder - could be added as uniform)
uniform sampler2D skyTexture;


// Include the enhanced BVH common code (ported from bvh_article)
#include "bvh_tlas_common.glsl"

// Camera ray generation
vec3 computeCameraRay(vec2 uv) {
    // Aspect ratio and FOV calculations
    float aspect = screenSize.x / screenSize.y;
    float fovRadians = radians(cameraFovy);
    float halfHeight = tan(fovRadians * 0.5);
    float halfWidth = aspect * halfHeight;
    
    // Camera coordinate system
    vec3 forward = normalize(cameraTarget - cameraPos);
    vec3 right = normalize(cross(forward, cameraUp));
    vec3 up = cross(right, forward);
    
    // Convert screen coordinates to camera space
    vec3 rayDir = normalize(
        halfWidth * (uv.x * 2.0 - 1.0) * right +
        halfHeight * (uv.y * 2.0 - 1.0) * up +
        forward
    );
    
    return rayDir;
}

// Sky sampling (ported from raytracer.cl)
vec3 sampleSky(vec3 direction) {
    // Procedural sky for now (could use skyTexture in the future)
    float phi = atan(direction.z, direction.x);
    float theta = acos(direction.y);
    
    // Simple gradient sky
    float skyFactor = (direction.y + 1.0) * 0.5;
    vec3 skyColor = mix(vec3(0.2, 0.4, 0.8), vec3(0.8, 0.9, 1.0), skyFactor);
    
    // Add sun effect
    vec3 sunDir = normalize(vec3(0.5, 0.7, 0.3));
    float sunDot = max(0.0, dot(direction, sunDir));
    float sun = pow(sunDot, 256.0);
    skyColor += vec3(1.0, 0.9, 0.7) * sun;
    
    return skyColor * 0.65;
}

// Advanced raytracing with reflections (ported from raytracer.cl)
vec3 trace(vec3 rayOrigin, vec3 rayDirection, uint seed) {
    vec3 rayPos = rayOrigin;
    vec3 rayDir = rayDirection;
    vec3 color = vec3(0.0);
    vec3 attenuation = vec3(1.0);
    
    for (int rayDepth = 0; rayDepth < 4; rayDepth++) {
        HitResult hit = intersectScene(rayPos, rayDir);
        
        if (!hit.hit) {
            // Hit sky
            color += attenuation * sampleSky(rayDir);
            break;
        }
        
        // DEBUG: If we found any hit, show a bright color
        if (rayDepth == 0 && gl_FragCoord.x < 100.0 && gl_FragCoord.y < 100.0) {
            return vec3(0.0, 1.0, 0.0); // Bright green for hits in top-left corner
        }
        
        // Get triangle for normal calculation and material properties
        vec3 hitPos = rayPos + rayDir * hit.t;
        vec3 normal = hit.normal;
        
        // Material properties based on instance ID (simulate material variation)
        bool isMirror = (hit.instanceId * 17) % 2 == 1;
        
        if (isMirror) {
            // Mirror reflection
            if (rayDepth == 1) {
                // Quick sky sample for depth 1 reflections
                vec3 reflectedDir = rayDir - 2.0 * normal * dot(normal, rayDir);
                color += attenuation * sampleSky(reflectedDir);
                break;
            }
            
            // Calculate reflection ray
            rayDir = rayDir - 2.0 * normal * dot(normal, rayDir);
            rayPos = hitPos + rayDir * 0.005; // Offset to avoid self-intersection
            
            // Slight attenuation for reflections
            attenuation *= 0.9;
        } else {
            // Diffuse material - calculate lighting
            vec3 lightVec = lightPos - hitPos;
            float lightDist = length(lightVec);
            vec3 lightDir = lightVec / lightDist;
            
            // Base material color
            vec3 albedo;
            if (hit.material == 0) albedo = vec3(0.8, 0.2, 0.2);      // Red
            else if (hit.material == 1) albedo = vec3(0.2, 0.2, 0.8); // Blue
            else if (hit.material == 2) albedo = vec3(0.2, 0.8, 0.2); // Green
            else if (hit.material == 3) albedo = vec3(0.8, 0.8, 0.2); // Yellow
            else if (hit.material == 4) albedo = vec3(0.8, 0.2, 0.8); // Magenta
            else albedo = vec3(0.5);                                   // Gray
            
            // Lighting calculation
            float lightFalloff = 1.0 / (lightDist * lightDist);
            float diffuse = max(0.0, dot(normal, lightDir));
            
            color += attenuation * albedo * (ambient + diffuse * lightColor * lightFalloff);
            break;
        }
    }
    
    return color;
}

void main() {
    // Use gl_FragCoord for reliable screen-space coordinates
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    
    // Initialize RNG seed (similar to raytracer.cl)
    uint seed = WangHash(uint(gl_FragCoord.x + gl_FragCoord.y * screenSize.x) * 17u + 1u);
    
    // Compute camera basis
    vec3 forward = normalize(cameraTarget - cameraPos);
    vec3 right = normalize(cross(forward, cameraUp));
    vec3 up = cross(right, forward);
    
    // Multiple samples for antialiasing (like raytracer.cl)
    vec3 color = vec3(0.0);
    for (int i = 0; i < 2; i++) {
        // Add random offset for antialiasing
        vec2 pixelOffset = vec2(RandomFloat(seed), RandomFloat(seed));
        vec2 uv = ((gl_FragCoord.xy + pixelOffset) / screenSize) * 2.0 - 1.0;
        uv.x *= screenSize.x / screenSize.y;
        
        // Compute ray direction
        float fovScale = tan(radians(cameraFovy) * 0.5);
        vec3 rayDir = normalize(uv.x * right * fovScale + uv.y * up * fovScale + forward);
        
        // Trace the ray
        color += trace(cameraPos, rayDir, seed);
    }
    
    // Average the samples
    color *= 0.5;
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    // DEBUG: Add a test pattern to verify shader is working
    vec2 testUV = gl_FragCoord.xy / screenSize;
    if (testUV.x < 0.1 && testUV.y < 0.1) {
        color = vec3(1.0, 0.0, 0.0); // Red square in top-left corner
    }
    
    finalColor = vec4(color, 1.0);
}