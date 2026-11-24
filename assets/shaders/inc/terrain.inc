// ---------------------------------------------------------------------------
//  Procedural Terrain (ubo.uFractalType == 4)
//  fBm heightfield + ridged rocks + trees + rocks + clouds + fog
// ---------------------------------------------------------------------------

// Groove field (cracks in rocky areas)
float grooveField(vec2 xz) {
    vec2 p = xz * ubo.uTerrainFrequency;
    return pow(smoothstep(0.58, 0.94, vnoise(p * 6.3 + 53.7)), 2.0);
}

// Terrain height
float terrainH(vec2 xz) {
    vec2 p = xz * ubo.uTerrainFrequency;

    // Domain warping for rocks blended into large forms
    vec2 warp = vec2(fbm2(p + vec2(13.7, 9.2), 2),
                     fbm2(p + vec2(5.1, 21.3), 2)) - 0.5;
    vec2 wp = p + warp * 0.9;

    float base = fbm2(wp * 0.55, 4);
    float ridge = ridge2(wp * 1.3 + 7.7, 3);
    float h = mix(base, ridge, smoothstep(0.62, 0.85, base));

    float detail = fbm2(p * 4.8 + 3.1, 1);
    h += (detail - 0.5) * 0.10;

    // Cracks only on rocky elevations
    h -= grooveField(xz) * 0.065 * smoothstep(0.50, 0.75, base);

    return h * ubo.uTerrainAmplitude;
}

// Terrain normal (finite differences)
vec3 terrainNormal(vec2 xz) {
    const float e = 0.03;
    float hL = terrainH(xz - vec2(e, 0.0));
    float hR = terrainH(xz + vec2(e, 0.0));
    float hD = terrainH(xz - vec2(0.0, e));
    float hU = terrainH(xz + vec2(0.0, e));
    return normalize(vec3(hL - hR, 2.0 * e, hD - hU));
}

// Triplanar rock texture
vec2 rockTriplanar(vec3 p, vec3 n, float scale) {
    vec3 w = abs(n);
    w = w * w * w * w;
    w /= max(w.x + w.y + w.z, 1e-4);
    vec2 px = p.zy, py = p.xz, pz = p.xy;
    float g = fbm2(px * scale + 21.3, 2) * w.x +
              fbm2(py * scale + 5.7, 2) * w.y +
              fbm2(pz * scale + 41.1, 2) * w.z;
    float rv = ridge2(px * scale * 1.7 + 7.0, 1) * w.x +
               ridge2(py * scale * 1.7 + 3.0, 1) * w.y +
               ridge2(pz * scale * 1.7 + 9.0, 1) * w.z;
    float crack = smoothstep(0.40, 0.49, rv);
    return vec2(g, crack);
}

// Terrain color: sand -> grass -> rock -> snow
vec3 terrainColor(vec3 pos, vec3 n, float h, float slope, float dist) {
    vec2 xz = pos.xz;
    float nt = daylightLevel();
    vec3 grass = vec3(0.22, 0.36, 0.13);
    vec3 rock  = vec3(0.30, 0.27, 0.24);
    vec3 snow  = vec3(0.82, 0.87, 0.93);
    vec3 sand  = vec3(0.62, 0.55, 0.36);
    float hn = h / max(ubo.uTerrainAmplitude, 0.01);

    vec3 col = mix(sand, grass, smoothstep(0.08, 0.30, hn));
    float veg = fbm2(xz * 1.1 + 7.7, 1);
    col = mix(col * vec3(0.80, 0.94, 0.72), col, veg);

    float rockMask = smoothstep(0.38, 0.70, slope + hn * 0.18 -
                                (fbm2(xz * 0.75 + 41.0, 2) - 0.5) * 0.10);
    vec2 rt = rockTriplanar(pos, n, 1.9);
    vec3 rockTex = rock * (0.62 + 0.85 * rt.x);
    rockTex = mix(rockTex, rockTex * vec3(0.55, 0.53, 0.52), rt.y * 0.7);
    col = mix(col, rockTex, rockMask);

    float snowMask = smoothstep(0.72, 0.92, hn - slope * 0.12 +
                                (fbm2(xz * 0.45 + 11.0, 2) - 0.5) * 0.08);
    col = mix(col, snow, snowMask);

    col *= 1.0 - grooveField(xz) * (0.18 + 0.24 * rockMask);

    float blotch = 0.90 + 0.20 * (fbm2(xz * 1.6 + 89.1, 2) - 0.5);
    float grain = 0.92 + 0.16 * (fbm2(xz * 7.3 + 13.7, 1) - 0.5);
    col *= blotch * grain;
    col *= mix(0.35, 1.0, nt);
    return col;
}

// Tree specification per grid cell
struct TreeSpec {
    vec2 base;
    float h;
    float width;
    float tiers;
    float phase;
};

bool treeSpec(vec2 c, out TreeSpec t) {
    if (hash12(c) > ubo.uTreeDensity) return false;
    t.base = c + 0.5 + (vec2(hash12(c + 71.3), hash12(c + 43.1)) - 0.5) * 0.4;
    t.h = 1.5 + 1.5 * hash12(c + 19.7);
    t.width = t.h * (0.14 + 0.05 * hash12(c + 11.3));
    t.tiers = 3.0 + 1.6 * hash12(c + 31.7);
    t.phase = hash12(c + 55.9) * 0.5;
    return true;
}

// Tree SDF
float treesDE(vec3 p, float ground) {
    if (ubo.uTreeDensity <= 0.0) return 1e9;
    if (ground > ubo.uTerrainAmplitude * 0.45) return 1e9;

    float res = 1e9;
    vec2 cell = floor(p.xz);
    float y = p.y - ground;
    if (y < -0.1 || y > 3.2) return 1e9;

    TreeSpec t;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 c = cell + vec2(float(i), float(j));
            if (!treeSpec(c, t)) continue;
            vec2 rel = p.xz - t.base;
            float r2 = dot(rel, rel);
            if (r2 > 0.68) continue;

            float lr = sqrt(r2) * (1.0 + 0.15 * 2.0 * rel.x * rel.y / max(r2, 1e-4));
            float y0 = t.h * 0.30;
            res = min(res, max(lr - 0.05, y - y0 * 1.15));

            if (y > y0) {
                float tt = (y - y0) / max(t.h - y0, 0.2);
                float w = 1.0 - abs(fract(tt * t.tiers + t.phase) * 2.0 - 1.0);
                float R = t.width * (1.0 - tt) * (0.74 + 0.40 * w) + 0.02;
                res = min(res, max(lr - R, y - t.h));
            }
        }
    }
    return res;
}

// Tree normal
vec3 treeNormalAt(vec3 p, float ground) {
    const float e = 0.03;
    return normalize(vec3(
        treesDE(p + vec3(e, 0.0, 0.0), ground) - treesDE(p - vec3(e, 0.0, 0.0), ground),
        treesDE(p + vec3(0.0, e, 0.0), ground) - treesDE(p - vec3(0.0, e, 0.0), ground),
        treesDE(p + vec3(0.0, 0.0, e), ground) - treesDE(p - vec3(0.0, 0.0, e), ground)));
}

// Rock SDF
float rocksDE(vec3 p, float ground, out vec3 rockN) {
    rockN = vec3(0.0, 1.0, 0.0);
    const float g = 0.9;
    const float maxReach = 0.68;
    float res = 1e9;

    float steep = smoothstep(0.45, 0.85, ground / max(ubo.uTerrainAmplitude, 0.01));
    float prob = 0.10 + 0.5 * steep;
    vec2 cell = floor(p.xz / g);

    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 c = cell + vec2(float(i), float(j));
            float hc = hash12(c * 1.37 + 17.7);
            if (hc > prob) continue;
            vec2 base = (c + 0.5 +
                         (vec2(hash12(c + 71.3), hash12(c + 43.1)) - 0.5) * 0.55) * g;
            vec2 d2 = p.xz - base;
            if (dot(d2, d2) > maxReach * maxReach) continue;

            float size = (0.08 + 0.24 * hash12(c + 5.7)) * (0.75 + 0.55 * steep);
            float reach = size * 1.35 + 0.08;
            if (dot(d2, d2) > reach * reach) continue;

            vec3 q = p - vec3(base.x, ground + size * 0.15, base.y);
            vec3 rad = vec3(size, size * 0.72,
                            size * (0.72 + 0.5 * hash12(c + 9.1)));
            float dd = min(rad.x, min(rad.y, rad.z)) * (length(q / rad) - 1.0) +
                       (vnoise3(q * 5.5 + c.x) - 0.5) * 0.08 * size;
            if (dd < res) {
                res = dd;
                vec3 gr = q / (rad * rad);
                rockN = dot(gr, gr) > 1e-12 ? normalize(gr) : vec3(0.0, 1.0, 0.0);
            }
        }
    }
    return res;
}

// Rock normal with terrain slope correction
vec3 rockNormalAt(vec3 hit) {
    float gh = terrainH(hit.xz);
    vec3 gr;
    rocksDE(hit, gh, gr);
    const float e = 0.03;
    vec2 gradH = vec2(terrainH(hit.xz + vec2(e, 0.0)) - terrainH(hit.xz - vec2(e, 0.0)),
                      terrainH(hit.xz + vec2(0.0, e)) - terrainH(hit.xz - vec2(0.0, e))) /
                 (2.0 * e);
    return normalize(gr - gr.y * vec3(gradH.x, 0.0, gradH.y));
}

// Combined landscape SDF (terrain + trees + rocks)
float mapLandscape(vec3 p) {
    float gh = terrainH(p.xz);
    float dT = p.y - gh;
    if (dT < -0.8) return dT;
    float d = dT;
    if (dT < 3.35 && ubo.uTreeDensity > 0.0) d = min(d, treesDE(p, gh) * 0.75);
    if (dT < 2.5) {
        vec3 rn;
        d = min(d, rocksDE(p, gh, rn) * 0.85);
    }
    return d;
}

// March landscape with bisection on sign change
float marchLandscape(vec3 ro, vec3 rd, float maxDist, out int mat) {
    mat = 0;
    float t = 0.0;
    float previousT = 0.0;
    const float stepMin = 0.06;
    float hitT = -1.0;
    int steps = max(ubo.uMaxSteps, 24);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapLandscape(p);
        if (d < 0.0) {
            float lo = previousT;
            float hi = t;
            for (int j = 0; j < 4; ++j) {
                float mid = 0.5 * (lo + hi);
                if (mapLandscape(ro + rd * mid) < 0.0) hi = mid;
                else lo = mid;
            }
            hitT = 0.5 * (lo + hi);
            break;
        }
        if (d < 0.015 + 0.0008 * t) { hitT = t; break; }
        previousT = t;
        t += clamp(d, stepMin, 2.0);
        if (t > maxDist) break;
    }
    if (hitT < 0.0) return -1.0;

    vec3 hit = ro + rd * hitT;
    float gh = terrainH(hit.xz);
    vec3 rn;
    float dR = rocksDE(hit, gh, rn);
    float dTr = treesDE(hit, gh);
    float aR = abs(dR), aT = abs(dTr), aG = abs(hit.y - gh);
    mat = (aR <= aG && aR <= aT) ? 2 : ((aT <= aG) ? 1 : 0);
    return hitT;
}

// Shadows
float sceneShadow(vec3 p, vec3 sunDir_) {
    float t = 0.14;
    float res = 1.0;
    for (int i = 0; i < 14; ++i) {
        vec3 q = p + sunDir_ * t;
        float d = mapLandscape(q);
        if (d < 0.0) return 0.0;
        res = min(res, 4.0 * d / t);
        if (res < 0.01) return 0.0;
        t += clamp(d, 0.08, 1.2);
        if (t > 24.0) break;
    }
    return res;
}

// Ambient occlusion
float terrainAO(vec3 pos, vec3 nor, float camDist) {
    int taps = 5;
    if (camDist > 18.0) taps = 3;
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        if (i >= taps) break;
        float h = 0.06 + 0.2775 * float(i);
        float d = mapLandscape(pos + nor * h);
        occ += max(h - d, 0.0) * sca;
        sca *= 0.72;
        if (occ > 0.3) break;
    }
    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}

// Render terrain entry point
vec3 renderTerrain(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float dusk = duskLevel();
    vec3 sunTint = sunTintRGB(dusk) * nt;
    vec3 coolAmb = coolAmbient(dusk);
    vec3 skyTint = skyTintRGB(dusk);

    int mat;
    float t = marchLandscape(ro, rd, 90.0, mat);

    vec3 skyCol = skyGradient(rd, sun, nt, vec3(0.55, 0.72, 0.95),
                              vec3(0.25, 0.45, 0.85));
    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0) {
        col = mix(col, cloudShade(nt), cloudA);
    }
    if (t < 0.0) return grade(col);

    vec3 hit = ro + rd * t;

    if (mat == 2) {
        // Rock
        vec3 rn = rockNormalAt(hit);
        float rdiff = clamp(dot(rn, sun), 0.0, 1.0);
        float rsh = sceneShadow(hit + rn * 0.08, sun);
        float ramp = 0.18 + 0.14 * clamp(rn.y, 0.0, 1.0);
        float rsky = 0.25 + 0.45 * clamp(rn.y, 0.0, 1.0);
        vec2 rt = rockTriplanar(hit, rn, 3.2);
        vec3 rcol = mix(vec3(0.24, 0.22, 0.20),
                        vec3(0.50, 0.47, 0.43), clamp(rt.x * 1.4, 0.0, 1.0));
        rcol = mix(rcol, rcol * vec3(0.52, 0.50, 0.48), rt.y * 0.8);
        float rSnow = smoothstep(0.72, 0.92, hit.y / max(ubo.uTerrainAmplitude, 0.01)) *
                      smoothstep(0.55, 0.85, rn.y);
        rcol = mix(rcol, vec3(0.82, 0.87, 0.93), rSnow);

        vec3 rlight = rcol * ramp * coolAmb * mix(0.45, 1.0, nt) +
                      rcol * (rdiff * sunTint + 0.5 * rsky * skyTint * nt) * rsh;

        vec3 rH = normalize(sun - rd);
        float rVH = clamp(dot(-rd, rH), 0.0, 1.0);
        float rFres = 0.04 + 0.96 * pow(1.0 - rVH, 5.0);
        float rWet = mix(1.0 - smoothstep(0.15, 0.60,
                                          hit.y / max(ubo.uTerrainAmplitude, 0.01)),
                         0.60, rSnow);
        float rShin = mix(36.0, 140.0, rSnow);
        rlight += sunTint * pow(clamp(dot(rn, rH), 0.0, 1.0), rShin) *
                  rFres * rWet * (0.35 + 0.65 * rsh) * (0.55 + 0.45 * nt);
        rlight += rcol * rimLight(rn, rd, sun, rsh, 4.0, 0.0, dusk) *
                  (0.55 * nt);
        rlight = applyFog(rlight, rd, sun, nt, t);
        return grade(rlight);
    }

    if (mat == 1) {
        // Tree
        float gh = terrainH(hit.xz);
        vec3 tn = treeNormalAt(hit, gh);
        float tdiff = clamp(dot(tn, sun), 0.0, 1.0);
        float tsh = sceneShadow(hit + tn * 0.10, sun);

        vec2 cell = floor(hit.xz);
        TreeSpec bt = TreeSpec(hit.xz, 2.0, 0.35, 4.0, 0.0);
        float bD2 = 1e9;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                TreeSpec ts;
                if (!treeSpec(cell + vec2(float(i), float(j)), ts)) continue;
                float d2 = dot(hit.xz - ts.base, hit.xz - ts.base);
                if (d2 < bD2) { bD2 = d2; bt = ts; }
            }
        }
        float ty = hit.y - gh;
        float tlr = length(hit.xz - bt.base);
        float y0 = bt.h * 0.30;
        float trunkF = (1.0 - smoothstep(0.05, 0.085, tlr)) *
                       (1.0 - smoothstep(y0 * 0.85, y0 * 1.2, ty));
        float needle = clamp(vnoise3(hit * 3.0) * 1.4 - 0.1, 0.0, 1.0);
        vec3 foliage = mix(vec3(0.030, 0.115, 0.040),
                           vec3(0.085, 0.230, 0.075), needle);
        foliage *= mix(0.62, 1.0, clamp(ty / max(bt.h, 0.5), 0.0, 1.0));
        vec3 tcol = mix(foliage, vec3(0.115, 0.075, 0.040), trunkF);
        vec3 tlight = tcol * (0.18 + 0.10 * clamp(tn.y, 0.0, 1.0) +
                              0.78 * tdiff * tsh);
        tlight = applyFog(tlight, rd, sun, nt, t);
        return grade(tlight);
    }

    // Terrain
    vec3 n = terrainNormal(hit.xz);
    float slope = 1.0 - clamp(n.y, 0.0, 1.0);
    float diff = clamp(dot(n, sun), 0.0, 1.0);
    float sh = sceneShadow(hit + n * 0.05, sun);

    float ao = 1.0;
    if (t < 28.0) {
        ao = mix(1.0, terrainAO(hit + n * 0.05, n, t), smoothstep(28.0, 18.0, t));
    }
    float ambient = 0.16 + 0.12 * clamp(n.y, 0.0, 1.0);
    float sky = 0.25 + 0.45 * clamp(n.y, 0.0, 1.0);
    vec3 base = terrainColor(hit, n, hit.y, slope, t);
    float aoSun = mix(1.0, ao, 0.45);
    vec3 light = base * (ambient * ao + (diff * aoSun + 0.50 * sky * ao) * sh);
    light *= clamp(0.86 + 0.14 * n.y, 0.0, 1.0);
    light = applyFog(light, rd, sun, nt, t);
    return grade(light);
}