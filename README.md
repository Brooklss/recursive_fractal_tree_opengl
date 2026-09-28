# Recursive Fractal Tree — OpenGL + Matrix Stacks

A real-time, interactive fractal tree rendered with **OpenGL** (PyOpenGL + pygame).

The core algorithm uses a **hand-rolled matrix stack** (no `glPushMatrix`/`glPopMatrix`)
to manage all recursive branch transformations.

---

## Features

| Feature | Detail |
|---|---|
| **Matrix Stack** | Custom class: `push()`, `pop()`, `translate()`, `rotate_z()`, `scale()`, `apply()` |
| **Recursion** | Full binary tree up to depth 12 |
| **Wind animation** | Sinusoidal sway that grows with branch depth |
| **Color gradient** | Trunk warm brown → tips bright green |
| **Line taper** | 6 px trunk → 1 px leaf tips |
| **Live controls** | Keyboard adjusts all parameters in real-time |
| **HUD overlay** | In-window parameter readout with FPS counter |

---

## Requirements

Python 3.8+ with:

```bash
pip install -r requirements.txt
```

---

## Run

```bash
python fractal_tree.py
```

---

## Controls

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

## Architecture

```
fractal_tree.py
├── MatrixStack              ← custom push/pop transform stack
│   ├── push() / pop()       ← duplicate / restore top matrix
│   ├── translate(x, y)      ← right-multiply translation matrix
│   ├── rotate_z(deg)        ← right-multiply Z-rotation matrix
│   ├── scale(sx, sy)        ← right-multiply scale matrix
│   └── apply()              ← glLoadMatrixf(top)
├── FractalTree
│   ├── draw()               ← reset stack, place trunk, call _recurse()
│   ├── _recurse(length, d)  ← recursive branch drawing
│   ├── _branch_color(idx)   ← depth-based colour gradient
│   ├── _branch_width(idx)   ← depth-based line taper
│   └── _wind_sway(level)    ← sinusoidal animation offset
└── App                      ← pygame window, event loop, HUD overlay
```

### Recursive pattern

```
_recurse(length, depth):
  apply current transform          ← glLoadMatrixf(stack.top)
  draw segment from (0,0)→(0,length)
  push → translate(0, length)
    push → rotate_z(+angle+sway) → _recurse(length*ratio, depth-1) → pop
    push → rotate_z(-angle-sway) → _recurse(length*ratio, depth-1) → pop
  pop
```

---

## File structure

```
recursive_fractal_tree_opengl/
├── fractal_tree.py      ← main application
├── requirements.txt     ← Python dependencies
└── README.md
```
