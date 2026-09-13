#version 330 core

// ---------------------------------------------------------------------------
//  Фрактальный ray-marching шадер (Mandelbrot 2D, Mandelbulb 3D,
//  Menger Sponge, Julia 3D).
// ---------------------------------------------------------------------------

out vec4 FragColor;

uniform vec2  uResolution;
uniform float uTime;
uniform vec3  uCamPos;
uniform vec3  uCamRight;
uniform vec3  uCamUp;
uniform vec3  uCamForward;
uniform float uFov;

uniform int   uFractalType;   // 0=2D, 1=Mandelbulb, 2=Menger, 3=Julia
uniform int   uIterations;
uniform float uBailout;
uniform float uPower;
uniform vec3  uJuliaC;
uniform float uDetail;
uniform float uColorScale;
uniform float uHueShift;
uniform int   uColorMode;
uniform float uM2dZoom;
uniform int   uMaxSteps;      // бюджет шагов ray marching (меньше при низком разрешении)

// ---------------------------------------------------------------------------
//  Distance estimators
// ---------------------------------------------------------------------------

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float mandelbulbDE(vec3 pos, float power, int iters) {
    vec3 z = pos;
    float dr = 1.0;
    float r = 0.0;
    for (int i = 0; i < iters; ++i) {
        r = length(z);
        if (r > uBailout || r < 1e-12) break;
        float theta = acos(z.z / r);
        float phi = atan(z.y, z.x);
        float zr = exp2(power * log2(r));
        dr = power * (zr / r) * dr + 1.0;
        theta *= power;
        phi *= power;
        z = zr * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)) + pos;
    }
    r = max(r, 1e-6);
    return 0.5 * log(r) * r / dr;
}

float juliaDE(vec3 pos, float power, vec3 c, int iters) {
    vec3 z = pos;
    float dr = 1.0;
    float r = 0.0;
    for (int i = 0; i < iters; ++i) {
        r = length(z);
        if (r > uBailout || r < 1e-12) break;
        float theta = acos(z.z / r);
        float phi = atan(z.y, z.x);
        float zr = exp2(power * log2(r));
        dr = power * (zr / r) * dr + 1.0;
        theta *= power;
        phi *= power;
        z = zr * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)) + c;
    }
    r = max(r, 1e-6);
    return 0.5 * log(r) * r / dr;
}

float posMod(float x, float m) {
    return x - m * floor(x / m);
}

float mengerDE(vec3 pos, int iters) {
    vec3 q = abs(pos);
    float d = max(q.x, max(q.y, q.z)) - 1.0;   // sdBox куба
    float s = 1.0;
    for (int i = 0; i < iters; ++i) {
        vec3 a = vec3(posMod(pos.x * s, 2.0) - 1.0,
                      posMod(pos.y * s, 2.0) - 1.0,
                      posMod(pos.z * s, 2.0) - 1.0);
        s *= 3.0;
        vec3 r = abs(1.0 - 3.0 * abs(a));
        float da = max(r.x, r.y);
        float db = max(r.y, r.z);
        float dc = max(r.z, r.x);
        float c = (min(da, min(db, dc)) - 1.0) / s;
        d = max(d, c);
    }
    return d;
}

float sceneDE(vec3 p) {
    if (uFractalType == 1) return mandelbulbDE(p, uPower, uIterations);
    if (uFractalType == 2) return mengerDE(p, uIterations);
    if (uFractalType == 3) return juliaDE(p, uPower, uJuliaC, uIterations);
    return 1e9;
}

// ---------------------------------------------------------------------------
//  Палитра и цвет
// ---------------------------------------------------------------------------

vec3 palette(float t) {
    vec3 a = vec3(0.5);
    vec3 b = vec3(0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.05, 0.33, 0.67);  // сине-зелёно-красный
    return a + b * cos(6.28318 * (c * t + d));
}

// ---------------------------------------------------------------------------
//  2D Mandelbrot
// ---------------------------------------------------------------------------

vec3 mandelbrot2D() {
    vec2 center = uCamPos.xy;   // камера: pos.x/pos.y -> центр плоскости
    vec2 uv = (gl_FragCoord.xy - 0.5 * uResolution) / (0.5 * uResolution.y);
    vec2 c = center + uv * (2.0 / max(uM2dZoom, 0.0001));

    vec2 z = vec2(0.0);
    int iter = uIterations;
    for (int i = 0; i < uIterations; ++i) {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (dot(z, z) > uBailout * uBailout) {
            iter = i;
            break;
        }
    }

    if (iter == uIterations) return vec3(0.0); // внутри множества -> чёрный

    float mu = float(iter) + 1.0 - log2(log(dot(z, z)) * 0.5);
    float t = mu / float(uIterations);
    if (uColorMode == 1) return vec3(t);
    return palette(t * uColorScale + uHueShift);
}

// ---------------------------------------------------------------------------
//  Ray marching (3D)
// ---------------------------------------------------------------------------

vec3 march(vec3 origin, vec3 dir, out float progress) {
    float t = 0.0;
    progress = 0.0;
    int maxIter = uMaxSteps;
    if (maxIter <= 0) maxIter = 256;
    for (int i = 0; i < maxIter; ++i) {
        vec3 pos = origin + dir * t;
        float d = sceneDE(pos);
        if (d < uDetail || t > 40.0) break;
        t += d;
        progress = float(i) / float(maxIter);
    }
    return origin + dir * t;
}

vec3 calcNormal(vec3 p) {
    // дешёвые нормали (2 перекрёстных сэмпла, 4 вызова DE вместо 6)
    vec2 e = vec2(1.0, -1.0) * uDetail;
    return normalize(e.xyy * sceneDE(p + e.xyy) +
                     e.yyx * sceneDE(p + e.yyx) +
                     e.yxy * sceneDE(p + e.yxy) +
                     e.xxx * sceneDE(p + e.xxx));
}

vec3 shade(vec3 pos, vec3 n, float progress) {
    vec3 lightDir = normalize(vec3(0.65, 0.8, -0.35));
    float diffuse = max(dot(n, lightDir), 0.0);
    float ao = 1.0 - progress * 0.55;

    vec3 base = (uColorMode == 1)
        ? vec3(progress)
        : palette(progress * uColorScale + uHueShift);

    vec3 col = base * (0.12 + 0.9 * diffuse) * ao;

    // лёгкий фог
    float dist = length(pos - uCamPos);
    col = mix(col, vec3(0.04, 0.05, 0.09), 1.0 - exp(-0.045 * dist));
    return col;
}

// ---------------------------------------------------------------------------

void main() {
    float tanHalf = tan(radians(uFov * 0.5));
    vec2 p = (gl_FragCoord.xy - 0.5 * uResolution) / uResolution.y;
    vec3 dir = normalize(uCamForward +
                         uCamRight * p.x * tanHalf +
                         uCamUp * p.y * tanHalf);

    // 2D-фрактал рисуется без ray marching
    if (uFractalType == 0) {
        FragColor = vec4(mandelbrot2D(), 1.0);
        return;
    }

    float progress;
    vec3 pos = march(uCamPos, dir, progress);

    vec3 col;
    float dist = length(pos - uCamPos);
    if (dist > 39.5) {
        // фон
        col = palette(0.08 + 0.20 * dir.y);
        col *= 0.5;
    } else {
        vec3 n = calcNormal(pos);
        col = shade(pos, n, progress);
    }

    FragColor = vec4(col, 1.0);
}