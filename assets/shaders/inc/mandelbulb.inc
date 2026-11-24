// ---------------------------------------------------------------------------
//  Mandelbulb / Julia 3D Distance Estimators
// ---------------------------------------------------------------------------

float mandelbulbDE(vec3 pos, float power, int iters) {
    vec3 z = pos;
    float dr = 1.0;
    float r = 0.0;
    for (int i = 0; i < iters; ++i) {
        r = length(z);
        if (r > ubo.uBailout || r < 1e-12) break;
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
        if (r > ubo.uBailout || r < 1e-12) break;
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

// Unified scene DE for types 1 (Mandelbulb), 2 (Menger), 3 (Julia)
float sceneDE(vec3 p) {
    if (ubo.uFractalType == 1) return mandelbulbDE(p, ubo.uPower, ubo.uIterations);
    if (ubo.uFractalType == 2) return mengerDE(p, ubo.uIterations);
    if (ubo.uFractalType == 3) return juliaDE(p, ubo.uPower, ubo.uJuliaC, ubo.uIterations);
    return 1e9;
}

// Normal calculation (4-sample cross)
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(1.0, -1.0) * ubo.uDetail;
    return normalize(e.xyy * sceneDE(p + e.xyy) +
                     e.yyx * sceneDE(p + e.yyx) +
                     e.yxy * sceneDE(p + e.yxy) +
                     e.xxx * sceneDE(p + e.xxx));
}

// Shading for 3D fractals (types 1, 2, 3)
vec3 shade(vec3 pos, vec3 n, float progress) {
    vec3 lightDir = normalize(vec3(0.65, 0.8, -0.35));
    float diffuse = max(dot(n, lightDir), 0.0);
    float ao = 1.0 - progress * 0.55;

    vec3 base = (ubo.uColorMode == 1)
        ? vec3(progress)
        : palette(progress * ubo.uColorScale + ubo.uHueShift);

    vec3 col = base * (0.12 + 0.9 * diffuse) * ao;

    // Light fog
    float dist = length(pos - uCamPos);
    col = mix(col, vec3(0.04, 0.05, 0.09), 1.0 - exp(-0.045 * dist));
    return col;
}

// Ray marching for 3D fractals
vec3 march(vec3 origin, vec3 dir, out float progress) {
    float t = 0.0;
    progress = 0.0;
    int maxIter = ubo.uMaxSteps;
    if (maxIter <= 0) maxIter = 256;
    for (int i = 0; i < maxIter; ++i) {
        vec3 pos = origin + dir * t;
        float d = sceneDE(pos);
        if (d < ubo.uDetail || t > 40.0) break;
        t += d;
        progress = float(i) / float(maxIter);
    }
    return origin + dir * t;
}