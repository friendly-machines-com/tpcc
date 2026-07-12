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
//   m_<name>  - Pascal-invisible views that are used by the compiler
// Anything else in this namespace is implementation detail and not reachable
// from Pascal source.
#pragma once

// FIXME: Probably shouldn't NUL terminate shortstrings.

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <cstddef> // for std::byte

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
static_assert(sizeof(t_ptrint) == 8);
static_assert(sizeof(t_ptruint) == 8);
static_assert(sizeof(t_sizeint) == 8);
static_assert(sizeof(t_sizeuint) == 8);
static_assert(std::is_signed_v<t_ptrint>);
static_assert(std::is_unsigned_v<t_ptruint>);
static_assert(std::is_signed_v<t_sizeint>);
static_assert(std::is_unsigned_v<t_sizeuint>);
enum t_boolean : uint8_t {
	p_false = false,
	p_true = true,
};
static_assert(sizeof(t_byte) == 1);
static_assert(sizeof(t_shortint) == 1);
static_assert(sizeof(t_word) == 2);
static_assert(sizeof(t_smallint) == 2);
static_assert(sizeof(t_longword) == 4);
static_assert(sizeof(t_integer) == 4);
static_assert(sizeof(t_longint) == 4);
static_assert(sizeof(t_qword) == 8);
static_assert(sizeof(t_int64) == 8);
static_assert(sizeof(t_boolean) == 1);

// FPC's Char is an unsigned 8-bit ordinal, but it is nominally distinct from
// Byte. A wrapper preserves both facts in C++ overloads while remaining an
// inline, trivially-copyable one-byte value suitable for ShortString storage.
struct t_char {
	uint8_t value;

	constexpr t_char() = default;
	constexpr t_char(uint8_t value) : value(value) {}
	constexpr operator uint8_t() const { return value; }
};
static_assert(sizeof(t_char) == 1);
static_assert(alignof(t_char) == 1);
static_assert(std::is_trivially_copyable_v<t_char>);
using tpcc_unknown_type = void*;

inline t_boolean tpcc_bool_to_boolean(bool value) {
	return value ? p_true : p_false;
}

struct t_shortstring {
	t_char length;
	t_char data[255];
};

struct tpcc_storage_ref {
	std::byte* data;
	std::size_t size;
};

// Inside an emitted Pascal routine, an omitted-type formal contains only the
// storage address and remaining byte extent. At a call site, the builders
// return a derived view that also retains the selected C++ value type, allowing
// typed RTL templates to use the value without changing the storage interface.
struct tpcc_const_storage_ref {
	const std::byte* data;
	std::size_t size;
};

template<typename T>
struct tpcc_typed_storage_ref : tpcc_storage_ref {
	T* value;
};

template<typename T>
struct tpcc_typed_const_storage_ref : tpcc_const_storage_ref {
	const T* value;
};

template<typename T>
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(T& value) {
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(value)),
	        sizeof(T),
	    },
	    std::addressof(value),
	};
}

// Permit assignment through an explicit ordinal cast when the source and
// target carriers have equal size. Do not form a C++ reference to the target
// type: no target object exists in SOURCE's storage. Instead construct a real
// SOURCE value with the target representation and assign it through SOURCE's
// correctly typed pointer. The parser admits only intrinsic ordinal carriers;
// these static assertions keep the emitted contract independently auditable.
template<typename Target, typename Source>
inline void tpcc_store_writable_cast(
    tpcc_typed_storage_ref<Source> destination, Target value) {
	static_assert(
	    sizeof(Target) == sizeof(Source),
	    "writable Pascal cast requires equal-size carriers");
	static_assert(
	    std::is_trivially_copyable_v<Target>,
	    "writable Pascal cast target must be trivially copyable");
	static_assert(
	    std::is_trivially_copyable_v<Source>,
	    "writable Pascal cast source must be trivially copyable");
	if (destination.size < sizeof(Source))
		throw std::length_error(
		    "writable Pascal cast exceeds its storage view");
	*destination.value = std::bit_cast<Source>(value);
}

template<typename T>
inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(
    const T& value) {
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(value)),
	        sizeof(T),
	    },
	    std::addressof(value),
	};
}

inline tpcc_storage_ref tpcc_make_storage_ref(tpcc_storage_ref value) {
	return value;
}

inline tpcc_const_storage_ref tpcc_make_const_storage_ref(
    tpcc_const_storage_ref value) {
	return value;
}

inline tpcc_const_storage_ref tpcc_make_const_storage_ref(
    tpcc_storage_ref value) {
	return tpcc_const_storage_ref{
	    value.data,
	    value.size,
	};
}

struct tpcc_set_span {
	int64_t lower;
	int64_t upper;
};

template<typename T>
struct t_set {
	std::vector<tpcc_set_span> spans;
};

template<typename T>
inline int64_t tpcc_set_key(T value) {
	if constexpr (std::is_same_v<T, t_char>)
		return static_cast<int64_t>(value.value);
	else
		return static_cast<int64_t>(value);
}

template<typename T>
inline tpcc_set_span tpcc_set_single(T value) {
	const int64_t key = tpcc_set_key(value);
	return tpcc_set_span{key, key};
}

template<typename T>
inline tpcc_set_span tpcc_set_range(T lower, T upper) {
	return tpcc_set_span{tpcc_set_key(lower), tpcc_set_key(upper)};
}

template<typename T>
inline t_set<T> tpcc_make_set(std::initializer_list<tpcc_set_span> spans) {
	return t_set<T>{std::vector<tpcc_set_span>(spans)};
}

template<typename Value, typename T>
inline t_boolean p_in(Value value, const t_set<T>& set) {
	const int64_t key = tpcc_set_key(value);
	for (const tpcc_set_span& span : set.spans)
		if (span.lower <= key && key <= span.upper)
			return p_true;
	return p_false;
}

template<typename Value, typename T>
inline t_boolean p_in(tpcc_typed_const_storage_ref<Value> value,
    tpcc_typed_const_storage_ref<t_set<T>> set) {
	return p_in(*value.value, *set.value);
}

// VALUE is intentionally separate from T. The Pascal checker has already
// verified conversion to the set's item type, but an untyped integer literal
// is still emitted with its raw C++ literal carrier at an omitted-type call
// boundary. Convert that carrier here before deriving the ordinal set key.
template<typename T, typename Value>
inline void p_include(tpcc_typed_storage_ref<t_set<T>> set,
    tpcc_typed_const_storage_ref<Value> item) {
	const T converted = static_cast<T>(*item.value);
	const int64_t key = tpcc_set_key(converted);
	// t_set membership is the union of its spans; the carrier does not require
	// canonical or disjoint spans. Appending a singleton is therefore a
	// complete Include operation. Exclude below removes KEY from every span.
	set.value->spans.push_back(tpcc_set_span{key, key});
}

template<typename T, typename Value>
inline void p_exclude(tpcc_typed_storage_ref<t_set<T>> set,
    tpcc_typed_const_storage_ref<Value> item) {
	const T converted = static_cast<T>(*item.value);
	const int64_t key = tpcc_set_key(converted);
	std::vector<tpcc_set_span> remaining;
	remaining.reserve(set.value->spans.size() + 1);
	for (const tpcc_set_span& span : set.value->spans) {
		if (key < span.lower || key > span.upper) {
			remaining.push_back(span);
			continue;
		}
		// Strict comparisons make the +/- 1 operations safe even at the
		// int64_t endpoints. Remove KEY from every overlapping span so a
		// duplicate Include followed by Exclude still has Pascal set
		// semantics rather than multiset semantics.
		if (span.lower < key)
			remaining.push_back(tpcc_set_span{span.lower, key - 1});
		if (key < span.upper)
			remaining.push_back(tpcc_set_span{key + 1, span.upper});
	}
	set.value->spans = std::move(remaining);
}

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

// Pascal indexing is always emitted as an RTL call. The compiler never needs
// to know a container's C++ representation or lower bound.
template<typename T, std::size_t length, auto low, typename I>
inline T& p_index(t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		throw std::out_of_range("Pascal fixed-array index out of range");
	return value.items[static_cast<std::size_t>(actual - first)];
}

template<typename T, std::size_t length, auto low, typename I>
inline const T& p_index(const t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		throw std::out_of_range("Pascal fixed-array index out of range");
	return value.items[static_cast<std::size_t>(actual - first)];
}

template<typename T, std::size_t length, auto low, typename I>
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(
    t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		throw std::out_of_range("Pascal fixed-array storage index out of range");
	const std::size_t offset = static_cast<std::size_t>(actual - first);
	auto* bytes = reinterpret_cast<std::byte*>(std::addressof(value.items));
	return tpcc_typed_storage_ref<T>{
	    {
	        bytes + offset * sizeof(T),
	        (length - offset) * sizeof(T),
	    },
	    std::addressof(value.items[offset]),
	};
}

template<typename T, std::size_t length, auto low, typename I>
inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(
    const t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		throw std::out_of_range("Pascal fixed-array storage index out of range");
	const std::size_t offset = static_cast<std::size_t>(actual - first);
	const auto* bytes =
	    reinterpret_cast<const std::byte*>(std::addressof(value.items));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        bytes + offset * sizeof(T),
	        (length - offset) * sizeof(T),
	    },
	    std::addressof(value.items[offset]),
	};
}

template<typename I>
inline t_char& p_index(t_shortstring& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > 255)
		throw std::out_of_range("Pascal ShortString index out of range");
	if (actual == 0)
		return value.length;
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template<typename I>
inline const t_char& p_index(const t_shortstring& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > 255)
		throw std::out_of_range("Pascal ShortString index out of range");
	if (actual == 0)
		return value.length;
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template<typename I>
inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(
    t_shortstring& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > 255)
		throw std::out_of_range("Pascal ShortString storage index out of range");
	if (actual == 0)
		return tpcc_typed_storage_ref<t_char>{
		    {
		        reinterpret_cast<std::byte*>(std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	const std::size_t offset = static_cast<std::size_t>(actual - 1);
	auto* bytes = reinterpret_cast<std::byte*>(std::addressof(value.data));
	return tpcc_typed_storage_ref<t_char>{
	    {
	        bytes + offset * sizeof(t_char),
	        (sizeof(value.data) - 1) - offset,
	    },
	    std::addressof(value.data[offset]),
	};
}

template<typename I>
inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(
    const t_shortstring& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > 255)
		throw std::out_of_range("Pascal ShortString storage index out of range");
	if (actual == 0)
		return tpcc_typed_const_storage_ref<t_char>{
		    {
		        reinterpret_cast<const std::byte*>(
		            std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	const std::size_t offset = static_cast<std::size_t>(actual - 1);
	const auto* bytes =
	    reinterpret_cast<const std::byte*>(std::addressof(value.data));
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        bytes + offset * sizeof(t_char),
	        (sizeof(value.data) - 1) - offset,
	    },
	    std::addressof(value.data[offset]),
	};
}

template<typename T, typename I>
inline T& p_index(T* value, I index) {
	if (!value)
		throw std::out_of_range("Pascal pointer index through nil");
	return value[static_cast<std::ptrdiff_t>(index)];
}

template<typename T, typename I>
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(T* value, I index) {
	if (!value)
		throw std::out_of_range("Pascal pointer storage index through nil");
	T* selected = value + static_cast<std::ptrdiff_t>(index);
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(selected),
	        std::numeric_limits<std::size_t>::max(),
	    },
	    selected,
	};
}

template<typename T, typename I>
inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(
    const T* value, I index) {
	if (!value)
		throw std::out_of_range("Pascal pointer storage index through nil");
	const T* selected = value + static_cast<std::ptrdiff_t>(index);
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(selected),
	        std::numeric_limits<std::size_t>::max(),
	    },
	    selected,
	};
}

template<typename T>
struct t_dynamicarray {
	int length;
	T* items;
};

// Placeholder carrier for Pascal AnsiString. It is deliberately a distinct C++
// type from t_shortstring so Pascal overloads on string vs AnsiString do not
// collapse, even though this runtime does not implement real managed strings yet.
struct t_ansistring : t_shortstring {};

// Mutation of an AnsiString element must detach shared storage before a
// writable character reference escapes. The temporary inline representation
// is already unique, so this becomes a real copy-on-write barrier when
// t_ansistring acquires managed storage.
inline void p_uniquestring(t_ansistring&) {
}

template<typename I>
inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(
    t_ansistring& value, I index) {
	p_uniquestring(value);
	return tpcc_make_storage_ref(static_cast<t_shortstring&>(value), index);
}

template<typename I>
inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(
    const t_ansistring& value, I index) {
	return tpcc_make_const_storage_ref(
	    static_cast<const t_shortstring&>(value), index);
}

template<typename I>
inline t_char& tpcc_index_write(t_ansistring& value, I index) {
	p_uniquestring(value);
	return p_index(static_cast<t_shortstring&>(value), index);
}

inline t_shortstring tpcc_shortstring_from_c(const char* s, std::size_t length) {
	t_shortstring result{};
	const std::size_t stored_length = std::min(length, sizeof(result.data) - 1);
	result.length = t_char{static_cast<uint8_t>(stored_length)};
	if (stored_length != 0)
		memcpy(result.data, s, stored_length);
	result.data[stored_length] = t_char{0};
	return result;
}

inline t_shortstring p_char_to_shortstring(t_char value) {
	t_shortstring result{};
	result.length = 1;
	result.data[0] = value;
	result.data[1] = 0;
	return result;
}

inline t_char p_chr(t_byte value) {
	return t_char{value};
}

inline void p_fillchar(tpcc_storage_ref destination, t_sizeint count, t_byte value) {
	if (count <= 0)
		return;
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > destination.size)
		throw std::out_of_range("FillChar exceeds destination storage");
	std::memset(destination.data, value, byte_count);
}

inline void p_move(tpcc_const_storage_ref source,
    tpcc_storage_ref destination, t_sizeint count) {
	if (count <= 0)
		return;
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > source.size)
		throw std::out_of_range("Move exceeds source storage");
	if (byte_count > destination.size)
		throw std::out_of_range("Move exceeds destination storage");
	std::memmove(destination.data, source.data, byte_count);
}

inline t_sizeint p_comparebyte(tpcc_const_storage_ref first,
    tpcc_const_storage_ref second, t_sizeint count) {
	if (count <= 0)
		return 0;
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > first.size)
		throw std::out_of_range("CompareByte exceeds first buffer");
	if (byte_count > second.size)
		throw std::out_of_range("CompareByte exceeds second buffer");
	const int comparison = std::memcmp(first.data, second.data, byte_count);
	return comparison < 0 ? -1 : comparison > 0 ? 1 : 0;
}

inline t_sizeint p_comparechar(tpcc_const_storage_ref first,
    tpcc_const_storage_ref second, t_sizeint count) {
	return p_comparebyte(first, second, count);
}

inline t_longint p_pos(const t_shortstring& needle, const t_shortstring& haystack) {
	if (needle.length == 0)
		return 1;
	if (needle.length > haystack.length)
		return 0;
	const std::size_t last = static_cast<std::size_t>(haystack.length - needle.length);
	for (std::size_t offset = 0; offset <= last; ++offset) {
		if (std::memcmp(haystack.data + offset, needle.data, needle.length) == 0)
			return static_cast<t_longint>(offset + 1);
	}
	return 0;
}

inline t_longint p_pos(t_char needle, const t_shortstring& haystack) {
	for (std::size_t offset = 0; offset < haystack.length; ++offset) {
		if (haystack.data[offset] == needle)
			return static_cast<t_longint>(offset + 1);
	}
	return 0;
}

// Pascal Copy uses one-based indices. Preserve the RTL's ShortString
// invariant on every return path: at most 254 payload bytes followed by the
// reserved zero terminator.
inline t_shortstring p_copy(const t_shortstring& value, t_longint index, t_longint count) {
	t_shortstring result{};
	if (count <= 0)
		return result;
	if (index < 1)
		index = 1;
	const std::size_t start = static_cast<std::size_t>(index - 1);
	const std::size_t source_length = value.length;
	if (start >= source_length)
		return result;
	const std::size_t requested = static_cast<std::size_t>(count);
	const std::size_t copied = std::min({
	    requested,
	    source_length - start,
	    sizeof(result.data) - 1,
	});
	result.length = t_char{static_cast<uint8_t>(copied)};
	if (copied != 0)
		std::memcpy(result.data, value.data + start, copied);
	result.data[copied] = t_char{0};
	return result;
}

inline t_ansistring p_copy(const t_ansistring& value, t_longint index, t_longint count) {
	t_ansistring result{};
	static_cast<t_shortstring&>(result) =
	    p_copy(static_cast<const t_shortstring&>(value), index, count);
	return result;
}

inline t_shortstring p_copy(t_char value, t_longint index, t_longint count) {
	t_shortstring source{};
	source.length = t_char{1};
	source.data[0] = value;
	source.data[1] = t_char{0};
	return p_copy(source, index, count);
}

inline void p_delete(t_shortstring& value, t_longint index, t_longint count) {
	if (index < 1 || count <= 0)
		return;
	const std::size_t start = static_cast<std::size_t>(index - 1);
	const std::size_t length = value.length;
	if (start >= length)
		return;
	const std::size_t requested = static_cast<std::size_t>(count);
	const std::size_t removed = std::min(requested, length - start);
	const std::size_t tail = length - start - removed;
	std::memmove(value.data + start, value.data + start + removed, tail);
	value.length = static_cast<uint8_t>(length - removed);
	value.data[value.length] = 0;
}

inline void p_delete(t_ansistring& value, t_longint index, t_longint count) {
	p_delete(static_cast<t_shortstring&>(value), index, count);
}

inline void p_insert(const t_shortstring& source, t_shortstring& value, t_longint index) {
	if (source.length == 0)
		return;

	if (index < 1)
		index = 1;

	std::size_t start = static_cast<std::size_t>(index - 1);
	// Pascal Semantics: index > length acts as append
	if (start > value.length)
		start = value.length;

	// Handle potential aliasing (e.g., Pascal's `Insert(S, S, 2)`).
	// Shortstrings are <= 256 bytes, so a stack copy is cheap and prevents memory corruption.
	t_shortstring temp_source;
	const t_shortstring* p_src = &source;
	if (&source == &value) {
		temp_source = source;
		p_src = &temp_source;
	}

	// Truncation logic: Calculate how much of the source we can actually fit
	std::size_t max_insert = 254 - start;
	std::size_t copy_count = std::min(static_cast<std::size_t>(p_src->length), max_insert);

	if (copy_count == 0)
		return;

	std::size_t max_tail = 254 - (start + copy_count);
	std::size_t tail = value.length - start;
	std::size_t tail_copy = std::min(tail, max_tail);

	// Shift existing characters to the right to make room (truncates the tail if max_tail is reached)
	if (tail_copy > 0) {
		std::memmove(value.data + start + copy_count, value.data + start, tail_copy);
	}

	// Copy the new characters from the source string into the gap
	std::memcpy(value.data + start, p_src->data, copy_count);

	// Update length and null-terminate
	value.length = static_cast<uint8_t>(start + copy_count + tail_copy);
	value.data[value.length] = 0;
}

inline void p_insert(t_char source, t_shortstring& destination, t_longint index) {
	t_shortstring one_character{};
	one_character.length = 1;
	one_character.data[0] = source;
	one_character.data[1] = 0;
	p_insert(one_character, destination, index);
}

inline void p_insert(const t_ansistring& source, t_ansistring& destination, t_longint index) {
	p_insert(static_cast<const t_shortstring&>(source), static_cast<t_shortstring&>(destination), index);
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

inline int tpcc_stringcmp(const t_shortstring& a, const t_shortstring& b) {
	int r = memcmp(a.data, b.data, std::min(a.length, b.length));
	if (r == 0) {
		return (int) a.length - (int) b.length;
	}
	return r;
}

inline t_boolean p_lessthan(const t_shortstring& a, const t_shortstring& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) < 0);
}

inline t_boolean p_lessthanorequal(const t_shortstring& a, const t_shortstring& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) <= 0);
}

inline t_boolean p_equal(const t_shortstring& a, const t_shortstring& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) == 0);
}

inline t_boolean p_greaterthan(const t_shortstring& a, const t_shortstring& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) > 0);
}

inline t_boolean p_greaterthanorequal(const t_shortstring& a, const t_shortstring& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) >= 0);
}

inline t_char p_assign(t_char value) { return value; }
inline t_ansistring p_assign(t_shortstring value) {
	t_ansistring result{};
	static_cast<t_shortstring&>(result) = value;
	return result;
}
inline t_boolean p_lessthan(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value < b.value); }
inline t_boolean p_lessthanorequal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value <= b.value); }
inline t_boolean p_equal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value == b.value); }
inline t_boolean p_greaterthan(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value > b.value); }
inline t_boolean p_greaterthanorequal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value >= b.value); }

template<typename T> inline t_longword p_ord(T x) { return static_cast<t_longword>(x); }

template<typename T>
inline t_longword p_ord(tpcc_typed_const_storage_ref<T> x) {
	return p_ord(*x.value);
}
template<typename T> inline T p_low() {
	if constexpr (std::is_same_v<T, t_char>)
		return t_char{0};
	else
		return std::numeric_limits<T>::lowest();
}
template<typename T> inline T p_high() {
	if constexpr (std::is_same_v<T, t_char>)
		return t_char{255};
	else
		return std::numeric_limits<T>::max();
}
inline t_sizeint p_length(const t_shortstring& s) { return s.length; }
inline t_sizeint p_length(const t_ansistring& s) { return s.length; }
inline void p_setlength(t_ansistring& s, t_integer value) {
	const std::size_t old_length = s.length;
	const std::size_t new_length =
	    value <= 0 ? 0 : std::min<std::size_t>(
		static_cast<std::size_t>(value), sizeof(s.data) - 1);
	if (new_length > old_length)
		std::fill(s.data + old_length, s.data + new_length, t_char{0});
	s.length = t_char{static_cast<uint8_t>(new_length)};
	s.data[new_length] = t_char{0};
}
template<typename T, std::size_t N> inline t_sizeint p_length(const T (&)[N]) { return static_cast<t_sizeint>(N); }
template<typename T, std::size_t N, auto Low> inline t_sizeint p_length(const t_fixedarray<T, N, Low>&) { return static_cast<t_sizeint>(N); }
template<typename T>
inline t_sizeint p_length(tpcc_typed_const_storage_ref<T> value) {
	return p_length(*value.value);
}
template<typename T>
inline t_sizeint p_sizeof(tpcc_typed_const_storage_ref<T>) {
	return static_cast<t_sizeint>(sizeof(T));
}

#define TPCC_DEFINE_ARITHMETIC_OPERATIONS(T, DIV_RESULT) \
	inline T p_add(T a, T b) { return a + b; } \
	inline T p_subtract(T a, T b) { return a - b; } \
	inline T p_positive(T b) { return +b; } \
	/* For unsigned T, unary minus wraps modulo T's range; this is intentional RTL behavior, not a widening or signed conversion. */ \
	inline T p_negative(T b) { return -b; } \
	inline T p_multiply(T a, T b) { return a * b; } \
	inline DIV_RESULT p_divide(T a, T b) { return static_cast<DIV_RESULT>(a) / static_cast<DIV_RESULT>(b); } \
	inline T p_assign(T source) { T target = source; return target; } \
	inline t_boolean p_lessthan(T a, T b) { return tpcc_bool_to_boolean(a < b); } \
	inline t_boolean p_lessthanorequal(T a, T b) { return tpcc_bool_to_boolean(a <= b); } \
	inline t_boolean p_equal(T a, T b) { return tpcc_bool_to_boolean(a == b); } \
	inline t_boolean p_greaterthan(T a, T b) { return tpcc_bool_to_boolean(a > b); } \
	inline t_boolean p_greaterthanorequal(T a, T b) { return tpcc_bool_to_boolean(a >= b); }

#define TPCC_DEFINE_INTEGER_OPERATIONS(T) \
	inline T p_bitwiseand(T a, T b) { return a & b; } \
	inline T p_bitwiseor(T a, T b) { return a | b; } \
	inline T p_bitwisexor(T a, T b) { return a ^ b; } \
	inline T p_intdivide(T a, T b) { return a / b; } \
	inline T p_modulus(T a, T b) { return a % b; } \
	inline T p_leftshift(T a, T b) { return a << b; } /* FIXME: b smaller */ \
	inline T p_rightshift(T a, T b) { return a >> b; } /* FIXME: b smaller */

#define TPCC_DEFINE_INTEGRAL_OPERATIONS(T) \
	TPCC_DEFINE_ARITHMETIC_OPERATIONS(T, t_double) \
	TPCC_DEFINE_INTEGER_OPERATIONS(T)

TPCC_DEFINE_INTEGRAL_OPERATIONS(t_byte)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_shortint)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_word)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_smallint)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_longword)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_int64)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_qword)
TPCC_DEFINE_ARITHMETIC_OPERATIONS(t_double, t_double)
TPCC_DEFINE_ARITHMETIC_OPERATIONS(t_extended, t_extended)

// Floating-to-integer conversion is undefined in C++ when the finite value is
// outside the destination range (and for NaN/infinity). Check before casting
// so Pascal Trunc/Round never rely on C++ undefined behavior.
inline t_int64 tpcc_checked_real_to_int64(t_extended value, const char* operation) {
	constexpr t_extended limit = 0x1p63L;
	if (!__builtin_isfinite(value) || value < -limit || value >= limit)
		throw std::range_error(operation);
	return static_cast<t_int64>(value);
}

inline t_int64 p_trunc(t_extended value) {
	return tpcc_checked_real_to_int64(::truncl(value), "Trunc result is outside Int64 range");
}

inline t_int64 p_round(t_extended value) {
	// Pascal Round follows the active floating-point rounding mode. nearbyint
	// does likewise and therefore gives ties-to-even under the default mode.
	return tpcc_checked_real_to_int64(::nearbyintl(value), "Round result is outside Int64 range");
}

inline t_extended p_frac(t_extended value) {
	t_extended integral = 0.0L;
	return ::modfl(value, &integral);
}

inline t_extended p_sqrt(t_extended value) { return ::sqrtl(value); }
inline t_extended p_exp(t_extended value) { return ::expl(value); }
inline t_extended p_ln(t_extended value) { return ::logl(value); }

template<typename T>
constexpr auto tpcc_for_ordinal_value(T value) {
	if constexpr (std::is_enum_v<T>)
		return static_cast<std::underlying_type_t<T>>(value);
	else
		return value;
}

template<typename T>
constexpr t_boolean tpcc_for_less_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) <= tpcc_for_ordinal_value(b));
}

template<typename T>
constexpr t_boolean tpcc_for_greater_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) >= tpcc_for_ordinal_value(b));
}

template<typename T>
constexpr t_boolean tpcc_for_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) == tpcc_for_ordinal_value(b));
}

template<typename T>
constexpr T tpcc_for_succ(T value) {
	return static_cast<T>(tpcc_for_ordinal_value(value) + 1);
}

template<typename T>
constexpr T tpcc_for_pred(T value) {
	return static_cast<T>(tpcc_for_ordinal_value(value) - 1);
}

inline t_boolean p_logicalnot(t_boolean a) {
	return tpcc_bool_to_boolean(!a);
}

inline t_boolean p_logicalxor(t_boolean a, t_boolean b) {
	return tpcc_bool_to_boolean(((a != 0) ^ (b != 0)) != 0);
}

inline t_boolean p_assign(t_boolean b) {
	return b;
}

inline t_boolean p_assigned(const void* p) {
	return tpcc_bool_to_boolean(p != nullptr);
}

template<typename T, bool = std::is_enum_v<T>>
struct tpcc_ordinal_raw {
	using type = T;
};

template<typename T>
struct tpcc_ordinal_raw<T, true> {
	using type = std::underlying_type_t<T>;
};

template<>
struct tpcc_ordinal_raw<t_char, false> {
	using type = uint8_t;
};

template<typename T>
inline T tpcc_ordinal_step(T value, t_integer amount, bool subtract) {
	using raw_type = typename tpcc_ordinal_raw<T>::type;
	static_assert(std::is_integral_v<raw_type>, "Inc/Dec require an ordinal carrier");
	using unsigned_type = std::make_unsigned_t<raw_type>;
	unsigned_type bits = static_cast<unsigned_type>(static_cast<raw_type>(value));
	unsigned_type delta = static_cast<unsigned_type>(amount);
	unsigned_type stepped = subtract ? bits - delta : bits + delta;
	raw_type raw;
	if constexpr (std::is_signed_v<raw_type>)
		raw = std::bit_cast<raw_type>(stepped);
	else
		raw = static_cast<raw_type>(stepped);
	return static_cast<T>(raw);
}

template<typename T> inline void p_inc(T& x, t_integer n = 1) {
	x = tpcc_ordinal_step(x, n, false);
}

template<typename T> inline void p_dec(T& x, t_integer n = 1) {
	x = tpcc_ordinal_step(x, n, true);
}

template<typename T>
inline void p_inc(tpcc_typed_storage_ref<T> x, t_integer n = 1) {
	p_inc(*x.value, n);
}

template<typename T>
inline void p_dec(tpcc_typed_storage_ref<T> x, t_integer n = 1) {
	p_dec(*x.value, n);
}

template<typename T>
requires std::is_integral_v<T>
inline void p_str(T x, t_shortstring& s) {
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

inline void p_str(t_extended x, t_shortstring& s) {
	// Str(Extended, ...) uses all 21 significant decimal digits of an 80-bit
	// Extended and always emits four exponent digits.
	if (!__builtin_isfinite(x)) {
		char formatted[29];
		const bool nan = __builtin_isnan(x);
		const std::size_t spaces = nan ? 26 : 25;
		memset(formatted, ' ', spaces);
		char* output = formatted + spaces;
		if (!nan)
			*output++ = __builtin_signbit(x) ? '-' : '+';
		memcpy(output, nan ? "Nan" : "Inf", 3);
		s = tpcc_shortstring_from_c(formatted, sizeof(formatted));
		return;
	}

	char digits[64];
	auto [end, error] = std::to_chars(
	    digits, digits + sizeof(digits), x, std::chars_format::scientific, 20);
	if (error != std::errc())
		throw std::runtime_error("Str could not format Extended value");

	char formatted[64];
	char* output = formatted;
	if (digits[0] != '-')
		*output++ = ' ';

	const char* exponent = std::find(digits, end, 'e');
	if (exponent == end)
		throw std::runtime_error("Str produced malformed Extended output");
	memcpy(output, digits, static_cast<std::size_t>(exponent - digits));
	output += exponent - digits;
	*output++ = 'E';
	*output++ = exponent[1];
	const std::ptrdiff_t exponent_digits = end - (exponent + 2);
	for (std::ptrdiff_t i = exponent_digits; i < 4; ++i)
		*output++ = '0';
	memcpy(output, exponent + 2, static_cast<std::size_t>(exponent_digits));
	output += exponent_digits;

	s = tpcc_shortstring_from_c(
	    formatted, static_cast<std::size_t>(output - formatted));
}

struct tpcc_val_prefix {
	std::size_t position;
	unsigned base;
	bool negative;
};

inline tpcc_val_prefix tpcc_val_parse_prefix(
    const t_shortstring& source, t_integer& code) {
	const std::size_t length = source.length;
	std::size_t position = 0;
	while (position < length &&
	       (source.data[position].value == ' ' || source.data[position].value == '\t'))
		++position;

	bool negative = false;
	if (position < length &&
	    (source.data[position].value == '+' || source.data[position].value == '-')) {
		negative = source.data[position].value == '-';
		++position;
	}

	unsigned base = 10;
	if (position < length) {
		switch (source.data[position].value) {
		case '$':
		case 'x':
		case 'X':
			base = 16;
			++position;
			break;
		case '%':
			base = 2;
			++position;
			break;
		case '&':
			base = 8;
			++position;
			break;
		case '0':
			if (position + 1 < length &&
			    (source.data[position + 1].value == 'x' ||
			     source.data[position + 1].value == 'X')) {
				base = 16;
				position += 2;
			}
			break;
		}
	}

	code = static_cast<t_integer>(position + 1);
	return tpcc_val_prefix{position, base, negative};
}

inline unsigned tpcc_val_digit(uint8_t character) {
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	return 16;
}

template<typename T>
requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
inline void p_val(const t_shortstring& source, T& destination, t_integer& code) {
	destination = 0;
	tpcc_val_prefix prefix = tpcc_val_parse_prefix(source, code);
	const std::size_t length = source.length;
	std::size_t position = prefix.position;
	if (position >= length)
		return;

	using unsigned_type = std::make_unsigned_t<T>;
	constexpr unsigned_type unsigned_max = std::numeric_limits<unsigned_type>::max();
	unsigned_type limit = unsigned_max;
	if constexpr (std::is_signed_v<T>) {
		if (prefix.base == 10 || prefix.negative) {
			const unsigned_type signed_max =
			    static_cast<unsigned_type>(std::numeric_limits<T>::max());
			limit = prefix.negative ? signed_max + 1 : signed_max;
		}
	} else if (prefix.negative) {
		return;
	}

	unsigned_type magnitude = 0;
	bool saw_digit = false;
	for (; position < length; ++position) {
		const uint8_t character = source.data[position].value;
		if (character == 0)
			break;
		const unsigned digit = tpcc_val_digit(character);
		code = static_cast<t_integer>(position + 1);
		if (digit >= prefix.base)
			return;
		const unsigned_type typed_digit = static_cast<unsigned_type>(digit);
		if (magnitude > (limit - typed_digit) / prefix.base)
			return;
		magnitude = static_cast<unsigned_type>(
		    magnitude * static_cast<unsigned_type>(prefix.base) + typed_digit);
		saw_digit = true;
	}
	if (!saw_digit)
		return;

	if constexpr (std::is_signed_v<T>) {
		if (prefix.negative) {
			const unsigned_type minimum_magnitude =
			    static_cast<unsigned_type>(std::numeric_limits<T>::max()) + 1;
			if (magnitude == minimum_magnitude)
				destination = std::numeric_limits<T>::min();
			else
				destination = static_cast<T>(-static_cast<T>(magnitude));
		} else if (prefix.base != 10) {
			destination = std::bit_cast<T>(magnitude);
		} else {
			destination = static_cast<T>(magnitude);
		}
	} else {
		destination = static_cast<T>(magnitude);
	}
	code = 0;
}

template<typename T>
requires std::is_floating_point_v<T>
inline void p_val(const t_shortstring& source, T& destination, t_integer& code) {
	destination = 0;
	const std::size_t length = source.length;
	std::size_t position = 0;
	while (position < length &&
	       (source.data[position].value == ' ' || source.data[position].value == '\t'))
		++position;

	char text[255];
	std::size_t text_length = 0;
	if (position < length && source.data[position].value == '+')
		++position;
	for (std::size_t i = position; i < length; ++i) {
		if (source.data[i].value == 0)
			break;
		text[text_length++] = static_cast<char>(source.data[i].value);
	}
	code = static_cast<t_integer>(position + 1);
	if (text_length == 0)
		return;
	const std::size_t first_digit =
	    text[0] == '-' ? 1 : 0;
	if (first_digit >= text_length ||
	    !((text[first_digit] >= '0' && text[first_digit] <= '9') ||
	      text[first_digit] == '.'))
		return;

	T parsed = 0;
	auto [end, error] = std::from_chars(
	    text, text + text_length, parsed, std::chars_format::general);
	code = static_cast<t_integer>(position + (end - text) + 1);
	if (error != std::errc() || end != text + text_length)
		return;
	destination = parsed;
	code = 0;
}

template<typename T, typename Code>
requires ((std::is_integral_v<T> && (!std::is_same_v<T, bool>)) ||
          std::is_floating_point_v<T>) &&
         std::is_integral_v<Code> && (!std::is_same_v<Code, bool>)
inline void p_val(const t_shortstring& source, T& destination,
    tpcc_typed_storage_ref<Code> code) {
	t_integer parsed_code = 0;
	p_val(source, destination, parsed_code);
	*code.value = static_cast<Code>(parsed_code);
}

template<typename T>
requires (std::is_integral_v<T> && (!std::is_same_v<T, bool>)) ||
         std::is_floating_point_v<T>
inline void p_val(const t_shortstring& source, T& destination) {
	t_integer code = 0;
	p_val(source, destination, code);
}

template<typename T>
requires std::is_signed_v<T> && std::is_integral_v<T>
inline t_shortstring tpcc_octstr_signed(T value, t_byte count) {
	if (count >= sizeof(t_shortstring::data))
		throw std::length_error(
		    "OctStr count exceeds tpcc ShortString payload capacity");

	using unsigned_type = std::make_unsigned_t<T>;
	constexpr unsigned width = std::numeric_limits<unsigned_type>::digits;
	unsigned_type bits = std::bit_cast<unsigned_type>(value);
	const bool negative = value < 0;

	t_shortstring result{};
	result.length = t_char{count};
	for (std::size_t i = count; i != 0; --i) {
		result.data[i - 1] =
		    t_char{static_cast<uint8_t>('0' + (bits & unsigned_type{7}))};
		bits >>= 3;
		if (negative)
			bits |= static_cast<unsigned_type>(
			    ~unsigned_type{0} << (width - 3));
	}
	result.data[count] = t_char{0};
	return result;
}

inline t_shortstring p_octstr(t_longint value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring p_octstr(t_int64 value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring p_octstr(t_qword value, t_byte count) {
	// FPC's QWord overload delegates through Int64, preserving the QWord bit
	// pattern and therefore sign-extending values whose top bit is set.
	return tpcc_octstr_signed(std::bit_cast<t_int64>(value), count);
}

inline t_sizeint p_strlen(const t_char* value) {
	if (!value)
		return 0;
	t_sizeint length = 0;
	while (value[length].value != 0)
		++length;
	return length;
}

template<typename T>
requires std::is_object_v<T> || std::is_void_v<T>
inline void p_getmem(T*& destination, t_ptruint size) {
	destination = static_cast<T*>(std::malloc(static_cast<std::size_t>(size)));
}

inline t_pointer p_getmem(t_ptruint size) {
	return std::malloc(static_cast<std::size_t>(size));
}

inline void p_freemem(t_pointer value, t_ptruint size) {
	(void)size;
	std::free(value);
}

inline t_ptruint p_freemem(t_pointer value) {
	std::free(value);
	return 0;
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
			return tpcc_shortstring_from_c("tobject", strlen("tobject"));
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
