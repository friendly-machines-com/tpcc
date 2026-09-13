#pragma once
#include <limits>
inline double recent_test_nan() { return std::numeric_limits<double>::quiet_NaN(); }
inline double recent_test_infinity() { return std::numeric_limits<double>::infinity(); }
