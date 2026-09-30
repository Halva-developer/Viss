#pragma once
#include "../vissrt.hpp"
#include <cmath>
#include <random>
#include <algorithm>
#include <vector>

namespace viss {
    namespace math {
        inline const Dec PI = 3.14159265358979323846;
        inline const Dec E  = 2.71828182845904523536;

        inline Dec sin(Dec x) { return std::sin(x); }
        inline Dec cos(Dec x) { return std::cos(x); }
        inline Dec tan(Dec x) { return std::tan(x); }
        inline Dec asin(Dec x) { return std::asin(x); }
        inline Dec acos(Dec x) { return std::acos(x); }
        inline Dec atan(Dec x) { return std::atan(x); }
        inline Dec atan2(Dec y, Dec x) { return std::atan2(y, x); }

        inline Dec sqrt(Dec x) { return std::sqrt(x); }
        inline Dec pow(Dec base, Dec exp) { return std::pow(base, exp); }
        inline Dec abs(Dec x) { return std::abs(x); }
        inline Int abs(Int x) { return std::abs(x); }
        inline Dec round(Dec x) { return std::round(x); }
        inline Dec floor(Dec x) { return std::floor(x); }
        inline Dec ceil(Dec x) { return std::ceil(x); }

        inline Dec deg_to_rad(Dec deg) { return deg * (PI / 180.0); }
        inline Dec rad_to_deg(Dec rad) { return rad * (180.0 / PI); }

        template<typename T>
        inline Int sign(T val) {
            return (T(0) < val) - (val < T(0));
        }

        template<typename T, typename MinT, typename MaxT>
        inline T clamp(T val, MinT min_v, MaxT max_v) {
            if (val < (T)min_v) return (T)min_v;
            if (val > (T)max_v) return (T)max_v;
            return val;
        }

        inline Dec lerp(Dec a, Dec b, Dec t) {
            return a + t * (b - a);
        }

        inline Dec map_range(Dec val, Dec in_min, Dec in_max, Dec out_min, Dec out_max) {
            if (in_max - in_min == 0) return out_min;
            return out_min + (val - in_min) * (out_max - out_min) / (in_max - in_min);
        }

        inline Dec distance(Dec x1, Dec y1, Dec x2, Dec y2) {
            Dec dx = x2 - x1;
            Dec dy = y2 - y1;
            return std::sqrt(dx * dx + dy * dy);
        }

        inline Int random_int(Int min_v = 0, Int max_v = 100) {
            if (min_v > max_v) std::swap(min_v, max_v);
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            std::uniform_int_distribution<long long> dis(min_v, max_v);
            return dis(gen);
        }

        inline Dec random_dec(Dec min_v = 0.0, Dec max_v = 1.0) {
            if (min_v > max_v) std::swap(min_v, max_v);
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            std::uniform_real_distribution<double> dis(min_v, max_v);
            return dis(gen);
        }

        struct Vec2 {
            Dec x = 0;
            Dec y = 0;

            Vec2() : x(0), y(0) {}
            Vec2(Dec x_, Dec y_) : x(x_), y(y_) {}

            inline Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
            inline Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
            inline Vec2 operator*(Dec s) const { return Vec2(x * s, y * s); }
            inline Vec2 operator/(Dec s) const { return s != 0 ? Vec2(x / s, y / s) : Vec2(0, 0); }

            inline Dec length() const { return std::sqrt(x * x + y * y); }
            inline Vec2 normalized() const {
                Dec len = length();
                return len > 0 ? Vec2(x / len, y / len) : Vec2(0, 0);
            }
            inline Dec dot(const Vec2& o) const { return x * o.x + y * o.y; }
            inline Dec dist_to(const Vec2& o) const { return (*this - o).length(); }
        };

        // 2D Perlin Noise Generator
        namespace detail_noise {
            inline double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
            inline double grad(int hash, double x, double y) {
                int h = hash & 7;
                double u = h < 4 ? x : y;
                double v = h < 4 ? y : x;
                return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
            }
        }

        inline Dec noise2d(Dec x, Dec y) {
            static const int p[512] = {
                151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
                8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
                35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
                134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
                55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
                18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
                250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
                189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
                172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
                228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
                107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
                138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
            };
            int X = (int)std::floor(x) & 255;
            int Y = (int)std::floor(y) & 255;
            x -= std::floor(x);
            y -= std::floor(y);
            double u = detail_noise::fade(x);
            double v = detail_noise::fade(y);
            int A = p[X] + Y, B = p[X + 1] + Y;
            return lerp(
                lerp(detail_noise::grad(p[A], x, y), detail_noise::grad(p[B], x - 1, y), u),
                lerp(detail_noise::grad(p[A + 1], x, y - 1), detail_noise::grad(p[B + 1], x - 1, y - 1), u),
                v
            );
        }
    }
}
