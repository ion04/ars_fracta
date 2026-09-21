# Контекст проекта ars_fracta

## Архитектура (подтверждена аудитом)

```
main.cpp
  └── ars_gui (MainWindow / ControlPanel / Viewport — ImGui)
        ├── ars_render (Window / Shader / Camera / Renderer — OpenGL)
        └── ars_utils (ImageLoader / Config / Logger)
              └── ars_core (math / async / export / fractal — БЕЗ OpenGL)
```

### Слои (зависимости строго вниз)

- **ars_core** (src/core/): математика, фрактальная генерация, async, экспорт — headless, без OpenGL
  - `math/`: Complex, Vector3 (header-only, constexpr)
  - `fractal/`: Fractal.h (базовый класс), Mandelbrot2D, Mandelbulb3D, Julia3D, MengerSponge, MengerMesh, SphericalDistance.h
  - `async/`: JobSystem (кооперативная отмена, пробрасывание исключений)
  - `export/`: MeshExporter (OBJ, binary STL)

- **ars_utils** (src/utils/): конфигурация, логирование, загрузка изображений
  - Config.cpp: JSON ↔ FractalParams (typeToString/typeFromString — красная зона!)
  - Logger.cpp: простой буфер сообщений для UI
  - ImageLoader.cpp: stb_image / stb_image_write

- **ars_render** (src/render/): OpenGL рендеринг
  - Window.cpp: GLFW + GLAD init (известная проблема: двойной glfwTerminate при фейле init)
  - Camera.cpp: орбитальная камера (orbit, zoom, pan, revision для selective redraw)
  - Shader.cpp: компиляция шейдеров (известная проблема: кэш location включая -1)
  - Renderer.cpp: ray marching в offscreen FBO, адаптивное разрешение, кэширование кадра

- **ars_gui** (src/gui/): ImGui интерфейс
  - MainWindow.cpp: главный цикл, адаптивное разрешение при движении, автовращение
  - ControlPanel.cpp: слайдеры параметров фракталов, загрузка пресетов, экспорт Menger mesh
  - Viewport.cpp: показ текстуры рендерера, мышиный ввод (orbit/pan/zoom)

## Красная зона (критичные инварианты)

### 1. FractalType ↔ uFractalType (индексы ОБЯЗАНЫ совпадать)

**C++ (Fractal.h:10-17):**
```cpp
enum class FractalType : int {
    Mandelbrot2D = 0,  // уникальный 2D-рендер (не ray marching)
    Mandelbulb3D = 1,  // spherical DE
    MengerSponge = 2,  // folding SDF
    Julia3D      = 3,  // spherical DE
    Terrain3D    = 4,  // процедурный пейзаж (fBm height field)
    Coast3D      = 5,  // морское побережье (граница множества Мандельброта)
};
```

**GLSL (fractal.frag:18, 742-757):**
```glsl
uniform int uFractalType;  // 0=2D, 1=Mandelbulb, 2=Menger, 3=Julia, 4=Terrain, 5=Coast

// main():
if (uFractalType == 0) { FragColor = vec4(mandelbrot2D(), 1.0); return; }
if (uFractalType == 4) { FragColor = vec4(renderTerrain(uCamPos, dir), 1.0); return; }
if (uFractalType == 5) { FragColor = vec4(renderCoast(uCamPos, dir), 1.0); return; }
// иначе: ray marching для 1/2/3
```

**JSON (Config.cpp:18-38, typeToString/typeFromString):**
- Строковые имена: "Mandelbrot2D", "Mandelbulb3D", "MengerSponge", "Julia3D", "Terrain3D", "Coast3D"

**Добавление нового фрактала требует:**
1. Новый индекс в `FractalType` (Fractal.h)
2. Ветка `uFractalType == N` в шейдере (fractal.frag main())
3. Строки в `typeToString`/`typeFromString` (Config.cpp)
4. Пресет в `assets/presets/<name>.json`
5. Тест в `test_core.cpp` (проверка на наличие ветки в шейдере)

### 2. FractalParams ↔ uniform'ы ↔ JSON

**Поля структуры `FractalParams` (Fractal.h:37-58):**
```cpp
struct FractalParams {
    FractalType type;
    int   iterations, colorMode;
    float bailout, power, juliaReal, juliaImag, juliaImag3D;
    float detail, colorScale, hueShift, m2dZoom;
    float terrainAmplitude, terrainFrequency;  // Terrain3D
    float cloudDensity, treeDensity, timeOfDay;
    float coastCenterRe, coastCenterIm;        // Coast3D
    bool  autoRotate;
};
```

**Uniform'ы в шейдере (fractal.frag:18-39):**
- Все поля `FractalParams` передаются как uniform (кроме `type` и `autoRotate`)
- `type` → `uFractalType`, `autoRotate` обрабатывается в MainWindow.cpp

**JSON-сериализация (Config.cpp:40-88, fractalToJson/fractalFromJson):**
- Все поля сохраняются/загружаются
- `type` преобразуется в строку

**Добавление нового параметра:**
1. Поле в `FractalParams` (Fractal.h)
2. Uniform в шейдере (fractal.frag)
3. `shader_.set*()` в Renderer.cpp:renderFrame()
4. JSON-сериализация в Config.cpp
5. Слайдер/чекбокс в ControlPanel.cpp (если требуется UI)
6. Обновить `operator==` в `FractalParams` (Fractal.h:60-72) для кэширования кадра

### 3. Функции renderTerrain / renderCoast (тесты проверяют их наличие)

**Проверка в test_core.cpp:154-161:**
```cpp
std::ifstream shader("assets/shaders/fractal.frag");
const std::string shaderText(...);
check(shaderText.find("uFractalType == 4") != std::string::npos &&
      shaderText.find("uFractalType == 5") != std::string::npos &&
      shaderText.find("renderTerrain") != std::string::npos &&
      shaderText.find("renderCoast") != std::string::npos, ...);
```

**НЕ УДАЛЯТЬ** эти функции и ветки без обновления теста.

---

## Реализация пейзажа и морского берега

### Terrain3D (uFractalType == 4)

**Расположение:** `fractal.frag:203-515`

**Ключевые функции:**
- `terrainH(vec2 xz)` (253-269): высота рельефа
  - fBm для холмов (4 октавы)
  - ridge noise для фрактальных хребтов (3 октавы)
  - Domain warping для естественных форм
  - Детализация мелкими камнями (1 октава)
- `marchTerrain()` (276-301): ray marching по height field с бисекцией
- `terrainShadow()` (304-316): короткий марш к солнцу для теней
- `terrainNormal()` (319-326): конечные разности по высоте
- `treesDE()` (380-408): SDF-ели в сетке 1×1, конусы + стволы
- `cloudAlpha()` (411-424): пересечение луча с плоскостью облаков
- `skyGradient()` (427-440): градиент неба + солнечный диск + гало
- `terrainColor()` (350-377): песок → трава → камень → снег (по высоте и slope)
- `renderTerrain()` (473-515): главная функция рендера

**Что можно улучшить:**
1. **Детализация рельефа:** добавить камни (SDF-сферы), расщелины (вычитание noise), пещеры
2. **Текстурирование:** tri-planar mapping для скал, процедурные пятна травы
3. **Ambient occlusion:** сейчас нет — добавить short-range AO марш
4. **Деревья:** сейчас простые конусы, можно добавить ветки (рекурсивный SDF)
5. **Облака:** сейчас 2D plane, можно volumetric raymarching
6. **Вода:** сейчас нет, можно добавить реки/озёра (высота < threshold)
7. **Lighting:** сейчас diffuse + тени, можно добавить specular для мокрых камней
8. **Туман:** параметр захардкожен (строка 451: `0.0055`), можно вынести в uniform

**Параметры в UI (ControlPanel.cpp:127-138):**
- ✓ terrainAmplitude (1..20)
- ✓ terrainFrequency (0.03..0.5)
- ✓ cloudDensity (0..1.5)
- ✓ treeDensity (0..1)
- ✓ timeOfDay (0..1)
- ✗ fogDensity — НЕТ (захардкожен в шейдере)
- ✗ waterLevel — НЕТ (воды нет)
- ✗ rockDetail — НЕТ (фиксированная детализация)

### Coast3D (uFractalType == 5)

**Расположение:** `fractal.frag:517-730`

**Ключевые функции:**
- `coastC(vec2 xz)` (524-527): отображение world → комплексная плоскость Мандельброта
- `coastInteriorFast()` (531-538): быстрые тесты кардиоиды и бульбы (избегаем итераций)
- `coastField(vec2 xz)` (541-569): итерация Мандельброта → (land, extDist)
  - land = 0..1 (доля суши)
  - extDist = внешнее расстояние до границы в мировых единицах
- `coastHeight()` (574-583): высота суши (холмы внутри множества, береговая полка у границы)
- `waveHeight()` (586-592): мелкие волны на воде
- `coastSurface()` (595-599): видимая высота (суша или волны)
- `marchCoast()` (605-628): ray marching по surface
- `coastSurfNormal()` (630-638): нормаль (шаг сэмплирования масштабируется с zoom)
- `coastShadow()` (641-653): тени на суше
- `renderCoast()` (655-730): главная функция рендера

**Что можно улучшить:**
1. **Детализация береговой линии:** сейчас гладкая, можно добавить камни/скалы вдоль границы
2. **Вода:** простая (френель + блик), можно caustics, подповерхностное рассеяние, пену у берега
3. **Суша:** сейчас песок → трава → камень, можно добавить растительность (как в Terrain)
4. **Волны:** простая модуляция высоты, можно Gerstner waves для реалистичной формы
5. **Небо/облака:** используется тот же skyGradient что и в Terrain — можно специализировать (морской горизонт)
6. **Zoom-контроль:** сейчас m2dZoom управляет масштабом, но камера не двигается — неинтуитивно
7. **Цветокоррекция:** gradeCoast (469-471) повышает насыщенность на 15% + яркость 10% — можно сделать параметром

**Параметры в UI (ControlPanel.cpp:139-155):**
- ✓ iterations (16..512) — точность границы Мандельброта
- ✓ coastCenterRe (-2..1)
- ✓ coastCenterIm (-1.2..1.2)
- ✓ m2dZoom (0.05..40) — масштаб берега
- ✓ terrainAmplitude (1..20) — высота суши
- ✓ cloudDensity (0..1.5)
- ✓ timeOfDay (0..1)
- ✗ waveAmplitude — НЕТ (захардкожен)
- ✗ waterColor — НЕТ (захардкожен: deep/shallow в строках 715-716)
- ✗ foamIntensity — НЕТ (пены нет)

---

## Известные проблемы (из AGENTS.md + аудит)

### 1. Баг автовращения (КРИТИЧЕСКИЙ — Фаза 4)

**Симптомы:**
- Переключение с фрактала (Mandelbulb/Julia/Menger) с включённым автовращением на Terrain3D/Coast3D
- Вращение НЕ отключается, хотя UI-чекбокса нет

**Причина (найдена):**
- `ControlPanel.cpp:179` — чекбокс автовращения показывается только для типов 0/1/2/3 (внутри блока `else`)
- `MainWindow.cpp:112-115` — автовращение применяется **БЕЗ проверки типа**:
  ```cpp
  if (params_.autoRotate) {
      renderer_->camera().orbit(deltaTime * 10.0f, 0.0f);
  }
  ```
- При переключении типа фрактала поле `params_.autoRotate` остаётся `true`

**Фикс:**
1. При переключении на Terrain3D/Coast3D принудительно сбрасывать `autoRotate = false` (ControlPanel.cpp:116-124)
2. Либо: в MainWindow.cpp:112 добавить проверку типа перед применением вращения

### 2. Shader::location кэширует `-1` (MINOR — Фаза 4)

**Файл:** `Shader.cpp:104-110`

**Проблема:**
```cpp
GLint Shader::location(const std::string& name) const {
    const auto it = locations_.find(name);
    if (it != locations_.end()) return it->second;  // может вернуть -1
    const GLint loc = glGetUniformLocation(program_, name.c_str());
    locations_[name] = loc;  // кэширует -1 если uniform не найден
    return loc;
}
```

**Последствия:**
- Если uniform отсутствует в шейдере (опечатка / оптимизация компилятора), `glGetUniformLocation` возвращает `-1`
- `-1` кэшируется, последующие `setInt/setFloat` с этим именем молча игнорируются (не вызывают ошибку OpenGL)
- Сложно отладить: параметр не применяется, но ошибки нет

**Фикс:**
- Либо: не кэшировать `-1` (повторно запрашивать при каждом set)
- Либо: логировать warning при первом получении `-1`

### 3. Window: двойной glfwTerminate (MINOR — Фаза 4)

**Файл:** `Window.cpp:16-19, 22-34`

**Проблема:**
```cpp
Window::~Window() {
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();  // вызывается всегда
}

bool Window::init() {
    if (!glfwInit()) {
        std::cerr << "[Window] glfwInit() failed\n";
        return false;  // деструктор потом вызовет terminate на неинициализированном GLFW
    }
    // ...
    window_ = glfwCreateWindow(...);
    if (!window_) {
        std::cerr << "[Window] Cannot create GLFW window\n";
        glfwTerminate();  // первый вызов
        return false;     // деструктор вызовет второй
    }
```

**Последствия:**
- Двойной `glfwTerminate()` — undefined behavior (GLFW не гарантирует идемпотентность)
- На практике обычно безвреден, но нарушает спецификацию

**Фикс:**
- Флаг `glfwInitialized_` в Window, деструктор проверяет его перед terminate

### 4. Адаптивное разрешение: сложная эвристика (OPTIMIZATION — Фаза 4)

**Файл:** `MainWindow.cpp:117-171`

**Проблема:**
- Алгоритм подбора масштаба основан на EMA времени кадра и целевом диапазоне 20-23 мс
- «Зона бездействия» (20-23 мс) узкая — возможны осцилляции
- Первый кадр движения ограничивается 200k пикселей (строка 149) — может быть слишком мало для больших экранов

**Возможные улучшения:**
- Расширить зону бездействия (например, 18-25 мс)
- Сделать целевой FPS настраиваемым (сейчас захардкожен >= 45 fps)
- Динамически подбирать начальное разрешение по GPU (замерить 1 кадр на полном разрешении, оценить throughput)

---

## Производительность (hotspots)

### GPU (шейдеры)

1. **Ray marching бюджет (Renderer.cpp:175-194):**
   - Menger: 60-160 шагов (зависит от renderScale)
   - Mandelbulb/Julia: 140-300 шагов
   - Terrain: 54-124 шага
   - Coast: 30-70 шагов
   - **Оптимизация:** адаптивный step size (сейчас clamp(d, stepMin, constant))

2. **Distance estimation:**
   - `mandelbulbDE` (fractal.frag:50-67): exp2(power*log2(r)) — дорого при высоких power
   - `mengerDE` (92-109): folding SDF — дёшево, но итераций > 12 не нужны (Renderer.cpp:154-157)
   - **Оптимизация:** для Menger итерации ограничены max=12 (уже сделано)

3. **Terrain/Coast марш:**
   - Height field не SDF → требуется бисекция (6 итераций) при пересечении
   - `terrainH` вызывает fbm2/ridge2 (до 4+3 октав) → много вызовов vnoise
   - `coastField` вызывает итерацию Мандельброта (до 512 итераций в высоком zoom)
   - **Оптимизация:** табулировать шум (texture lookup), cache coastField для соседних пикселей

4. **Нормали (fractal.frag:176-183, 319-326, 630-638):**
   - 3D фракталы: 4 вызова DE на пиксель (тетраэдральный градиент)
   - Terrain/Coast: 4 вызова height function
   - **Оптимизация:** для статичных сцен — precompute normal map

### CPU

1. **Адаптивное разрешение (MainWindow.cpp:117-171):**
   - `glFinish()` на каждом кадре движения (строка 181) — синхронная точка, блокирует pipeline
   - **Оптимизация:** async query (GL_TIME_ELAPSED), не блокировать

2. **Кэширование кадра (Renderer.cpp:101-123):**
   - Сравнение `params != lastParams_` (Fractal.h:60-72) — 20+ полей
   - **Оптимизация:** уже достаточно быстро (структура мала), но можно хэш

3. **JobSystem (async/JobSystem.cpp):**
   - Один worker поток на задачу (не pool) — может быть много потоков при параллельных задачах
   - **Оптимизация:** thread pool с очередью (но для данного проекта избыточно — задачи редкие)

---

## Референсы для вдохновения (НЕ КОПИРОВАТЬ КОД)

### Процедурная генерация ландшафтов

- **Inigo Quilez: "Terrain Raymarching"** — https://iquilezles.org/articles/terrainmarching/
  - Техники оптимизации марша по height field
  - Domain warping для естественных форм
- **Íñigo Quilez: "Noise functions"** — https://www.shadertoy.com/view/XslGRr
  - Библиотека шумов (value, gradient, simplex)
- **Shadertoy: "Elevated" by Íñigo Quilez** — https://www.shadertoy.com/view/MdX3Rr
  - Пейзаж с деревьями, облаками, атмосферой
  - **НЕ КОПИРОВАТЬ** — использовать как референс визуала

### Вода и океан

- **"GPU Gems: Chapter 1. Effective Water Simulation from Physical Models"**
  - Gerstner waves для реалистичной формы волн
- **Shadertoy: "Seascape" by TDM** — https://www.shadertoy.com/view/Ms2SD1
  - Процедурный океан с пеной и подповерхностным рассеянием
  - **НЕ КОПИРОВАТЬ** — референс техник

### Облака

- **Íñigo Quilez: "Clouds" tutorial** — https://iquilezles.org/articles/clouds/
  - Volumetric raymarching облаков
- **Shadertoy: "Clouds" by Íñigo Quilez** — https://www.shadertoy.com/view/XslGRr

### Мандельброт и фракталы

- **"The Beauty of Fractals" by Peitgen & Richter** (книга) — визуальный референс
- **Shadertoy: "Mandelbrot - Smooth" by Íñigo Quilez** — https://www.shadertoy.com/view/lsX3W4
  - Техника smooth iteration count (уже используется в проекте)

---

## Следующие шаги (см. todo.md)

1. **Фаза 2:** Улучшение пейзажа и побережья (детализация, освещение, вода)
2. **Фаза 2.5:** Камера (плавность, границы)
3. **Фаза 3:** UI (группировка параметров, улучшение layout)
4. **Фаза 4:** Фикс автовращения + оптимизация + known issues
5. **Фаза 5:** Полировка (Doxygen, README, чек-лист)
