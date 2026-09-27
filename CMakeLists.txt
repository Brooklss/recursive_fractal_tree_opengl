cmake_minimum_required(VERSION 3.15)
project(FractalTree CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find dependencies ──────────────────────────────────────────
find_package(OpenGL REQUIRED)
find_package(GLUT   REQUIRED)

# ── Executable ────────────────────────────────────────────────
add_executable(fractal_tree fractal_tree.cpp)

target_link_libraries(fractal_tree
    PRIVATE
        OpenGL::GL
        OpenGL::GLU
        GLUT::GLUT
)

# ── Compiler warnings ─────────────────────────────────────────
if(MSVC)
    target_compile_options(fractal_tree PRIVATE /W4 /WX-)
    # Copy freeglut DLL next to the executable on Windows
    add_custom_command(TARGET fractal_tree POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:fractal_tree>
            $<TARGET_FILE_DIR:fractal_tree>
        COMMAND_EXPAND_LISTS
    )
else()
    target_compile_options(fractal_tree PRIVATE -Wall -Wextra -Wpedantic)
endif()
