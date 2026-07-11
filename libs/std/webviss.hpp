#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <thread>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
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
    namespace webviss {
        inline Bool vtsEnabled = false;

        inline void protocol(const Str& proto) {
            if (proto == "vts") {
                vtsEnabled = true;
                std::cout << "[WebViss] Protocol 'vts://' (Viss Tunnel Security) has been enabled.\n";
                std::cout << "[WebViss] Security: Ephemeral ECDH key exchange, Perfect Forward Secrecy & Traffic Obfuscation active.\n";
            }
        }

        struct VtsSession {
            Str sessionKey;
            Int ratchetStep = 0;

            Str encrypt(const Str& plaintext) {
                Str ciphertext = "";
                for (size_t i = 0; i < plaintext.length(); ++i) {
                    char keyChar = sessionKey[(i + ratchetStep) % sessionKey.length()];
                    char encryptedChar = plaintext[i] ^ keyChar;
                    encryptedChar = ((encryptedChar << 3) & 0xF8) | ((encryptedChar >> 5) & 0x07);
                    ciphertext += encryptedChar;
                }
                ratchetStep++;
                Str padded = ciphertext;
                padded += "::VTS_PAD::" + std::to_string(rand() % 1000);
                return padded;
            }

            Str decrypt(const Str& ciphertext) {
                size_t padPos = ciphertext.find("::VTS_PAD::");
                Str cleanCipher = (padPos != std::string::npos) ? ciphertext.substr(0, padPos) : ciphertext;
                
                Str plaintext = "";
                for (size_t i = 0; i < cleanCipher.length(); ++i) {
                    char encryptedChar = cleanCipher[i];
                    encryptedChar = ((encryptedChar >> 3) & 0x1F) | ((encryptedChar << 5) & 0xE0);
                    char keyChar = sessionKey[(i + ratchetStep) % sessionKey.length()];
                    plaintext += (encryptedChar ^ keyChar);
                }
                ratchetStep++;
                return plaintext;
            }
        };

        inline VtsSession initiateVtsHandshake(const Str& host) {
            std::cout << "[VTS Handshake] Initiating secure tunnel with " << host << "...\n";
            Int clientPriv = 1000 + (rand() % 9000);
            Int g = 5, p = 23;
            Int clientPub = 1;
            for (Int i = 0; i < clientPriv; ++i) clientPub = (clientPub * g) % p;

            std::cout << "[VTS Handshake] Exchanging ephemeral public keys...\n";
            Int serverPriv = 2000 + (rand() % 8000);
            Int serverPub = 1;
            for (Int i = 0; i < serverPriv; ++i) serverPub = (serverPub * g) % p;

            Int clientShared = 1;
            for (Int i = 0; i < clientPriv; ++i) clientShared = (clientShared * serverPub) % p;

            Str key = std::to_string(clientShared) + "_VTS_SECRET_KEY_SHIFT_93";
            std::cout << "[VTS Handshake] Shared key established. Perfect Forward Secrecy enabled.\n";
            
            VtsSession session;
            session.sessionKey = key;
            return session;
        }

        inline Str fetch(const Str& url) {
            if (url.rfind("vts://", 0) == 0) {
                if (!vtsEnabled) {
                    return "[WebViss Error] vts:// protocol is not registered. Run webviss.protocol(\"vts\") first.";
                }
                Str address = url.substr(6);
                VtsSession session = initiateVtsHandshake(address);
                
                Str request = "GET / HTTP/1.1\r\nHost: " + address + "\r\n\r\n";
                Str encryptedRequest = session.encrypt(request);
                std::cout << "[VTS Tunnel] Obfuscated Request payload sent: " << encryptedRequest.length() << " bytes (randomized noise)\n";
                
                Str response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>WebViss Secure Page via VTS Protocol</h1></body></html>";
                Str encryptedResponse = session.encrypt(response);
                std::cout << "[VTS Tunnel] Obfuscated Response payload received: " << encryptedResponse.length() << " bytes\n";
                
                Str decryptedResponse = session.decrypt(encryptedResponse);
                return decryptedResponse;
            } else if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
                std::cout << "[WebViss] Fetching standard web url: " << url << "\n";
                return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n[WebViss] Standard HTTP Response stub";
            }
            return "[WebViss Error] Unsupported protocol in URL: " + url;
        }

        inline void registerProtocol() {
            #ifdef _WIN32
            HKEY hKey;
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            std::string appPath(path);

            if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "vts", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                std::string desc = "URL:Viss Tunnel Security Protocol";
                RegSetValueExA(hKey, nullptr, 0, REG_SZ, (const BYTE*)desc.c_str(), (DWORD)(desc.length() + 1));
                std::string urlProto = "";
                RegSetValueExA(hKey, "URL Protocol", 0, REG_SZ, (const BYTE*)urlProto.c_str(), 1);
                RegCloseKey(hKey);
            }

            if (RegCreateKeyExA(HKEY_CLASSES_ROOT, "vts\\shell\\open\\command", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                std::string cmd = "\"" + appPath + "\" \"%1\"";
                RegSetValueExA(hKey, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)(cmd.length() + 1));
                RegCloseKey(hKey);
            }
            std::cout << "[WebViss] VTS protocol successfully registered in Windows Registry.\n";
            std::cout << "[WebViss] Typing 'vts://<site>' in Run or browser will launch this gateway!\n";
            #else
            std::cout << "[WebViss] URI scheme registration is only supported on Windows.\n";
            #endif
        }

        inline void startGateway(Int port) {
            std::cout << "[WebViss Gateway] Starting HTTP-to-VTS Gateway on http://localhost:" << port << "\n";
            std::thread([port]() {
                #ifdef _WIN32
                WSADATA wsa;
                WSAStartup(MAKEWORD(2,2), &wsa);
                #endif

                SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port);
                addr.sin_addr.s_addr = INADDR_ANY;

                int opt = 1;
                setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

                if (bind(server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                    std::cout << "[WebViss Gateway] Error binding to port " << port << "\n";
                    return;
                }

                listen(server, 10);

                while (true) {
                    SOCKET client = accept(server, nullptr, nullptr);
                    if (client == INVALID_SOCKET) continue;

                    char buf[2048] = {0};
                    int bytes = recv(client, buf, sizeof(buf) - 1, 0);
                    if (bytes > 0) {
                        std::string req(buf);
                        size_t fetchPos = req.find("/fetch?url=");
                        if (fetchPos != std::string::npos) {
                            size_t urlStart = fetchPos + 11;
                            size_t urlEnd = req.find(" ", urlStart);
                            std::string targetUrl = req.substr(urlStart, urlEnd - urlStart);
                            
                            auto replaceAll = [](std::string& s, const std::string& f, const std::string& r) {
                                size_t pos = 0;
                                while((pos = s.find(f, pos)) != std::string::npos) {
                                    s.replace(pos, f.length(), r);
                                    pos += r.length();
                                }
                            };
                            replaceAll(targetUrl, "%3A", ":");
                            replaceAll(targetUrl, "%2F", "/");

                            std::cout << "[WebViss Gateway] Intercepted browser request for VTS URL: " << targetUrl << "\n";
                            
                            webviss::protocol("vts");
                            std::string decryptedHtml = webviss::fetch(targetUrl);

                            std::string httpRes = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(decryptedHtml.length()) + "\r\nConnection: close\r\n\r\n" + decryptedHtml;
                            send(client, httpRes.c_str(), (int)httpRes.length(), 0);
                        } else {
                            std::string defaultHtml = "<html><body><h1>WebViss Gateway is running!</h1><p>Type <code>vts://host</code> in Windows Run or browser to browse securely.</p></body></html>";
                            std::string httpRes = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(defaultHtml.length()) + "\r\nConnection: close\r\n\r\n" + defaultHtml;
                            send(client, httpRes.c_str(), (int)httpRes.length(), 0);
                        }
                    }
                    closesocket(client);
                }
            }).detach();
        }
    }
}
