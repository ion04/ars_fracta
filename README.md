# ars_fracta

Курсовая работа: **визуализация фракталов (OpenGL) и фрактальное сжатие изображений** на C++17.

## Возможности

- Интерактивная визуализация в реальном времени (ray marching в фрагментном шейдере):
  - Mandelbrot 2D
  - Mandelbulb 3D
  - Menger Sponge (губка Менгера)
  - Julia 3D
- Фрактальное сжатие изображений (IFS-кодирование по Жакину): энкодер, декодер, метрики MSE / PSNR / коэффициент сжатия.
- GUI на Dear ImGui: панель параметров, пресеты (JSON), статусная строка.
- Загрузка/сохранение PNG (stb_image), конфигурация приложения (nlohmann/json).

## Сборка

Требования: CMake >= 3.21, компилятор с поддержкой C++17 (MSVC/GCC), OpenGL 3.3+, Python (нужен для генерации GLAD).

```sh
cmake -S . -B build
cmake --build build --config Release
# запуск (Windows)
.\build\Release\ars_fracta.exe
```

Зависимости (GLAD, GLFW, GLM, Dear ImGui, stb, nlohmann/json) скачиваются и собираются автоматически через `FetchContent` — первая конфигурация требует интернет.

## Тесты

```sh
ctest --test-dir build -C Release --output-on-failure
```

Тест `test_compression` строит изображение множества Мандельброта, выполняет цикл
«сжатие → восстановление» и проверяет качество (PSNR) и коэффициент сжатия.

## Структура проекта

```
├── CMakeLists.txt
├── assets/
│   ├── shaders/                # GLSL: fractal.vert / fractal.frag
│   └── presets/                # JSON-пресеты параметров фракталов
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── math/               # Complex, Vector3, Matrix4
│   │   └── fractal/            # Fractal (базовый) + Mandelbrot2D/Mandelbulb3D/MengerSponge/Julia3D
│   ├── render/                 # Window, Shader, Camera, Renderer (OpenGL)
│   ├── compression/            # Encoder, Decoder, Metrics
│   ├── gui/                    # MainWindow, ControlPanel, Viewport (ImGui)
│   └── utils/                  # ImageLoader, Config, Logger
└── tests/                      # test_compression.cpp
```

## Управление (в видовом окне)

- Левая кнопка мыши + drag — вращение камеры
- Правая кнопка + drag — панорама
- Колесо мыши — приближение/отдаление
- WASD — движение, Q/E — вверх/вниз
- ESC — выход