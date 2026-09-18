# Verification

The headless configuration is intended for machines without an OpenGL SDK or
display server. It builds the CPU libraries, the core test, and their
FetchContent dependencies without declaring or downloading GLAD, GLFW, GLM, or
ImGui:

```sh
cmake -S . -B build-quality -DARS_FRACTA_BUILD_GUI=OFF -DARS_FRACTA_BUILD_TESTS=ON
cmake --build build-quality --parallel
ctest --test-dir build-quality --output-on-failure
```

Optional GCC instrumentation can be enabled with
`-DARS_FRACTA_ENABLE_COVERAGE=ON`. After running CTest, use an installed tool
such as `gcovr` against `build-quality` to produce a report; the project does
not add a runtime or reporting dependency.

If Doxygen is available at configure time, CMake provides a `docs` target:

```sh
cmake --build build-quality --target docs
```

The GitHub workflow runs this headless configuration with project warnings
treated as errors on Linux/GCC and Windows/MSVC (`/W4 /WX /utf-8`). GUI and
OpenGL runtime behavior must additionally be checked manually on a machine with
an OpenGL 3.3 context.
