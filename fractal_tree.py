"""
Recursive Fractal Tree — OpenGL + Matrix Stacks
================================================
Real-time interactive fractal tree rendered with PyOpenGL + pygame.
A custom MatrixStack (push/pop) drives all recursive branch transformations
without relying on the deprecated glPushMatrix/glPopMatrix API.

Controls:
  UP / DOWN    — increase / decrease recursion depth (max 12)
  LEFT / RIGHT — decrease / increase branch angle
  W / S        — increase / decrease branch length ratio
  A / D        — increase / decrease trunk length
  SPACE        — toggle wind animation
  R            — reset to defaults
  ESC          — quit
"""

import sys
import math
import pygame
from pygame.locals import *

from OpenGL.GL import *
from OpenGL.GLU import *
import numpy as np


# ---------------------------------------------------------------------------
# Matrix Stack
# ---------------------------------------------------------------------------

class MatrixStack:
    """A simple LIFO stack of 4x4 column-major transformation matrices."""

    def __init__(self):
        self._stack = [np.eye(4, dtype=np.float32)]

    @property
    def top(self) -> np.ndarray:
        return self._stack[-1]

    def push(self):
        """Duplicate the top matrix and push onto the stack."""
        self._stack.append(self._stack[-1].copy())

    def pop(self):
        """Pop the top matrix off the stack."""
        if len(self._stack) <= 1:
            raise RuntimeError("MatrixStack underflow")
        self._stack.pop()

    # --- transformation helpers (right-multiply the top matrix) ---

    def translate(self, tx: float, ty: float, tz: float = 0.0):
        T = np.eye(4, dtype=np.float32)
        T[0, 3] = tx
        T[1, 3] = ty
        T[2, 3] = tz
        self._stack[-1] = self._stack[-1] @ T

    def rotate_z(self, angle_deg: float):
        a = math.radians(angle_deg)
        c, s = math.cos(a), math.sin(a)
        R = np.array([
            [c, -s, 0, 0],
            [s,  c, 0, 0],
            [0,  0, 1, 0],
            [0,  0, 0, 1],
        ], dtype=np.float32)
        self._stack[-1] = self._stack[-1] @ R

    def scale(self, sx: float, sy: float, sz: float = 1.0):
        S = np.diag([sx, sy, sz, 1.0]).astype(np.float32)
        self._stack[-1] = self._stack[-1] @ S

    def apply(self):
        """Load the top matrix as the current OpenGL MODELVIEW matrix."""
        glLoadMatrixf(self.top.T)   # OpenGL expects column-major


# ---------------------------------------------------------------------------
# Tree renderer
# ---------------------------------------------------------------------------

class FractalTree:
    """Draws a recursive fractal tree using a MatrixStack."""

    def __init__(self):
        self.depth          = 8       # recursion depth
        self.angle          = 25.0    # branch split angle (degrees)
        self.length_ratio   = 0.68    # child length / parent length
        self.trunk_length   = 0.40    # trunk length in world units
        self.wind_enabled   = True
        self._time          = 0.0

        self._stack = MatrixStack()

    # ------------------------------------------------------------------
    def update(self, dt: float):
        if self.wind_enabled:
            self._time += dt

    # ------------------------------------------------------------------
    def _wind_sway(self, depth: int) -> float:
        """Return a small extra rotation caused by 'wind' at this depth."""
        if not self.wind_enabled:
            return 0.0
        # Higher branches sway more
        freq   = 0.8 + depth * 0.15
        amp    = 1.0 + depth * 0.5
        phase  = depth * 0.4
        return amp * math.sin(self._time * freq + phase)

    # ------------------------------------------------------------------
    def _branch_color(self, depth: int, max_depth: int) -> tuple:
        """
        Gradient from warm brown (trunk) to bright green (leaves).
        """
        t = depth / max(max_depth, 1)
        # trunk: (0.55, 0.27, 0.07)  leaves: (0.18, 0.80, 0.25)
        r = 0.55 + t * (0.18 - 0.55)
        g = 0.27 + t * (0.80 - 0.27)
        b = 0.07 + t * (0.25 - 0.07)
        return (r, g, b)

    # ------------------------------------------------------------------
    def _branch_width(self, depth: int, max_depth: int) -> float:
        """Line width tapers from trunk to tip."""
        t = depth / max(max_depth, 1)
        return max(1.0, 6.0 * (1.0 - t))

    # ------------------------------------------------------------------
    def _draw_segment(self, length: float):
        """Draw a single branch segment from (0,0) to (0, length)."""
        glBegin(GL_LINES)
        glVertex2f(0.0, 0.0)
        glVertex2f(0.0, length)
        glEnd()

    # ------------------------------------------------------------------
    def _recurse(self, length: float, depth: int):
        """
        Recursively draw a branch of `length` at the current transform.
        Uses push/pop to save & restore the matrix stack.
        """
        if depth == 0 or length < 0.002:
            return

        color = self._branch_color(self.depth - depth, self.depth)
        width = self._branch_width(self.depth - depth, self.depth)

        glColor3f(*color)
        glLineWidth(width)

        # Apply current transform to OpenGL
        self._stack.apply()

        # Draw this branch
        self._draw_segment(length)

        # Move to tip of this branch
        self._stack.push()
        self._stack.translate(0.0, length)

        sway = self._wind_sway(self.depth - depth + 1)

        # ----- Left child -----
        self._stack.push()
        self._stack.rotate_z(self.angle + sway)
        self._recurse(length * self.length_ratio, depth - 1)
        self._stack.pop()

        # ----- Right child -----
        self._stack.push()
        self._stack.rotate_z(-(self.angle + sway * 0.7))
        self._recurse(length * self.length_ratio, depth - 1)
        self._stack.pop()

        self._stack.pop()

    # ------------------------------------------------------------------
    def draw(self):
        """Entry point: reset stack, position trunk, recurse."""
        self._stack = MatrixStack()
        self._stack.translate(0.0, -0.85)  # start just above the bottom edge
        self._recurse(self.trunk_length, self.depth)


# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

class App:
    WIDTH  = 900
    HEIGHT = 900
    TITLE  = "Recursive Fractal Tree — OpenGL + Matrix Stacks"

    def __init__(self):
        pygame.init()
        pygame.display.set_caption(self.TITLE)
        flags = DOUBLEBUF | OPENGL
        pygame.display.set_mode((self.WIDTH, self.HEIGHT), flags)

        self._setup_gl()
        self.tree  = FractalTree()
        self.clock = pygame.time.Clock()
        self._font = pygame.font.SysFont("consolas", 16)

    # ------------------------------------------------------------------
    def _setup_gl(self):
        glClearColor(0.06, 0.06, 0.10, 1.0)   # near-black deep-blue background
        glEnable(GL_LINE_SMOOTH)
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        # Orthographic: x in [-1,1], y in [-1,1]  (square window)
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0)

        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    # ------------------------------------------------------------------
    def _handle_events(self):
        for event in pygame.event.get():
            if event.type == QUIT:
                return False
            if event.type == KEYDOWN:
                t = self.tree
                if event.key == K_ESCAPE:
                    return False
                elif event.key == K_UP:
                    t.depth = min(t.depth + 1, 12)
                elif event.key == K_DOWN:
                    t.depth = max(t.depth - 1, 1)
                elif event.key == K_RIGHT:
                    t.angle = min(t.angle + 1.0, 89.0)
                elif event.key == K_LEFT:
                    t.angle = max(t.angle - 1.0, 1.0)
                elif event.key == K_w:
                    t.length_ratio = min(t.length_ratio + 0.01, 0.95)
                elif event.key == K_s:
                    t.length_ratio = max(t.length_ratio - 0.01, 0.30)
                elif event.key == K_d:
                    t.trunk_length = min(t.trunk_length + 0.02, 0.80)
                elif event.key == K_a:
                    t.trunk_length = max(t.trunk_length - 0.02, 0.10)
                elif event.key == K_SPACE:
                    t.wind_enabled = not t.wind_enabled
                elif event.key == K_r:
                    self.tree = FractalTree()
        return True

    # ------------------------------------------------------------------
    def _draw_hud(self):
        """Blit a small info panel over the OpenGL scene using pygame."""
        t = self.tree
        lines = [
            f"Depth        : {t.depth}  [UP/DOWN]",
            f"Angle        : {t.angle:.1f}°  [LEFT/RIGHT]",
            f"Length ratio : {t.length_ratio:.2f}  [W/S]",
            f"Trunk length : {t.trunk_length:.2f}  [A/D]",
            f"Wind         : {'ON' if t.wind_enabled else 'OFF'}  [SPACE]",
            f"FPS          : {self.clock.get_fps():.0f}",
            "",
            "R=Reset   ESC=Quit",
        ]

        # Render text to a surface, then blit it onto the raw GL window
        # We switch temporarily to 2D pixel coords.
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0, self.WIDTH, 0, self.HEIGHT, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()

        glDisable(GL_LINE_SMOOTH)
        glDisable(GL_BLEND)

        # Draw semi-transparent dark panel
        panel_w, panel_h = 270, len(lines) * 20 + 10
        glColor4f(0.0, 0.0, 0.0, 0.55)
        glEnable(GL_BLEND)
        glBegin(GL_QUADS)
        glVertex2f(8,  self.HEIGHT - 8)
        glVertex2f(8 + panel_w, self.HEIGHT - 8)
        glVertex2f(8 + panel_w, self.HEIGHT - 8 - panel_h)
        glVertex2f(8,  self.HEIGHT - 8 - panel_h)
        glEnd()

        # Render text with pygame then push as texture
        for i, line in enumerate(lines):
            surf = self._font.render(line, True, (200, 230, 200))
            tw, th = surf.get_size()
            tex_data = pygame.image.tostring(surf, "RGBA", True)
            tex_id = glGenTextures(1)
            glBindTexture(GL_TEXTURE_2D, tex_id)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, tex_data)
            glEnable(GL_TEXTURE_2D)

            y = self.HEIGHT - 20 - i * 20
            glColor4f(1, 1, 1, 1)
            glBegin(GL_QUADS)
            glTexCoord2f(0, 0); glVertex2f(12,      y)
            glTexCoord2f(1, 0); glVertex2f(12 + tw, y)
            glTexCoord2f(1, 1); glVertex2f(12 + tw, y + th)
            glTexCoord2f(0, 1); glVertex2f(12,      y + th)
            glEnd()

            glDisable(GL_TEXTURE_2D)
            glDeleteTextures(1, [tex_id])

        glDisable(GL_BLEND)
        glEnable(GL_LINE_SMOOTH)
        glEnable(GL_BLEND)

        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopMatrix()

    # ------------------------------------------------------------------
    def run(self):
        running = True
        while running:
            dt = self.clock.tick(60) / 1000.0

            running = self._handle_events()

            self.tree.update(dt)

            glClear(GL_COLOR_BUFFER_BIT)
            self.tree.draw()
            self._draw_hud()

            pygame.display.flip()

        pygame.quit()
        sys.exit()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    App().run()
