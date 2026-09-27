/*
 * Recursive Fractal Tree — Matrix Stacks (C++)
 * =============================================
 * OpenGL + GLUT implementation. A hand-rolled MatrixStack<>
 * (no glPushMatrix / glPopMatrix) drives all recursive branch
 * transformations.
 *
 * Build: see CMakeLists.txt and build.bat
 *
 * Controls:
 *   UP / DOWN    — increase / decrease recursion depth (max 12)
 *   LEFT / RIGHT — decrease / increase branch angle
 *   W / S        — increase / decrease length ratio
 *   A / D        — increase / decrease trunk length
 *   SPACE        — toggle wind animation
 *   R            — reset to defaults
 *   ESC / Q      — quit
 */

#define _USE_MATH_DEFINES
#include <GL/glut.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


// ============================================================
// Mat4 — column-major 4×4 float matrix
// ============================================================
using Mat4 = std::array<float, 16>;

/// Return the 4×4 identity matrix (column-major).
static Mat4 mat_identity()
{
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

/// Column-major matrix product: C = A * B
/// Element (row, col) lives at index  col*4 + row.
static Mat4 mat_mul(const Mat4& A, const Mat4& B)
{
    Mat4 C{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k   = 0; k   < 4; ++k)
                C[col*4+row] += A[k*4+row] * B[col*4+k];
    return C;
}


// ============================================================
// MatrixStack
// ============================================================
/**
 * A LIFO stack of column-major 4×4 matrices.
 *
 * Transforms are right-multiplied into the top matrix, so the
 * order matches the classic push/translate/rotate/draw/pop idiom.
 *
 * apply() calls glLoadMatrixf() to sync with OpenGL.
 */
class MatrixStack
{
public:
    MatrixStack() { reset(); }

    // ---- Stack operations ----------------------------------------

    /// Duplicate the top matrix and push it onto the stack.
    void push()
    {
        _s.push_back(_s.back());
    }

    /// Discard the top matrix and restore the previous one.
    void pop()
    {
        if (_s.size() <= 1)
            throw std::runtime_error("MatrixStack underflow");
        _s.pop_back();
    }

    /// Reset to a single identity matrix.
    void reset()
    {
        _s.clear();
        _s.push_back(mat_identity());
    }

    // ---- Transforms (right-multiply into top) --------------------

    void translate(float tx, float ty, float tz = 0.0f)
    {
        Mat4 T = mat_identity();
        T[12] = tx;  T[13] = ty;  T[14] = tz;
        _s.back() = mat_mul(_s.back(), T);
    }

    void rotate_z(float deg)
    {
        const float a = deg * static_cast<float>(M_PI) / 180.0f;
        const float c = std::cos(a), s = std::sin(a);
        Mat4 R = mat_identity();
        // Column 0: ( c,  s, 0, 0 )
        R[0] =  c;  R[1] = s;
        // Column 1: ( -s, c, 0, 0 )
        R[4] = -s;  R[5] = c;
        _s.back() = mat_mul(_s.back(), R);
    }

    void scale(float sx, float sy, float sz = 1.0f)
    {
        Mat4 S = mat_identity();
        S[0] = sx;  S[5] = sy;  S[10] = sz;
        _s.back() = mat_mul(_s.back(), S);
    }

    // ---- Apply to OpenGL -----------------------------------------

    /// Load the top matrix as the current OpenGL MODELVIEW matrix.
    void apply() const
    {
        glLoadMatrixf(_s.back().data());  // data is already column-major
    }

    // ---- Introspection -------------------------------------------

    /// Number of matrices currently on the stack.
    int  depth()       const { return static_cast<int>(_s.size()); }
    const Mat4& top()  const { return _s.back(); }

private:
    std::vector<Mat4> _s;
};


// ============================================================
// FractalTree
// ============================================================
struct Color3 { float r, g, b; };

/// Interpolate colour from trunk-brown (idx=0) to leaf-green (idx=max_depth).
static Color3 branch_color(int idx, int max_depth)
{
    const float t = max_depth > 0 ? static_cast<float>(idx) / max_depth : 0.0f;
    return { 0.55f + t*(0.18f - 0.55f),
             0.27f + t*(0.80f - 0.27f),
             0.07f + t*(0.25f - 0.07f) };
}

/// Taper line width from 6 px (trunk) down to 1 px (tips).
static float branch_width(int idx, int max_depth)
{
    const float t = max_depth > 0 ? static_cast<float>(idx) / max_depth : 0.0f;
    return std::max(1.0f, 6.0f * (1.0f - t));
}


class FractalTree
{
public:
    // ---- Parameters (live-editable via keyboard) -----------------
    int   depth        = 8;
    float angle        = 25.0f;   // branch split angle, degrees
    float length_ratio = 0.68f;   // child / parent length ratio
    float trunk_length = 0.40f;   // trunk length in world units
    bool  wind_enabled = true;
    float time         = 0.0f;    // elapsed animation time

    int  stack_depth() const { return _stack.depth(); }

    void update(float dt)
    {
        if (wind_enabled) time += dt;
    }

    void draw()
    {
        _stack.reset();
        _stack.translate(0.0f, -0.85f);  // anchor trunk near bottom of screen
        recurse(trunk_length, depth);
    }

private:
    MatrixStack _stack;

    /// Sinusoidal sway that grows larger for shallower-remaining branches.
    float wind_sway(int level) const
    {
        if (!wind_enabled) return 0.0f;
        const float freq  = 0.8f  + level * 0.15f;
        const float amp   = 1.0f  + level * 0.50f;
        const float phase = level * 0.40f;
        return amp * std::sin(time * freq + phase);
    }

    /**
     * Recursively draw a branch of `length` at the current transform.
     *
     *   apply transform  →  draw segment
     *   push → translate to tip → rotate +angle → recurse → pop
     *   push → translate to tip → rotate -angle → recurse → pop
     */
    void recurse(float length, int rem)
    {
        if (rem == 0 || length < 0.002f) return;

        const int    idx   = depth - rem;
        const Color3 col   = branch_color(idx, depth);
        const float  sway  = wind_sway(idx + 1);
        const float  next  = length * length_ratio;

        glColor3f(col.r, col.g, col.b);
        glLineWidth(branch_width(idx, depth));
        _stack.apply();

        glBegin(GL_LINES);
            glVertex2f(0.0f, 0.0f);
            glVertex2f(0.0f, length);
        glEnd();

        // Move origin to the tip of this segment
        _stack.push();
        _stack.translate(0.0f, length);

            // ---- Left child ----
            _stack.push();
            _stack.rotate_z(angle + sway);
            recurse(next, rem - 1);
            _stack.pop();

            // ---- Right child ----
            _stack.push();
            _stack.rotate_z(-(angle + sway * 0.7f));
            recurse(next, rem - 1);
            _stack.pop();

        _stack.pop();
    }
};


// ============================================================
// Application globals
// ============================================================
static FractalTree g_tree;
static int         g_prev_ms = 0;
static const int   WIN_W = 900, WIN_H = 900;


// ============================================================
// HUD rendering
// ============================================================
static void render_string(float x, float y, const std::string& s)
{
    glRasterPos2f(x, y);
    for (char c : s)
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
}

static void draw_hud()
{
    // --- switch to pixel-space projection -----------------------
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // --- semi-transparent panel ---------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.58f);
    glBegin(GL_QUADS);
        glVertex2f(8,   WIN_H -   8);
        glVertex2f(295, WIN_H -   8);
        glVertex2f(295, WIN_H - 185);
        glVertex2f(8,   WIN_H - 185);
    glEnd();
    glDisable(GL_BLEND);

    // --- text lines ---------------------------------------------
    glColor3f(0.78f, 0.91f, 0.78f);

    std::ostringstream ss;
    int row = 0;
    const float LINE_H = 21.0f;
    const float X0     = 14.0f;
    const float Y0     = WIN_H - 26.0f;

    auto line = [&](const std::string& text) {
        render_string(X0, Y0 - row++ * LINE_H, text);
    };

    ss.str(""); ss << "Depth        : " << g_tree.depth
                   << "  [UP / DOWN]";
    line(ss.str());

    ss.str(""); ss << std::fixed << std::setprecision(1)
                   << "Angle        : " << g_tree.angle
                   << "\xb0  [LEFT / RIGHT]";
    line(ss.str());

    ss.str(""); ss << std::fixed << std::setprecision(2)
                   << "Length ratio : " << g_tree.length_ratio
                   << "  [W / S]";
    line(ss.str());

    ss.str(""); ss << std::fixed << std::setprecision(2)
                   << "Trunk length : " << g_tree.trunk_length
                   << "  [A / D]";
    line(ss.str());

    ss.str(""); ss << "Wind         : "
                   << (g_tree.wind_enabled ? "ON " : "OFF")
                   << "  [SPACE]";
    line(ss.str());

    ss.str(""); ss << "Stack depth  : " << g_tree.stack_depth();
    line(ss.str());

    ++row;  // blank line
    line("R = Reset     ESC / Q = Quit");

    // --- restore matrices ---------------------------------------
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}


// ============================================================
// GLUT callbacks
// ============================================================
static void on_display()
{
    const int now = glutGet(GLUT_ELAPSED_TIME);
    const float dt = (now - g_prev_ms) / 1000.0f;
    g_prev_ms = now;

    g_tree.update(dt);

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);

    g_tree.draw();
    draw_hud();

    glutSwapBuffers();
}

static void on_timer(int /*v*/)
{
    glutPostRedisplay();
    glutTimerFunc(16, on_timer, 0);   // ~60 fps
}

static void on_reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

static void on_keyboard(unsigned char key, int /*x*/, int /*y*/)
{
    switch (key) {
        case 27: case 'q': case 'Q': std::exit(0);
        case 'w': g_tree.length_ratio = std::min(g_tree.length_ratio + 0.01f, 0.95f); break;
        case 's': g_tree.length_ratio = std::max(g_tree.length_ratio - 0.01f, 0.30f); break;
        case 'd': g_tree.trunk_length = std::min(g_tree.trunk_length + 0.02f, 0.80f); break;
        case 'a': g_tree.trunk_length = std::max(g_tree.trunk_length - 0.02f, 0.10f); break;
        case ' ': g_tree.wind_enabled = !g_tree.wind_enabled;                          break;
        case 'r': case 'R': g_tree = FractalTree();                                    break;
        default: break;
    }
    glutPostRedisplay();
}

static void on_special(int key, int /*x*/, int /*y*/)
{
    switch (key) {
        case GLUT_KEY_UP:    g_tree.depth = std::min(g_tree.depth + 1, 12);             break;
        case GLUT_KEY_DOWN:  g_tree.depth = std::max(g_tree.depth - 1,  1);             break;
        case GLUT_KEY_RIGHT: g_tree.angle = std::min(g_tree.angle + 1.0f, 89.0f);       break;
        case GLUT_KEY_LEFT:  g_tree.angle = std::max(g_tree.angle - 1.0f,  1.0f);       break;
        default: break;
    }
    glutPostRedisplay();
}


// ============================================================
// main
// ============================================================
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Recursive Fractal Tree — OpenGL + Matrix Stacks (C++)");

    // OpenGL state
    glClearColor(0.06f, 0.06f, 0.10f, 1.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initial projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Register callbacks
    glutDisplayFunc(on_display);
    glutReshapeFunc(on_reshape);
    glutKeyboardFunc(on_keyboard);
    glutSpecialFunc(on_special);
    glutTimerFunc(16, on_timer, 0);

    g_prev_ms = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    return 0;
}
