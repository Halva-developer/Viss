#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <exception>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <GL/gl.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

namespace viss {
    using Str = std::string;
    using Int = long long;
    using Dec = double;
    using Bool = bool;

    struct Point {
        Int x = 0;
        Int y = 0;
    };

    template<typename T>
    class List {
    private:
        std::vector<T> data;
    public:
        List() = default;
        List(std::initializer_list<T> init) : data(init) {}
        
        inline void add(const T& item) {
            data.push_back(item);
        }
        inline void insert(Int index, const T& item) {
            if (index >= 0 && index <= (Int)data.size()) {
                data.insert(data.begin() + index, item);
            }
        }
        inline void removeAt(Int index) {
            if (index >= 0 && index < (Int)data.size()) {
                data.erase(data.begin() + index);
            }
        }
        inline void removeLast() {
            if (!data.empty()) {
                data.pop_back();
            }
        }
        inline T get(Int index) const {
            if (index >= 0 && index < (Int)data.size()) {
                return data[index];
            }
            return T();
        }
        inline Int size() const {
            return (Int)data.size();
        }
        inline void clear() {
            data.clear();
        }
    };

    class Error : public std::exception {
    private:
        Str msg;
    public:
        Error(const Str& m) : msg(m) {}
        virtual const char* what() const noexcept override {
            return msg.c_str();
        }
        inline Str message() const {
            return msg;
        }
    };

    template<typename K, typename V>
    class Map {
    private:
        std::unordered_map<K, V> data;
    public:
        Map() = default;
        
        inline void set(const K& key, const V& val) {
            data[key] = val;
        }
        inline V get(const K& key) const {
            auto it = data.find(key);
            if (it != data.end()) {
                return it->second;
            }
            return V();
        }
        inline Bool has(const K& key) const {
            return data.find(key) != data.end();
        }
        inline void remove(const K& key) {
            data.erase(key);
        }
        inline Int size() const {
            return (Int)data.size();
        }
        inline void clear() {
            data.clear();
        }
        inline List<K> keys() const {
            List<K> kList;
            for (const auto& pair : data) {
                kList.add(pair.first);
            }
            return kList;
        }
    };

    namespace sys {
        inline void seed() {
            #ifdef _WIN32
            srand(GetTickCount());
            #else
            srand((unsigned int)time(nullptr));
            #endif
        }
        inline Int random(Int min, Int max) {
            if (max <= min) return min;
            return min + (rand() % (max - min));
        }
    }

    namespace io {
        inline void println(const Str& value) { std::cout << value << "\n"; }
        inline void println(Int value) { std::cout << value << "\n"; }
        inline void println(Dec value) { std::cout << value << "\n"; }
        inline void println(Bool value) { std::cout << (value ? "true" : "false") << "\n"; }

        inline void print(const Str& value) { std::cout << value; }
        inline void print(Int value) { std::cout << value; }
        inline void print(Dec value) { std::cout << value; }
        inline void print(Bool value) { std::cout << (value ? "true" : "false"); }

        inline void eprint(const Str& value) { std::cerr << value; }
        inline void eprintln(const Str& value) { std::cerr << value << "\n"; }

        inline Str readln() {
            Str s;
            std::getline(std::cin, s);
            return s;
        }

        inline void color(Int colorCode) {
            #ifdef _WIN32
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)colorCode);
            #else
            int ansiCode = 37;
            switch (colorCode) {
                case 0: ansiCode = 30; break;
                case 1: ansiCode = 34; break;
                case 2: ansiCode = 32; break;
                case 3: ansiCode = 36; break;
                case 4: ansiCode = 31; break;
                case 5: ansiCode = 35; break;
                case 6: ansiCode = 33; break;
                case 7: ansiCode = 37; break;
                case 8: ansiCode = 90; break;
                case 9: ansiCode = 94; break;
                case 10: ansiCode = 92; break;
                case 11: ansiCode = 96; break;
                case 12: ansiCode = 91; break;
                case 13: ansiCode = 95; break;
                case 14: ansiCode = 93; break;
                case 15: ansiCode = 97; break;
            }
            std::cout << "\033[" << ansiCode << "m";
            #endif
        }
        inline void clear() {
            #ifdef _WIN32
            COORD topLeft  = { 0, 0 };
            HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO screen;
            DWORD written;
            GetConsoleScreenBufferInfo(console, &screen);
            FillConsoleOutputCharacterA(console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
            FillConsoleOutputAttribute(console, screen.wAttributes, screen.dwSize.X * screen.dwSize.Y, topLeft, &written);
            SetConsoleCursorPosition(console, topLeft);
            #else
            std::cout << "\033[2J\033[1;1H";
            #endif
        }
        inline void cursor(Int x, Int y) {
            #ifdef _WIN32
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
            #else
            std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H";
            #endif
        }
    }

    namespace fs {
        inline void write(const Str& path, const Str& content) {
            std::ofstream f(path);
            if (f.is_open()) {
                f << content;
            }
        }
        inline Str read(const Str& path) {
            std::ifstream f(path);
            if (!f.is_open()) return "";
            Str content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            return content;
        }
        inline Bool exists(const Str& path) {
            std::ifstream f(path);
            return f.good();
        }
        inline void append(const Str& path, const Str& content) {
            std::ofstream f(path, std::ios::app);
            if (f.is_open()) {
                f << content;
            }
        }
        inline void remove(const Str& path) {
            std::remove(path.c_str());
        }
        inline void mkdir(const Str& path) {
            #ifdef _WIN32
            CreateDirectoryA(path.c_str(), NULL);
            #else
            mkdir(path.c_str(), 0777);
            #endif
        }
    }

    namespace math {
        inline const Dec PI = 3.14159265358979323846;
        inline const Dec E  = 2.71828182845904523536;

        inline Dec sin(Dec x) { return std::sin(x); }
        inline Dec cos(Dec x) { return std::cos(x); }
        inline Dec tan(Dec x) { return std::tan(x); }
        inline Dec sqrt(Dec x) { return std::sqrt(x); }
        inline Dec pow(Dec base, Dec exp) { return std::pow(base, exp); }
        inline Dec abs(Dec x) { return std::abs(x); }
        inline Int abs(Int x) { return std::abs(x); }
        inline Dec round(Dec x) { return std::round(x); }
        inline Dec floor(Dec x) { return std::floor(x); }
        inline Dec ceil(Dec x) { return std::ceil(x); }
    }

    namespace time {
        inline Int now() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline Int ms() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }
        inline void sleep(Int milliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        }
    }

    namespace str {
        inline Int len(const Str& s) {
            return (Int)s.length();
        }
        inline Str sub(const Str& s, Int start, Int len) {
            if (start < 0 || start >= (Int)s.length()) return "";
            return s.substr(start, len);
        }
        inline Int find(const Str& s, const Str& subStr) {
            auto pos = s.find(subStr);
            if (pos == std::string::npos) return -1;
            return (Int)pos;
        }
        inline Str lower(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
            return s;
        }
        inline Str upper(Str s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
            return s;
        }
        inline List<Str> split(const Str& s, const Str& delimiter) {
            List<Str> tokens;
            size_t prev = 0, pos = 0;
            do {
                pos = s.find(delimiter, prev);
                if (pos == std::string::npos) pos = s.length();
                Str token = s.substr(prev, pos - prev);
                tokens.add(token);
                prev = pos + delimiter.length();
            } while (pos < s.length() && prev < s.length());
            return tokens;
        }
    }

    namespace env {
        inline Str get(const Str& name) {
            char* val = std::getenv(name.c_str());
            return val ? Str(val) : "";
        }
        inline void set(const Str& name, const Str& value) {
            #ifdef _WIN32
            _putenv_s(name.c_str(), value.c_str());
            #else
            setenv(name.c_str(), value.c_str(), 1);
            #endif
        }
        inline List<Str> args() {
            List<Str> argList;
            return argList;
        }
        inline void exit(Int code) {
            std::exit((int)code);
        }
    }

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
                memDC = CreateCompatibleDC(hdc);
                hbm = CreateCompatibleBitmap(hdc, width, height);
                SelectObject(memDC, hbm);
                running = true;
            }
        }

        inline Bool isOpen() {
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    running = false;
                }
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
            TextOut(memDC, x, y, text.c_str(), text.length());
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

    inline Int toInt(const Str& s) {
        try {
            return std::stoll(s);
        } catch (...) {
            return 0;
        }
    }
    inline Dec toDec(const Str& s) {
        try {
            return std::stod(s);
        } catch (...) {
            return 0.0;
        }
    }
    template<typename T>
    inline Str toStr(const T& val) {
        return std::to_string(val);
    }
    inline Str toStr(const Str& val) {
        return val;
    }

    namespace gl {
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
    }

    namespace vk {
        typedef void* (*PFN_vkVoidFunction)(void);
        typedef int (*PFN_vkCreateInstance)(const void*, const void*, void**);
        
        inline HMODULE vulkanLib = nullptr;
        inline PFN_vkCreateInstance createInstanceFunc = nullptr;

        inline Bool init() {
            if (vulkanLib) return true;
            vulkanLib = LoadLibrary("vulkan-1.dll");
            if (!vulkanLib) return false;
            
            createInstanceFunc = (PFN_vkCreateInstance)GetProcAddress(vulkanLib, "vkCreateInstance");
            return createInstanceFunc != nullptr;
        }

        inline Bool isSupported() {
            return init();
        }

        inline Str getStatus() {
            if (init()) {
                return "Vulkan driver (vulkan-1.dll) loaded successfully! GPU Vulkan support is active.";
            }
            return "Vulkan is not supported on this device (vulkan-1.dll not found).";
        }
    }

    namespace thread {
        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }

        template<typename F>
        inline void run(F&& func) {
            std::thread t(std::forward<F>(func));
            t.detach();
        }
    }

    namespace net {
        inline void initWinSock() {
            #ifdef _WIN32
            static bool initialized = false;
            if (!initialized) {
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2, 2), &wsaData);
                initialized = true;
            }
            #endif
        }

        class Socket {
        private:
            SOCKET sock = INVALID_SOCKET;
        public:
            Socket() {
                initWinSock();
                sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            }
            ~Socket() {
                close();
            }

            inline Bool connect(const Str& host, Int port) {
                struct sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port);
                
                #ifdef _WIN32
                addr.sin_addr.s_addr = inet_addr(host.c_str());
                if (addr.sin_addr.s_addr == INADDR_NONE) {
                #else
                if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
                #endif
                    struct hostent* he = gethostbyname(host.c_str());
                    if (he) {
                        addr.sin_addr = *(struct in_addr*)he->h_addr;
                    } else {
                        return false;
                    }
                }

                return ::connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0;
            }

            inline void send(const Str& data) {
                ::send(sock, data.c_str(), (int)data.length(), 0);
            }

            inline Str recv(Int bufferSize = 4096) {
                std::vector<char> buffer(bufferSize);
                int bytesReceived = ::recv(sock, buffer.data(), (int)(bufferSize - 1), 0);
                if (bytesReceived > 0) {
                    return Str(buffer.data(), bytesReceived);
                }
                return "";
            }

            inline void close() {
                if (sock != INVALID_SOCKET) {
                    closesocket(sock);
                    sock = INVALID_SOCKET;
                }
            }
        };
    }
}
