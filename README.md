# Ars Fracta

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![CMake](https://img.shields.io/badge/CMake-3.21%2B-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Курсовая работа: **визуализация фракталов (OpenGL)** на C++17.

## Возможности

- **Интерактивная визуализация в реальном времени** (ray marching в фрагментном шейдере):
  - **Mandelbrot 2D** — классическое множество Мандельброта
  - **Mandelbulb 3D** — трёхмерная фрактальная сфера
  - **Menger Sponge** — губка Менгера (фрактальный куб, итераций до 12)
  - **Julia 3D** — множество Жюлиа в трёх измерениях
  - **Terrain 3D** — процедурный пейзаж с деревьями, скалами, ambient occlusion
  - **Coast 3D** — морское побережье вдоль границы множества Мандельброта с Gerstner waves
- Плавная орбитальная камера с EMA-сглаживанием и адаптивными границами
- **Адаптивное разрешение**: 30+ FPS для анимированных сцен, 45+ FPS при интерактивном движении
- GUI на **Dear ImGui**: панель параметров, пресеты (JSON), статусная строка
- Сохранение скриншотов в PNG (stb_image), конфигурация приложения (nlohmann/json)
- Headless-рендер: `ars_fracta.exe --screenshot out.png [preset.json]`
- Иконка окна (16/32/48/256 PNG) и автосохранение конфига при выходе

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

## Структура проекта

```
├── CMakeLists.txt
├── assets/
│   ├── shaders/                # GLSL: fractal.vert / fractal.frag
│   ├── presets/                # JSON-пресеты параметров фракталов
│   ├── icon/                   # PNG-иконки окна (16/32/48/256) + .ico
│   └── fonts/                  # Inter-Regular.ttf (кириллица)
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── math/               # Complex, Vector3
│   │   ├── async/              # CancellationToken и joinable CPU Job
│   │   └── fractal/            # CPU/GLSL-фракталы и валидация
│   ├── render/                 # Window, Shader, Camera, Renderer (OpenGL)
│   ├── gui/                    # MainWindow, ControlPanel, Viewport (ImGui)
│   └── utils/                  # ImageLoader, Config, Logger
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
- **Фрактал**: выбор типа (Mandelbrot 2D, Mandelbulb 3D, Menger Sponge, Julia 3D, Terrain 3D, Coast 3D)
- **Геометрия**: итерации, граница (bailout), степень (для Mandelbulb/Julia), форма (Julia)
- **Ландшафт / Берег**: высота рельефа, частота холмов, неравномерность, масштаб области
- **Атмосфера**: густота тумана, облачность (%), густота леса (%), время суток (24ч)
- **Цвет**: масштаб палитры, сдвиг цвета (об.), палитра
- **Камера**: позиция, сброс, вид сверху, автовращение
- **Рендер**: адаптивное разрешение, целевой FPS при движении
- **Статус-бар** (внизу): FPS, время кадра (ms)

### Пресеты
- Сохранение/загрузка JSON через вкладку «Пресеты» внизу панели
- Примеры в `assets/presets/`: `mandelbrot.json`, `landscape.json`, `coast.json`, `menger_sponge.json`

## Установка (Installers)

### Windows (NSIS)
Требуется NSIS 3.x: `winget install NSIS.NSIS` или скачайте с [nsis.sourceforge.io](https://nsis.sourceforge.io/Download).

```sh
# Сборка и создание установщика
cmake -G "MinGW Makefiles" -S . -B build-installer -DARS_FRACTA_BUILD_GUI=ON
cmake --build build-installer --config Release --target ars_fracta
cpack -G NSIS -C Release --config build-installer/CPackConfig.cmake
# Результат: build-installer/ars_fracta-0.1.0-windows-x64.exe
```

Или используйте скрипт:
```sh
.\build_installer.bat
# или PowerShell
.\build_installer.ps1
```

### Linux (DEB/RPM)
```sh
cmake -S . -B build-installer -DARS_FRACTA_BUILD_GUI=ON
cmake --build build-installer --config Release --target ars_fracta
cpack -G DEB -C Release --config build-installer/CPackConfig.cmake
cpack -G RPM -C Release --config build-installer/CPackConfig.cmake
```

### macOS (DMG)
```sh
cmake -S . -B build-installer -DARS_FRACTA_BUILD_GUI=ON
cmake --build build-installer --config Release --target ars_fracta
cpack -G DragNDrop -C Release --config build-installer/CPackConfig.cmake
```

## Лицензия

MIT — см. [LICENSE](LICENSE).