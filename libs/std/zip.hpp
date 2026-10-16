#pragma once
#include "../vissrt.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

namespace viss {
    namespace zip {
        namespace fs = std::filesystem;

        // Native ZIP Central Directory parser to list filenames with 0 external processes
        inline List<Str> list(const Str& zip_path) {
            List<Str> results;
            std::string p = zip_path.c_str();
            std::ifstream file(p, std::ios::binary);
            if (!file.is_open()) return results;

            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            if (file_size < 22) return results;

            // Search backwards for End of Central Directory Record (signature 0x06054b50)
            size_t search_len = std::min(file_size, (size_t)65557);
            std::vector<char> buf(search_len);
            file.seekg(file_size - search_len, std::ios::beg);
            file.read(buf.data(), search_len);

            size_t eocd_pos = std::string::npos;
            for (int i = (int)search_len - 22; i >= 0; --i) {
                if ((uint8_t)buf[i] == 0x50 && (uint8_t)buf[i+1] == 0x4b &&
                    (uint8_t)buf[i+2] == 0x05 && (uint8_t)buf[i+3] == 0x06) {
                    eocd_pos = (file_size - search_len) + i;
                    break;
                }
            }

            if (eocd_pos != std::string::npos) {
                file.seekg(eocd_pos + 12, std::ios::beg);
                uint32_t cd_size = 0;
                uint32_t cd_offset = 0;
                file.read((char*)&cd_size, 4);
                file.read((char*)&cd_offset, 4);

                file.seekg(cd_offset, std::ios::beg);
                while (file.tellg() < (std::streampos)(cd_offset + cd_size)) {
                    uint32_t sig = 0;
                    file.read((char*)&sig, 4);
                    if (sig != 0x02014b50) break; // Central directory file header signature

                    file.seekg(24, std::ios::cur);
                    uint16_t fname_len = 0, extra_len = 0, comment_len = 0;
                    file.read((char*)&fname_len, 2);
                    file.read((char*)&extra_len, 2);
                    file.read((char*)&comment_len, 2);
                    file.seekg(12, std::ios::cur); // skip to filename

                    if (fname_len > 0) {
                        std::vector<char> fname_buf(fname_len + 1, 0);
                        file.read(fname_buf.data(), fname_len);
                        results.add(Str(fname_buf.data()));
                    }
                    if (extra_len + comment_len > 0) {
                        file.seekg(extra_len + comment_len, std::ios::cur);
                    }
                }
                if (results.size() > 0) return results;
            }

            // Fallback to tar / unzip command if binary parsing encountered an edge case
#ifdef _WIN32
            std::string cmd = "tar.exe -tf \"" + p + "\"";
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (pipe) {
                char line[1024];
                while (fgets(line, sizeof(line), pipe)) {
                    std::string s(line);
                    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
                    if (!s.empty()) results.add(Str(s.c_str()));
                }
                _pclose(pipe);
            }
#else
            std::string cmd = "tar -tf \"" + p + "\" 2>/dev/null || unzip -l \"" + p + "\"";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char line[1024];
                while (fgets(line, sizeof(line), pipe)) {
                    std::string s(line);
                    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
                    if (!s.empty()) results.add(Str(s.c_str()));
                }
                pclose(pipe);
            }
#endif
            return results;
        }

        // Read text of a file inside zip directly
        inline Str read_text(const Str& zip_path, const Str& file_in_zip) {
            std::string zp = zip_path.c_str();
            std::string fz = file_in_zip.c_str();
            std::string result;

#ifdef _WIN32
            // Use tar -xOf to stream file directly to stdout
            std::string cmd = "tar.exe -xOf \"" + zp + "\" \"" + fz + "\"";
            FILE* pipe = _popen(cmd.c_str(), "rb");
            if (pipe) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
                    result.append(buf, n);
                }
                _pclose(pipe);
            }
#else
            std::string cmd = "tar -xOf \"" + zp + "\" \"" + fz + "\" 2>/dev/null || unzip -p \"" + zp + "\" \"" + fz + "\" 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buf[4096];
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
                    result.append(buf, n);
                }
                pclose(pipe);
            }
#endif
            return Str(result.c_str());
        }

        // Extract entire zip to destination directory
        inline bool extract(const Str& zip_path, const Str& dest_dir) {
            std::string zp = zip_path.c_str();
            std::string dp = dest_dir.c_str();
            fs::create_directories(dp);

#ifdef _WIN32
            std::string cmd = "tar.exe -xf \"" + zp + "\" -C \"" + dp + "\"";
            int ret = std::system(cmd.c_str());
            return ret == 0;
#else
            std::string cmd = "tar -xf \"" + zp + "\" -C \"" + dp + "\" 2>/dev/null || unzip -q -o \"" + zp + "\" -d \"" + dp + "\"";
            int ret = std::system(cmd.c_str());
            return ret == 0;
#endif
        }

    } // namespace zip
} // namespace viss
