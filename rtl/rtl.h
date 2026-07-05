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
using t_longint  = int32_t;
using t_int64 = int64_t;
using t_qword = uint64_t;
using t_pointer = void*;
using t_ptrint = intptr_t;
using t_ptruint = uintptr_t;
enum t_boolean {
	p_false,
	p_true,
};

using t_char     = char;

struct t_shortstring {
	uint8_t length;
	char data[255];
};

template<typename T> inline t_integer p_ord(T x) { return static_cast<t_integer>(x); }

#define DEFINE_OPERATIONS(T) \
	inline T p_bitwiseand(T a, T b) { return a & b; } \
	inline T p_bitwiseor(T a, T b) { return a | b; } \
	inline T p_bitwisexor(T a, T b) { return a ^ b; } \
	inline T p_add(T a, T b) { return a + b; } \
	inline T p_subtract(T a, T b) { return a - b; } \
	inline T p_positive(T b) { return +b; } \
	inline T p_negative(T b) { return -b; } \
	inline T p_multiply(T a, T b) { return a * b; } \
	inline double p_divide(T a, T b) { return (double) a / (double) b; } \
	inline T p_assign(T& target, T source) { target = source; return target; } \
	inline T p_intdivide(T a, T b) { return a / b; } \
	inline T p_modulus(T a, T b) { return a % b; } \
	inline T p_leftshift(T a, T b) { return a << b; } /* FIXME: b smaller */ \
	inline T p_rightshift(T a, T b) { return a >> b; } /* FIXME: b smaller */ \
	inline t_boolean p_lessthan(T a, T b) { return a < b; } \
	inline t_boolean p_lessthanorequal(T a, T b) { return a <= b; } \
	inline t_boolean p_equal(T a, T b) { return a == b; } \
	inline t_boolean p_notequal(T a, T b) { return !(p_equal(a, b)); } \
	inline t_boolean p_greaterthan(T a, T b) { return a > b; } \
	inline t_boolean p_greaterthanorequal(T a, T b) { return a >= b; }

DEFINE_OPERATIONS(t_byte)
DEFINE_OPERATIONS(t_shortint)
DEFINE_OPERATIONS(t_word)
DEFINE_OPERATIONS(t_smallint)
DEFINE_OPERATIONS(t_cardinal)
DEFINE_OPERATIONS(t_integer)
DEFINE_OPERATIONS(t_longint)
DEFINE_OPERATIONS(t_int64)
DEFINE_OPERATIONS(t_qword)

inline t_boolean p_logicaland(t_boolean a, t_boolean b) {
	return a && b;
}

inline t_boolean p_logicalor(t_boolean a, t_boolean b) {
	return a || b;
}

inline t_boolean p_logicalnot(t_boolean a) {
	return !a;
}

inline t_boolean p_logicalxor(t_boolean a, t_boolean b) {
	return (a != 0) ^ (b != 0);
}

inline t_boolean p_assigned(const char* p) {
	return (p != nullptr);
}

template<typename T> inline void p_inc(T& x, t_integer n = 1) { x = p_add(x, static_cast<T>(n)); }
template<typename T> inline void p_dec(T& x, t_integer n = 1) { x = p_subtract(x, static_cast<T>(n)); }

} // namespace pas
