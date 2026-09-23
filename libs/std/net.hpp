#pragma once
#include "../vissrt.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

#include <sstream>
#include <iomanip>

namespace viss {
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

        // --- HTTP Client ---
        inline Str http_get(const Str& url) {
            #ifdef _WIN32
            HINTERNET hInternet = InternetOpenA("Viss-Net/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (!hInternet) return "";
            HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
            if (!hUrl) {
                InternetCloseHandle(hInternet);
                return "";
            }
            Str response;
            char buffer[4096];
            DWORD bytesRead = 0;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                response.append(buffer, bytesRead);
            }
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return response;
            #else
            return "";
            #endif
        }

        inline Bool download_file(const Str& url, const Str& destinationPath) {
            #ifdef _WIN32
            HINTERNET hInternet = InternetOpenA("Viss-Net/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (!hInternet) return false;
            HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
            if (!hUrl) {
                InternetCloseHandle(hInternet);
                return false;
            }
            std::ofstream out(destinationPath, std::ios::binary);
            if (!out.is_open()) {
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                return false;
            }
            char buffer[8192];
            DWORD bytesRead = 0;
            while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                out.write(buffer, bytesRead);
            }
            out.close();
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return true;
            #else
            return false;
            #endif
        }

        inline Str url_encode(const Str& value) {
            std::ostringstream escaped;
            escaped.fill('0');
            escaped << std::hex;
            for (char c : value) {
                if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << c;
                } else {
                    escaped << std::uppercase;
                    escaped << '%' << std::setw(2) << int((unsigned char)c);
                    escaped << std::nouppercase;
                }
            }
            return escaped.str();
        }

        inline Str url_decode(const Str& value) {
            std::string ret;
            for (size_t i = 0; i < value.length(); ++i) {
                if (value[i] == '%') {
                    if (i + 2 < value.length()) {
                        int code = 0;
                        std::istringstream hex_chars(value.substr(i + 1, 2));
                        if (hex_chars >> std::hex >> code) {
                            ret.push_back((char)code);
                            i += 2;
                        } else {
                            ret.push_back(value[i]);
                        }
                    } else {
                        ret.push_back(value[i]);
                    }
                } else if (value[i] == '+') {
                    ret.push_back(' ');
                } else {
                    ret.push_back(value[i]);
                }
            }
            return ret;
        }

        // --- Low-level TCP Socket ---
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
                addr.sin_port = htons((u_short)port);
                
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

        using TcpClient = Socket;
    }
}
