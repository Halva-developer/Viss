#pragma once
#include "../vissrt.hpp"
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstdlib>

namespace viss {
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

        inline List<Str> read_lines(const Str& path) {
            List<Str> res;
            std::ifstream f(path);
            if (!f.is_open()) return res;
            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                res.add(line);
            }
            return res;
        }

        inline List<Str> lines(const Str& path) {
            return read_lines(path);
        }

        inline void write_lines(const Str& path, const List<Str>& lines) {
            std::ofstream f(path);
            if (f.is_open()) {
                for (size_t i = 0; i < lines.size(); ++i) {
                    f << lines.get(i) << "\n";
                }
            }
        }

        inline Bool exists(const Str& path) {
            return std::filesystem::exists(path);
        }

        inline Bool is_file(const Str& path) {
            return std::filesystem::is_regular_file(path);
        }

        inline Bool is_dir(const Str& path) {
            return std::filesystem::is_directory(path);
        }

        inline void append(const Str& path, const Str& content) {
            std::ofstream f(path, std::ios::app);
            if (f.is_open()) {
                f << content;
            }
        }

        inline void remove(const Str& path) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }

        inline void remove_all(const Str& path) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        inline void mkdir(const Str& path) {
            std::error_code ec;
            std::filesystem::create_directory(path, ec);
        }

        inline void mkdir_p(const Str& path) {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
        }

        inline void copy(const Str& src, const Str& dst) {
            std::error_code ec;
            std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        }

        inline void move(const Str& src, const Str& dst) {
            std::error_code ec;
            std::filesystem::rename(src, dst, ec);
        }

        inline Int size(const Str& path) {
            std::error_code ec;
            auto sz = std::filesystem::file_size(path, ec);
            if (ec) return 0;
            return (Int)sz;
        }

        inline Int mtime(const Str& path) {
            std::error_code ec;
            auto t = std::filesystem::last_write_time(path, ec);
            if (ec) return 0;
            auto s = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
            return (Int)s;
        }

        inline Str extension(const Str& path) {
            return std::filesystem::path(path).extension().string();
        }

        inline Str filename(const Str& path) {
            return std::filesystem::path(path).filename().string();
        }

        inline Str stem(const Str& path) {
            return std::filesystem::path(path).stem().string();
        }

        inline Str parent(const Str& path) {
            return std::filesystem::path(path).parent_path().string();
        }

        inline Bytes read_bytes(const Str& path) {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            if (!f.is_open()) return Bytes(0);
            std::streamsize sz = f.tellg();
            f.seekg(0, std::ios::beg);
            Bytes b((size_t)sz);
            if (sz > 0) {
                f.read(reinterpret_cast<char*>(b.raw()), sz);
            }
            return b;
        }

        inline void write_bytes(const Str& path, const Bytes& b) {
            std::ofstream f(path, std::ios::binary);
            if (f.is_open() && b.size() > 0) {
                f.write(reinterpret_cast<const char*>(b.raw()), b.size());
            }
        }

        inline List<Str> list_dir(const Str& path) {
            List<Str> files;
            try {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    files.add(entry.path().string());
                }
            } catch (...) {}
            return files;
        }

        inline List<Str> list(const Str& path) {
            return list_dir(path);
        }

        inline List<Str> list_recursive(const Str& path) {
            List<Str> files;
            std::error_code ec;
            auto opts = std::filesystem::directory_options::skip_permission_denied;
            std::filesystem::recursive_directory_iterator it(path, opts, ec);
            std::filesystem::recursive_directory_iterator end;
            if (ec) return files;
            while (it != end) {
                try {
                    if (!ec) {
                        files.add(it->path().string());
                    }
                } catch (...) {}
                it.increment(ec);
                if (ec) {
                    ec.clear();
                }
            }
            return files;
        }

        inline Str app_path() {
#ifdef _WIN32
            wchar_t buf[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            return std::filesystem::path(buf).string();
#else
            return std::filesystem::current_path().string();
#endif
        }

        inline Str app_dir() {
#ifdef _WIN32
            wchar_t buf[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            return std::filesystem::path(buf).parent_path().string();
#else
            return std::filesystem::current_path().string();
#endif
        }

        inline Str cwd() {
            return std::filesystem::current_path().string();
        }

        inline Bool has_embedded_zip() {
#ifdef _WIN32
            wchar_t buf[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            std::ifstream file(buf, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return false;
            std::streamsize size = file.tellg();
            if (size < 22) return false;
            std::streamsize search_len = std::min<std::streamsize>(size, 65536 + 22);
            file.seekg(size - search_len);
            std::vector<char> buffer(search_len);
            file.read(buffer.data(), search_len);
            for (long long i = (long long)search_len - 22; i >= 0; --i) {
                if ((unsigned char)buffer[i] == 0x50 && (unsigned char)buffer[i+1] == 0x4B &&
                    (unsigned char)buffer[i+2] == 0x05 && (unsigned char)buffer[i+3] == 0x06) {
                    return true;
                }
            }
#endif
            return false;
        }

        inline Bool extract_embedded_zip(const Str& dest_dir) {
#ifdef _WIN32
            wchar_t buf[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, buf, MAX_PATH);
            std::filesystem::path exe_path(buf);
            std::ifstream file(exe_path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return false;
            std::streamsize size = file.tellg();
            if (size < 22) return false;

            std::streamsize search_len = std::min<std::streamsize>(size, 65536 + 22);
            file.seekg(size - search_len);
            std::vector<char> buffer(search_len);
            file.read(buffer.data(), search_len);

            long long eocd_pos = -1;
            for (long long i = (long long)search_len - 22; i >= 0; --i) {
                if ((unsigned char)buffer[i] == 0x50 && (unsigned char)buffer[i+1] == 0x4B &&
                    (unsigned char)buffer[i+2] == 0x05 && (unsigned char)buffer[i+3] == 0x06) {
                    eocd_pos = (size - search_len) + i;
                    break;
                }
            }
            if (eocd_pos < 0) return false;

            file.seekg(eocd_pos + 12);
            uint32_t cd_size = 0;
            uint32_t cd_offset = 0;
            file.read(reinterpret_cast<char*>(&cd_size), 4);
            file.read(reinterpret_cast<char*>(&cd_offset), 4);

            long long zip_start = eocd_pos - (long long)cd_size - (long long)cd_offset;
            if (zip_start < 0 || zip_start >= size) return false;

            std::filesystem::path temp_zip = std::filesystem::temp_directory_path() / "viss_installer_payload.zip";
            std::ofstream out(temp_zip, std::ios::binary);
            if (!out.is_open()) return false;

            file.seekg(zip_start);
            char chunk[65536];
            std::streamsize remaining = size - zip_start;
            while (remaining > 0) {
                std::streamsize to_read = std::min<std::streamsize>(remaining, sizeof(chunk));
                file.read(chunk, to_read);
                out.write(chunk, to_read);
                remaining -= to_read;
            }
            out.close();
            file.close();

            std::error_code ec;
            std::filesystem::create_directories(dest_dir, ec);

            std::string cmd = "tar -xf \"" + temp_zip.string() + "\" -C \"" + dest_dir + "\"";
            int res = std::system(cmd.c_str());
            if (res != 0) {
                std::string ps = "powershell -WindowStyle Hidden -Command \"Expand-Archive -LiteralPath '" + temp_zip.string() + "' -DestinationPath '" + dest_dir + "' -Force\"";
                res = std::system(ps.c_str());
            }

            std::filesystem::remove(temp_zip, ec);
            return (res == 0);
#else
            return false;
#endif
        }

        inline bool glob_match(const std::string& pattern, const std::string& str) {
            size_t p = 0, s = 0;
            size_t star_p = std::string::npos, star_s = 0;
            while (s < str.size()) {
                if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
                    p++;
                    s++;
                } else if (p < pattern.size() && pattern[p] == '*') {
                    star_p = p++;
                    star_s = s;
                } else if (star_p != std::string::npos) {
                    p = star_p + 1;
                    s = ++star_s;
                } else {
                    return false;
                }
            }
            while (p < pattern.size() && pattern[p] == '*') p++;
            return p == pattern.size();
        }

        inline List<Str> glob(const Str& dir, const Str& pattern, bool recursive = true) {
            List<Str> results;
            std::error_code ec;
            std::string norm_pattern = pattern;
            std::replace(norm_pattern.begin(), norm_pattern.end(), '\\', '/');

            bool is_recursive = recursive || (norm_pattern.find("**") != std::string::npos);
            std::string file_pat = norm_pattern;
            size_t last_slash = norm_pattern.rfind('/');
            if (last_slash != std::string::npos) {
                file_pat = norm_pattern.substr(last_slash + 1);
            }

            if (is_recursive) {
                auto opts = std::filesystem::directory_options::skip_permission_denied;
                std::filesystem::recursive_directory_iterator it(dir, opts, ec);
                std::filesystem::recursive_directory_iterator end;
                if (ec) return results;
                while (it != end) {
                    try {
                        if (!ec && it->is_regular_file()) {
                            std::string fname = it->path().filename().string();
                            if (glob_match(file_pat, fname)) {
                                results.add(it->path().string());
                            }
                        }
                    } catch (...) {}
                    it.increment(ec);
                    if (ec) ec.clear();
                }
            } else {
                std::filesystem::directory_iterator it(dir, ec);
                std::filesystem::directory_iterator end;
                if (ec) return results;
                while (it != end) {
                    try {
                        if (!ec && it->is_regular_file()) {
                            std::string fname = it->path().filename().string();
                            if (glob_match(file_pat, fname)) {
                                results.add(it->path().string());
                            }
                        }
                    } catch (...) {}
                    it.increment(ec);
                    if (ec) ec.clear();
                }
            }
            return results;
        }

        inline List<Str> glob(const Str& pattern) {
            std::string norm = pattern;
            std::replace(norm.begin(), norm.end(), '\\', '/');
            size_t last_slash = norm.rfind('/');
            if (last_slash != std::string::npos) {
                std::string dir = norm.substr(0, last_slash);
                std::string pat = norm.substr(last_slash + 1);
                return glob(dir, pat, norm.find("**") != std::string::npos);
            }
            return glob(".", norm, false);
        }
    }
}

