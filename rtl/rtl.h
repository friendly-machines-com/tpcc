// Mini-pascal runtime library (header-only).
//
// Emitted C++ references the names in namespace `pas`. The compiler's
// intrinsic-type and builtin-procedure descriptor tables (see builtins.h)
// bind each Pascal name to its rtl counterpart here. Behavior lives here;
// the descriptor tables hold the Pascal-to-rtl name binding.
//
// Naming convention:
//   t_<name>  - a Pascal-visible TYPE
//   p_<name>  - a Pascal-visible procedure, function, or value
// Anything else in this namespace is implementation detail and not reachable
// from Pascal source.
#pragma once

#include <cstdint>
#include <iostream>

namespace pas {

using t_byte     = uint8_t;
using t_shortint = int8_t;
using t_word     = uint16_t;
using t_smallint = int16_t;
using t_cardinal = uint32_t;
using t_integer  = int32_t;
using t_longint  = int64_t;
using t_boolean  = bool;
using t_char     = char;

struct t_shortstring {
	uint8_t length;
	char data[255];
};

template<typename T> inline void p_inc(T& x, t_integer n = 1) { x += static_cast<T>(n); }
template<typename T> inline void p_dec(T& x, t_integer n = 1) { x -= static_cast<T>(n); }
template<typename T> inline t_integer p_ord(T x) { return static_cast<t_integer>(x); }

inline t_boolean p_and(t_boolean a, t_boolean b) {
	return a && b;
}

inline t_boolean p_or(t_boolean a, t_boolean b) {
	return a || b;
}

inline t_boolean p_not(t_boolean a) {
	return !a;
}

inline t_boolean p_xor(t_boolean a, t_boolean b) {
	return (a != 0) ^ (b != 0);
}

// FIXME: limit to integral types
template<typename T> inline T p_add(T a, T b) {
	return a + b;
}

// FIXME: limit to integral types
template<typename T> inline T p_subtract(T a, T b) {
	return a - b;
}

// FIXME: limit to integral types
template<typename T> inline T p_multiply(T a, T b) {
	return a * b;
}

// FIXME: limit to integral types
template<typename T> inline double p_divide(T a, T b) {
	return (double) a / (double) b;
}

// FIXME: limit to integral types
template<typename T> inline T p_assign(T& target, T source) {
	target = source;
	return target;
}

// FIXME: limit to integral types
template<typename T> inline T p_div(T a, T b) {
    return a / b;
}

// FIXME: limit to integral types
template<typename T> inline T p_mod(T a, T b) {
    return a % b;
}

// FIXME: limit to integral types
template<typename T> inline T p_shl(T a, T b) {
    return a << b;
}

// FIXME: limit to integral types
template<typename T> inline T p_shr(T a, T b) {
    return a >> b;
}

template<typename T> inline t_boolean p_less(T a, T b) {
	return a < b;
}

template<typename T> inline t_boolean p_less_equal(T a, T b) {
	return a <= b;
}

template<typename T> inline t_boolean p_equal(T a, T b) {
	return a == b;
}

template<typename T> inline t_boolean p_not_equal(T a, T b) {
	return a != b;
}

template<typename T> inline t_boolean p_greater(T a, T b) {
	return a > b;
}

template<typename T> inline t_boolean p_greater_equal(T a, T b) {
	return a >= b;
}

} // namespace pas
