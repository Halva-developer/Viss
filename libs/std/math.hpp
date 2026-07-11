#pragma once
#include "../vissrt.hpp"
#include <cmath>

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
    }
}
