# ars_fracta

Курсовая работа: **визуализация фракталов (OpenGL)** на C++17.

## Возможности

- Интерактивная визуализация в реальном времени (ray marching в фрагментном шейдере):
  - **Mandelbrot 2D** — классическое множество Мандельброта
  - **Mandelbulb 3D** — трёхмерная фрактальная сфера
  - **Menger Sponge** — губка Менгера (фрактальный куб)
  - **Julia 3D** — множество Жюлиа в трёх измерениях
  - **Terrain 3D** — процедурный пейзаж с деревьями, скалами, ambient occlusion
  - **Coast 3D** — морское побережье вдоль границы множества Мандельброта с Gerstner waves
- Плавная орбитальная камера с EMA-сглаживанием и адаптивными границами
- Адаптивное разрешение: 30+ FPS для анимированных сцен, 45+ FPS при интерактивном движении
- GUI на Dear ImGui: панель параметров, пресеты (JSON), статусная строка
- Сохранение скриншотов в PNG (stb_image), конфигурация приложения (nlohmann/json)
- Headless-рендер: `ars_fracta.exe --screenshot out.png [preset.json]`


## Сборка

Требования: CMake >= 3.21, MinGW-w64 (C++17), OpenGL 3.3+, Python (для генерации GLAD).

```sh
cmake -G "MinGW Makefiles" -S . -B build
mingw32-make -C build ars_fracta
# запуск (Windows)
.\build\ars_fracta.exe

# headless-рендер в PNG
.\build\ars_fracta.exe --screenshot terrain.png assets/presets/landscape.json
```

Зависимости (GLAD, GLFW, GLM, Dear ImGui, stb, nlohmann/json) скачиваются автоматически через `FetchContent` — первая конфигурация требует интернет (~2 мин).

## Тесты

```sh
mingw32-make -C build test_core
.\build\test_core.exe
```

Для headless-сборки с `-Werror` (проверка без GUI):

```sh
cmake -G "MinGW Makefiles" -S . -B build-quality-werror \
  -DARS_FRACTA_BUILD_GUI=OFF -DARS_FRACTA_BUILD_TESTS=ON \
  -DARS_FRACTA_WARNINGS_AS_ERRORS=ON
mingw32-make -C build-quality-werror
.\build-quality-werror\test_core.exe
```

Тест `test_core` покрывает математику фракталов (Complex, Vector3, DE), фоновые CPU-задачи с отменой и переносом исключений, а также валидацию параметров и JSON round-trip.

## Структура проекта

```
├── CMakeLists.txt
├── assets/
│   ├── shaders/                # GLSL: fractal.vert / fractal.frag
│   └── presets/                # JSON-пресеты параметров фракталов
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── math/               # Complex, Vector3
│   │   ├── async/              # CancellationToken и joinable CPU Job
│   │   └── fractal/            # CPU/GLSL-фракталы и валидация
│   ├── render/                 # Window, Shader, Camera, Renderer (OpenGL)
│   ├── gui/                    # MainWindow, ControlPanel, Viewport (ImGui)
│   └── utils/                  # ImageLoader, Config, Logger
└── tests/                      # test_core.cpp
```

## Управление (в видовом окне)

- Левая кнопка мыши + drag — вращение камеры
- Правая кнопка + drag — панорама
- Колесо мыши — приближение/отдаление
- WASD — движение, Q/E — вверх/вниз
- ESC — выход
