# ars_fracta

Курсовая работа: **визуализация фракталов (OpenGL)** на C++17.

## Возможности

- Интерактивная визуализация в реальном времени (ray marching в фрагментном шейдере):
  - Mandelbrot 2D
  - Mandelbulb 3D
  - Menger Sponge (губка Менгера)
  - Julia 3D
  - Terrain 3D (процедурный пейзаж)
  - Coast 3D (морское побережье вдоль границы множества Мандельброта)
- Отменяемая асинхронная генерация в CPU worker без блокировки GUI.
- GUI на Dear ImGui: панель параметров, пресеты (JSON), статусная строка.
- Сохранение скриншотов в PNG (stb_image), конфигурация приложения (nlohmann/json).


## Сборка

Требования: CMake >= 3.21, компилятор с поддержкой C++17 (MSVC/GCC), OpenGL 3.3+, Python (нужен для генерации GLAD).

```sh
cmake -S . -B build
cmake --build build --config Release
# запуск (Windows)
.\build\ars_fracta.exe
```

Зависимости (GLAD, GLFW, GLM, Dear ImGui, stb, nlohmann/json) скачиваются и собираются автоматически через `FetchContent` — первая конфигурация требует интернет.

## Тесты

```sh
ctest --test-dir build -C Release --output-on-failure
```

Для измерения покрытия CPU-кода (GCC/MinGW) используется отдельная конфигурация:

```sh
cmake -S . -B build-coverage -DARS_FRACTA_BUILD_GUI=OFF \
  -DARS_FRACTA_BUILD_TESTS=ON -DARS_FRACTA_ENABLE_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
# При установленном gcovr:
cmake --build build-coverage --target coverage
```

Покрытие относится к CPU-модулям, которые запускаются тестовым стендом; GUI/OpenGL
и shader-only генерация в процент CPU-покрытия не включаются.

Тест `test_core` покрывает математику фракталов (Complex, Vector3, DE),
фоновые CPU-задачи с отменой и переносом исключений, а также экспорт
губки Менгера в OBJ/STL.

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
│   │   ├── export/             # OBJ и binary STL
│   │   └── fractal/            # CPU/GLSL-фракталы и MengerMesh
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
