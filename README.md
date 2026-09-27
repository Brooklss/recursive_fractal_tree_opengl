# Recursive Fractal Tree — OpenGL + Matrix Stacks

A real-time, interactive fractal tree rendered with **OpenGL**.  
Available in both **Python** (PyOpenGL + pygame) and **C++** (GLUT/freeglut).

The core algorithm uses a **hand-rolled matrix stack** (no `glPushMatrix`/`glPopMatrix`)
to manage all recursive branch transformations.

---

## Features

| Feature | Detail |
|---|---|
| **Matrix Stack** | Custom class: `push()`, `pop()`, `translate()`, `rotate_z()`, `scale()`, `apply()` |
| **Recursion** | Full binary tree up to depth 12 |
| **Wind animation** | Sinusoidal sway that grows with branch depth |
| **Color gradient** | Trunk (warm brown → tips bright green) |
| **Line taper** | 6 px trunk → 1 px leaf tips |
| **Live controls** | Keyboard adjusts all parameters in real-time |
| **HUD overlay** | In-window parameter readout |

---

## Controls (both versions)

| Key | Action |
|---|---|
| `↑` / `↓` | Recursion depth (1 – 12) |
| `←` / `→` | Branch angle |
| `W` / `S` | Length ratio |
| `A` / `D` | Trunk length |
| `SPACE` | Toggle wind animation |
| `R` | Reset all parameters |
| `ESC` | Quit |

---

## Python version

### Requirements
```bash
pip install -r requirements.txt
```

### Run
```bash
python fractal_tree.py
```

---

## C++ version

### Dependencies
Requires **freeglut** (provides OpenGL windowing via GLUT API).

**Install via vcpkg (recommended):**
```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
%USERPROFILE%\vcpkg\vcpkg install freeglut:x64-windows
```

### Build (Windows)
```bat
set VCPKG_ROOT=%USERPROFILE%\vcpkg
build.bat
```

Or manually with CMake:
```bat
mkdir build_cpp && cd build_cpp
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
         -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build . --config Release
```

### Run
```bat
build_cpp\Release\fractal_tree.exe
```

---

## Architecture

```
FractalTree
├── MatrixStack              ← custom push/pop transform stack
│   ├── push() / pop()       ← duplicate / restore top matrix
│   ├── translate(x, y)      ← right-multiply translation matrix
│   ├── rotate_z(deg)        ← right-multiply Z-rotation matrix
│   ├── scale(sx, sy)        ← right-multiply scale matrix
│   └── apply()              ← glLoadMatrixf(top)
├── draw()                   ← reset stack, place trunk, call recurse()
├── recurse(length, depth)   ← recursive branch drawing
├── branch_color(idx)        ← depth-based colour gradient
├── branch_width(idx)        ← depth-based line taper
└── wind_sway(level)         ← sinusoidal animation offset
```

### MatrixStack internals

All matrices are stored as **column-major** `float[16]` arrays — the same
format OpenGL's `glLoadMatrixf` expects.

```
Element at (row, col)  →  data[col*4 + row]

Matrix multiply (C = A * B):
  C[col*4+row] = Σ_k  A[k*4+row] * B[col*4+k]

Translation:
  T[12]=tx, T[13]=ty, T[14]=tz   (column 3)

Z-Rotation by angle a:
  R[0]=cos(a), R[1]=sin(a)    (column 0)
  R[4]=-sin(a), R[5]=cos(a)  (column 1)
```

### Recursive pattern

```
recurse(length, depth):
  apply current transform         ← glLoadMatrixf(stack.top)
  draw segment from (0,0)→(0,length)
  push → translate(0, length)
    push → rotate_z(+angle+sway) → recurse(length*ratio, depth-1) → pop
    push → rotate_z(-angle-sway) → recurse(length*ratio, depth-1) → pop
  pop
```

---

## File structure

```
recursive_fractal_tree_opengl/
├── fractal_tree.py      ← Python implementation (PyOpenGL + pygame)
├── fractal_tree.cpp     ← C++ implementation (OpenGL + GLUT)
├── CMakeLists.txt       ← CMake build for C++ version
├── build.bat            ← Windows build helper (vcpkg)
├── requirements.txt     ← Python dependencies
└── README.md
```
