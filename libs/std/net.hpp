#pragma once
#include "../vissrt.hpp"

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define closesocket close
#endif

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
