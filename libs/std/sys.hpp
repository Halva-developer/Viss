#pragma once
#include "../vissrt.hpp"
#include <chrono>
#include <cstdlib>
#include <thread>
#include <sstream>
#include <array>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#endif

namespace viss {
    namespace sys {
        inline void seed() {
            srand((unsigned int)std::chrono::system_clock::now().time_since_epoch().count());
        }
        inline Int random(Int min_v, Int max_v) {
            if (max_v <= min_v) return min_v;
            return min_v + (rand() % (max_v - min_v));
        }

        static int g_last_exit_code = 0;

        inline Int last_exit_code() {
            return (Int)g_last_exit_code;
        }

#ifdef _WIN32
        inline Int system(const Str& cmd) {
            std::string c = "cmd.exe /s /c \"" + cmd + "\"";
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), NULL, 0);
            std::wstring wcmd(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), &wcmd[0], size_needed);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = { 0 };

            // CREATE_NO_WINDOW = 0x08000000
            BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi);
            if (!ok) {
                g_last_exit_code = -1;
                return -1;
            }
            while (true) {
                DWORD wait_res = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 50, QS_ALLINPUT);
                if (wait_res == WAIT_OBJECT_0) {
                    break;
                } else if (wait_res == WAIT_OBJECT_0 + 1) {
                    MSG msg;
                    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessageW(&msg);
                    }
                } else if (wait_res == WAIT_TIMEOUT) {
                    // continue waiting
                } else {
                    break;
                }
            }
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            g_last_exit_code = (int)exit_code;
            return (Int)exit_code;
        }
        inline void command(const Str& cmd) {
            system(cmd);
        }
        inline Str exec(const Str& cmd) {
            HANDLE hRead, hWrite;
            SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
            if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
                g_last_exit_code = -1;
                return "";
            }
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

            std::string c = "cmd.exe /s /c \"" + cmd + "\"";
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), NULL, 0);
            std::wstring wcmd(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), &wcmd[0], size_needed);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
            si.wShowWindow = SW_HIDE;
            si.hStdOutput = hWrite;
            si.hStdError = hWrite;
            PROCESS_INFORMATION pi = { 0 };

            BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE, 0x08000000, NULL, NULL, &si, &pi);
            CloseHandle(hWrite);
            if (!ok) {
                CloseHandle(hRead);
                g_last_exit_code = -1;
                return "";
            }

            std::string result;
            char buffer[4096];
            DWORD bytesRead = 0;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }
            CloseHandle(hRead);
            while (true) {
                DWORD wait_res = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 50, QS_ALLINPUT);
                if (wait_res == WAIT_OBJECT_0) {
                    break;
                } else if (wait_res == WAIT_OBJECT_0 + 1) {
                    MSG msg;
                    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessageW(&msg);
                    }
                } else if (wait_res == WAIT_TIMEOUT) {
                    // continue waiting
                } else {
                    break;
                }
            }
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            g_last_exit_code = (int)exit_code;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return Str(result);
        }
        inline void spawn(const Str& cmd) {
            std::string c = "cmd.exe /s /c \"" + cmd + "\"";
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), NULL, 0);
            std::wstring wcmd(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), &wcmd[0], size_needed);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = { 0 };

            // CREATE_NO_WINDOW = 0x08000000
            if (CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
        inline void open_url(const Str& url) {
            std::string u = url;
            ShellExecuteA(NULL, "open", u.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
#else
        inline void command(const Str& cmd) {
            std::system(cmd.c_str());
        }
        inline Int system(const Str& cmd) {
            return (Int)std::system(cmd.c_str());
        }
        inline Str exec(const Str& cmd) {
            std::array<char, 128> buffer;
            Str result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe) return "";
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            return result;
        }
        inline void spawn(const Str& cmd) {
            std::string c = cmd + " &";
            std::system(c.c_str());
        }
        inline void open_url(const Str& url) {
            #ifdef __APPLE__
            std::string c = "open \"" + url + "\" &";
            #else
            std::string c = "xdg-open \"" + url + "\" &";
            #endif
            std::system(c.c_str());
        }
#endif
        inline Str env(const Str& key) {
            const char* val = std::getenv(key.c_str());
            return val ? Str(val) : "";
        }
        inline void exit(Int code = 0) {
            std::exit((int)code);
        }
        inline Str os() {
            #ifdef _WIN32
            return "windows";
            #elif __APPLE__
            return "darwin";
            #else
            return "linux";
            #endif
        }
        inline Int cpu_count() {
            unsigned int c = std::thread::hardware_concurrency();
            return c > 0 ? (Int)c : 1;
        }
        inline void sleep(Int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }

#ifdef _WIN32
        inline Bool is_process_running(const Str& name) {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap == INVALID_HANDLE_VALUE) return false;
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            int sz = MultiByteToWideChar(CP_UTF8, 0, name.data(), (int)name.size(), NULL, 0);
            std::wstring wname(sz, 0);
            MultiByteToWideChar(CP_UTF8, 0, name.data(), (int)name.size(), &wname[0], sz);
            bool found = false;
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, wname.c_str()) == 0) {
                        found = true;
                        break;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
            return found;
        }

        inline void notify(const Str& title, const Str& msg) {
            std::string t = title;
            std::string m = msg;
            for (char& c : t) if (c == '\'' || c == '"') c = ' ';
            for (char& c : m) if (c == '\'' || c == '"') c = ' ';
            std::string ps = "powershell -WindowStyle Hidden -Command \"[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null; $template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02); $audio = $template.CreateElement('audio'); $audio.SetAttribute('silent', 'true'); $template.DocumentElement.AppendChild($audio) > $null; $textNodes = $template.GetElementsByTagName('text'); $textNodes.Item(0).AppendChild($template.CreateTextNode('" + t + "')) > $null; $textNodes.Item(1).AppendChild($template.CreateTextNode('" + m + "')) > $null; $toast = [Windows.UI.Notifications.ToastNotification]::new($template); [Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('AudioCoverWatcher').Show($toast)\"";
            
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, ps.data(), (int)ps.size(), NULL, 0);
            std::wstring wcmd(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, ps.data(), (int)ps.size(), &wcmd[0], size_needed);
            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = { 0 };
            if (CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
#else
        inline Bool is_process_running(const Str& name) { return false; }
        inline void notify(const Str& title, const Str& msg) {}
#endif
    }
}
