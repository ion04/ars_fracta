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

uniform int   uFractalType;   // 0=2D, 1=Mandelbulb, 2=Menger, 3=Julia, 4=Terrain, 5=Coast
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

// параметры пейзажа (uFractalType == 4)
uniform float uTerrainAmplitude;  // высота рельефа / суши
uniform float uTerrainFrequency;  // масштаб шума
uniform float uCloudDensity;      // плотность облаков
uniform float uTreeDensity;       // плотность деревьев
uniform float uTimeOfDay;         // 0..1: положение солнца над горизонтом

// параметры побережья (uFractalType == 5): центр плоскости Мандельброта,
// где проходит линия берега
uniform vec2  uCoastCenter;

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
//  Процедурный пейзаж (uFractalType == 4): fBm-рельеф + фрактальные скалы,
//  облака и лес. Рельеф задаётся аналитической высотой h(x,z), скалы —
//  фрактальными гребнями (ridged noise), дерево — SDF-конусом.
// ---------------------------------------------------------------------------

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
    float a = 0.5f;
    float f = 1.0f;
    float s = 0.0f;
    for (int i = 0; i < oct; ++i) {
        s += a * vnoise(p * f);
        f *= 2.03f;
        a *= 0.5f;
    }
    return s;
}

// фрактальные гребни: |noise| у окантовок -> острые хребты скал
float ridge2(vec2 p, int oct) {
    float a = 0.5f;
    float f = 1.0f;
    float s = 0.0f;
    for (int i = 0; i < oct; ++i) {
        float n = vnoise(p * f);
        s += a * (1.0f - abs(2.0f * n - 1.0f));
        f *= 2.07f;
        a *= 0.5f;
    }
    return s;
}

float terrainH(vec2 xz) {
    vec2 p = xz * uTerrainFrequency;

    // крупная форма рельефа + гребни (скалы вплетены домен-варпингом)
    vec2 warp = vec2(fbm2(p + vec2(13.7, 9.2), 2),
                     fbm2(p + vec2(5.1, 21.3), 2)) - 0.5f;
    vec2 wp = p + warp * 0.9f;

    float base = fbm2(wp * 0.55f, 4);            // холмы и равнины
    float ridge = ridge2(wp * 1.3f + 7.7f, 3);   // фрактальные хребты
    // скалы доминируют там, где крупная форма поднята
    float h = mix(base, ridge, smoothstep(0.62f, 0.85f, base));

    float detail = fbm2(p * 4.8f + 3.1f, 1);
    h += (detail - 0.5f) * 0.10f;                 // мелкие камни
    return h * uTerrainAmplitude;                 // [0, amp]
}

float mapTerrain(vec3 p) {
    return p.y - terrainH(p.xz);
}

//  пересечение луча с terrain: возвращает t (или -1 если промах)
float marchTerrain(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.0f;
    const float stepMin = 0.12f;
    int steps = max(uMaxSteps, 24);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapTerrain(p);
        if (d < 0.0f) return t;                 // нос под землёй
        t += clamp(d, stepMin, 2.0f);
        if (t > maxDist) break;
    }
    return -1.0f;
}

//  тень от солнца: короткий марш от точки к источнику света
float terrainShadow(vec3 p, vec3 sunDir_) {
    float t = 0.14f;
    float res = 1.0f;
    for (int i = 0; i < 14; ++i) {
        vec3 q = p + sunDir_ * t;
        float d = mapTerrain(q);
        if (d < 0.0f) return 0.0f;
        res = min(res, 4.0f * d / t);
        t += clamp(d, 0.08f, 1.2f);
        if (t > 24.0f) break;
    }
    return res;
}

//  нормаль к террейну (метод конечных разностей по высоте)
vec3 terrainNormal(vec2 xz) {
    const float e = 0.03f;
    float hL = terrainH(xz - vec2(e, 0.0f));
    float hR = terrainH(xz + vec2(e, 0.0f));
    float hD = terrainH(xz - vec2(0.0f, e));
    float hU = terrainH(xz + vec2(0.0f, e));
    return normalize(vec3(hL - hR, 2.0f * e, hD - hU));
}

//  солнечное направление из времени суток uTimeOfDay (0..1)
vec3 sunDir() {
    float ang = (uTimeOfDay - 0.25f) * 3.14159265f * 2.0f;
    return normalize(vec3(0.5f, 0.35f + 0.5f * sin(ang), -0.55f));
}

//  0..1: дневная освещённость (0 — ночь, 1 — день)
float daylightLevel() {
    float el = 0.35f + 0.5f * sin((uTimeOfDay - 0.25f) * 6.2831853f);
    return smoothstep(-0.10f, 0.25f, el);
}

//  цвет террейна по высоте/уклону: песок -> трава -> камень -> снег
vec3 terrainColor(float h, float slope, float dist) {
    // ночной оттенок по времени суток (высота солнца -> ночь при el<0)
    float nt = daylightLevel();
    vec3 grass = vec3(0.22f, 0.36f, 0.13f);
    vec3 rock = vec3(0.30f, 0.27f, 0.24f);
    vec3 snow = vec3(0.82f, 0.87f, 0.93f);
    vec3 sand = vec3(0.62f, 0.55f, 0.36f);
    // пороги каменных зон и снеговой линии в долях амплитуды рельефа,
    // чтобы внешний вид не зависел от абсолютной высоты
    float hn = h / max(uTerrainAmplitude, 0.01f);
    float g = 1.0f - smoothstep(0.55f, 0.75f, 1.0f - slope); // внизу, полого => трава
    vec3 col = mix(sand, grass, g);
    col = mix(col, rock, smoothstep(0.35f, 0.55f, 1.0f - slope) * smoothstep(0.06f, 0.10f, hn));
    col = mix(col, snow, smoothstep(0.105f, 0.16f, hn));
    col *= mix(0.35f, 1.0f, nt);                   // night dimming
    col *= mix(0.55f, 1.0f, 1.0f / (1.0f + dist * 0.06f)); // далёкий туман
    return col;
}

//  дерево: SDF конуса (ствол+крона). p — локальная точка, offset y0 — база.
float treeSDF(vec3 p, float baseY, float h) {
    // буфер: позиции деревьев хешируются в ячейках сетки; здесь рисуем одну ель
    float y = p.y - baseY;
    if (y < 0.0f || y > h) return 1e9;
    float r = 0.28f * (1.0f - y / h);             // крона: конус к вершине
    float dSl = length(vec2(length(p.xz), y + 0.0f)) - r; // ~side
    // простое приближение: расстояние до оси с радиусом по высоте
    float dAxis = length(p.xz) - r;
    return max(dAxis, -y);                        // палка (спрайт-замена стvora)
}

//  проверить деревья: объекты в сетке 1x1 вокруг xz; вернуть расстояние
float treesDE(vec3 p) {
    if (uTreeDensity <= 0.0f) return 1e9;
    // деревья ставятся в ячейках сетки (hash -> смещение внутри)
    float res = 1e9;
    vec2 cell = floor(p.xz);
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 c = cell + vec2(float(i), float(j));
            if (hash12(c) > uTreeDensity) continue;
            // база дерева в ячейке, высота ели
            vec2 base = c + 0.5f + (vec2(hash12(c + 71.3f), hash12(c + 43.1f)) - 0.5f) * 0.4f;
            float hEl = 1.6f + 1.6f * hash12(c + 19.7f);
            // ель не растёт на крутых склонах и в высоких горах
            float ground = terrainH(base);
            if (ground > uTerrainAmplitude * 0.45f) continue;
            vec3 rel = vec3(p.x - base.x, p.y, p.z - base.y);
            float y = rel.y - ground;
            if (y < -0.1f) continue;
            // радиус конуса по высоте
            float r = 0.5f * (1.0f - y / hEl);
            if (y <= hEl) {
                res = min(res, length(vec2(length(rel.xz), y)) - r * 1.3f);
            }
            // ствол
            res = min(res, length(rel.xz) - 0.07f);
        }
    }
    return res;
}

//  облачный слой: быстрое пересечение луча с плоскостью облаков
float cloudAlpha(vec3 ro, vec3 rd, float hitT) {
    if (uCloudDensity <= 0.0f) return 0.0f;
    float yLayer = uTerrainAmplitude * 1.25f + 2.0f;
    if (rd.y <= 0.0f) return 0.0f;
    float tC = (yLayer - ro.y) / rd.y;
    if (tC < 0.0f || (hitT > 0.0f && tC > hitT)) return 0.0f; // под землёй/за горами
    vec2 px = (ro + rd * tC).xz;
    vec2 q = px * 0.06f + vec2(uTime * 0.01f, 0.0f);
    float n = fbm2(q, 4);
    float cover = smoothstep(0.42f, 0.78f, n);    // прореживание
    float density = smoothstep(0.45f, 0.75f, n) * uCloudDensity;
    // тень/подсветка облака
    return clamp(cover * density * 0.9f, 0.0f, 0.95f);
}

vec3 renderTerrain(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float t = marchTerrain(ro, rd, 90.0f);

    vec3 skyCol;
    {
        // градиент неба; ночью гаснет до тёмно-синего
        float hUp = rd.y * 0.5f + 0.5f;
        vec3 horizon = vec3(0.55f, 0.72f, 0.95f);
        vec3 zenith = vec3(0.25f, 0.45f, 0.85f);
        skyCol = mix(horizon, zenith, pow(hUp, 0.55f));
        skyCol *= mix(0.12f, 1.0f, nt);            // ночное небо темнеет
        // солнечный диск и закатная подсветка (только днём)
        float sunHit = clamp(dot(rd, sun), 0.0f, 1.0f);
        skyCol += vec3(1.0f, 0.88f, 0.6f) * pow(sunHit, 220.0f) * 2.2f * nt;
        skyCol += vec3(1.0f, 0.6f, 0.3f) * pow(sunHit, 8.0f) * 0.35f * nt;
    }

    // небо (или далёкий горизонт), затем облака
    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0f) {
        col = mix(col, vec3(0.95f, 0.96f, 0.98f) * (0.25f + 0.75f * nt), cloudA);
    }

    if (t < 0.0f) {
        return col;                     // чистое небо
    }

    vec3 hit = ro + rd * t;
    vec3 n = terrainNormal(hit.xz);
    // низкая точка => возможны деревья: проверяем SDF ели
    float dTree = treesDE(hit + n * 0.02f);
    if (dTree < 0.05f) {
        // ель: тёмная хвоя, затенённая
        float shade = clamp(dot(vec3(0.0f, 1.0f, 0.0f), sun), 0.0f, 1.0f);
        return vec3(0.06f, 0.16f, 0.05f) * (0.4f + 0.6f * shade);
    }
    if (dTree < 0.6f) {
        // стvол/край кроны
        return mix(vec3(0.05f, 0.10f, 0.04f), vec3(0.16f, 0.13f, 0.08f),
                   smoothstep(0.0f, 0.6f, dTree));
    }

    float diff = clamp(dot(n, sun), 0.0f, 1.0f);
    float sh = terrainShadow(hit + n * 0.05f, sun);
    float ambient = 0.16f + 0.12f * clamp(rd.y, -1.0f, 0.0f);
    // мягкий свет от неба и отражение "склона"
    float sky = 0.25f + 0.45f * clamp(n.y, 0.0f, 1.0f);
    float slope = length(vec2(dFdx(hit.y), dFdy(hit.y)));
    vec3 base = terrainColor(hit.y, slope, t);
    vec3 light = base * (ambient + (diff + 0.35f * sky) * sh);
    light = mix(light, col, clamp((t - 55.0f) * 0.045f, 0.0f, 0.8f)); // air perspective
    return light;
}

// ---------------------------------------------------------------------------
//  Морское побережье (uFractalType == 5): линия берега повторяет границу
//  множества Мандельброта. Суша — внутренность множества, море — внешность,
//  «пляж» — узкая полоса внешнего расстояния D около границы.
// ---------------------------------------------------------------------------

//  отображение мировых координат в комплексную плоскость: world (0,0) -> центр
vec2 coastC(vec2 xz) {
    float scale = 0.15f / max(uM2dZoom, 0.01f);
    return uCoastCenter + xz * scale;
}

//  быстрые тесты внутренности множества (главная кардиоида и период-2 бульба)
//  — покрывают почти всю «сушу» без дорогого итерационного цикла
bool coastInteriorFast(vec2 c) {
    float x = c.x - 0.25f;
    float q = x * x + c.y * c.y;
    bool cardioid = q * (q + x) < c.y * c.y * 0.25f;
    vec2 b = c + vec2(1.0f, 0.0f);
    bool bulb = b.x * b.x + b.y * b.y < 0.0625f;
    return cardioid || bulb;
}

//  x = доля суши (0..1), y = внешнее расстояние до границы в мировых единицах
vec2 coastField(vec2 xz) {
    vec2 c = coastC(xz);
    if (coastInteriorFast(c)) return vec2(1.0f, 0.0f);

    vec2 z = vec2(0.0f);
    vec2 dz = vec2(0.0f);
    int it = uIterations;
    float bail = max(uBailout, 2.0f);
    bool escaped = false;
    for (int i = 0; i < uIterations; ++i) {
        dz = 2.0f * vec2(z.x * dz.x - z.y * dz.y, z.x * dz.y + z.y * dz.x) +
             vec2(1.0f, 0.0f);
        z = vec2(z.x * z.x - z.y * z.y, 2.0f * z.x * z.y) + c;
        if (dot(z, z) > bail * bail) { it = i; escaped = true; break; }
    }
    if (!escaped) return vec2(1.0f, 0.0f);

    float mz = sqrt(dot(z, z));
    float mdz = max(sqrt(dot(dz, dz)), 1e-6f);
    float dCom = mz * log(mz) / mdz;                 // exterior distance (complex)
    float dW = dCom * max(uM2dZoom, 0.01f) / 0.15f;  // те же единицы, что xz

    //  плавный счётчик итераций: на границе si -> uIterations, в открытом море — мал
    float si = float(it) + 1.0f - log2(log2(mz) / log2(uBailout));
    float land = smoothstep(float(uIterations - 4), float(uIterations), si);
    return vec2(land, dW);
}

//  высота суши: холмы внутри множества, спадающие к берегу
float coastHeight(vec2 xz) {
    float land = coastField(xz).x;
    float hills = 0.30f + 0.70f * fbm2(xz * 0.13f + 17.3f, 3);
    return land * uTerrainAmplitude * hills;
}

//  волны на воде (мелкая модуляция поверхности, не пересекается с сушей)
float waveHeight(vec2 xz) {
    vec2 q = xz * 0.20f + vec2(uTime * 0.05f, 0.0f);
    float a = (fbm2(q, 2) - 0.5f) * 0.30f;
    vec2 s = xz * 0.8f + vec2(0.0f, uTime * 0.07f);
    a += 0.4f * (sin(s.x) + cos(s.y)) * 0.12f;
    return a;
}

//  высота видимой поверхности в точке xz (суша или уровень волн)
float coastSurface(vec2 xz) {
    float h = coastHeight(xz);
    if (h < 0.3f) h += waveHeight(xz);
    return h;
}

float mapCoast(vec3 p) {
    return min(p.y - coastHeight(p.xz), p.y);   // суша сверху, вода на y=0
}

float marchCoast(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.0f;
    const float stepMin = 0.10f;
    int steps = max(uMaxSteps, 30);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapCoast(p);
        if (d < 0.0f) return t;
        t += clamp(d, stepMin, 2.5f);
        if (t > maxDist) break;
    }
    return -1.0f;
}

vec3 coastSurfNormal(vec2 xz) {
    const float e = 0.06f;
    float hL = coastSurface(xz - vec2(e, 0.0f));
    float hR = coastSurface(xz + vec2(e, 0.0f));
    float hD = coastSurface(xz - vec2(0.0f, e));
    float hU = coastSurface(xz + vec2(0.0f, e));
    return normalize(vec3(hL - hR, 2.0f * e, hD - hU));
}

vec3 renderCoast(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float t = marchCoast(ro, rd, 110.0f);

    // небо (морской горизонт) — ночью гаснет, как и в пейзаже
    vec3 skyCol;
    {
        float hUp = rd.y * 0.5f + 0.5f;
        vec3 horizon = vec3(0.49f, 0.68f, 0.85f);
        vec3 zenith = vec3(0.22f, 0.44f, 0.80f);
        skyCol = mix(horizon, zenith, pow(hUp, 0.55f));
        skyCol *= mix(0.12f, 1.0f, nt);
        float sunHit = clamp(dot(rd, sun), 0.0f, 1.0f);
        skyCol += vec3(1.0f, 0.88f, 0.6f) * pow(sunHit, 220.0f) * 2.2f * nt;
        skyCol += vec3(1.0f, 0.6f, 0.3f) * pow(sunHit, 8.0f) * 0.35f * nt;
    }

    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0f) {
        col = mix(col, vec3(0.95f, 0.96f, 0.98f) * (0.25f + 0.75f * nt), cloudA);
    }
    if (t < 0.0f) return col;          // чистое небо

    vec3 hit = ro + rd * t;
    vec3 n = coastSurfNormal(hit.xz);
    vec2 cf = coastField(hit.xz);
    float land = cf.x;

    if (land > 0.5f) {
        // --- суша: песчаный пляж у воды -> зелень -> скалы на вершинах
        float diff = clamp(dot(n, sun), 0.0f, 1.0f);
        float sky = 0.28f + 0.42f * clamp(n.y, 0.0f, 1.0f);
        float slope = length(vec2(dFdx(hit.y), dFdy(hit.y)));
        float hn = hit.y / max(uTerrainAmplitude, 0.01f);
        vec3 sand = vec3(0.76f, 0.68f, 0.45f);
        vec3 green = vec3(0.21f, 0.35f, 0.12f);
        vec3 rock = vec3(0.30f, 0.27f, 0.24f);
        vec3 base = mix(sand, green, smoothstep(0.0f, 0.35f, hn) * (1.0f - slope));
        base = mix(base, rock, smoothstep(0.62f, 0.85f, hn));
        base *= mix(0.40f, 1.0f, nt);
        col = base * (0.16f + 0.90f * (0.45f * diff + 0.55f * sky));
        col = mix(col, skyCol, clamp((t - 70.0f) * 0.05f, 0.0f, 0.7f)); // морская дымка
    } else {
        // --- вода: глубокое море -> бирюзовое мелководье у берега
        float dw = cf.y;
        float shore = exp(-max(dw, 0.0f) * 5.0f);
        vec3 deep = vec3(0.02f, 0.10f, 0.18f);
        vec3 shallow = vec3(0.10f, 0.46f, 0.42f);
        vec3 w = mix(deep, shallow, shore);
        w *= (0.45f + 0.5f * nt);
        // блик солнца на волнах + френель-отражение неба
        float v = clamp(dot(n, -rd), 0.0f, 1.0f);
        float fres = pow(1.0f - v, 3.0f);
        float spec = pow(clamp(dot(reflect(-sun, n), -rd), 0.0f, 1.0f), 90.0f);
        col = mix(w, skyCol, fres * 0.85f);
        col += vec3(1.0f, 0.95f, 0.8f) * spec * 0.8f * nt;
    }
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

    // процедурный пейзаж
    if (uFractalType == 4) {
        FragColor = vec4(renderTerrain(uCamPos, dir), 1.0);
        return;
    }

    // морское побережье (граница множества Мандельброта)
    if (uFractalType == 5) {
        FragColor = vec4(renderCoast(uCamPos, dir), 1.0);
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