#pragma once
#include "../vissrt.hpp"

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#pragma comment(lib, "opengl32.lib")
#endif

namespace viss {
    namespace gl {
        #ifdef _WIN32
        inline const Int COLOR_BUFFER_BIT = GL_COLOR_BUFFER_BIT;
        inline const Int DEPTH_BUFFER_BIT = GL_DEPTH_BUFFER_BIT;
        inline const Int TRIANGLES = GL_TRIANGLES;
        inline const Int QUADS = GL_QUADS;
        inline const Int LINES = GL_LINES;

        inline void clearColor(Dec r, Dec g, Dec b, Dec a) {
            glClearColor(r, g, b, a);
        }
        inline void clear(Int mask) {
            glClear(mask);
        }
        inline void begin(Int mode) {
            glBegin(mode);
        }
        inline void end() {
            glEnd();
        }
        inline void color3(Dec r, Dec g, Dec b) {
            glColor3f(r, g, b);
        }
        inline void vertex2(Dec x, Dec y) {
            glVertex2f(x, y);
        }
        inline void vertex3(Dec x, Dec y, Dec z) {
            glVertex3f(x, y, z);
        }
        inline void rotate(Dec angle, Dec x, Dec y, Dec z) {
            glRotatef(angle, x, y, z);
        }
        #else
        inline const Int COLOR_BUFFER_BIT = 0;
        inline const Int DEPTH_BUFFER_BIT = 0;
        inline const Int TRIANGLES = 0;
        inline const Int QUADS = 0;
        inline const Int LINES = 0;

        inline void clearColor(Dec r, Dec g, Dec b, Dec a) {}
        inline void clear(Int mask) {}
        inline void begin(Int mode) {}
        inline void end() {}
        inline void color3(Dec r, Dec g, Dec b) {}
        inline void vertex2(Dec x, Dec y) {}
        inline void vertex3(Dec x, Dec y, Dec z) {}
        inline void rotate(Dec angle, Dec x, Dec y, Dec z) {}
        #endif
    }
}
