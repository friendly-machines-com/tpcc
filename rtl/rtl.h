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

#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <exception>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <cstdio>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <cstddef> // for std::byte

namespace u_system {

// C++ does not include a function result in overload identity. Pascal
// implicit-conversion selection does include the context-requested
// destination, so generated p_implicit declarations and calls carry this
// otherwise-empty backend parameter. It is not a Pascal formal and the
// conversion remains an ordinary value-returning operation.
template<typename Destination>
struct m_implicit_target {};

// `Fail` is constructor control flow, not a Pascal exception. Pascal except
// handlers catch only tpcc_pascal_exception, so this marker passes through
// them to an allocation or direct-initializer boundary.
struct tpcc_constructor_fail final {};

// Exit/break/continue which cross a protected Pascal try cannot use native
// C++ control transfer: the try suffix is not known until its body has already
// streamed. These private carriers unwind through each crossed try; that try's
// outer catch decrements next_try_depth and either propagates or performs the
// real C++ transfer. They are not Pascal exceptions and are therefore never
// visible to `except`.
template<typename T>
struct tpcc_return_transfer {
	unsigned next_try_depth;
	T value;
};

template<>
struct tpcc_return_transfer<void> {
	unsigned next_try_depth;
};

struct tpcc_loop_transfer {
	unsigned next_try_depth;
	unsigned target_try_depth;
	bool is_break;
};

// One carrier type represents every object raised by Pascal. Root is the
// generated System.TObject carrier; keeping it a template lets rtl.h define
// the mechanism before that generated class is complete. The exception owns
// the Pascal object until a handler completes or rethrows it.
template<typename Root>
class tpcc_pascal_exception {
	std::unique_ptr<Root> raised_object;
	void* raised_address;
	void* raised_frame;

public:
	tpcc_pascal_exception(
	    std::unique_ptr<Root> object,
	    void* address, void* frame)
	    : raised_object(std::move(object)),
	      raised_address(address),
	      raised_frame(frame) {}
	tpcc_pascal_exception(
	    tpcc_pascal_exception&&) noexcept = default;
	tpcc_pascal_exception& operator=(
	    tpcc_pascal_exception&&) noexcept = default;
	tpcc_pascal_exception(
	    const tpcc_pascal_exception&) = delete;
	tpcc_pascal_exception& operator=(
	    const tpcc_pascal_exception&) = delete;

	Root* object() const noexcept {
		return raised_object.get();
	}
	void* address() const noexcept {
		return raised_address;
	}
	void* frame() const noexcept {
		return raised_frame;
	}
	template<typename T>
	T* get_if() const noexcept {
		return dynamic_cast<T*>(raised_object.get());
	}

	std::unique_ptr<Root> release_if(
	    Root* object) noexcept {
		if (raised_object.get() != object)
			return {};
		return std::move(raised_object);
	}
};

// While a Pascal handler executes, its caught carrier is the owner of the
// handler variable's object. `raise E` is an explicit new raise, but E may
// still designate that same object (directly or through another variable).
// Transfer its existing ownership into the new carrier instead of giving the
// same raw pointer two owners. The linked stack also handles an outer handler
// object raised from inside a nested handler.
template<typename Root>
class tpcc_pascal_exception_scope {
	tpcc_pascal_exception<Root>& exception;
	tpcc_pascal_exception_scope* previous;
	inline static thread_local
	    tpcc_pascal_exception_scope* active = nullptr;

public:
	explicit tpcc_pascal_exception_scope(
	    tpcc_pascal_exception<Root>& exception) noexcept
	    : exception(exception), previous(active) {
		active = this;
	}
	~tpcc_pascal_exception_scope() {
		active = previous;
	}
	tpcc_pascal_exception_scope(
	    const tpcc_pascal_exception_scope&) = delete;
	tpcc_pascal_exception_scope& operator=(
	    const tpcc_pascal_exception_scope&) = delete;

	static std::unique_ptr<Root> take_if_active(
	    Root* object) noexcept {
		for (auto* scope = active; scope;
		     scope = scope->previous) {
			auto owned =
			    scope->exception.release_if(object);
			if (owned)
				return owned;
		}
		return {};
	}
};

template<typename Root>
[[noreturn]] inline void m_raise_pascal(
    Root* object, void* address, void* frame) {
	auto ownership =
	    tpcc_pascal_exception_scope<Root>::
	        take_if_active(object);
	if (!ownership)
		ownership.reset(object);
	throw tpcc_pascal_exception<Root>{
	    std::move(ownership), address, frame};
}

// Keep target-specific call-site recovery in one helper. noinline makes the
// return address the generated Pascal raise site on the supported GCC/Clang
// SysV target instead of an inlined point inside its containing expression.
template<typename Root>
[[noreturn, gnu::noinline]] inline void m_raise_pascal(
    Root* object) {
	void* frame = __builtin_frame_address(0);
	void* address = __builtin_extract_return_addr(
	    __builtin_return_address(0));
	m_raise_pascal(object, address, frame);
}

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
using t_single = float;
using t_double = double;
using t_extended = long double;

// Runtime handle base for Pascal `class of T`. The target is allowed to be
// incomplete: this empty specialization never inspects T. Every generated
// T::m_meta inherits m_classref<T>, so a class-reference value is an ordinary
// pointer to that marker subobject. Different T arguments remain different
// C++ types (and therefore keep overload signatures distinct) without adding
// a pointer field or requiring T::m_meta to be nameable at the declaration.
template<typename Target>
struct m_classref {};

static_assert(std::is_empty_v<m_classref<void>>);

// Pascal's public, untyped method-pointer view. The compiler registers these
// members under the Pascal spellings Code and Data; their C++ spellings follow
// the RTL p_<name> convention for Pascal-visible values.
struct t_tmethod {
	t_pointer p_code;
	t_pointer p_data;
};

template<typename Signature>
using m_proc = Signature*;

template<typename FunctionPointer>
inline t_pointer m_function_to_code_pointer(
    FunctionPointer value) noexcept {
	static_assert(std::is_pointer_v<FunctionPointer>);
	static_assert(std::is_function_v<
	    std::remove_pointer_t<FunctionPointer>>);
	static_assert(sizeof(FunctionPointer) == sizeof(t_pointer),
	    "the target ABI must fit a function pointer in TMethod.Code");
	return std::bit_cast<t_pointer>(value);
}

template<typename FunctionPointer>
inline FunctionPointer m_code_pointer_to_function(
    t_pointer value) noexcept {
	static_assert(std::is_pointer_v<FunctionPointer>);
	static_assert(std::is_function_v<
	    std::remove_pointer_t<FunctionPointer>>);
	static_assert(sizeof(FunctionPointer) == sizeof(t_pointer),
	    "the target ABI must fit a function pointer in TMethod.Code");
	return std::bit_cast<FunctionPointer>(value);
}

template<typename Signature>
struct m_method;

template<typename Result, typename... Args>
struct m_method<Result(Args...)> {
	t_pointer p_code;
	t_pointer p_data;

	Result operator()(Args... args) const {
		using invoke_type = Result (*)(void*, Args...);
		auto invoke =
		    m_code_pointer_to_function<invoke_type>(p_code);
		return invoke(p_data, std::forward<Args>(args)...);
	}
};

template<auto Method>
struct m_method_adapter;

template<typename Owner, typename Result, typename... Args,
         Result (Owner::*Method)(Args...)>
struct m_method_adapter<Method> {
	using owner_type = Owner;
	using signature_type = Result(Args...);

	static Result invoke(void* data, Args... args) {
		return (static_cast<Owner*>(data)->*Method)(
		    std::forward<Args>(args)...);
	}
};

template<auto Method, typename Object>
inline auto m_bind_method(Object* object) noexcept {
	using adapter = m_method_adapter<Method>;
	using owner = typename adapter::owner_type;
	using signature = typename adapter::signature_type;
	owner* adjusted = static_cast<owner*>(object);
	return m_method<signature>{
	    m_function_to_code_pointer(&adapter::invoke),
	    static_cast<void*>(adjusted),
	};
}

template<auto Function>
struct m_receiver_function_adapter;

template<typename Owner, typename Result, typename... Args,
         Result (*Function)(Owner*, Args...)>
struct m_receiver_function_adapter<Function> {
	using owner_type = Owner;
	using signature_type = Result(Args...);

	static Result invoke(void* data, Args... args) {
		return Function(
		    static_cast<Owner*>(data),
		    std::forward<Args>(args)...);
	}
};

template<auto Function, typename Object>
inline auto m_bind_receiver_function(
    Object* object) noexcept {
	using adapter =
	    m_receiver_function_adapter<Function>;
	using owner = typename adapter::owner_type;
	using signature = typename adapter::signature_type;
	owner* adjusted = static_cast<owner*>(object);
	return m_method<signature>{
	    m_function_to_code_pointer(&adapter::invoke),
	    static_cast<void*>(adjusted),
	};
}

static_assert(std::is_standard_layout_v<t_tmethod>);
static_assert(std::is_trivially_copyable_v<t_tmethod>);
static_assert(std::is_standard_layout_v<m_method<void()>>);
static_assert(std::is_trivially_copyable_v<m_method<void()>>);
static_assert(offsetof(t_tmethod, p_code) ==
    offsetof(m_method<void()>, p_code));
static_assert(offsetof(t_tmethod, p_data) ==
    offsetof(m_method<void()>, p_data));
static_assert(sizeof(t_tmethod) == sizeof(m_method<void()>));
static_assert(alignof(t_tmethod) == alignof(m_method<void()>));

template<typename Signature>
inline t_tmethod m_method_to_tmethod(
    m_method<Signature> value) noexcept {
	static_assert(sizeof(m_method<Signature>) == sizeof(t_tmethod));
	static_assert(alignof(m_method<Signature>) == alignof(t_tmethod));
	static_assert(std::is_trivially_copyable_v<m_method<Signature>>);
	return std::bit_cast<t_tmethod>(value);
}

template<typename Signature>
inline m_method<Signature> m_tmethod_to_method(
    t_tmethod value) noexcept {
	static_assert(sizeof(m_method<Signature>) == sizeof(t_tmethod));
	static_assert(alignof(m_method<Signature>) == alignof(t_tmethod));
	static_assert(std::is_trivially_copyable_v<m_method<Signature>>);
	return std::bit_cast<m_method<Signature>>(value);
}

template<typename Signature>
inline void m_store_tmethod_code(
    m_method<Signature>& destination, t_pointer value) noexcept {
	t_tmethod public_value =
	    m_method_to_tmethod(destination);
	public_value.p_code = value;
	destination =
	    m_tmethod_to_method<Signature>(public_value);
}

template<typename Signature>
inline void m_store_tmethod_data(
    m_method<Signature>& destination, t_pointer value) noexcept {
	t_tmethod public_value =
	    m_method_to_tmethod(destination);
	public_value.p_data = value;
	destination =
	    m_tmethod_to_method<Signature>(public_value);
}

inline t_word p_errorcode = 0;
using m_error_proc =
    void (*)(t_longint, t_pointer, t_pointer);
inline m_error_proc p_errorproc = nullptr;
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
static_assert(sizeof(t_single) == 4);
static_assert(sizeof(t_double) == 8);

[[noreturn]] inline void p_halt(t_longint value) {
	std::exit(static_cast<int>(value));
}

[[noreturn]] inline void p_halt() {
	p_halt(0);
}

[[noreturn]] inline void p_runerror(t_word value) {
	p_errorcode = value;
	p_halt(static_cast<t_longint>(value));
}

[[noreturn]] inline void p_runerror() {
	p_runerror(0);
}

[[noreturn]] inline void m_runtime_error(
    t_longint code, t_pointer address,
    t_pointer frame) {
	if (p_errorproc)
		p_errorproc(code, address, frame);
	// ErrorProc has a procedure type in Pascal rather than a noreturn type.
	// If a user-installed callback returns, retain System's ordinary runtime
	// error behavior instead of continuing after a failed RTL operation.
	p_runerror(static_cast<t_word>(code));
}

[[noreturn, gnu::noinline]] inline void
m_runtime_error(t_longint code) {
	t_pointer frame = __builtin_frame_address(0);
	t_pointer address = __builtin_extract_return_addr(
	    __builtin_return_address(0));
	m_runtime_error(code, address, frame);
}

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

template<typename T, bool IsEnum = std::is_enum_v<T>>
struct tpcc_ordinal_storage {
	using type = T;
	static constexpr type get(T value) {
		return value;
	}
	static constexpr T make(type value) {
		return value;
	}
};

template<typename T>
struct tpcc_ordinal_storage<T, true> {
	using type = std::underlying_type_t<T>;
	static constexpr type get(T value) {
		return static_cast<type>(value);
	}
	static constexpr T make(type value) {
		return static_cast<T>(value);
	}
};

template<>
struct tpcc_ordinal_storage<t_char, false> {
	using type = uint8_t;
	static constexpr type get(t_char value) {
		return value.value;
	}
	static constexpr t_char make(type value) {
		return t_char{value};
	}
};

// Pascal's explicit ordinal cast is a bit-width operation. Convert through
// unsigned storage, where C++20 defines modulo reduction, then bit-cast a
// signed destination so out-of-range unsigned-to-signed conversion is never
// implementation-defined.
template<typename Target, typename Source>
constexpr Target m_ordinal_cast(Source source) {
	using source_traits =
	    tpcc_ordinal_storage<Source>;
	using source_storage =
	    typename source_traits::type;
	using target_traits =
	    tpcc_ordinal_storage<Target>;
	using target_storage =
	    typename target_traits::type;
	static_assert(
	    std::is_integral_v<source_storage>);
	static_assert(
	    std::is_integral_v<target_storage>);
	using target_unsigned =
	    std::make_unsigned_t<target_storage>;
	const target_unsigned bits =
	    static_cast<target_unsigned>(
	        source_traits::get(source));
	target_storage stored;
	if constexpr (
	    std::is_signed_v<target_storage>)
		stored =
		    std::bit_cast<target_storage>(
		        bits);
	else
		stored =
		    static_cast<target_storage>(
		        bits);
	return target_traits::make(stored);
}

using tpcc_unknown_type = void*;

inline t_boolean tpcc_bool_to_boolean(bool value) {
	return value ? p_true : p_false;
}

template<std::size_t Capacity>
struct t_shortstring {
	static_assert(
	    Capacity >= 1 && Capacity <= 255,
	    "Pascal ShortString capacity must be in 1..255");
	static constexpr std::size_t capacity = Capacity;

	t_char length;
	t_char data[Capacity];

	constexpr t_byte m_length() const {
		return static_cast<t_byte>(length);
	}

	constexpr t_integer m_low() const {
		return 1;
	}

	constexpr t_integer m_high() const {
		return static_cast<t_integer>(
		    m_length());
	}

	constexpr t_char* m_data() {
		return data;
	}

	constexpr const t_char* m_data() const {
		return data;
	}

	std::string m_string() const {
		std::string result;
		const std::size_t count =
		    std::min<std::size_t>(
		        length.value, Capacity);
		result.reserve(count);
		for (std::size_t i = 0; i < count; ++i)
			result.push_back(
			    static_cast<char>(data[i].value));
		return result;
	}
};

template<typename T>
struct tpcc_is_shortstring : std::false_type {};

template<std::size_t Capacity>
struct tpcc_is_shortstring<t_shortstring<Capacity>>
    : std::true_type {};

template<typename T>
inline constexpr bool tpcc_is_shortstring_v =
    tpcc_is_shortstring<std::remove_cv_t<T>>::value;

static_assert(sizeof(t_shortstring<255>) == 256);
static_assert(alignof(t_shortstring<255>) == 1);
static_assert(std::is_aggregate_v<t_shortstring<255>>);
static_assert(
    std::is_trivially_default_constructible_v<
        t_shortstring<255>>);
static_assert(std::is_trivially_copyable_v<t_shortstring<255>>);

template<std::size_t DestinationCapacity,
         std::size_t SourceCapacity>
constexpr t_shortstring<DestinationCapacity>
tpcc_shortstring_cast(
    const t_shortstring<SourceCapacity>& source) {
	t_shortstring<DestinationCapacity> result{};
	const std::size_t copied = std::min({
	    static_cast<std::size_t>(source.length.value),
	    SourceCapacity,
	    DestinationCapacity,
	});
	result.length =
	    t_char{static_cast<uint8_t>(copied)};
	for (std::size_t i = 0; i < copied; ++i)
		result.data[i] = source.data[i];
	return result;
}

// A Pascal Text variable currently stores the stream used by Write/WriteLn.
// The pointer is non-owning and null means that the Text variable is unopened.
// Write/WriteLn without an explicit Text argument write to std::cout directly.
// Assign/Rewrite/Close and owned file streams are not implemented yet.
struct t_text {
	std::ostream* stream = nullptr;
};
static_assert(sizeof(t_text) == sizeof(void*));
static_assert(alignof(t_text) == alignof(void*));

// Text, untyped binary files, and typed binary files are incompatible Pascal
// types even though all three currently carry one runtime-state pointer.
// Binary state is deliberately opaque here: file operations own its concrete
// handle, filename, mode, and record-size representation.
struct binary_file_state;

struct t_file {
	binary_file_state* state = nullptr;
};

template<typename Element>
struct t_typedfile {
	using element_type = Element;
	binary_file_state* state = nullptr;
};

static_assert(sizeof(t_file) == sizeof(void*));
static_assert(alignof(t_file) == alignof(void*));
static_assert(std::is_aggregate_v<t_file>);
static_assert(std::is_standard_layout_v<t_file>);
static_assert(std::is_trivially_copyable_v<t_file>);
static_assert(sizeof(t_typedfile<t_byte>) == sizeof(void*));
static_assert(alignof(t_typedfile<t_byte>) == alignof(void*));
static_assert(std::is_aggregate_v<t_typedfile<t_byte>>);
static_assert(std::is_standard_layout_v<t_typedfile<t_byte>>);
static_assert(std::is_trivially_copyable_v<t_typedfile<t_byte>>);
static_assert(!std::is_same_v<t_file, t_text>);
static_assert(!std::is_same_v<t_file, t_typedfile<t_byte>>);
static_assert(!std::is_same_v<t_text, t_typedfile<t_byte>>);

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

// Dereferencing Pascal's untyped Pointer does not produce a C++ value: void
// has no object representation that can be named by `*pointer`. It produces
// an unbounded raw storage place, which can be consumed by Pascal's omitted-
// type var/out/const parameters. Keep this operation distinct from
// tpcc_make_storage_ref(pointer_variable), which refers to the bytes occupied
// by the pointer variable itself.
inline tpcc_storage_ref tpcc_dereference_storage(
	t_pointer value) {
	if (!value)
		m_runtime_error(216);
	return tpcc_storage_ref{
	    reinterpret_cast<std::byte*>(value),
	    std::numeric_limits<std::size_t>::max(),
	};
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

template<typename Target, typename Source>
inline t_set<Target> m_set_cast(
    const t_set<Source>& source) {
	return t_set<Target>{source.spans};
}

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

	constexpr t_sizeint m_length() const {
		return static_cast<t_sizeint>(length);
	}

	constexpr auto m_low() const {
		return low;
	}

	constexpr auto m_high() const {
		using value_type = decltype(low);
		using traits =
		    tpcc_ordinal_storage<value_type>;
		using storage_type =
		    typename traits::type;
		return traits::make(
		    static_cast<storage_type>(
			traits::get(low) +
			static_cast<storage_type>(
			    length - 1)));
	}

	constexpr T* m_data() {
		return items;
	}

	constexpr const T* m_data() const {
		return items;
	}

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
		m_runtime_error(201);
	return value.items[static_cast<std::size_t>(actual - first)];
}

template<typename T, std::size_t length, auto low, typename I>
inline const T& p_index(const t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		m_runtime_error(201);
	return value.items[static_cast<std::size_t>(actual - first)];
}

template<typename T, std::size_t length, auto low, typename I>
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(
    t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length)
		m_runtime_error(201);
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
		m_runtime_error(201);
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

template<std::size_t Capacity, typename I>
inline t_char& p_index(t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual > static_cast<std::ptrdiff_t>(Capacity))
		m_runtime_error(201);
	if (actual == 0)
		return value.length;
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template<std::size_t Capacity, typename I>
inline const t_char& p_index(
    const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual > static_cast<std::ptrdiff_t>(Capacity))
		m_runtime_error(201);
	if (actual == 0)
		return value.length;
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template<std::size_t Capacity, typename I>
inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(
    t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual > static_cast<std::ptrdiff_t>(Capacity))
		m_runtime_error(201);
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
	        Capacity - offset,
	    },
	    std::addressof(value.data[offset]),
	};
}

template<std::size_t Capacity, typename I>
inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(
    const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual > static_cast<std::ptrdiff_t>(Capacity))
		m_runtime_error(201);
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
	        Capacity - offset,
	    },
	    std::addressof(value.data[offset]),
	};
}

template<typename T, typename I>
inline T& p_index(T* value, I index) {
	if (!value)
		m_runtime_error(216);
	return value[static_cast<std::ptrdiff_t>(index)];
}

template<typename T, typename I>
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(T* value, I index) {
	if (!value)
		m_runtime_error(216);
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
		m_runtime_error(216);
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
struct m_shared_block {
	std::atomic<std::size_t> references{1};
	std::vector<T> values;

	explicit m_shared_block(
	    std::vector<T> values)
	    : values(std::move(values)) {
	}
};

// The public managed carriers below must remain one pointer wide. This
// private handle supplies the shared reference-counting and exception-safe
// block replacement without imposing one mutation policy: dynamic arrays
// deliberately share element writes, while AnsiString detaches before a
// writable character escapes.
template<typename T>
class m_shared_buffer {
	m_shared_block<T>* block = nullptr;

	void m_retain() noexcept {
		if (block)
			block->references.fetch_add(
			    1, std::memory_order_relaxed);
	}

	void m_release() noexcept {
		if (block &&
		    block->references.fetch_sub(
			1, std::memory_order_acq_rel) == 1)
			delete block;
	}

public:
	m_shared_buffer() = default;

	m_shared_buffer(const m_shared_buffer& other)
	    : block(other.block) {
		m_retain();
	}

	m_shared_buffer(m_shared_buffer&& other) noexcept
	    : block(std::exchange(other.block, nullptr)) {
	}

	~m_shared_buffer() {
		m_release();
	}

	m_shared_buffer& operator=(
	    const m_shared_buffer& other) {
		if (this == &other)
			return *this;
		m_shared_buffer replacement(other);
		std::swap(block, replacement.block);
		return *this;
	}

	m_shared_buffer& operator=(
	    m_shared_buffer&& other) noexcept {
		if (this == &other)
			return *this;
		m_release();
		block = std::exchange(
		    other.block, nullptr);
		return *this;
	}

	std::size_t m_size() const noexcept {
		return block ? block->values.size() : 0;
	}

	const T* m_data() const noexcept {
		return block ? block->values.data() : nullptr;
	}

	T* m_shared_data() noexcept {
		return block ? block->values.data() : nullptr;
	}

	void m_replace(std::vector<T> values) {
		auto* replacement =
		    values.empty()
			? nullptr
			: new m_shared_block<T>(
			      std::move(values));
		m_release();
		block = replacement;
	}

	void m_make_unique() {
		if (!block ||
		    block->references.load(
			std::memory_order_acquire) == 1)
			return;
		std::vector<T> copy = block->values;
		m_replace(std::move(copy));
	}
};

static_assert(
    sizeof(m_shared_buffer<t_byte>) ==
    sizeof(void*));

template<typename T>
struct t_dynamicarray {
	m_shared_buffer<T> storage;

	t_sizeint m_length() const {
		return static_cast<t_sizeint>(
		    storage.m_size());
	}

	t_sizeint m_low() const {
		return 0;
	}

	t_sizeint m_high() const {
		return m_length() - 1;
	}

	T* m_data() {
		return storage.m_shared_data();
	}

	const T* m_data() const {
		return storage.m_data();
	}

	static t_dynamicarray m_from_values(
	    std::initializer_list<T> values) {
		t_dynamicarray result;
		result.storage.m_replace(
		    std::vector<T>(
			values.begin(),
			values.end()));
		return result;
	}

	void m_resize(t_sizeint requested_length) {
		const std::size_t new_length =
		    requested_length <= 0
			? 0
			: static_cast<std::size_t>(
			      requested_length);
		std::vector<T> replacement(new_length);
		const std::size_t copied = std::min(
		    storage.m_size(), new_length);
		std::copy_n(
		    storage.m_data(), copied,
		    replacement.data());
		// SetLength is a handle operation. Replacing the block even when the
		// size is unchanged ensures aliases retain their original array.
		storage.m_replace(std::move(replacement));
	}
};

static_assert(
    sizeof(t_dynamicarray<t_byte>) ==
    sizeof(void*));
static_assert(
    alignof(t_dynamicarray<t_byte>) ==
    alignof(void*));

template<typename T>
struct t_openarray {
	T* data = nullptr;
	t_sizeint count = 0;

	t_sizeint m_length() const {
		return count;
	}

	t_sizeint m_low() const {
		return 0;
	}

	t_sizeint m_high() const {
		return m_length() - 1;
	}

	T* m_data() const {
		return data;
	}
};

static_assert(
    sizeof(t_openarray<t_byte>) ==
    sizeof(void*) * 2);

template<typename T>
class m_openarray_owner {
	// A value open-array argument is a copy whose storage must survive until
	// the complete call expression finishes. The conversion below exposes a
	// descriptor while this owner remains the materialized argument object.
	std::vector<T> values;

public:
	m_openarray_owner(
	    const T* data, std::size_t count)
	    : values() {
		if (count != 0)
			values.assign(
			    data, data + count);
	}

	explicit m_openarray_owner(
	    std::vector<T> values)
	    : values(std::move(values)) {
	}

	operator t_openarray<T>() {
		return t_openarray<T>{
		    values.data(),
		    static_cast<t_sizeint>(
			values.size()),
		};
	}

	operator t_openarray<const T>() const {
		return t_openarray<const T>{
		    values.data(),
		    static_cast<t_sizeint>(
			values.size()),
		};
	}
};

template<typename T>
inline m_openarray_owner<T>
m_openarray_values(
    std::initializer_list<T> values) {
	return m_openarray_owner<T>{
	    std::vector<T>(
		values.begin(), values.end())};
}

template<typename T, std::size_t N, auto Low>
inline t_openarray<const T>
m_openarray_const_view(
    const t_fixedarray<T, N, Low>& value) {
	return t_openarray<const T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template<typename T>
inline t_openarray<const T>
m_openarray_const_view(
    const t_dynamicarray<T>& value) {
	return t_openarray<const T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template<typename T>
inline t_openarray<const std::remove_const_t<T>>
m_openarray_const_view(t_openarray<T> value) {
	return {
	    value.m_data(),
	    value.m_length(),
	};
}

template<typename T, std::size_t N, auto Low>
inline t_openarray<T>
m_openarray_mutable_view(
    t_fixedarray<T, N, Low>& value) {
	return t_openarray<T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template<typename T>
inline t_openarray<T>
m_openarray_mutable_view(
    t_dynamicarray<T>& value) {
	return t_openarray<T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template<typename T>
requires (!std::is_const_v<T>)
inline t_openarray<T>
m_openarray_mutable_view(t_openarray<T> value) {
	return value;
}

template<typename T>
inline t_openarray<T>
m_openarray_reset(t_openarray<T> value) {
	// The currently supported managed carriers implement Pascal default
	// initialization through assignment from T{}. Custom Initialize/Finalize
	// hooks require a separate lifetime protocol and must not be guessed here.
	for (t_sizeint i = 0;
	     i < value.m_length(); ++i)
		value.m_data()[i] = T{};
	return value;
}

template<typename T, std::size_t N, auto Low>
inline t_openarray<T>
m_openarray_out_view(
    t_fixedarray<T, N, Low>& value) {
	return m_openarray_reset(
	    m_openarray_mutable_view(value));
}

template<typename T>
inline t_openarray<T>
m_openarray_out_view(
    t_dynamicarray<T>& value) {
	return m_openarray_reset(
	    m_openarray_mutable_view(value));
}

template<typename T>
requires (!std::is_const_v<T>)
inline t_openarray<T>
m_openarray_out_view(t_openarray<T> value) {
	return m_openarray_reset(value);
}

template<typename T, std::size_t N, auto Low>
inline m_openarray_owner<T>
m_openarray_value_copy(
    const t_fixedarray<T, N, Low>& value) {
	return m_openarray_owner<T>{
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length()),
	};
}

template<typename T>
inline m_openarray_owner<T>
m_openarray_value_copy(
    const t_dynamicarray<T>& value) {
	return m_openarray_owner<T>{
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length()),
	};
}

template<typename T>
inline m_openarray_owner<std::remove_const_t<T>>
m_openarray_value_copy(t_openarray<T> value) {
	using item_type =
	    std::remove_const_t<T>;
	return m_openarray_owner<item_type>{
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length()),
	};
}

struct t_ansistring {
	m_shared_buffer<t_char> storage;

	t_sizeint m_length() const {
		const std::size_t stored =
		    storage.m_size();
		return stored == 0
			   ? 0
			   : static_cast<t_sizeint>(
				 stored - 1);
	}

	t_sizeint m_low() const {
		return 1;
	}

	t_sizeint m_high() const {
		return m_length();
	}

	const t_char* m_data() const noexcept {
		if (const t_char* data =
			storage.m_data())
			return data;
		static const t_char zero{0};
		return &zero;
	}

	void m_make_unique() {
		if (storage.m_size() == 0) {
			storage.m_replace(
			    std::vector<t_char>{
				t_char{0}});
			return;
		}
		storage.m_make_unique();
	}

	t_char* m_writable_data() {
		m_make_unique();
		return storage.m_shared_data();
	}

	// Explicit Pointer(AnsiString) exposes character one. Because the result
	// is writable Pascal Pointer storage, detachment happens before it escapes.
	t_pointer m_pointer() {
		return m_writable_data();
	}

	std::string m_string() const {
		std::string result;
		const std::size_t count =
		    static_cast<std::size_t>(
			m_length());
		result.reserve(count);
		const t_char* data = m_data();
		for (std::size_t i = 0; i < count; ++i)
			result.push_back(
			    static_cast<char>(
				data[i].value));
		return result;
	}

	template<typename I>
	t_char& index(I index) {
		const std::ptrdiff_t actual =
		    static_cast<std::ptrdiff_t>(
			index);
		if (actual < 1 ||
		    actual >
			static_cast<std::ptrdiff_t>(
			    m_length()))
			m_runtime_error(201);
		return m_writable_data()[
		    static_cast<std::size_t>(
			actual - 1)];
	}

	template<typename I>
	const t_char& index(I index) const {
		const std::ptrdiff_t actual =
		    static_cast<std::ptrdiff_t>(
			index);
		if (actual < 1 ||
		    actual >
			static_cast<std::ptrdiff_t>(
			    m_length()))
			m_runtime_error(201);
		return m_data()[
		    static_cast<std::size_t>(
			actual - 1)];
	}

	template<typename I>
	std::size_t storage_extent(I index) const {
		const std::ptrdiff_t actual =
		    static_cast<std::ptrdiff_t>(
			index);
		if (actual < 1 ||
		    actual >
			static_cast<std::ptrdiff_t>(
			    m_length()))
			m_runtime_error(201);
		return static_cast<std::size_t>(
		    m_length() - actual + 2);
	}

	template<std::size_t SourceCapacity>
	void assign(
	    const t_shortstring<SourceCapacity>& source) {
		const std::size_t copied =
		    static_cast<std::size_t>(
			source.m_length());
		std::vector<t_char> replacement(
		    copied + 1, t_char{0});
		std::copy_n(
		    source.m_data(), copied,
		    replacement.data());
		storage.m_replace(
		    std::move(replacement));
	}

	void m_resize(t_sizeint requested_length) {
		const std::size_t new_length =
		    requested_length <= 0
			? 0
			: static_cast<std::size_t>(
			      requested_length);
		std::vector<t_char> replacement(
		    new_length + 1, t_char{0});
		const std::size_t copied = std::min(
		    static_cast<std::size_t>(
			m_length()),
		    new_length);
		std::copy_n(
		    m_data(), copied,
		    replacement.data());
		storage.m_replace(
		    std::move(replacement));
	}

	t_ansistring slice(
	    t_sizeint index, t_sizeint count) const {
		t_ansistring result;
		if (count <= 0)
			return result;
		if (index < 1)
			index = 1;
		const std::size_t start =
		    static_cast<std::size_t>(
			index - 1);
		const std::size_t source_length =
		    static_cast<std::size_t>(
			m_length());
		if (start >= source_length)
			return result;
		const std::size_t copied = std::min(
		    static_cast<std::size_t>(count),
		    source_length - start);
		std::vector<t_char> replacement(
		    copied + 1, t_char{0});
		std::copy_n(
		    m_data() + start, copied,
		    replacement.data());
		result.storage.m_replace(
		    std::move(replacement));
		return result;
	}

	void erase(
	    t_longint index, t_longint count) {
		if (index < 1 || count <= 0)
			return;
		const std::size_t start =
		    static_cast<std::size_t>(
			index - 1);
		const std::size_t old_length =
		    static_cast<std::size_t>(
			m_length());
		if (start >= old_length)
			return;
		const std::size_t removed = std::min(
		    static_cast<std::size_t>(count),
		    old_length - start);
		std::vector<t_char> replacement(
		    old_length - removed + 1,
		    t_char{0});
		std::copy_n(
		    m_data(), start,
		    replacement.data());
		std::copy(
		    m_data() + start + removed,
		    m_data() + old_length,
		    replacement.data() + start);
		storage.m_replace(
		    std::move(replacement));
	}

	void insert(
	    const t_ansistring& source,
	    t_longint index) {
		const std::size_t source_length =
		    static_cast<std::size_t>(
			source.m_length());
		if (source_length == 0)
			return;
		const std::size_t old_length =
		    static_cast<std::size_t>(
			m_length());
		if (index < 1)
			index = 1;
		std::size_t start =
		    static_cast<std::size_t>(
			index - 1);
		if (start > old_length)
			start = old_length;

		// Copy both source ranges before replacing the shared block. This also
		// covers Insert(S, S, I) without an alias-specific branch.
		std::vector<t_char> replacement(
		    old_length + source_length + 1,
		    t_char{0});
		std::copy_n(
		    m_data(), start,
		    replacement.data());
		std::copy_n(
		    source.m_data(), source_length,
		    replacement.data() + start);
		std::copy(
		    m_data() + start,
		    m_data() + old_length,
		    replacement.data() + start +
			source_length);
		storage.m_replace(
		    std::move(replacement));
	}
};

static_assert(
    sizeof(t_ansistring) == sizeof(void*));
static_assert(
    alignof(t_ansistring) == alignof(void*));

template<typename T, typename I>
inline T& p_index(
    t_dynamicarray<T>& value, I index) {
	const std::ptrdiff_t actual =
	    static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual >= static_cast<std::ptrdiff_t>(
			  value.m_length()))
		m_runtime_error(201);
	return value.m_data()[
	    static_cast<std::size_t>(actual)];
}

template<typename T, typename I>
inline const T& p_index(
    const t_dynamicarray<T>& value, I index) {
	const std::ptrdiff_t actual =
	    static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual >= static_cast<std::ptrdiff_t>(
			  value.m_length()))
		m_runtime_error(201);
	return value.m_data()[
	    static_cast<std::size_t>(actual)];
}

template<typename T, typename I>
inline tpcc_typed_storage_ref<T>
tpcc_make_storage_ref(
    t_dynamicarray<T>& value, I index) {
	T& selected = p_index(value, index);
	const std::size_t offset =
	    static_cast<std::size_t>(
		static_cast<std::ptrdiff_t>(
		    index));
	return tpcc_typed_storage_ref<T>{
	    {
		reinterpret_cast<std::byte*>(
		    std::addressof(selected)),
		(static_cast<std::size_t>(
		     value.m_length()) -
		 offset) *
		    sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template<typename T, typename I>
inline tpcc_typed_const_storage_ref<T>
tpcc_make_const_storage_ref(
    const t_dynamicarray<T>& value, I index) {
	const T& selected = p_index(value, index);
	const std::size_t offset =
	    static_cast<std::size_t>(
		static_cast<std::ptrdiff_t>(
		    index));
	return tpcc_typed_const_storage_ref<T>{
	    {
		reinterpret_cast<const std::byte*>(
		    std::addressof(selected)),
		(static_cast<std::size_t>(
		     value.m_length()) -
		 offset) *
		    sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template<typename T, typename I>
inline T& p_index(
    t_openarray<T> value, I index) {
	const std::ptrdiff_t actual =
	    static_cast<std::ptrdiff_t>(index);
	if (actual < 0 ||
	    actual >= static_cast<std::ptrdiff_t>(
			  value.m_length()))
		m_runtime_error(201);
	return value.m_data()[
	    static_cast<std::size_t>(actual)];
}

template<typename T, typename I>
requires (!std::is_const_v<T>)
inline tpcc_typed_storage_ref<T>
tpcc_make_storage_ref(
    t_openarray<T> value, I index) {
	T& selected = p_index(value, index);
	const std::size_t offset =
	    static_cast<std::size_t>(
		static_cast<std::ptrdiff_t>(
		    index));
	return tpcc_typed_storage_ref<T>{
	    {
		reinterpret_cast<std::byte*>(
		    std::addressof(selected)),
		(static_cast<std::size_t>(
		     value.m_length()) -
		 offset) *
		    sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template<typename T, typename I>
inline tpcc_typed_const_storage_ref<T>
tpcc_make_const_storage_ref(
    t_openarray<const T> value, I index) {
	const T& selected = p_index(value, index);
	const std::size_t offset =
	    static_cast<std::size_t>(
		static_cast<std::ptrdiff_t>(
		    index));
	return tpcc_typed_const_storage_ref<T>{
	    {
		reinterpret_cast<const std::byte*>(
		    std::addressof(selected)),
		(static_cast<std::size_t>(
		     value.m_length()) -
		 offset) *
		    sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template<typename T>
class m_array_view_enumerator {
	T* data;
	std::size_t count;
	std::size_t next = 0;

public:
	m_array_view_enumerator(
	    T* data, std::size_t count)
	    : data(data), count(count) {
	}

	bool m_move_next() {
		if (next == count)
			return false;
		++next;
		return true;
	}

	decltype(auto) m_current() const {
		return data[next - 1];
	}
};

template<typename T>
class m_dynamicarray_enumerator {
	// Copying the shared handle pins the exact buffer selected when iteration
	// begins. A later SetLength on the source replaces its handle and cannot
	// invalidate this traversal.
	t_dynamicarray<T> value;
	std::size_t next = 0;

public:
	explicit m_dynamicarray_enumerator(
	    const t_dynamicarray<T>& value)
	    : value(value) {
	}

	bool m_move_next() {
		if (next ==
		    static_cast<std::size_t>(
			value.m_length()))
			return false;
		++next;
		return true;
	}

	decltype(auto) m_current() const {
		return value.m_data()[next - 1];
	}
};

template<typename String>
class m_string_enumerator {
	// Strings use the same snapshot rule as dynamic arrays. Short strings copy
	// their inline bytes; AnsiString copies its shared handle and relies on
	// copy-on-write before either alias is modified.
	String value;
	std::size_t next = 0;

public:
	explicit m_string_enumerator(
	    const String& value)
	    : value(value) {
	}

	bool m_move_next() {
		if (next ==
		    static_cast<std::size_t>(
			value.m_length()))
			return false;
		++next;
		return true;
	}

	t_char m_current() const {
		return value.m_data()[next - 1];
	}
};

template<typename T>
inline T m_ordinal_from_storage(
    typename tpcc_ordinal_storage<T>::type value) {
	return tpcc_ordinal_storage<T>::make(
	    value);
}

template<typename T>
class m_ordinal_enumerator {
	using traits = tpcc_ordinal_storage<T>;
	using storage_type = typename traits::type;
	storage_type lower;
	storage_type upper;
	storage_type current{};
	bool started = false;
	bool valid;

public:
	m_ordinal_enumerator(T lower, T upper)
	    : lower(traits::get(lower)),
	      upper(traits::get(upper)),
	      valid(this->lower <= this->upper) {
	}

	bool m_move_next() {
		if (!valid)
			return false;
		if (!started) {
			current = lower;
			started = true;
			return true;
		}
		if (current == upper)
			return false;
		++current;
		return true;
	}

	T m_current() const {
		return m_ordinal_from_storage<T>(
		    current);
	}
};

template<typename T>
struct m_ordinal_range {
	T lower;
	T upper;
};

template<typename T>
inline m_ordinal_enumerator<T>
m_enumerate(m_ordinal_range<T> range) {
	return m_ordinal_enumerator<T>{
	    range.lower, range.upper};
}

template<typename T>
class m_set_enumerator {
	using traits = tpcc_ordinal_storage<T>;
	using storage_type = typename traits::type;
	t_set<T> value;
	storage_type lower;
	storage_type upper;
	storage_type current{};
	bool started = false;
	bool exhausted;

public:
	m_set_enumerator(
	    const t_set<T>& value,
	    T lower, T upper)
	    : value(value),
	      lower(traits::get(lower)),
	      upper(traits::get(upper)),
	      exhausted(this->lower >
			    this->upper ||
			value.spans.empty()) {
	}

	bool m_move_next() {
		// Scan the declared ordinal domain and ask set membership for each
		// value. This yields each Pascal value once even if stored spans
		// overlap or are represented differently after normalization.
		while (!exhausted) {
			if (!started) {
				current = lower;
				started = true;
			} else if (current == upper) {
				exhausted = true;
				return false;
			} else {
				++current;
			}
			if (p_in(
				m_ordinal_from_storage<T>(
				    current),
				value) == p_true)
				return true;
		}
		return false;
	}

	T m_current() const {
		return m_ordinal_from_storage<T>(
		    current);
	}
};

template<typename T, std::size_t N, auto Low>
inline m_array_view_enumerator<T>
m_enumerate(t_fixedarray<T, N, Low>& value) {
	return {
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length())};
}

template<typename T, std::size_t N, auto Low>
inline m_array_view_enumerator<const T>
m_enumerate(
    const t_fixedarray<T, N, Low>& value) {
	return {
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length())};
}

template<typename T>
inline m_dynamicarray_enumerator<T>
m_enumerate(
    const t_dynamicarray<T>& value) {
	return m_dynamicarray_enumerator<T>{
	    value};
}

template<typename T>
inline m_array_view_enumerator<T>
m_enumerate(t_openarray<T> value) {
	return {
	    value.m_data(),
	    static_cast<std::size_t>(
		value.m_length())};
}

template<std::size_t Capacity>
inline m_string_enumerator<
    t_shortstring<Capacity>>
m_enumerate(
    const t_shortstring<Capacity>& value) {
	return m_string_enumerator<
	    t_shortstring<Capacity>>{value};
}

inline m_string_enumerator<t_ansistring>
m_enumerate(const t_ansistring& value) {
	return m_string_enumerator<
	    t_ansistring>{value};
}

template<typename T>
inline m_set_enumerator<T>
m_enumerate(
    const t_set<T>& value, T lower,
    T upper) {
	return m_set_enumerator<T>{
	    value, lower, upper};
}

struct binary_file_state {
	std::string name;
	std::FILE* handle = nullptr;
	t_longint record_size = 128;
	bool readable = false;
	bool writable = false;
};

// FPC's file variables are opaque handles and Close preserves the assigned
// filename so Reset/Rewrite can reopen without another Assign. Keep the
// pointed-to state in process-owned storage: t_file remains its pointer-sized,
// trivially-copyable Pascal carrier, while every state is reclaimed during
// normal C++ process teardown.
inline std::vector<std::unique_ptr<binary_file_state>>&
m_binary_file_states() {
	static std::vector<
	    std::unique_ptr<binary_file_state>> states;
	return states;
}

inline t_word m_inoutres = 0;
inline t_byte p_filemode = 2;

inline void m_set_io_error(t_word error) {
	if (m_inoutres == 0)
		m_inoutres = error;
}

inline t_word m_file_error_from_errno(
    int error, t_word fallback) {
	switch (error) {
	case ENOENT:
		return 2;
	case EACCES:
	case EPERM:
		return 5;
	case EMFILE:
	case ENFILE:
		return 4;
	case EBADF:
		return 6;
	default:
		return fallback;
	}
}

inline t_word p_ioresult() {
	const t_word result = m_inoutres;
	m_inoutres = 0;
	return result;
}

inline void m_close_binary_handle(
    binary_file_state& state) {
	if (!state.handle)
		return;
	if (std::fclose(state.handle) != 0)
		m_set_io_error(
		    m_file_error_from_errno(errno, 101));
	state.handle = nullptr;
	state.readable = false;
	state.writable = false;
}

template<typename PascalString>
inline void p_assign(
    t_file& file, const PascalString& name) {
	if (m_inoutres != 0)
		return;
	if (file.state && file.state->handle)
		m_close_binary_handle(*file.state);
	auto state =
	    std::make_unique<binary_file_state>();
	state->name = name.m_string();
	file.state = state.get();
	m_binary_file_states().push_back(
	    std::move(state));
}

inline bool m_prepare_binary_open(
    t_file& file, t_longint record_size) {
	if (m_inoutres != 0)
		return false;
	if (!file.state) {
		m_set_io_error(102);
		return false;
	}
	if (record_size <= 0) {
		m_set_io_error(12);
		return false;
	}
	if (file.state->handle)
		m_close_binary_handle(*file.state);
	if (m_inoutres != 0)
		return false;
	file.state->record_size = record_size;
	return true;
}

inline void p_rewrite(
    t_file& file, t_longint record_size) {
	if (!m_prepare_binary_open(
	        file, record_size))
		return;
	errno = 0;
	file.state->handle =
	    std::fopen(file.state->name.c_str(), "w+b");
	if (!file.state->handle) {
		m_set_io_error(
		    m_file_error_from_errno(errno, 101));
		return;
	}
	// FPC opens an untyped Rewrite file for writing. The C handle is
	// read/write so a later Reset can reuse ordinary host file semantics;
	// Pascal access checks still use these explicit mode flags.
	file.state->readable = false;
	file.state->writable = true;
}

inline void p_reset(
    t_file& file, t_longint record_size) {
	if (!m_prepare_binary_open(
	        file, record_size))
		return;
	const t_byte access =
	    static_cast<t_byte>(p_filemode & 3);
	const char* mode = nullptr;
	switch (access) {
	case 0:
		mode = "rb";
		break;
	case 1:
	case 2:
		mode = "r+b";
		break;
	default:
		m_set_io_error(12);
		return;
	}
	errno = 0;
	file.state->handle =
	    std::fopen(file.state->name.c_str(), mode);
	if (!file.state->handle) {
		m_set_io_error(
		    m_file_error_from_errno(errno, 100));
		return;
	}
	file.state->readable = access != 1;
	file.state->writable = access != 0;
}

inline bool m_require_open_binary_file(
    t_file& file) {
	if (m_inoutres != 0)
		return false;
	if (!file.state) {
		m_set_io_error(102);
		return false;
	}
	if (!file.state->handle) {
		m_set_io_error(103);
		return false;
	}
	return true;
}

inline void p_close(t_file& file) {
	if (!m_require_open_binary_file(file))
		return;
	m_close_binary_handle(*file.state);
}

inline void p_seek(
    t_file& file, t_int64 record_position) {
	if (!m_require_open_binary_file(file))
		return;
	if (record_position < 0 ||
	    record_position >
	        std::numeric_limits<long>::max() /
	            file.state->record_size) {
		m_set_io_error(156);
		return;
	}
	const long byte_position =
	    static_cast<long>(
	        record_position *
	        file.state->record_size);
	if (std::fseek(
	        file.state->handle,
	        byte_position, SEEK_SET) != 0)
		m_set_io_error(156);
}

inline t_int64 p_filepos(t_file& file) {
	if (!m_require_open_binary_file(file))
		return -1;
	const long position =
	    std::ftell(file.state->handle);
	if (position < 0) {
		m_set_io_error(156);
		return -1;
	}
	return static_cast<t_int64>(
	    position / file.state->record_size);
}

inline t_int64 p_filesize(t_file& file) {
	if (!m_require_open_binary_file(file))
		return -1;
	const long original =
	    std::ftell(file.state->handle);
	if (original < 0 ||
	    std::fseek(
	        file.state->handle, 0, SEEK_END) != 0) {
		m_set_io_error(156);
		return -1;
	}
	const long end =
	    std::ftell(file.state->handle);
	if (std::fseek(
	        file.state->handle,
	        original, SEEK_SET) != 0) {
		m_set_io_error(156);
		return -1;
	}
	if (end < 0) {
		m_set_io_error(156);
		return -1;
	}
	return static_cast<t_int64>(
	    end / file.state->record_size);
}

inline t_boolean p_eof(t_file& file) {
	if (!m_require_open_binary_file(file))
		return p_true;
	if (!file.state->readable) {
		m_set_io_error(104);
		return p_true;
	}
	const long original =
	    std::ftell(file.state->handle);
	if (original < 0 ||
	    std::fseek(
	        file.state->handle, 0, SEEK_END) != 0) {
		m_set_io_error(156);
		return p_true;
	}
	const long end =
	    std::ftell(file.state->handle);
	if (std::fseek(
	        file.state->handle,
	        original, SEEK_SET) != 0 ||
	    end < 0) {
		m_set_io_error(156);
		return p_true;
	}
	return tpcc_bool_to_boolean(
	    original >= end);
}

inline void p_truncate(t_file& file) {
	if (!m_require_open_binary_file(file))
		return;
	if (!file.state->writable) {
		m_set_io_error(105);
		return;
	}
	const long position =
	    std::ftell(file.state->handle);
	if (position < 0 ||
	    std::fflush(file.state->handle) != 0) {
		m_set_io_error(101);
		return;
	}
	std::error_code error;
	std::filesystem::resize_file(
	    file.state->name,
	    static_cast<std::uintmax_t>(position),
	    error);
	if (error) {
		m_set_io_error(101);
		return;
	}
	std::clearerr(file.state->handle);
	if (std::fseek(
	        file.state->handle,
	        position, SEEK_SET) != 0)
		m_set_io_error(156);
}

template<typename Count>
inline bool m_binary_transfer_count(
    Count count, t_longint record_size,
    std::size_t* records,
    std::size_t* bytes) {
	if constexpr (std::is_signed_v<Count>)
		if (count < 0) {
			m_set_io_error(106);
			return false;
		}
	const auto unsigned_count =
	    static_cast<std::make_unsigned_t<Count>>(count);
	if (unsigned_count >
	    std::numeric_limits<std::size_t>::max()) {
		m_set_io_error(106);
		return false;
	}
	*records =
	    static_cast<std::size_t>(unsigned_count);
	if (*records >
	    std::numeric_limits<std::size_t>::max() /
	        static_cast<std::size_t>(record_size)) {
		m_set_io_error(106);
		return false;
	}
	*bytes =
	    *records *
	    static_cast<std::size_t>(record_size);
	return true;
}

template<typename Count, typename Result>
inline void p_blockread(
    t_file& file, tpcc_storage_ref buffer,
    Count count, Result& result) {
	result = 0;
	if (!m_require_open_binary_file(file))
		return;
	if (!file.state->readable) {
		m_set_io_error(104);
		return;
	}
	std::size_t records;
	std::size_t bytes;
	if (!m_binary_transfer_count(
	        count, file.state->record_size,
	        &records, &bytes))
		return;
	if (bytes > buffer.size) {
		m_set_io_error(100);
		return;
	}
	const std::size_t transferred =
	    std::fread(
	        buffer.data,
	        static_cast<std::size_t>(
	            file.state->record_size),
	        records, file.state->handle);
	result = static_cast<Result>(transferred);
	if (std::ferror(file.state->handle))
		m_set_io_error(100);
}

template<typename Count>
inline void p_blockread(
    t_file& file, tpcc_storage_ref buffer,
    Count count) {
	Count transferred = 0;
	p_blockread(
	    file, buffer, count, transferred);
	if (m_inoutres == 0 &&
	    transferred != count)
		m_set_io_error(100);
}

template<typename Count, typename Result>
inline void p_blockwrite(
    t_file& file, tpcc_const_storage_ref buffer,
    Count count, Result& result) {
	result = 0;
	if (!m_require_open_binary_file(file))
		return;
	if (!file.state->writable) {
		m_set_io_error(105);
		return;
	}
	std::size_t records;
	std::size_t bytes;
	if (!m_binary_transfer_count(
	        count, file.state->record_size,
	        &records, &bytes))
		return;
	if (bytes > buffer.size) {
		m_set_io_error(101);
		return;
	}
	const std::size_t transferred =
	    std::fwrite(
	        buffer.data,
	        static_cast<std::size_t>(
	            file.state->record_size),
	        records, file.state->handle);
	result = static_cast<Result>(transferred);
	if (transferred != records ||
	    std::ferror(file.state->handle))
		m_set_io_error(101);
}

template<typename Count>
inline void p_blockwrite(
    t_file& file, tpcc_const_storage_ref buffer,
    Count count) {
	Count transferred = 0;
	p_blockwrite(
	    file, buffer, count, transferred);
	if (m_inoutres == 0 &&
	    transferred != count)
		m_set_io_error(101);
}

template<typename T>
struct tpcc_write_arg {
	T value;
	bool has_width;
	t_sizeint width;
	bool has_precision;
	t_sizeint precision;
};

template<typename T>
inline auto tpcc_make_write_arg(T&& value) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_write_arg<value_type>{
	    std::forward<T>(value), false, 0, false, 0};
}

template<typename T>
inline auto tpcc_make_write_arg(T&& value, t_sizeint width) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_write_arg<value_type>{
	    std::forward<T>(value), true, width, false, 0};
}

template<typename T>
inline auto tpcc_make_write_arg(
    T&& value, t_sizeint width, t_sizeint precision) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_write_arg<value_type>{
	    std::forward<T>(value), true, width, true, precision};
}

template<typename>
inline constexpr bool tpcc_dependent_false = false;

template<typename T>
inline std::string tpcc_render_write_value(
    const T& value, bool has_precision, t_sizeint precision) {
	std::ostringstream out;
	if constexpr (tpcc_is_shortstring_v<T> ||
		              std::is_same_v<T, t_ansistring>) {
		return value.m_string();
	} else if constexpr (std::is_same_v<T, t_char>) {
		return std::string(
		    1, static_cast<char>(value.value));
	} else if constexpr (std::is_same_v<T, t_boolean>) {
		return value == p_true ? "TRUE" : "FALSE";
	} else if constexpr (std::is_integral_v<T>) {
		// uint8_t/int8_t stream as characters, so widen every Pascal
		// integer carrier before insertion.
		if constexpr (std::is_signed_v<T>)
			out << static_cast<long long>(value);
		else
			out << static_cast<unsigned long long>(value);
	} else if constexpr (std::is_floating_point_v<T>) {
		if (has_precision)
			out << std::fixed << std::setprecision(
			    static_cast<int>(
			        std::max<t_sizeint>(0, precision)));
		out << value;
	} else {
		static_assert(
		    tpcc_dependent_false<T>,
		    "unsupported Pascal Write/WriteLn value type");
	}
	return out.str();
}

template<typename T>
inline void tpcc_write_one(
    std::ostream& out, const tpcc_write_arg<T>& argument) {
	std::string rendered = tpcc_render_write_value(
	    argument.value, argument.has_precision,
	    argument.precision);
	if (argument.has_width &&
	    argument.width > 0 &&
	    static_cast<std::make_unsigned_t<t_sizeint>>(
	        argument.width) > rendered.size()) {
		const std::size_t padding =
		    static_cast<std::size_t>(argument.width) -
		    rendered.size();
		for (std::size_t i = 0; i < padding; ++i)
			out.put(' ');
	}
	out.write(
	    rendered.data(),
	    static_cast<std::streamsize>(rendered.size()));
}

template<typename... Values>
inline void tpcc_write_many(
    std::ostream& out,
    const tpcc_write_arg<Values>&... arguments) {
	(tpcc_write_one(out, arguments), ...);
}

inline std::ostream& tpcc_text_stream(t_text& file) {
	if (!file.stream)
		m_runtime_error(103);
	return *file.stream;
}

template<typename... Values>
inline void p_write(
    const tpcc_write_arg<Values>&... arguments) {
	tpcc_write_many(std::cout, arguments...);
}

template<typename... Values>
inline void p_write(
    t_text& file,
    const tpcc_write_arg<Values>&... arguments) {
	tpcc_write_many(tpcc_text_stream(file), arguments...);
}

template<typename... Values>
inline void p_writeln(
    const tpcc_write_arg<Values>&... arguments) {
	tpcc_write_many(std::cout, arguments...);
	std::cout.put('\n');
}

template<typename... Values>
inline void p_writeln(
    t_text& file,
    const tpcc_write_arg<Values>&... arguments) {
	std::ostream& out = tpcc_text_stream(file);
	tpcc_write_many(out, arguments...);
	out.put('\n');
}

inline void p_uniquestring(t_ansistring& value) {
	value.m_make_unique();
}

template<typename I>
inline t_char& p_index(t_ansistring& value, I index) {
	p_uniquestring(value);
	return value.index(index);
}

template<typename I>
inline const t_char& p_index(
    const t_ansistring& value, I index) {
	return value.index(index);
}

template<typename I>
inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(
    t_ansistring& value, I index) {
	p_uniquestring(value);
	t_char& selected = value.index(index);
	return tpcc_typed_storage_ref<t_char>{
	    {
	        reinterpret_cast<std::byte*>(
	            std::addressof(selected)),
	        value.storage_extent(index),
	    },
	    std::addressof(selected),
	};
}

template<typename I>
inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(
    const t_ansistring& value, I index) {
	const t_char& selected = value.index(index);
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        reinterpret_cast<const std::byte*>(
	            std::addressof(selected)),
	        value.storage_extent(index),
	    },
	    std::addressof(selected),
	};
}

template<typename I>
inline t_char& tpcc_index_write(t_ansistring& value, I index) {
	p_uniquestring(value);
	return value.index(index);
}

template<std::size_t Capacity = 255>
inline t_shortstring<Capacity> tpcc_shortstring_from_c(
    const char* s, std::size_t length) {
	t_shortstring<Capacity> result{};
	const std::size_t stored_length = std::min(length, Capacity);
	result.length = t_char{static_cast<uint8_t>(stored_length)};
	if (stored_length != 0)
		memcpy(result.data, s, stored_length);
	return result;
}

inline t_shortstring<255> p_implicit(
    t_char value,
    m_implicit_target<t_shortstring<255>>) {
	t_shortstring<255> result{};
	result.length = 1;
	result.data[0] = value;
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
		m_runtime_error(201);
	std::memset(destination.data, value, byte_count);
}

inline void p_move(tpcc_const_storage_ref source,
    tpcc_storage_ref destination, t_sizeint count) {
	if (count <= 0)
		return;
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > source.size)
		m_runtime_error(201);
	if (byte_count > destination.size)
		m_runtime_error(201);
	std::memmove(destination.data, source.data, byte_count);
}

inline t_sizeint p_comparebyte(tpcc_const_storage_ref first,
    tpcc_const_storage_ref second, t_sizeint count) {
	if (count <= 0)
		return 0;
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > first.size)
		m_runtime_error(201);
	if (byte_count > second.size)
		m_runtime_error(201);
	const int comparison = std::memcmp(first.data, second.data, byte_count);
	return comparison < 0 ? -1 : comparison > 0 ? 1 : 0;
}

inline t_sizeint p_comparechar(tpcc_const_storage_ref first,
    tpcc_const_storage_ref second, t_sizeint count) {
	return p_comparebyte(first, second, count);
}

template<typename Needle, typename Haystack>
requires
    (tpcc_is_shortstring_v<Needle> ||
     std::is_same_v<Needle, t_ansistring>) &&
    (tpcc_is_shortstring_v<Haystack> ||
     std::is_same_v<Haystack, t_ansistring>)
inline t_longint p_pos(
    const Needle& needle, const Haystack& haystack) {
	const std::size_t needle_length =
	    static_cast<std::size_t>(
		needle.m_length());
	const std::size_t haystack_length =
	    static_cast<std::size_t>(
		haystack.m_length());
	if (needle_length == 0)
		return 1;
	if (needle_length > haystack_length)
		return 0;
	const std::size_t last =
	    haystack_length - needle_length;
	for (std::size_t offset = 0; offset <= last; ++offset) {
		if (std::memcmp(
			haystack.m_data() + offset,
			needle.m_data(),
			needle_length) == 0)
			return static_cast<t_longint>(offset + 1);
	}
	return 0;
}

template<typename Haystack>
requires
    tpcc_is_shortstring_v<Haystack> ||
    std::is_same_v<Haystack, t_ansistring>
inline t_longint p_pos(
    t_char needle, const Haystack& haystack) {
	const std::size_t length =
	    static_cast<std::size_t>(
		haystack.m_length());
	for (std::size_t offset = 0;
	     offset < length; ++offset) {
		if (haystack.m_data()[offset] ==
		    needle)
			return static_cast<t_longint>(offset + 1);
	}
	return 0;
}

// Pascal Copy uses one-based indices and returns the ordinary 255-byte
// ShortString type declared by System.
template<std::size_t Capacity>
inline t_shortstring<255> p_copy(
    const t_shortstring<Capacity>& value,
    t_sizeint index, t_sizeint count) {
	t_shortstring<255> result{};
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
	    decltype(result)::capacity,
	});
	result.length = t_char{static_cast<uint8_t>(copied)};
	if (copied != 0)
		std::memcpy(result.data, value.data + start, copied);
	return result;
}

inline t_ansistring p_copy(const t_ansistring& value, t_sizeint index, t_sizeint count) {
	return value.slice(index, count);
}

inline t_shortstring<255> p_copy(
    t_char value, t_sizeint index, t_sizeint count) {
	t_shortstring<255> source{};
	source.length = t_char{1};
	source.data[0] = value;
	return p_copy(source, index, count);
}

template<std::size_t Capacity>
inline void p_delete(
    t_shortstring<Capacity>& value,
    t_longint index, t_longint count) {
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
}

inline void p_delete(t_ansistring& value, t_longint index, t_longint count) {
	value.erase(index, count);
}

template<std::size_t SourceCapacity, std::size_t DestinationCapacity>
inline void p_insert(
    const t_shortstring<SourceCapacity>& source,
    t_shortstring<DestinationCapacity>& value,
    t_longint index) {
	if (source.length == 0)
		return;

	if (index < 1)
		index = 1;

	std::size_t start = static_cast<std::size_t>(index - 1);
	// Pascal Semantics: index > length acts as append
	if (start > value.length)
		start = value.length;

	// Copy the source payload before moving the destination tail. Besides the
	// ordinary Insert(S, S, ...) case, different-capacity references can alias
	// through Pascal pointer casts.
	t_shortstring<SourceCapacity> temp_source = source;
	const auto* p_src = &temp_source;

	// Truncation logic: Calculate how much of the source we can actually fit
	std::size_t max_insert = DestinationCapacity - start;
	std::size_t copy_count = std::min(static_cast<std::size_t>(p_src->length), max_insert);

	if (copy_count == 0)
		return;

	std::size_t max_tail =
	    DestinationCapacity - (start + copy_count);
	std::size_t tail = value.length - start;
	std::size_t tail_copy = std::min(tail, max_tail);

	// Shift existing characters to the right to make room (truncates the tail if max_tail is reached)
	if (tail_copy > 0) {
		std::memmove(value.data + start + copy_count, value.data + start, tail_copy);
	}

	// Copy the new characters from the source string into the gap
	std::memcpy(value.data + start, p_src->data, copy_count);

	// Update the Pascal length byte.
	value.length = static_cast<uint8_t>(start + copy_count + tail_copy);
}

template<std::size_t Capacity>
inline void p_insert(
    t_char source, t_shortstring<Capacity>& destination,
    t_longint index) {
	t_shortstring<1> one_character{};
	one_character.length = 1;
	one_character.data[0] = source;
	p_insert(one_character, destination, index);
}

inline void p_insert(const t_ansistring& source, t_ansistring& destination, t_longint index) {
	destination.insert(source, index);
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_shortstring<255> p_add(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	t_shortstring<255> result{};
	const std::size_t result_length = std::min<std::size_t>(
	    static_cast<std::size_t>(a.length) + static_cast<std::size_t>(b.length),
	    decltype(result)::capacity);
	const std::size_t a_length = std::min<std::size_t>(a.length, result_length);
	const std::size_t b_length = result_length - a_length;
	result.length = static_cast<uint8_t>(result_length);
	memcpy(result.data, a.data, a_length);
	memcpy(&result.data[a_length], b.data, b_length);
	return result;
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline int tpcc_stringcmp(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	int r = memcmp(a.data, b.data, std::min(a.length, b.length));
	if (r == 0) {
		return (int) a.length - (int) b.length;
	}
	return r;
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_boolean p_lessthan(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) < 0);
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_boolean p_lessthanorequal(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) <= 0);
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_boolean p_equal(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) == 0);
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_boolean p_greaterthan(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) > 0);
}

template<std::size_t ACapacity, std::size_t BCapacity>
inline t_boolean p_greaterthanorequal(
    const t_shortstring<ACapacity>& a,
    const t_shortstring<BCapacity>& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) >= 0);
}

inline t_char p_implicit(
    t_char value,
    m_implicit_target<t_char>) {
	return value;
}
template<std::size_t Capacity>
inline t_ansistring p_implicit(
    t_shortstring<Capacity> value,
    m_implicit_target<t_ansistring>) {
	t_ansistring result{};
	result.assign(value);
	return result;
}
inline t_boolean p_lessthan(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value < b.value); }
inline t_boolean p_lessthanorequal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value <= b.value); }
inline t_boolean p_equal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value == b.value); }
inline t_boolean p_greaterthan(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value > b.value); }
inline t_boolean p_greaterthanorequal(t_char a, t_char b) { return tpcc_bool_to_boolean(a.value >= b.value); }

// Pascal Pointer equality compares pointer values; it does not inspect the
// pointed-to storage. Typed pointers reach this overload through Pascal's
// existing typed-pointer/untyped-Pointer compatibility conversion.
inline t_boolean p_equal(t_pointer a, t_pointer b) {
	return tpcc_bool_to_boolean(a == b);
}

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
template<typename T>
requires requires(const T& value) {
	value.m_low();
}
inline auto p_low(const T& value) {
	return value.m_low();
}
template<typename T>
requires requires(const T& value) {
	value.m_high();
}
inline auto p_high(const T& value) {
	return value.m_high();
}
template<typename T>
requires requires(const T& value) {
	value.m_length();
}
inline auto p_length(const T& value) {
	// Length is one Pascal operation, but representation and exact result type
	// belong to the carrier (for example Byte for short strings and SizeInt
	// for dynamic arrays). The generic wrapper must not reproduce those rules.
	return value.m_length();
}

template<typename T>
inline auto p_length(
    tpcc_typed_const_storage_ref<T> value)
    -> decltype(p_length(*value.value)) {
	return p_length(*value.value);
}

inline void p_setlength(
    t_ansistring& value,
    t_sizeint length) {
	value.m_resize(length);
}

template<typename T>
inline void p_setlength(
    tpcc_typed_storage_ref<T> value,
    t_sizeint length)
requires requires(T& sequence) {
	sequence.m_resize(length);
}
{
	value.value->m_resize(length);
}
template<typename T>
inline t_sizeint p_sizeof(tpcc_typed_const_storage_ref<T>) {
	return static_cast<t_sizeint>(sizeof(T));
}

#define TPCC_DEFINE_ARITHMETIC_OPERATIONS(T, ARITH_RESULT, DIV_RESULT) \
	inline ARITH_RESULT p_add(T a, T b) { return static_cast<ARITH_RESULT>(a) + static_cast<ARITH_RESULT>(b); } \
	inline ARITH_RESULT p_subtract(T a, T b) { return static_cast<ARITH_RESULT>(a) - static_cast<ARITH_RESULT>(b); } \
	inline T p_positive(T b) { return +b; } \
	/* For unsigned T, unary minus wraps modulo T's range; this is intentional RTL behavior, not a widening or signed conversion. */ \
	inline T p_negative(T b) { return -b; } \
	inline ARITH_RESULT p_multiply(T a, T b) { return static_cast<ARITH_RESULT>(a) * static_cast<ARITH_RESULT>(b); } \
	inline DIV_RESULT p_divide(T a, T b) { return static_cast<DIV_RESULT>(a) / static_cast<DIV_RESULT>(b); } \
	inline T p_implicit(T source, m_implicit_target<T>) { T target = source; return target; } \
	inline t_boolean p_lessthan(T a, T b) { return tpcc_bool_to_boolean(a < b); } \
	inline t_boolean p_lessthanorequal(T a, T b) { return tpcc_bool_to_boolean(a <= b); } \
	inline t_boolean p_equal(T a, T b) { return tpcc_bool_to_boolean(a == b); } \
	inline t_boolean p_greaterthan(T a, T b) { return tpcc_bool_to_boolean(a > b); } \
	inline t_boolean p_greaterthanorequal(T a, T b) { return tpcc_bool_to_boolean(a >= b); }

#define TPCC_DEFINE_INTEGER_OPERATIONS(T, INTEGER_RESULT) \
	/* Delphi calls unary `not` LogicalNot even for integer bitwise complement; there is no separate BitwiseNot overload name. */ \
	inline T p_logicalnot(T a) { return static_cast<T>(~a); } \
	inline INTEGER_RESULT p_bitwiseand(T a, T b) { return static_cast<INTEGER_RESULT>(a) & static_cast<INTEGER_RESULT>(b); } \
	inline INTEGER_RESULT p_bitwiseor(T a, T b) { return static_cast<INTEGER_RESULT>(a) | static_cast<INTEGER_RESULT>(b); } \
	inline INTEGER_RESULT p_bitwisexor(T a, T b) { return static_cast<INTEGER_RESULT>(a) ^ static_cast<INTEGER_RESULT>(b); } \
	inline INTEGER_RESULT p_intdivide(T a, T b) { return static_cast<INTEGER_RESULT>(a) / static_cast<INTEGER_RESULT>(b); } \
	inline INTEGER_RESULT p_modulus(T a, T b) { return static_cast<INTEGER_RESULT>(a) % static_cast<INTEGER_RESULT>(b); } \
	inline T p_leftshift(T a, T b) { return a << b; } /* FIXME: b smaller */ \
	inline T p_rightshift(T a, T b) { return a >> b; } /* FIXME: b smaller */

#define TPCC_DEFINE_INTEGRAL_OPERATIONS(T, INTEGER_RESULT) \
	TPCC_DEFINE_ARITHMETIC_OPERATIONS(T, INTEGER_RESULT, t_double) \
	TPCC_DEFINE_INTEGER_OPERATIONS(T, INTEGER_RESULT)

TPCC_DEFINE_INTEGRAL_OPERATIONS(t_byte, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_shortint, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_word, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_smallint, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_longword, t_longword)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_integer, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_int64, t_int64)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_qword, t_qword)
TPCC_DEFINE_ARITHMETIC_OPERATIONS(t_single, t_single, t_single)
TPCC_DEFINE_ARITHMETIC_OPERATIONS(t_double, t_double, t_double)
TPCC_DEFINE_ARITHMETIC_OPERATIONS(t_extended, t_extended, t_extended)

// Floating-to-integer conversion is undefined in C++ when the finite value is
// outside the destination range (and for NaN/infinity). Check before casting
// so Pascal Trunc/Round never rely on C++ undefined behavior.
inline t_int64 tpcc_checked_real_to_int64(t_extended value, const char* operation) {
	(void)operation;
	constexpr t_extended limit = 0x1p63L;
	if (!__builtin_isfinite(value) || value < -limit || value >= limit)
		m_runtime_error(201);
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

inline t_boolean p_implicit(
    t_boolean b,
    m_implicit_target<t_boolean>) {
	return b;
}

inline t_boolean p_assigned(const void* p) {
	return tpcc_bool_to_boolean(p != nullptr);
}

template<typename Signature>
inline t_boolean p_assigned(m_proc<Signature> p) {
	return tpcc_bool_to_boolean(p != nullptr);
}

template<typename Signature>
inline t_boolean p_assigned(const m_method<Signature>& p) {
	return tpcc_bool_to_boolean(p.p_code != nullptr);
}

template<typename Signature>
inline t_boolean m_equal(
    m_proc<Signature> a, m_proc<Signature> b) {
	return tpcc_bool_to_boolean(a == b);
}

template<typename Signature>
inline t_boolean m_equal(
    const m_method<Signature>& a,
    const m_method<Signature>& b) {
	// FPC's method-routine equality compares the code word. Data is copied
	// but is not part of this operation.
	return tpcc_bool_to_boolean(a.p_code == b.p_code);
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

template<typename T, std::size_t Capacity>
requires std::is_integral_v<T>
inline void p_str(T x, t_shortstring<Capacity>& s) {
	char buf[128];
	int n;
	if constexpr (std::is_signed_v<T>)
		n = std::snprintf(buf, sizeof(buf), "%lld", (long long)x);
	else
		n = std::snprintf(buf, sizeof(buf), "%llu", (unsigned long long)x);
	if (n < 0)
		n = 0;
	if (static_cast<std::size_t>(n) > Capacity)
		n = static_cast<int>(Capacity);
	s.length = static_cast<uint8_t>(n);
	memcpy(s.data, buf, s.length);
}

template<std::size_t Capacity>
inline void p_str(t_extended x, t_shortstring<Capacity>& s) {
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
		s = tpcc_shortstring_from_c<Capacity>(
		    formatted, sizeof(formatted));
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

	s = tpcc_shortstring_from_c<Capacity>(
	    formatted, static_cast<std::size_t>(output - formatted));
}

struct tpcc_val_prefix {
	std::size_t position;
	unsigned base;
	bool negative;
};

template<std::size_t Capacity>
inline tpcc_val_prefix tpcc_val_parse_prefix(
    const t_shortstring<Capacity>& source, t_integer& code) {
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

template<typename T, std::size_t Capacity>
requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
inline void p_val(
    const t_shortstring<Capacity>& source,
    T& destination, t_integer& code) {
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

template<typename T, std::size_t Capacity>
requires std::is_floating_point_v<T>
inline void p_val(
    const t_shortstring<Capacity>& source,
    T& destination, t_integer& code) {
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

template<typename T, typename Code, std::size_t Capacity>
requires ((std::is_integral_v<T> && (!std::is_same_v<T, bool>)) ||
	          std::is_floating_point_v<T>) &&
	         std::is_integral_v<Code> && (!std::is_same_v<Code, bool>)
inline void p_val(const t_shortstring<Capacity>& source, T& destination,
	    tpcc_typed_storage_ref<Code> code) {
	t_integer parsed_code = 0;
	p_val(source, destination, parsed_code);
	*code.value = static_cast<Code>(parsed_code);
}

template<typename T, std::size_t Capacity>
requires (std::is_integral_v<T> && (!std::is_same_v<T, bool>)) ||
	         std::is_floating_point_v<T>
inline void p_val(
    const t_shortstring<Capacity>& source, T& destination) {
	t_integer code = 0;
	p_val(source, destination, code);
}

template<typename T>
requires std::is_signed_v<T> && std::is_integral_v<T>
inline t_shortstring<255> tpcc_octstr_signed(T value, t_byte count) {
	using unsigned_type = std::make_unsigned_t<T>;
	constexpr unsigned width = std::numeric_limits<unsigned_type>::digits;
	unsigned_type bits = std::bit_cast<unsigned_type>(value);
	const bool negative = value < 0;

	t_shortstring<255> result{};
	result.length = t_char{count};
	for (std::size_t i = count; i != 0; --i) {
		result.data[i - 1] =
		    t_char{static_cast<uint8_t>('0' + (bits & unsigned_type{7}))};
		bits >>= 3;
		if (negative)
			bits |= static_cast<unsigned_type>(
			    ~unsigned_type{0} << (width - 3));
	}
	return result;
}

inline t_shortstring<255> p_octstr(
    t_longint value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring<255> p_octstr(
    t_int64 value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring<255> p_octstr(
    t_qword value, t_byte count) {
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
	if (!destination && size != 0)
		m_runtime_error(203);
}

inline t_pointer p_getmem(t_ptruint size) {
	t_pointer result =
	    std::malloc(static_cast<std::size_t>(size));
	if (!result && size != 0)
		m_runtime_error(203);
	return result;
}

inline void p_freemem(t_pointer value, t_ptruint size) {
	(void)size;
	std::free(value);
}

inline t_ptruint p_freemem(t_pointer value) {
	std::free(value);
	return 0;
}

// A generated metaclass cannot define `new T` until its enclosing object T is
// complete, so its out-of-class m_allocate definition calls this template.
// Abstract generated C++ classes must still have a valid metaclass vtable;
// they produce no instance and are rejected by construction before use.
template<typename T>
inline T* m_allocate_object() {
	if constexpr (std::is_abstract_v<T>)
		return nullptr;
	else {
		try {
			return new T{};
		} catch (const std::bad_alloc&) {
			m_runtime_error(203);
		}
	}
}

// Default TObject.NewInstance preserves the dynamic metaclass receiver. Every
// generated metaclass overrides m_allocate covariantly, so an inherited
// NewInstance body allocates the exact represented object class.
template<typename Object>
inline Object* m_new_instance(
    m_classref<Object>* meta) {
	return static_cast<typename Object::m_meta*>(
	           meta)
	    ->m_allocate();
}

template<typename Object>
inline void m_free_object(Object* object) {
	delete object;
}

// Plain Pascal New allocates the exact pointed-to carrier. Deliberately omit
// braces: old-style object and record scalar storage is not generally
// zero-initialized. Native C++ default initialization still constructs
// managed carrier members and, for polymorphic objects, installs the exact
// vptr before any Pascal initializer method runs.
template<typename T>
inline T* m_new_value() {
	try {
		return new T;
	} catch (const std::bad_alloc&) {
		m_runtime_error(203);
	}
}

// An ordinary constructor call on existing storage has no result in Pascal.
// Fail exits that initializer without becoming visible as a Pascal exception.
// Allocation-owning calls use m_construct/m_new_object instead so they can
// turn the same marker into nil and release their storage.
template<typename Initializer>
inline void m_invoke_initializer(
    Initializer&& initializer) {
	try {
		std::forward<Initializer>(
		    initializer)();
	} catch (const tpcc_constructor_fail&) {
	}
}

// Old-style object New owns allocation but not the initializer declaration.
// T is the exact pointed-to object, while Initializer may name an inherited
// nonvirtual constructor. Applying that base member pointer to T preserves
// the already-installed most-derived C++ virtual dispatch inside its body.
template<typename T, auto Initializer,
         typename... Args>
inline T* m_new_object(Args&&... args) {
	std::unique_ptr<T> object;
	try {
		object.reset(new T);
	} catch (const std::bad_alloc&) {
		m_runtime_error(203);
	}
	try {
		(object.get()->*Initializer)(
		    std::forward<Args>(args)...);
	} catch (const tpcc_constructor_fail&) {
		return nullptr;
	}
	return object.release();
}

template<typename T>
inline void m_dispose_value(T* object) {
	delete object;
}

// Pascal Done is an ordinary, possibly virtual method. The unique_ptr is
// armed before entering it so carrier/managed-field teardown still occurs if
// Done raises. Its hidden C++ virtual destructor contains no Pascal body and
// solely makes deletion through a VMT-bearing ancestor exact.
template<auto Finalizer, typename T>
inline void m_dispose_object(T* object) {
	if (!object)
		return;
	std::unique_ptr<T> storage(object);
	(object->*Finalizer)();
}

// Construct is the one allocation boundary. Initializer remains an ordinary
// Unit-returning object method, so inherited and virtual constructor bodies
// use the already allocated most-derived C++ object. The pointer-to-member
// template argument is the declaration selected by Pascal overload
// resolution; C++ is not asked to select the overload again.
template<typename Result, auto Initializer,
         typename Meta, typename... Args>
inline Result* m_construct(
    Meta* meta, Args&&... args) {
	auto* object =
	    static_cast<Result*>(
	        meta->p_newinstance());
	if (!object)
		m_runtime_error(203);
	try {
		(object->*Initializer)(
		    std::forward<Args>(args)...);
		object->p_afterconstruction();
		return object;
	} catch (const tpcc_constructor_fail&) {
		delete object;
		return nullptr;
	} catch (...) {
		delete object;
		throw;
	}
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
		virtual t_shortstring<255> p_classname() {
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
		/*not virtual*/ inline static t_shortstring<255> p_classname() {
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
	virtual t_shortstring<255> p_classname() = 0;
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

} // namespace u_system
