// ---------------------------------------------------------------------------
//  Coast3D (ubo.uFractalType == 5): Mandelbrot boundary as coastline
// ---------------------------------------------------------------------------

vec2 coastC(vec2 xz) {
    float scale = 0.15 / max(ubo.uM2dZoom, 0.01);
    return ubo.uCoastCenter + xz * scale;
}

bool coastInteriorFast(vec2 c) {
    float x = c.x - 0.25;
    float q = x * x + c.y * c.y;
    bool cardioid = q * (q + x) < c.y * c.y * 0.25;
    vec2 b = c + vec2(1.0, 0.0);
    bool bulb = b.x * b.x + b.y * b.y < 0.0625;
    return cardioid || bulb;
}

vec2 coastField(vec2 xz) {
    vec2 c = coastC(xz);
    if (coastInteriorFast(c)) return vec2(1.0, 0.0);

    vec2 z = vec2(0.0);
    vec2 dz = vec2(0.0);
    int it = ubo.uIterations;
    float bail = max(ubo.uBailout, 2.0);
    bool escaped = false;
    for (int i = 0; i < ubo.uIterations; ++i) {
        dz = 2.0 * vec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) +
             vec2(1.0, 0.0);
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (dot(z, z) > bail * bail) { it = i; escaped = true; break; }
    }
    if (!escaped) return vec2(1.0, 0.0);

    float mz = sqrt(dot(z, z));
    float mdz = max(sqrt(dot(dz, dz)), 1e-6);
    float dCom = mz * log(mz) / mdz;
    float dW = dCom * max(ubo.uM2dZoom, 0.01) / 0.15;

    float si = float(it) + 1.0 - log2(log2(mz) / log2(ubo.uBailout));
    float land = smoothstep(float(ubo.uIterations - 18), float(ubo.uIterations), si);
    return vec2(land, dW);
}

float coastHeight(vec2 xz) {
    vec2 f = coastField(xz);
    float land = f.x;
    float dW = f.y;
    float hills = 0.30 + 0.70 * fbm2(xz * 0.13 + 17.3, 3);
    float shore = exp(-max(dW, 0.0) * 1.4);
    float berm = mix(0.02, 0.18, shore);
    float profile = mix(berm, 1.0, smoothstep(0.05, 0.55, land));
    return ubo.uTerrainAmplitude * hills * profile;
}

vec2 gerstnerWaves(vec2 xz) {
    const vec2 d0 = vec2(0.962, 0.279);
    const vec2 d1 = vec2(-0.751, 0.661);
    const vec2 d2 = vec2(0.310, -0.951);
    float t = ubo.uTime;

    vec2 s = xz;
    for (int k = 0; k < 2; ++k) {
        float c0 = cos(0.262 * dot(s, d0) - t * 0.47);
        float c1 = cos(0.785 * dot(s, d1) - t * 0.79);
        float c2 = cos(1.848 * dot(s, d2) - t * 1.16);
        s = xz - (d0 * (0.330 * c0) + d1 * (0.195 * c1) + d2 * (0.090 * c2));
    }

    float p0 = 0.262 * dot(s, d0) - t * 0.47;
    float p1 = 0.785 * dot(s, d1) - t * 0.79;
    float p2 = 1.848 * dot(s, d2) - t * 1.16;
    float h = 0.150 * sin(p0) + 0.075 * sin(p1) + 0.030 * sin(p2);
    float crest = (0.086 * sin(p0) + 0.153 * sin(p1) + 0.166 * sin(p2)) / 0.405;
    return vec2(h, crest);
}

float waveHeight(vec2 xz) {
    return gerstnerWaves(xz).x;
}

float caustics(vec2 xz, float t) {
    vec2 q = xz * 2.2;
    vec2 warp = vec2(vnoise(q * 0.5 + vec2(3.1, 0.0) + t * 0.30),
                     vnoise(q * 0.5 + vec2(0.0, 7.4) - t * 0.26)) - 0.5;
    q += warp * 2.6;
    float f = 0.5 + 0.125 * (cos(q.x + t * 0.9) +
                             cos(q.y * 1.07 - t * 0.8) +
                             cos((q.x + q.y) * 0.8 + t * 1.15) +
                             cos((q.x - q.y) * 0.9 - t));
    f = clamp(f, 0.0, 1.0);
    float f2 = f * f;
    return f2 * f2 * f2 * f2;
}

float coastSurface(vec2 xz) {
    float h = coastHeight(xz);
    if (h < 0.3) h += waveHeight(xz);
    return h;
}

float mapCoast(vec3 p) {
    return min(p.y - coastHeight(p.xz), p.y - waveHeight(p.xz));
}

float marchCoast(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.0;
    float previousT = 0.0;
    const float stepMin = 0.06;
    int steps = max(ubo.uMaxSteps, 30);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapCoast(p);
        if (d < 0.0) {
            float lo = previousT;
            float hi = t;
            for (int j = 0; j < 6; ++j) {
                float mid = 0.5 * (lo + hi);
                if (mapCoast(ro + rd * mid) < 0.0) hi = mid;
                else lo = mid;
            }
            return 0.5 * (lo + hi);
        }
        previousT = t;
        t += clamp(d, stepMin, 2.5);
        if (t > maxDist) break;
    }
    return -1.0;
}

vec3 coastSurfNormal(vec2 xz) {
    float e = 0.06 / max(sqrt(max(ubo.uM2dZoom, 0.01)), 1.0);
    float hL = coastSurface(xz - vec2(e, 0.0));
    float hR = coastSurface(xz + vec2(e, 0.0));
    float hD = coastSurface(xz - vec2(0.0, e));
    float hU = coastSurface(xz + vec2(0.0, e));
    return normalize(vec3(hL - hR, 2.0 * e, hD - hU));
}

float coastShadow(vec3 p, vec3 sunDir_) {
    float t = 0.14;
    float res = 1.0;
    for (int i = 0; i < 10; ++i) {
        vec3 q = p + sunDir_ * t;
        float d = q.y - coastHeight(q.xz);
        if (d < 0.0) return 0.0;
        res = min(res, 4.0 * d / t);
        t += clamp(d, 0.08, 1.2);
        if (t > 28.0) break;
    }
    return res;
}

vec3 renderCoast(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float t = marchCoast(ro, rd, 110.0);

    vec3 skyCol = skyGradient(rd, sun, nt, vec3(0.49, 0.68, 0.85),
                              vec3(0.22, 0.44, 0.80));
    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0) {
        col = mix(col, cloudShade(nt), cloudA);
    }
    if (t < 0.0) return gradeCoast(col);

    vec3 hit = ro + rd * t;
    vec3 n = coastSurfNormal(hit.xz);
    vec2 cf = coastField(hit.xz);
    float land = cf.x;
    float shoreGlow = exp(-max(cf.y, 0.0) * 1.4);

    if (land > 0.12 || shoreGlow > 0.30) {
        // Land
        float diff = clamp(dot(n, sun), 0.0, 1.0);
        float sh = coastShadow(hit + n * 0.05, sun);
        float sky = 0.30 + 0.45 * clamp(n.y, 0.0, 1.0);
        float ambient = 0.20 + 0.14 * clamp(n.y, 0.0, 1.0);
        float slope = 1.0 - clamp(n.y, 0.0, 1.0);
        float hn = hit.y / max(ubo.uTerrainAmplitude, 0.01);
        vec3 wetSand = vec3(0.48, 0.39, 0.23);
        vec3 sand = vec3(0.76, 0.68, 0.45);
        vec3 green = vec3(0.18, 0.37, 0.14);
        vec3 rock = vec3(0.31, 0.30, 0.29);
        float nearShore = exp(-max(cf.y, 0.0) * 2.2);
        float shoreBand = mix(smoothstep(0.0, 0.10, hn),
                              smoothstep(0.0, 0.24, hn), nearShore);
        vec3 base = mix(wetSand, sand, shoreBand);
        float vegetation = smoothstep(0.13, 0.42, hn) *
                           (1.0 - smoothstep(0.34, 0.68, slope));
        vegetation *= 0.80 - 0.55 * nearShore;
        vegetation *= 0.75 + 0.35 * fbm2(hit.xz * 0.65 + 23.4, 2);
        base = mix(base, green, vegetation);
        float rockMask = smoothstep(0.48, 0.76, slope + hn * 0.28 -
                                    (fbm2(hit.xz * 0.70 + 4.2, 2) - 0.5) * 0.12);
        base = mix(base, rock, rockMask);
        float blotch = 0.90 + 0.20 * (fbm2(hit.xz * 1.6 + 89.1, 2) - 0.5);
        float grain = 0.92 + 0.16 * (fbm2(hit.xz * 7.3 + 13.7, 1) - 0.5);
        base *= blotch * grain;
        base *= mix(0.40, 1.0, nt);
        col = base * (ambient + (diff + 0.50 * sky) * sh);
        col = applyFog(col, rd, sun, nt, t);
        return gradeCoast(col);
    } else {
        // Water
        float dw = cf.y;
        float shore = exp(-max(dw, 0.0) * 3.2);
        vec3 deep = vec3(0.025, 0.12, 0.25);
        vec3 shallow = vec3(0.10, 0.50, 0.46);
        vec3 w = mix(deep, shallow, shore);
        w *= (0.55 + 0.55 * nt);

        float caust = caustics(hit.xz + n.xz * (0.5 + dw * 0.7), ubo.uTime);
        float caustBand = exp(-max(dw, 0.0) * 2.0);
        w += vec3(0.50, 0.85, 0.72) * caust * caustBand * (0.35 + 0.60 * nt);

        vec2 gw = gerstnerWaves(hit.xz);
        float crest = gw.y;
        float foamNoise = fbm2(hit.xz * 1.6 +
                               vec2(-ubo.uTime * 0.11, ubo.uTime * 0.08), 2);
        float crestFoam = smoothstep(0.45, 0.80, crest) *
                          smoothstep(0.30, 0.62, foamNoise);
        float wash = smoothstep(1.40, 0.10, dw) *
                     (0.40 + 0.60 * smoothstep(-0.50, 0.55, crest)) *
                     (0.50 + 0.50 * smoothstep(0.22, 0.55, foamNoise));
        float foam = clamp(crestFoam * 0.85 + wash, 0.0, 1.0);

        float v = clamp(dot(n, -rd), 0.0, 1.0);
        float x = 1.0 - v;
        float x2 = x * x;
        float fres = 0.02 + 0.98 * x2 * x2 * x;
        float spec = clamp(dot(reflect(-sun, n), -rd), 0.0, 1.0);

        vec3 rdir = reflect(rd, n);
        rdir.y = abs(rdir.y);
        vec3 reflSky = skyGradient(rdir, sun, nt, vec3(0.49, 0.68, 0.85),
                                   vec3(0.22, 0.44, 0.80));
        reflSky = mix(reflSky, cloudShade(nt),
                      cloudAlpha(hit + n * 0.05, rdir, -1.0));
        col = mix(w, reflSky, fres);

        float sssB = clamp(dot(rd, sun) * 0.5 + 0.5, 0.0, 1.0);
        float sss = sssB * sssB * sssB * sssB *
                    clamp(crest * 0.5 + 0.65, 0.0, 1.0) * (0.35 + 0.65 * shore);
        col += vec3(0.05, 0.35, 0.30) * sss * nt * 0.55;

        col += vec3(1.0, 0.95, 0.8) * pow(spec, 90.0) * 0.8 * nt;
        col += vec3(1.0, 0.90, 0.75) * pow(spec, 6.0) * 0.10 * nt *
               (0.6 + 0.8 * fbm2(hit.xz * 1.4, 1));
        vec3 foamCol = vec3(0.92, 0.96, 0.98) * (0.40 + 0.75 * nt);
        col = mix(col, foamCol, clamp(foam * (0.70 + 0.30 * fres), 0.0, 1.0));
        col = applyFog(col, rd, sun, nt, t);
    }
    return gradeCoast(col);
}