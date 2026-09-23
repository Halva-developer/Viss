#pragma once
#include "../vissrt.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace viss {
    namespace math {
        inline const Dec PI = 3.14159265358979323846;
        inline const Dec E  = 2.71828182845904523536;

        inline Dec sin(Dec x) { return std::sin(x); }
        inline Dec cos(Dec x) { return std::cos(x); }
        inline Dec tan(Dec x) { return std::tan(x); }
        inline Dec sqrt(Dec x) { return std::sqrt(x); }
        inline Dec pow(Dec base, Dec exp) { return std::pow(base, exp); }
        inline Dec abs(Dec x) { return std::abs(x); }
        inline Int abs(Int x) { return std::abs(x); }
        inline Dec round(Dec x) { return std::round(x); }
        inline Dec floor(Dec x) { return std::floor(x); }
        inline Dec ceil(Dec x) { return std::ceil(x); }

        template<typename T, typename MinT, typename MaxT>
        inline T& clamp_in_place(T& val, MinT min_v, MaxT max_v) {
            if (val < (T)min_v) val = (T)min_v;
            if (val > (T)max_v) val = (T)max_v;
            return val;
        }

        template<typename T, typename MinT, typename MaxT>
        inline T clamp(T val, MinT min_v, MaxT max_v) {
            if (val < (T)min_v) return (T)min_v;
            if (val > (T)max_v) return (T)max_v;
            return val;
        }

        inline Int random_int(Int min_v, Int max_v) {
            if (min_v > max_v) std::swap(min_v, max_v);
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            std::uniform_int_distribution<long long> dis(min_v, max_v);
            return dis(gen);
        }

        inline Dec random_dec(Dec min_v, Dec max_v) {
            if (min_v > max_v) std::swap(min_v, max_v);
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            std::uniform_real_distribution<double> dis(min_v, max_v);
            return dis(gen);
        }
    }
}
