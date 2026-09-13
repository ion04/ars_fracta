cmake_minimum_required(VERSION 3.21)

project(ars_fracta
    VERSION 0.1.0
    DESCRIPTION "Fractal rendering and fractal compression (coursework)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ARS_FRACTA_BUILD_GUI   "Build OpenGL/ImGui application" ON)
option(ARS_FRACTA_BUILD_TESTS "Build unit tests"               ON)

find_package(OpenGL REQUIRED)

# ----------------------------------------------------------------------------
#  Dependencies (downloaded and built automatically by CMake FetchContent)
# ----------------------------------------------------------------------------
include(FetchContent)

# GLAD - OpenGL 3.3 core loader (requires Python to generate sources)
set(GLAD_PROFILE "core" CACHE STRING "GLAD OpenGL profile" FORCE)
set(GLAD_API "gl=3.3"   CACHE STRING "GLAD API type/version pairs" FORCE)
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
    GIT_SHALLOW    TRUE)

# GLFW - windowing and input
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE)

# GLM - math for rendering
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE)

# Dear ImGui - immediate mode GUI (official repo has no CMakeLists, see below)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE)

# stb - image loading/saving
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)

# nlohmann/json - settings and presets
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(glad glfw glm imgui stb json)

# --- bundled targets for dependencies without their own CMake ---------------
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui PUBLIC glad glfw OpenGL::GL)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# ----------------------------------------------------------------------------
#  Common definitions and asset copying
# ----------------------------------------------------------------------------
set(ARS_FRACTA_ASSETS_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets"
    CACHE PATH "Runtime assets directory")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# --- core: math + fractal generation (no OpenGL dependency) -----------------
add_library(ars_core STATIC
    src/core/fractal/Mandelbrot2D.cpp
    src/core/fractal/Mandelbulb3D.cpp
    src/core/fractal/MengerSponge.cpp
    src/core/fractal/Julia3D.cpp)
target_include_directories(ars_core PUBLIC src)

# --- utils: image I/O, config, logging --------------------------------------
add_library(ars_utils STATIC
    src/utils/ImageLoader.cpp
    src/utils/Config.cpp
    src/utils/Logger.cpp)
target_include_directories(ars_utils PUBLIC src)
target_link_libraries(ars_utils PUBLIC stb nlohmann_json::nlohmann_json ars_core)

# --- compression: fractal image compression ----------------------------------
add_library(ars_compression STATIC
    src/compression/Encoder.cpp
    src/compression/Decoder.cpp
    src/compression/Metrics.cpp)
target_include_directories(ars_compression PUBLIC src)
target_link_libraries(ars_compression PUBLIC ars_utils ars_core)

# --- render: OpenGL rendering ------------------------------------------------
if(ARS_FRACTA_BUILD_GUI)
    add_library(ars_render STATIC
        src/render/Camera.cpp
        src/render/Shader.cpp
        src/render/Renderer.cpp
        src/render/Window.cpp)
    target_include_directories(ars_render PUBLIC src)
    target_link_libraries(ars_render PUBLIC ars_core glad glfw OpenGL::GL glm::glm)
    target_compile_definitions(ars_render PUBLIC ARS_FRACTA_ASSETS_DIR="${ARS_FRACTA_ASSETS_DIR}")

    # --- gui: ImGui user interface -------------------------------------------
    add_library(ars_gui STATIC
        src/gui/MainWindow.cpp
        src/gui/ControlPanel.cpp
        src/gui/Viewport.cpp)
    target_include_directories(ars_gui PUBLIC src)
    target_link_libraries(ars_gui PUBLIC ars_core ars_render ars_utils ars_compression
                                      imgui glm::glm)
    target_compile_definitions(ars_gui PUBLIC ARS_FRACTA_ASSETS_DIR="${ARS_FRACTA_ASSETS_DIR}")

    # --- main executable ------------------------------------------------------
    add_executable(ars_fracta src/main.cpp)
    target_include_directories(ars_fracta PRIVATE src)
    target_link_libraries(ars_fracta PRIVATE ars_gui ars_compression)
    target_compile_definitions(ars_fracta PRIVATE ARS_FRACTA_ASSETS_DIR="${ARS_FRACTA_ASSETS_DIR}")

    # Перекопируем ассеты (шейдеры, пресеты) при каждой сборке, чтобы правки
    # в assets/ попадали в build/assets без повторного configure.
    add_custom_command(TARGET ars_fracta POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "${ARS_FRACTA_ASSETS_DIR}")
endif()

# --- tests --------------------------------------------------------------------
if(ARS_FRACTA_BUILD_TESTS)
    enable_testing()
    add_executable(test_compression tests/test_compression.cpp)
    target_include_directories(test_compression PRIVATE src)
    target_link_libraries(test_compression PRIVATE ars_core ars_compression ars_utils)
    add_test(NAME compression COMMAND test_compression)
endif()