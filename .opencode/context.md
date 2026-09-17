# Контекст миссии

Цель: улучшить систему генерации и визуализации фракталов без поломки текущего GUI.

Наблюдения анализа:
- В `FractalType` уже есть 6 типов: Mandelbrot2D, Mandelbulb3D, MengerSponge,
  Julia3D, Terrain3D и Coast3D; GUI и GLSL поддерживают их.
- Сборка CMake уже разделена на `ars_core`, `ars_utils`, `ars_compression`,
  `ars_render`, `ars_gui`; зависимости подключаются через FetchContent.
- `ctest --test-dir build --output-on-failure` проходит: 1/1 тест.
- Сжатие уже имеет encoder/decoder и MSE/PSNR/ratio, но вычисление синхронное
  и экспорт геометрии отсутствует.
- В GUI есть адаптивное разрешение, но тяжёлые операции compression выполняются
  прямо в `ControlPanel::compressCurrentView()`.

Ближайший безопасный шаг: добавить независимый от OpenGL отменяемый асинхронный
job-контур, затем подключить к compression/export без обращения к ImGui из worker.

Текущая реализация:
- `JobSystem` уже добавлен в рабочее дерево как отдельный CPU-only модуль и
  включён в `ars_core`.
- `Job` использует выделенный `std::thread`, кооперативный `CancellationToken`,
  атомарный флаг готовности и переносит исключение worker в `wait()`.
- Перед подключением к GUI необходимо проверить отмену, неблокирующую готовность
  и распространение исключения; `ControlPanel::compressCurrentView()` пока
  остаётся синхронным.
