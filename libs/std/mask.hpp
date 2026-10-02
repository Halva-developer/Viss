#pragma once
#include "../vissrt.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

namespace viss {
namespace bytemask {

    // =========================================================================
    // 1. CREATION & ALLOCATION
    // =========================================================================

    // Allocate generic 1D byte-mask buffer (default 48 bytes = 16 RGB colors)
    inline viss::Bytes create(int size = 48, int fill = 0) {
        if (size < 1) size = 1;
        return viss::Bytes((size_t)size, (uint8_t)fill);
    }

    // Allocate RGB colormask palette (num_colors * 3 bytes, up to 256 colors = 768 bytes)
    inline viss::Bytes create_rgb(int num_colors = 16) {
        if (num_colors < 1) num_colors = 1;
        if (num_colors > 256) num_colors = 256;
        return viss::Bytes((size_t)(num_colors * 3), 0);
    }
    inline viss::Bytes create_palette(int num_colors = 16) {
        return create_rgb(num_colors);
    }

    // Allocate 2D spatial byte-mask grid (width * height)
    inline viss::Bytes create_2d(int w, int h, int fill = 0) {
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        return viss::Bytes((size_t)(w * h), (uint8_t)fill);
    }

    // =========================================================================
    // 2. COLOR MASK & PALETTE TOOLS (RGB & HSV)
    // =========================================================================

    inline void set_rgb(viss::Bytes& m, int id, int r, int g, int b) {
        if (id >= 0 && id < 256) {
            size_t offset = (size_t)id * 3;
            if (offset + 2 >= m.size()) {
                m.resize(offset + 3, 0);
            }
            m[offset]     = (uint8_t)std::clamp(r, 0, 255);
            m[offset + 1] = (uint8_t)std::clamp(g, 0, 255);
            m[offset + 2] = (uint8_t)std::clamp(b, 0, 255);
        }
    }
    inline void set(viss::Bytes& m, int id, int r, int g, int b) {
        set_rgb(m, id, r, g, b);
    }

    inline int get_r(const viss::Bytes& m, int id) {
        size_t offset = (size_t)id * 3;
        return (offset < m.size()) ? (int)m[offset] : 0;
    }

    inline int get_g(const viss::Bytes& m, int id) {
        size_t offset = (size_t)id * 3 + 1;
        return (offset < m.size()) ? (int)m[offset] : 0;
    }

    inline int get_b(const viss::Bytes& m, int id) {
        size_t offset = (size_t)id * 3 + 2;
        return (offset < m.size()) ? (int)m[offset] : 0;
    }

    // HSV Color Model: h (0..360), s (0..1 or 0..100), v (0..1 or 0..100)
    inline void set_hsv(viss::Bytes& m, int id, double h, double s, double v) {
        h = std::fmod(h, 360.0);
        if (h < 0) h += 360.0;
        if (s > 1.0) s /= 100.0;
        if (v > 1.0) v /= 100.0;
        s = std::clamp(s, 0.0, 1.0);
        v = std::clamp(v, 0.0, 1.0);

        double c = v * s;
        double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
        double m_val = v - c;

        double r1 = 0, g1 = 0, b1 = 0;
        if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
        else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
        else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
        else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
        else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
        else              { r1 = c; g1 = 0; b1 = x; }

        int r = (int)std::round((r1 + m_val) * 255.0);
        int g = (int)std::round((g1 + m_val) * 255.0);
        int b = (int)std::round((b1 + m_val) * 255.0);
        set_rgb(m, id, r, g, b);
    }

    // Linear RGB Color Gradient
    inline void gradient(viss::Bytes& m, int start_id, int end_id, int r1, int g1, int b1, int r2, int g2, int b2) {
        if (start_id > end_id) std::swap(start_id, end_id);
        int count = end_id - start_id;
        if (count <= 0) {
            set_rgb(m, start_id, r1, g1, b1);
            return;
        }
        for (int i = 0; i <= count; ++i) {
            double t = (double)i / (double)count;
            int r = (int)(r1 + (r2 - r1) * t);
            int g = (int)(g1 + (g2 - g1) * t);
            int b = (int)(b1 + (b2 - b1) * t);
            set_rgb(m, start_id + i, r, g, b);
        }
    }

    // Perceptual Rainbow / HSV Color Gradient
    inline void gradient_hsv(viss::Bytes& m, int start_id, int end_id, double h1, double s1, double v1, double h2, double s2, double v2) {
        if (start_id > end_id) std::swap(start_id, end_id);
        int count = end_id - start_id;
        if (count <= 0) {
            set_hsv(m, start_id, h1, s1, v1);
            return;
        }
        for (int i = 0; i <= count; ++i) {
            double t = (double)i / (double)count;
            double h = h1 + (h2 - h1) * t;
            double s = s1 + (s2 - s1) * t;
            double v = v1 + (v2 - v1) * t;
            set_hsv(m, start_id + i, h, s, v);
        }
    }

    inline int count(const viss::Bytes& m) {
        return (int)(m.size() / 3);
    }

    inline void fade(viss::Bytes& m, double factor) {
        if (factor < 0.0) factor = 0.0;
        if (factor > 1.0) factor = 1.0;
        for (size_t i = 0; i < m.size(); ++i) {
            m[i] = (uint8_t)(m[i] * factor);
        }
    }

    inline void invert(viss::Bytes& m) {
        for (size_t i = 0; i < m.size(); ++i) {
            m[i] = (uint8_t)(255 - m[i]);
        }
    }

    inline void shift(viss::Bytes& m, int dr, int dg, int db) {
        for (size_t i = 0; i + 2 < m.size(); i += 3) {
            m[i]     = (uint8_t)std::clamp((int)m[i] + dr, 0, 255);
            m[i + 1] = (uint8_t)std::clamp((int)m[i + 1] + dg, 0, 255);
            m[i + 2] = (uint8_t)std::clamp((int)m[i + 2] + db, 0, 255);
        }
    }

    inline void brightness(viss::Bytes& m, int delta) {
        shift(m, delta, delta, delta);
    }

    inline void contrast(viss::Bytes& m, double factor) {
        if (factor < 0.0) factor = 0.0;
        for (size_t i = 0; i < m.size(); ++i) {
            int val = (int)(128.0 + factor * ((double)m[i] - 128.0));
            m[i] = (uint8_t)std::clamp(val, 0, 255);
        }
    }

    inline void grayscale(viss::Bytes& m) {
        for (size_t i = 0; i + 2 < m.size(); i += 3) {
            uint8_t gray = (uint8_t)std::clamp((int)(0.299 * m[i] + 0.587 * m[i+1] + 0.114 * m[i+2]), 0, 255);
            m[i] = gray;
            m[i + 1] = gray;
            m[i + 2] = gray;
        }
    }

    // Color Quantization: Nearest RGB Color Matching
    inline int nearest(const viss::Bytes& m, int r, int g, int b) {
        int best_id = 0;
        int best_dist = 10000000;
        size_t total_colors = m.size() / 3;
        for (size_t i = 0; i < total_colors; ++i) {
            int cr = (int)m[i * 3];
            int cg = (int)m[i * 3 + 1];
            int cb = (int)m[i * 3 + 2];
            int dr = cr - r;
            int dg = cg - g;
            int db = cb - b;
            int dist = dr * dr * 2 + dg * dg * 4 + db * db * 3; // Weighted perceptual distance
            if (dist < best_dist) {
                best_dist = dist;
                best_id = (int)i;
            }
        }
        return best_id;
    }

    inline void lerp(viss::Bytes& dest, const viss::Bytes& a, const viss::Bytes& b, double t) {
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        size_t max_sz = std::max(a.size(), b.size());
        if (dest.size() < max_sz) dest.resize(max_sz, 0);
        for (size_t i = 0; i < max_sz; ++i) {
            uint8_t va = (i < a.size()) ? a[i] : 0;
            uint8_t vb = (i < b.size()) ? b[i] : 0;
            dest[i] = (uint8_t)(va + (vb - va) * t);
        }
    }

    // =========================================================================
    // 3. BUILT-IN COLOR MASK PRESETS
    // =========================================================================

    inline void preset(viss::Bytes& m, const std::string& name) {
        if (name == "tetris") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0,  18,  20,  28);  // 0: Board bg
            set_rgb(m, 1,  0,   240, 240); // 1: I (Cyan)
            set_rgb(m, 2,  33,  66,  230); // 2: J (Blue)
            set_rgb(m, 3,  255, 140, 0);   // 3: L (Orange)
            set_rgb(m, 4,  255, 220, 0);   // 4: O (Yellow)
            set_rgb(m, 5,  30,  220, 30);  // 5: S (Green)
            set_rgb(m, 6,  170, 0,   255); // 6: T (Purple)
            set_rgb(m, 7,  240, 30,  30);  // 7: Z (Red)
            set_rgb(m, 8,  95,  105, 125); // 8: Wall / Border (Steel)
            set_rgb(m, 9,  40,  50,  68);  // 9: Ghost shadow
            set_rgb(m, 10, 255, 255, 255); // 10: White text
            set_rgb(m, 11, 255, 205, 45);  // 11: Gold accent
            set_rgb(m, 12, 255, 255, 220); // 12: Flash clear
            set_rgb(m, 13, 10,  12,  18);  // 13: UI dark slate
            set_rgb(m, 14, 0,   210, 255); // 14: Sky cyan
            set_rgb(m, 15, 225, 25,  45);  // 15: Game over red
        } else if (name == "gameboy") {
            if (m.size() < 12) m.resize(12, 0);
            set_rgb(m, 0, 155, 188, 15);
            set_rgb(m, 1, 139, 172, 15);
            set_rgb(m, 2, 48,  98,  48);
            set_rgb(m, 3, 15,  56,  15);
        } else if (name == "pico8") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 0,0,0);        set_rgb(m, 1, 29,43,83);    set_rgb(m, 2, 126,37,83);   set_rgb(m, 3, 0,135,81);
            set_rgb(m, 4, 171,82,54);    set_rgb(m, 5, 95,87,79);    set_rgb(m, 6, 194,195,199); set_rgb(m, 7, 255,241,232);
            set_rgb(m, 8, 255,0,77);     set_rgb(m, 9, 255,163,0);   set_rgb(m, 10, 255,236,39); set_rgb(m, 11, 0,228,54);
            set_rgb(m, 12, 41,173,255);  set_rgb(m, 13, 131,118,156);set_rgb(m, 14, 255,119,168);set_rgb(m, 15, 255,204,170);
        } else if (name == "nes") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0,  92,  148, 252); set_rgb(m, 1,  180, 70,  20);  set_rgb(m, 2,  236, 30,  30);  set_rgb(m, 3,  0,   68,  220);
            set_rgb(m, 4,  252, 188, 176); set_rgb(m, 5,  0,   168, 0);   set_rgb(m, 6,  252, 216, 0);   set_rgb(m, 7,  255, 255, 255);
            set_rgb(m, 8,  0,   0,   0);   set_rgb(m, 9,  120, 40,  10);  set_rgb(m, 10, 0,   100, 0);   set_rgb(m, 11, 240, 140, 0);
            set_rgb(m, 12, 90,  90,  90);  set_rgb(m, 13, 180, 180, 180); set_rgb(m, 14, 140, 20,  20);  set_rgb(m, 15, 15,  25,  70);
        } else if (name == "fire") {
            if (m.size() < 48) m.resize(48, 0);
            gradient(m, 0, 4, 0, 0, 0, 180, 0, 0);
            gradient(m, 4, 8, 180, 0, 0, 255, 120, 0);
            gradient(m, 8, 12, 255, 120, 0, 255, 240, 0);
            gradient(m, 12, 15, 255, 240, 0, 255, 255, 255);
        } else if (name == "cyberpunk") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 10, 10, 20);
            gradient(m, 1, 5, 0, 240, 255, 255, 0, 128);
            gradient(m, 6, 10, 255, 0, 128, 255, 230, 0);
            gradient(m, 11, 15, 128, 0, 255, 255, 255, 255);
        } else if (name == "monochrome") {
            if (m.size() < 6) m.resize(6, 0);
            set_rgb(m, 0, 0, 0, 0);
            set_rgb(m, 1, 255, 255, 255);
        } else if (name == "neon") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 10, 5, 20);
            set_rgb(m, 1, 255, 0, 128);   // Neon Pink
            set_rgb(m, 2, 0, 255, 240);   // Neon Cyan
            set_rgb(m, 3, 50, 255, 50);   // Neon Green
            set_rgb(m, 4, 255, 240, 0);   // Neon Yellow
            set_rgb(m, 5, 170, 0, 255);   // Neon Purple
            set_rgb(m, 6, 255, 100, 0);   // Neon Orange
            set_rgb(m, 7, 0, 150, 255);   // Deep Neon Sky
            set_rgb(m, 8, 80, 80, 100);   // Gray
            set_rgb(m, 9, 30, 20, 50);    // Shadow
            set_rgb(m, 10, 255, 255, 255);// White
        } else if (name == "c64") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 0, 0, 0);        set_rgb(m, 1, 255, 255, 255);  set_rgb(m, 2, 136, 0, 0);     set_rgb(m, 3, 170, 255, 238);
            set_rgb(m, 4, 204, 68, 204);   set_rgb(m, 5, 0, 204, 85);     set_rgb(m, 6, 0, 0, 170);     set_rgb(m, 7, 238, 238, 119);
            set_rgb(m, 8, 221, 136, 85);   set_rgb(m, 9, 102, 68, 0);     set_rgb(m, 10, 255, 119, 119);set_rgb(m, 11, 51, 51, 51);
            set_rgb(m, 12, 119, 119, 119); set_rgb(m, 13, 170, 255, 102); set_rgb(m, 14, 0, 136, 255);  set_rgb(m, 15, 187, 187, 187);
        } else if (name == "matrix") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 0, 5, 0);
            gradient(m, 1, 14, 0, 40, 0, 20, 255, 60);
            set_rgb(m, 15, 220, 255, 220); // Matrix core white-green
        } else if (name == "lava") {
            if (m.size() < 48) m.resize(48, 0);
            gradient(m, 0, 5, 15, 5, 5, 180, 20, 0);
            gradient(m, 6, 11, 180, 20, 0, 255, 180, 0);
            gradient(m, 12, 15, 255, 180, 0, 255, 255, 180);
        } else if (name == "pastel") {
            if (m.size() < 48) m.resize(48, 0);
            set_rgb(m, 0, 30, 30, 40);
            set_rgb(m, 1, 255, 179, 186); // Pastel Pink
            set_rgb(m, 2, 255, 223, 186); // Pastel Orange
            set_rgb(m, 3, 255, 255, 186); // Pastel Yellow
            set_rgb(m, 4, 186, 255, 201); // Pastel Green
            set_rgb(m, 5, 186, 225, 255); // Pastel Blue
            set_rgb(m, 6, 218, 186, 255); // Pastel Lavender
            set_rgb(m, 7, 255, 200, 221); // Pastel Rose
            gradient(m, 8, 15, 200, 210, 220, 255, 255, 255);
        }
    }

    inline viss::Bytes preset(const std::string& name) {
        viss::Bytes m = create_rgb(16);
        preset(m, name);
        return m;
    }

    // =========================================================================
    // 4. SPATIAL 2D MASK OPERATIONS (STENCILS, SHAPES, BLENDING)
    // =========================================================================

    inline void set_2d(viss::Bytes& m, int w, int h, int x, int y, int val) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            size_t idx = (size_t)(y * w + x);
            if (idx < m.size()) m[idx] = (uint8_t)val;
        }
    }

    inline int get_2d(const viss::Bytes& m, int w, int h, int x, int y) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            size_t idx = (size_t)(y * w + x);
            if (idx < m.size()) return (int)m[idx];
        }
        return 0;
    }

    inline void rect_2d(viss::Bytes& m, int w, int h, int rx, int ry, int rw, int rh, int val) {
        for (int y = ry; y < ry + rh; ++y) {
            for (int x = rx; x < rx + rw; ++x) {
                set_2d(m, w, h, x, y, val);
            }
        }
    }

    inline void circle_2d(viss::Bytes& m, int w, int h, int cx, int cy, int radius, int val) {
        int r2 = radius * radius;
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                int dx = x - cx;
                int dy = y - cy;
                if (dx * dx + dy * dy <= r2) {
                    set_2d(m, w, h, x, y, val);
                }
            }
        }
    }

    inline void line_2d(viss::Bytes& m, int w, int h, int x0, int y0, int x1, int y1, int val) {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            set_2d(m, w, h, x0, y0, val);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Threshold: creates binary mask where elements >= threshold_val become high_val, else low_val
    inline viss::Bytes threshold(const viss::Bytes& src, int threshold_val, int high_val = 1, int low_val = 0) {
        viss::Bytes out(src.size(), 0);
        for (size_t i = 0; i < src.size(); ++i) {
            out[i] = (src[i] >= (uint8_t)threshold_val) ? (uint8_t)high_val : (uint8_t)low_val;
        }
        return out;
    }

    // Stencil Masking: copies src into dest only where stencil == pass_id
    inline void apply_stencil(viss::Bytes& dest, const viss::Bytes& src, const viss::Bytes& stencil, int pass_id = 1) {
        size_t limit = std::min({dest.size(), src.size(), stencil.size()});
        for (size_t i = 0; i < limit; ++i) {
            if (stencil[i] == (uint8_t)pass_id) {
                dest[i] = src[i];
            }
        }
    }

    // Stencil Invert: copies src into dest where stencil != pass_id
    inline void apply_stencil_inverted(viss::Bytes& dest, const viss::Bytes& src, const viss::Bytes& stencil, int pass_id = 1) {
        size_t limit = std::min({dest.size(), src.size(), stencil.size()});
        for (size_t i = 0; i < limit; ++i) {
            if (stencil[i] != (uint8_t)pass_id) {
                dest[i] = src[i];
            }
        }
    }

    // Alpha Blend: blends two buffers A and B using an 8-bit alpha mask (0..255)
    inline void blend(viss::Bytes& dest, const viss::Bytes& src_a, const viss::Bytes& src_b, const viss::Bytes& alpha_mask) {
        size_t limit = std::min({dest.size(), src_a.size(), src_b.size(), alpha_mask.size()});
        for (size_t i = 0; i < limit; ++i) {
            int a = (int)alpha_mask[i];
            int v1 = (int)src_a[i];
            int v2 = (int)src_b[i];
            dest[i] = (uint8_t)((v1 * (255 - a) + v2 * a) / 255);
        }
    }

    // =========================================================================
    // 5. COLLISION & PHYSICS MASKING
    // =========================================================================

    inline bool collides_2d(const viss::Bytes& a, int ax, int ay, int aw, int ah,
                            const viss::Bytes& b, int bx, int by, int bw, int bh,
                            int transparent_id = 0) {
        int ix1 = std::max(ax, bx);
        int iy1 = std::max(ay, by);
        int ix2 = std::min(ax + aw, bx + bw);
        int iy2 = std::min(ay + ah, by + bh);

        if (ix1 >= ix2 || iy1 >= iy2) return false;

        for (int y = iy1; y < iy2; ++y) {
            for (int x = ix1; x < ix2; ++x) {
                int a_x = x - ax;
                int a_y = y - ay;
                int b_x = x - bx;
                int b_y = y - by;

                size_t a_idx = (size_t)(a_y * aw + a_x);
                size_t b_idx = (size_t)(b_y * bw + b_x);

                if (a_idx < a.size() && b_idx < b.size()) {
                    uint8_t val_a = a[a_idx];
                    uint8_t val_b = b[b_idx];
                    if (val_a != (uint8_t)transparent_id && val_b != (uint8_t)transparent_id) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Raycast through 2D mask: returns hit distance or -1 if no hit
    inline double raycast_2d(const viss::Bytes& m, int w, int h, double x0, double y0, double dir_x, double dir_y, double max_dist, int solid_id = 1) {
        double len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
        if (len <= 0.0001) return -1.0;
        double ndx = dir_x / len;
        double ndy = dir_y / len;

        double step = 0.5;
        for (double d = 0; d <= max_dist; d += step) {
            int cx = (int)std::floor(x0 + ndx * d);
            int cy = (int)std::floor(y0 + ndy * d);
            if (cx < 0 || cx >= w || cy < 0 || cy >= h) return -1.0;
            if (get_2d(m, w, h, cx, cy) == solid_id) {
                return d;
            }
        }
        return -1.0;
    }

    // =========================================================================
    // 6. BITWISE & BUFFER OPERATIONS
    // =========================================================================

    // Pure bitwise operations (returns new Bytes buffer)
    inline viss::Bytes and_op(const viss::Bytes& a, const viss::Bytes& b) {
        size_t limit = std::min(a.size(), b.size());
        viss::Bytes res(limit, 0);
        for (size_t i = 0; i < limit; ++i) res[i] = a[i] & b[i];
        return res;
    }

    inline viss::Bytes or_op(const viss::Bytes& a, const viss::Bytes& b) {
        size_t limit = std::min(a.size(), b.size());
        viss::Bytes res(limit, 0);
        for (size_t i = 0; i < limit; ++i) res[i] = a[i] | b[i];
        return res;
    }

    inline viss::Bytes xor_op(const viss::Bytes& a, const viss::Bytes& b) {
        size_t limit = std::min(a.size(), b.size());
        viss::Bytes res(limit, 0);
        for (size_t i = 0; i < limit; ++i) res[i] = a[i] ^ b[i];
        return res;
    }

    inline viss::Bytes not_op(const viss::Bytes& a) {
        viss::Bytes res(a.size(), 0);
        for (size_t i = 0; i < a.size(); ++i) res[i] = ~a[i];
        return res;
    }

    // In-place bitwise operations
    inline void and_into(viss::Bytes& dest, const viss::Bytes& mask) {
        size_t limit = std::min(dest.size(), mask.size());
        for (size_t i = 0; i < limit; ++i) dest[i] &= mask[i];
    }

    inline void or_into(viss::Bytes& dest, const viss::Bytes& mask) {
        size_t limit = std::min(dest.size(), mask.size());
        for (size_t i = 0; i < limit; ++i) dest[i] |= mask[i];
    }

    inline void xor_into(viss::Bytes& dest, const viss::Bytes& mask) {
        size_t limit = std::min(dest.size(), mask.size());
        for (size_t i = 0; i < limit; ++i) dest[i] ^= mask[i];
    }

    inline void not_into(viss::Bytes& dest) {
        for (size_t i = 0; i < dest.size(); ++i) dest[i] = ~dest[i];
    }

    inline int count_matching(const viss::Bytes& m, int val) {
        int cnt = 0;
        for (size_t i = 0; i < m.size(); ++i) {
            if (m[i] == (uint8_t)val) cnt++;
        }
        return cnt;
    }

    inline int find_first(const viss::Bytes& m, int val) {
        for (size_t i = 0; i < m.size(); ++i) {
            if (m[i] == (uint8_t)val) return (int)i;
        }
        return -1;
    }

    inline void replace(viss::Bytes& m, int old_val, int new_val) {
        for (size_t i = 0; i < m.size(); ++i) {
            if (m[i] == (uint8_t)old_val) m[i] = (uint8_t)new_val;
        }
    }

    // =========================================================================
    // 7. SERIALIZATION & HEX FORMATTING
    // =========================================================================

    inline std::string to_hex(const viss::Bytes& m) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < m.size(); ++i) {
            ss << std::setw(2) << (int)m[i];
            if (i + 1 < m.size()) ss << " ";
        }
        return ss.str();
    }

    inline viss::Bytes from_hex(const std::string& hex_str) {
        std::vector<uint8_t> bytes;
        std::string cur = "";
        for (char ch : hex_str) {
            if (std::isxdigit(ch)) {
                cur += ch;
                if (cur.size() == 2) {
                    bytes.push_back((uint8_t)std::strtol(cur.c_str(), nullptr, 16));
                    cur.clear();
                }
            }
        }
        viss::Bytes res(bytes.size(), 0);
        for (size_t i = 0; i < bytes.size(); ++i) res[i] = bytes[i];
        return res;
    }

    inline bool save_hex(const std::string& path, const viss::Bytes& m) {
        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << to_hex(m) << "\n";
        return true;
    }

    inline viss::Bytes load_hex(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return viss::Bytes(0, 0);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return from_hex(buffer.str());
    }

    inline bool save_bin(const std::string& path, const viss::Bytes& m) {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;
        out.write((const char*)m.raw(), m.size());
        return true;
    }

    inline viss::Bytes load_bin(const std::string& path) {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in.is_open()) return viss::Bytes(0, 0);
        std::streamsize sz = in.tellg();
        in.seekg(0, std::ios::beg);
        viss::Bytes res((size_t)sz, 0);
        in.read((char*)res.raw(), sz);
        return res;
    }

    // =========================================================================
    // 8. RAW BINARY (BIT STRINGS) & RAW BUFFER TOOLS
    // =========================================================================

    inline std::string to_bin(const viss::Bytes& m) {
        return m.to_bin();
    }

    inline std::string to_bin_raw(const viss::Bytes& m) {
        return m.to_bin_raw();
    }

    inline viss::Bytes from_bin(const std::string& bin_str) {
        return viss::Bytes::from_bin(bin_str);
    }

    inline viss::Bytes from_raw(const std::string& raw_str) {
        return viss::Bytes::from_raw(raw_str);
    }

    inline std::string get_bin(const viss::Bytes& m, int idx) {
        return m.get_bin((size_t)idx);
    }

    inline void set_bin(viss::Bytes& m, int idx, const std::string& bin_str) {
        m.set_bin((size_t)idx, bin_str);
    }

    inline std::string get_hex(const viss::Bytes& m, int idx) {
        return m.get_hex((size_t)idx);
    }

    inline void set_hex(viss::Bytes& m, int idx, const std::string& hex_str) {
        m.set_hex((size_t)idx, hex_str);
    }

    inline void dump_raw(const viss::Bytes& m) {
        m.dump_raw();
    }

    inline bool save_raw(const std::string& path, const viss::Bytes& m) {
        return save_bin(path, m);
    }

    inline viss::Bytes load_raw(const std::string& path) {
        return load_bin(path);
    }

    inline void set_view(viss::Bytes& m, const std::string& v) {
        m.set_view(v);
    }

    inline void view(viss::Bytes& m, const std::string& v) {
        m.set_view(v);
    }

    inline std::string get_view(const viss::Bytes& m) {
        return m.get_view();
    }

} // namespace bytemask
namespace colormask = bytemask;
namespace mask = bytemask;
} // namespace viss
