#pragma once
#include "../vissrt.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#endif

namespace viss {
namespace license {

class SHA256 {
private:
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    static uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    static uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    static uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    static const uint32_t K[64];

    void transform(const uint8_t data[64]) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
                   ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sigma1(e) + ch(e, f, g) + K[i] + w[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

public:
    SHA256() { reset(); }

    void reset() {
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
        count = 0;
    }

    void update(const uint8_t* data, size_t len) {
        size_t idx = (count >> 3) & 63;
        count += (len << 3);
        size_t partLen = 64 - idx;
        size_t i = 0;

        if (len >= partLen) {
            memcpy(&buffer[idx], data, partLen);
            transform(buffer);
            for (i = partLen; i + 63 < len; i += 64) transform(&data[i]);
            idx = 0;
        }
        memcpy(&buffer[idx], &data[i], len - i);
    }

    std::vector<uint8_t> digest() {
        uint8_t finalCount[8];
        for (int i = 0; i < 8; ++i) finalCount[i] = (uint8_t)((count >> ((7 - i) * 8)) & 0xff);

        uint8_t pad = 0x80;
        update(&pad, 1);
        pad = 0x00;
        while (((count >> 3) & 63) != 56) update(&pad, 1);
        update(finalCount, 8);

        std::vector<uint8_t> result(32);
        for (int i = 0; i < 8; ++i) {
            result[i * 4] = (uint8_t)((state[i] >> 24) & 0xff);
            result[i * 4 + 1] = (uint8_t)((state[i] >> 16) & 0xff);
            result[i * 4 + 2] = (uint8_t)((state[i] >> 8) & 0xff);
            result[i * 4 + 3] = (uint8_t)(state[i] & 0xff);
        }
        return result;
    }

    static std::vector<uint8_t> hash(const uint8_t* data, size_t len) {
        SHA256 ctx;
        ctx.update(data, len);
        return ctx.digest();
    }

    static std::string hex(const std::vector<uint8_t>& d) {
        std::stringstream ss;
        for (uint8_t b : d) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return ss.str();
    }
};

inline const uint32_t SHA256::K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline std::vector<uint8_t> hmac_sha256(const std::string& key, const std::string& msg) {
    uint8_t k[64] = {0};
    if (key.size() > 64) {
        auto h = SHA256::hash((const uint8_t*)key.data(), key.size());
        memcpy(k, h.data(), 32);
    } else {
        memcpy(k, key.data(), key.size());
    }

    uint8_t i_pad[64], o_pad[64];
    for (int i = 0; i < 64; ++i) {
        i_pad[i] = k[i] ^ 0x36;
        o_pad[i] = k[i] ^ 0x5c;
    }

    SHA256 ctx_inner;
    ctx_inner.update(i_pad, 64);
    ctx_inner.update((const uint8_t*)msg.data(), msg.size());
    auto inner_hash = ctx_inner.digest();

    SHA256 ctx_outer;
    ctx_outer.update(o_pad, 64);
    ctx_outer.update(inner_hash.data(), 32);
    return ctx_outer.digest();
}

inline std::string get_raw_fingerprint() {
    std::string mg = "UNKNOWN_GUID";
    std::string cpu = "UNKNOWN_CPU";
    char comp_buf[256] = {0};
    DWORD sz = sizeof(comp_buf);
    if (GetComputerNameA(comp_buf, &sz)) {}
    std::string comp = comp_buf;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char val[256] = {0};
        DWORD val_sz = sizeof(val);
        if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)val, &val_sz) == ERROR_SUCCESS) {
            mg = val;
        }
        RegCloseKey(hKey);
    }

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char val[256] = {0};
        DWORD val_sz = sizeof(val);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)val, &val_sz) == ERROR_SUCCESS) {
            cpu = val;
        }
        RegCloseKey(hKey);
    }

    return mg + "::" + cpu + "::" + comp;
}

inline Str get_hardware_id() {
    std::string raw = get_raw_fingerprint();
    auto h = SHA256::hex(SHA256::hash((const uint8_t*)raw.data(), raw.size()));
    for (char& c : h) c = toupper(c);
    std::string h12 = h.substr(0, 12);
    return Str("ACW-" + h12.substr(0, 4) + "-" + h12.substr(4, 4) + "-" + h12.substr(8, 4));
}

inline const std::string SECRET_SALT = "ACW_STUDIO_OFFLINE_PROTECT_2026_xV79_HALVIK";
inline const char SYMBOLS[] = {'$', '!', '?', '%'};
inline const int ODD_DIGITS[] = {1, 3, 5, 7, 9};

inline Str generate_key(const Str& hwid) {
    std::string clean_id = hwid;
    for (char& c : clean_id) c = toupper(c);

    auto h2 = SHA256::hex(hmac_sha256(SECRET_SALT, clean_id + "_P2"));
    for (char& c : h2) c = toupper(c);
    std::string part2 = h2.substr(0, 15);

    auto h_seq = hmac_sha256(SECRET_SALT, clean_id + "_SEQ");
    char s1 = SYMBOLS[h_seq[0] % 4];
    int d1 = ODD_DIGITS[h_seq[1] % 5];
    char l1 = (char)('A' + (h_seq[2] % 26));

    char s2 = SYMBOLS[h_seq[3] % 4];
    int step1 = ((h_seq[4] % 4) * 2 + 2);
    int d2 = (d1 + step1) % 10;
    if (d2 % 2 == 0) d2 = (d2 + 1) % 10;
    char l2 = (char)('A' + (h_seq[5] % 26));

    char s3 = SYMBOLS[h_seq[6] % 4];
    int step2 = ((h_seq[7] % 4) * 2 + 2);
    int d3 = (d2 + step2) % 10;
    if (d3 % 2 == 0) d3 = (d3 + 1) % 10;
    char l3 = (char)('A' + (h_seq[8] % 26));

    std::stringstream cycle_ss;
    cycle_ss << s1 << d1 << l1 << s2 << d2 << l2 << s3 << d3 << l3;
    std::string cycle = cycle_ss.str();

    auto h_tail = hmac_sha256(SECRET_SALT, clean_id + "_TAIL");
    std::vector<int> d_src;
    int sum_d = 0;
    for (int i = 0; i < 5; ++i) {
        int d = (int)h_tail[i] % 10;
        d_src.push_back(d);
        sum_d += d;
    }
    int rem = sum_d % 3;
    int last_d = (3 - rem) % 3;
    if ((h_tail[5] % 2 == 1) && (last_d + 3 <= 9)) {
        last_d += 3;
    }
    d_src.push_back(last_d);

    std::string tail = "";
    for (int d : d_src) tail += std::to_string(d);

    return Str("KEY-" + part2 + "-" + cycle + "-ACW-LICENSE-!" + tail);
}

inline Bool verify_key(const Str& key, const Str& hwid = "") {
    if (key.empty()) return false;
    std::string clean_k = key;
    while (!clean_k.empty() && (clean_k.back() == '\r' || clean_k.back() == '\n' || clean_k.back() == ' ')) clean_k.pop_back();
    std::string target_hwid = hwid.empty() ? get_hardware_id() : std::string(hwid);
    std::string expected = generate_key(target_hwid);
    return (clean_k == expected);
}

inline Str get_license_path(const Str& dir_hint = "") {
    std::string p1 = (dir_hint.empty() ? "." : std::string(dir_hint)) + "\\license.key";
    if (std::filesystem::exists(p1)) return Str(p1);

    char appdata[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("LOCALAPPDATA", appdata, sizeof(appdata))) {
        std::string p2 = std::string(appdata) + "\\Programs\\AudioCoverWatcher\\license.key";
        if (std::filesystem::exists(p2)) return Str(p2);
        std::string p3 = std::string(appdata) + "\\Programs\\AudioCoverWatcherViss\\license.key";
        if (std::filesystem::exists(p3)) return Str(p3);
    }
    return Str(p1);
}

inline Bool is_activated(const Str& dir_hint = "") {
    Str p = get_license_path(dir_hint);
    if (!std::filesystem::exists(p)) return false;
    std::ifstream f(p);
    if (!f.is_open()) return false;
    std::string key;
    std::getline(f, key);
    return verify_key(key);
}

inline Bool save_license(const Str& key, const Str& target_dir = "") {
    if (!verify_key(key)) return false;
    std::string main_path = "";
    if (!target_dir.empty()) {
        main_path = std::string(target_dir) + "\\license.key";
    } else {
        char appdata[MAX_PATH] = {0};
        if (GetEnvironmentVariableA("LOCALAPPDATA", appdata, sizeof(appdata))) {
            std::string p_app = std::string(appdata) + "\\Programs\\AudioCoverWatcher";
            if (std::filesystem::exists(p_app)) {
                main_path = p_app + "\\license.key";
            }
        }
        if (main_path.empty()) main_path = ".\\license.key";
    }

    std::filesystem::create_directories(std::filesystem::path(main_path).parent_path());
    std::ofstream f(main_path);
    if (f.is_open()) {
        f << key;
        f.close();
    }

    // Also sync to AppData if target_dir was specified separately
    char appdata[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("LOCALAPPDATA", appdata, sizeof(appdata))) {
        std::string p_app = std::string(appdata) + "\\Programs\\AudioCoverWatcher\\license.key";
        if (p_app != main_path && std::filesystem::exists(std::string(appdata) + "\\Programs\\AudioCoverWatcher")) {
            std::ofstream f2(p_app);
            if (f2.is_open()) {
                f2 << key;
                f2.close();
            }
        }
    }
    return true;
}

inline Bool verify(const Str& key, const Str& hwid = "") {
    return verify_key(key, hwid);
}

inline Bool save(const Str& key, const Str& target_dir = "") {
    return save_license(key, target_dir);
}

inline Bool remove_license(const Str& target_dir = "") {
    Str p = get_license_path(target_dir);
    std::error_code ec;
    return std::filesystem::remove(p, ec);
}

inline Bool remove(const Str& target_dir = "") {
    return remove_license(target_dir);
}

inline Str get_hwid() {
    return get_hardware_id();
}

inline Str get_key(const Str& hwid) {
    return generate_key(hwid);
}

} // namespace license
} // namespace viss
