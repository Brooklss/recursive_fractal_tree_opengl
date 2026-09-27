# Recursive Fractal Tree — Complete Code Explanation

---

## Table of Contents

1. [High-Level Architecture](#1-high-level-architecture)
2. [The Math: Column-Major Matrices](#2-the-math-column-major-matrices)
3. [Python: Imports & Setup](#3-python-imports--setup)
4. [Python: MatrixStack class](#4-python-matrixstack-class)
5. [Python: FractalTree class](#5-python-fractaltree-class)
6. [Python: App class (window, HUD, loop)](#6-python-app-class)
7. [C++: Includes & Type aliases](#7-c-includes--type-aliases)
8. [C++: mat_identity & mat_mul functions](#8-c-mat_identity--mat_mul)
9. [C++: MatrixStack class](#9-c-matrixstack-class)
10. [C++: FractalTree class](#10-c-fractaltree-class)
11. [C++: HUD rendering](#11-c-hud-rendering)
12. [C++: GLUT callbacks](#12-c-glut-callbacks)
13. [C++: main()](#13-c-main)
14. [How the Build Works](#14-how-the-build-works)

---

## 1. High-Level Architecture

Both versions have **identical logic** split across the same three layers:

```
┌────────────────────────────────────────────────────────┐
│  App / main()         Window, event loop, timing       │
│  ┌──────────────────────────────────────────────────┐  │
│  │  FractalTree        Parameters, animation, draw  │  │
│  │  ┌────────────────────────────────────────────┐  │  │
│  │  │  MatrixStack    push/pop transform stack   │  │  │
│  │  └────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

Every frame:
1. `App` / `on_display()` measures elapsed time (dt)
2. Calls `tree.update(dt)` — advances wind timer
3. Clears the screen
4. Calls `tree.draw()` — resets stack, recurses
5. Draws the HUD on top
6. Swaps the double buffer to the screen

---

## 2. The Math: Column-Major Matrices

OpenGL stores matrices in **column-major** order, meaning columns are laid out sequentially in memory.

A 4×4 matrix with element at **(row, col)** lives at flat index `col * 4 + row`:

```
Matrix (row, col notation):         Flat array indices:
┌                    ┐              ┌                       ┐
│ m[0,0] m[0,1] ... │              │  0   4   8  12        │
│ m[1,0] m[1,1] ... │    →  flat   │  1   5   9  13        │
│ m[2,0] m[2,1] ... │              │  2   6  10  14        │
│ m[3,0] m[3,1] ... │              │  3   7  11  15        │
└                    ┘              └                       ┘
```

**Identity matrix** — 1s on the diagonal (indices 0, 5, 10, 15):
```
1 0 0 0
0 1 0 0
0 0 1 0
0 0 0 1
```
Multiplying anything by the identity leaves it unchanged.

**Translation matrix** — tx, ty go into column 3 (indices 12, 13):
```
1 0 0 tx       flat: [..., tx at index 12, ty at 13, tz at 14 ...]
0 1 0 ty
0 0 1 tz
0 0 0  1
```

**Z-Rotation matrix** by angle `a`:
```
cos(a)  -sin(a)  0  0       Column 0: ( cos, sin, 0, 0 ) → indices 0,1,2,3
sin(a)   cos(a)  0  0       Column 1: (-sin, cos, 0, 0 ) → indices 4,5,6,7
  0        0     1  0
  0        0     0  1
```

**Matrix multiply** (C = A × B), column-major:
```
C[col*4 + row] = Σ(k=0..3)  A[k*4 + row]  ×  B[col*4 + k]
```
This accumulates all transforms: each new transform is right-multiplied, meaning it is applied *after* all previous transforms already in the matrix.

---

## 3. Python: Imports & Setup

```python
import sys          # sys.exit() to quit cleanly
import math         # math.radians(), math.cos(), math.sin()
import pygame       # window creation, event loop, font rendering
from pygame.locals import *  # brings K_UP, K_ESCAPE, DOUBLEBUF, etc. into scope

from OpenGL.GL import *   # all glXxx() functions: glBegin, glVertex2f, glColor3f ...
from OpenGL.GLU import *  # gluOrtho2D() for 2D projection
import numpy as np         # matrix/array math (np.eye, np.array, @-operator)
```

- **pygame** handles the OS window and event polling. It doesn't know about OpenGL natively, but it can create an OpenGL-capable surface with the `OPENGL` flag.
- **PyOpenGL** (`OpenGL.GL`) wraps every OpenGL C function into Python. The `*` import means you call `glBegin(...)` exactly like C.
- **numpy** provides fast N-dimensional arrays. We use 4×4 float32 arrays as matrices and `@` for matrix multiplication.

---

## 4. Python: MatrixStack class

```python
class MatrixStack:
```
A class that owns a Python list (used as a LIFO stack) of 4×4 numpy matrices.

---

```python
    def __init__(self):
        self._stack = [np.eye(4, dtype=np.float32)]
```
- `np.eye(4)` creates the 4×4 identity matrix.
- `dtype=np.float32` — OpenGL wants 32-bit floats, not Python's default 64-bit.
- We start with one identity matrix already on the stack — this is the "base" transform (no movement, no rotation).

---

```python
    @property
    def top(self) -> np.ndarray:
        return self._stack[-1]
```
- `@property` makes `stack.top` a readable attribute (no parentheses needed).
- `self._stack[-1]` is Python's way of accessing the **last element** of a list.

---

```python
    def push(self):
        self._stack.append(self._stack[-1].copy())
```
- `.copy()` is critical — without it, both the old and new entry would point to the **same** numpy array. Any changes to the new entry would corrupt the saved one.
- After push, the stack has one more entry that is an *exact duplicate* of what was there before.

---

```python
    def pop(self):
        if len(self._stack) <= 1:
            raise RuntimeError("MatrixStack underflow")
        self._stack.pop()
```
- Guard: we always need at least the base identity matrix. Popping below 1 is a bug.
- `list.pop()` removes and discards the last element (the top of the stack).

---

```python
    def translate(self, tx: float, ty: float, tz: float = 0.0):
        T = np.eye(4, dtype=np.float32)  # start from identity
        T[0, 3] = tx   # row 0, column 3 = tx
        T[1, 3] = ty   # row 1, column 3 = ty
        T[2, 3] = tz   # row 2, column 3 = tz
        self._stack[-1] = self._stack[-1] @ T
```
- Builds a 4×4 translation matrix. In numpy, `T[row, col]` uses row-major indexing, so `T[0, 3]` is correct for placing tx at (row=0, col=3).
- `@` is Python's **matrix multiply** operator (PEP 465). `A @ B` computes A×B.
- We **right-multiply**: `current_top × T`. This means the translation is applied *after* whatever transform was already accumulated in the top matrix.

---

```python
    def rotate_z(self, angle_deg: float):
        a = math.radians(angle_deg)    # convert degrees → radians
        c, s = math.cos(a), math.sin(a)
        R = np.array([
            [c, -s, 0, 0],
            [s,  c, 0, 0],
            [0,  0, 1, 0],
            [0,  0, 0, 1],
        ], dtype=np.float32)
        self._stack[-1] = self._stack[-1] @ R
```
- Creates a standard 2D rotation matrix extended to 4D homogeneous form.
- OpenGL's coordinate system: positive angles rotate **counter-clockwise** in the XY plane.
- Right-multiplied the same way as translation.

---

```python
    def scale(self, sx, sy, sz=1.0):
        S = np.diag([sx, sy, sz, 1.0]).astype(np.float32)
        self._stack[-1] = self._stack[-1] @ S
```
- `np.diag([...])` creates a diagonal matrix — scale factors on the diagonal, zeros everywhere else.
- `astype(np.float32)` converts to 32-bit float after creation.

---

```python
    def apply(self):
        glLoadMatrixf(self.top.T)   # OpenGL expects column-major
```
- `glLoadMatrixf` replaces OpenGL's internal MODELVIEW matrix with ours.
- The `.T` is a numpy **transpose**. numpy stores arrays in row-major order internally, so `self.top` has rows laid out sequentially in memory. Transposing it makes columns sequential — which is exactly what OpenGL's column-major convention requires.
- After this call, every `glVertex2f` you draw will be transformed by our accumulated matrix.

---

## 5. Python: FractalTree class

```python
    def __init__(self):
        self.depth          = 8       # how many levels of recursion
        self.angle          = 25.0    # degrees each branch splits left/right
        self.length_ratio   = 0.68    # each child is 68% the length of its parent
        self.trunk_length   = 0.40    # starting length in OpenGL world units
        self.wind_enabled   = True
        self._time          = 0.0     # accumulates elapsed seconds for animation
        self._stack = MatrixStack()
```

The **world units** here map to OpenGL's normalized device coordinates: the screen spans [-1, 1] in both X and Y, so 0.40 units = 20% of the half-screen height.

---

```python
    def update(self, dt: float):
        if self.wind_enabled:
            self._time += dt
```
- `dt` is the time in seconds since the last frame (typically ~0.016 s at 60 fps).
- Accumulating `dt` gives a monotonically increasing timer used by the sway formula.

---

```python
    def _wind_sway(self, depth: int) -> float:
        freq   = 0.8 + depth * 0.15   # higher branches oscillate faster
        amp    = 1.0 + depth * 0.5    # higher branches swing further
        phase  = depth * 0.4          # each level starts at a different point in its cycle
        return amp * math.sin(self._time * freq + phase)
```
- Returns an extra rotation in degrees to add to the branch angle.
- `sin()` oscillates between -1 and +1, multiplied by `amp` for the actual swing range.
- Without `phase`, all branches would swing in perfect sync — the phase offset makes the tree look organic.
- `depth` here is `self.depth - remaining_depth + 1`, so it increases as we go further from the trunk.

---

```python
    def _branch_color(self, depth: int, max_depth: int) -> tuple:
        t = depth / max(max_depth, 1)
        r = 0.55 + t * (0.18 - 0.55)   # from 0.55 (trunk brown) to 0.18 (leaf green)
        g = 0.27 + t * (0.80 - 0.27)
        b = 0.07 + t * (0.25 - 0.07)
        return (r, g, b)
```
- **Linear interpolation** (lerp): `start + t * (end - start)`.
- `t` goes from 0.0 (trunk, `depth=0`) to 1.0 (leaf tips, `depth=max_depth`).
- At `t=0`: RGB = (0.55, 0.27, 0.07) — warm brown bark color.
- At `t=1`: RGB = (0.18, 0.80, 0.25) — bright green leaf color.

---

```python
    def _branch_width(self, depth: int, max_depth: int) -> float:
        t = depth / max(max_depth, 1)
        return max(1.0, 6.0 * (1.0 - t))
```
- At `t=0` (trunk): width = `6.0 * 1.0 = 6.0` pixels.
- At `t=1` (tips): width = `max(1.0, 6.0 * 0.0) = 1.0` pixel (floor at 1).
- `glLineWidth()` controls the thickness of OpenGL line primitives.

---

```python
    def _draw_segment(self, length: float):
        glBegin(GL_LINES)
        glVertex2f(0.0, 0.0)
        glVertex2f(0.0, length)
        glEnd()
```
- `glBegin(GL_LINES)` tells OpenGL the next vertices define line segments (pairs of points).
- We always draw from the local origin `(0,0)` straight **up** to `(0, length)`.
- The MatrixStack's current transform rotates/translates this line into the correct world position.
- `glEnd()` closes the primitive.

---

### The core recursive function

```python
    def _recurse(self, length: float, depth: int):
        if depth == 0 or length < 0.002:
            return
```
- **Base cases**: stop when we've reached the configured depth limit, or when the branch is too short to be visible (< 0.002 world units).

---

```python
        color = self._branch_color(self.depth - depth, self.depth)
        width = self._branch_width(self.depth - depth, self.depth)
        glColor3f(*color)
        glLineWidth(width)
```
- `self.depth - depth` converts "remaining depth" to "depth index from trunk".
  - At the first call, `depth = self.depth`, so index = 0 (trunk).
  - At deepest recursion, `depth = 1`, so index = `self.depth - 1` (tips).
- `*color` unpacks the `(r, g, b)` tuple into three separate arguments.

---

```python
        self._stack.apply()
        self._draw_segment(length)
```
- **Sync our matrix to OpenGL first**, then draw.
- The segment always goes from (0,0) to (0,length) in local space; the matrix handles world placement.

---

```python
        self._stack.push()
        self._stack.translate(0.0, length)
```
- **Push** saves the current transform (at the base of this branch).
- **Translate** moves the origin to the **tip** of the just-drawn segment.
- All subsequent push/pops work relative to the tip.

---

```python
        sway = self._wind_sway(self.depth - depth + 1)

        # Left child
        self._stack.push()
        self._stack.rotate_z(self.angle + sway)
        self._recurse(length * self.length_ratio, depth - 1)
        self._stack.pop()

        # Right child
        self._stack.push()
        self._stack.rotate_z(-(self.angle + sway * 0.7))
        self._recurse(length * self.length_ratio, depth - 1)
        self._stack.pop()

        self._stack.pop()   # restore to base of this branch
```

This is the **entire branching algorithm** in six effective lines:

| Step | Stack contents |
|---|---|
| Before left branch | `[..., tip_transform]` |
| After left `push` | `[..., tip_transform, tip_transform_copy]` |
| After left `rotate_z` | `[..., tip_transform, tip_rotated_left]` |
| Left `_recurse` | draws entire left subtree |
| After left `pop` | `[..., tip_transform]` — back to tip |
| After right `push + rotate` | `[..., tip_transform, tip_rotated_right]` |
| Right `_recurse` | draws entire right subtree |
| After right `pop` | `[..., tip_transform]` |
| Final `pop` | `[...]` — back to base of this branch |

The right child uses `sway * 0.7` instead of `sway` to make the tree's sway **asymmetric** — one side swings slightly less than the other, which looks more natural.

---

```python
    def draw(self):
        self._stack = MatrixStack()          # fresh stack, identity at base
        self._stack.translate(0.0, -0.85)   # move trunk base near bottom of window
        self._recurse(self.trunk_length, self.depth)
```
- A fresh `MatrixStack()` is created each frame (avoids any accumulated state).
- The `-0.85` Y translation places the trunk base 85% of the way down in the [-1,1] world space.

---

## 6. Python: App class

```python
    def __init__(self):
        pygame.init()
        pygame.display.set_caption(self.TITLE)
        flags = DOUBLEBUF | OPENGL
        pygame.display.set_mode((self.WIDTH, self.HEIGHT), flags)
```
- `pygame.init()` initializes all pygame subsystems (display, font, events).
- `DOUBLEBUF` — uses **double buffering**: render into a back buffer, then swap to screen. Without this you'd see tearing.
- `OPENGL` — tells pygame to create an OpenGL context instead of a normal 2D surface.

---

```python
    def _setup_gl(self):
        glClearColor(0.06, 0.06, 0.10, 1.0)   # background: near-black blue-tinted
        glEnable(GL_LINE_SMOOTH)               # anti-aliased line edges
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST) # use best quality anti-aliasing
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
```
- `GL_BLEND` enables transparency. Without it, `glColor4f(..., 0.5)` would be fully opaque.
- `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)` is standard alpha blending:
  `result = src_color * src_alpha + dst_color * (1 - src_alpha)`.

---

```python
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
```
- OpenGL has two main matrices: **PROJECTION** (how 3D maps to 2D screen) and **MODELVIEW** (object placement).
- `glOrtho(-1,1,-1,1,-1,1)` sets up an **orthographic projection**: no perspective distortion. X and Y both span [-1, 1]. Our tree lives in this space.
- We switch back to MODELVIEW so all subsequent drawing operations use the model matrix.

---

### The HUD (Heads-Up Display)

```python
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()        # save the [-1,1] projection
        glLoadIdentity()
        glOrtho(0, self.WIDTH, 0, self.HEIGHT, -1, 1)  # switch to pixel coords
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
```
- We **temporarily switch** the projection to pixel space (0..900 x 0..900) so we can place text at exact pixel positions.
- `glPushMatrix()` / `glPopMatrix()` here use OpenGL's *built-in* stack (not our custom one) just to save/restore the projection — that's fine for the HUD.

---

```python
        surf = self._font.render(line, True, (200, 230, 200))
        tex_data = pygame.image.tostring(surf, "RGBA", True)
        tex_id = glGenTextures(1)
        glBindTexture(GL_TEXTURE_2D, tex_id)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data)
```
- pygame's font rendering produces a pygame **Surface** (a CPU-side image).
- `tostring(..., "RGBA", True)` converts it to raw bytes OpenGL can consume. The `True` flips it vertically (pygame and OpenGL have opposite Y conventions).
- `glGenTextures(1)` allocates a new GPU texture slot.
- `glTexImage2D` uploads the pixel data to the GPU.

---

```python
        glBegin(GL_QUADS)
        glTexCoord2f(0, 0); glVertex2f(12,      y)
        glTexCoord2f(1, 0); glVertex2f(12 + tw, y)
        glTexCoord2f(1, 1); glVertex2f(12 + tw, y + th)
        glTexCoord2f(0, 1); glVertex2f(12,      y + th)
        glEnd()
        glDeleteTextures(1, [tex_id])
```
- Draws a textured rectangle. Each vertex gets a UV coordinate (`glTexCoord2f`) mapping the texture corners to the quad corners.
- `glDeleteTextures` immediately frees the GPU memory — since we re-create the texture every frame, we must clean up or we'd leak GPU memory.

---

### The game loop

```python
    def run(self):
        running = True
        while running:
            dt = self.clock.tick(60) / 1000.0  # cap at 60 fps, convert ms → seconds
            running = self._handle_events()
            self.tree.update(dt)
            glClear(GL_COLOR_BUFFER_BIT)        # erase previous frame
            self.tree.draw()
            self._draw_hud()
            pygame.display.flip()               # swap back buffer → screen
```
- `clock.tick(60)` sleeps enough to cap at 60 fps and returns the actual ms elapsed.
- `glClear(GL_COLOR_BUFFER_BIT)` fills the screen with `glClearColor` (our dark blue).
- `pygame.display.flip()` presents the completed frame.

---

## 7. C++: Includes & Type aliases

```cpp
#define _USE_MATH_DEFINES
```
On MSVC (Microsoft's compiler), `M_PI` is not defined by default — it's a POSIX extension. This `#define` enables it before including `<cmath>`.

```cpp
#include <GL/glut.h>
```
freeglut's main header. It internally includes `<GL/gl.h>` and `<GL/glu.h>`, giving us all `glXxx` and `gluXxx` functions, plus GLUT's own `glutXxx` window/event functions.

```cpp
#include <algorithm>  // std::min, std::max
#include <array>      // std::array (fixed-size arrays)
#include <cmath>      // std::cos, std::sin, M_PI
#include <cstdlib>    // std::exit()
#include <iomanip>    // std::fixed, std::setprecision (number formatting)
#include <sstream>    // std::ostringstream (build strings)
#include <stdexcept>  // std::runtime_error
#include <string>     // std::string
#include <vector>     // std::vector (dynamic array, used for the stack)
```

```cpp
using Mat4 = std::array<float, 16>;
```
- `using` creates a **type alias**. `Mat4` is now a shorthand for `std::array<float, 16>`.
- `std::array<float, 16>` is a stack-allocated array of exactly 16 floats — our column-major 4×4 matrix laid out flat.
- Unlike a raw `float[16]`, `std::array` can be returned by value, copied, and assigned with `=`.

---

## 8. C++: mat_identity & mat_mul

```cpp
static Mat4 mat_identity()
{
    Mat4 m{};           // value-initialize: all 16 floats set to 0.0f
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}
```
- `Mat4 m{}` — the `{}` triggers **value initialization**, zeroing all elements. Without it, the array would have garbage values.
- Indices 0, 5, 10, 15 correspond to (row=0,col=0), (row=1,col=1), (row=2,col=2), (row=3,col=3) — the diagonal.
- `static` means this function has **internal linkage** — it's only visible in this translation unit (this .cpp file). It prevents symbol conflicts if the project ever has multiple .cpp files.

---

```cpp
static Mat4 mat_mul(const Mat4& A, const Mat4& B)
{
    Mat4 C{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                C[col*4+row] += A[k*4+row] * B[col*4+k];
    return C;
}
```
- `const Mat4&` — pass by **const reference**: no copy of the array, and the function promises not to modify it.
- The triple loop implements the standard matrix multiplication formula for column-major storage.
- `C[col*4+row]` — element at (row, col) of result C.
- `A[k*4+row]` — element at (row, k) of A.
- `B[col*4+k]` — element at (k, col) of B.
- Each element of C is the **dot product** of a row of A with a column of B.

---

## 9. C++: MatrixStack class

```cpp
class MatrixStack
{
public:
    MatrixStack() { reset(); }
```
The constructor immediately calls `reset()` to put one identity matrix on the stack.

---

```cpp
    void push()
    {
        _s.push_back(_s.back());
    }
```
- `_s.back()` gets a reference to the last (top) element.
- `push_back` appends a **copy** of that element.
- Because `Mat4` is `std::array`, copying it copies all 16 floats — no hidden pointer sharing.

---

```cpp
    void pop()
    {
        if (_s.size() <= 1)
            throw std::runtime_error("MatrixStack underflow");
        _s.pop_back();
    }
```
- `_s.size()` returns `std::size_t` (unsigned). Comparing to 1 is safe.
- `_s.pop_back()` removes the last element and calls its destructor (trivial for float arrays).

---

```cpp
    void translate(float tx, float ty, float tz = 0.0f)
    {
        Mat4 T = mat_identity();
        T[12] = tx;  T[13] = ty;  T[14] = tz;
        _s.back() = mat_mul(_s.back(), T);
    }
```
- `T[12]`, `T[13]`, `T[14]` are column 3, rows 0/1/2 respectively (`col*4+row = 3*4+0 = 12`).
- Right-multiply: `top = top × T`.

---

```cpp
    void rotate_z(float deg)
    {
        const float a = deg * static_cast<float>(M_PI) / 180.0f;
        const float c = std::cos(a), s = std::sin(a);
        Mat4 R = mat_identity();
        R[0] =  c;  R[1] = s;    // column 0: (c, s, 0, 0)
        R[4] = -s;  R[5] = c;    // column 1: (-s, c, 0, 0)
        _s.back() = mat_mul(_s.back(), R);
    }
```
- `static_cast<float>(M_PI)` — `M_PI` is a `double`. Casting avoids a compiler warning about narrowing conversion to `float`.
- `R[0] = c, R[1] = s` fills column 0 (indices 0 and 1).
- `R[4] = -s, R[5] = c` fills column 1 (indices 4 and 5).

---

```cpp
    void apply() const
    {
        glLoadMatrixf(_s.back().data());
    }
```
- `.data()` returns a raw `const float*` pointer to the underlying array memory.
- `glLoadMatrixf` expects a pointer to 16 floats in **column-major** order — which is exactly what our `Mat4` stores.
- No transpose needed here (unlike Python), because C++'s `std::array` already stores its data sequentially in column order.

---

```cpp
private:
    std::vector<Mat4> _s;
```
- `std::vector` is a dynamic array that grows automatically.
- Each element is a `Mat4` (16 floats = 64 bytes). With depth=8 and 2 branches per level, the maximum stack depth is about 17 matrices = ~1 KB. Trivial.

---

## 10. C++: FractalTree class

```cpp
struct Color3 { float r, g, b; };
```
A simple **aggregate struct** — no constructor needed, initialize with `{ r, g, b }` syntax.

---

```cpp
static Color3 branch_color(int idx, int max_depth)
{
    const float t = max_depth > 0 ? static_cast<float>(idx) / max_depth : 0.0f;
    return { 0.55f + t*(0.18f - 0.55f),
             0.27f + t*(0.80f - 0.27f),
             0.07f + t*(0.25f - 0.07f) };
}
```
- The ternary `max_depth > 0 ? ... : 0.0f` guards against division by zero.
- `static_cast<float>(idx)` — integer division would give 0 or 1; cast to float first.
- The `return { ... }` uses **aggregate initialization** to construct a `Color3` in-place.

---

```cpp
static float branch_width(int idx, int max_depth)
{
    const float t = max_depth > 0 ? static_cast<float>(idx) / max_depth : 0.0f;
    return std::max(1.0f, 6.0f * (1.0f - t));
}
```
- `std::max(1.0f, ...)` — the `1.0f` suffix is essential. `std::max(1, ...)` would be an integer version and cause a type mismatch compiler error.

---

```cpp
    void recurse(float length, int rem)
    {
        if (rem == 0 || length < 0.002f) return;

        const int    idx   = depth - rem;
        const Color3 col   = branch_color(idx, depth);
        const float  sway  = wind_sway(idx + 1);
        const float  next  = length * length_ratio;
```
- `rem` = "remaining" recursion levels.
- `idx = depth - rem` converts remaining to index-from-trunk (0 = trunk, depth-1 = tips).
- `next` pre-computes the child length (avoids repeating the multiply twice).

---

```cpp
        glColor3f(col.r, col.g, col.b);
        glLineWidth(branch_width(idx, depth));
        _stack.apply();

        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f);
            glVertex2f(0.0f, length);
        glEnd();
```
Same logic as Python. Set color, set width, sync transform to OpenGL, draw vertical line from local origin to local tip.

---

```cpp
        _stack.push();
        _stack.translate(0.0f, length);

            _stack.push();
            _stack.rotate_z(angle + sway);
            recurse(next, rem - 1);
            _stack.pop();

            _stack.push();
            _stack.rotate_z(-(angle + sway * 0.7f));
            recurse(next, rem - 1);
            _stack.pop();

        _stack.pop();
```
Identical branching algorithm to Python. The indentation is just style — it visually shows which push/pop pairs match.

---

## 11. C++: HUD rendering

```cpp
static void render_string(float x, float y, const std::string& s)
{
    glRasterPos2f(x, y);
    for (char c : s)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
}
```
- `glRasterPos2f(x, y)` sets the **raster position** — the pixel coordinate where the next bitmap character will be drawn.
- `GLUT_BITMAP_8_BY_13` is a built-in fixed-width bitmap font (8 pixels wide, 13 pixels tall per character).
- The for loop renders one character at a time; GLUT automatically advances the raster position rightward after each character.
- This is simpler than Python's approach (no GPU textures needed) because GLUT includes basic bitmap fonts. The tradeoff is you can't choose font style or size.

---

```cpp
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
```
- `gluOrtho2D` is a helper that calls `glOrtho` with near/far = -1/1. It switches the projection to pixel space so we can place text at exact pixel coordinates.
- `glPushMatrix()` here saves the [-1,1] world projection so we can restore it after the HUD.

---

```cpp
    auto line = [&](const std::string& text) {
        render_string(X0, Y0 - row++ * LINE_H, text);
    };
```
- `auto line = [&](...)` is a **C++ lambda** — an anonymous function defined inline.
- `[&]` **capture by reference**: the lambda can read and write `row`, `X0`, `Y0`, `LINE_H` from the surrounding scope.
- `row++` uses post-increment: it returns the current value of `row`, then increments it. So each line is placed 21 pixels below the previous one.

---

```cpp
    ss.str(""); ss << std::fixed << std::setprecision(2) << g_tree.length_ratio;
```
- `ss.str("")` resets the string stream (clears previous content).
- `std::fixed` + `std::setprecision(2)` formats the following float with exactly 2 decimal places (e.g. `0.68`).
- `<<` is the stream insertion operator — builds the string piece by piece.

---

## 12. C++: GLUT callbacks

GLUT works by **registering callback functions** that it calls automatically when events happen. You don't write your own event loop — GLUT's `glutMainLoop()` runs it for you.

```cpp
static void on_display()
{
    const int now = glutGet(GLUT_ELAPSED_TIME);   // ms since program start
    const float dt = (now - g_prev_ms) / 1000.0f;
    g_prev_ms = now;
    ...
    glutSwapBuffers();  // present the completed frame (double-buffer swap)
}
```
- `glutGet(GLUT_ELAPSED_TIME)` returns an integer millisecond counter — the C++ equivalent of pygame's clock.
- We compute `dt` by subtracting the previous timestamp, then store the current one for next frame.

---

```cpp
static void on_timer(int /*v*/)
{
    glutPostRedisplay();          // ask GLUT to call on_display() soon
    glutTimerFunc(16, on_timer, 0);  // re-arm the timer for ~60fps
}
```
- GLUT's timer fires **once** then stops. To get continuous animation, `on_timer` re-registers itself every time it fires.
- `16` milliseconds ≈ 60 fps. (1000ms / 60 = 16.67ms)
- `/*v*/` — the parameter name is commented out because we don't use it. This silences the "unused parameter" warning.
- `glutPostRedisplay()` marks the window as needing a redraw; GLUT calls `on_display()` at the next opportunity.

---

```cpp
static void on_reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}
```
- Called whenever the window is **resized**.
- `glViewport` tells OpenGL which portion of the window to render into (the whole window).
- We reset the projection to maintain the correct [-1,1] coordinate space regardless of window size.

---

```cpp
static void on_keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key) {
        case 27: case 'q': case 'Q': std::exit(0);
```
- GLUT separates regular printable keys (`on_keyboard`) from special keys like arrows (`on_special`).
- `unsigned char key` is the ASCII code of the pressed key.
- `27` is the ASCII code for ESC.
- `std::exit(0)` terminates the process cleanly (0 = success).

---

```cpp
        case 'r': case 'R': g_tree = FractalTree(); break;
```
- `FractalTree()` constructs a **new** default FractalTree object.
- `g_tree = ...` uses the compiler-generated **copy assignment operator** to overwrite all fields of `g_tree` with the defaults.
- This is the C++ equivalent of Python's `self.tree = FractalTree()`.

---

```cpp
static void on_special(int key, int /*x*/, int /*y*/)
{
    switch (key) {
        case GLUT_KEY_UP:    g_tree.depth = std::min(g_tree.depth + 1, 12); break;
```
- Arrow keys, function keys, and modifier keys come through `glutSpecialFunc`.
- `GLUT_KEY_UP`, `GLUT_KEY_DOWN`, etc. are integer constants defined by freeglut.
- `std::min(g_tree.depth + 1, 12)` clamps the value: if `depth+1 > 12`, it returns 12.

---

## 13. C++: main()

```cpp
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
```
- `glutInit` must be called first. It processes any GLUT-specific command-line arguments (like `--display`) and initializes the GLUT library.
- `&argc, argv` passes the command-line arguments by pointer so GLUT can remove the ones it handles.

---

```cpp
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
```
- `GLUT_DOUBLE` — request double buffering (render to back buffer, swap to screen).
- `GLUT_RGBA` — request an RGBA color buffer (red, green, blue, alpha channels).
- These are **bitflags** combined with `|` (bitwise OR).

---

```cpp
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Recursive Fractal Tree — OpenGL + Matrix Stacks (C++)");
```
- Sets the initial window dimensions, then creates and shows the OS window.
- After `glutCreateWindow`, an OpenGL context is active and `gl*` calls are valid.

---

```cpp
    glutDisplayFunc(on_display);
    glutReshapeFunc(on_reshape);
    glutKeyboardFunc(on_keyboard);
    glutSpecialFunc(on_special);
    glutTimerFunc(16, on_timer, 0);
```
- Registers all callbacks. GLUT will call these functions at the appropriate times.
- `glutTimerFunc(16, on_timer, 0)` — fire `on_timer(0)` after 16ms. `on_timer` then re-arms itself forever.

---

```cpp
    g_prev_ms = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    return 0;
}
```
- Initialize the timing baseline before entering the loop.
- `glutMainLoop()` **never returns** — it runs GLUT's event loop indefinitely until the process exits.
- `return 0` is technically unreachable, but required by the C++ standard for `main`.

---

## 14. How the Build Works

Building the C++ version required several steps since no compiler was available initially.

### Step 1 — CMake (installed via winget)
```
winget install --id Kitware.CMake
```
CMake is a **meta-build system**: it generates actual build files (Makefiles, Visual Studio .sln, etc.) from a `CMakeLists.txt` description. We installed it but ultimately used a simpler direct g++ command instead.

### Step 2 — MSYS2 was already installed at `C:\msys64`
MSYS2 is a collection of Unix-like tools and a package manager (`pacman`) ported to Windows. It provides:
- A bash shell
- The `pacman` package manager (same as Arch Linux)
- The MinGW-w64 toolchain: GCC/G++ that produces **native Windows `.exe` files** (not Unix binaries)

### Step 3 — Installing the toolchain via pacman
```bash
pacman -S --needed --noconfirm \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-freeglut \
    mingw-w64-x86_64-make
```
- `--needed` — skip packages already installed.
- `--noconfirm` — answer "yes" to all prompts automatically.
- `mingw-w64-x86_64-gcc` — GCC 16 compiler for 64-bit Windows targets.
- `mingw-w64-x86_64-freeglut` — freeglut headers (`GL/glut.h`) and the import library (`libfreeglut.a`) plus the DLL (`freeglut.dll`).

### Step 4 — Compiling
```bash
/mingw64/bin/g++ fractal_tree.cpp -o fractal_tree.exe \
    -std=c++17 -O2 \
    -I/mingw64/include \
    -L/mingw64/lib \
    -lfreeglut -lopengl32 -lglu32
```

| Flag | Meaning |
|---|---|
| `-std=c++17` | Use C++17 standard (needed for `if constexpr`, structured bindings, etc.) |
| `-O2` | Optimization level 2 — fast code, reasonable compile time |
| `-I/mingw64/include` | Add freeglut's header directory to the include search path |
| `-L/mingw64/lib` | Add freeglut's library directory to the linker search path |
| `-lfreeglut` | Link against `libfreeglut.a` (the import library for `freeglut.dll`) |
| `-lopengl32` | Link Windows' built-in OpenGL implementation |
| `-lglu32` | Link Windows' GLU (OpenGL Utility Library) for `gluOrtho2D` |

This produced `fractal_tree.exe` (158 KB).

### Step 5 — Running: why PATH matters
```
set PATH=C:\msys64\mingw64\bin;%PATH%
fractal_tree.exe
```
The `.exe` was compiled to **dynamically link** `freeglut.dll`. At runtime, Windows searches for DLLs in:
1. The exe's own directory
2. Directories in `PATH`

`freeglut.dll` lives in `C:\msys64\mingw64\bin`. Adding that to `PATH` lets Windows find it. Without this, you'd get a "missing DLL" error dialog on startup.

### Why we used bash for compilation
When we ran `g++` directly from PowerShell, the stderr output was silently swallowed — a known issue with how PowerShell handles inherited handles from MSYS2 processes. Running through `C:\msys64\usr\bin\bash.exe -c "..."` gave g++ a proper POSIX-compatible stdio environment where it could write its error messages correctly.
