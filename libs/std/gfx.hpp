#pragma once
#include "../vissrt.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>

namespace viss {
    namespace gfx {
        inline HWND hwnd = nullptr;
        inline HDC hdc = nullptr;
        inline HDC memDC = nullptr;
        inline HBITMAP hbm = nullptr;
        inline int width = 0;
        inline int height = 0;
        inline bool running = false;
        inline bool keys[256] = {false};

        inline LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            switch(msg) {
                case WM_DESTROY:
                    PostQuitMessage(0);
                    running = false;
                    return 0;
                case WM_KEYDOWN:
                    if (wParam < 256) keys[wParam] = true;
                    return 0;
                case WM_KEYUP:
                    if (wParam < 256) keys[wParam] = false;
                    return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        inline void init(Int w, Int h, const Str& title) {
            width = w;
            height = h;
            HINSTANCE hInst = GetModuleHandle(nullptr);
            
            WNDCLASS wc = {};
            wc.lpfnWndProc = WndProc;
            wc.hInstance = hInst;
            wc.lpszClassName = "VissGfxWindowClass";
            wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

            RegisterClass(&wc);

            RECT r = {0, 0, (LONG)width, (LONG)height};
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

            hwnd = CreateWindowEx(
                0, "VissGfxWindowClass", title.c_str(),
                WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                nullptr, nullptr, hInst, nullptr
            );

            if (hwnd) {
                ShowWindow(hwnd, SW_SHOW);
                hdc = GetDC(hwnd);

                HDC winDC = GetDC(hwnd);
                memDC = CreateCompatibleDC(winDC);
                hbm = CreateCompatibleBitmap(winDC, width, height);
                SelectObject(memDC, hbm);
                ReleaseDC(hwnd, winDC);

                running = true;
            }
        }

        inline Bool isOpen() {
            if (!running) return false;
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            return running;
        }

        inline void clear(Int color) {
            if (!memDC) return;
            RECT r = {0, 0, width, height};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(memDC, &r, brush);
            DeleteObject(brush);
        }

        inline void drawRect(Int x, Int y, Int w, Int h, Int color) {
            if (!memDC) return;
            RECT r = {(LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h)};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(memDC, &r, brush);
            DeleteObject(brush);
        }

        inline void drawText(Int x, Int y, const Str& text, Int color) {
            if (!memDC) return;
            SetTextColor(memDC, color);
            SetBkMode(memDC, TRANSPARENT);
            TextOut(memDC, x, y, text.c_str(), (int)text.length());
        }

        inline Bool getKey(Int keyCode) {
            if (keyCode >= 0 && keyCode < 256) {
                return keys[keyCode];
            }
            return false;
        }

        inline void update() {
            if (!hwnd || !hdc || !memDC) return;
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
            Sleep(16);
        }

        inline void close() {
            if (memDC) DeleteDC(memDC);
            if (hbm) DeleteObject(hbm);
            if (hwnd && hdc) ReleaseDC(hwnd, hdc);
            running = false;
        }
    }
}
#else
namespace viss {
    namespace gfx {
        inline void init(Int w, Int h, const Str& title) {
            std::cout << "[Viss Gfx] Graphics window initialization is only supported on Windows GDI.\n";
        }
        inline Bool isOpen() { return false; }
        inline void clear(Int color) {}
        inline void drawRect(Int x, Int y, Int w, Int h, Int color) {}
        inline void drawText(Int x, Int y, const Str& text, Int color) {}
        inline Bool getKey(Int keyCode) { return false; }
        inline void update() {}
        inline void close() {}
    }
}
#endif
