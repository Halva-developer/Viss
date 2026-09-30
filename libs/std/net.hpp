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

        static int g_last_status_code = 0;

        inline Int status_code() {
            return (Int)g_last_status_code;
        }

        inline Str request(const Str& method, const Str& url, const Str& body = "", const Str& headers = "") {
#ifdef _WIN32
            g_last_status_code = 0;
            URL_COMPONENTSA uc = { sizeof(uc) };
            char host[512] = {0};
            char path[4096] = {0};
            uc.lpszHostName = host;
            uc.dwHostNameLength = sizeof(host);
            uc.lpszUrlPath = path;
            uc.dwUrlPathLength = sizeof(path);
            if (!InternetCrackUrlA(url.c_str(), (DWORD)url.size(), 0, &uc)) {
                return "";
            }

            HINTERNET hInternet = InternetOpenA("Viss-Net/2.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (!hInternet) return "";

            HINTERNET hConnect = InternetConnectA(hInternet, host, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
            if (!hConnect) {
                InternetCloseHandle(hInternet);
                return "";
            }

            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
            if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
                flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
            }

            const char* path_str = (path[0] != '\0') ? path : "/";
            HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path_str, NULL, NULL, NULL, flags, 0);
            if (!hRequest) {
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return "";
            }

            DWORD timeout_ms = 15000;
            InternetSetOptionA(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
            InternetSetOptionA(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

            BOOL sent = FALSE;
            if (body.empty()) {
                sent = HttpSendRequestA(hRequest, headers.empty() ? NULL : headers.c_str(), (DWORD)headers.size(), NULL, 0);
            } else {
                sent = HttpSendRequestA(hRequest, headers.empty() ? NULL : headers.c_str(), (DWORD)headers.size(), (LPVOID)body.data(), (DWORD)body.size());
            }

            if (!sent) {
                InternetCloseHandle(hRequest);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return "";
            }

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            if (HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, NULL)) {
                g_last_status_code = (int)statusCode;
            }

            Str response;
            char buffer[8192];
            DWORD bytesRead = 0;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                response.append(buffer, bytesRead);
            }

            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return response;
#else
            return "";
#endif
        }

        inline Str get(const Str& url, const Str& headers = "") {
            return request("GET", url, "", headers);
        }

        inline Str post(const Str& url, const Str& body = "", const Str& headers = "") {
            Str hdr = headers;
            if (hdr.empty() && !body.empty()) {
                hdr = "Content-Type: application/x-www-form-urlencoded\r\n";
            }
            return request("POST", url, body, hdr);
        }

        inline Str post_json(const Str& url, const Str& json_data, const Str& auth_token = "") {
            Str hdr = "Content-Type: application/json\r\n";
            if (!auth_token.empty()) {
                hdr += "Authorization: Bearer " + auth_token + "\r\n";
            }
            return request("POST", url, json_data, hdr);
        }

        inline Bool download(const Str& url, const Str& destinationPath) {
#ifdef _WIN32
            URL_COMPONENTSA uc = { sizeof(uc) };
            char host[512] = {0};
            char path[4096] = {0};
            uc.lpszHostName = host;
            uc.dwHostNameLength = sizeof(host);
            uc.lpszUrlPath = path;
            uc.dwUrlPathLength = sizeof(path);
            if (!InternetCrackUrlA(url.c_str(), (DWORD)url.size(), 0, &uc)) return false;

            HINTERNET hInternet = InternetOpenA("Viss-Net/2.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
            if (!hInternet) return false;
            HINTERNET hConnect = InternetConnectA(hInternet, host, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
            if (!hConnect) { InternetCloseHandle(hInternet); return false; }

            DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
            if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
                flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
            }

            const char* path_str = (path[0] != '\0') ? path : "/";
            HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path_str, NULL, NULL, NULL, flags, 0);
            if (!hRequest) {
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return false;
            }

            if (!HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
                InternetCloseHandle(hRequest);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return false;
            }

            std::ofstream out(destinationPath, std::ios::binary);
            if (!out.is_open()) {
                InternetCloseHandle(hRequest);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return false;
            }

            char buffer[16384];
            DWORD bytesRead = 0;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                out.write(buffer, bytesRead);
            }
            out.close();

            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return true;
#else
            return false;
#endif
        }

        inline Str http_get(const Str& url) {
            return get(url);
        }

        inline Str http_post(const Str& url, const Str& body) {
            return post(url, body);
        }

        inline Bool download_file(const Str& url, const Str& destinationPath) {
            return download(url, destinationPath);
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
