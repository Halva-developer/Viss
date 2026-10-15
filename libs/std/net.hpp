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
#define closesocket ::close
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
                flags |= INTERNET_FLAG_SECURE;
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
            g_last_status_code = 0;
            std::string s_url = url;
            std::string proto = "http";
            std::string host = "";
            int port = 80;
            std::string path = "/";

            size_t p_pos = s_url.find("://");
            size_t rest_start = 0;
            if (p_pos != std::string::npos) {
                proto = s_url.substr(0, p_pos);
                rest_start = p_pos + 3;
            }
            size_t slash_pos = s_url.find('/', rest_start);
            std::string host_port = "";
            if (slash_pos != std::string::npos) {
                host_port = s_url.substr(rest_start, slash_pos - rest_start);
                path = s_url.substr(slash_pos);
            } else {
                host_port = s_url.substr(rest_start);
            }
            size_t colon_pos = host_port.find(':');
            if (colon_pos != std::string::npos) {
                host = host_port.substr(0, colon_pos);
                port = std::atoi(host_port.substr(colon_pos + 1).c_str());
            } else {
                host = host_port;
                port = (proto == "https") ? 443 : 80;
            }

            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            std::string port_str = std::to_string(port);
            if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
                return "";
            }
            int s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (s < 0) { freeaddrinfo(res); return ""; }

            struct timeval tv = { 5, 0 };
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            if (::connect(s, res->ai_addr, res->ai_addrlen) != 0) {
                ::close(s); freeaddrinfo(res); return "";
            }
            freeaddrinfo(res);

            std::ostringstream req;
            req << method << " " << path << " HTTP/1.1\r\n"
                << "Host: " << host << "\r\n"
                << "User-Agent: Viss-Net/2.0\r\n"
                << "Connection: close\r\n";
            if (!body.empty()) {
                req << "Content-Length: " << body.size() << "\r\n";
            }
            if (!headers.empty()) {
                req << headers;
                if (headers.find("\r\n") == std::string::npos) req << "\r\n";
            }
            req << "\r\n";
            if (!body.empty()) req << body;

            std::string req_str = req.str();
            ::send(s, req_str.data(), req_str.size(), 0);

            std::string resp;
            char buf[4096];
            ssize_t n;
            while ((n = ::recv(s, buf, sizeof(buf), 0)) > 0) {
                resp.append(buf, n);
            }
            ::close(s);

            auto header_end = resp.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                auto status_line_end = resp.find("\r\n");
                if (status_line_end != std::string::npos) {
                    std::string status_line = resp.substr(0, status_line_end);
                    auto p1 = status_line.find(' ');
                    if (p1 != std::string::npos) {
                        g_last_status_code = std::atoi(status_line.c_str() + p1 + 1);
                    }
                }
                return resp.substr(header_end + 4);
            }
            return resp;
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
                flags |= INTERNET_FLAG_SECURE;
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
            std::string cmd = "curl -s -L -A \"Viss-Net/2.0\" -o \"" + destinationPath + "\" \"" + url + "\"";
            int ret = std::system(cmd.c_str());
            return (ret == 0 && std::filesystem::exists(destinationPath) && std::filesystem::file_size(destinationPath) > 0);
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

        // --- HTTP Request & Response models ---
        struct HttpRequest {
            Str method = "GET";
            Str path = "/";
            Str body = "";
            Map<Str, Str> query;
            Map<Str, Str> headers;
            Str client_ip = "127.0.0.1";
        };

        struct HttpResponse {
            Int status = 200;
            Str content_type = "text/html; charset=utf-8";
            Str body = "";
            Map<Str, Str> headers;

            void send(const Str& text) {
                body = text;
            }

            void json(const Str& json_data) {
                content_type = "application/json; charset=utf-8";
                body = json_data;
            }

            void status_code(Int code) {
                status = code;
            }

            void set_header(const Str& k, const Str& v) {
                Str clean_k = k;
                Str clean_v = v;
                clean_k.erase(std::remove(clean_k.begin(), clean_k.end(), '\r'), clean_k.end());
                clean_k.erase(std::remove(clean_k.begin(), clean_k.end(), '\n'), clean_k.end());
                clean_v.erase(std::remove(clean_v.begin(), clean_v.end(), '\r'), clean_v.end());
                clean_v.erase(std::remove(clean_v.begin(), clean_v.end(), '\n'), clean_v.end());
                headers.set(clean_k, clean_v);
            }
        };

        using HttpHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

        // --- Embedded Multi-threaded HTTP Server Implementation ---
        class HttpServerInternal {
        private:
            SOCKET server_sock = INVALID_SOCKET;
            std::atomic<bool> is_running{false};
            std::thread accept_thread;
            std::map<Str, HttpHandler> routes_get;
            std::map<Str, HttpHandler> routes_post;
            std::map<Str, HttpHandler> routes_any;
            HttpHandler not_found_handler = nullptr;
            Int server_port = 8080;

            static Str status_message(Int code) {
                switch (code) {
                    case 200: return "OK";
                    case 201: return "Created";
                    case 204: return "No Content";
                    case 301: return "Moved Permanently";
                    case 302: return "Found";
                    case 400: return "Bad Request";
                    case 401: return "Unauthorized";
                    case 403: return "Forbidden";
                    case 404: return "Not Found";
                    case 500: return "Internal Server Error";
                    default: return "OK";
                }
            }

            void handle_client(SOCKET client_fd, Str client_ip) {
#ifdef _WIN32
                DWORD tv = 10000;
                setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
                setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
                struct timeval tv;
                tv.tv_sec = 10;
                tv.tv_usec = 0;
                setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
                setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const void*)&tv, sizeof(tv));
#endif
                std::vector<char> buffer(8192, 0);
                int bytes = ::recv(client_fd, buffer.data(), (int)(buffer.size() - 1), 0);
                if (bytes <= 0) {
                    closesocket(client_fd);
                    return;
                }

                Str raw(buffer.data(), bytes);
                std::istringstream req_stream(raw);
                Str req_line;
                std::getline(req_stream, req_line);
                if (!req_line.empty() && req_line.back() == '\r') req_line.pop_back();

                HttpRequest req;
                req.client_ip = client_ip;

                std::istringstream line_parts(req_line);
                line_parts >> req.method >> req.path;

                // Parse query params if any
                size_t q_pos = req.path.find('?');
                if (q_pos != Str::npos) {
                    Str qs = req.path.substr(q_pos + 1);
                    req.path = req.path.substr(0, q_pos);
                    std::istringstream q_stream(qs);
                    Str pair;
                    while (std::getline(q_stream, pair, '&')) {
                        size_t eq = pair.find('=');
                        if (eq != Str::npos) {
                            req.query.set(url_decode(pair.substr(0, eq)), url_decode(pair.substr(eq + 1)));
                        } else {
                            req.query.set(url_decode(pair), "");
                        }
                    }
                }

                // Headers
                Str header_line;
                size_t content_length = 0;
                while (std::getline(req_stream, header_line) && header_line != "\r" && !header_line.empty()) {
                    if (header_line.back() == '\r') header_line.pop_back();
                    size_t colon = header_line.find(':');
                    if (colon != Str::npos) {
                        Str hk = str::lower(str::trim(header_line.substr(0, colon)));
                        Str hv = str::trim(header_line.substr(colon + 1));
                        req.headers.set(hk, hv);
                        if (hk == "content-length") {
                            try { content_length = std::stoull(hv); } catch (...) {}
                        }
                    }
                }

                // Body
                size_t header_end = raw.find("\r\n\r\n");
                if (header_end != Str::npos) {
                    req.body = raw.substr(header_end + 4);
                }
                while (content_length > 0 && req.body.size() < content_length) {
                    size_t needed = content_length - req.body.size();
                    size_t chunk_size = std::min(needed, (size_t)8192);
                    int b = ::recv(client_fd, buffer.data(), (int)chunk_size, 0);
                    if (b <= 0) break;
                    req.body.append(buffer.data(), b);
                }

                HttpResponse res;
                HttpHandler matched = nullptr;

                if (req.method == "GET" && routes_get.count(req.path)) {
                    matched = routes_get[req.path];
                } else if (req.method == "POST" && routes_post.count(req.path)) {
                    matched = routes_post[req.path];
                } else if (routes_any.count(req.path)) {
                    matched = routes_any[req.path];
                }

                if (matched) {
                    try {
                        matched(req, res);
                    } catch (const std::exception& e) {
                        res.status = 500;
                        res.body = "500 Internal Server Error: " + Str(e.what());
                    } catch (...) {
                        res.status = 500;
                        res.body = "500 Internal Server Error";
                    }
                } else if (not_found_handler) {
                    try {
                        not_found_handler(req, res);
                    } catch (const std::exception& e) {
                        res.status = 500;
                        res.body = "500 Internal Server Error: " + Str(e.what());
                    } catch (...) {
                        res.status = 500;
                        res.body = "500 Internal Server Error";
                    }
                } else {
                    res.status = 404;
                    res.body = "404 Not Found: " + req.path;
                }

                // Build HTTP response
                std::ostringstream resp_stream;
                resp_stream << "HTTP/1.1 " << res.status << " " << status_message(res.status) << "\r\n";
                resp_stream << "Content-Type: " << res.content_type << "\r\n";
                resp_stream << "Content-Length: " << res.body.size() << "\r\n";
                resp_stream << "Connection: close\r\n";
                resp_stream << "Server: Viss-HTTP/0.3\r\n";

                auto hkeys = res.headers.keys();
                for (size_t i = 0; i < (size_t)hkeys.len(); ++i) {
                    resp_stream << hkeys[i] << ": " << res.headers[hkeys[i]] << "\r\n";
                }
                resp_stream << "\r\n";
                resp_stream << res.body;

                Str resp_bytes = resp_stream.str();
                ::send(client_fd, resp_bytes.data(), (int)resp_bytes.size(), 0);
                closesocket(client_fd);
            }

        public:
            HttpServerInternal() {
                initWinSock();
            }

            ~HttpServerInternal() {
                stop();
            }

            void on(const Str& method, const Str& path, HttpHandler handler) {
                if (method == "GET") routes_get[path] = handler;
                else if (method == "POST") routes_post[path] = handler;
                else routes_any[path] = handler;
            }

            void get(const Str& path, HttpHandler handler) {
                routes_get[path] = handler;
            }

            void post(const Str& path, HttpHandler handler) {
                routes_post[path] = handler;
            }

            void not_found(HttpHandler handler) {
                not_found_handler = handler;
            }

            Bool listen(Int port, Bool background = false) {
                server_port = port;
                server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (server_sock == INVALID_SOCKET) return false;

                int opt = 1;
                setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

                struct sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_port = htons((u_short)port);
                addr.sin_addr.s_addr = INADDR_ANY;

                if (::bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
                    closesocket(server_sock);
                    server_sock = INVALID_SOCKET;
                    return false;
                }

                if (::listen(server_sock, SOMAXCONN) != 0) {
                    closesocket(server_sock);
                    server_sock = INVALID_SOCKET;
                    return false;
                }

                is_running = true;

                auto accept_loop = [this]() {
                    while (is_running) {
                        struct sockaddr_in client_addr = {};
                        #ifdef _WIN32
                        int c_len = sizeof(client_addr);
                        #else
                        socklen_t c_len = sizeof(client_addr);
                        #endif
                        SOCKET client_fd = ::accept(server_sock, (struct sockaddr*)&client_addr, &c_len);
                        if (client_fd == INVALID_SOCKET) {
                            if (!is_running) break;
                            continue;
                        }

                        char ip_buf[64] = "127.0.0.1";
                        #ifdef _WIN32
                        char* cip = inet_ntoa(client_addr.sin_addr);
                        if (cip) strncpy_s(ip_buf, cip, sizeof(ip_buf));
                        #else
                        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
                        #endif

                        std::thread worker(&HttpServerInternal::handle_client, this, client_fd, Str(ip_buf));
                        worker.detach();
                    }
                };

                if (background) {
                    accept_thread = std::thread(accept_loop);
                    return true;
                } else {
                    accept_loop();
                    return true;
                }
            }

            void stop() {
                if (is_running) {
                    is_running = false;
                    #ifndef _WIN32
                    if (server_port > 0) {
                        int dummy = ::socket(AF_INET, SOCK_STREAM, 0);
                        if (dummy >= 0) {
                            struct sockaddr_in daddr = {};
                            daddr.sin_family = AF_INET;
                            daddr.sin_port = htons((u_short)server_port);
                            inet_pton(AF_INET, "127.0.0.1", &daddr.sin_addr);
                            ::connect(dummy, (struct sockaddr*)&daddr, sizeof(daddr));
                            ::close(dummy);
                        }
                    }
                    #endif
                    if (server_sock != INVALID_SOCKET) {
                        closesocket(server_sock);
                        server_sock = INVALID_SOCKET;
                    }
                    if (accept_thread.joinable()) {
                        accept_thread.join();
                    }
                }
            }

            Bool running() const {
                return is_running.load();
            }
        };

        // Value-type copyable HttpServer handle
        class HttpServer {
        private:
            std::shared_ptr<HttpServerInternal> impl;
        public:
            HttpServer() : impl(std::make_shared<HttpServerInternal>()) {}

            void on(const Str& method, const Str& path, HttpHandler handler) { if (impl) impl->on(method, path, handler); }
            void get(const Str& path, HttpHandler handler) { if (impl) impl->get(path, handler); }
            void post(const Str& path, HttpHandler handler) { if (impl) impl->post(path, handler); }
            void not_found(HttpHandler handler) { if (impl) impl->not_found(handler); }
            Bool listen(Int port, Bool background = false) { return impl ? impl->listen(port, background) : false; }
            void stop() { if (impl) impl->stop(); }
            Bool running() const { return impl ? impl->running() : false; }
        };

        // Convenience factory
        inline HttpServer create_server() {
            return HttpServer();
        }

        // Quick 1-liner server
        inline Bool server_listen(Int port, std::function<Str(const Str& path, const Str& method)> handler) {
            HttpServer srv;
            srv.not_found([handler](const HttpRequest& req, HttpResponse& res) {
                res.send(handler(req.path, req.method));
            });
            return srv.listen(port, false);
        }

    } // namespace net
} // namespace viss
