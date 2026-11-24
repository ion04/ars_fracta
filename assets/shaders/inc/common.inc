#version 330 core

// ---------------------------------------------------------------------------
//  Common definitions shared by all fractal shaders
// ---------------------------------------------------------------------------

out vec4 FragColor;

// Camera / view
uniform vec2  uResolution;
uniform float uTime;
uniform vec3  uCamPos;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform vec3  uCamForward;
uniform float uFov;

// Fractal parameters via UBO (std140 layout)
layout(std140) uniform FractalParamsUBO {
    int   uFractalType;   // 0=2D, 1=Mandelbulb, 2=Menger, 3=Julia, 4=Terrain, 5=Coast
    int   uIterations;
    float uBailout;
    float uPower;
    vec3  uJuliaC;
    float uDetail;
    float uColorScale;
    float uHueShift;
    int   uColorMode;
    float uM2dZoom;
    int   uMaxSteps;
    float uTime;          // frame time for animations

    // Terrain3D (uFractalType == 4)
    float uTerrainAmplitude;
    float uTerrainFrequency;
    float uCloudDensity;
    float uTreeDensity;
    float uTimeOfDay;
    float uFogDensity;

    // Coast3D (uFractalType == 5)
    vec2  uCoastCenter;
} ubo;

// ---------------------------------------------------------------------------
//  Shared math utilities
// ---------------------------------------------------------------------------

// Forward declarations for cross-module DE functions
float mengerDE(vec3 pos, int iters);

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float posMod(float x, float m) {
    return x - m * floor(x / m);
}

float hash11(float n) {
    return fract(sin(n * 127.1) * 43758.5453123);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
               mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x),
               u.y);
}

float fbm2(vec2 p, int oct) {
    float a = 0.5;
    float f = 1.0;
    float s = 0.0;
    for (int i = 0; i < oct; ++i) {
        s += a * vnoise(p * f);
        f *= 2.03;
        a *= 0.5;
    }
    return s;
}

float ridge2(vec2 p, int oct) {
    float a = 0.5;
    float f = 1.0;
    float s = 0.0;
    for (int i = 0; i < oct; ++i) {
        float n = vnoise(p * f);
        s += a * (1.0 - abs(2.0 * n - 1.0));
        f *= 2.07;
        a *= 0.5;
    }
    return s;
}

float hash13(vec3 p) {
    p = fract(p * 0.10301);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float vnoise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    float a = mix(mix(hash13(i), hash13(i + vec3(1.0, 0.0, 0.0)), u.x),
                  mix(hash13(i + vec3(0.0, 1.0, 0.0)),
                      hash13(i + vec3(1.0, 1.0, 0.0)), u.x), u.y);
    float b = mix(mix(hash13(i + vec3(0.0, 0.0, 1.0)),
                      hash13(i + vec3(1.0, 0.0, 1.0)), u.x),
                  mix(hash13(i + vec3(0.0, 1.0, 1.0)),
                      hash13(i + vec3(1.0, 1.0, 1.0)), u.x), u.y);
    return mix(a, b, u.z);
}

// Palette
vec3 palette(float t) {
    vec3 a = vec3(0.5);
    vec3 b = vec3(0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.05, 0.33, 0.67);
    return a + b * cos(6.28318 * (c * t + d));
}

// Grade (mild saturation boost)
vec3 grade(vec3 c) {
    return mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, 1.12);
}

vec3 gradeCoast(vec3 c) {
    return mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, 1.15) * 1.10;
}

// Sun / time of day
vec3 sunDir() {
    float ang = (ubo.uTimeOfDay - 0.25) * 6.2831853;
    return normalize(vec3(0.45, 0.42 + 0.58 * sin(ang), 0.72));
}

float daylightLevel() {
    float el = 0.35 + 0.5 * sin((ubo.uTimeOfDay - 0.25) * 6.2831853);
    return smoothstep(-0.10, 0.25, el);
}

float duskLevel() {
    float el = 0.35 + 0.5 * sin((ubo.uTimeOfDay - 0.25) * 6.2831853);
    return 1.0 - smoothstep(0.02, 0.32, el);
}

vec3 sunTintRGB(float dusk) {
    return mix(vec3(1.10, 0.98, 0.86), vec3(1.15, 0.62, 0.34), dusk * 0.85);
}

vec3 coolAmbient(float dusk) {
    return mix(vec3(0.70, 0.80, 1.05), vec3(0.62, 0.66, 0.92), dusk * 0.70);
}

vec3 skyTintRGB(float dusk) {
    return mix(vec3(0.55, 0.70, 1.05), vec3(0.85, 0.70, 0.85), dusk * 0.60);
}

// Fog
vec3 applyFog(vec3 col, vec3 rd, vec3 sun, float nt, float dist) {
    float sunHit = clamp(dot(rd, sun), 0.0, 1.0);
    float dusk = duskLevel();
    vec3 fog = vec3(0.62, 0.71, 0.87);
    fog += vec3(1.0, 0.70, 0.45) * pow(sunHit, 5.0) * 0.35 * nt;
    fog = mix(fog, vec3(1.0, 0.55, 0.35), dusk * 0.70);
    float a = 1.0 - exp(-dist * ubo.uFogDensity);
    return mix(col, fog, clamp(a, 0.0, 0.58));
}

// Sky gradient
vec3 skyGradient(vec3 rd, vec3 sun, float nt, vec3 horizon, vec3 zenith) {
    float hUp = rd.y * 0.5 + 0.5;
    float dusk = duskLevel();
    vec3 hor = mix(horizon, vec3(1.0, 0.62, 0.38), dusk * 0.85);
    vec3 zen = mix(zenith, vec3(0.45, 0.32, 0.55), dusk * 0.55);
    vec3 col = mix(hor, zen, pow(hUp, 0.55));
    col *= mix(0.12, 1.0, nt);
    float sunHit = clamp(dot(rd, sun), 0.0, 1.0);
    col += vec3(1.0, 0.88, 0.6) * pow(sunHit, 220.0) * 2.2 * nt;
    col += vec3(1.0, 0.60, 0.30) * pow(sunHit, 7.0) * (0.40 + 0.9 * dusk) * nt;
    return col;
}

// Clouds
float cloudAlpha(vec3 ro, vec3 rd, float hitT) {
    if (ubo.uCloudDensity <= 0.0) return 0.0;
    float yLayer = ubo.uTerrainAmplitude * 1.25 + 2.0;
    if (rd.y <= 0.0) return 0.0;
    float tC = (yLayer - ro.y) / rd.y;
    if (tC < 0.0 || (hitT > 0.0 && tC > hitT)) return 0.0;
    vec2 px = (ro + rd * tC).xz;
    vec2 q = px * 0.06 + vec2(ubo.uTime * 0.01, 0.0);
    float n = fbm2(q, 4);
    float cover = smoothstep(0.42, 0.78, n);
    float density = smoothstep(0.45, 0.75, n) * ubo.uCloudDensity;
    return clamp(cover * density * 0.9, 0.0, 0.95);
}

vec3 cloudShade(float nt) {
    float dusk = duskLevel();
    vec3 c = vec3(0.95, 0.96, 0.98) * (0.25 + 0.75 * nt);
    return mix(c, vec3(1.0, 0.82, 0.68), dusk * 0.75);
}

// Rim light
vec3 rimLight(vec3 n, vec3 rd, vec3 sun, float shadow, float k, float boost, float dusk) {
    float rim = pow(1.0 - clamp(dot(n, -rd), 0.0, 1.0), k) *
                clamp(dot(sun, rd), 0.0, 1.0);
    return sunTintRGB(dusk) * (rim * (0.35 + 0.65 * shadow) * (1.0 + boost));
}