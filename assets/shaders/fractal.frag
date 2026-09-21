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
uniform float uFogDensity;        // плотность тумана (воздушная перспектива)

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
//  фрактальными гребнями (ridged noise), дерево — ярусной SDF-елью (конусная
//  огибающая, нарезанная на юбки + голый ствол), валуны —
//  SDF-эллипсоидами в сетке. Деревья и валуны входят в ОСНОВНОЙ марш
//  (mapLandscape) как отдельные SDF-объекты, поэтому это объёмные тела со
//  своим силуэтом на горизонте, собственными нормалями и тенями. Каменная
//  текстура накладывается tri-planar проекцией, в крутых склонах прорезаны
//  мелкие расщелины.
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

//  3D-хеш и value-шум: нужны для огранки валунов (rocksDE)
float hash13(vec3 p) {
    p = fract(p * 0.10301f);
    p += dot(p, p.yzx + 33.33f);
    return fract((p.x + p.y) * p.z);
}

float vnoise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0f - 2.0f * f);
    float a = mix(mix(hash13(i), hash13(i + vec3(1.0f, 0.0f, 0.0f)), u.x),
                  mix(hash13(i + vec3(0.0f, 1.0f, 0.0f)),
                      hash13(i + vec3(1.0f, 1.0f, 0.0f)), u.x), u.y);
    float b = mix(mix(hash13(i + vec3(0.0f, 0.0f, 1.0f)),
                      hash13(i + vec3(1.0f, 0.0f, 1.0f)), u.x),
                  mix(hash13(i + vec3(0.0f, 1.0f, 1.0f)),
                      hash13(i + vec3(1.0f, 1.0f, 1.0f)), u.x), u.y);
    return mix(a, b, u.z);
}

//  Поле расщелин: узкий жёлоб по изолинии шума (0..1). Используется и в
//  высоте (вычитание канавок), и в цвете (затемнение глубины), поэтому
//  вынесено в отдельную функцию; один вызов vnoise — terrainH работает
//  на каждом шаге марша и в бисекции, лишние октавы здесь дорого стоят.
float grooveField(vec2 xz) {
    vec2 p = xz * uTerrainFrequency;
    return pow(smoothstep(0.58f, 0.94f, vnoise(p * 6.3f + 53.7f)), 2.0f);
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

    // мелкие расщелины: жёлоба прорезают только каменистые поднятия —
    // в низинах рельеф остаётся сглаженным
    h -= grooveField(xz) * 0.065f * smoothstep(0.50f, 0.75f, base);

    return h * uTerrainAmplitude;                 // [0, amp]
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
    // солнце ВПЕРЕДИ камеры: светит на склоны террейна И побережья,
    // даёт солнечную дорожку на море. Пейзаж и побережье оба смотрят +z.
    return normalize(vec3(0.45f, 0.42f + 0.58f * sin(ang), 0.72f));
}

//  0..1: дневная освещённость (0 — ночь, 1 — день)
float daylightLevel() {
    float el = 0.35f + 0.5f * sin((uTimeOfDay - 0.25f) * 6.2831853f);
    return smoothstep(-0.10f, 0.25f, el);
}

//  0..1: «закатность» — низкое солнце красит горизонт и облака в тёплый
float duskLevel() {
    float el = 0.35f + 0.5f * sin((uTimeOfDay - 0.25f) * 6.2831853f);
    return 1.0f - smoothstep(0.02f, 0.32f, el);
}

//  Трипланарная процедурная текстура скал: fBm/ridge-шум проецируется на
//  3 плоскости осей и смешивается весами |n|^4 -> нет растяжения на крутых
//  склонах (в отличие от плоской xz-проекции). Возврат: .x = светотеневой
//  рисунок 0..1, .y = тёмные трещины-линии 0..1.
vec2 rockTriplanar(vec3 p, vec3 n, float scale) {
    vec3 w = abs(n);
    w = w * w * w * w;
    w /= max(w.x + w.y + w.z, 1e-4f);
    vec2 px = p.zy, py = p.xz, pz = p.xy;
    float g = fbm2(px * scale + 21.3f, 2) * w.x +
              fbm2(py * scale +  5.7f, 2) * w.y +
              fbm2(pz * scale + 41.1f, 2) * w.z;           // 0..1, пятна/рельеф
    float rv = ridge2(px * scale * 1.7f + 7.0f, 1) * w.x +
               ridge2(py * scale * 1.7f + 3.0f, 1) * w.y +
               ridge2(pz * scale * 1.7f + 9.0f, 1) * w.z;  // 0..0.5
    float crack = smoothstep(0.40f, 0.49f, rv);            // узкие гребни -> трещины
    return vec2(g, crack);
}

//  цвет террейна: песок -> трава -> камень -> снег
//  slope = 1-n.y: 0=плоско, 1=обрыв. hn = h/amp: 0=низина, 1=пик.
//  pos/n — мировая точка и нормаль: нужны для трипланарной текстуры скал.
vec3 terrainColor(vec3 pos, vec3 n, float h, float slope, float dist) {
    vec2 xz = pos.xz;
    float nt = daylightLevel();
    vec3 grass = vec3(0.22f, 0.36f, 0.13f);
    vec3 rock  = vec3(0.30f, 0.27f, 0.24f);
    vec3 snow  = vec3(0.82f, 0.87f, 0.93f);
    vec3 sand  = vec3(0.62f, 0.55f, 0.36f);
    float hn = h / max(uTerrainAmplitude, 0.01f);
    // низины и пологие участки -> песок, slightly higher -> трава
    vec3 col = mix(sand, grass, smoothstep(0.08f, 0.30f, hn));
    // живые пятна: прохладные луга и тёплые поля чередуются
    float veg = fbm2(xz * 1.1f + 7.7f, 1);
    col = mix(col * vec3(0.80f, 0.94f, 0.72f), col, veg);
    // Rock emerges gradually on steep, elevated faces; noise breaks up bands.
    float rockMask = smoothstep(0.38f, 0.70f, slope + hn * 0.18f -
                                (fbm2(xz * 0.75f + 41.0f, 2) - 0.5f) * 0.10f);
    // скалистые поверхности получают трипланарную фактуру вместо плоской заливки
    vec2 rt = rockTriplanar(pos, n, 1.9f);
    vec3 rockTex = rock * (0.62f + 0.85f * rt.x);
    rockTex = mix(rockTex, rockTex * vec3(0.55f, 0.53f, 0.52f), rt.y * 0.7f);
    col = mix(col, rockTex, rockMask);
    // Snow is limited to genuinely high ground and softens on exposed rock.
    float snowMask = smoothstep(0.72f, 0.92f, hn - slope * 0.12f +
                                (fbm2(xz * 0.45f + 11.0f, 2) - 0.5f) * 0.08f);
    col = mix(col, snow, snowMask);
    // расщелины затемнены (саевые тени в канавках), сильнее всего на скалах
    col *= 1.0f - grooveField(xz) * (0.18f + 0.24f * rockMask);
    // микро-текстура: крупные пятна травы/камня + мелкий гранул, чтобы не было
    // плоской заливки цвета
    float blotch = 0.90f + 0.20f * (fbm2(xz * 1.6f + 89.1f, 2) - 0.5f);
    float grain = 0.92f + 0.16f * (fbm2(xz * 7.3f + 13.7f, 1) - 0.5f);
    col *= blotch * grain;
    col *= mix(0.35f, 1.0f, nt);
    return col;
}

//  Параметры одного дерева в ячейке сетки 1x1. Все размеры — доли от h,
//  поэтому максимальная высота кроны = 3.0 и вертикальные границы слоя
//  марша (3.2 / 3.35) не ломаются. hash-параметры ячейки задают вариативность
//  (рост, толщина, число ярусов, сдвиг рисунка), а сами значения переис-
//  пользуются в SDF и в раскраске, чтобы геометрия и цвет соответствовали
//  одному и тому же дереву.
struct TreeSpec {
    vec2 base;    // основание ствола (xz)
    float h;      // полная высота ели
    float width;  // максимальный радиус нижней юбки кроны
    float tiers;  // число ярусов-«этажей» кроны
    float phase;  // сдвиг рисунка ярусов по высоте
};

bool treeSpec(vec2 c, out TreeSpec t) {
    if (hash12(c) > uTreeDensity) return false;
    t.base = c + 0.5f + (vec2(hash12(c + 71.3f), hash12(c + 43.1f)) - 0.5f) * 0.4f;
    t.h = 1.5f + 1.5f * hash12(c + 19.7f);                 // 1.5..3.0
    t.width = t.h * (0.14f + 0.05f * hash12(c + 11.3f));   // 0.14..0.19 h
    t.tiers = 3.0f + 1.6f * hash12(c + 31.7f);             // 3.0..4.6
    t.phase = hash12(c + 55.9f) * 0.5f;
    return true;
}

//  Хвойное дерево в сетке 1x1 вокруг xz: голый цилиндрический ствол ниже
//  нижней юбки + конусная огибающая кроны, нарезанная треугольной волной на
//  ярусы: радиус плавно убывает к верхушке, но внутри каждого яруса делает
//  выступ наружу и врез у границы яруса — силуэт рваный, еловый, а не гладкий
//  конус-«вантуз». Четырёхлопастная угловая модуляция (без atan2) снимает
//  идеальную круглую форму сечения. ground — высота террейна под точкой p;
//  передаётся из mapLandscape, чтобы на каждый шаг марша не звать terrainH
//  повторно. База дерева привязана к local ground, поэтому ель «сидит» на
//  склоне и имеет настоящий объёмный силуэт.
float treesDE(vec3 p, float ground) {
    if (uTreeDensity <= 0.0f) return 1e9f;
    // ель не растёт в высоких горах (проверка по ground — один раз на вызов)
    if (ground > uTerrainAmplitude * 0.45f) return 1e9f;
    float res = 1e9f;
    vec2 cell = floor(p.xz);
    float y = p.y - ground;
    if (y < -0.1f || y > 3.2f) return 1e9f;    // вне вертикального охвата елей
    TreeSpec t;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 c = cell + vec2(float(i), float(j));
            if (!treeSpec(c, t)) continue;
            vec2 rel = p.xz - t.base;
            // боковой охват: максимальный радиус юбки 0.19*3.0*1.14+0.02 ~ 0.67,
            // с учётом угловой модуляции (+15%) — отсекаем ячейку до sqrt
            float r2 = dot(rel, rel);
            if (r2 > 0.68f) continue;
            // 4-лопастная модуляция: |2xz|/r^2 = |sin 2theta| <= 1 — сечение
            // перестаёт быть окружностью, дерево выглядит живым
            float lr = sqrt(r2) * (1.0f + 0.15f * 2.0f * rel.x * rel.y / max(r2, 1e-4f));
            // ствол: виден ниже нижней юбки и не торчит из макушки
            float y0 = t.h * 0.30f;                        // низ первой юбки
            res = min(res, max(lr - 0.05f, y - y0 * 1.15f));
            // крона: конусная огибающая (радиус -> 0 к верхушке), помноженная
            // на треугольную волну по высоте -> ярусы-юбки с резкими кромками
            if (y > y0) {
                float tt = (y - y0) / max(t.h - y0, 0.2f);     // 0..1 по кроне
                float w = 1.0f - abs(fract(tt * t.tiers + t.phase) * 2.0f - 1.0f);
                float R = t.width * (1.0f - tt) * (0.74f + 0.40f * w) + 0.02f;
                // cap по верхушке: выше h кусок кроны положителен всегда
                res = min(res, max(lr - R, y - t.h));
            }
        }
    }
    return res;
}

//  Поле валунов: SDF-эллипсоиды в сетке 0.9, вполовину встроенные в склон.
//  Включено в основной марш (mapLandscape), поэтому валун — объёмное тело:
//  свой силуэт на горизонте, аналитическая нормаль (out-параметр, чтобы не
//  делать 6 вызовов SDF на нормаль) и собственная тень. Огранка — модуляция
//  радиуса 3D-шумом. ground — высота террейна под точкой p (берётся из
//  mapLandscape бесплатно): центр валуна следует за склоняющейся землёй,
//  форма остаётся корректной уровнем множества, а аналитическая нормаль
//  дополняется градиентом высоты при закрашивании (rockNormalAt).
//  На крутизну склона вместо настоящего градиента (4 terrainH на шаг марша —
//  слишком дорого) указывает абсолютная высота: в этом рельефе хребты и
//  есть скальные зоны.
float rocksDE(vec3 p, float ground, out vec3 rockN) {
    rockN = vec3(0.0f, 1.0f, 0.0f);
    const float g = 0.9f;
    const float maxReach = 0.68f;             // (0.08+0.24)*1.3*1.35 + 0.08
    float res = 1e9f;
    // на склонах и в скалистых зонах камней заметно больше, на равнине — редкие
    float steep = smoothstep(0.45f, 0.85f, ground / max(uTerrainAmplitude, 0.01f));
    float prob = 0.10f + 0.5f * steep;
    vec2 cell = floor(p.xz / g);
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            vec2 c = cell + vec2(float(i), float(j));
            float hc = hash12(c * 1.37f + 17.7f);
            if (hc > prob) continue;
            vec2 base = (c + 0.5f +
                         (vec2(hash12(c + 71.3f), hash12(c + 43.1f)) - 0.5f) * 0.55f) * g;
            vec2 d2 = p.xz - base;
            if (dot(d2, d2) > maxReach * maxReach) continue;  // дешёвый ранний отсев
            float size = (0.08f + 0.24f * hash12(c + 5.7f)) * (0.75f + 0.55f * steep);
            float reach = size * 1.35f + 0.08f;
            if (dot(d2, d2) > reach * reach) continue;
            // центр приспущен к земле: эллипсоид вполовину встроен в склон
            vec3 q = p - vec3(base.x, ground + size * 0.15f, base.y);
            vec3 rad = vec3(size, size * 0.72f,
                            size * (0.72f + 0.5f * hash12(c + 9.1f)));
            float dd = min(rad.x, min(rad.y, rad.z)) * (length(q / rad) - 1.0f) +
                       (vnoise3(q * 5.5f + c.x) - 0.5f) * 0.08f * size;
            if (dd < res) {
                res = dd;
                vec3 gr = q / (rad * rad);
                rockN = dot(gr, gr) > 1e-12f ? normalize(gr) : vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }
    return res;
}

//  Аналитическая нормаль валуна с поправкой на наклон земли: поверхность
//  задана уровнем q.y = p.y - terrainH(p.xz) - c, значит её нормаль —
//  градиент с учётом dh/dxz (поправка -gr.y * gradH).
vec3 rockNormalAt(vec3 hit) {
    float gh = terrainH(hit.xz);
    vec3 gr;
    rocksDE(hit, gh, gr);
    const float e = 0.03f;
    vec2 gradH = vec2(terrainH(hit.xz + vec2(e, 0.0f)) - terrainH(hit.xz - vec2(e, 0.0f)),
                      terrainH(hit.xz + vec2(0.0f, e)) - terrainH(hit.xz - vec2(0.0f, e))) /
                 (2.0f * e);
    return normalize(gr - gr.y * vec3(gradH.x, 0.0f, gradH.y));
}

//  Полное поле пейзажа (uFractalType == 4): рельеф + ели + валуны. Объекты
//  живут в тонком приземном слое (ель ≤ ~3.3 м над local ground, валун ≤
//  ~0.9 м), поэтому выше слоя DE равен расстоянию до террейна и марш в
//  воздухе остаётся таким же дешёвым, как был. SDF объектов домножаются на
//  коэффициент < 1 (конусы и шумовая огранка — грубые DE, иначе марш их
//  проскакивает).
float mapLandscape(vec3 p) {
    float gh = terrainH(p.xz);
    float dT = p.y - gh;
    if (dT < -0.8f) return dT;                // глубоко под землёй
    float d = dT;
    // ель ≤ 3.2 над высотой земли под точкой p — строгая граница
    if (dT < 3.35f && uTreeDensity > 0.0f) d = min(d, treesDE(p, gh) * 0.75f);
    if (dT < 2.5f) {                          // валун + перепад base/column ground
        vec3 rn;
        d = min(d, rocksDE(p, gh, rn) * 0.85f);
    }
    return d;
}

//  Нормаль дерева: градиент treesDE (6 вызовов SDF, ground фиксирован —
//  на шаге конечных разностей высота террейна не успевает измениться).
vec3 treeNormalAt(vec3 p, float ground) {
    const float e = 0.03f;
    return normalize(vec3(
        treesDE(p + vec3(e, 0.0f, 0.0f), ground) - treesDE(p - vec3(e, 0.0f, 0.0f), ground),
        treesDE(p + vec3(0.0f, e, 0.0f), ground) - treesDE(p - vec3(0.0f, e, 0.0f), ground),
        treesDE(p + vec3(0.0f, 0.0f, e), ground) - treesDE(p - vec3(0.0f, 0.0f, e), ground)));
}

//  Марш по полному полю пейзажа. Возвращает t пересечения (или -1) и в mat —
//  материал ближайшей поверхности: 0=террейн, 1=дерево, 2=камень. Допуск
//  остановки растёт с дистанцией, компенсируя огрубение шага вдали.
float marchLandscape(vec3 ro, vec3 rd, float maxDist, out int mat) {
    mat = 0;
    float t = 0.0f;
    float previousT = 0.0f;
    const float stepMin = 0.06f;
    float hitT = -1.0f;
    int steps = max(uMaxSteps, 24);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapLandscape(p);
        if (d < 0.0f) {
            // Перелёт: высота террейна — не точный SDF. Уточняем интервал
            // смены знака комбинированного поля (границей может оказаться
            // и поверхность камня/ели).
            float lo = previousT;
            float hi = t;
            for (int j = 0; j < 4; ++j) {
                float mid = 0.5f * (lo + hi);
                if (mapLandscape(ro + rd * mid) < 0.0f) hi = mid;
                else lo = mid;
            }
            hitT = 0.5f * (lo + hi);
            break;
        }
        if (d < 0.015f + 0.0008f * t) { hitT = t; break; }
        previousT = t;
        t += clamp(d, stepMin, 2.0f);
        if (t > maxDist) break;
    }
    if (hitT < 0.0f) return -1.0f;

    // классификация: ближайшая (по модулю расстояния) поверхность к точке
    // остановки — она и видна в этом пикселе
    vec3 hit = ro + rd * hitT;
    float gh = terrainH(hit.xz);
    vec3 rn;
    float dR = rocksDE(hit, gh, rn);
    float dTr = treesDE(hit, gh);
    float aR = abs(dR), aT = abs(dTr), aG = abs(hit.y - gh);
    mat = (aR <= aG && aR <= aT) ? 2 : ((aT <= aG) ? 1 : 0);
    return hitT;
}

//  тень от солнца: короткий марш от точки к источнику света ПОЛНЫМ полем —
//  ели и валуны отбрасывают тени на рельеф и друг на друга
float sceneShadow(vec3 p, vec3 sunDir_) {
    float t = 0.14f;
    float res = 1.0f;
    for (int i = 0; i < 14; ++i) {
        vec3 q = p + sunDir_ * t;
        float d = mapLandscape(q);
        if (d < 0.0f) return 0.0f;
        res = min(res, 4.0f * d / t);
        if (res < 0.01f) return 0.0f;         // уже в полной тени — дальше не считаем
        t += clamp(d, 0.08f, 1.2f);
        if (t > 24.0f) break;
    }
    return res;
}

//  Ambient occlusion (задача 2.2): короткий марш ВДОЛЬ нормали полным полем
//  mapLandscape (рельеф + ели + валуны). Классическая оценка IQ
//  (nvscene2008): на каждом шаге сравниваем ожидаемое расстояние h с фактическим
//  значением DE; экранирующие поверхности (стенки расщелины, соседний валун,
//  нижняя юбка ели над головой) дают d << h -> накопление occ с затухающим
//  весом. 5 отсчётов вдоль одной нормали (0.06..1.17 — достаёт до нижней
//  юбки кроны ~0.45..1.2 над землёй) вместо 32 по полусфере: в heightfield-
//  сцене этого хватает для читаемых контактных теней. Бюджет: за 18 модулей
//  camera-дистанции остаются только 3 первых отсчёта (эффект уже субпиксельный,
//  а весь AO вдали гасится микшированием), ранний выход по occ докупает
//  ещё ~30% экономии в тёмных зонах. Возврат 0..1 (1 = открытое небо).
float terrainAO(vec3 pos, vec3 nor, float camDist) {
    int taps = 5;
    if (camDist > 18.0f) taps = 3;
    float occ = 0.0f;
    float sca = 1.0f;
    for (int i = 0; i < 5; ++i) {
        if (i >= taps) break;
        float h = 0.06f + 0.2775f * float(i);        // 0.06..1.17 вдоль нормали
        float d = mapLandscape(pos + nor * h);
        occ += max(h - d, 0.0f) * sca;               // только экранирование (d < h)
        sca *= 0.72f;
        if (occ > 0.3f) break;                       // дальше AO всё равно в нуле
    }
    return clamp(1.0f - 3.0f * occ, 0.0f, 1.0f);
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

//  цвет неба: градиент + диск солнца + гало + закатный горизонт
vec3 skyGradient(vec3 rd, vec3 sun, float nt, vec3 horizon, vec3 zenith) {
    float hUp = rd.y * 0.5f + 0.5f;
    float dusk = duskLevel();
    // низкое солнце красит горизонт в тёплый, зенит — в сине-фиолетовый
    vec3 hor = mix(horizon, vec3(1.0f, 0.62f, 0.38f), dusk * 0.85f);
    vec3 zen = mix(zenith, vec3(0.45f, 0.32f, 0.55f), dusk * 0.55f);
    vec3 col = mix(hor, zen, pow(hUp, 0.55f));
    col *= mix(0.12f, 1.0f, nt);
    float sunHit = clamp(dot(rd, sun), 0.0f, 1.0f);
    col += vec3(1.0f, 0.88f, 0.6f) * pow(sunHit, 220.0f) * 2.2f * nt;   // диск
    col += vec3(1.0f, 0.60f, 0.30f) * pow(sunHit, 7.0f) *
           (0.40f + 0.9f * dusk) * nt;                                    // гало
    return col;
}

//  экспоненциальная воздушная перспектива: дальние участки тонут в дымке
vec3 applyFog(vec3 col, vec3 rd, vec3 sun, float nt, float dist) {
    float sunHit = clamp(dot(rd, sun), 0.0f, 1.0f);
    float dusk = duskLevel();
    vec3 fog = vec3(0.62f, 0.71f, 0.87f);
    fog += vec3(1.0f, 0.70f, 0.45f) * pow(sunHit, 5.0f) * 0.35f * nt;
    fog = mix(fog, vec3(1.0f, 0.55f, 0.35f), dusk * 0.70f);
    //  плотность тумана настраивается (uFogDensity); clamp по альфе держит
    //  дальние хребты читаемыми: туман не вымывает их в цвет неба полностью
    float a = 1.0f - exp(-dist * uFogDensity);
    return mix(col, fog, clamp(a, 0.0f, 0.58f));
}

//  цвет облаков: светлая масса, на закате — тёплая
vec3 cloudShade(float nt) {
    float dusk = duskLevel();
    vec3 c = vec3(0.95f, 0.96f, 0.98f) * (0.25f + 0.75f * nt);
    return mix(c, vec3(1.0f, 0.82f, 0.68f), dusk * 0.75f);
}

//  мягкая цветокоррекция: лёгкое повышение насыщенности
vec3 grade(vec3 c) {
    return mix(vec3(dot(c, vec3(0.299f, 0.587f, 0.114f))), c, 1.12f);
}

//  коррекция для побережья: лёгкое повышение насыщенности и яркости
//  (тёмное море и зелень иначе «вянут» на 8-битном экране)
vec3 gradeCoast(vec3 c) {
    return mix(vec3(dot(c, vec3(0.299f, 0.587f, 0.114f))), c, 1.15f) * 1.10f;
}

// ---------------------------------------------------------------------------
//  Освещение пейзажа (задача 2.2) — только свет, геометрию не трогаем:
//  1) тёплый прямой свет / холодная ambient-подсветка неба (color grading):
//     диффузная составляющая тонзируется цветом солнца (на закате —
//     оранжевым), ambient и небесная подсветка — холодным синим.
//     Освещённые склоны и тени оказываются в разных цветовых температурах
//     -> объём читается.
//  2) rim/backlight (контур): скольжение нормали к лучу (1-N·V)^k, включено
//     когда солнце ЗА поверхностью (dot(sun,rd)>0) — светящийся контур хребтов,
//     крон и валунов на фоне неба; для елей усиливает «просвет» хвои.
//  3) specular влажных камней и снега: Blinn-Schlick (F0 диэлектрика 0.04 +
//     0.96(1-V·H)^5), блик сужается на гладком льду снега и шире на камне.
// ---------------------------------------------------------------------------

//  цвет прямой солнечной составляющей: нейтрально-тёплый днём, оранжевый в dusk
vec3 sunTintRGB(float dusk) {
    return mix(vec3(1.10f, 0.98f, 0.86f), vec3(1.15f, 0.62f, 0.34f), dusk * 0.85f);
}

//  холодный ambient (заполняющий свет неба у земли)
vec3 coolAmbient(float dusk) {
    return mix(vec3(0.70f, 0.80f, 1.05f), vec3(0.62f, 0.66f, 0.92f), dusk * 0.70f);
}

//  небесная подсветка (sky term) — чуть голубее ambient, на закате теплее
vec3 skyTintRGB(float dusk) {
    return mix(vec3(0.55f, 0.70f, 1.05f), vec3(0.85f, 0.70f, 0.85f), dusk * 0.60f);
}

//  контровая подсветка: k — острота контура, boost — яркость хвои на просвет.
//  Работает только в backlight-геометрии (солнце за поверхностью относительно
//  камеры) и гасится в собственной тени объекта, но не до нуля: часть контура
//  освещена небом.
vec3 rimLight(vec3 n, vec3 rd, vec3 sun, float shadow, float k, float boost,
              float dusk) {
    float rim = pow(1.0f - clamp(dot(n, -rd), 0.0f, 1.0f), k) *
                clamp(dot(sun, rd), 0.0f, 1.0f);
    return sunTintRGB(dusk) * (rim * (0.35f + 0.65f * shadow) * (1.0f + boost));
}

vec3 renderTerrain(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float dusk = duskLevel();
    vec3 sunTint = sunTintRGB(dusk) * nt;      // прямой свет гаснет вместе с nt
    vec3 coolAmb = coolAmbient(dusk);
    vec3 skyTint = skyTintRGB(dusk);
    int mat;                                  // 0=террейн, 1=дерево, 2=камень
    float t = marchLandscape(ro, rd, 90.0f, mat);

    vec3 skyCol = skyGradient(rd, sun, nt, vec3(0.55f, 0.72f, 0.95f),
                              vec3(0.25f, 0.45f, 0.85f));
    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0f) {
        col = mix(col, cloudShade(nt), cloudA);
    }

    if (t < 0.0f) {
        return grade(col);          // чистое небо (объекты на горизонте уже в марше)
    }

    vec3 hit = ro + rd * t;

    if (mat == 2) {
        // Валун: аналитическая нормаль (с поправкой на наклон земли) и
        // собственная тень — другие валуны/ели тоже ложатся на этот камень.
        vec3 rn = rockNormalAt(hit);
        float rdiff = clamp(dot(rn, sun), 0.0f, 1.0f);
        float rsh = sceneShadow(hit + rn * 0.08f, sun);
        float ramp = 0.18f + 0.14f * clamp(rn.y, 0.0f, 1.0f);
        float rsky = 0.25f + 0.45f * clamp(rn.y, 0.0f, 1.0f);
        vec2 rt = rockTriplanar(hit, rn, 3.2f);   // мелкая фактура на камнях
        vec3 rcol = mix(vec3(0.24f, 0.22f, 0.20f),
                        vec3(0.50f, 0.47f, 0.43f), clamp(rt.x * 1.4f, 0.0f, 1.0f));
        rcol = mix(rcol, rcol * vec3(0.52f, 0.5f, 0.48f), rt.y * 0.8f);
        // вершины камней на высоте пылятся снегом, как у террейна
        float rSnow = smoothstep(0.72f, 0.92f, hit.y / max(uTerrainAmplitude, 0.01f)) *
                      smoothstep(0.55f, 0.85f, rn.y);
        rcol = mix(rcol, vec3(0.82f, 0.87f, 0.93f), rSnow);
        //  освещение вала: ambient — холодный небесный, диффузный — тёплый
        //  солнечный (см. шапку задачи 2.2); величина подобрана так, чтобы
        //  дневная экспозиция совпадала со старой формулой rcol*(ramp+...)
        vec3 rlight = rcol * ramp * coolAmb * mix(0.45f, 1.0f, nt) +
                      rcol * (rdiff * sunTint + 0.5f * rsky * skyTint * nt) * rsh;
        //  влажный камень: низкие валуны в долине блестят, снежная шапка —
        //  ледяной узкой искрой; диэлектрический F0=0.04 + Schlick
        vec3 rH = normalize(sun - rd);
        float rVH = clamp(dot(-rd, rH), 0.0f, 1.0f);
        float rFres = 0.04f + 0.96f * pow(1.0f - rVH, 5.0f);
        float rWet = mix(1.0f - smoothstep(0.15f, 0.60f,
                                           hit.y / max(uTerrainAmplitude, 0.01f)),
                         0.60f, rSnow);
        float rShin = mix(36.0f, 140.0f, rSnow);       // лёд глаже мокрого камня
        rlight += sunTint * pow(clamp(dot(rn, rH), 0.0f, 1.0f), rShin) *
                  rFres * rWet * (0.35f + 0.65f * rsh) * (0.55f + 0.45f * nt);
        //  контур на просвет (rim)
        rlight += rcol * rimLight(rn, rd, sun, rsh, 4.0f, 0.0f, dusk) *
                  (0.55f * nt);
        rlight = applyFog(rlight, rd, sun, nt, t);
        return grade(rlight);
    }

    if (mat == 1) {
        // Хвойное дерево: полная высота (раньше проецировалась на террейн
        // post-hit), своя нормаль, тень от рельефа и соседних объектов.
        float gh = terrainH(hit.xz);
        vec3 tn = treeNormalAt(hit, gh);
        float tdiff = clamp(dot(tn, sun), 0.0f, 1.0f);
        float tsh = sceneShadow(hit + tn * 0.10f, sun);
        // К какому дереву относится точка (тот же обход 3x3 ячеек, что и в
        // treesDE): отличать голый ствол внизу кроны по её геометрии, а не
        // полосами по мировому y (старый trunkMask красил коричневые полосы
        // по всей ели).
        vec2 cell = floor(hit.xz);
        TreeSpec bt = TreeSpec(hit.xz, 2.0f, 0.35f, 4.0f, 0.0f);
        float bD2 = 1e9f;
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
        float y0 = bt.h * 0.30f;
        // ствол — только у оси и только ниже юбок; ниже верхней границы
        // видимого ствола (y0*1.15) маска плавно гаснет
        float trunkF = (1.0f - smoothstep(0.05f, 0.085f, tlr)) *
                       (1.0f - smoothstep(y0 * 0.85f, y0 * 1.2f, ty));
        // хвоя: два тона зелени по 3D-шуму, тёмная внутренняя часть кроны и
        // нижние ярусы (у земли ели всегда гуще и темнее)
        float needle = clamp(vnoise3(hit * 3.0f) * 1.4f - 0.1f, 0.0f, 1.0f);
        vec3 foliage = mix(vec3(0.030f, 0.115f, 0.040f),
                           vec3(0.085f, 0.230f, 0.075f), needle);
        foliage *= mix(0.62f, 1.0f, clamp(ty / max(bt.h, 0.5f), 0.0f, 1.0f));
        vec3 tcol = mix(foliage, vec3(0.115f, 0.075f, 0.040f), trunkF);
        vec3 tlight = tcol * (0.18f + 0.10f * clamp(tn.y, 0.0f, 1.0f) +
                              0.78f * tdiff * tsh);
        tlight = applyFog(tlight, rd, sun, nt, t);
        return grade(tlight);
    }

    vec3 n = terrainNormal(hit.xz);
    float slope = 1.0f - clamp(n.y, 0.0f, 1.0f);
    float diff = clamp(dot(n, sun), 0.0f, 1.0f);
    float sh = sceneShadow(hit + n * 0.05f, sun);
    //  AO (задача 2.2): затемнение в расщелинах, между валунами и под кронами.
    //  Передний план (крупные ели/валуны) лежит на 15..27 модулях, поэтому
    //  полное действие держим до 18, дальше плавно гасим к 28.
    float ao = 1.0f;
    if (t < 28.0f) {
        ao = mix(1.0f, terrainAO(hit + n * 0.05f, n, t), smoothstep(28.0f, 18.0f, t));
    }
    float ambient = 0.16f + 0.12f * clamp(n.y, 0.0f, 1.0f);
    float sky = 0.25f + 0.45f * clamp(n.y, 0.0f, 1.0f);
    vec3 base = terrainColor(hit, n, hit.y, slope, t);
    //  Непрямой свет (ambient и небесная подсветка) гасится AO полностью —
    //  это физический смысл окклюзии; прямой солнечный — лишь наполовину
    //  (солнце — направленный источник, и его экранирование уже считает sh).
    //  На открытых участках ao == 1 и формула совпадает со старой.
    float aoSun = mix(1.0f, ao, 0.45f);
    vec3 light = base * (ambient * ao + (diff * aoSun + 0.50f * sky * ao) * sh);
    // глубинная отдача: крутые ложбины получают меньше неба -> тёмные
    light *= clamp(0.86f + 0.14f * n.y, 0.0f, 1.0f);
    light = applyFog(light, rd, sun, nt, t);
    return grade(light);
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
    //  широкая зона перехода (было 4 итерации): берег получает наклон и
    //  «полку», а не мгновенный обрыв в y = 0
    float land = smoothstep(float(uIterations - 18), float(uIterations), si);
    return vec2(land, dW);
}

//  высота суши: холмы внутри множества, спадающие к берегу.
//  Вдоль границы (dW -> 0) добавляется береговая «полка» — читаемый периметр
//  высоты прямо у воды, иначе побережье выглядело бы плоской кромкой.
float coastHeight(vec2 xz) {
    vec2 f = coastField(xz);
    float land = f.x;
    float dW = f.y;
    float hills = 0.30f + 0.70f * fbm2(xz * 0.13f + 17.3f, 3);
    float shore = exp(-max(dW, 0.0f) * 1.4f);
    float berm = mix(0.02f, 0.18f, shore);
    float profile = mix(berm, 1.0f, smoothstep(0.05f, 0.55f, land));
    return uTerrainAmplitude * hills * profile;
}

//  Волны Герстнера (циклоидальная форма, как в GPU Gems ch.1): сумма трёх
//  независимых волн вместо синусоидальной модуляции. Каждая волна задаётся
//  направлением d, числом волн w = 2π/λ, амплитудой A и крутизной Q; острый
//  гребень достигается горизонтальным смещением точек поверхности к гребням:
//      φ = w·(d·s) − ω·t,   y = Σ A·sinφ,   s = xz − Σ Q·A·d·cosφ.
//  Возвращает (высоту, нормированную «гребневитость» crest ∈ [-1,1]) —
//  crest это якобиан сжатия фронта Σ Q·A·w·sinφ и служит маской пены.
vec2 gerstnerWaves(vec2 xz) {
    //  направления подобраны не сонаправленно: длинная зыбь + поперечная
    //  ветровая волна + короткая рябь (ω = sqrt(g·w) с g уменьшенным
    //  ради спокойного прибойного ритма)
    const vec2 d0 = vec2(0.962f, 0.279f);    // λ ≈ 24, w = 0.262
    const vec2 d1 = vec2(-0.751f, 0.661f);   // λ ≈  8, w = 0.785
    const vec2 d2 = vec2(0.310f, -0.951f);   // λ ≈ 3.4, w = 1.848
    float t = uTime;

    //  обратное горизонтальное смещение — две итерации неподвижной точки
    //  (Σ Q·A·w ≈ 0.41 < 1, циклоиды без петель — сходится быстро)
    vec2 s = xz;
    for (int k = 0; k < 2; ++k) {
        float c0 = cos(0.262f * dot(s, d0) - t * 0.47f);
        float c1 = cos(0.785f * dot(s, d1) - t * 0.79f);
        float c2 = cos(1.848f * dot(s, d2) - t * 1.16f);
        s = xz - (d0 * (0.330f * c0) + d1 * (0.195f * c1) + d2 * (0.090f * c2));
    }

    float p0 = 0.262f * dot(s, d0) - t * 0.47f;
    float p1 = 0.785f * dot(s, d1) - t * 0.79f;
    float p2 = 1.848f * dot(s, d2) - t * 1.16f;
    float h = 0.150f * sin(p0) + 0.075f * sin(p1) + 0.030f * sin(p2);
    float crest = (0.086f * sin(p0) + 0.153f * sin(p1) + 0.166f * sin(p2)) /
                  0.405f;
    return vec2(h, crest);
}

//  волны на воде (высотный профиль Герстнера; суша его не перекрывает)
float waveHeight(vec2 xz) {
    return gerstnerWaves(xz).x;
}

//  подводные каустики: интерференция четырёх плоских «световых волн»,
//  искажённых шумом (аппроксимация преломления света на волнистой
//  поверхности). Дешёвая процедурная сетка ярких линий вместо Worley-патерна.
float caustics(vec2 xz, float t) {
    vec2 q = xz * 2.2f;
    vec2 warp = vec2(vnoise(q * 0.5f + vec2(3.1f, 0.0f) + t * 0.30f),
                     vnoise(q * 0.5f + vec2(0.0f, 7.4f) - t * 0.26f)) - 0.5f;
    q += warp * 2.6f;
    float f = 0.5f + 0.125f * (cos(q.x + t * 0.9f) +
                               cos(q.y * 1.07f - t * 0.8f) +
                               cos((q.x + q.y) * 0.8f + t * 1.15f) +
                               cos((q.x - q.y) * 0.9f - t));
    f = clamp(f, 0.0f, 1.0f);
    float f2 = f * f;
    return f2 * f2 * f2 * f2;   // pow8 — тонкие яркие нити
}

//  высота видимой поверхности в точке xz (суша или уровень волн)
float coastSurface(vec2 xz) {
    float h = coastHeight(xz);
    if (h < 0.3f) h += waveHeight(xz);
    return h;
}

float mapCoast(vec3 p) {
    return min(p.y - coastHeight(p.xz), p.y - waveHeight(p.xz));
}

float marchCoast(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.0f;
    float previousT = 0.0f;
    const float stepMin = 0.06f;
    int steps = max(uMaxSteps, 30);
    for (int i = 0; i < steps; ++i) {
        vec3 p = ro + rd * t;
        float d = mapCoast(p);
        if (d < 0.0f) {
            float lo = previousT;
            float hi = t;
            for (int j = 0; j < 6; ++j) {
                float mid = 0.5f * (lo + hi);
                if (mapCoast(ro + rd * mid) < 0.0f) hi = mid;
                else lo = mid;
            }
            return 0.5f * (lo + hi);
        }
        previousT = t;
        t += clamp(d, stepMin, 2.5f);
        if (t > maxDist) break;
    }
    return -1.0f;
}

vec3 coastSurfNormal(vec2 xz) {
    // шаг сэмплирования масштабируется с zoom-ом: вблизи — мельче, вдали — крупнее
    float e = 0.06f / max(sqrt(max(uM2dZoom, 0.01f)), 1.0f);
    float hL = coastSurface(xz - vec2(e, 0.0f));
    float hR = coastSurface(xz + vec2(e, 0.0f));
    float hD = coastSurface(xz - vec2(0.0f, e));
    float hU = coastSurface(xz + vec2(0.0f, e));
    return normalize(vec3(hL - hR, 2.0f * e, hD - hU));
}

//  тень на суше: короткий марш к солнцу (урезанный бюджет — суша дешевле террейна)
float coastShadow(vec3 p, vec3 sunDir_) {
    float t = 0.14f;
    float res = 1.0f;
    for (int i = 0; i < 10; ++i) {
        vec3 q = p + sunDir_ * t;
        float d = q.y - coastHeight(q.xz);
        if (d < 0.0f) return 0.0f;
        res = min(res, 4.0f * d / t);
        t += clamp(d, 0.08f, 1.2f);
        if (t > 28.0f) break;
    }
    return res;
}

vec3 renderCoast(vec3 ro, vec3 rd) {
    vec3 sun = sunDir();
    float nt = daylightLevel();
    float t = marchCoast(ro, rd, 110.0f);

    vec3 skyCol = skyGradient(rd, sun, nt, vec3(0.49f, 0.68f, 0.85f),
                              vec3(0.22f, 0.44f, 0.80f));
    vec3 col = skyCol;
    float cloudA = cloudAlpha(ro, rd, t);
    if (cloudA > 0.0f) {
        col = mix(col, cloudShade(nt), cloudA);
    }
    if (t < 0.0f) return gradeCoast(col);   // чистое небо

    vec3 hit = ro + rd * t;
    vec3 n = coastSurfNormal(hit.xz);
    vec2 cf = coastField(hit.xz);
    float land = cf.x;
    float shoreGlow = exp(-max(cf.y, 0.0f) * 1.4f);

    //  береговая «полка» у границы рассматривается как суша даже там, где
    //  внутренность множества ещё не набрала близкую к 1 долю суши
    if (land > 0.12f || shoreGlow > 0.30f) {
        // --- суша: песчаный пляж у воды -> зелень -> скалы на вершинах
        float diff = clamp(dot(n, sun), 0.0f, 1.0f);
        float sh = coastShadow(hit + n * 0.05f, sun);
        float sky = 0.30f + 0.45f * clamp(n.y, 0.0f, 1.0f);
        float ambient = 0.20f + 0.14f * clamp(n.y, 0.0f, 1.0f);
        float slope = 1.0f - clamp(n.y, 0.0f, 1.0f);
        float hn = hit.y / max(uTerrainAmplitude, 0.01f);
        vec3 wetSand = vec3(0.48f, 0.39f, 0.23f);
        vec3 sand = vec3(0.76f, 0.68f, 0.45f);
        vec3 green = vec3(0.18f, 0.37f, 0.14f);
        vec3 rock = vec3(0.31f, 0.30f, 0.29f);
        //  песчаная дюна держится ближе всего к линии берега и поднимается
        //  вместе с береговой полкой; дальше от воды — трава и камни
        float nearShore = exp(-max(cf.y, 0.0f) * 2.2f);
        float shoreBand = mix(smoothstep(0.0f, 0.10f, hn),
                              smoothstep(0.0f, 0.24f, hn), nearShore);
        vec3 base = mix(wetSand, sand, shoreBand);
        float vegetation = smoothstep(0.13f, 0.42f, hn) *
                           (1.0f - smoothstep(0.34f, 0.68f, slope));
        vegetation *= 0.80f - 0.55f * nearShore;
        vegetation *= 0.75f + 0.35f * fbm2(hit.xz * 0.65f + 23.4f, 2);
        base = mix(base, green, vegetation);
        float rockMask = smoothstep(0.48f, 0.76f, slope + hn * 0.28f -
                                    (fbm2(hit.xz * 0.70f + 4.2f, 2) - 0.5f) * 0.12f);
        base = mix(base, rock, rockMask);
        // та же микро-текстура, что и в пейзаже
        float blotch = 0.90f + 0.20f * (fbm2(hit.xz * 1.6f + 89.1f, 2) - 0.5f);
        float grain = 0.92f + 0.16f * (fbm2(hit.xz * 7.3f + 13.7f, 1) - 0.5f);
        base *= blotch * grain;
        base *= mix(0.40f, 1.0f, nt);
        col = base * (ambient + (diff + 0.50f * sky) * sh);
        col = applyFog(col, rd, sun, nt, t);
        return gradeCoast(col);
    } else {
        // --- вода: глубокое море -> бирюзовое мелководье у берега
        float dw = cf.y;
        float shore = exp(-max(dw, 0.0f) * 3.2f);
        vec3 deep = vec3(0.025f, 0.12f, 0.25f);
        vec3 shallow = vec3(0.10f, 0.50f, 0.46f);
        vec3 w = mix(deep, shallow, shore);
        w *= (0.55f + 0.55f * nt);

        //  каустики: сетка преломлённого света на дне; видна на мелководье,
        //  проекция смещается нормалью волны (параллакс зависит от глубины)
        float caust = caustics(hit.xz + n.xz * (0.5f + dw * 0.7f), uTime);
        float caustBand = exp(-max(dw, 0.0f) * 2.0f);
        w += vec3(0.50f, 0.85f, 0.72f) * caust * caustBand * (0.35f + 0.60f * nt);

        //  пена: гребни волн Герстнера (высокая крутизна) + прибойная полоса
        //  у линии воды, «дышащая» с фазой волн и разорванная шумовыми полосами
        vec2 gw = gerstnerWaves(hit.xz);
        float crest = gw.y;
        float foamNoise = fbm2(hit.xz * 1.6f +
                               vec2(-uTime * 0.11f, uTime * 0.08f), 2);
        //  гребневая пена (whitecaps): редкие светлые шапки на самых крутых пиках
        float crestFoam = smoothstep(0.45f, 0.80f, crest) *
                          smoothstep(0.30f, 0.62f, foamNoise);
        //  прибой: широкая светлая полоса там, где глубина dw мала (у берега),
        //  модулируется фазой волны (накат/откат) и рвётся шумом в языки
        float wash = smoothstep(1.40f, 0.10f, dw) *
                     (0.40f + 0.60f * smoothstep(-0.50f, 0.55f, crest)) *
                     (0.50f + 0.50f * smoothstep(0.22f, 0.55f, foamNoise));
        float foam = clamp(crestFoam * 0.85f + wash, 0.0f, 1.0f);

        // Френель-Шлик (вода F0=0.02) + честное отражение неба по зеркальному
        // направлению: солнечный диск и облака отражаются на склонах волн
        // (дорожка сверкает), а не смазаны по направлению взгляда
        float v = clamp(dot(n, -rd), 0.0f, 1.0f);
        float x = 1.0f - v;
        float x2 = x * x;
        float fres = 0.02f + 0.98f * x2 * x2 * x;
        float spec = clamp(dot(reflect(-sun, n), -rd), 0.0f, 1.0f);

        vec3 rdir = reflect(rd, n);
        rdir.y = abs(rdir.y);                      // отражение не уходит под горизонт
        vec3 reflSky = skyGradient(rdir, sun, nt, vec3(0.49f, 0.68f, 0.85f),
                                   vec3(0.22f, 0.44f, 0.80f));
        reflSky = mix(reflSky, cloudShade(nt),
                      cloudAlpha(hit + n * 0.05f, rdir, -1.0f));
        col = mix(w, reflSky, fres);

        //  подповерхностное рассеяние: солнце «просвечивает» гребни волн
        //  насквозь полупрозрачным бирюзовым свечением при взгляде на солнце;
        //  толщина слоя — фаза Герстнера (crest), у берега света больше
        float sssB = clamp(dot(rd, sun) * 0.5f + 0.5f, 0.0f, 1.0f);
        float sss = sssB * sssB * sssB * sssB *
                    clamp(crest * 0.5f + 0.65f, 0.0f, 1.0f) * (0.35f + 0.65f * shore);
        col += vec3(0.05f, 0.35f, 0.30f) * sss * nt * 0.55f;

        //  прямой солнечный блик (острый) и дорожка (широкая, мерцает шумом)
        col += vec3(1.0f, 0.95f, 0.8f) * pow(spec, 90.0f) * 0.8f * nt;
        // солнечная дорожка (мерцающие искры на волнах)
        col += vec3(1.0f, 0.90f, 0.75f) * pow(spec, 6.0f) * 0.10f * nt *
               (0.6f + 0.8f * fbm2(hit.xz * 1.4f, 1));
        // пена лежит поверх отражений (светлое матовое рассеяние)
        vec3 foamCol = vec3(0.92f, 0.96f, 0.98f) * (0.40f + 0.75f * nt);
        col = mix(col, foamCol, clamp(foam * (0.70f + 0.30f * fres), 0.0f, 1.0f));
        //  атмосферная дымка: дальнее море, как и суша, тонет в горизонте
        //  (раньше водная ветка возвращалась без тумана — море было «плоским»)
        col = applyFog(col, rd, sun, nt, t);
    }
    return gradeCoast(col);
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
