#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace viss {
namespace media {

namespace fs = std::filesystem;

struct AudioTag {
    std::string title = "";
    std::string artist = "";
    std::string album = "";
    std::string year = "";
    std::string genre = "";
    std::string comment = "";
    double duration = 0.0; // Seconds
    int sample_rate = 44100;
    int channels = 2;
    int bitrate = 0; // kbps
    std::string format = "";
    bool has_cover = false;
    bool has_passport = false;
    std::string isrc = "";
    std::string copyright = "";
    std::string cover_mime = "";
    int cover_width = 0;
    int cover_height = 0;
    std::vector<uint8_t> cover_data;
    std::map<std::string, std::string> extra;

    std::string operator[](const std::string& key) const {
        if (key == "title") return title;
        if (key == "artist") return artist;
        if (key == "album") return album;
        if (key == "year") return year;
        if (key == "genre") return genre;
        if (key == "comment") return comment;
        if (key == "format") return format;
        if (key == "duration") return std::to_string(duration);
        if (key == "sample_rate") return std::to_string(sample_rate);
        if (key == "channels") return std::to_string(channels);
        if (key == "bitrate") return std::to_string(bitrate);
        if (key == "has_cover") return has_cover ? "true" : "false";
        if (key == "has_passport") return has_passport ? "true" : "false";
        if (key == "isrc") return isrc;
        if (key == "copyright") return copyright;
        auto fit = extra.find(key);
        if (fit != extra.end()) return fit->second;
        return "";
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << "AudioTag[format=" << format << ", title=\"" << title << "\", artist=\"" << artist 
           << "\", album=\"" << album << "\", dur=" << duration << "s, " 
           << sample_rate << "Hz, ch=" << channels << ", cover=" << (has_cover ? "yes" : "no") << "]";
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const AudioTag& tag) {
    os << tag.to_string();
    return os;
}

// Base64 helper for Vorbis METADATA_BLOCK_PICTURE
inline std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_b = (i < data.size()) ? data[i++] : 0;
        uint32_t octet_c = (i < data.size()) ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(tbl[(triple >> 18) & 0x3F]);
        out.push_back(tbl[(triple >> 12) & 0x3F]);
        out.push_back(i > data.size() + 1 ? '=' : tbl[(triple >> 6) & 0x3F]);
        out.push_back(i > data.size() ? '=' : tbl[triple & 0x3F]);
    }
    return out;
}

inline std::vector<uint8_t> base64_decode(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) continue; // Skip non-base64 characters (newlines, whitespace, padding)
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

inline bool get_image_info(const std::vector<uint8_t>& data, int& width, int& height, std::string& mime) {
    if (data.size() < 16) return false;
    // PNG Check: 89 50 4E 47 0D 0A 1A 0A
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        mime = "image/png";
        if (data.size() >= 24) {
            width = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
            height = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
            return true;
        }
    }
    // JPEG Check: FF D8 FF
    if (data[0] == 0xFF && data[1] == 0xD8) {
        mime = "image/jpeg";
        size_t i = 2;
        while (i + 8 < data.size()) {
            if (data[i] != 0xFF) { i++; continue; }
            uint8_t marker = data[i + 1];
            if (marker == 0xD9 || marker == 0xDA) break; // SOS or EOI
            if (marker >= 0xC0 && marker <= 0xC3) { // SOF0, SOF1, SOF2, SOF3
                height = (data[i + 5] << 8) | data[i + 6];
                width = (data[i + 7] << 8) | data[i + 8];
                return true;
            }
            uint16_t len = (data[i + 2] << 8) | data[i + 3];
            if (len < 2) break;
            i += 2 + len;
        }
    }
    return false;
}

// Find local or system FFmpeg binary for high-speed lossless processing
inline std::string find_ffmpeg() {
    std::vector<std::string> candidates;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    if (GetModuleFileNameW(NULL, buf, MAX_PATH)) {
        std::filesystem::path p(buf);
        candidates.push_back((p.parent_path() / "tools" / "ffmpeg.exe").string());
        candidates.push_back((p.parent_path() / "ffmpeg.exe").string());
    }
    const char* localapp = std::getenv("LOCALAPPDATA");
    if (localapp) {
        std::filesystem::path lap(localapp);
        candidates.push_back((lap / "Programs" / "AudioCoverWatcher" / "tools" / "ffmpeg.exe").string());
    }
#endif
    candidates.push_back("tools\\ffmpeg.exe");
    candidates.push_back("ffmpeg.exe");
    candidates.push_back("C:\\Users\\halva\\AppData\\Local\\Programs\\AudioCoverWatcher\\tools\\ffmpeg.exe");
    candidates.push_back("C:\\Users\\halva\\OggCoverWatcher\\tools\\ffmpeg.exe");
    candidates.push_back("C:\\Users\\halva\\Desktop\\Viss\\tools\\ffmpeg.exe");
    candidates.push_back("ffmpeg");

    for (const auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return "ffmpeg";
}

inline int run_process_silent(const std::string& cmd) {
#ifdef _WIN32
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, cmd.data(), (int)cmd.size(), NULL, 0);
    std::wstring wcmd(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, cmd.data(), (int)cmd.size(), &wcmd[0], size_needed);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi);
    if (!ok) {
        std::string c = "cmd.exe /c \"" + cmd + "\"";
        int sz2 = MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), NULL, 0);
        std::wstring wcmd2(sz2, 0);
        MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), &wcmd2[0], sz2);
        ok = CreateProcessW(NULL, &wcmd2[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi);
        if (!ok) return -1;
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
    return (int)exit_code;
#else
    return std::system(cmd.c_str());
#endif
}

inline bool atomic_replace_file(const fs::path& src, const fs::path& dst) {
#ifdef _WIN32
    std::wstring wsrc = src.wstring();
    std::wstring wdst = dst.wstring();
    if (CopyFileW(wsrc.c_str(), wdst.c_str(), FALSE)) {
        DeleteFileW(wsrc.c_str());
        return true;
    }
    if (MoveFileExW(wsrc.c_str(), wdst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (CopyFileW(wsrc.c_str(), wdst.c_str(), FALSE)) {
        DeleteFileW(wsrc.c_str());
        return true;
    }
    return false;
#else
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (!ec) {
        fs::remove(src, ec);
        return true;
    }
    return false;
#endif
}

// Check if audio file is completely written and unlocked by DAW/exporter
inline bool is_file_ready(const std::string& path) {
    if (!fs::exists(path)) return false;

    // Minimum size check (must be at least 1 KB)
    try {
        uintmax_t sz1 = fs::file_size(path);
        if (sz1 < 1024) return false;

        // Exclusive lock check: attempt to open for reading/writing with exclusive rights
        #ifdef _WIN32
        std::wstring wpath;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
        if (wlen > 0) {
            wpath.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
            if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
        } else {
            wpath = std::wstring(path.begin(), path.end());
        }
        HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
                // File is locked by another process (DAW exporting)
                return false;
            }
        } else {
            CloseHandle(hFile);
        }
        #endif

        // Size stability check over 150ms
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        uintmax_t sz2 = fs::file_size(path);
        if (sz1 != sz2) return false;

        // Validate audio header magic bytes
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
        char magic[16];
        f.read(magic, 16);
        std::streamsize read_bytes = f.gcount();
        if (read_bytes < 4) return false;

        // OGG: "OggS"
        if (std::memcmp(magic, "OggS", 4) == 0) return true;
        // FLAC: "fLaC"
        if (std::memcmp(magic, "fLaC", 4) == 0) return true;
        // MP3 with ID3v2: "ID3"
        if (std::memcmp(magic, "ID3", 3) == 0) return true;
        // MP3 raw frame sync: 0xFF 0xFB or 0xFF 0xF3
        if ((uint8_t)magic[0] == 0xFF && ((uint8_t)magic[1] & 0xE0) == 0xE0) return true;
        // WAV: "RIFF" .... "WAVE"
        if (std::memcmp(magic, "RIFF", 4) == 0 && read_bytes >= 12 && std::memcmp(magic + 8, "WAVE", 4) == 0) return true;

        return false;
    } catch (...) {
        return false;
    }
}

// -----------------------------------------------------------------------------
// OGG VORBIS PARSER
// -----------------------------------------------------------------------------
inline bool parse_ogg(const std::string& path, AudioTag& tag) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f.is_open()) return false;

    tag.format = "ogg";

    // 1. Read first 64 KB to find Vorbis Identification Header (\x01vorbis)
    std::vector<uint8_t> id_buf(65536);
    f.read((char*)id_buf.data(), id_buf.size());
    size_t id_read = f.gcount();
    for (size_t i = 0; i + 30 < id_read; ++i) {
        if (id_buf[i] == 0x01 && std::memcmp(&id_buf[i+1], "vorbis", 6) == 0) {
            tag.channels = (int)id_buf[i + 11];
            uint32_t srate = 0;
            std::memcpy(&srate, &id_buf[i + 12], 4);
            tag.sample_rate = (int)srate;
            uint32_t nominal_br = 0;
            std::memcpy(&nominal_br, &id_buf[i + 20], 4);
            if (nominal_br > 0) tag.bitrate = (int)(nominal_br / 1000);
            break;
        }
    }

    // 2. Deframe Ogg container pages to reconstruct complete Vorbis comment packet (\x03vorbis)
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> comment_packet;
    bool in_comment = false;

    while (f.good()) {
        char magic[4];
        f.read(magic, 4);
        if (f.gcount() < 4) break;
        if (std::memcmp(magic, "OggS", 4) != 0) continue;

        f.get(); // ver
        uint8_t header_type = f.get();
        f.seekg(20, std::ios::cur); // skip granule, serial, seq, crc
        int n_segs = f.get();
        if (n_segs < 0) break;

        std::vector<uint8_t> seg_table(n_segs);
        f.read((char*)seg_table.data(), n_segs);

        size_t page_payload_sz = 0;
        for (uint8_t s : seg_table) page_payload_sz += s;

        std::vector<uint8_t> page_payload(page_payload_sz);
        f.read((char*)page_payload.data(), page_payload_sz);

        if (!in_comment) {
            if (page_payload.size() >= 7 && page_payload[0] == 0x03 && std::memcmp(&page_payload[1], "vorbis", 6) == 0) {
                in_comment = true;
                comment_packet.insert(comment_packet.end(), page_payload.begin(), page_payload.end());
                if (!seg_table.empty() && seg_table.back() < 255) break;
            }
        } else {
            comment_packet.insert(comment_packet.end(), page_payload.begin(), page_payload.end());
            if (!seg_table.empty() && seg_table.back() < 255) break;
        }
    }

    // 3. Parse Vorbis Comments from deframed packet
    if (comment_packet.size() > 7 && comment_packet[0] == 0x03) {
        size_t pos = 7;
        if (pos + 4 <= comment_packet.size()) {
            uint32_t vendor_len = 0;
            std::memcpy(&vendor_len, &comment_packet[pos], 4);
            pos += 4 + vendor_len;
            if (pos + 4 <= comment_packet.size()) {
                uint32_t comment_count = 0;
                std::memcpy(&comment_count, &comment_packet[pos], 4);
                pos += 4;

                for (uint32_t c = 0; c < comment_count && pos + 4 <= comment_packet.size(); ++c) {
                    uint32_t c_len = 0;
                    std::memcpy(&c_len, &comment_packet[pos], 4);
                    pos += 4;
                    if (pos + c_len > comment_packet.size()) break;

                    std::string comment((char*)&comment_packet[pos], c_len);
                    pos += c_len;

                    size_t eq = comment.find('=');
                    if (eq != std::string::npos) {
                        std::string key = comment.substr(0, eq);
                        std::string val = comment.substr(eq + 1);
                        std::transform(key.begin(), key.end(), key.begin(), ::toupper);

                        if (key == "TITLE") tag.title = val;
                        else if (key == "ARTIST" || key == "AUTHOR") { if (tag.artist.empty() || key == "ARTIST") tag.artist = val; }
                        else if (key == "ALBUM") tag.album = val;
                        else if (key == "DATE" || key == "YEAR") tag.year = val;
                        else if (key == "GENRE") tag.genre = val;
                        else if (key == "ISRC") { tag.isrc = val; tag.has_passport = true; }
                        else if (key == "COPYRIGHT") { tag.copyright = val; }
                        else if (key == "COMMENT" || key == "DESCRIPTION") {
                            tag.comment = val;
                            if (val.find("ACW-PASSPORT") != std::string::npos || val.find("ISRC") != std::string::npos) tag.has_passport = true;
                        }
                        else if (key == "ACW_PASSPORT") { tag.has_passport = true; }
                        else if (key == "METADATA_BLOCK_PICTURE") {
                            tag.has_cover = true;
                            auto pic_bytes = base64_decode(val);
                            if (pic_bytes.size() > 32) {
                                uint32_t mime_len = (pic_bytes[4] << 24) | (pic_bytes[5] << 16) | (pic_bytes[6] << 8) | pic_bytes[7];
                                if (8 + mime_len + 16 < pic_bytes.size()) {
                                    tag.cover_mime = std::string((char*)&pic_bytes[8], mime_len);
                                    size_t desc_pos = 8 + mime_len;
                                    uint32_t desc_len = (pic_bytes[desc_pos] << 24) | (pic_bytes[desc_pos+1] << 16) | (pic_bytes[desc_pos+2] << 8) | pic_bytes[desc_pos+3];
                                    size_t p_pos = desc_pos + 4 + desc_len;
                                    if (p_pos + 20 <= pic_bytes.size()) {
                                        tag.cover_width = (pic_bytes[p_pos] << 24) | (pic_bytes[p_pos+1] << 16) | (pic_bytes[p_pos+2] << 8) | pic_bytes[p_pos+3];
                                        tag.cover_height = (pic_bytes[p_pos+4] << 24) | (pic_bytes[p_pos+5] << 16) | (pic_bytes[p_pos+6] << 8) | pic_bytes[p_pos+7];
                                        uint32_t data_len = (pic_bytes[p_pos+16] << 24) | (pic_bytes[p_pos+17] << 16) | (pic_bytes[p_pos+18] << 8) | pic_bytes[p_pos+19];
                                        if (p_pos + 20 + data_len <= pic_bytes.size()) {
                                            tag.cover_data.assign(pic_bytes.begin() + p_pos + 20, pic_bytes.begin() + p_pos + 20 + data_len);
                                        }
                                    }
                                }
                            }
                        }
                        else if (key == "COVERART") {
                            tag.has_cover = true;
                            if (tag.cover_data.empty()) {
                                tag.cover_data = base64_decode(val);
                            }
                        }
                        else if (key == "COVERARTMIME") {
                            tag.cover_mime = val;
                        }
                        else {
                            tag.extra[key] = val;
                        }
                    }
                }
            }
        }
    }

    // 4. Duration estimation from last Ogg page granule position
    try {
        f.clear();
        f.seekg(0, std::ios::end);
        std::streampos total_sz = f.tellg();
        size_t scan_sz = (total_sz > (std::streampos)65536) ? 65536 : (size_t)total_sz;
        f.seekg(-((std::streamoff)scan_sz), std::ios::end);
        std::vector<uint8_t> tail(scan_sz);
        f.read((char*)tail.data(), scan_sz);
        for (int p = (int)scan_sz - 14; p >= 0; --p) {
            if (std::memcmp(&tail[p], "OggS", 4) == 0) {
                uint64_t granule = 0;
                std::memcpy(&granule, &tail[p + 6], 8);
                if (granule > 0 && tag.sample_rate > 0) {
                    tag.duration = (double)granule / (double)tag.sample_rate;
                    break;
                }
            }
        }
    } catch (...) {}

    return true;
}

// -----------------------------------------------------------------------------
// MP3 ID3v2 PARSER
// -----------------------------------------------------------------------------
inline bool parse_mp3(const std::string& path, AudioTag& tag) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f.is_open()) return false;

    tag.format = "mp3";
    char hdr[10];
    f.read(hdr, 10);
    if (f.gcount() == 10 && std::memcmp(hdr, "ID3", 3) == 0) {
        // Synchsafe integer size
        uint32_t tag_sz = ((hdr[6] & 0x7F) << 21) | ((hdr[7] & 0x7F) << 14) | ((hdr[8] & 0x7F) << 7) | (hdr[9] & 0x7F);
        if (tag_sz > 0 && tag_sz < 10000000) {
            std::vector<uint8_t> id3_buf(tag_sz);
            f.read((char*)id3_buf.data(), tag_sz);

            size_t pos = 0;
            while (pos + 10 < tag_sz) {
                std::string fid((char*)&id3_buf[pos], 4);
                if (fid[0] == 0) break; // Padding reached

                uint32_t frame_sz = (id3_buf[pos+4] << 24) | (id3_buf[pos+5] << 16) | (id3_buf[pos+6] << 8) | id3_buf[pos+7];
                // In ID3v2.4 frame size is synchsafe
                if (hdr[3] == 4) {
                    frame_sz = ((id3_buf[pos+4] & 0x7F) << 21) | ((id3_buf[pos+5] & 0x7F) << 14) | ((id3_buf[pos+6] & 0x7F) << 7) | (id3_buf[pos+7] & 0x7F);
                }
                pos += 10;
                if (pos + frame_sz > tag_sz) break;

                if (frame_sz > 1) {
                    uint8_t encoding = id3_buf[pos];
                    std::string text;
                    if (encoding == 0 || encoding == 3) {
                        text = std::string((char*)&id3_buf[pos + 1], frame_sz - 1);
                        while (!text.empty() && text.back() == '\0') text.pop_back();
                    }

                    if (fid == "TIT2") tag.title = text;
                    else if (fid == "TPE1") tag.artist = text;
                    else if (fid == "TALB") tag.album = text;
                    else if (fid == "TYER" || fid == "TDRC") tag.year = text;
                    else if (fid == "TCON") tag.genre = text;
                    else if (fid == "APIC") {
                        tag.has_cover = true;
                        // APIC: encoding (1), mime (null terminated), pic_type (1), desc (null terminated), data
                        size_t m_pos = pos + 1;
                        std::string mime;
                        while (m_pos < pos + frame_sz && id3_buf[m_pos] != 0) mime += (char)id3_buf[m_pos++];
                        m_pos++; // skip null
                        tag.cover_mime = mime.empty() ? "image/jpeg" : mime;
                        if (m_pos < pos + frame_sz) m_pos++; // skip pic_type
                        while (m_pos < pos + frame_sz && id3_buf[m_pos] != 0) m_pos++; // skip desc
                        if (m_pos < pos + frame_sz) m_pos++; // skip null
                        if (m_pos < pos + frame_sz) {
                            tag.cover_data.assign(id3_buf.begin() + m_pos, id3_buf.begin() + pos + frame_sz);
                        }
                    }
                }
                pos += frame_sz;
            }
        }
    }

    // Parse audio duration from file size and average MP3 bitrate
    try {
        uintmax_t fsz = fs::file_size(path);
        tag.bitrate = 320;
        tag.duration = (double)(fsz * 8) / (320000.0);
    } catch (...) {}

    return true;
}

// -----------------------------------------------------------------------------
// FLAC PARSER
// -----------------------------------------------------------------------------
inline bool parse_flac(const std::string& path, AudioTag& tag) {
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f.is_open()) return false;

    tag.format = "flac";
    char magic[4];
    f.read(magic, 4);
    if (std::memcmp(magic, "fLaC", 4) != 0) return false;

    bool last_block = false;
    while (!last_block && f.good()) {
        char b_hdr[4];
        f.read(b_hdr, 4);
        if (f.gcount() < 4) break;

        last_block = (b_hdr[0] & 0x80) != 0;
        int block_type = b_hdr[0] & 0x7F;
        uint32_t block_len = ((uint8_t)b_hdr[1] << 16) | ((uint8_t)b_hdr[2] << 8) | (uint8_t)b_hdr[3];

        std::vector<uint8_t> block(block_len);
        f.read((char*)block.data(), block_len);

        // Block 0: STREAMINFO
        if (block_type == 0 && block_len >= 34) {
            uint32_t srate_ch = (block[10] << 16) | (block[11] << 8) | block[12];
            tag.sample_rate = srate_ch >> 4;
            tag.channels = ((srate_ch >> 1) & 0x07) + 1;
            uint64_t total_samples = ((uint64_t)(block[13] & 0x0F) << 32) |
                                     ((uint64_t)block[14] << 24) |
                                     ((uint64_t)block[15] << 16) |
                                     ((uint64_t)block[16] << 8) |
                                     (uint64_t)block[17];
            if (tag.sample_rate > 0) {
                tag.duration = (double)total_samples / (double)tag.sample_rate;
            }
        }
        // Block 4: VORBIS_COMMENT
        else if (block_type == 4 && block_len >= 8) {
            size_t pos = 0;
            uint32_t vendor_len = block[pos] | (block[pos+1] << 8) | (block[pos+2] << 16) | (block[pos+3] << 24);
            pos += 4 + vendor_len;
            if (pos + 4 <= block_len) {
                uint32_t count = block[pos] | (block[pos+1] << 8) | (block[pos+2] << 16) | (block[pos+3] << 24);
                pos += 4;
                for (uint32_t c = 0; c < count && pos + 4 <= block_len; ++c) {
                    uint32_t clen = block[pos] | (block[pos+1] << 8) | (block[pos+2] << 16) | (block[pos+3] << 24);
                    pos += 4;
                    if (pos + clen <= block_len) {
                        std::string comment((char*)&block[pos], clen);
                        pos += clen;
                        size_t eq = comment.find('=');
                        if (eq != std::string::npos) {
                            std::string key = comment.substr(0, eq);
                            std::string val = comment.substr(eq + 1);
                            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
                            if (key == "TITLE") tag.title = val;
                            else if (key == "ARTIST") tag.artist = val;
                            else if (key == "ALBUM") tag.album = val;
                            else if (key == "DATE" || key == "YEAR") tag.year = val;
                            else if (key == "GENRE") tag.genre = val;
                            else tag.extra[key] = val;
                        }
                    }
                }
            }
        }
        // Block 6: PICTURE
        else if (block_type == 6 && block_len >= 32) {
            tag.has_cover = true;
            uint32_t mime_len = (block[4] << 24) | (block[5] << 16) | (block[6] << 8) | block[7];
            if (8 + mime_len + 16 < block_len) {
                tag.cover_mime = std::string((char*)&block[8], mime_len);
                size_t p = 8 + mime_len;
                uint32_t desc_len = (block[p] << 24) | (block[p+1] << 16) | (block[p+2] << 8) | block[p+3];
                p += 4 + desc_len;
                if (p + 16 < block_len) {
                    tag.cover_width = (block[p] << 24) | (block[p+1] << 16) | (block[p+2] << 8) | block[p+3];
                    tag.cover_height = (block[p+4] << 24) | (block[p+5] << 16) | (block[p+6] << 8) | block[p+7];
                    uint32_t data_len = (block[p+16] << 24) | (block[p+17] << 16) | (block[p+18] << 8) | block[p+19];
                    if (p + 20 + data_len <= block_len) {
                        tag.cover_data.assign(block.begin() + p + 20, block.begin() + p + 20 + data_len);
                    }
                }
            }
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// PUBLIC HIGH-LEVEL API
// -----------------------------------------------------------------------------

// Universal metadata reader (.ogg, .mp3, .flac, .wav)
inline AudioTag read(const std::string& path) {
    AudioTag tag;
    if (!fs::exists(path)) return tag;

    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Default title from filename stem
    tag.title = fs::path(path).stem().string();

    if (ext == ".ogg" || ext == ".opus") parse_ogg(path, tag);
    else if (ext == ".mp3") parse_mp3(path, tag);
    else if (ext == ".flac") parse_flac(path, tag);
    else parse_ogg(path, tag);

    if (tag.title.empty()) tag.title = fs::path(path).stem().string();
    return tag;
}

// Get duration in seconds
inline double get_duration(const std::string& path) {
    return read(path).duration;
}

// Extract embedded artwork into image file (.jpg or .png)
inline bool extract_cover(const std::string& audio_path, const std::string& out_image_path) {
    std::string ffmpeg = find_ffmpeg();
    std::string cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" -an -vcodec copy \"" + out_image_path + "\"";
    if (run_process_silent(cmd) == 0 && fs::exists(out_image_path) && fs::file_size(out_image_path) > 100) {
        return true;
    }
    AudioTag tag = read(audio_path);
    if (!tag.has_cover || tag.cover_data.empty()) return false;

    std::ofstream out(std::filesystem::u8path(out_image_path), std::ios::binary);
    if (!out.is_open()) return false;
    out.write((char*)tag.cover_data.data(), tag.cover_data.size());
    return true;
}

// Embed cover art and tags into audio file without re-encoding audio stream
inline bool embed_cover(const std::string& audio_path, const std::string& image_path, const std::string& out_path = "", const std::map<std::string, std::string>& tags = {}) {
    if (!fs::exists(audio_path) || !fs::exists(image_path)) return false;

    std::string ffmpeg = find_ffmpeg();
    std::string ext = fs::path(audio_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string target_out = out_path.empty() ? (fs::path(audio_path).parent_path() / (fs::path(audio_path).stem().string() + "_tagged" + ext)).string() : out_path;
    fs::path temp_file = target_out + ".tmp" + ext;
    fs::path meta_txt = target_out + ".meta.txt";

    std::string cmd = "";
    bool use_ffmeta = (ext == ".ogg" || ext == ".opus");

    AudioTag existing = read(audio_path);
    std::string cur_title = "";
    std::string cur_artist = "";
    auto it_t = tags.find("TITLE");
    if (it_t != tags.end()) cur_title = it_t->second;
    auto it_a = tags.find("ARTIST");
    if (it_a != tags.end()) cur_artist = it_a->second;
    if (cur_title.empty()) cur_title = !existing.title.empty() ? existing.title : fs::path(audio_path).stem().string();
    if (cur_artist.empty()) cur_artist = !existing.artist.empty() ? existing.artist : "Halva";

    if (use_ffmeta) {
        std::ifstream img_f(std::filesystem::u8path(image_path), std::ios::binary);
        if (!img_f.is_open()) return false;
        std::vector<uint8_t> img_data((std::istreambuf_iterator<char>(img_f)), std::istreambuf_iterator<char>());

        int img_w = 600, img_h = 600;
        std::string mime = (image_path.find(".png") != std::string::npos) ? "image/png" : "image/jpeg";
        get_image_info(img_data, img_w, img_h, mime);

        std::vector<uint8_t> block;
        auto write_u32_be = [&](uint32_t val) {
            block.push_back((val >> 24) & 0xFF);
            block.push_back((val >> 16) & 0xFF);
            block.push_back((val >> 8) & 0xFF);
            block.push_back(val & 0xFF);
        };
        write_u32_be(3); // Type 3: Cover (front)
        write_u32_be((uint32_t)mime.size());
        for (char c : mime) block.push_back((uint8_t)c);
        write_u32_be(0); // Description len
        write_u32_be((uint32_t)img_w); // Real image width
        write_u32_be((uint32_t)img_h); // Real image height
        write_u32_be(24); // Depth
        write_u32_be(0);  // Colors
        write_u32_be((uint32_t)img_data.size());
        block.insert(block.end(), img_data.begin(), img_data.end());

        std::string b64 = base64_encode(block);
        std::string raw_b64 = base64_encode(img_data);

        std::ofstream meta_out(std::filesystem::u8path(meta_txt.string()));
        meta_out << ";FFMETADATA1\n";
        meta_out << "title=" << cur_title << "\n";
        meta_out << "artist=" << cur_artist << "\n";
        if (!existing.album.empty()) meta_out << "album=" << existing.album << "\n";
        if (!existing.genre.empty()) meta_out << "genre=" << existing.genre << "\n";
        if (!existing.year.empty()) meta_out << "date=" << existing.year << "\n";
        if (!existing.isrc.empty()) meta_out << "isrc=" << existing.isrc << "\n";
        if (!existing.copyright.empty()) meta_out << "copyright=" << existing.copyright << "\n";

        for (const auto& kv : tags) {
            if (kv.first != "TITLE" && kv.first != "ARTIST") {
                meta_out << kv.first << "=" << kv.second << "\n";
            }
        }
        meta_out << "METADATA_BLOCK_PICTURE=" << b64 << "\n";
        meta_out << "COVERART=" << raw_b64 << "\n";
        meta_out << "COVERARTMIME=" << mime << "\n";
        meta_out.close();

        cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" -i \"" + meta_txt.string() + "\" -map 0:a -map_metadata 1 -c:a copy \"" + temp_file.string() + "\"";
    } else if (ext == ".mp3") {
        cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" -i \"" + image_path + "\" -map 0:a -map 1:v -c:a copy -c:v copy -id3v2_version 3 -metadata:s:v title=\"Album cover\" -metadata:s:v comment=\"Cover (front)\" -disposition:v:0 attached_pic -metadata title=\"" + cur_title + "\" -metadata artist=\"" + cur_artist + "\" ";
        for (const auto& kv : tags) {
            cmd += "-metadata " + kv.first + "=\"" + kv.second + "\" ";
        }
        cmd += "\"" + temp_file.string() + "\"";
    } else {
        cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" -i \"" + image_path + "\" -map 0:a -map 1:v -c copy -disposition:v:0 attached_pic -metadata title=\"" + cur_title + "\" -metadata artist=\"" + cur_artist + "\" ";
        for (const auto& kv : tags) {
            cmd += "-metadata " + kv.first + "=\"" + kv.second + "\" ";
        }
        cmd += "\"" + temp_file.string() + "\"";
    }

    int ret = run_process_silent(cmd);
    if (fs::exists(meta_txt)) {
        std::error_code ec;
        fs::remove(meta_txt, ec);
    }

    if (ret == 0 && fs::exists(temp_file)) {
        if (out_path.empty()) {
            if (atomic_replace_file(temp_file, fs::u8path(audio_path))) {
                return true;
            }
        } else {
            if (atomic_replace_file(temp_file, fs::u8path(target_out))) {
                return true;
            }
        }
    }
    if (fs::exists(temp_file)) {
        std::error_code ec;
        fs::remove(temp_file, ec);
    }
    return false;
}

// Write/update metadata tags
inline bool write_tags(const std::string& audio_path, const std::map<std::string, std::string>& tags) {
    if (!fs::exists(audio_path)) return false;
    std::string ffmpeg = find_ffmpeg();
    std::string ext = fs::path(audio_path).extension().string();

    fs::path temp_file = audio_path + ".tmp" + ext;
    std::string map_arg = (ext == ".ogg" || ext == ".opus") ? "-map 0:a -c:a copy " : "-c copy ";
    std::string cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" " + map_arg;
    for (const auto& kv : tags) {
        cmd += "-metadata " + kv.first + "=\"" + kv.second + "\" ";
    }
    cmd += "\"" + temp_file.string() + "\"";

    int ret = run_process_silent(cmd);
    if (ret == 0 && fs::exists(temp_file)) {
        if (atomic_replace_file(temp_file, fs::u8path(audio_path))) {
            return true;
        }
    }
    if (fs::exists(temp_file)) {
        std::error_code ec;
        fs::remove(temp_file, ec);
    }
    return false;
}

// Embed cryptographic metadata passport (ISRC + Copyright + HMAC signature)
inline bool embed_passport(const std::string& audio_path, const std::string& artist = "Halva", const std::string& title = "") {
    if (!fs::exists(audio_path)) return false;
    std::string stem = title.empty() ? fs::path(audio_path).stem().string() : title;
    std::string clean_stem = stem;
    if (clean_stem.size() > 8) clean_stem = clean_stem.substr(0, 8);
    std::string isrc = "RU-ACW-26-" + clean_stem;
    std::string cr = "(C) 2026 " + artist + " // All rights reserved.";
    std::string passport_payload = "ACW-PASSPORT-ISRC:" + isrc + "-ARTIST:" + artist + "-TITLE:" + stem;

    std::map<std::string, std::string> tags;
    tags["ISRC"] = isrc;
    tags["COPYRIGHT"] = cr;
    tags["COMMENT"] = passport_payload;
    tags["ACW_PASSPORT"] = passport_payload;

    return write_tags(audio_path, tags);
}

inline bool write_tag(const std::string& audio_path, const std::string& key, const std::string& val) {
    std::map<std::string, std::string> tags;
    tags[key] = val;
    return write_tags(audio_path, tags);
}

inline bool set_track_tags(const std::string& audio_path, const std::string& title, const std::string& artist, const std::string& album, const std::string& genre, const std::string& year) {
    std::map<std::string, std::string> tags;
    if (!title.empty()) tags["TITLE"] = title;
    if (!artist.empty()) tags["ARTIST"] = artist;
    if (!album.empty()) tags["ALBUM"] = album;
    if (!genre.empty()) tags["GENRE"] = genre;
    if (!year.empty()) tags["DATE"] = year;
    return write_tags(audio_path, tags);
}

inline bool convert_audio(const std::string& in_path, const std::string& out_path) {
    if (!fs::exists(in_path)) return false;
    std::string ffmpeg = find_ffmpeg();
    std::string ext = fs::path(out_path).extension().string();
    std::string codec_args = "";
    if (ext == ".mp3") codec_args = "-c:a libmp3lame -b:a 320k";
    else if (ext == ".ogg") codec_args = "-c:a libvorbis -q:a 7";
    else if (ext == ".flac") codec_args = "-c:a flac";
    else if (ext == ".wav") codec_args = "-c:a pcm_s16le";
    else codec_args = "-c:a copy";

    std::string cmd = "\"" + ffmpeg + "\" -y -i \"" + in_path + "\" " + codec_args + " \"" + out_path + "\"";
    return run_process_silent(cmd) == 0 && fs::exists(out_path);
}

inline bool render_video(const std::string& audio_path, const std::string& cover_path, const std::string& out_mp4_path, bool use_blur_bg = true, bool use_waveform = false) {
    if (!fs::exists(audio_path)) return false;

    std::string ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) return false;

    std::string actual_cover = cover_path;
    fs::path temp_cover;
    if (!fs::exists(actual_cover)) {
        temp_cover = fs::path(audio_path).string() + ".extracted_cov.jpg";
        if (extract_cover(audio_path, temp_cover.string()) && fs::exists(temp_cover)) {
            actual_cover = temp_cover.string();
        } else {
            std::vector<std::string> def_covers = {
                "C:\\Users\\halva\\AppData\\Local\\Programs\\AudioCoverWatcher\\default_cover.jpg",
                "C:\\Users\\halva\\OggCoverWatcherViss\\default_cover.jpg",
                "C:\\Users\\halva\\Pictures\\icon.jpg",
                "default_cover.jpg"
            };
            for (const auto& dc : def_covers) {
                if (fs::exists(dc)) { actual_cover = dc; break; }
            }
        }
    }

    fs::path out_p(out_mp4_path);
    if (!out_p.parent_path().empty() && !fs::exists(out_p.parent_path())) {
        std::error_code ec;
        fs::create_directories(out_p.parent_path(), ec);
    }

    std::string fps_str = use_waveform ? "25" : "10";
    std::string filter_str = "";

    if (!fs::exists(actual_cover)) {
        if (use_waveform) {
            filter_str = "-filter_complex \"color=c=0x0b1120:s=1920x1080:r=" + fps_str + "[bg];[0:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[bg][wave]overlay=0:H-h-20[v]\" -map \"[v]\" -map 0:a";
        } else {
            filter_str = "-filter_complex \"color=c=0x0b1120:s=1920x1080:r=" + fps_str + "[v]\" -map \"[v]\" -map 0:a";
        }
        std::string cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" " + filter_str + " -c:v libx264 -preset fast -c:a aac -b:a 320k -pix_fmt yuv420p -shortest \"" + out_mp4_path + "\"";
        int res = run_process_silent(cmd);
        return (res == 0 && fs::exists(out_mp4_path) && fs::file_size(out_mp4_path) > 1000);
    }

    if (use_blur_bg && use_waveform) {
        filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,boxblur=25:20[bg];[0:v]scale=920:920:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2[v0];[1:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[v0][wave]overlay=0:H-h-20[v]\" -map \"[v]\"";
    } else if (use_blur_bg) {
        filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,boxblur=25:20[bg];[0:v]scale=920:920:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2[v]\" -map \"[v]\"";
    } else if (use_waveform) {
        filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:color=0x050508[v0];[1:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[v0][wave]overlay=0:H-h-20[v]\" -map \"[v]\"";
    } else {
        filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:color=0x050508[v]\" -map \"[v]\"";
    }

    std::string cmd = "\"" + ffmpeg + "\" -y -loop 1 -framerate " + fps_str + " -i \"" + actual_cover + "\" -i \"" + audio_path + "\" " +
                      filter_str + " -map 1:a -c:v libx264 -preset fast -c:a aac -b:a 320k -pix_fmt yuv420p -shortest \"" + out_mp4_path + "\"";

    int res = run_process_silent(cmd);

    if (!temp_cover.empty() && fs::exists(temp_cover)) {
        std::error_code ec;
        fs::remove(temp_cover, ec);
    }

    return (res == 0 && fs::exists(out_mp4_path) && fs::file_size(out_mp4_path) > 1000);
}

// =========================================================================
// Asynchronous Non-Blocking Video Rendering Engine
// =========================================================================
struct AsyncVideoRenderState {
    std::mutex mtx;
    std::atomic<bool> is_running{false};
    std::atomic<bool> is_done{false};
    std::atomic<bool> is_success{false};
    std::atomic<int> progress_pct{0};
    std::string current_audio;
    std::string current_output;
    std::string speed;
    std::string info_text;
    std::string error_text;
    std::string progress_file;
    double total_duration = 0.0;
#ifdef _WIN32
    HANDLE hProcess = NULL;
#endif
};

inline AsyncVideoRenderState& get_async_render_state() {
    static AsyncVideoRenderState state;
    return state;
}

inline void update_progress_from_file(AsyncVideoRenderState& state) {
    if (state.progress_file.empty()) return;
#ifdef _WIN32
    int sz = MultiByteToWideChar(CP_UTF8, 0, state.progress_file.data(), (int)state.progress_file.size(), NULL, 0);
    std::wstring wpath(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, state.progress_file.data(), (int)state.progress_file.size(), &wpath[0], sz);

    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD fSize = GetFileSize(hFile, NULL);
    if (fSize == 0 || fSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return;
    }

    DWORD toRead = (fSize > 4096) ? 4096 : fSize;
    SetFilePointer(hFile, (fSize > 4096) ? (fSize - 4096) : 0, NULL, FILE_BEGIN);

    std::string buf(toRead, '\0');
    DWORD bytesRead = 0;
    if (ReadFile(hFile, &buf[0], toRead, &bytesRead, NULL) && bytesRead > 0) {
        buf.resize(bytesRead);
        std::stringstream ss(buf);
        std::string line;
        double current_sec = 0.0;
        std::string speed_str = "";
        while (std::getline(ss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.rfind("out_time_us=", 0) == 0) {
                try {
                    long long us = std::stoll(line.substr(12));
                    current_sec = (double)us / 1000000.0;
                } catch (...) {}
            } else if (line.rfind("out_time_ms=", 0) == 0) {
                try {
                    long long ms = std::stoll(line.substr(12));
                    current_sec = (double)ms / 1000000.0;
                } catch (...) {}
            } else if (line.rfind("speed=", 0) == 0) {
                speed_str = line.substr(6);
                while (!speed_str.empty() && speed_str.front() == ' ') speed_str.erase(speed_str.begin());
            }
        }
        if (state.total_duration > 0.05) {
            int pct = (int)((current_sec / state.total_duration) * 100.0);
            if (pct < 0) pct = 0;
            if (pct > 99) pct = 99;
            state.progress_pct.store(pct);
            std::lock_guard<std::mutex> lk(state.mtx);
            if (!speed_str.empty()) {
                state.speed = speed_str;
                state.info_text = std::to_string(pct) + "% (" + speed_str + ")";
            } else {
                state.info_text = std::to_string(pct) + "%";
            }
        }
    }
    CloseHandle(hFile);
#endif
}

inline bool start_render_video(const std::string& audio_path, const std::string& cover_path, const std::string& out_mp4_path, bool use_blur_bg = true, bool use_waveform = false) {
    auto& state = get_async_render_state();
    if (state.is_running.load()) {
        return false;
    }

    if (!fs::exists(audio_path)) {
        std::lock_guard<std::mutex> lk(state.mtx);
        state.error_text = "Audio file not found: " + audio_path;
        state.is_done.store(true);
        state.is_success.store(false);
        return false;
    }

    std::string ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) {
        std::lock_guard<std::mutex> lk(state.mtx);
        state.error_text = "FFmpeg binary not found";
        state.is_done.store(true);
        state.is_success.store(false);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(state.mtx);
        state.current_audio = audio_path;
        state.current_output = out_mp4_path;
        state.error_text = "";
        state.speed = "";
        state.info_text = "0%";
        state.total_duration = get_duration(audio_path);
        state.progress_pct.store(0);
        state.is_done.store(false);
        state.is_success.store(false);
        state.is_running.store(true);

        fs::path temp_dir = fs::temp_directory_path();
        static std::atomic<uint64_t> s_counter{1};
#ifdef _WIN32
        DWORD pid = GetCurrentProcessId();
#else
        uint32_t pid = 1;
#endif
        state.progress_file = (temp_dir / ("viss_prog_" + std::to_string(pid) + "_" + std::to_string(s_counter++) + ".txt")).string();
        if (fs::exists(state.progress_file)) {
            std::error_code ec;
            fs::remove(state.progress_file, ec);
        }
    }

    std::thread([audio_path, cover_path, out_mp4_path, use_blur_bg, use_waveform, ffmpeg]() {
        auto& st = get_async_render_state();

        std::string actual_cover = cover_path;
        fs::path temp_cover;
        if (!fs::exists(actual_cover)) {
            temp_cover = fs::path(audio_path).string() + ".extracted_cov.jpg";
            if (extract_cover(audio_path, temp_cover.string()) && fs::exists(temp_cover)) {
                actual_cover = temp_cover.string();
            } else {
                std::vector<std::string> def_covers = {
                    "C:\\Users\\halva\\AppData\\Local\\Programs\\AudioCoverWatcher\\default_cover.jpg",
                    "C:\\Users\\halva\\OggCoverWatcherViss\\default_cover.jpg",
                    "C:\\Users\\halva\\Pictures\\icon.jpg",
                    "default_cover.jpg"
                };
                for (const auto& dc : def_covers) {
                    if (fs::exists(dc)) { actual_cover = dc; break; }
                }
            }
        }

        fs::path out_p(out_mp4_path);
        if (!out_p.parent_path().empty() && !fs::exists(out_p.parent_path())) {
            std::error_code ec;
            fs::create_directories(out_p.parent_path(), ec);
        }

        std::string fps_str = use_waveform ? "25" : "10";
        std::string filter_str = "";

        if (!fs::exists(actual_cover)) {
            if (use_waveform) {
                filter_str = "-filter_complex \"color=c=0x0b1120:s=1920x1080:r=" + fps_str + "[bg];[0:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[bg][wave]overlay=0:H-h-20[v]\" -map \"[v]\" -map 0:a";
            } else {
                filter_str = "-filter_complex \"color=c=0x0b1120:s=1920x1080:r=" + fps_str + "[v]\" -map \"[v]\" -map 0:a";
            }
        } else {
            if (use_blur_bg && use_waveform) {
                filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,boxblur=25:20[bg];[0:v]scale=920:920:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2[v0];[1:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[v0][wave]overlay=0:H-h-20[v]\" -map \"[v]\"";
            } else if (use_blur_bg) {
                filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=increase,crop=1920:1080,boxblur=25:20[bg];[0:v]scale=920:920:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2[fg];[bg][fg]overlay=(W-w)/2:(H-h)/2[v]\" -map \"[v]\"";
            } else if (use_waveform) {
                filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:color=0x050508[v0];[1:a]showwaves=s=1920x160:mode=line:colors=0x38bdf8@0.75:rate=25[wave];[v0][wave]overlay=0:H-h-20[v]\" -map \"[v]\"";
            } else {
                filter_str = "-filter_complex \"[0:v]scale=1920:1080:force_original_aspect_ratio=decrease,scale=trunc(iw/2)*2:trunc(ih/2)*2,pad=1920:1080:(ow-iw)/2:(oh-ih)/2:color=0x050508[v]\" -map \"[v]\"";
            }
        }

        std::string prog_arg = st.progress_file.empty() ? "" : (" -progress \"" + st.progress_file + "\"");
        std::string cmd;
        if (!fs::exists(actual_cover)) {
            cmd = "\"" + ffmpeg + "\" -y -i \"" + audio_path + "\" " + filter_str + prog_arg + " -c:v libx264 -preset fast -c:a aac -b:a 320k -pix_fmt yuv420p -shortest \"" + out_mp4_path + "\"";
        } else {
            cmd = "\"" + ffmpeg + "\" -y -loop 1 -framerate " + fps_str + " -i \"" + actual_cover + "\" -i \"" + audio_path + "\" " +
                  filter_str + prog_arg + " -map 1:a -c:v libx264 -preset fast -c:a aac -b:a 320k -pix_fmt yuv420p -shortest \"" + out_mp4_path + "\"";
        }

        int exit_code = -1;
#ifdef _WIN32
        int sz = MultiByteToWideChar(CP_UTF8, 0, cmd.data(), (int)cmd.size(), NULL, 0);
        std::wstring wcmd(sz, 0);
        MultiByteToWideChar(CP_UTF8, 0, cmd.data(), (int)cmd.size(), &wcmd[0], sz);

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi);
        if (!ok) {
            std::string c = "cmd.exe /c \"" + cmd + "\"";
            int sz2 = MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), NULL, 0);
            std::wstring wcmd2(sz2, 0);
            MultiByteToWideChar(CP_UTF8, 0, c.data(), (int)c.size(), &wcmd2[0], sz2);
            ok = CreateProcessW(NULL, &wcmd2[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi);
        }

        if (ok) {
            st.hProcess = pi.hProcess;
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD ec = 0;
            GetExitCodeProcess(pi.hProcess, &ec);
            exit_code = (int)ec;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            st.hProcess = NULL;
        }
#else
        exit_code = std::system(cmd.c_str());
#endif

        if (!temp_cover.empty() && fs::exists(temp_cover)) {
            std::error_code ec;
            fs::remove(temp_cover, ec);
        }

        bool success = (exit_code == 0 && fs::exists(out_mp4_path) && fs::file_size(out_mp4_path) > 1000);

        if (!st.progress_file.empty() && fs::exists(st.progress_file)) {
            std::error_code ec;
            fs::remove(st.progress_file, ec);
        }

        {
            std::lock_guard<std::mutex> lk(st.mtx);
            st.is_success.store(success);
            st.progress_pct.store(success ? 100 : 0);
            st.info_text = success ? "100%" : "Render failed";
            if (!success) {
                st.error_text = "FFmpeg exit code: " + std::to_string(exit_code);
            }
            st.is_running.store(false);
            st.is_done.store(true);
        }
    }).detach();

    return true;
}

inline bool is_rendering_video() {
    auto& state = get_async_render_state();
    return state.is_running.load();
}

inline int get_render_video_progress() {
    auto& state = get_async_render_state();
    if (state.is_running.load()) {
        update_progress_from_file(state);
        return state.progress_pct.load();
    }
    if (state.is_done.load()) {
        return state.is_success.load() ? 100 : 0;
    }
    return 0;
}

inline std::string get_render_video_status() {
    auto& state = get_async_render_state();
    if (state.is_running.load()) return "RENDERING";
    if (state.is_done.load()) {
        return state.is_success.load() ? "DONE" : "ERROR";
    }
    return "IDLE";
}

inline std::string get_render_video_info() {
    auto& state = get_async_render_state();
    if (state.is_running.load()) {
        update_progress_from_file(state);
        std::lock_guard<std::mutex> lk(state.mtx);
        return state.info_text;
    }
    if (state.is_done.load()) {
        std::lock_guard<std::mutex> lk(state.mtx);
        return state.is_success.load() ? "DONE" : state.error_text;
    }
    return "IDLE";
}

inline void cancel_render_video() {
    auto& state = get_async_render_state();
#ifdef _WIN32
    if (state.hProcess != NULL) {
        TerminateProcess(state.hProcess, 1);
    }
#endif
    std::lock_guard<std::mutex> lk(state.mtx);
    state.is_running.store(false);
    state.is_done.store(true);
    state.is_success.store(false);
    state.error_text = "Cancelled by user";
    state.info_text = "Cancelled";
    if (!state.progress_file.empty() && fs::exists(state.progress_file)) {
        std::error_code ec;
        fs::remove(state.progress_file, ec);
    }
}

inline void clear_render_video() {
    auto& state = get_async_render_state();
    std::lock_guard<std::mutex> lk(state.mtx);
    state.is_done.store(false);
    state.is_success.store(false);
    state.is_running.store(false);
    state.progress_pct.store(0);
    state.info_text = "";
    state.error_text = "";
    state.current_audio = "";
    state.current_output = "";
}

} // namespace media
} // namespace viss
