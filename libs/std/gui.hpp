#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <thread>
#include <functional>
#include <memory>

#ifdef _WIN32
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objidl.h>
#ifndef PROPID
typedef unsigned long PROPID;
#endif
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace viss {
namespace gui {

inline std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
#ifdef _WIN32
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    std::wstring cleaned;
    cleaned.reserve(wstr.size());
    for (wchar_t c : wstr) {
        if (c != (wchar_t)0xFE0E && c != (wchar_t)0xFE0F) {
            cleaned.push_back(c);
        }
    }
    return cleaned;
#else
    return std::wstring(str.begin(), str.end());
#endif
}

inline std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#ifdef _WIN32
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    return str;
#else
    return std::string(wstr.begin(), wstr.end());
#endif
}

struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    static Color rgb(uint8_t r, uint8_t g, uint8_t b) { return Color(r, g, b, 255); }
    static Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return Color(r, g, b, a); }

    static Color hex(uint32_t hex_val) {
        return Color(
            (hex_val >> 16) & 0xFF,
            (hex_val >> 8) & 0xFF,
            hex_val & 0xFF,
            255
        );
    }

    #ifdef _WIN32
    COLORREF to_colorref() const { return RGB(r, g, b); }
    Gdiplus::Color to_gdip() const { return Gdiplus::Color(a, r, g, b); }
    #endif
};

// Common Modern Palette
namespace Colors {
    inline const Color DarkBg = Color::hex(0x0f172a);      // Deep Slate
    inline const Color PanelBg = Color::hex(0x1e293b);     // Slate Card
    inline const Color PanelBorder = Color::hex(0x334155); // Slate Border
    inline const Color AccentCyan = Color::hex(0x06b6d4);  // Cyber Cyan
    inline const Color AccentBlue = Color::hex(0x3b82f6);  // Studio Blue
    inline const Color AccentGreen = Color::hex(0x10b981); // Emerald Green
    inline const Color AccentRed = Color::hex(0xef4444);   // Crimson Red
    inline const Color AccentGold = Color::hex(0xf59e0b);  // Sunset Gold
    inline const Color TextLight = Color::hex(0xf8fafc);   // Crisp White
    inline const Color TextDim = Color::hex(0x94a3b8);     // Muted Gray
    inline const Color InputBg = Color::hex(0x0b1120);     // Dark Input
}

#ifdef _WIN32
class WindowApp {
private:
    HWND hwnd = NULL;
    HDC hdc_mem = NULL;
    HBITMAP hbm_mem = NULL;
    HBITMAP hbm_old = NULL;
    ULONG_PTR gdiplus_token = 0;
    std::unique_ptr<Gdiplus::Graphics> graphics;

    int win_width = 800;
    int win_height = 600;
    int design_width = 800;
    int design_height = 600;
    bool autoscale = true;
    std::string win_title = "Viss Window";
    bool running = false;

    inline float get_scale_x() const {
        return (autoscale && design_width > 0) ? ((float)win_width / (float)design_width) : 1.0f;
    }
    inline float get_scale_y() const {
        return (autoscale && design_height > 0) ? ((float)win_height / (float)design_height) : 1.0f;
    }

    // Mouse and Keyboard state
    int m_x = 0;
    int m_y = 0;
    bool m_down = false;
    bool m_clicked = false;
    bool m_right_down = false;
    bool m_right_clicked = false;
    bool keys[256] = {false};
    bool keys_pressed[256] = {false};
    char last_char_input = 0;
    std::string last_input_str = "";

    // Active widget interaction state
    std::string active_input_id = "";
    int cursor_counter = 0;
    std::string dropped_file_path = "";
    std::map<std::string, int> input_carets;
    std::map<std::string, int> input_scrolls;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowApp* app = (WindowApp*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (msg == WM_NCCREATE) {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            app = (WindowApp*)cs->lpCreateParams;
            app->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        if (app) return app->handleMessage(msg, wParam, lParam);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_DROPFILES: {
                HDROP hDrop = (HDROP)wParam;
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
                    int needed = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, NULL, 0, NULL, NULL);
                    std::string u8(needed, 0);
                    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, &u8[0], needed, NULL, NULL);
                    if (!u8.empty() && u8.back() == '\0') u8.pop_back();
                    dropped_file_path = u8;
                }
                DragFinish(hDrop);
                return 0;
            }

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (hdc_mem) {
                    BitBlt(hdc, 0, 0, win_width, win_height, hdc_mem, 0, 0, SRCCOPY);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1;

            case WM_SYSCOMMAND:
                switch (wParam & 0xFFF0) {
                    case SC_MINIMIZE:
                        ShowWindow(hwnd, SW_MINIMIZE);
                        return 0;
                    case SC_MAXIMIZE:
                        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                        return 0;
                    case SC_RESTORE:
                        ShowWindow(hwnd, SW_RESTORE);
                        return 0;
                    case SC_CLOSE:
                        running = false;
                        DestroyWindow(hwnd);
                        return 0;
                }
                break;

            case WM_MOUSEMOVE:
                m_x = (int)(GET_X_LPARAM(lParam) / get_scale_x());
                m_y = (int)(GET_Y_LPARAM(lParam) / get_scale_y());
                return 0;

            case WM_LBUTTONDOWN:
                m_x = (int)(GET_X_LPARAM(lParam) / get_scale_x());
                m_y = (int)(GET_Y_LPARAM(lParam) / get_scale_y());
                m_down = true;
                m_clicked = true;
                return 0;

            case WM_LBUTTONUP:
                m_x = (int)(GET_X_LPARAM(lParam) / get_scale_x());
                m_y = (int)(GET_Y_LPARAM(lParam) / get_scale_y());
                m_down = false;
                return 0;

            case WM_RBUTTONDOWN:
                m_x = (int)(GET_X_LPARAM(lParam) / get_scale_x());
                m_y = (int)(GET_Y_LPARAM(lParam) / get_scale_y());
                m_right_down = true;
                m_right_clicked = true;
                return 0;

            case WM_RBUTTONUP:
                m_x = (int)(GET_X_LPARAM(lParam) / get_scale_x());
                m_y = (int)(GET_Y_LPARAM(lParam) / get_scale_y());
                m_right_down = false;
                return 0;

            case WM_KEYDOWN:
                if (wParam < 256) {
                    if (!keys[wParam]) keys_pressed[wParam] = true;
                    keys[wParam] = true;
                }
                return 0;

            case WM_KEYUP:
                if (wParam < 256) keys[wParam] = false;
                return 0;

            case WM_CHAR:
                if (wParam == '\b' || wParam == '\r' || wParam == '\n' || wParam == 22 || wParam == 1 || wParam == 3) {
                    last_char_input = (wParam == '\r') ? '\n' : (char)wParam;
                    last_input_str = (wParam == '\r' || wParam == '\n') ? "\n" : "";
                } else if (wParam >= 32) {
                    wchar_t wch = (wchar_t)wParam;
                    char buf[8] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, &wch, 1, buf, sizeof(buf), NULL, NULL);
                    last_input_str = buf;
                    last_char_input = buf[0];
                }
                return 0;

            case WM_SIZE: {
                int new_w = LOWORD(lParam);
                int new_h = HIWORD(lParam);
                if (new_w > 0 && new_h > 0 && (new_w != win_width || new_h != win_height)) {
                    resizeBuffer(new_w, new_h);
                }
                return 0;
            }

            case WM_CLOSE:
                running = false;
                DestroyWindow(hwnd);
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void applyScaleTransform() {
        if (!graphics) return;
        graphics->ResetTransform();
        if (autoscale && design_width > 0 && design_height > 0) {
            graphics->ScaleTransform((Gdiplus::REAL)win_width / (Gdiplus::REAL)design_width,
                                    (Gdiplus::REAL)win_height / (Gdiplus::REAL)design_height);
        }
    }

    void resizeBuffer(int w, int h) {
        win_width = w;
        win_height = h;

        graphics.reset();
        if (hdc_mem) {
            if (hbm_old) SelectObject(hdc_mem, hbm_old);
            if (hbm_mem) DeleteObject(hbm_mem);
            DeleteDC(hdc_mem);
        }

        HDC screen_dc = GetDC(hwnd);
        hdc_mem = CreateCompatibleDC(screen_dc);
        hbm_mem = CreateCompatibleBitmap(screen_dc, win_width, win_height);
        hbm_old = (HBITMAP)SelectObject(hdc_mem, hbm_mem);
        ReleaseDC(hwnd, screen_dc);

        graphics = std::make_unique<Gdiplus::Graphics>(hdc_mem);
        graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        applyScaleTransform();
    }

public:
    WindowApp() {
        OleInitialize(NULL);
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplusStartupInput, NULL);
    }

    ~WindowApp() {
        close();
        if (gdiplus_token) {
            Gdiplus::GdiplusShutdown(gdiplus_token);
            gdiplus_token = 0;
        }
        OleUninitialize();
    }

    bool create(const std::string& title, int width = 800, int height = 600, bool center = true) {
#ifdef _WIN32
        HWND con_wnd = GetConsoleWindow();
        if (con_wnd != NULL) {
            ShowWindow(con_wnd, SW_HIDE);
        }
#endif
        win_title = title;
        win_width = width;
        win_height = height;
        design_width = width;
        design_height = height;
        autoscale = true;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        std::wstring w_title = utf8_to_wstring(title);
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
        if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0);
        wc.lpszClassName = L"VissModernWindowClass";

        RegisterClassExW(&wc);

        RECT rect = {0, 0, width, height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        int real_w = rect.right - rect.left;
        int real_h = rect.bottom - rect.top;

        int pos_x = center ? (GetSystemMetrics(SM_CXSCREEN) - real_w) / 2 : CW_USEDEFAULT;
        int pos_y = center ? (GetSystemMetrics(SM_CYSCREEN) - real_h) / 2 : CW_USEDEFAULT;

        hwnd = CreateWindowExW(
            0,
            L"VissModernWindowClass",
            w_title.c_str(),
            WS_OVERLAPPEDWINDOW,
            pos_x, pos_y, real_w, real_h,
            NULL, NULL, hInstance, this
        );

        if (!hwnd) return false;

        SetWindowTextW(hwnd, w_title.c_str());
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        DragAcceptFiles(hwnd, TRUE);

        resizeBuffer(width, height);
        running = true;
        return true;
    }

    HWND getHwnd() const { return hwnd; }
    bool is_open() const { return running; }
    std::string get_dropped_file() {
        std::string ret = dropped_file_path;
        dropped_file_path = "";
        return ret;
    }

    void close() {
        running = false;
        graphics.reset();
        if (hdc_mem) {
            if (hbm_old) SelectObject(hdc_mem, hbm_old);
            if (hbm_mem) DeleteObject(hbm_mem);
            DeleteDC(hdc_mem);
            hdc_mem = NULL;
        }
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = NULL;
        }
    }

    bool poll_events() {
        m_clicked = false;
        m_right_clicked = false;
        std::memset(keys_pressed, 0, sizeof(keys_pressed));
        last_char_input = 0;
        last_input_str = "";
        cursor_counter = (cursor_counter + 1) % 60;

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return running;
    }

    void clear(Color color = Colors::DarkBg) {
        if (graphics) {
            graphics->ResetTransform();
            graphics->Clear(color.to_gdip());
            applyScaleTransform();
        }
    }

    void update() {
        if (!hwnd || !hdc_mem) return;
        HDC screen_dc = GetDC(hwnd);
        BitBlt(screen_dc, 0, 0, win_width, win_height, hdc_mem, 0, 0, SRCCOPY);
        ReleaseDC(hwnd, screen_dc);
    }

    void set_icon(const std::string& ico_path) {
        if (!hwnd) return;
        std::wstring w_path = utf8_to_wstring(ico_path);
        HICON hIcon = (HICON)LoadImageW(NULL, w_path.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        HICON hIconSm = (HICON)LoadImageW(NULL, w_path.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (hIcon) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        if (hIconSm) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    }

    void set_design_size(int w, int h) {
        design_width = w;
        design_height = h;
        autoscale = true;
        applyScaleTransform();
    }

    void set_autoscale(bool s) {
        autoscale = s;
        applyScaleTransform();
    }

    // Coordinates & Properties
    int width() const { return autoscale ? design_width : win_width; }
    int height() const { return autoscale ? design_height : win_height; }
    int actual_width() const { return win_width; }
    int actual_height() const { return win_height; }
    int mouse_x() const { return m_x; }
    int mouse_y() const { return m_y; }
    bool mouse_down() const { return m_down; }
    bool mouse_clicked() const { return m_clicked; }
    bool key_down(int vk) const { return (vk >= 0 && vk < 256) ? keys[vk] : false; }
    bool key_pressed(int vk) const { return (vk >= 0 && vk < 256) ? keys_pressed[vk] : false; }

    // -------------------------------------------------------------------------
    // 2D DRAWING PRIMITIVES
    // -------------------------------------------------------------------------

    void draw_rect(int x, int y, int w, int h, Color color, bool fill = true, Color border_color = Color(0, 0, 0, 0), int border_w = 1) {
        if (!graphics) return;
        if (fill) {
            Gdiplus::SolidBrush brush(color.to_gdip());
            graphics->FillRectangle(&brush, x, y, w, h);
        }
        if (border_color.a > 0 && border_w > 0) {
            Gdiplus::Pen pen(border_color.to_gdip(), (Gdiplus::REAL)border_w);
            graphics->DrawRectangle(&pen, x, y, w, h);
        }
    }

    void draw_round_rect(int x, int y, int w, int h, int radius, Color color, bool fill = true, Color border_color = Color(0, 0, 0, 0), int border_w = 1) {
        if (!graphics) return;
        Gdiplus::GraphicsPath path;
        int d = radius * 2;
        path.AddArc(x, y, d, d, 180, 90);
        path.AddArc(x + w - d, y, d, d, 270, 90);
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
        path.CloseFigure();

        if (fill) {
            Gdiplus::SolidBrush brush(color.to_gdip());
            graphics->FillPath(&brush, &path);
        }
        if (border_color.a > 0 && border_w > 0) {
            Gdiplus::Pen pen(border_color.to_gdip(), (Gdiplus::REAL)border_w);
            graphics->DrawPath(&pen, &path);
        }
    }

    void draw_circle(int cx, int cy, int radius, Color color, bool fill = true) {
        if (!graphics) return;
        if (fill) {
            Gdiplus::SolidBrush brush(color.to_gdip());
            graphics->FillEllipse(&brush, cx - radius, cy - radius, radius * 2, radius * 2);
        } else {
            Gdiplus::Pen pen(color.to_gdip(), 1.0f);
            graphics->DrawEllipse(&pen, cx - radius, cy - radius, radius * 2, radius * 2);
        }
    }

    void draw_line(int x1, int y1, int x2, int y2, Color color, int width = 1) {
        if (!graphics) return;
        Gdiplus::Pen pen(color.to_gdip(), (Gdiplus::REAL)width);
        graphics->DrawLine(&pen, x1, y1, x2, y2);
    }

    void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color, bool fill = true, Color border_color = Color(0, 0, 0, 0), int border_w = 1) {
        if (!graphics) return;
        Gdiplus::Point pts[3] = {
            Gdiplus::Point(x1, y1),
            Gdiplus::Point(x2, y2),
            Gdiplus::Point(x3, y3)
        };
        if (fill) {
            Gdiplus::SolidBrush brush(color.to_gdip());
            graphics->FillPolygon(&brush, pts, 3);
        }
        if (border_color.a > 0 && border_w > 0) {
            Gdiplus::Pen pen(border_color.to_gdip(), (Gdiplus::REAL)border_w);
            graphics->DrawPolygon(&pen, pts, 3);
        }
    }

    void draw_quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, Color color, bool fill = true, Color border_color = Color(0, 0, 0, 0), int border_w = 1) {
        if (!graphics) return;
        Gdiplus::Point pts[4] = {
            Gdiplus::Point(x1, y1),
            Gdiplus::Point(x2, y2),
            Gdiplus::Point(x3, y3),
            Gdiplus::Point(x4, y4)
        };
        if (fill) {
            Gdiplus::SolidBrush brush(color.to_gdip());
            graphics->FillPolygon(&brush, pts, 4);
        }
        if (border_color.a > 0 && border_w > 0) {
            Gdiplus::Pen pen(border_color.to_gdip(), (Gdiplus::REAL)border_w);
            graphics->DrawPolygon(&pen, pts, 4);
        }
    }

    void draw_text(int x, int y, const std::string& text, int size) {
        draw_text(x, y, text, Colors::TextLight, size, "Segoe UI Emoji", false);
    }

    void draw_text(int x, int y, const std::string& text, Color color = Colors::TextLight, int size = 14, const std::string& font_name = "Segoe UI Emoji", bool bold = false) {
        if (!graphics || text.empty()) return;

        std::wstring w_text = utf8_to_wstring(text);
        std::wstring w_font = utf8_to_wstring(font_name);

        int style = bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;
        Gdiplus::Font font(w_font.c_str(), (Gdiplus::REAL)size, style, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(color.to_gdip());

        Gdiplus::PointF origin((Gdiplus::REAL)x, (Gdiplus::REAL)y);
        graphics->DrawString(w_text.c_str(), -1, &font, origin, &brush);
    }

    void draw_image(int x, int y, int w, int h, const std::string& image_path) {
        if (!graphics || image_path.empty()) return;
        std::wstring w_path = utf8_to_wstring(image_path);
        Gdiplus::Image img(w_path.c_str());
        if (img.GetLastStatus() == Gdiplus::Ok) {
            graphics->DrawImage(&img, x, y, w, h);
        }
    }

    // -------------------------------------------------------------------------
    // IMMEDIATE-MODE UI WIDGETS
    // -------------------------------------------------------------------------

    // Interactive Button: returns true on click
    bool button(int x, int y, int w, int h, const std::string& label, Color bg = Colors::PanelBg, Color text_color = Colors::TextLight, int radius = 4, int font_size = 12, bool bold = true) {
        bool hovered = (m_x >= x && m_x <= x + w && m_y >= y && m_y <= y + h);
        Color draw_bg = bg;
        if (hovered) {
            if (m_down) {
                draw_bg = Color::rgb(std::min(255, bg.r + 30), std::min(255, bg.g + 30), std::min(255, bg.b + 30));
            } else {
                draw_bg = Color::rgb(std::min(255, bg.r + 15), std::min(255, bg.g + 15), std::min(255, bg.b + 15));
            }
        }

        if (radius > 0) {
            draw_round_rect(x, y, w, h, radius, draw_bg, true, Colors::PanelBorder, 1);
        } else {
            draw_rect(x, y, w, h, draw_bg, true, Colors::PanelBorder, 1);
        }

        std::wstring w_label = utf8_to_wstring(label);
        Gdiplus::Font font(L"Segoe UI Emoji", (Gdiplus::REAL)font_size, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::RectF bound;
        graphics->MeasureString(w_label.c_str(), -1, &font, Gdiplus::PointF(0, 0), &bound);

        int text_x = x + std::max(4, (int)((w - bound.Width) / 2));
        int text_y = y + (int)((h - bound.Height) / 2);
        draw_text(text_x, text_y, label, text_color, font_size, "Segoe UI Emoji", bold);

        return hovered && m_clicked;
    }

    // Label
    void label(int x, int y, const std::string& text, int size) {
        draw_text(x, y, text, Colors::TextLight, size, "Segoe UI Emoji", false);
    }

    void label(int x, int y, const std::string& text, Color color = Colors::TextLight, int size = 14, bool bold = false) {
        draw_text(x, y, text, color, size, "Segoe UI Emoji", bold);
    }

    // Card / Panel container
    void card(int x, int y, int w, int h, Color bg = Colors::PanelBg, Color border = Colors::PanelBorder, int radius = 6) {
        draw_round_rect(x, y, w, h, radius, bg, true, border, 1);
    }

    // LabelFrame / Card with Header Title on Top Border (exact Tkinter LabelFrame)
    void card_group(int x, int y, int w, int h, const std::string& title, Color bg = Colors::PanelBg, Color border = Colors::PanelBorder) {
        if (!graphics) return;
        draw_round_rect(x, y, w, h, 6, bg, true, border, 1);
        if (!title.empty()) {
            std::wstring w_title = utf8_to_wstring(title);
            Gdiplus::Font font(L"Segoe UI Emoji", 12.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::RectF bound;
            graphics->MeasureString(w_title.c_str(), -1, &font, Gdiplus::PointF(0, 0), &bound);
            int tw = (int)bound.Width + 14;
            draw_rect(x + 12, y - 1, tw, 3, bg, true);
            Gdiplus::SolidBrush brush(Colors::TextLight.to_gdip());
            graphics->DrawString(w_title.c_str(), -1, &font, Gdiplus::PointF((Gdiplus::REAL)(x + 16), (Gdiplus::REAL)(y - 8)), &brush);
        }
    }

    std::string open_file(const std::string& title = "Select File") {
        OPENFILENAMEW ofn;
        wchar_t szFile[MAX_PATH] = { 0 };
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        std::wstring wfilter = L"Supported Images (*.jpg;*.jpeg;*.png;*.webp)\0*.jpg;*.jpeg;*.png;*.webp\0All Files (*.*)\0*.*\0";
        ofn.lpstrFilter = wfilter.c_str();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        std::wstring w_title = utf8_to_wstring(title);
        ofn.lpstrTitle = w_title.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&ofn)) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, NULL, 0, NULL, NULL);
            std::string res(size_needed - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &res[0], size_needed, NULL, NULL);
            return res;
        }
        return "";
    }

    std::string browse_folder(const std::string& title = "Select Folder:") {
        OleInitialize(NULL);
        BROWSEINFOW bi = { 0 };
        bi.hwndOwner = hwnd;
        std::wstring w_title = utf8_to_wstring(title);
        bi.lpszTitle = w_title.c_str();
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl != 0) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                IMalloc* imalloc = 0;
                if (SUCCEEDED(SHGetMalloc(&imalloc))) {
                    imalloc->Free(pidl);
                    imalloc->Release();
                }
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
                std::string res(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, path, -1, &res[0], size_needed, NULL, NULL);
                return res;
            }
        }
        return "";
    }

    // Checkbox: returns toggled state
    bool checkbox(int x, int y, const std::string& label_text, bool& checked) {
        int box_sz = 18;
        bool hovered = (m_x >= x && m_x <= x + box_sz + 150 && m_y >= y && m_y <= y + box_sz);
        if (hovered && m_clicked) {
            checked = !checked;
        }

        draw_round_rect(x, y, box_sz, box_sz, 4, checked ? Colors::AccentCyan : Colors::InputBg, true, Colors::PanelBorder, 1);
        if (checked) {
            // Draw checkmark
            draw_line(x + 4, y + 9, x + 8, y + 13, Colors::DarkBg, 2);
            draw_line(x + 8, y + 13, x + 14, y + 5, Colors::DarkBg, 2);
        }

        draw_text(x + box_sz + 8, y + 1, label_text, Colors::TextLight, 13, "Segoe UI", false);
        return checked;
    }

    // Progress Bar
    void progress_bar(int x, int y, int w, int h, double percent, Color fill_color = Colors::AccentCyan, Color bg_color = Colors::InputBg) {
        if (percent < 0.0) percent = 0.0;
        if (percent > 1.0) percent = 1.0;
        draw_round_rect(x, y, w, h, h / 2, bg_color, true, Colors::PanelBorder, 1);
        int fill_w = (int)(w * percent);
        if (fill_w > h) {
            draw_round_rect(x, y, fill_w, h, h / 2, fill_color, true);
        }
    }

    // Slider: returns updated value
    template<typename T>
    T slider(int x, int y, int w, int h, T& value, T min_val, T max_val, const std::string& title = "") {
        int bar_h = 6;
        int bar_y = y + (h - bar_h) / 2;
        int knob_r = 8;

        float norm = (max_val > min_val) ? (float)((value - min_val) / (max_val - min_val)) : 0.0f;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;

        bool active = (m_down && m_x >= x - 5 && m_x <= x + w + 5 && m_y >= y - 5 && m_y <= y + h + 5);
        if (active) {
            float new_norm = (float)(m_x - x) / (float)w;
            if (new_norm < 0.0f) new_norm = 0.0f;
            if (new_norm > 1.0f) new_norm = 1.0f;
            value = (T)(min_val + new_norm * (max_val - min_val));
            norm = new_norm;
        }

        // Draw track
        draw_round_rect(x, bar_y, w, bar_h, 3, Colors::InputBg, true, Colors::PanelBorder, 1);
        draw_round_rect(x, bar_y, (int)(w * norm), bar_h, 3, Colors::AccentCyan, true);

        // Draw knob
        int knob_x = x + (int)(w * norm);
        draw_circle(knob_x, bar_y + bar_h / 2, knob_r, Colors::TextLight, true);
        draw_circle(knob_x, bar_y + bar_h / 2, knob_r - 2, Colors::AccentCyan, true);

        if (!title.empty()) {
            draw_text(x, y - 16, title, Colors::TextDim, 12);
        }

        return value;
    }

    std::string get_clipboard_text() {
        if (!OpenClipboard(hwnd)) return "";
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData) {
            CloseClipboard();
            return "";
        }
        wchar_t* pText = (wchar_t*)GlobalLock(hData);
        if (!pText) {
            CloseClipboard();
            return "";
        }
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, pText, -1, NULL, 0, NULL, NULL);
        std::string res = "";
        if (size_needed > 1) {
            res.resize(size_needed - 1);
            WideCharToMultiByte(CP_UTF8, 0, pText, -1, &res[0], size_needed, NULL, NULL);
        }
        GlobalUnlock(hData);
        CloseClipboard();
        return res;
    }

    void set_clipboard_text(const std::string& text) {
        if (!OpenClipboard(hwnd)) return;
        EmptyClipboard();
        std::wstring w_text = utf8_to_wstring(text);
        size_t bytes = (w_text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, w_text.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }

    // Editable Text Input Box (accurate caret, arrow keys, click positioning, auto-scroll)
    std::string text_input(int x, int y, int w, int h, const std::string& id, std::string& text, const std::string& placeholder = "") {
        bool hovered = (m_x >= x && m_x <= x + w && m_y >= y && m_y <= y + h);
        if (hovered && m_clicked) {
            active_input_id = id;
        } else if (m_clicked && !hovered && active_input_id == id) {
            active_input_id = "";
        }

        bool focused = (active_input_id == id);
        std::wstring w_text = utf8_to_wstring(text);

        if (input_carets.find(id) == input_carets.end()) {
            input_carets[id] = (int)w_text.size();
        }
        int& caret = input_carets[id];
        if (caret < 0) caret = 0;
        if (caret > (int)w_text.size()) caret = (int)w_text.size();

        Gdiplus::Font font(L"Segoe UI Emoji", 12.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
        fmt.SetFormatFlags(fmt.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces | Gdiplus::StringFormatFlagsNoWrap);

        int& scroll_x = input_scrolls[id];

        // Click to place caret
        if (hovered && m_clicked && graphics) {
            float click_x = (float)(m_x - (x + 10) + scroll_x);
            int best_pos = 0;
            float best_dist = 1e9f;
            for (int i = 0; i <= (int)w_text.size(); ++i) {
                Gdiplus::RectF bound;
                graphics->MeasureString(w_text.substr(0, i).c_str(), i, &font, Gdiplus::PointF(0,0), &fmt, &bound);
                float dist = std::abs(bound.Width - click_x);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_pos = i;
                }
            }
            caret = best_pos;
            cursor_counter = 0;
        }

        if (focused) {
            bool modified = false;
            if (keys_pressed[VK_LEFT]) {
                if (caret > 0) caret--;
                cursor_counter = 0;
            } else if (keys_pressed[VK_RIGHT]) {
                if (caret < (int)w_text.size()) caret++;
                cursor_counter = 0;
            } else if (keys_pressed[VK_HOME]) {
                caret = 0;
                cursor_counter = 0;
            } else if (keys_pressed[VK_END]) {
                caret = (int)w_text.size();
                cursor_counter = 0;
            } else if (keys_pressed[VK_DELETE]) {
                if (caret < (int)w_text.size()) {
                    w_text.erase(caret, 1);
                    modified = true;
                }
            } else if (last_char_input == '\b' || keys_pressed[VK_BACK]) {
                if (caret > 0 && !w_text.empty()) {
                    w_text.erase(caret - 1, 1);
                    caret--;
                    modified = true;
                }
            } else if (last_char_input == 22 || (keys[VK_CONTROL] && (keys_pressed['V'] || keys_pressed['v']))) {
                std::string clip = get_clipboard_text();
                std::wstring w_clip;
                for (wchar_t wc : utf8_to_wstring(clip)) {
                    if (wc != L'\r' && wc != L'\n') w_clip += wc;
                }
                if (!w_clip.empty()) {
                    w_text.insert(caret, w_clip);
                    caret += (int)w_clip.size();
                    modified = true;
                }
            } else if (last_char_input == 1) {
                w_text.clear();
                caret = 0;
                modified = true;
            } else if (last_char_input == 3) {
                set_clipboard_text(text);
            } else if (!last_input_str.empty() && last_char_input != '\r' && last_char_input != '\n') {
                std::wstring w_ins = utf8_to_wstring(last_input_str);
                w_text.insert(caret, w_ins);
                caret += (int)w_ins.size();
                modified = true;
            }

            if (modified) {
                text = wstring_to_utf8(w_text);
                cursor_counter = 0;
            }
        }

        Color border = focused ? Colors::AccentCyan : Colors::PanelBorder;
        draw_round_rect(x, y, w, h, 6, Colors::InputBg, true, border, focused ? 2 : 1);

        // Clip text rendering to inside the box so text does not bleed out
        Gdiplus::Region old_clip;
        graphics->GetClip(&old_clip);
        Gdiplus::RectF clip_rect((Gdiplus::REAL)(x + 4), (Gdiplus::REAL)(y + 2), (Gdiplus::REAL)(w - 8), (Gdiplus::REAL)(h - 4));
        graphics->SetClip(clip_rect, Gdiplus::CombineModeIntersect);

        // Measure caret position
        Gdiplus::RectF caret_bound;
        graphics->MeasureString(w_text.substr(0, caret).c_str(), caret, &font, Gdiplus::PointF(0,0), &fmt, &caret_bound);

        // Scroll offset adjustment
        float caret_screen_x = (x + 10) - scroll_x + caret_bound.Width;
        if (caret_screen_x > x + w - 24) {
            scroll_x = (int)(caret_bound.Width - (w - 34));
        } else if (caret_screen_x < x + 10) {
            scroll_x = (int)caret_bound.Width;
        }
        if (scroll_x < 0) scroll_x = 0;

        float draw_x = (float)(x + 10 - scroll_x);
        float draw_y = (float)(y + (h - 16) / 2);

        if (text.empty() && !placeholder.empty() && !focused) {
            std::wstring w_ph = utf8_to_wstring(placeholder);
            Gdiplus::SolidBrush dimBrush(Colors::TextDim.to_gdip());
            graphics->DrawString(w_ph.c_str(), -1, &font, Gdiplus::PointF(draw_x, draw_y), &fmt, &dimBrush);
        } else {
            Gdiplus::SolidBrush textBrush(Colors::TextLight.to_gdip());
            graphics->DrawString(w_text.c_str(), -1, &font, Gdiplus::PointF(draw_x, draw_y), &fmt, &textBrush);
        }

        if (focused && (cursor_counter < 30)) {
            int cur_x = (int)(draw_x + caret_bound.Width);
            draw_line(cur_x, y + 6, cur_x, y + h - 6, Colors::AccentCyan, 2);
        }

        graphics->SetClip(&old_clip);
        return text;
    }

    // Multiline Text Area Box (supports Enter, wrapping, clipboard paste, caret positioning)
    std::string textarea(int x, int y, int w, int h, const std::string& id, std::string& text, const std::string& placeholder = "") {
        bool hovered = (m_x >= x && m_x <= x + w && m_y >= y && m_y <= y + h);
        if (hovered && m_clicked) {
            active_input_id = id;
        } else if (m_clicked && !hovered && active_input_id == id) {
            active_input_id = "";
        }

        bool focused = (active_input_id == id);
        std::wstring w_text = utf8_to_wstring(text);

        if (input_carets.find(id) == input_carets.end()) {
            input_carets[id] = (int)w_text.size();
        }
        int& caret = input_carets[id];
        if (caret < 0) caret = 0;
        if (caret > (int)w_text.size()) caret = (int)w_text.size();

        Gdiplus::Font font(L"Segoe UI Emoji", 11.5f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
        fmt.SetFormatFlags(fmt.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

        const float line_h = 18.0f;

        // Break w_text into lines to know line and column
        auto get_line_col = [&](int pos, int& out_l_idx, size_t& out_l_start, size_t& out_col) {
            out_l_idx = 0;
            out_l_start = 0;
            out_col = 0;
            for (int i = 0; i <= pos && i <= (int)w_text.size(); ++i) {
                if (i == pos) {
                    out_col = i - out_l_start;
                    break;
                }
                if (w_text[i] == L'\n') {
                    out_l_idx++;
                    out_l_start = i + 1;
                }
            }
        };

        // Handle mouse click to position caret
        if (hovered && m_clicked && graphics) {
            int clicked_line = (int)((m_y - (y + 8)) / line_h);
            if (clicked_line < 0) clicked_line = 0;
            int l_idx = 0;
            size_t l_start = 0;
            for (size_t i = 0; i <= w_text.size(); ++i) {
                if (i == w_text.size() || w_text[i] == L'\n') {
                    if (l_idx == clicked_line || i == w_text.size()) {
                        size_t l_end = i;
                        std::wstring l_str = w_text.substr(l_start, l_end - l_start);
                        float click_x = (float)(m_x - (x + 10));
                        int best_col = 0;
                        float best_dist = 1e9f;
                        for (int c = 0; c <= (int)l_str.size(); ++c) {
                            Gdiplus::RectF bound;
                            graphics->MeasureString(l_str.substr(0, c).c_str(), c, &font, Gdiplus::PointF(0,0), &fmt, &bound);
                            float dist = std::abs(bound.Width - click_x);
                            if (dist < best_dist) {
                                best_dist = dist;
                                best_col = c;
                            }
                        }
                        caret = (int)(l_start + best_col);
                        break;
                    }
                    l_idx++;
                    l_start = i + 1;
                }
            }
            cursor_counter = 0;
        }

        if (focused) {
            bool modified = false;
            int cur_l = 0; size_t cur_start = 0, cur_col = 0;
            get_line_col(caret, cur_l, cur_start, cur_col);

            if (keys_pressed[VK_LEFT]) {
                if (caret > 0) caret--;
                cursor_counter = 0;
            } else if (keys_pressed[VK_RIGHT]) {
                if (caret < (int)w_text.size()) caret++;
                cursor_counter = 0;
            } else if (keys_pressed[VK_HOME]) {
                caret = (int)cur_start;
                cursor_counter = 0;
            } else if (keys_pressed[VK_END]) {
                size_t l_end = w_text.find(L'\n', cur_start);
                if (l_end == std::wstring::npos) l_end = w_text.size();
                caret = (int)l_end;
                cursor_counter = 0;
            } else if (keys_pressed[VK_UP]) {
                if (cur_l > 0) {
                    int prev_l = cur_l - 1;
                    int li = 0; size_t ls = 0;
                    for (size_t i = 0; i <= w_text.size(); ++i) {
                        if (i == w_text.size() || w_text[i] == L'\n') {
                            if (li == prev_l) {
                                size_t plen = i - ls;
                                caret = (int)(ls + std::min(cur_col, plen));
                                break;
                            }
                            li++; ls = i + 1;
                        }
                    }
                }
                cursor_counter = 0;
            } else if (keys_pressed[VK_DOWN]) {
                size_t next_start = w_text.find(L'\n', cur_start);
                if (next_start != std::wstring::npos) {
                    next_start++;
                    size_t next_end = w_text.find(L'\n', next_start);
                    if (next_end == std::wstring::npos) next_end = w_text.size();
                    size_t nlen = next_end - next_start;
                    caret = (int)(next_start + std::min(cur_col, nlen));
                }
                cursor_counter = 0;
            } else if (keys_pressed[VK_DELETE]) {
                if (caret < (int)w_text.size()) {
                    w_text.erase(caret, 1);
                    modified = true;
                }
            } else if (last_char_input == '\b' || keys_pressed[VK_BACK]) {
                if (caret > 0 && !w_text.empty()) {
                    w_text.erase(caret - 1, 1);
                    caret--;
                    modified = true;
                }
            } else if (last_char_input == '\n' || keys_pressed[VK_RETURN]) {
                w_text.insert(caret, L"\n");
                caret++;
                modified = true;
            } else if (last_char_input == 22 || (keys[VK_CONTROL] && (keys_pressed['V'] || keys_pressed['v']))) {
                std::string clip = get_clipboard_text();
                std::wstring w_clip;
                for (wchar_t wc : utf8_to_wstring(clip)) {
                    if (wc != L'\r') w_clip += wc;
                }
                if (!w_clip.empty()) {
                    w_text.insert(caret, w_clip);
                    caret += (int)w_clip.size();
                    modified = true;
                }
            } else if (last_char_input == 1) {
                w_text.clear();
                caret = 0;
                modified = true;
            } else if (last_char_input == 3) {
                set_clipboard_text(text);
            } else if (!last_input_str.empty()) {
                std::wstring w_ins = utf8_to_wstring(last_input_str);
                w_text.insert(caret, w_ins);
                caret += (int)w_ins.size();
                modified = true;
            }

            if (modified) {
                text = wstring_to_utf8(w_text);
                cursor_counter = 0;
            }
        }

        Color border = focused ? Colors::AccentCyan : Colors::PanelBorder;
        draw_round_rect(x, y, w, h, 6, Colors::InputBg, true, border, focused ? 2 : 1);

        Gdiplus::Region old_clip;
        graphics->GetClip(&old_clip);
        Gdiplus::RectF clip_rect((Gdiplus::REAL)(x + 4), (Gdiplus::REAL)(y + 4), (Gdiplus::REAL)(w - 8), (Gdiplus::REAL)(h - 8));
        graphics->SetClip(clip_rect, Gdiplus::CombineModeIntersect);

        if (text.empty() && !placeholder.empty() && !focused) {
            std::wstring w_ph = utf8_to_wstring(placeholder);
            Gdiplus::SolidBrush dimBrush(Colors::TextDim.to_gdip());
            graphics->DrawString(w_ph.c_str(), -1, &font, Gdiplus::PointF((float)(x + 10), (float)(y + 8)), &fmt, &dimBrush);
        } else {
            Gdiplus::SolidBrush textBrush(Colors::TextLight.to_gdip());
            size_t l_start = 0;
            int l_idx = 0;
            for (size_t i = 0; i <= w_text.size(); ++i) {
                if (i == w_text.size() || w_text[i] == L'\n') {
                    std::wstring line = w_text.substr(l_start, i - l_start);
                    graphics->DrawString(line.c_str(), -1, &font, Gdiplus::PointF((float)(x + 10), (float)(y + 8 + l_idx * line_h)), &fmt, &textBrush);
                    l_idx++;
                    l_start = i + 1;
                }
            }
        }

        if (focused && (cursor_counter < 30)) {
            int cur_l = 0; size_t cur_start = 0, cur_col = 0;
            get_line_col(caret, cur_l, cur_start, cur_col);
            std::wstring sub = w_text.substr(cur_start, cur_col);
            Gdiplus::RectF c_bound;
            graphics->MeasureString(sub.c_str(), (int)sub.size(), &font, Gdiplus::PointF(0,0), &fmt, &c_bound);
            int cur_x = (int)(x + 10 + c_bound.Width);
            int cur_y = (int)(y + 8 + cur_l * line_h);
            draw_line(cur_x, cur_y, cur_x, cur_y + (int)line_h - 2, Colors::AccentCyan, 2);
        }

        graphics->SetClip(&old_clip);
        return text;
    }
};

inline WindowApp& getApp() {
    static WindowApp app;
    return app;
}

// -----------------------------------------------------------------------------
// GLOBAL PUBLIC API FOR VISS
// -----------------------------------------------------------------------------
inline bool create(const std::string& title, int w = 800, int h = 600) { return getApp().create(title, w, h); }
inline bool window(const std::string& title, int w = 800, int h = 600) { return getApp().create(title, w, h); }
inline bool is_open() { return getApp().is_open(); }
inline bool should_close() { return !getApp().is_open(); }
inline bool poll() { return getApp().poll_events(); }
inline bool poll_events() { return getApp().poll_events(); }
inline void clear(Color c = Colors::DarkBg) { getApp().clear(c); }
inline void update() { getApp().update(); }
inline void close() { getApp().close(); }

inline int width() { return getApp().width(); }
inline int height() { return getApp().height(); }
inline int actual_width() { return getApp().actual_width(); }
inline int actual_height() { return getApp().actual_height(); }
inline void set_design_size(int w, int h) { getApp().set_design_size(w, h); }
inline void autoscale(int w, int h) { getApp().set_design_size(w, h); }
inline void set_autoscale(bool s) { getApp().set_autoscale(s); }
inline void set_icon(const std::string& ico_path) { getApp().set_icon(ico_path); }

inline int mouse_x() { return getApp().mouse_x(); }
inline int mouse_y() { return getApp().mouse_y(); }
inline bool mouse_down() { return getApp().mouse_down(); }
inline bool mouse_clicked() { return getApp().mouse_clicked(); }
inline bool key_down(int vk) { return getApp().key_down(vk); }
inline bool key_pressed(int vk) { return getApp().key_pressed(vk); }

inline void draw_rect(int x, int y, int w, int h, Color c, bool fill = true) { getApp().draw_rect(x, y, w, h, c, fill); }
inline void draw_round_rect(int x, int y, int w, int h, int r, Color c, bool fill = true, Color border = Colors::PanelBorder, int border_w = 0) {
    getApp().draw_round_rect(x, y, w, h, r, c, fill, border, border_w);
}
inline void draw_circle(int cx, int cy, int r, Color c, bool fill = true) { getApp().draw_circle(cx, cy, r, c, fill); }
inline void draw_line(int x1, int y1, int x2, int y2, Color c, int w = 1) { getApp().draw_line(x1, y1, x2, y2, c, w); }
inline void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color c, bool fill = true, Color border = Color(0,0,0,0), int border_w = 0) {
    getApp().draw_triangle(x1, y1, x2, y2, x3, y3, c, fill, border, border_w);
}
inline void draw_quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, Color c, bool fill = true, Color border = Color(0,0,0,0), int border_w = 0) {
    getApp().draw_quad(x1, y1, x2, y2, x3, y3, x4, y4, c, fill, border, border_w);
}
inline void draw_text(int x, int y, const std::string& text, int size) { getApp().draw_text(x, y, text, size); }
inline void draw_text(int x, int y, const std::string& text, Color c = Colors::TextLight, int size = 14) { getApp().draw_text(x, y, text, c, size); }
inline void draw_text(int x, int y, const std::string& text, Color c, int size, const std::string& font_name, bool bold = false) {
    getApp().draw_text(x, y, text, c, size, font_name, bold);
}
inline void draw_image(int x, int y, int w, int h, const std::string& path) { getApp().draw_image(x, y, w, h, path); }

// UI Widgets
inline bool button(int x, int y, int w, int h, const std::string& label, Color bg = Colors::PanelBg, Color text = Colors::TextLight, int radius = 4, int font_size = 12, bool bold = true) {
    return getApp().button(x, y, w, h, label, bg, text, radius, font_size, bold);
}
inline void label(int x, int y, const std::string& text, int size) {
    getApp().label(x, y, text, size);
}
inline void label(int x, int y, const std::string& text, Color c = Colors::TextLight, int size = 14, bool bold = false) {
    getApp().label(x, y, text, c, size, bold);
}
inline void card(int x, int y, int w, int h, Color bg = Colors::PanelBg, Color border = Colors::PanelBorder, int radius = 6) {
    getApp().card(x, y, w, h, bg, border, radius);
}
inline void card_group(int x, int y, int w, int h, const std::string& title, Color bg = Colors::PanelBg, Color border = Colors::PanelBorder) {
    getApp().card_group(x, y, w, h, title, bg, border);
}
inline std::string open_file(const std::string& title = "Select File") {
    return getApp().open_file(title);
}
inline std::string browse_folder(const std::string& title = "Select Folder:") {
    return getApp().browse_folder(title);
}
inline std::string dropped_file() {
    return getApp().get_dropped_file();
}
inline bool checkbox(int x, int y, const std::string& text, bool& checked) {
    return getApp().checkbox(x, y, text, checked);
}
inline void progress_bar(int x, int y, int w, int h, double percent, Color fill = Colors::AccentCyan) {
    getApp().progress_bar(x, y, w, h, percent, fill);
}
inline double slider(int x, int y, int w, int h, double& val, double min_v, double max_v, const std::string& title = "") {
    return getApp().slider<double>(x, y, w, h, val, min_v, max_v, title);
}
inline float slider(int x, int y, int w, int h, float& val, float min_v, float max_v, const std::string& title = "") {
    return getApp().slider<float>(x, y, w, h, val, min_v, max_v, title);
}
inline std::string text_input(int x, int y, int w, int h, const std::string& id, std::string& text, const std::string& placeholder = "") {
    return getApp().text_input(x, y, w, h, id, text, placeholder);
}
inline std::string textarea(int x, int y, int w, int h, const std::string& id, std::string& text, const std::string& placeholder = "") {
    return getApp().textarea(x, y, w, h, id, text, placeholder);
}
inline std::string get_clipboard() {
    return getApp().get_clipboard_text();
}
inline void set_clipboard(const std::string& text) {
    getApp().set_clipboard_text(text);
}

#endif

} // namespace gui
} // namespace viss
