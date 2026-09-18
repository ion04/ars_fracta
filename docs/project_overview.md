# ars_fracta — обзор проекта и код-ревью

## Общее

`ars_fracta` — курсовая работа на C++17: интерактивная OpenGL-визуализация фракталов (ray marching в GLSL). ~2300 строк кода, MSVC/GCC, CMake + FetchContent (GLAD, GLFW, GLM, ImGui, stb, nlohmann/json).

## Архитектура

Слоистая, зависимости идут в одну сторону — это главная сила проекта:

```
main.cpp
  └── ars_gui (ImGui: MainWindow / ControlPanel / Viewport)
        ├── ars_render (Window / Shader / Camera / Renderer — OpenGL)
        └── ars_utils (ImageLoader / Config / Logger)
              └── ars_core (math / async / export / fractal — БЕЗ OpenGL)
```

`ars_core` не знает про OpenGL и ImGui, поэтому тестируется независимо от GUI (GUI вообще можно отключить через `ARS_FRACTA_BUILD_GUI=OFF`, что и делается для coverage).

## Модули

- **core/math** — `Complex`, `Vector3` (header-only, constexpr-лёгкие).
- **core/async** — `Job`/`JobSystem`/`CancellationToken`: один поток на задачу, joinable, перенос исключений через `wait()`, кооперативная отмена. Деструктор/move-assignment отменяет и делает join.
- **core/fractal** — `Fractal` (абстрактный, `sanitize()` валидирует параметры) + CPU-реализации DE: Mandelbrot2D, Mandelbulb3D, Julia3D, MengerSponge, `MengerMesh` (построение внешних граней для OBJ/STL), плюс Terrain3D/Coast3D (реализация в шейдере).
- **core/export** — `MeshExporter`: OBJ и binary STL с ручной little-endian записью, валидацией NaN/degenerate-треугольников, LOD-лимитом глубины.
- **render** — GLFW-окно, шейдер с кэшем uniform-локаций, орбитальная камера, отрисовка в offscreen-FBO + кэш «не пересчитывать, если ничего не изменилось».
- **gui** — панель управления (параметры, пресеты, экспорт Менгера в фоне), viewport с мышью, адаптивное разрешение (понижение во время движения, полное качество в статике), статус-бар с последним логом.
- **utils** — stb-загрузка PNG, JSON-конфиг/пресеты, потокобезопасный Logger с ring-буфером.

## Код ревью

Сильные стороны:
- Чистое разделение CPU-логики и GUI; все тяжёлые операции (построение меша) уходят в worker с отменой, UI не блокируется и только опрашивает `job.ready()`.
- Нет сырого ручного управления памятью, RAII для GL-объектов, корректные move-семантики у `Shader`/`Job`.
- Валидация входов почти везде (MeshExporter, `Config` через try/catch).
- Хорошая документация (Doxygen-комментарии, README, `docs/verification.md`) и осмысленные тесты (математика, async-отмена, проверка OBJ-индексов и STL-размера).

### Найденные проблемы (по убыванию важности)

1. **`Logger::entries()`** (`src/utils/Logger.cpp:25`) возвращает `const&` к `entries_` без блокировки — латентная data race, если кто-то из worker'ов залогирует.
2. **README отстал от кода**: в списке возможностей 4 фрактала, по факту 6 (Terrain3D, Coast3D). Секция «Структура проекта» тоже устарела.
3. Мелочи: `Window` вызывает `glfwTerminate` дважды при фейле инициализации (`src/render/Window.cpp`); `Shader::location` кэширует и `-1`.