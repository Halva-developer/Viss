#pragma once
#include "../vissrt.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstdint>
#include <cstring>

namespace viss {
    namespace crypto {

        // --- Base64 ---
        inline Str base64_encode(const Str& in) {
            static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            Str out;
            int val = 0, valb = -6;
            for (uint8_t c : in) {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back(chars[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            return out;
        }

        inline Str base64_decode(const Str& in) {
            static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T[(unsigned char)chars[i]] = i;

            Str out;
            int val = 0, valb = -8;
            for (uint8_t c : in) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }

        // --- CRC32 ---
        inline uint32_t crc32_raw(const uint8_t* data, size_t length) {
            uint32_t crc = 0xFFFFFFFF;
            for (size_t i = 0; i < length; ++i) {
                uint8_t byte = data[i];
                crc ^= byte;
                for (int j = 0; j < 8; ++j) {
                    crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
                }
            }
            return ~crc;
        }

        inline Int crc32(const Str& text) {
            return (Int)crc32_raw((const uint8_t*)text.data(), text.size());
        }

        inline Str crc32_hex(const Str& text) {
            uint32_t c = crc32_raw((const uint8_t*)text.data(), text.size());
            std::stringstream ss;
            ss << std::hex << std::setfill('0') << std::setw(8) << c;
            return ss.str();
        }

        // --- MD5 ---
        namespace detail_md5 {
            inline uint32_t left_rotate(uint32_t x, uint32_t c) { return (x << c) | (x >> (32 - c)); }
            inline Str compute(const uint8_t* initial_msg, size_t initial_len) {
                static const uint32_t k[64] = {
                    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
                };
                static const uint32_t r[64] = {
                    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
                    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
                    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
                    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
                };
                uint32_t h0 = 0x67452301, h1 = 0xefcdab89, h2 = 0x98badcfe, h3 = 0x10325476;
                size_t new_len = ((((initial_len + 8) / 64) + 1) * 64);
                std::vector<uint8_t> msg(new_len, 0);
                std::memcpy(msg.data(), initial_msg, initial_len);
                msg[initial_len] = 0x80;
                uint64_t bits_len = 8 * (uint64_t)initial_len;
                std::memcpy(msg.data() + new_len - 8, &bits_len, 8);

                for (size_t offset = 0; offset < new_len; offset += 64) {
                    uint32_t *w = (uint32_t*)(msg.data() + offset);
                    uint32_t a = h0, b = h1, c = h2, d = h3;
                    for (uint32_t i = 0; i < 64; ++i) {
                        uint32_t f, g;
                        if (i < 16) { f = (b & c) | ((~b) & d); g = i; }
                        else if (i < 32) { f = (d & b) | ((~d) & c); g = (5 * i + 1) % 16; }
                        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
                        else { f = c ^ (b | (~d)); g = (7 * i) % 16; }
                        uint32_t temp = d;
                        d = c;
                        c = b;
                        b = b + left_rotate(a + f + k[i] + w[g], r[i]);
                        a = temp;
                    }
                    h0 += a; h1 += b; h2 += c; h3 += d;
                }
                std::stringstream ss;
                ss << std::hex << std::setfill('0');
                uint32_t parts[4] = {h0, h1, h2, h3};
                for (int i = 0; i < 4; ++i) {
                    for (int b = 0; b < 4; ++b) {
                        ss << std::setw(2) << ((parts[i] >> (b * 8)) & 0xFF);
                    }
                }
                return ss.str();
            }
        }

        inline Str md5(const Str& text) {
            return detail_md5::compute((const uint8_t*)text.data(), text.size());
        }

        // --- SHA-256 ---
        namespace detail_sha256 {
            inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
            inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
            inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
            inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
            inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
            inline uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
            inline uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

            inline Str compute(const uint8_t* data, size_t len) {
                static const uint32_t K[64] = {
                    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
                    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
                    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
                    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
                    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
                    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
                    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
                    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
                };
                uint32_t H[8] = {
                    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
                };
                uint64_t bit_len = (uint64_t)len * 8;
                size_t padded_len = ((len + 8) / 64 + 1) * 64;
                std::vector<uint8_t> p(padded_len, 0);
                std::memcpy(p.data(), data, len);
                p[len] = 0x80;
                for (int i = 0; i < 8; ++i) {
                    p[padded_len - 1 - i] = (uint8_t)((bit_len >> (i * 8)) & 0xFF);
                }

                for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
                    uint32_t W[64];
                    for (int t = 0; t < 16; ++t) {
                        size_t idx = chunk + t * 4;
                        W[t] = ((uint32_t)p[idx] << 24) | ((uint32_t)p[idx+1] << 16) | ((uint32_t)p[idx+2] << 8) | (uint32_t)p[idx+3];
                    }
                    for (int t = 16; t < 64; ++t) {
                        W[t] = theta1(W[t - 2]) + W[t - 7] + theta0(W[t - 15]) + W[t - 16];
                    }
                    uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
                    for (int t = 0; t < 64; ++t) {
                        uint32_t T1 = h + sig1(e) + ch(e, f, g) + K[t] + W[t];
                        uint32_t T2 = sig0(a) + maj(a, b, c);
                        h = g; g = f; f = e; e = d + T1;
                        d = c; c = b; b = a; a = T1 + T2;
                    }
                    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
                    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
                }
                std::stringstream ss;
                ss << std::hex << std::setfill('0');
                for (int i = 0; i < 8; ++i) {
                    ss << std::setw(8) << H[i];
                }
                return ss.str();
            }
        }

        inline Str sha256(const Str& text) {
            return detail_sha256::compute((const uint8_t*)text.data(), text.size());
        }

        // --- UUID v4 ---
        inline Str uuid() {
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            static std::uniform_int_distribution<uint32_t> dis;

            uint32_t data[4];
            for (int i = 0; i < 4; ++i) data[i] = dis(gen);
            // Set version to 4
            data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
            // Set variant to RFC 4122
            data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            ss << std::setw(8) << data[0] << "-";
            ss << std::setw(4) << (data[1] >> 16) << "-";
            ss << std::setw(4) << (data[1] & 0xFFFF) << "-";
            ss << std::setw(4) << (data[2] >> 16) << "-";
            ss << std::setw(4) << (data[2] & 0xFFFF);
            ss << std::setw(8) << data[3];
            return ss.str();
        }

        inline Bytes random_bytes(Int count) {
            if (count <= 0) return Bytes(0);
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<int> dis(0, 255);
            Bytes b((size_t)count);
            for (Int i = 0; i < count; ++i) {
                b.set(i, (uint8_t)dis(gen));
            }
            return b;
        }
    }
}
