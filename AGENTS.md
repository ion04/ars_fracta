# AGENTS.md

Курсовой проект на C++17: OpenGL-визуализация фракталов (ray marching в GLSL).
Windows + MinGW-w64 (`C:/mingw64/bin/c++.exe`), CMake >= 3.21, генератор
`"MinGW Makefiles"`, сборка через `mingw32-make`.

## Архитектура (слои, зависимости строго в одну сторону)

```
main.cpp
  └── ars_gui (MainWindow / ControlPanel / Viewport — ImGui)
        ├── ars_render (Window / Shader / Camera / Renderer — OpenGL)
        └── ars_utils (ImageLoader / Config / Logger)
              └── ars_core (math / async / export / fractal — БЕЗ OpenGL)
```

Правило: `ars_core` НЕ должен зависеть от OpenGL/ImGui — он один тестируется
в headless-сборке (`ARS_FRACTA_BUILD_GUI=OFF`).

## Сборка и тесты

<!-- prettier-ignore -->
```sh
# Полная (GUI) сборка + тесты
cmake -S . -B build
mingw32-make -C build ars_fracta test_core
.\build\test_core.exe                     # → «=== Итог: 0 ошибок ===» означает успех
```

```sh
# Headless проверка (-Werror, без GUI/OpenGL) — обязательна после правок в core
cmake -G "MinGW Makefiles" -S . -B build-quality-werror \
  -DARS_FRACTA_BUILD_GUI=OFF -DARS_FRACTA_BUILD_TESTS=ON -DARS_FRACTA_WARNINGS_AS_ERRORS=ON
mingw32-make -C build-quality-werror
.\build-quality-werror\test_core.exe
```

Опции CMake: `ARS_FRACTA_BUILD_GUI`, `ARS_FRACTA_BUILD_TESTS`,
`ARS_FRACTA_WARNINGS_AS_ERRORS`, `ARS_FRACTA_ENABLE_COVERAGE` (gcovr).
Зависимости (GLAD, GLFW, GLM, ImGui, stb, nlohmann/json) скачиваются
FetchContent — первый configure требует интернета (~2 мин), для GLAD нужен
Python. Несколько build-каталогов допустимы (gitignored через `build-*/`).

## Запуск

```sh
.\build\ars_fracta.exe                                  # интерактивный режим
.\build\ars_fracta.exe --screenshot out.png [preset.json]  # headless-рендер в PNG
.\build\ars_fracta.exe <config.json>                    # свой путь к конфигу
```

`config.json` — gitignored, перезаписывается при выходе из приложения.
Пресеты: `assets/presets/*.json`.

## Критичные инварианты (красная зона)

- Индексы `FractalType` (`src/core/fractal/Fractal.h`, 0..5) ОБЯЗАНЫ совпадать
  с `uFractalType` в `assets/shaders/fractal.frag`. Добавление фрактала = новый
  индекс + ветка в шейдере + строки в `Config.cpp` (`typeToString`/
  `typeFromString`) + пресет + тест в `tests/test_core.cpp`.
- Поля `FractalParams` <-> uniform'ы в `fractal.frag` <-> поля JSON
  (`config.json` / пресеты).
- `assets/` копируются в `build/assets` (при configure и POST_BUILD); шейдер
  читается оттуда (`ARS_FRACTA_ASSETS_DIR`). Правки в `assets/` требуют
  пересборки.
- `test_core` проверяет наличие в `fractal.frag` веток `uFractalType == 4/5`
  и функций `renderTerrain`/`renderCoast` — не удалять их.

## Конвенции кода

- Исходники в UTF-8 БЕЗ BOM; комментарии — по-русски; публичное API — в
  Doxygen-стиле (`@brief`).
- C++17, нет сторонних библиотек кроме перечисленных в `CMakeLists.txt`.
  Пространства имён: `ars::core::{fractal, math, async, exporter}`,
  `ars::render`, `ars::gui`, `ars::utils`.
- Асинхронность: `JobSystem` — один worker на задачу; кооперативная отмена
  через токен; `wait()` пробрасывает исключение worker'а; UI-клиент только
  опрашивает `job.ready()`. Деструктор/move-assignment отменяют и делают join.
- Мёртвый код не оставлять: неиспользуемый публичный API и include удалять
  (перед удалением искать вызовы). Бинарные операторы в `Complex`/`Vector3` —
  осмысленный API, не выпиливать без спроса.
- `Complex`/`Vector3` — header-only, constexpr-лёгкие.
- Не коммитить без явной просьбы пользователя.

## Git
- Формат сообщений: `feat:`, `fix:`, `refactor:`, `docs:`, `test:`.
- Не коммитить `config.json` (gitignored).
- Все коммиты — только по явной просьбе пользователя.

## PLANNING MODE (для Commander и Planner)

- **Всегда задавай уточняющие вопросы.** Если требование неоднозначно — спроси, что именно имеется в виду.
- **Никогда не предполагай дизайн, стек или фичи.** Если в задаче не сказано добавить, не придумывай его сам. Стек зафиксирован в этом файле.
- **Используй сабагентов для исследования.** Перед тем как предложить план, делегируй анализ кода Planner'у — он разбирает слои и текущие реализации.
- **Проверяй план перед показом пользователю.** Прежде чем выдать мне `.opencode/todo.md`, попроси Reviewer или Planner проверить его на соответствие инвариантам.

## CHANGE / EDIT MODE (для Commander)

- **Никогда не реализуй фичи сам, если есть Worker.** Commander — координатор, а не кодер. Вся реализация идёт через `Worker`.
- **Ищи параллельные задачи.** Если две фичи не зависят друг от друга, запускай их параллельно отдельными потоками агентов.
- **Используй правильную модель для задачи.** Простые правки и тесты — `gpt-5.6-terra`; шейдеры, рефакторинг и дизайн — сильная модель, не наоборот.
- **Расширение скоупа — только после ОК Planner'а.** Не добавляй фичи, которых нет в задаче.
- **Эскалация к ExpertDebugger — только после неудачи Worker.** ExpertDebugger (`gpt-6-sol`) вызывается, лишь если стандартный Worker не исправил критическую проблему компиляции C++, утечку памяти или shader bug; передавай ему полные логи/трассы/состояние render pipeline. После исправления обязательны обычные проверки и Reviewer.
- **После каждой фичи запускай проверки.** Сборка + `test_core` + headless `-Werror`.

## TESTING (обязательно для Worker, ExpertDebugger и Reviewer)

- **Всегда используй доступные инструменты тестирования.** В проекте это `test_core.exe` и headless-сборка с `-Werror`.
- **Никогда не предполагай, что изменения работают.** Фраза «код выглядит правильно» — не доказательство. Запусти `test_core` и покажи вывод `=== Итог: 0 ошибок ===`.
- **Если инструментов тестирования нет — спроси пользователя.** Не пропускай тесты молча.
- **После правок в `ars_core` — обязательна headless-проверка.** Без неё изменения не считаются завершёнными.
- **Фикс ExpertDebugger — не исключение.** После эскалации и исправления критической проблемы прогоняется полный набор проверок выше, затем ревью Reviewer'ом.

## Скиллы агентов

Агенты используют только разрешённые им скиллы (конфигурация в
`~/.config/opencode/opencode.jsonc`) и перед профильной работой загружают
подходящий скилл:

- **Commander:** writing-plans, dispatching-parallel-agents, planning, setup-codemap-cli
- **Planner:** codebase-design, spec-workflow, planning, writing-plans
- **Worker:** cpp-coding-standards, shader-dev, cmake, test-driven-development, systematic-debugging
- **ExpertDebugger:** cpp-coding-standards, cmake, shader-dev, systematic-debugging
- **Reviewer:** requesting-code-review, diff-risk-review
