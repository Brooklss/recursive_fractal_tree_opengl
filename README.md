# Recursive Fractal Tree — OpenGL + Python

A real-time, interactive fractal tree rendered with **PyOpenGL** and **pygame**.  
The core algorithm uses a **hand-rolled matrix stack** (no `glPushMatrix`/`glPopMatrix`) to manage recursive branch transformations.

---

## Features

| Feature | Detail |
|---|---|
| **Matrix Stack** | Custom `MatrixStack` class with `push()`, `pop()`, `translate()`, `rotate_z()`, `scale()`, and `apply()` |
| **Recursion** | Full binary tree recursion up to depth 12 |
| **Wind animation** | Sinusoidal sway, scaled by branch depth |
| **Color gradient** | Trunk (warm brown) → tips (bright green) |
| **Line taper** | Thick trunk lines, thin leaf lines |
| **Live controls** | Keyboard adjusts depth, angle, length ratio, trunk length |
| **HUD overlay** | In-window parameter readout |

---

## Quick Start

```bash
pip install -r requirements.txt
python fractal_tree.py
```

---

## Controls

| Key | Action |
|---|---|
| `↑` / `↓` | Increase / decrease recursion depth (1–12) |
| `←` / `→` | Decrease / increase branch angle |
| `W` / `S` | Increase / decrease length ratio |
| `A` / `D` | Decrease / increase trunk length |
| `SPACE` | Toggle wind animation |
| `R` | Reset all parameters |
| `ESC` | Quit |

---

## Architecture

```
App
 └── FractalTree
      ├── MatrixStack          ← custom push/pop transform stack
      ├── _recurse()           ← recursive branch drawing
      ├── _branch_color()      ← depth-based color gradient
      ├── _branch_width()      ← depth-based line tapering
      └── _wind_sway()         ← sinusoidal animation offset
```

### MatrixStack internals

```python
stack.push()          # clone top matrix
stack.translate(x, y) # right-multiply translation
stack.rotate_z(deg)   # right-multiply Z-rotation
stack.pop()           # restore previous matrix
stack.apply()         # glLoadMatrixf(top)
```

Each recursive call:
1. **Applies** the current transform to OpenGL via `stack.apply()`
2. **Draws** the branch segment
3. **Pushes** → moves to branch tip → rotates left → recurses → **pops**
4. **Pushes** → moves to branch tip → rotates right → recurses → **pops**
