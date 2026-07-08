// Mini-pascal runtime library (header-only).
//
// Emitted C++ references the names in namespace `pas`. The compiler's
// intrinsic-type and builtin-procedure descriptor tables (see builtins.h)
// bind each Pascal name to its rtl counterpart here. Behavior lives here;
// the descriptor tables hold the Pascal-to-rtl name binding.
//
// Naming convention:
//   t_<name>  - a Pascal-visible TYPE
//   p_<name>  - a Pascal-visible value (including procedure or function or operation)
// Anything else in this namespace is implementation detail and not reachable
// from Pascal source.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <cstdio>
#include <limits>

namespace pas {

using t_byte     = uint8_t;
using t_shortint = int8_t;
using t_word     = uint16_t;
using t_smallint = int16_t;
using t_longword = uint32_t;
using t_integer  = int32_t;
using t_longint  = int32_t;
using t_int64 = int64_t;
using t_qword = uint64_t;
using t_pointer = void*;
using t_ptrint = intptr_t;
using t_ptruint = uintptr_t;
using t_sizeint = ssize_t;
using t_sizeuint = size_t;
using t_double = double;
using t_extended = long double;
enum t_boolean {
	p_false = false,
	p_true = true,
};

using t_char     = char;
using unknown_type = void*;

inline t_boolean bool_to_boolean(bool value) {
	return value ? p_true : p_false;
}

struct t_shortstring {
	uint8_t length;
	t_char data[255];
};

struct t_set {
};

template<typename T, std::size_t length, auto low>
struct t_fixedarray {
	T items[length];

	template<typename I>
	constexpr T& operator[](I index) {
		return items[static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low)];
	}

	template<typename I>
	constexpr const T& operator[](I index) const {
		return items[static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low)];
	}
};

template<typename T>
struct t_dynamicarray {
	int length;
	T* items;
};

// Placeholder carrier for Pascal AnsiString. It is deliberately a distinct C++
// type from t_shortstring so Pascal overloads on string vs AnsiString do not
// collapse, even though this runtime does not implement real managed strings yet.
struct t_ansistring : t_shortstring {};

inline t_shortstring tpcc_shortstring_from_c(const char* s) {
	t_shortstring result{};
	result.length = std::min(strlen(s), 254ul);
	memcpy(result.data, s, result.length);
	result.data[result.length] = 0;
	return result;
}

inline t_shortstring p_add(const t_shortstring& a, const t_shortstring& b) {
	t_shortstring result {};
	const std::size_t result_length = std::min<std::size_t>(
	    static_cast<std::size_t>(a.length) + static_cast<std::size_t>(b.length),
	    sizeof(result.data) - 1);
	const std::size_t a_length = std::min<std::size_t>(a.length, result_length);
	const std::size_t b_length = result_length - a_length;
	result.length = static_cast<uint8_t>(result_length);
	memcpy(result.data, a.data, a_length);
	memcpy(&result.data[a_length], b.data, b_length);
	result.data[result.length] = 0;
	return result;
}

inline int stringcmp(const t_shortstring& a, const t_shortstring& b) {
	int r = memcmp(a.data, b.data, std::min(a.length, b.length));
	if (r == 0) {
		return (int) a.length - (int) b.length;
	}
	return r;
}

inline t_boolean p_lessthan(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) < 0);
}

inline t_boolean p_lessthanorequal(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) <= 0);
}

inline t_boolean p_equal(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) == 0);
}

inline t_boolean p_notequal(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) != 0);
}

inline t_boolean p_greaterthan(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) > 0);
}

inline t_boolean p_greaterthanorequal(const t_shortstring& a, const t_shortstring& b) {
	return bool_to_boolean(stringcmp(a, b) >= 0);
}

template<typename T> inline t_integer p_ord(T x) { return static_cast<t_integer>(x); }
template<typename T> inline T p_low() { return std::numeric_limits<T>::lowest(); }
template<typename T> inline T p_high() { return std::numeric_limits<T>::max(); }
inline t_integer p_length(const t_shortstring& s) { return s.length; }
inline t_integer p_length(const t_ansistring& s) { return s.length; }
template<typename T, std::size_t N> inline t_integer p_length(const T (&)[N]) { return static_cast<t_integer>(N); }
template<typename T, std::size_t N, auto Low> inline t_integer p_length(const t_fixedarray<T, N, Low>&) { return static_cast<t_integer>(N); }

#define DEFINE_ARITHMETIC_OPERATIONS(T) \
	inline T p_add(T a, T b) { return a + b; } \
	inline T p_subtract(T a, T b) { return a - b; } \
	inline T p_positive(T b) { return +b; } \
	/* For unsigned T, unary minus wraps modulo T's range; this is intentional RTL behavior, not a widening or signed conversion. */ \
	inline T p_negative(T b) { return -b; } \
	inline T p_multiply(T a, T b) { return a * b; } \
	inline double p_divide(T a, T b) { return (double) a / (double) b; } \
	inline T p_assign(T source) { T target = source; return target; } \
	inline t_boolean p_lessthan(T a, T b) { return bool_to_boolean(a < b); } \
	inline t_boolean p_lessthanorequal(T a, T b) { return bool_to_boolean(a <= b); } \
	inline t_boolean p_equal(T a, T b) { return bool_to_boolean(a == b); } \
	inline t_boolean p_notequal(T a, T b) { return bool_to_boolean(a != b); } \
	inline t_boolean p_greaterthan(T a, T b) { return bool_to_boolean(a > b); } \
	inline t_boolean p_greaterthanorequal(T a, T b) { return bool_to_boolean(a >= b); }

#define DEFINE_INTEGER_OPERATIONS(T) \
	inline T p_bitwiseand(T a, T b) { return a & b; } \
	inline T p_bitwiseor(T a, T b) { return a | b; } \
	inline T p_bitwisexor(T a, T b) { return a ^ b; } \
	inline T p_intdivide(T a, T b) { return a / b; } \
	inline T p_modulus(T a, T b) { return a % b; } \
	inline T p_leftshift(T a, T b) { return a << b; } /* FIXME: b smaller */ \
	inline T p_rightshift(T a, T b) { return a >> b; } /* FIXME: b smaller */

#define DEFINE_INTEGRAL_OPERATIONS(T) \
	DEFINE_ARITHMETIC_OPERATIONS(T) \
	DEFINE_INTEGER_OPERATIONS(T)

DEFINE_INTEGRAL_OPERATIONS(t_byte)
DEFINE_INTEGRAL_OPERATIONS(t_shortint)
DEFINE_INTEGRAL_OPERATIONS(t_word)
DEFINE_INTEGRAL_OPERATIONS(t_smallint)
DEFINE_INTEGRAL_OPERATIONS(t_longword)
DEFINE_INTEGRAL_OPERATIONS(t_integer)
DEFINE_INTEGRAL_OPERATIONS(t_int64)
DEFINE_INTEGRAL_OPERATIONS(t_qword)
DEFINE_ARITHMETIC_OPERATIONS(t_double)
DEFINE_ARITHMETIC_OPERATIONS(t_extended)

inline t_boolean p_logicalnot(t_boolean a) {
	return bool_to_boolean(!a);
}

inline t_boolean p_logicalxor(t_boolean a, t_boolean b) {
	return bool_to_boolean(((a != 0) ^ (b != 0)) != 0);
}

inline t_boolean p_assign(t_boolean b) {
	return b;
}

inline t_boolean p_assigned(const void* p) {
	return bool_to_boolean(p != nullptr);
}

template<typename T> inline void p_inc(T& x, t_integer n = 1) { x = p_add(x, static_cast<T>(n)); }
template<typename T> inline void p_dec(T& x, t_integer n = 1) { x = p_subtract(x, static_cast<T>(n)); }

template<typename T> inline void p_str(T x, t_shortstring& s) {
	char buf[128];
	int n;
	if constexpr (std::is_signed_v<T>)
		n = std::snprintf(buf, sizeof(buf), "%lld", (long long)x);
	else
		n = std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)x);
	if (n < 0)
		n = 0;
	if (n > 254)
		n = 254;
	s.length = static_cast<uint8_t>(n);
	memcpy(s.data, buf, s.length);
	s.data[s.length] = 0;
}

#if 0
// Could be generated by compiler, except that we need to know what t_tobject::m_tobject is here.
// It would be possible to declare an interface and use that for defining t_tclass--but that's terrible since we would implement a magical private interface for no reason, and only for the metaclass.
// Worse, the interface methods then would have the exact same problem: How to refer to the actual metaclass--since we very much DO have class vars stored in the latter eventually.
struct m_iobject;
struct t_tobject: public m_iobject {
	struct m_meta: public m_iobject { // = t_tclass maybe
		virtual ~m_meta() = default;
		virtual m_iobject* classtype() {
			static m_meta meta{};
			return &meta;
		}
		virtual t_shortstring p_classname() {
			return tpcc_shortstring_from_c("tobject");
		}
		virtual bool p_inheritsfrom(struct m_iobject* s) {
			return s == this;
		}
		virtual m_iobject* p_classparent() {
			return nullptr;
		}
    };

	//private inline static m_meta meta{};
	//std::unique_ptr<m_iobject> meta = std::make_unique<m_meta>();
	virtual ~t_tobject() = default;
	m_iobject* p_classtype() {
		return m_meta::p_classtype();
	}
	/*not virtual*/ inline static t_shortstring p_classname() {
		return p_classtype()->p_classname();
	}
	/*not virtual*/ inline static bool p_inheritsfrom(struct m_meta* s) {
		return p_classtype()->p_inheritsfrom(s->classtype());
	}
	/*not virtual*/ inline static m_meta* p_classparent() {
		return p_classtype()->p_classparent();
	}
};
#else
/*interface*/ struct m_iobject {
	virtual m_iobject* p_classtype() = 0;
	virtual t_shortstring p_classname() = 0;
	virtual bool p_inheritsfrom(m_iobject* s) = 0;
	virtual m_iobject* p_classparent() = 0;
};

#endif

using t_tclass = m_iobject;

// FIXME: Terrible.  Since I think class(SUPER, IA, IB, IC) ONLY implements IA IB and IC and its supers, regardless of what SUPER implemented.
// FIXME: at least make sure target_interface is actually an interface, for example by missing ::m_meta, or other ways
#define p_supports(instance, target_interface) (dynamic_cast<(target_interface)*>((instance)) != nullptr)

// TODO: Supports(Instance, InterfaceType, InterfaceVar) that assigns InterfaceVar on success (and returns True then)

//#define class_instance_new(X) (new X)

} // namespace pas
