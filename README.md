# Ars Fracta

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.21%2B-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Курсовая работа: **визуализация фракталов (OpenGL)** на C++17.

## Возможности

- **Интерактивная визуализация в реальном времени** (ray marching в фрагментном шейдере):
  - **Мандельброт 2D** — классическое множество Мандельброта (2D escape-time)
  - **Мандельброт 3D** — трёхмерная фрактальная сфера (Mandelbulb 3D)
  - **Губка Менгера** — фрактальный куб (итераций до 12)
  - **Множество Жюлиа** — множество Жюлиа в трёх измерениях
  - **Пейзаж** — процедурный ландшафт с деревьями, скалами, ambient occlusion, облаками, туманом, циклом суток
  - **Морское побережье** — береговая линия вдоль границы множества Мандельброта с волнами Герстнера, каустикой, пеной
- Плавная орбитальная камера с EMA-сглаживанием и адаптивными границами
- **Адаптивное разрешение**: 30+ FPS для анимированных сцен, 45+ FPS при интерактивном движении
- GUI на **Dear ImGui**: панель параметров с вкладками, пресеты (JSON), статусная строка
- Сохранение скриншотов в PNG (stb_image), конфигурация приложения (nlohmann/json)
- Headless-рендер: `ars_fracta.exe --screenshot out.png [preset.json]`
- Иконка окна (16/32/48/256 PNG) и автосохранение конфига при выходе
- **Модульная шейдерная система**: кастомный препроцессор `#include` для GLSL, Uniform Buffer Object (std140)
- Запуск в полноэкранном режиме (primary monitor)

## Сборка

### Требования
- CMake ≥ 3.21
- Компилятор C++17 (MinGW-w64 / MSVC / Clang)
- OpenGL 3.3+
- Python 3 (для генерации GLAD)

### Windows (MinGW-w64)
```sh
cmake -G "MinGW Makefiles" -S . -B build
mingw32-make -C build ars_fracta
# запуск
.\build\ars_fracta.exe
```

### Windows (MSVC / Visual Studio)
```sh
cmake -S . -B build
cmake --build build --config Release --target ars_fracta
# запуск
.\build\Release\ars_fracta.exe
```

### Linux/macOS
```sh
cmake -S . -B build
cmake --build build --target ars_fracta
./build/ars_fracta
```

> Зависимости (GLAD, GLFW, GLM, Dear ImGui, stb, nlohmann/json) скачиваются автоматически через `FetchContent` — первая конфигурация требует интернет (~2 мин).

### Headless-рендер (без GUI)
```sh
ars_fracta.exe --screenshot terrain.png assets/presets/landscape.json
```

## Тесты

```sh
# MinGW
mingw32-make -C build test_core
.\build\test_core.exe

# CMake (любой генератор)
cmake --build build --target test_core
ctest --test-dir build --output-on-failure
```

### Headless-сборка с `-Werror` (проверка без GUI)
```sh
cmake -G "MinGW Makefiles" -S . -B build-quality-werror \
  -DARS_FRACTA_BUILD_GUI=OFF -DARS_FRACTA_BUILD_TESTS=ON \
  -DARS_FRACTA_WARNINGS_AS_ERRORS=ON
cmake --build build-quality-werror
.\build-quality-werror\test_core.exe
```

Тест `test_core` покрывает:
- математику фракталов (Complex, Vector3, Distance Estimation)
- фоновые CPU-задачи с отменой и переносом исключений
- валидацию параметров фракталов и JSON round-trip
- проверку модульной структуры шейдеров (UBO, отсутствие hardcoded литерулов)

## Структура проекта

```
├── CMakeLists.txt
├── assets/
│   ├── shaders/
│   │   ├── fractal.vert              # вершинный шейдер (fullscreen triangle)
│   │   ├── fractal.frag              # главный фрагментный шейдер (entry point + #include)
│   │   └── inc/                      # модульные включения GLSL
│   │       ├── common.inc            # UBO FractalParamsUBO, общие утилиты, палитра, солнце, туман, облака
│   │       ├── mandelbulb.inc        # Mandelbulb/Julia DE, sceneDE, calcNormal, shade, march
│   │       ├── menger.inc            # Menger Sponge DE
│   │       ├── mandelbrot2d.inc      # 2D Mandelbrot escape-time
│   │       ├── terrain.inc           # Procedural Terrain: heightfield, trees, rocks, AO, shadows
│   │       └── coast.inc             # Coast3D: Mandelbrot boundary, Gerstner waves, caustics, foam
│   ├── presets/                      # JSON-пресеты параметров фракталов
│   │   ├── mandelbrot2d.json
│   │   ├── mandelbulb3d.json
│   │   ├── julia3d.json
│   │   ├── landscape.json
│   │   ├── coast.json
│   │   └── menger_sponge.json
│   ├── icon/                         # PNG-иконки окна (16/32/48/256) + .ico
│   └── fonts/                        # Inter-Regular.ttf (кириллица)
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── math/                     # Complex, Vector3
│   │   ├── async/                    # CancellationToken и joinable CPU Job
│   │   └── fractal/                  # CPU/GLSL-фракталы и валидация
│   ├── render/                       # Window, Shader, Camera, Renderer (OpenGL)
│   │   └── Shader.cpp                # кастомный препроцессор #include для GLSL
│   ├── gui/                          # MainWindow, ControlPanel, Viewport (ImGui)
│   └── utils/                        # ImageLoader, Config, Logger
└── tests/
    └── test_core.cpp
```

## Управление

| Действие | Клавиши / Мышь |
|----------|----------------|
| Вращение камеры | ЛКМ + drag |
| Панорамирование | ПКМ + drag |
| Зум | Колесо мыши |
| Движение вперёд/назад/вбок | W / A / S / D |
| Вверх / вниз | Q / E |
| Автовращение (кроме пейзажа/берега) | чекбокс в вкладке «Камера» |
| Сброс камеры / Вид сверху | кнопки в вьюпорте |
| Выход | ESC |

### Панель параметров (ImGui)
- **Фрактал**: выбор типа (Мандельброт 2D, Мандельброт 3D, Губка Менгера, Множество Жюлиа, Пейзаж, Морское побережье)
- **Геометрия**: итерации, граница (bailout), степень (для Мандельброт 3D/Множество Жюлиа), форма (Жюлиа)
- **Ландшафт / Берег**: высота рельефа, частота холмов, неравномерность, масштаб области
- **Атмосфера**: густота тумана, облачность (%), густота леса (%), время суток (24ч)
- **Цвет** (только для 3D-фракталов): масштаб палитры, сдвиг цвета (об.), палитра
- **Камера**: позиция, сброс, вид сверху, автовращение
- **Рендер**: адаптивное разрешение, целевой FPS при движении
- **Статус-бар** (внизу): FPS, время кадра (ms)

### Пресеты
- Сохранение/загрузка JSON через вкладку «Пресеты» внизу панели
- Примеры в `assets/presets/`: `mandelbrot2d.json`, `landscape.json`, `coast.json`, `menger_sponge.json`, `mandelbulb3d.json`, `julia3d.json`

## Технические детали

### Модульные шейдеры
Фрагментный шейдер разбит на 6 модулей в `assets/shaders/inc/`, собираемых через кастомный препроцессор (`Shader::preprocessShader`), сохраняющий `#version` как первую директиву.

### Uniform Buffer Object (std140)
Все параметры фракталов передаются через один UBO (`FractalParamsUBO`, 96 байт, binding point 0) вместо 30+ отдельных `glUniform*` вызовов. Включает `uTime` для анимаций в math utilities.

### Полноэкранный режим
Приложение запускается в fullscreen на primary monitor (разрешение берётся из `glfwGetVideoMode`).

## Лицензия

MIT — см. [LICENSE](LICENSE).