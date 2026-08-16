// Mini-pascal runtime library (header-only).
//
// Emitted C++ references the names in namespace `pas`. The compiler's
// intrinsic-type and builtin-procedure descriptor tables (see builtins.h)
// bind each Pascal name to its rtl counterpart here. Behavior lives here;
// the descriptor tables hold the Pascal-to-rtl name binding.
//
// Naming convention:
//   t_<name>  - a Pascal-visible TYPE
//   p_<name>  - a Pascal-visible ordinary value, procedure, or function
//   o_<name>  - a Pascal operator operation
//   m_<name>  - Pascal-invisible views that are used by the compiler
// Anything else in this namespace is implementation detail and not reachable
// from Pascal source.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstddef> // for std::byte
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <exception>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <memory>
#include <new>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>

// These must expand at the generated Pascal call site. Wrapping the compiler
// builtins in an ordinary C++ function would insert that wrapper's frame and
// report the wrong routine. Their System declarations therefore use these
// deliberately unqualified `m_...` external names rather than the usual
// `::u_system::p_...` convention. The distinct prefix also keeps these global
// preprocessor names from colliding with `p_...` C++ identifiers generated for
// ordinary Pascal declarations.
//
// Get_Frame uses the compiler builtin at the call site. The caller operations
// pass that explicit frame to ABI helpers below; unlike level-1 frame builtins,
// this both honors their Pascal operand and avoids GCC's deliberately fatal
// -Wframe-address diagnostic under -Werror.
#define m_get_frame() (__builtin_frame_address(0))
#define m_get_caller_addr(framebp, address) (::u_system::m_caller_addr_from_frame((framebp), (address)))
#define m_get_caller_frame(framebp, address) (::u_system::m_caller_frame_from_frame((framebp), (address)))

namespace u_system {

// C++ does not include a function result in overload identity. Pascal
// conversion-operator selection does include the context-requested
// destination, so generated Explicit, Implicit, and UncheckedImplicit
// declarations and calls carry this otherwise-empty backend parameter. The
// tag deliberately does not encode which conversion family was selected:
// that identity is already in the emitted callable name, while this
// parameter exists only to preserve result-type overloading. It is not a
// Pascal formal and the conversion remains an ordinary value-returning
// operation.
template <typename Destination> struct m_conversion_target {};

// Pascal declarations decide which direct integer assignment edges exist.
// This backend helper merely evaluates the already-selected edge; keeping it
// generic avoids duplicating the same natural C++ conversion for every
// source/destination declaration in system.pp.
template <typename Source, typename Destination>
        requires(std::is_integral_v<Source> && std::is_integral_v<Destination>)
inline Destination o_implicit(Source source, m_conversion_target<Destination>) {
	return static_cast<Destination>(source);
}

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
template <typename T> struct tpcc_return_transfer {
	unsigned next_try_depth;
	T value;
};

template <> struct tpcc_return_transfer<void> {
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
template <typename Root> class tpcc_pascal_exception {
	std::unique_ptr<Root> raised_object;
	void* raised_address;
	void* raised_frame;

      public:
	tpcc_pascal_exception(std::unique_ptr<Root> object, void* address, void* frame) : raised_object(std::move(object)), raised_address(address), raised_frame(frame) {
	}

	tpcc_pascal_exception(tpcc_pascal_exception&&) noexcept = default;
	tpcc_pascal_exception& operator=(tpcc_pascal_exception&&) noexcept = default;
	tpcc_pascal_exception(const tpcc_pascal_exception&) = delete;
	tpcc_pascal_exception& operator=(const tpcc_pascal_exception&) = delete;

	Root* object() const noexcept {
		return raised_object.get();
	}

	void* address() const noexcept {
		return raised_address;
	}

	void* frame() const noexcept {
		return raised_frame;
	}

	template <typename T> T* get_if() const noexcept {
		return dynamic_cast<T*>(raised_object.get());
	}

	std::unique_ptr<Root> release_if(Root* object) noexcept {
		if (raised_object.get() != object) {
			return {};
		}
		return std::move(raised_object);
	}
};

// While a Pascal handler executes, its caught carrier is the owner of the
// handler variable's object. `raise E` is an explicit new raise, but E may
// still designate that same object (directly or through another variable).
// Transfer its existing ownership into the new carrier instead of giving the
// same raw pointer two owners. The linked stack also handles an outer handler
// object raised from inside a nested handler.
template <typename Root> class tpcc_pascal_exception_scope {
	tpcc_pascal_exception<Root>& exception;
	tpcc_pascal_exception_scope* previous;
	inline static thread_local tpcc_pascal_exception_scope* active = nullptr;

      public:
	explicit tpcc_pascal_exception_scope(tpcc_pascal_exception<Root>& exception) noexcept : exception(exception), previous(active) {
		active = this;
	}

	~tpcc_pascal_exception_scope() {
		active = previous;
	}

	tpcc_pascal_exception_scope(const tpcc_pascal_exception_scope&) = delete;
	tpcc_pascal_exception_scope& operator=(const tpcc_pascal_exception_scope&) = delete;

	static std::unique_ptr<Root> take_if_active(Root* object) noexcept {
		for (auto* scope = active; scope; scope = scope->previous) {
			auto owned = scope->exception.release_if(object);
			if (owned) {
				return owned;
			}
		}
		return {};
	}
};

template <typename Root> [[noreturn]] inline void m_raise_pascal(Root* object, void* address, void* frame) {
	auto ownership = tpcc_pascal_exception_scope<Root>::take_if_active(object);
	if (!ownership) {
		ownership.reset(object);
	}
	throw tpcc_pascal_exception<Root>{std::move(ownership), address, frame};
}

// Keep target-specific call-site recovery in one helper. noinline makes the
// return address the generated Pascal raise site on the supported GCC/Clang
// SysV target instead of an inlined point inside its containing expression.
template <typename Root> [[noreturn, gnu::noinline]] inline void m_raise_pascal(Root* object) {
	void* frame = __builtin_frame_address(0);
	void* address = __builtin_extract_return_addr(__builtin_return_address(0));
	m_raise_pascal(object, address, frame);
}

using t_byte = uint8_t;
using t_shortint = int8_t;
using t_word = uint16_t;
using t_smallint = int16_t;
using t_longword = uint32_t;
using t_integer = int32_t;
using t_longint = int32_t;
using t_int64 = int64_t;
using t_qword = uint64_t;
using t_pointer = void*;
// FIXME: A 32-bit -P target must make these aliases t_integer/t_longword.
// The current RTL implements System's 64-bit Int64/QWord aliases exactly.
using t_ptrint = t_int64;
using t_ptruint = t_qword;
using t_sizeint = t_int64;
using t_sizeuint = t_qword;
using t_single = float;
using t_double = double;
using t_extended = long double;

inline t_pointer m_frame_word(t_pointer frame, std::size_t index) {
	if (!frame) {
		return nullptr;
	}
	t_pointer result;
	// The supported flat GCC/Clang ABIs store the previous frame pointer and
	// return address as the first two pointer-sized words of a materialized
	// frame. memcpy avoids pretending those ABI-maintained bytes are live C++
	// void* objects for aliasing and lifetime purposes.
	std::memcpy(&result, static_cast<std::byte*>(frame) + index * sizeof(t_pointer), sizeof(result));
	return result;
}

inline t_pointer m_caller_frame_from_frame(t_pointer frame, t_pointer address) {
	return m_frame_word(frame, 0);
}

inline t_pointer m_caller_addr_from_frame(t_pointer frame, t_pointer address) {
	t_pointer stored = m_frame_word(frame, 1);
	return stored ? __builtin_extract_return_addr(stored) : nullptr;
}

// Runtime handle base for Pascal `class of T`. The target is allowed to be
// incomplete: this empty specialization never inspects T. Every generated
// T::m_meta inherits m_classref<T>, so a class-reference value is an ordinary
// pointer to that marker subobject. Different T arguments remain different
// C++ types (and therefore keep overload signatures distinct) without adding
// a pointer field or requiring T::m_meta to be nameable at the declaration.
template <typename Target> struct m_classref {};

static_assert(std::is_empty_v<m_classref<void>>);

// Pascal's public, untyped method-pointer view. The compiler registers these
// members under the Pascal spellings Code and Data; their C++ spellings follow
// the RTL p_<name> convention for Pascal-visible values.
struct t_tmethod {
	t_pointer p_code;
	t_pointer p_data;
};

template <typename Signature> using m_proc = Signature*;

// Explicit Pascal casts between routine types may change only by-value data
// pointer parameter types; the compiler checks that the result, parameter
// modes, arity, and one-word versus two-word representation remain unchanged.
//
// Calling the resulting pointer is not defined by portable ISO C++20 when the
// function types differ. TPCC deliberately targets the GNOME/GObject-capable
// data-pointer callback ABI documented in README.md, where object/data pointer
// parameters have one representation and calling convention. Keep this helper
// visibly separate from ordinary assignment so that ABI exception can never
// become an implicit C++ conversion.
template <typename TargetSignature, typename SourceSignature> inline m_proc<TargetSignature> m_explicit_routine_cast(m_proc<SourceSignature> value) noexcept {
	static_assert(sizeof(m_proc<TargetSignature>) == sizeof(m_proc<SourceSignature>));
	static_assert(std::is_trivially_copyable_v<m_proc<TargetSignature>>);
	static_assert(std::is_trivially_copyable_v<m_proc<SourceSignature>>);
	return std::bit_cast<m_proc<TargetSignature>>(value);
}

template <typename FunctionPointer> inline t_pointer m_function_to_code_pointer(FunctionPointer value) noexcept {
	static_assert(std::is_pointer_v<FunctionPointer>);
	static_assert(std::is_function_v<std::remove_pointer_t<FunctionPointer>>);
	static_assert(sizeof(FunctionPointer) == sizeof(t_pointer), "the target ABI must fit a function pointer in TMethod.Code");
	return std::bit_cast<t_pointer>(value);
}

template <typename FunctionPointer> inline FunctionPointer m_code_pointer_to_function(t_pointer value) noexcept {
	static_assert(std::is_pointer_v<FunctionPointer>);
	static_assert(std::is_function_v<std::remove_pointer_t<FunctionPointer>>);
	static_assert(sizeof(FunctionPointer) == sizeof(t_pointer), "the target ABI must fit a function pointer in TMethod.Code");
	return std::bit_cast<FunctionPointer>(value);
}

template <typename Signature> struct m_method;

template <typename Result, typename... Args> struct m_method<Result(Args...)> {
	t_pointer p_code;
	t_pointer p_data;

	Result operator()(Args... args) const {
		using invoke_type = Result (*)(void*, Args...);
		auto invoke = m_code_pointer_to_function<invoke_type>(p_code);
		return invoke(p_data, std::forward<Args>(args)...);
	}
};

template <typename TargetSignature, typename SourceSignature> inline m_method<TargetSignature> m_explicit_routine_cast(m_method<SourceSignature> value) noexcept {
	static_assert(sizeof(m_method<TargetSignature>) == sizeof(m_method<SourceSignature>));
	static_assert(alignof(m_method<TargetSignature>) == alignof(m_method<SourceSignature>));
	static_assert(std::is_trivially_copyable_v<m_method<TargetSignature>>);
	static_assert(std::is_trivially_copyable_v<m_method<SourceSignature>>);
	return std::bit_cast<m_method<TargetSignature>>(value);
}

template <auto Method> struct m_method_adapter;

template <typename Owner, typename Result, typename... Args, Result (Owner::*Method)(Args...)> struct m_method_adapter<Method> {
	using owner_type = Owner;
	using signature_type = Result(Args...);

	static Result invoke(void* data, Args... args) {
		return (static_cast<Owner*>(data)->*Method)(std::forward<Args>(args)...);
	}
};

template <auto Method, typename Object> inline auto m_bind_method(Object* object) noexcept {
	using adapter = m_method_adapter<Method>;
	using owner = typename adapter::owner_type;
	using signature = typename adapter::signature_type;
	owner* adjusted = static_cast<owner*>(object);
	return m_method<signature>{
	    m_function_to_code_pointer(&adapter::invoke),
	    static_cast<void*>(adjusted),
	};
}

template <auto Function> struct m_receiver_function_adapter;

template <typename Owner, typename Result, typename... Args, Result (*Function)(Owner*, Args...)> struct m_receiver_function_adapter<Function> {
	using owner_type = Owner;
	using signature_type = Result(Args...);

	static Result invoke(void* data, Args... args) {
		return Function(static_cast<Owner*>(data), std::forward<Args>(args)...);
	}
};

template <auto Function, typename Object> inline auto m_bind_receiver_function(Object* object) noexcept {
	using adapter = m_receiver_function_adapter<Function>;
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
static_assert(offsetof(t_tmethod, p_code) == offsetof(m_method<void()>, p_code));
static_assert(offsetof(t_tmethod, p_data) == offsetof(m_method<void()>, p_data));
static_assert(sizeof(t_tmethod) == sizeof(m_method<void()>));
static_assert(alignof(t_tmethod) == alignof(m_method<void()>));

template <typename Signature> inline t_tmethod m_method_to_tmethod(m_method<Signature> value) noexcept {
	static_assert(sizeof(m_method<Signature>) == sizeof(t_tmethod));
	static_assert(alignof(m_method<Signature>) == alignof(t_tmethod));
	static_assert(std::is_trivially_copyable_v<m_method<Signature>>);
	return std::bit_cast<t_tmethod>(value);
}

template <typename Signature> inline m_method<Signature> m_tmethod_to_method(t_tmethod value) noexcept {
	static_assert(sizeof(m_method<Signature>) == sizeof(t_tmethod));
	static_assert(alignof(m_method<Signature>) == alignof(t_tmethod));
	static_assert(std::is_trivially_copyable_v<m_method<Signature>>);
	return std::bit_cast<m_method<Signature>>(value);
}

template <typename Signature> inline void m_store_tmethod_code(m_method<Signature>& destination, t_pointer value) noexcept {
	t_tmethod public_value = m_method_to_tmethod(destination);
	public_value.p_code = value;
	destination = m_tmethod_to_method<Signature>(public_value);
}

template <typename Signature> inline void m_store_tmethod_data(m_method<Signature>& destination, t_pointer value) noexcept {
	t_tmethod public_value = m_method_to_tmethod(destination);
	public_value.p_data = value;
	destination = m_tmethod_to_method<Signature>(public_value);
}

inline t_word p_errorcode = 0;
using m_error_proc = void (*)(t_longint, t_pointer, t_pointer);
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

[[noreturn]] inline void m_runtime_error(t_longint code, t_pointer address, t_pointer frame) {
	if (p_errorproc) {
		p_errorproc(code, address, frame);
	}
	// ErrorProc has a procedure type in Pascal rather than a noreturn type.
	// If a user-installed callback returns, retain System's ordinary runtime
	// error behavior instead of continuing after a failed RTL operation.
	p_runerror(static_cast<t_word>(code));
}

[[noreturn, gnu::noinline]] inline void m_runtime_error(t_longint code) {
	t_pointer frame = __builtin_frame_address(0);
	t_pointer address = __builtin_extract_return_addr(__builtin_return_address(0));
	m_runtime_error(code, address, frame);
}

// FPC's Char is an unsigned 8-bit ordinal, but it is nominally distinct from
// Byte. A wrapper preserves both facts in C++ overloads while remaining an
// inline, trivially-copyable one-byte value suitable for ShortString storage.
struct t_char {
	uint8_t value;

	constexpr t_char() = default;

	constexpr t_char(uint8_t value) : value(value) {
	}

	constexpr operator uint8_t() const {
		return value;
	}
};

static_assert(sizeof(t_char) == 1);
static_assert(alignof(t_char) == 1);
static_assert(std::is_trivially_copyable_v<t_char>);

// WideChar is a character-domain value, not another spelling of Word.
// Keeping a separate trivial wrapper preserves Pascal overload identity while
// fixing the representation at one unsigned 16-bit UTF-16 code unit on every
// backend target.
struct t_widechar {
	uint16_t value;

	constexpr t_widechar() = default;

	constexpr t_widechar(uint16_t value) : value(value) {
	}

	constexpr operator uint16_t() const {
		return value;
	}
};

static_assert(sizeof(t_widechar) == 2);
static_assert(alignof(t_widechar) == 2);
static_assert(std::is_trivially_copyable_v<t_widechar>);

template <typename T, typename Enable = void> struct tpcc_ordinal_storage {
	using type = T;

	static constexpr type get(T value) {
		return value;
	}

	static constexpr T make(type value) {
		return value;
	}
};

template <typename T> struct tpcc_ordinal_storage<T, std::enable_if_t<std::is_enum_v<T>>> {
	using type = std::underlying_type_t<T>;

	static constexpr type get(T value) {
		return static_cast<type>(value);
	}

	static constexpr T make(type value) {
		return static_cast<T>(value);
	}
};

template <> struct tpcc_ordinal_storage<t_char, void> {
	using type = uint8_t;

	static constexpr type get(t_char value) {
		return value.value;
	}

	static constexpr t_char make(type value) {
		return t_char{value};
	}
};

template <> struct tpcc_ordinal_storage<t_widechar, void> {
	using type = uint16_t;

	static constexpr type get(t_widechar value) {
		return value.value;
	}

	static constexpr t_widechar make(type value) {
		return t_widechar{value};
	}
};

template <typename T> struct tpcc_ordinal_storage<T, std::void_t<typename T::m_tpcc_ordinal_storage_type>> {
	// Generated subrange structs deliberately do not define an implicit
	// conversion to their storage member: that would let C++ conversions and
	// overload ranking run after Pascal has selected a declaration. This
	// compiler-private nested alias is the explicit bridge used by ordinal
	// RTL operations, and it also works for C++ local classes for which an
	// out-of-class trait specialization cannot be declared.
	using wrapped_type = typename T::m_tpcc_ordinal_storage_type;
	using wrapped_traits = tpcc_ordinal_storage<wrapped_type>;
	using type = typename wrapped_traits::type;

	static constexpr type get(T value) {
		return wrapped_traits::get(value.m_value);
	}

	static constexpr T make(type value) {
		return T{wrapped_traits::make(value)};
	}
};

// Pascal's explicit ordinal cast is a bit-width operation. Convert through
// unsigned storage, where C++20 defines modulo reduction, then bit-cast a
// signed destination so out-of-range unsigned-to-signed conversion is never
// implementation-defined.
template <typename Target, typename Source> constexpr Target m_ordinal_cast(Source source) {
	using source_traits = tpcc_ordinal_storage<Source>;
	using source_storage = typename source_traits::type;
	using target_traits = tpcc_ordinal_storage<Target>;
	using target_storage = typename target_traits::type;
	static_assert(std::is_integral_v<source_storage>);
	static_assert(std::is_integral_v<target_storage>);
	using target_unsigned = std::make_unsigned_t<target_storage>;
	const target_unsigned bits = static_cast<target_unsigned>(source_traits::get(source));
	target_storage stored;
	if constexpr (std::is_signed_v<target_storage>) {
		stored = std::bit_cast<target_storage>(bits);
	} else {
		stored = static_cast<target_storage>(bits);
	}
	return target_traits::make(stored);
}

template <typename T> constexpr auto m_ordinal_sign_magnitude(T value) {
	using traits = tpcc_ordinal_storage<T>;
	using storage = typename traits::type;
	static_assert(std::is_integral_v<storage>);
	using unsigned_storage = std::make_unsigned_t<storage>;
	const storage raw = traits::get(value);
	const bool negative = std::is_signed_v<storage> && raw < 0;
	const unsigned_storage bits = static_cast<unsigned_storage>(raw);
	const uint64_t magnitude = negative ? static_cast<uint64_t>(unsigned_storage{0} - bits) : static_cast<uint64_t>(bits);
	return std::pair<bool, uint64_t>{negative, magnitude};
}

template <typename Left, typename Right> constexpr bool m_ordinal_less(Left left, Right right) {
	const auto [left_negative, left_magnitude] = m_ordinal_sign_magnitude(left);
	const auto [right_negative, right_magnitude] = m_ordinal_sign_magnitude(right);
	if (left_negative != right_negative) {
		return left_negative;
	}
	if (left_magnitude == right_magnitude) {
		return false;
	}
	if (left_negative) {
		return left_magnitude > right_magnitude;
	}
	return left_magnitude < right_magnitude;
}

template <typename Target, typename Source, typename Lower, typename Upper> inline Target m_range_checked_ordinal_cast(Source source, Lower lower, Upper upper) {
	// SOURCE is a by-value parameter so an expression with side effects is
	// evaluated exactly once before both comparisons and the conversion.
	if (m_ordinal_less(source, lower) || m_ordinal_less(upper, source)) {
		m_runtime_error(201);
	}
	return m_ordinal_cast<Target>(source);
}

// C++ leaves an out-of-range floating-to-floating conversion undefined.
// Pascal's unchecked real narrowing still needs a stable result, so finite
// overflow is made explicit as signed infinity. Precision rounding and
// underflow remain the target format's ordinary conversion behavior, while an
// infinity or NaN already present in the source propagates to the corresponding
// special value supported by every TPCC real carrier.
template <typename Target, typename Source>
        requires std::is_floating_point_v<Target> && std::is_floating_point_v<Source>
inline Target m_real_cast(Source source) {
	static_assert(std::numeric_limits<Target>::has_infinity);
	static_assert(std::numeric_limits<Target>::has_quiet_NaN);
	if constexpr (std::numeric_limits<Target>::max_exponent < std::numeric_limits<Source>::max_exponent) {
		const Source maximum = static_cast<Source>(std::numeric_limits<Target>::max());
		if (__builtin_isfinite(source) && (source < -maximum || source > maximum)) {
			const Target infinity = std::numeric_limits<Target>::infinity();
			return __builtin_signbit(source) ? -infinity : infinity;
		}
	}
	return static_cast<Target>(source);
}

template <typename Target, typename Source>
        requires std::is_floating_point_v<Target> && std::is_floating_point_v<Source>
inline Target m_range_checked_real_cast(Source source) {
	// SOURCE is by value so the comparisons and conversion evaluate the
	// Pascal expression exactly once. Only finite overflow is a range failure:
	// ordinary precision rounding/underflow is inherent in real narrowing,
	// while existing infinities and NaNs are representable target values.
	if constexpr (std::numeric_limits<Target>::max_exponent < std::numeric_limits<Source>::max_exponent) {
		const Source maximum = static_cast<Source>(std::numeric_limits<Target>::max());
		if (__builtin_isfinite(source) && (source < -maximum || source > maximum)) {
			m_runtime_error(201);
		}
	}
	return m_real_cast<Target>(source);
}

using tpcc_unknown_type = void*;

inline t_boolean tpcc_bool_to_boolean(bool value) {
	return value ? p_true : p_false;
}

template <std::size_t Capacity> struct t_shortstring {
	static_assert(Capacity >= 1 && Capacity <= 255, "Pascal ShortString capacity must be in 1..255");
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
		return static_cast<t_integer>(m_length());
	}

	constexpr t_char* m_data() {
		return data;
	}

	constexpr const t_char* m_data() const {
		return data;
	}

	constexpr void m_resize(t_sizeint requested_length) {
		// A ShortString resize changes only its logical length byte. Payload
		// storage is inline and fixed by the Pascal type, so growing exposes
		// the existing bytes just as assigning S[0] does. Clamp instead of
		// storing a length beyond Capacity: every sequence consumer trusts
		// this invariant when indexing, iterating, and copying the value.
		const std::size_t new_length = requested_length <= 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(requested_length), Capacity);
		length = t_char{static_cast<uint8_t>(new_length)};
	}

	std::string m_string() const {
		std::string result;
		const std::size_t count = std::min<std::size_t>(length.value, Capacity);
		result.reserve(count);
		for (std::size_t i = 0; i < count; ++i) {
			result.push_back(static_cast<char>(data[i].value));
		}
		return result;
	}
};

template <typename T> struct tpcc_is_shortstring : std::false_type {};

template <std::size_t Capacity> struct tpcc_is_shortstring<t_shortstring<Capacity>> : std::true_type {};

template <typename T> inline constexpr bool tpcc_is_shortstring_v = tpcc_is_shortstring<std::remove_cv_t<T>>::value;

static_assert(sizeof(t_shortstring<255>) == 256);
static_assert(alignof(t_shortstring<255>) == 1);
static_assert(std::is_aggregate_v<t_shortstring<255>>);
static_assert(std::is_trivially_default_constructible_v<t_shortstring<255>>);
static_assert(std::is_trivially_copyable_v<t_shortstring<255>>);

template <std::size_t DestinationCapacity, std::size_t SourceCapacity> constexpr t_shortstring<DestinationCapacity> tpcc_shortstring_cast(const t_shortstring<SourceCapacity>& source) {
	t_shortstring<DestinationCapacity> result{};
	const std::size_t copied = std::min({
	    static_cast<std::size_t>(source.length.value),
	    SourceCapacity,
	    DestinationCapacity,
	});
	result.length = t_char{static_cast<uint8_t>(copied)};
	for (std::size_t i = 0; i < copied; ++i) {
		result.data[i] = source.data[i];
	}
	return result;
}

// File carriers contain only one opaque state pointer. The state owns all
// C++ implementation objects and host resources; no FILE, std::string, or
// stream object is embedded in Pascal storage.
//
// Copying is deleted at the carrier boundary as a second line of defence
// behind the Pascal semantic check. ISO 7185 does not make file values (or
// structures containing them) assignment-compatible. A raw pointer copy
// would otherwise create two apparent owners with no defined Close/Finalize
// behavior.
struct text_file_state;
void m_release_text_file_state(text_file_state*& state) noexcept;

struct t_text {
	text_file_state* state = nullptr;

	constexpr t_text() noexcept = default;

	explicit constexpr t_text(text_file_state* state) noexcept : state(state) {
	}

	t_text(const t_text&) = delete;
	t_text& operator=(const t_text&) = delete;
	~t_text() noexcept;
};

static_assert(sizeof(t_text) == sizeof(void*));
static_assert(alignof(t_text) == alignof(void*));

// Text, untyped binary files, and typed binary files are incompatible Pascal
// types even though all three currently carry one runtime-state pointer.
// Binary state is deliberately opaque here: file operations own its concrete
// handle, filename, mode, and record-size representation.
struct binary_file_state;
void m_release_binary_file_state(binary_file_state*& state) noexcept;

struct t_file {
	binary_file_state* state = nullptr;

	constexpr t_file() noexcept = default;
	t_file(const t_file&) = delete;
	t_file& operator=(const t_file&) = delete;
	~t_file() noexcept;
};

template <typename Element> struct t_typedfile {
	using element_type = Element;
	binary_file_state* state = nullptr;

	constexpr t_typedfile() noexcept = default;
	t_typedfile(const t_typedfile&) = delete;
	t_typedfile& operator=(const t_typedfile&) = delete;
	~t_typedfile() noexcept;
};

static_assert(sizeof(t_file) == sizeof(void*));
static_assert(alignof(t_file) == alignof(void*));
static_assert(std::is_standard_layout_v<t_file>);
static_assert(sizeof(t_typedfile<t_byte>) == sizeof(void*));
static_assert(alignof(t_typedfile<t_byte>) == alignof(void*));
static_assert(std::is_standard_layout_v<t_typedfile<t_byte>>);
static_assert(!std::is_copy_constructible_v<t_file>);
static_assert(!std::is_copy_assignable_v<t_file>);
static_assert(!std::is_copy_constructible_v<t_text>);
static_assert(!std::is_copy_assignable_v<t_text>);
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

template <typename T> struct tpcc_typed_storage_ref : tpcc_storage_ref {
	T* value;
};

template <typename T> struct tpcc_typed_const_storage_ref : tpcc_const_storage_ref {
	const T* value;
};

template <typename T> inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(T& value) {
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
template <typename Target, typename Source> inline void tpcc_store_writable_cast(tpcc_typed_storage_ref<Source> destination, Target value) {
	static_assert(sizeof(Target) == sizeof(Source), "writable Pascal cast requires equal-size carriers");
	static_assert(std::is_trivially_copyable_v<Target>, "writable Pascal cast target must be trivially copyable");
	static_assert(std::is_trivially_copyable_v<Source>, "writable Pascal cast source must be trivially copyable");
	if (destination.size < sizeof(Source)) {
		throw std::length_error("writable Pascal cast exceeds its storage view");
	}
	*destination.value = std::bit_cast<Source>(value);
}

template <typename T> inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(const T& value) {
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

template <typename Element> inline Element* tpcc_omitted_formal_byte_pointer(tpcc_storage_ref value) {
	// Pascal byte and character pointers are one-byte storage views. Char is
	// a wrapper carrier, so its aliasing semantics rely on the generated-code
	// contract's -fno-strict-aliasing; these assertions separately enforce the
	// representation and alignment assumptions needed at this boundary.
	static_assert(sizeof(Element) == 1 && alignof(Element) == 1 && std::is_trivially_copyable_v<Element>, "Pascal byte pointer target must be a trivial one-byte carrier");
	static_assert(!std::is_same_v<Element, t_byte> || std::is_same_v<t_byte, unsigned char>, "Pascal PByte object-representation access requires Byte to be unsigned char");
	return reinterpret_cast<Element*>(value.data);
}

template <typename Element> inline Element* tpcc_omitted_formal_byte_pointer(tpcc_const_storage_ref value) {
	// `const` prevents assignment to the formal designator but does
	// not make an ordinary pointer obtained from `@formal` pointer-to-const.
	// Preserve that source-language escape while keeping const qualification
	// on every operation which consumes the storage descriptor directly.
	return tpcc_omitted_formal_byte_pointer<Element>(tpcc_storage_ref{
	    const_cast<std::byte*>(value.data),
	    value.size,
	});
}

// Dereferencing Pascal's untyped Pointer does not produce a C++ value: void
// has no object representation that can be named by `*pointer`. It produces
// an unbounded raw storage place, which can be consumed by Pascal's omitted-
// type var/out/const parameters. Keep this operation distinct from
// tpcc_make_storage_ref(pointer_variable), which refers to the bytes occupied
// by the pointer variable itself.
inline tpcc_storage_ref tpcc_dereference_storage(t_pointer value) {
	if (!value) {
		m_runtime_error(216);
	}
	return tpcc_storage_ref{
	    reinterpret_cast<std::byte*>(value),
	    std::numeric_limits<std::size_t>::max(),
	};
}

inline tpcc_const_storage_ref tpcc_make_const_storage_ref(tpcc_const_storage_ref value) {
	return value;
}

inline tpcc_const_storage_ref tpcc_make_const_storage_ref(tpcc_storage_ref value) {
	return tpcc_const_storage_ref{
	    value.data,
	    value.size,
	};
}

struct tpcc_set_span {
	int64_t lower;
	int64_t upper;
};

template <typename T> struct t_set {
	std::vector<tpcc_set_span> spans;
};

template <typename Target, typename Source> inline t_set<Target> m_set_cast(const t_set<Source>& source) {
	return t_set<Target>{source.spans};
}

template <typename T> inline int64_t tpcc_set_key(T value) {
	using traits = tpcc_ordinal_storage<T>;
	return static_cast<int64_t>(traits::get(value));
}

template <typename T> inline tpcc_set_span tpcc_set_single(T value) {
	const int64_t key = tpcc_set_key(value);
	return tpcc_set_span{key, key};
}

template <typename T> inline tpcc_set_span tpcc_set_range(T lower, T upper) {
	return tpcc_set_span{tpcc_set_key(lower), tpcc_set_key(upper)};
}

template <typename T> inline t_set<T> tpcc_make_set(std::initializer_list<tpcc_set_span> spans) {
	return t_set<T>{std::vector<tpcc_set_span>(spans)};
}

template <typename T> inline t_set<T> m_set_union(const t_set<T>& first, const t_set<T>& second) {
	t_set<T> result = first;
	// A t_set is the union of its spans and deliberately does not require
	// canonical, sorted, or disjoint storage. Concatenation is therefore the
	// complete mathematical union and preserves the inexpensive literal and
	// Include representation already used by the RTL.
	result.spans.insert(result.spans.end(), second.spans.begin(), second.spans.end());
	return result;
}

template <typename T> inline t_set<T> m_set_difference(const t_set<T>& first, const t_set<T>& second) {
	t_set<T> result = first;
	for (const tpcc_set_span& removed : second.spans) {
		if (removed.upper < removed.lower) {
			continue;
		}
		std::vector<tpcc_set_span> remaining;
		remaining.reserve(result.spans.size() + 1);
		for (const tpcc_set_span& span : result.spans) {
			if (span.upper < span.lower) {
				continue;
			}
			if (removed.upper < span.lower || span.upper < removed.lower) {
				remaining.push_back(span);
				continue;
			}
			// The strict comparisons prove these endpoint adjustments
			// cannot overflow even at INT64_MIN/INT64_MAX.
			if (span.lower < removed.lower) {
				remaining.push_back(tpcc_set_span{span.lower, removed.lower - 1});
			}
			if (removed.upper < span.upper) {
				remaining.push_back(tpcc_set_span{removed.upper + 1, span.upper});
			}
		}
		result.spans = std::move(remaining);
	}
	return result;
}

template <typename T> inline t_set<T> o_unchecked_add(const t_set<T>& first, const t_set<T>& second) {
	return m_set_union(first, second);
}

template <typename T> inline t_set<T> o_add(const t_set<T>& first, const t_set<T>& second) {
	return m_set_union(first, second);
}

template <typename T> inline t_set<T> o_unchecked_subtract(const t_set<T>& first, const t_set<T>& second) {
	return m_set_difference(first, second);
}

template <typename T> inline t_set<T> o_subtract(const t_set<T>& first, const t_set<T>& second) {
	return m_set_difference(first, second);
}

template <typename T> inline t_set<T> m_set_intersection(const t_set<T>& first, const t_set<T>& second) {
	t_set<T> result;
	for (const tpcc_set_span& left : first.spans) {
		for (const tpcc_set_span& right : second.spans) {
			const int64_t lower = std::max(left.lower, right.lower);
			const int64_t upper = std::min(left.upper, right.upper);
			if (lower <= upper) {
				result.spans.push_back(tpcc_set_span{lower, upper});
			}
		}
	}
	return result;
}

template <typename T> inline t_set<T> o_unchecked_multiply(const t_set<T>& first, const t_set<T>& second) {
	return m_set_intersection(first, second);
}

template <typename T> inline t_set<T> o_multiply(const t_set<T>& first, const t_set<T>& second) {
	return m_set_intersection(first, second);
}

template <typename T> inline t_set<T> o_symmetric_difference(const t_set<T>& first, const t_set<T>& second) {
	return m_set_union(m_set_difference(first, second), m_set_difference(second, first));
}

template <typename T> inline t_boolean o_equal(const t_set<T>& first, const t_set<T>& second) {
	return tpcc_bool_to_boolean(m_set_difference(first, second).spans.empty() && m_set_difference(second, first).spans.empty());
}

template <typename T> inline t_boolean o_lessthanorequal(const t_set<T>& first, const t_set<T>& second) {
	return tpcc_bool_to_boolean(m_set_difference(first, second).spans.empty());
}

template <typename T> inline t_boolean o_greaterthanorequal(const t_set<T>& first, const t_set<T>& second) {
	return tpcc_bool_to_boolean(m_set_difference(second, first).spans.empty());
}

template <typename Value, typename T> inline t_boolean o_in(Value value, const t_set<T>& set) {
	const int64_t key = tpcc_set_key(value);
	for (const tpcc_set_span& span : set.spans) {
		if (span.lower <= key && key <= span.upper) {
			return p_true;
		}
	}
	return p_false;
}

template <typename Value, typename T> inline t_boolean o_in(tpcc_typed_const_storage_ref<Value> value, tpcc_typed_const_storage_ref<t_set<T>> set) {
	return o_in(*value.value, *set.value);
}

// VALUE is intentionally separate from T. The Pascal checker has already
// verified conversion to the set's item type, but an untyped integer literal
// is still emitted with its raw C++ literal carrier at an omitted-type call
// boundary. Convert that carrier here before deriving the ordinal set key.
template <typename T, typename Value> inline void p_include(tpcc_typed_storage_ref<t_set<T>> set, tpcc_typed_const_storage_ref<Value> item) {
	const T converted = m_ordinal_cast<T>(*item.value);
	const int64_t key = tpcc_set_key(converted);
	// t_set membership is the union of its spans; the carrier does not require
	// canonical or disjoint spans. Appending a singleton is therefore a
	// complete Include operation. Exclude below removes KEY from every span.
	set.value->spans.push_back(tpcc_set_span{key, key});
}

template <typename T, typename Value> inline void p_exclude(tpcc_typed_storage_ref<t_set<T>> set, tpcc_typed_const_storage_ref<Value> item) {
	const T converted = m_ordinal_cast<T>(*item.value);
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
		if (span.lower < key) {
			remaining.push_back(tpcc_set_span{span.lower, key - 1});
		}
		if (key < span.upper) {
			remaining.push_back(tpcc_set_span{key + 1, span.upper});
		}
	}
	set.value->spans = std::move(remaining);
}

template <typename T, std::size_t length, auto low> struct t_fixedarray {
	T items[length];

	constexpr t_sizeint m_length() const {
		return static_cast<t_sizeint>(length);
	}

	constexpr auto m_low() const {
		return low;
	}

	constexpr auto m_high() const {
		using value_type = decltype(low);
		using traits = tpcc_ordinal_storage<value_type>;
		using storage_type = typename traits::type;
		return traits::make(static_cast<storage_type>(traits::get(low) + static_cast<storage_type>(length - 1)));
	}

	constexpr T* m_data() {
		return items;
	}

	constexpr const T* m_data() const {
		return items;
	}

	template <typename I> constexpr T& operator[](I index) {
		return items[static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low)];
	}

	template <typename I> constexpr const T& operator[](I index) const {
		return items[static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low)];
	}
};

template <typename T> inline constexpr bool tpcc_is_fixed_byte_array_v = false;

template <std::size_t length, auto low> inline constexpr bool tpcc_is_fixed_byte_array_v<t_fixedarray<t_byte, length, low>> = true;

// TPCC's C++ backend contract already requires -fno-strict-aliasing for
// Pascal storage aliases such as `absolute`. A same-sized fixed array of Byte
// uses that same direct-reference model for a scalar's object representation.
// Rvalues and const lvalues receive a const view; only a mutable scalar place
// can produce the writable array reference admitted by the Pascal parser.
template <typename Array, typename Scalar> inline auto& tpcc_byte_array_storage_view(Scalar&& value) {
	using source_type = std::remove_reference_t<Scalar>;
	using bare_source_type = std::remove_const_t<source_type>;
	using view_type = std::conditional_t<std::is_lvalue_reference_v<Scalar&&> && !std::is_const_v<source_type>, Array, const Array>;
	static_assert(tpcc_is_fixed_byte_array_v<Array>, "Pascal object-representation view must be a fixed Byte array");
	static_assert(std::is_same_v<t_byte, unsigned char>, "Pascal Byte object-representation views require unsigned char");
	static_assert(std::is_trivially_copyable_v<bare_source_type>, "Pascal Byte-array view source must be trivially copyable");
	static_assert(std::is_trivially_copyable_v<Array>, "Pascal fixed Byte array must be trivially copyable");
	static_assert(sizeof(Array) == sizeof(bare_source_type), "Pascal Byte-array view must equal the scalar storage size");
	return reinterpret_cast<view_type&>(value);
}

// Pascal indexing is always emitted as an RTL call. The compiler never needs
// to know a container's C++ representation or lower bound.
template <typename T, std::size_t length, auto low, typename I> inline T& p_index(t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length) {
		m_runtime_error(201);
	}
	return value.items[static_cast<std::size_t>(actual - first)];
}

template <typename T, std::size_t length, auto low, typename I> inline const T& p_index(const t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length) {
		m_runtime_error(201);
	}
	return value.items[static_cast<std::size_t>(actual - first)];
}

template <typename T, std::size_t length, auto low, typename I> inline T& m_unchecked_index(t_fixedarray<T, length, low>& value, I index) {
	return value.items[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low))];
}

template <typename T, std::size_t length, auto low, typename I> inline const T& m_unchecked_index(const t_fixedarray<T, length, low>& value, I index) {
	return value.items[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low))];
}

template <typename T, std::size_t length, auto low, typename I> inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length) {
		m_runtime_error(201);
	}
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

template <typename T, std::size_t length, auto low, typename I> inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(const t_fixedarray<T, length, low>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	const std::ptrdiff_t first = static_cast<std::ptrdiff_t>(low);
	if (actual < first || static_cast<std::size_t>(actual - first) >= length) {
		m_runtime_error(201);
	}
	const std::size_t offset = static_cast<std::size_t>(actual - first);
	const auto* bytes = reinterpret_cast<const std::byte*>(std::addressof(value.items));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        bytes + offset * sizeof(T),
	        (length - offset) * sizeof(T),
	    },
	    std::addressof(value.items[offset]),
	};
}

template <typename T, std::size_t length, auto low, typename I> inline tpcc_typed_storage_ref<T> m_unchecked_storage_ref(t_fixedarray<T, length, low>& value, I index) {
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low));
	T& selected = m_unchecked_index(value, index);
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        (length - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, std::size_t length, auto low, typename I> inline tpcc_typed_const_storage_ref<T> m_unchecked_const_storage_ref(const t_fixedarray<T, length, low>& value, I index) {
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - static_cast<std::ptrdiff_t>(low));
	const T& selected = m_unchecked_index(value, index);
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        (length - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <std::size_t Capacity, typename I> inline t_char& p_index(t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > static_cast<std::ptrdiff_t>(Capacity)) {
		m_runtime_error(201);
	}
	if (actual == 0) {
		return value.length;
	}
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template <std::size_t Capacity, typename I> inline const t_char& p_index(const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > static_cast<std::ptrdiff_t>(Capacity)) {
		m_runtime_error(201);
	}
	if (actual == 0) {
		return value.length;
	}
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template <std::size_t Capacity, typename I> inline t_char& m_unchecked_index(t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual == 0) {
		return value.length;
	}
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template <std::size_t Capacity, typename I> inline const t_char& m_unchecked_index(const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual == 0) {
		return value.length;
	}
	return value.data[static_cast<std::size_t>(actual - 1)];
}

template <std::size_t Capacity, typename I> inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > static_cast<std::ptrdiff_t>(Capacity)) {
		m_runtime_error(201);
	}
	if (actual == 0) {
		return tpcc_typed_storage_ref<t_char>{
		    {
		        reinterpret_cast<std::byte*>(std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	}
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

template <std::size_t Capacity, typename I> inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual > static_cast<std::ptrdiff_t>(Capacity)) {
		m_runtime_error(201);
	}
	if (actual == 0) {
		return tpcc_typed_const_storage_ref<t_char>{
		    {
		        reinterpret_cast<const std::byte*>(std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	}
	const std::size_t offset = static_cast<std::size_t>(actual - 1);
	const auto* bytes = reinterpret_cast<const std::byte*>(std::addressof(value.data));
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        bytes + offset * sizeof(t_char),
	        Capacity - offset,
	    },
	    std::addressof(value.data[offset]),
	};
}

template <std::size_t Capacity, typename I> inline tpcc_typed_storage_ref<t_char> m_unchecked_storage_ref(t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual == 0) {
		return tpcc_typed_storage_ref<t_char>{
		    {
		        reinterpret_cast<std::byte*>(std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	}
	const std::size_t offset = static_cast<std::size_t>(actual - 1);
	t_char& selected = m_unchecked_index(value, index);
	return tpcc_typed_storage_ref<t_char>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        Capacity - offset,
	    },
	    std::addressof(selected),
	};
}

template <std::size_t Capacity, typename I> inline tpcc_typed_const_storage_ref<t_char> m_unchecked_const_storage_ref(const t_shortstring<Capacity>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual == 0) {
		return tpcc_typed_const_storage_ref<t_char>{
		    {
		        reinterpret_cast<const std::byte*>(std::addressof(value.length)),
		        sizeof(value.length),
		    },
		    std::addressof(value.length),
		};
	}
	const std::size_t offset = static_cast<std::size_t>(actual - 1);
	const t_char& selected = m_unchecked_index(value, index);
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        Capacity - offset,
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline T& p_index(T* value, I index) {
	if (!value) {
		m_runtime_error(216);
	}
	return value[static_cast<std::ptrdiff_t>(index)];
}

template <typename T, typename I> inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(T* value, I index) {
	if (!value) {
		m_runtime_error(216);
	}
	T* selected = value + static_cast<std::ptrdiff_t>(index);
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(selected),
	        std::numeric_limits<std::size_t>::max(),
	    },
	    selected,
	};
}

template <typename T, typename I> inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(const T* value, I index) {
	if (!value) {
		m_runtime_error(216);
	}
	const T* selected = value + static_cast<std::ptrdiff_t>(index);
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(selected),
	        std::numeric_limits<std::size_t>::max(),
	    },
	    selected,
	};
}

template <typename T> struct m_shared_block {
	std::atomic<std::size_t> references{1};
	std::vector<T> values;

	explicit m_shared_block(std::vector<T> values) : values(std::move(values)) {
	}
};

// The public managed carriers below must remain one pointer wide. This
// private handle supplies the shared reference-counting and exception-safe
// block replacement without imposing one mutation policy: dynamic arrays
// deliberately share element writes, while AnsiString detaches before a
// writable character escapes.
template <typename T> class m_shared_buffer {
	m_shared_block<T>* block = nullptr;

	void m_retain() noexcept {
		if (block) {
			block->references.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void m_release() noexcept {
		if (block && block->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			delete block;
		}
	}

      public:
	m_shared_buffer() = default;

	m_shared_buffer(const m_shared_buffer& other) : block(other.block) {
		m_retain();
	}

	m_shared_buffer(m_shared_buffer&& other) noexcept : block(std::exchange(other.block, nullptr)) {
	}

	~m_shared_buffer() {
		m_release();
	}

	m_shared_buffer& operator=(const m_shared_buffer& other) {
		if (this == &other) {
			return *this;
		}
		m_shared_buffer replacement(other);
		std::swap(block, replacement.block);
		return *this;
	}

	m_shared_buffer& operator=(m_shared_buffer&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		m_release();
		block = std::exchange(other.block, nullptr);
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

	const void* m_identity() const noexcept {
		return block;
	}

	void m_replace(std::vector<T> values) {
		auto* replacement = values.empty() ? nullptr : new m_shared_block<T>(std::move(values));
		m_release();
		block = replacement;
	}

	void m_make_unique() {
		if (!block || block->references.load(std::memory_order_acquire) == 1) {
			return;
		}
		std::vector<T> copy = block->values;
		m_replace(std::move(copy));
	}
};

static_assert(sizeof(m_shared_buffer<t_byte>) == sizeof(void*));

template <typename T> struct t_dynamicarray {
	m_shared_buffer<T> storage;

	t_sizeint m_length() const {
		return static_cast<t_sizeint>(storage.m_size());
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

	static t_dynamicarray m_from_values(std::initializer_list<T> values) {
		t_dynamicarray result;
		result.storage.m_replace(std::vector<T>(values.begin(), values.end()));
		return result;
	}

	void m_resize(t_sizeint requested_length) {
		const std::size_t new_length = requested_length <= 0 ? 0 : static_cast<std::size_t>(requested_length);
		std::vector<T> replacement(new_length);
		const std::size_t copied = std::min(storage.m_size(), new_length);
		std::copy_n(storage.m_data(), copied, replacement.data());
		// SetLength is a handle operation. Replacing the block even when the
		// size is unchanged ensures aliases retain their original array.
		storage.m_replace(std::move(replacement));
	}

	const void* m_identity() const noexcept {
		return storage.m_identity();
	}
};

static_assert(sizeof(t_dynamicarray<t_byte>) == sizeof(void*));
static_assert(alignof(t_dynamicarray<t_byte>) == alignof(void*));

template <typename T> struct t_openarray {
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

static_assert(sizeof(t_openarray<t_byte>) == sizeof(void*) * 2);

template <typename T> class m_openarray_owner {
	// A value open-array argument is a copy whose storage must survive until
	// the complete call expression finishes. The conversion below exposes a
	// descriptor while this owner remains the materialized argument object.
	std::vector<T> values;

      public:
	m_openarray_owner(const T* data, std::size_t count) : values() {
		if (count != 0) {
			values.assign(data, data + count);
		}
	}

	explicit m_openarray_owner(std::vector<T> values) : values(std::move(values)) {
	}

	operator t_openarray<T>() {
		return t_openarray<T>{
		    values.data(),
		    static_cast<t_sizeint>(values.size()),
		};
	}

	operator t_openarray<const T>() const {
		return t_openarray<const T>{
		    values.data(),
		    static_cast<t_sizeint>(values.size()),
		};
	}
};

template <typename T> inline m_openarray_owner<T> m_openarray_values(std::initializer_list<T> values) {
	return m_openarray_owner<T>{std::vector<T>(values.begin(), values.end())};
}

template <typename T, std::size_t N, auto Low> inline t_openarray<const T> m_openarray_const_view(const t_fixedarray<T, N, Low>& value) {
	return t_openarray<const T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template <typename T> inline t_openarray<const T> m_openarray_const_view(const t_dynamicarray<T>& value) {
	return t_openarray<const T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template <typename T> inline t_openarray<const std::remove_const_t<T>> m_openarray_const_view(t_openarray<T> value) {
	return {
	    value.m_data(),
	    value.m_length(),
	};
}

template <typename T, std::size_t N, auto Low> inline t_openarray<T> m_openarray_mutable_view(t_fixedarray<T, N, Low>& value) {
	return t_openarray<T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template <typename T> inline t_openarray<T> m_openarray_mutable_view(t_dynamicarray<T>& value) {
	return t_openarray<T>{
	    value.m_data(),
	    value.m_length(),
	};
}

template <typename T>
        requires(!std::is_const_v<T>)
inline t_openarray<T> m_openarray_mutable_view(t_openarray<T> value) {
	return value;
}

template <typename T> inline t_openarray<T> m_openarray_reset(t_openarray<T> value) {
	// The currently supported managed carriers implement Pascal default
	// initialization through assignment from T{}. Custom Initialize/Finalize
	// hooks require a separate lifetime protocol and must not be guessed here.
	for (t_sizeint i = 0; i < value.m_length(); ++i) {
		value.m_data()[i] = T{};
	}
	return value;
}

template <typename T, std::size_t N, auto Low> inline t_openarray<T> m_openarray_out_view(t_fixedarray<T, N, Low>& value) {
	return m_openarray_reset(m_openarray_mutable_view(value));
}

template <typename T> inline t_openarray<T> m_openarray_out_view(t_dynamicarray<T>& value) {
	return m_openarray_reset(m_openarray_mutable_view(value));
}

template <typename T>
        requires(!std::is_const_v<T>)
inline t_openarray<T> m_openarray_out_view(t_openarray<T> value) {
	return m_openarray_reset(value);
}

template <typename T, std::size_t N, auto Low> inline m_openarray_owner<T> m_openarray_value_copy(const t_fixedarray<T, N, Low>& value) {
	return m_openarray_owner<T>{
	    value.m_data(),
	    static_cast<std::size_t>(value.m_length()),
	};
}

template <typename T> inline m_openarray_owner<T> m_openarray_value_copy(const t_dynamicarray<T>& value) {
	return m_openarray_owner<T>{
	    value.m_data(),
	    static_cast<std::size_t>(value.m_length()),
	};
}

template <typename T> inline m_openarray_owner<std::remove_const_t<T>> m_openarray_value_copy(t_openarray<T> value) {
	using item_type = std::remove_const_t<T>;
	return m_openarray_owner<item_type>{
	    value.m_data(),
	    static_cast<std::size_t>(value.m_length()),
	};
}

struct t_ansistring {
	m_shared_buffer<t_char> storage;

	t_sizeint m_length() const {
		const std::size_t stored = storage.m_size();
		return stored == 0 ? 0 : static_cast<t_sizeint>(stored - 1);
	}

	t_sizeint m_low() const {
		return 1;
	}

	t_sizeint m_high() const {
		return m_length();
	}

	const t_char* m_data() const noexcept {
		if (const t_char* data = storage.m_data()) {
			return data;
		}
		static const t_char zero{0};
		return &zero;
	}

	void m_make_unique() {
		if (storage.m_size() == 0) {
			storage.m_replace(std::vector<t_char>{t_char{0}});
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
		const std::size_t count = static_cast<std::size_t>(m_length());
		result.reserve(count);
		const t_char* data = m_data();
		for (std::size_t i = 0; i < count; ++i) {
			result.push_back(static_cast<char>(data[i].value));
		}
		return result;
	}

	template <typename I> t_char& index(I index) {
		const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
		if (actual < 1 || actual > static_cast<std::ptrdiff_t>(m_length())) {
			m_runtime_error(201);
		}
		return m_writable_data()[static_cast<std::size_t>(actual - 1)];
	}

	template <typename I> const t_char& index(I index) const {
		const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
		if (actual < 1 || actual > static_cast<std::ptrdiff_t>(m_length())) {
			m_runtime_error(201);
		}
		return m_data()[static_cast<std::size_t>(actual - 1)];
	}

	template <typename I> std::size_t storage_extent(I index) const {
		const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
		if (actual < 1 || actual > static_cast<std::ptrdiff_t>(m_length())) {
			m_runtime_error(201);
		}
		return static_cast<std::size_t>(m_length() - actual + 2);
	}

	template <std::size_t SourceCapacity> void assign(const t_shortstring<SourceCapacity>& source) {
		const std::size_t copied = static_cast<std::size_t>(source.m_length());
		std::vector<t_char> replacement(copied + 1, t_char{0});
		std::copy_n(source.m_data(), copied, replacement.data());
		storage.m_replace(std::move(replacement));
	}

	void m_resize(t_sizeint requested_length) {
		const std::size_t new_length = requested_length <= 0 ? 0 : static_cast<std::size_t>(requested_length);
		std::vector<t_char> replacement(new_length + 1, t_char{0});
		const std::size_t copied = std::min(static_cast<std::size_t>(m_length()), new_length);
		std::copy_n(m_data(), copied, replacement.data());
		storage.m_replace(std::move(replacement));
	}

	t_ansistring slice(t_sizeint index, t_sizeint count) const {
		t_ansistring result;
		if (count <= 0) {
			return result;
		}
		if (index < 1) {
			index = 1;
		}
		const std::size_t start = static_cast<std::size_t>(index - 1);
		const std::size_t source_length = static_cast<std::size_t>(m_length());
		if (start >= source_length) {
			return result;
		}
		const std::size_t copied = std::min(static_cast<std::size_t>(count), source_length - start);
		std::vector<t_char> replacement(copied + 1, t_char{0});
		std::copy_n(m_data() + start, copied, replacement.data());
		result.storage.m_replace(std::move(replacement));
		return result;
	}

	void erase(t_sizeint index, t_sizeint count) {
		if (index < 1 || count <= 0) {
			return;
		}
		const std::size_t start = static_cast<std::size_t>(index - 1);
		const std::size_t old_length = static_cast<std::size_t>(m_length());
		if (start >= old_length) {
			return;
		}
		const std::size_t removed = std::min(static_cast<std::size_t>(count), old_length - start);
		std::vector<t_char> replacement(old_length - removed + 1, t_char{0});
		std::copy_n(m_data(), start, replacement.data());
		std::copy(m_data() + start + removed, m_data() + old_length, replacement.data() + start);
		storage.m_replace(std::move(replacement));
	}

	void insert(const t_ansistring& source, t_sizeint index) {
		const std::size_t source_length = static_cast<std::size_t>(source.m_length());
		if (source_length == 0) {
			return;
		}
		const std::size_t old_length = static_cast<std::size_t>(m_length());
		if (index < 1) {
			index = 1;
		}
		std::size_t start = static_cast<std::size_t>(index - 1);
		if (start > old_length) {
			start = old_length;
		}

		// Copy both source ranges before replacing the shared block. This also
		// covers Insert(S, S, I) without an alias-specific branch.
		std::vector<t_char> replacement(old_length + source_length + 1, t_char{0});
		std::copy_n(m_data(), start, replacement.data());
		std::copy_n(source.m_data(), source_length, replacement.data() + start);
		std::copy(m_data() + start, m_data() + old_length, replacement.data() + start + source_length);
		storage.m_replace(std::move(replacement));
	}
};

static_assert(sizeof(t_ansistring) == sizeof(void*));
static_assert(alignof(t_ansistring) == alignof(void*));

inline t_ansistring tpcc_ansistring_literal(const char* source, std::size_t length) {
	t_ansistring result;
	std::vector<t_char> value(length + 1, t_char{0});
	for (std::size_t i = 0; i < length; ++i) {
		value[i] = t_char{static_cast<uint8_t>(static_cast<unsigned char>(source[i]))};
	}
	result.storage.m_replace(std::move(value));
	return result;
}

} // namespace u_system

namespace u_sysutils {

using ::u_system::p_false;
using ::u_system::t_ansistring;
using ::u_system::t_boolean;
using ::u_system::t_double;
using ::u_system::t_int64;
using ::u_system::t_integer;
using ::u_system::t_longint;
using ::u_system::t_longword;
using ::u_system::t_openarray;
using ::u_system::t_qword;
using ::u_system::t_sizeint;
using ::u_system::t_word;
using ::u_system::tpcc_ansistring_literal;
using ::u_system::tpcc_bool_to_boolean;

inline constexpr t_longint m_fa_read_only = 0x00000001;
inline constexpr t_longint m_fa_hidden = 0x00000002;
inline constexpr t_longint m_fa_system = 0x00000004;
inline constexpr t_longint m_fa_directory = 0x00000010;
inline constexpr t_longint m_fa_archive = 0x00000020;
inline constexpr t_longint m_fa_symlink = 0x00000400;

struct m_find_state {
	std::string directory_name;
	std::string pattern;
	DIR* directory = nullptr;
	t_longint search_attributes = 0;
	bool exact_path = false;

	~m_find_state() {
		if (directory) {
			::closedir(directory);
		}
	}
};

inline bool m_find_wildcard_match(const std::string& pattern, const std::string& name) {
	std::size_t pattern_position = 0;
	std::size_t name_position = 0;
	std::size_t star_position = std::string::npos;
	std::size_t star_match_position = 0;

	while (name_position < name.size()) {
		if (pattern_position < pattern.size() && (pattern[pattern_position] == '?' || pattern[pattern_position] == name[name_position])) {
			++pattern_position;
			++name_position;
		} else if (pattern_position < pattern.size() && pattern[pattern_position] == '*') {
			star_position = pattern_position++;
			star_match_position = name_position;
		} else if (star_position != std::string::npos) {
			pattern_position = star_position + 1;
			name_position = ++star_match_position;
		} else {
			return false;
		}
	}
	while (pattern_position < pattern.size() && pattern[pattern_position] == '*') {
		++pattern_position;
	}
	return pattern_position == pattern.size();
}

inline std::string m_find_base_name(const std::string& path) {
	const std::size_t separator = path.rfind('/');
	if (separator == std::string::npos) {
		return path;
	}
	return path.substr(separator + 1);
}

inline t_longint m_find_attributes(const std::string& path, const std::string& name, const struct stat& information) {
	t_longint attributes = m_fa_archive;
	if (S_ISDIR(information.st_mode)) {
		attributes |= m_fa_directory;
	}
	if (name.size() >= 2 && name[0] == '.' && name[1] != '.') {
		attributes |= m_fa_hidden;
	}
	if ((information.st_mode & S_IWUSR) == 0) {
		attributes |= m_fa_read_only;
	}
	if (S_ISSOCK(information.st_mode) || S_ISBLK(information.st_mode) || S_ISCHR(information.st_mode) || S_ISFIFO(information.st_mode)) {
		attributes |= m_fa_system;
	}
	if (S_ISLNK(information.st_mode)) {
		attributes |= m_fa_symlink;
		struct stat target_information{};
		if (::stat(path.c_str(), &target_information) == 0 && S_ISDIR(target_information.st_mode)) {
			attributes |= m_fa_directory;
		}
	}
	return attributes;
}

template <typename SearchRecord> inline bool m_find_populate(const std::string& path, const std::string& name, SearchRecord& result) {
	auto* state = static_cast<m_find_state*>(result.p_findhandle);
	if (!state) {
		return false;
	}

	struct stat information{};
	const int status = (state->search_attributes & m_fa_symlink) != 0 ? ::lstat(path.c_str(), &information) : ::stat(path.c_str(), &information);
	if (status != 0) {
		return false;
	}

	const t_longint attributes = m_find_attributes(path, name, information);
	if ((attributes & ~state->search_attributes) != 0) {
		return false;
	}

	result.p_time = static_cast<t_longint>(information.st_mtime);
	result.p_size = static_cast<t_int64>(information.st_size);
	result.p_attr = attributes;
	result.p_name = tpcc_ansistring_literal(name.data(), name.size());
	result.p_mode = static_cast<t_longint>(information.st_mode);
	return true;
}

template <typename SearchRecord> inline void p_findclose(SearchRecord& result) {
	delete static_cast<m_find_state*>(result.p_findhandle);
	result.p_findhandle = nullptr;
}

template <typename SearchRecord> inline t_longint p_findnext(SearchRecord& result) {
	auto* state = static_cast<m_find_state*>(result.p_findhandle);
	if (!state || state->exact_path) {
		return -1;
	}

	if (!state->directory) {
		state->directory = ::opendir(state->directory_name.c_str());
		if (!state->directory) {
			return -1;
		}
	}

	while (dirent* entry = ::readdir(state->directory)) {
		const std::string name(entry->d_name);
		if (!m_find_wildcard_match(state->pattern, name)) {
			continue;
		}
		if (m_find_populate(state->directory_name + name, name, result)) {
			return 0;
		}
	}
	return -1;
}

template <typename SearchRecord> inline t_longint p_findfirst(const t_ansistring& path_value, t_longint attributes, SearchRecord& result) {
	result.p_time = 0;
	result.p_size = 0;
	result.p_attr = 0;
	result.p_name = {};
	result.p_excludeattr = 0;
	result.p_findhandle = nullptr;
	result.p_mode = 0;

	const std::string path = path_value.m_string();
	if (path.empty()) {
		return -1;
	}

	auto state = std::make_unique<m_find_state>();
	state->search_attributes = attributes | m_fa_archive | m_fa_read_only;
	result.p_findhandle = state.get();

	if (path.find_first_of("*?") == std::string::npos) {
		state->exact_path = true;
		if (!m_find_populate(path, m_find_base_name(path), result)) {
			result.p_findhandle = nullptr;
			return -1;
		}
		state.release();
		return 0;
	}

	const std::size_t separator = path.rfind('/');
	if (separator == std::string::npos) {
		state->directory_name = "./";
		state->pattern = path;
	} else {
		state->directory_name = path.substr(0, separator + 1);
		state->pattern = path.substr(separator + 1);
	}
	if (p_findnext(result) == 0) {
		state.release();
		return 0;
	}
	result.p_findhandle = nullptr;
	return -1;
}

inline t_longint p_fileage(const t_ansistring& file_name) {
	const std::string path = file_name.m_string();
	struct stat information{};
	if (path.empty() || ::stat(path.c_str(), &information) != 0) {
		return -1;
	}
	// This is the legacy LongInt FileAge contract, which treats directories
	// as failure even though stat supplies a meaningful directory timestamp.
	if (S_ISDIR(information.st_mode)) {
		return -1;
	}
	return static_cast<t_longint>(information.st_mtime);
}

inline t_boolean p_deletefile(const t_ansistring& file_name) {
	const std::string path = file_name.m_string();
	if (path.empty()) {
		return p_false;
	}
	return tpcc_bool_to_boolean(::unlink(path.c_str()) == 0);
}

inline t_boolean p_fileexists(const t_ansistring& file_name, t_boolean follow_link) {
	const std::string path = file_name.m_string();
	if (path.empty()) {
		return p_false;
	}

	struct stat information{};
	bool exists = ::access(path.c_str(), F_OK) == 0;
	bool is_directory = false;
	if (exists && ::stat(path.c_str(), &information) == 0 && S_ISDIR(information.st_mode)) {
		exists = false;
		is_directory = true;
	}

	if (!exists && !is_directory && follow_link == p_false) {
		exists = ::lstat(path.c_str(), &information) == 0 && S_ISLNK(information.st_mode);
	}
	return tpcc_bool_to_boolean(exists);
}

inline t_boolean p_directoryexists(const t_ansistring& directory_name, t_boolean follow_link) {
	const std::string path = directory_name.m_string();
	struct stat information{};
	const bool exists = ::stat(path.c_str(), &information) == 0;
	bool is_directory = exists && S_ISDIR(information.st_mode);
	if (!exists && follow_link == p_false) {
		is_directory = ::lstat(path.c_str(), &information) == 0 && S_ISLNK(information.st_mode);
	}
	return tpcc_bool_to_boolean(is_directory);
}

inline t_ansistring p_getenvironmentvariable(const t_ansistring& name) {
	const std::string bytes = name.m_string();
	const char* value = std::getenv(bytes.c_str());
	if (!value) {
		return {};
	}
	return tpcc_ansistring_literal(value, std::strlen(value));
}

} // namespace u_sysutils

namespace u_baseunix {

inline ::u_system::t_char* p_fpgetenv(::u_system::t_char* name) {
	// BaseUnix exposes getenv's borrowed storage directly.  Reading the
	// t_char bytes through char is permitted, and preserves the pointer and
	// lifetime supplied by the C environment rather than manufacturing a copy.
	if (!name) {
		return nullptr;
	}
	return reinterpret_cast<::u_system::t_char*>(std::getenv(reinterpret_cast<const char*>(name)));
}

} // namespace u_baseunix

namespace u_sysutils {

template <typename SystemTime> inline void p_getlocaltime(SystemTime& system_time) {
	::timespec current{};
	std::tm local{};
	if (::clock_gettime(CLOCK_REALTIME, &current) != 0 || !::localtime_r(&current.tv_sec, &local)) {
		system_time = {};
		return;
	}

	system_time.p_year = static_cast<t_word>(local.tm_year + 1900);
	system_time.p_month = static_cast<t_word>(local.tm_mon + 1);
	system_time.p_dayofweek = static_cast<t_word>(local.tm_wday);
	system_time.p_day = static_cast<t_word>(local.tm_mday);
	system_time.p_hour = static_cast<t_word>(local.tm_hour);
	system_time.p_minute = static_cast<t_word>(local.tm_min);
	system_time.p_second = static_cast<t_word>(local.tm_sec);
	system_time.p_millisecond = static_cast<t_word>(current.tv_nsec / 1000000);
}

inline t_double p_filedatetodatetime(t_integer file_date) {
	const std::time_t epoch_seconds = static_cast<std::time_t>(file_date);
	std::tm local{};
	if (!::localtime_r(&epoch_seconds, &local)) {
		throw std::system_error(errno, std::generic_category(), "localtime_r");
	}

	const std::chrono::sys_days local_date{std::chrono::year{local.tm_year + 1900} / std::chrono::month{static_cast<unsigned>(local.tm_mon + 1)} / std::chrono::day{static_cast<unsigned>(local.tm_mday)}};
	const std::chrono::sys_days date_time_epoch{std::chrono::year{1899} / std::chrono::month{12} / std::chrono::day{30}};
	const t_double whole_days = static_cast<t_double>((local_date - date_time_epoch).count());
	const t_double seconds = static_cast<t_double>((local.tm_hour * 60 + local.tm_min) * 60 + local.tm_sec);
	return whole_days + seconds / 86400.0;
}

inline void p_decodedate(t_double date_time, t_word& year, t_word& month, t_word& day) {
	constexpr t_longint date_delta = 693594;
	constexpr t_double max_date_time = 2958465.99999999;
	constexpr t_double half_millisecond = 1.0 / (86400000.0 * 2.0);

	if (date_time <= -date_delta) {
		year = 0;
		month = 0;
		day = 0;
	} else {
		t_double adjusted = date_time > 0 ? date_time + half_millisecond : date_time - half_millisecond;
		if (adjusted > max_date_time) {
			adjusted = max_date_time;
		}

		const auto whole_days = static_cast<t_longint>(::trunc(adjusted));
		const std::chrono::sys_days date_time_epoch{std::chrono::year{1899} / std::chrono::month{12} / std::chrono::day{30}};
		const std::chrono::year_month_day decoded{date_time_epoch + std::chrono::days{whole_days}};
		year = static_cast<t_word>(static_cast<int>(decoded.year()));
		month = static_cast<t_word>(static_cast<unsigned>(decoded.month()));
		day = static_cast<t_word>(static_cast<unsigned>(decoded.day()));
	}
}

inline void p_decodetime(t_double date_time, t_word& hour, t_word& minute, t_word& second, t_word& millisecond) {
	constexpr t_int64 milliseconds_per_day = 86400000;
	const t_int64 rounded_milliseconds = static_cast<t_int64>(::round(date_time * static_cast<t_double>(milliseconds_per_day)));
	const t_qword absolute_milliseconds = rounded_milliseconds < 0 ? static_cast<t_qword>(-rounded_milliseconds) : static_cast<t_qword>(rounded_milliseconds);
	t_longword time = static_cast<t_longword>(absolute_milliseconds % milliseconds_per_day);

	hour = static_cast<t_word>(time / 3600000);
	time %= 3600000;
	minute = static_cast<t_word>(time / 60000);
	time %= 60000;
	second = static_cast<t_word>(time / 1000);
	millisecond = static_cast<t_word>(time % 1000);
}

} // namespace u_sysutils

namespace u_unix {

inline ::u_system::t_integer p_fpsystem(const ::u_system::t_ansistring& command) {
	if (command.m_length() == 0) {
		return 1;
	}
	const std::string bytes = command.m_string();
	return static_cast<::u_system::t_integer>(::system(bytes.c_str()));
}

} // namespace u_unix

namespace u_sysutils {

inline bool m_executeprocess_separator(char value) {
	return value == ' ' || value == '\t' || value == '\n';
}

inline std::vector<std::string> m_executeprocess_arguments_from_command_line(const t_ansistring& command_line) {
	const std::string bytes = command_line.m_string();
	const std::size_t nul = bytes.find('\0');
	const std::size_t size = nul == std::string::npos ? bytes.size() : nul;
	std::vector<std::string> result;
	std::size_t position = 0;
	while (position < size) {
		while (position < size && m_executeprocess_separator(bytes[position])) {
			++position;
		}
		if (position == size) {
			break;
		}

		if (bytes[position] == '"') {
			const std::size_t start = ++position;
			while (position < size && bytes[position] != '"') {
				++position;
			}
			result.emplace_back(bytes.substr(start, position - start));
			if (position < size) {
				++position;
			}
		} else {
			const std::size_t start = position;
			while (position < size && !m_executeprocess_separator(bytes[position])) {
				++position;
			}
			result.emplace_back(bytes.substr(start, position - start));
		}
	}
	return result;
}

inline t_integer m_executeprocess(const t_ansistring& path, std::vector<std::string> arguments) {
	const std::string path_bytes = path.m_string();
	arguments.insert(arguments.begin(), path_bytes);

	std::vector<char*> argv;
	argv.reserve(arguments.size() + 1);
	for (std::string& argument : arguments) {
		argv.push_back(argument.data());
	}
	argv.push_back(nullptr);

	pid_t pid = -1;
	const int spawn_error = ::posix_spawn(&pid, path_bytes.c_str(), nullptr, nullptr, argv.data(), ::environ);
	if (spawn_error != 0) {
		return spawn_error == EAGAIN || spawn_error == ENOMEM ? -1 : 127;
	}

	int status = 0;
	pid_t waited;
	do {
		waited = ::waitpid(pid, &status, 0);
	} while (waited == -1 && errno == EINTR);
	if (waited <= 0) {
		return -1;
	}
	if (WIFEXITED(status)) {
		return static_cast<t_integer>(WEXITSTATUS(status));
	}
	return status > 0 ? static_cast<t_integer>(-status) : static_cast<t_integer>(status);
}

inline t_integer p_executeprocess_commandline(const t_ansistring& path, const t_ansistring& command_line) {
	return m_executeprocess(path, m_executeprocess_arguments_from_command_line(command_line));
}

inline t_integer p_executeprocess_arguments(const t_ansistring& path, t_openarray<const t_ansistring> arguments) {
	std::vector<std::string> bytes;
	bytes.reserve(static_cast<std::size_t>(arguments.m_length()));
	for (t_sizeint index = 0; index < arguments.m_length(); ++index) {
		bytes.push_back(arguments.m_data()[index].m_string());
	}
	return m_executeprocess(path, std::move(bytes));
}

} // namespace u_sysutils

namespace u_system {

template <std::size_t DestinationCapacity> inline t_shortstring<DestinationCapacity> tpcc_shortstring_cast(const t_ansistring& source) {
	// An explicit AnsiString -> ShortString conversion uses the managed
	// string's stored length, not a C NUL terminator, and truncates to the
	// destination's declared inline capacity even under {$R+}.
	t_shortstring<DestinationCapacity> result{};
	const std::size_t copied = std::min<std::size_t>(static_cast<std::size_t>(source.m_length()), DestinationCapacity);
	result.length = t_char{static_cast<uint8_t>(copied)};
	std::copy_n(source.m_data(), copied, result.data);
	return result;
}

// Pascal Initialize starts the lifetime of the managed part of an otherwise
// uninitialized value. In particular, it must not release a handle which was
// placed in the destination by a preceding bytewise Move: that handle has not
// acquired another reference yet. The fallback is a no-op for unmanaged
// Pascal carriers. Generated aggregate overloads recursively visit only fields
// whose Pascal types have managed lifetime.
template <typename T> inline void m_pascal_initialize(T&) noexcept {
}

inline void m_pascal_initialize(t_ansistring& value) noexcept {
	::new (static_cast<void*>(std::addressof(value))) t_ansistring;
}

inline void m_pascal_initialize(t_text& value) noexcept {
	::new (static_cast<void*>(std::addressof(value))) t_text;
}

inline void m_pascal_initialize(t_file& value) noexcept {
	::new (static_cast<void*>(std::addressof(value))) t_file;
}

template <typename T> inline void m_pascal_initialize(t_typedfile<T>& value) noexcept {
	::new (static_cast<void*>(std::addressof(value))) t_typedfile<T>;
}

template <typename T> inline void m_pascal_initialize(t_dynamicarray<T>& value) noexcept {
	::new (static_cast<void*>(std::addressof(value))) t_dynamicarray<T>;
}

template <typename T, std::size_t length, auto low> inline void m_pascal_initialize(t_fixedarray<T, length, low>& value) noexcept {
	for (T& item : value.items) {
		using ::u_system::m_pascal_initialize;
		m_pascal_initialize(item);
	}
}

// Finalize releases the managed part once, then restores an empty live carrier.
// The reconstruction is backend bookkeeping: Pascal regards the value as
// finalized, while C++ scope/object teardown may still later invoke the
// carrier destructor and must therefore see a harmless empty value.
template <typename T> inline void m_pascal_finalize(T&) noexcept {
}

inline void m_pascal_finalize(t_ansistring& value) noexcept {
	std::destroy_at(std::addressof(value));
	::new (static_cast<void*>(std::addressof(value))) t_ansistring;
}

inline void m_pascal_finalize(t_text& value) noexcept {
	std::destroy_at(std::addressof(value));
	::new (static_cast<void*>(std::addressof(value))) t_text;
}

inline void m_pascal_finalize(t_file& value) noexcept {
	std::destroy_at(std::addressof(value));
	::new (static_cast<void*>(std::addressof(value))) t_file;
}

template <typename T> inline void m_pascal_finalize(t_typedfile<T>& value) noexcept {
	std::destroy_at(std::addressof(value));
	::new (static_cast<void*>(std::addressof(value))) t_typedfile<T>;
}

template <typename T> inline void m_pascal_finalize(t_dynamicarray<T>& value) noexcept {
	std::destroy_at(std::addressof(value));
	::new (static_cast<void*>(std::addressof(value))) t_dynamicarray<T>;
}

template <typename T, std::size_t length, auto low> inline void m_pascal_finalize(t_fixedarray<T, length, low>& value) noexcept {
	for (std::size_t i = length; i != 0; --i) {
		using ::u_system::m_pascal_finalize;
		m_pascal_finalize(value.items[i - 1]);
	}
}

template <typename T> inline void p_initialize(tpcc_typed_storage_ref<T> value) noexcept {
	if (value.size < sizeof(T)) {
		m_runtime_error(201);
	}
	using ::u_system::m_pascal_initialize;
	m_pascal_initialize(*value.value);
}

template <typename T> inline void p_finalize(tpcc_typed_storage_ref<T> value) noexcept {
	if (value.size < sizeof(T)) {
		m_runtime_error(201);
	}
	using ::u_system::m_pascal_finalize;
	m_pascal_finalize(*value.value);
}

template <typename T, typename I> inline T& p_index(t_dynamicarray<T>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual >= static_cast<std::ptrdiff_t>(value.m_length())) {
		m_runtime_error(201);
	}
	return value.m_data()[static_cast<std::size_t>(actual)];
}

template <typename T, typename I> inline const T& p_index(const t_dynamicarray<T>& value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual >= static_cast<std::ptrdiff_t>(value.m_length())) {
		m_runtime_error(201);
	}
	return value.m_data()[static_cast<std::size_t>(actual)];
}

template <typename T, typename I> inline T& m_unchecked_index(t_dynamicarray<T>& value, I index) {
	return value.m_data()[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index))];
}

template <typename T, typename I> inline const T& m_unchecked_index(const t_dynamicarray<T>& value, I index) {
	return value.m_data()[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index))];
}

template <typename T, typename I> inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(t_dynamicarray<T>& value, I index) {
	T& selected = p_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(const t_dynamicarray<T>& value, I index) {
	const T& selected = p_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline tpcc_typed_storage_ref<T> m_unchecked_storage_ref(t_dynamicarray<T>& value, I index) {
	T& selected = m_unchecked_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline tpcc_typed_const_storage_ref<T> m_unchecked_const_storage_ref(const t_dynamicarray<T>& value, I index) {
	const T& selected = m_unchecked_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline T& p_index(t_openarray<T> value, I index) {
	const std::ptrdiff_t actual = static_cast<std::ptrdiff_t>(index);
	if (actual < 0 || actual >= static_cast<std::ptrdiff_t>(value.m_length())) {
		m_runtime_error(201);
	}
	return value.m_data()[static_cast<std::size_t>(actual)];
}

template <typename T, typename I> inline T& m_unchecked_index(t_openarray<T> value, I index) {
	return value.m_data()[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index))];
}

template <typename T, typename I>
        requires(!std::is_const_v<T>)
inline tpcc_typed_storage_ref<T> tpcc_make_storage_ref(t_openarray<T> value, I index) {
	T& selected = p_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline tpcc_typed_const_storage_ref<T> tpcc_make_const_storage_ref(t_openarray<const T> value, I index) {
	const T& selected = p_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I>
        requires(!std::is_const_v<T>)
inline tpcc_typed_storage_ref<T> m_unchecked_storage_ref(t_openarray<T> value, I index) {
	T& selected = m_unchecked_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_storage_ref<T>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T, typename I> inline tpcc_typed_const_storage_ref<T> m_unchecked_const_storage_ref(t_openarray<const T> value, I index) {
	const T& selected = m_unchecked_index(value, index);
	const std::size_t offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_const_storage_ref<T>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        (static_cast<std::size_t>(value.m_length()) - offset) * sizeof(T),
	    },
	    std::addressof(selected),
	};
}

template <typename T> class m_array_view_enumerator {
	T* data;
	std::size_t count;
	std::size_t next = 0;

      public:
	m_array_view_enumerator(T* data, std::size_t count) : data(data), count(count) {
	}

	bool m_move_next() {
		if (next == count) {
			return false;
		}
		++next;
		return true;
	}

	decltype(auto) m_current() const {
		return data[next - 1];
	}
};

template <typename T> class m_dynamicarray_enumerator {
	// Copying the shared handle pins the exact buffer selected when iteration
	// begins. A later SetLength on the source replaces its handle and cannot
	// invalidate this traversal.
	t_dynamicarray<T> value;
	std::size_t next = 0;

      public:
	explicit m_dynamicarray_enumerator(const t_dynamicarray<T>& value) : value(value) {
	}

	bool m_move_next() {
		if (next == static_cast<std::size_t>(value.m_length())) {
			return false;
		}
		++next;
		return true;
	}

	decltype(auto) m_current() const {
		return value.m_data()[next - 1];
	}
};

template <typename String> class m_string_enumerator {
	// Strings use the same snapshot rule as dynamic arrays. Short strings copy
	// their inline bytes; AnsiString copies its shared handle and relies on
	// copy-on-write before either alias is modified.
	String value;
	std::size_t next = 0;

      public:
	explicit m_string_enumerator(const String& value) : value(value) {
	}

	bool m_move_next() {
		if (next == static_cast<std::size_t>(value.m_length())) {
			return false;
		}
		++next;
		return true;
	}

	t_char m_current() const {
		return value.m_data()[next - 1];
	}
};

template <typename T> inline T m_ordinal_from_storage(typename tpcc_ordinal_storage<T>::type value) {
	return tpcc_ordinal_storage<T>::make(value);
}

template <typename T> class m_ordinal_enumerator {
	using traits = tpcc_ordinal_storage<T>;
	using storage_type = typename traits::type;
	storage_type lower;
	storage_type upper;
	storage_type current{};
	bool started = false;
	bool valid;

      public:
	m_ordinal_enumerator(T lower, T upper) : lower(traits::get(lower)), upper(traits::get(upper)), valid(this->lower <= this->upper) {
	}

	bool m_move_next() {
		if (!valid) {
			return false;
		}
		if (!started) {
			current = lower;
			started = true;
			return true;
		}
		if (current == upper) {
			return false;
		}
		++current;
		return true;
	}

	T m_current() const {
		return m_ordinal_from_storage<T>(current);
	}
};

template <typename T> struct m_ordinal_range {
	T lower;
	T upper;
};

template <typename T> inline m_ordinal_enumerator<T> m_enumerate(m_ordinal_range<T> range) {
	return m_ordinal_enumerator<T>{range.lower, range.upper};
}

template <typename T> class m_set_enumerator {
	using traits = tpcc_ordinal_storage<T>;
	using storage_type = typename traits::type;
	t_set<T> value;
	storage_type lower;
	storage_type upper;
	storage_type current{};
	bool started = false;
	bool exhausted;

      public:
	m_set_enumerator(const t_set<T>& value, T lower, T upper) : value(value), lower(traits::get(lower)), upper(traits::get(upper)), exhausted(this->lower > this->upper || value.spans.empty()) {
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
			if (o_in(m_ordinal_from_storage<T>(current), value) == p_true) {
				return true;
			}
		}
		return false;
	}

	T m_current() const {
		return m_ordinal_from_storage<T>(current);
	}
};

template <typename T, std::size_t N, auto Low> inline m_array_view_enumerator<T> m_enumerate(t_fixedarray<T, N, Low>& value) {
	return {value.m_data(), static_cast<std::size_t>(value.m_length())};
}

template <typename T, std::size_t N, auto Low> inline m_array_view_enumerator<const T> m_enumerate(const t_fixedarray<T, N, Low>& value) {
	return {value.m_data(), static_cast<std::size_t>(value.m_length())};
}

template <typename T> inline m_dynamicarray_enumerator<T> m_enumerate(const t_dynamicarray<T>& value) {
	return m_dynamicarray_enumerator<T>{value};
}

template <typename T> inline m_array_view_enumerator<T> m_enumerate(t_openarray<T> value) {
	return {value.m_data(), static_cast<std::size_t>(value.m_length())};
}

template <std::size_t Capacity> inline m_string_enumerator<t_shortstring<Capacity>> m_enumerate(const t_shortstring<Capacity>& value) {
	return m_string_enumerator<t_shortstring<Capacity>>{value};
}

inline m_string_enumerator<t_ansistring> m_enumerate(const t_ansistring& value) {
	return m_string_enumerator<t_ansistring>{value};
}

template <typename T> inline m_set_enumerator<T> m_enumerate(const t_set<T>& value, T lower, T upper) {
	return m_set_enumerator<T>{value, lower, upper};
}

enum class text_file_mode {
	Closed,
	Input,
	Output,
};

struct text_file_state {
	std::string name;
	std::FILE* handle = nullptr;
	text_file_mode mode = text_file_mode::Closed;
	bool standard_stream = false;
};

inline text_file_state m_stdout_text_state{
    .name = {},
    .handle = stdout,
    .mode = text_file_mode::Output,
    .standard_stream = true,
};
inline text_file_state m_stderr_text_state{
    .name = {},
    .handle = stderr,
    .mode = text_file_mode::Output,
    .standard_stream = true,
};

// System.StdOut and System.StdErr are real Pascal variables. Their states are
// non-owning because the C runtime owns stdout/stderr. Flush and Write still
// use the same FILE API as every named Text; only finalization's ownership
// decision differs.
inline t_text p_stdout{&m_stdout_text_state};
inline t_text p_stderr{&m_stderr_text_state};

struct binary_file_state {
	std::string name;
	std::FILE* handle = nullptr;
	t_longint record_size = 128;
	bool readable = false;
	bool writable = false;
};

// This is the single pending status described at System.IOResult. Unchecked
// I/O preserves its first failure and skips later operations; IOResult returns
// and clears it. A C FILE's sticky error indicator is therefore not additional
// Pascal state and must be consumed when its error is translated below.
inline t_word m_inoutres = 0;
inline t_byte p_filemode = 2;

inline void m_set_io_error(t_word error) {
	if (error != 0 && m_inoutres == 0) {
		m_inoutres = error;
	}
}

inline t_word m_file_error_from_errno(int error, t_word fallback) {
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

inline t_word m_consume_stdio_error(std::FILE* file, int error, t_word fallback) noexcept {
	// stdio's error indicator is sticky, whereas InOutRes is Pascal's sole
	// pending-I/O-error latch. Once an operation has translated the host
	// error, consume the host indicator so IOResult (or a caught checked-I/O
	// exception) really does allow a later operation to run independently.
	std::clearerr(file);
	return m_file_error_from_errno(error, fallback);
}

inline t_word m_consume_stdio_error(std::FILE* file, t_word fallback) noexcept {
	// Call this overload immediately after a failing stdio operation. Read
	// paths which must call ferror first save errno and use the overload above.
	return m_consume_stdio_error(file, errno, fallback);
}

inline t_word p_ioresult() {
	const t_word result = m_inoutres;
	m_inoutres = 0;
	return result;
}

// Checked old-style I/O consumes a pending unchecked error before invoking
// ErrorProc. That lets an exception handler perform further I/O and ensures a
// caught EInOutError does not leave IOResult poisoned. Unchecked entry points,
// by contrast, preserve the first pending error and skip the operation.
inline void m_raise_pending_io_error() {
	const t_word error = p_ioresult();
	if (error != 0) {
		m_runtime_error(error);
	}
}

inline void m_finish_checked_io(t_word error) {
	if (error != 0) {
		m_runtime_error(error);
	}
}

inline void m_finish_unchecked_io(t_word error) {
	m_set_io_error(error);
}

template <typename T> struct m_io_result {
	T value;
	t_word error;
};

template <typename T> inline T m_finish_checked_io(m_io_result<T> result) {
	m_finish_checked_io(result.error);
	return result.value;
}

template <typename T> inline T m_finish_unchecked_io(m_io_result<T> result) {
	m_finish_unchecked_io(result.error);
	return result.value;
}

inline t_word m_do_close_binary_handle(binary_file_state& state) {
	if (!state.handle) {
		return 0;
	}
	errno = 0;
	const int result = std::fclose(state.handle);
	const t_word error = result == 0 ? 0 : m_file_error_from_errno(errno, 101);
	state.handle = nullptr;
	state.readable = false;
	state.writable = false;
	return error;
}

inline t_word m_do_close_text_handle(text_file_state& state) {
	if (!state.handle) {
		return 0;
	}
	errno = 0;
	const int result = std::fclose(state.handle);
	const t_word error = result == 0 ? 0 : m_file_error_from_errno(errno, 101);
	state.handle = nullptr;
	state.mode = text_file_mode::Closed;
	return error;
}

inline void m_release_text_file_state(text_file_state*& state) noexcept {
	if (!state) {
		return;
	}
	if (state->standard_stream) {
		// stdout/stderr and their state objects are owned by the C runtime
		// and this RTL respectively. A Pascal variable may refer to them but
		// must never fclose or delete them.
		state = nullptr;
		return;
	}
	const t_word close_error = m_do_close_text_handle(*state);
	m_set_io_error(close_error);
	delete state;
	state = nullptr;
}

inline void m_release_binary_file_state(binary_file_state*& state) noexcept {
	if (!state) {
		return;
	}
	const t_word close_error = m_do_close_binary_handle(*state);
	m_set_io_error(close_error);
	delete state;
	state = nullptr;
}

inline t_text::~t_text() noexcept {
	m_release_text_file_state(state);
}

inline t_file::~t_file() noexcept {
	m_release_binary_file_state(state);
}

template <typename Element> inline t_typedfile<Element>::~t_typedfile() noexcept {
	m_release_binary_file_state(state);
}

template <typename PascalString> inline void p_assign(t_text& file, const PascalString& name) {
	if (m_inoutres != 0) {
		return;
	}
	// Assign changes only the association. It must not hide a close/flush
	// operation, and standard Text variables cannot be repurposed while open.
	if (file.state && (file.state->handle || file.state->mode != text_file_mode::Closed)) {
		m_runtime_error(102);
	}
	if (file.state) {
		file.state->name = name.m_string();
		return;
	}
	file.state = new text_file_state;
	file.state->name = name.m_string();
}

template <typename PascalString> inline void p_assign(t_file& file, const PascalString& name) {
	if (m_inoutres != 0) {
		return;
	}
	// Assign associates a name; it is not an I/O operation and therefore has
	// no caller-{$I} alternate. Silently closing an already-open handle here
	// would both perform hidden I/O and create an error which no Assign call
	// site could handle. Treat that invalid file state as an unconditional
	// runtime error instead.
	if (file.state && file.state->handle) {
		m_runtime_error(102);
	}
	if (file.state) {
		file.state->name = name.m_string();
		return;
	}
	file.state = new binary_file_state;
	file.state->name = name.m_string();
}

inline t_word m_prepare_binary_open(t_file& file, t_longint record_size) {
	if (!file.state) {
		return 102;
	}
	if (record_size <= 0) {
		return 12;
	}
	if (file.state->handle) {
		const t_word error = m_do_close_binary_handle(*file.state);
		if (error != 0) {
			return error;
		}
	}
	file.state->record_size = record_size;
	return 0;
}

inline t_word m_do_rewrite(t_file& file, t_longint record_size) {
	const t_word prepare_error = m_prepare_binary_open(file, record_size);
	if (prepare_error != 0) {
		return prepare_error;
	}
	errno = 0;
	file.state->handle = std::fopen(file.state->name.c_str(), "w+b");
	if (!file.state->handle) {
		return m_file_error_from_errno(errno, 101);
	}
	// FPC opens an untyped Rewrite file for writing. The C handle is
	// read/write so a later Reset can reuse ordinary host file semantics;
	// Pascal access checks still use these explicit mode flags.
	file.state->readable = false;
	file.state->writable = true;
	return 0;
}

inline void p_rewrite(t_file& file, t_longint record_size) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_rewrite(file, record_size));
}

inline void m_unchecked_rewrite(t_file& file, t_longint record_size) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_rewrite(file, record_size));
}

inline t_word m_do_rewrite(t_text& file) {
	if (!file.state || file.state->standard_stream) {
		return 102;
	}
	// Rewrite on an already-open Text first closes that association and then
	// creates/truncates the same named file. Report a close failure before
	// attempting a second host open so Pascal retains the first I/O error.
	if (file.state->handle) {
		const t_word close_error = m_do_close_text_handle(*file.state);
		if (close_error != 0) {
			return close_error;
		}
	}
	errno = 0;
	file.state->handle = std::fopen(file.state->name.c_str(), "wb");
	if (!file.state->handle) {
		return m_file_error_from_errno(errno, 101);
	}
	file.state->mode = text_file_mode::Output;
	return 0;
}

inline void p_rewrite(t_text& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_rewrite(file));
}

inline void m_unchecked_rewrite(t_text& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_rewrite(file));
}

inline t_word m_do_reset(t_file& file, t_longint record_size) {
	const t_word prepare_error = m_prepare_binary_open(file, record_size);
	if (prepare_error != 0) {
		return prepare_error;
	}
	const t_byte access = static_cast<t_byte>(p_filemode & 3);
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
		return 12;
	}
	errno = 0;
	file.state->handle = std::fopen(file.state->name.c_str(), mode);
	if (!file.state->handle) {
		return m_file_error_from_errno(errno, 100);
	}
	file.state->readable = access != 1;
	file.state->writable = access != 0;
	return 0;
}

inline t_word m_do_reset(t_text& file) {
	if (!file.state || file.state->standard_stream) {
		return 102;
	}
	if (file.state->handle) {
		const t_word close_error = m_do_close_text_handle(*file.state);
		if (close_error != 0) {
			return close_error;
		}
	}
	errno = 0;
	file.state->handle = std::fopen(file.state->name.c_str(), "rb");
	if (!file.state->handle) {
		return m_file_error_from_errno(errno, 100);
	}
	file.state->mode = text_file_mode::Input;
	return 0;
}

inline void p_reset(t_text& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_reset(file));
}

inline void m_unchecked_reset(t_text& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_reset(file));
}

inline void p_reset(t_file& file, t_longint record_size) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_reset(file, record_size));
}

inline void m_unchecked_reset(t_file& file, t_longint record_size) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_reset(file, record_size));
}

inline t_word m_require_open_binary_file(t_file& file) {
	if (!file.state) {
		return 102;
	}
	if (!file.state->handle) {
		return 103;
	}
	return 0;
}

inline t_word m_do_close(t_file& file) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return open_error;
	}
	return m_do_close_binary_handle(*file.state);
}

inline void p_close(t_file& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_close(file));
}

inline void m_unchecked_close(t_file& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_close(file));
}

inline t_word m_do_close(t_text& file) {
	if (!file.state || file.state->mode == text_file_mode::Closed) {
		return 103;
	}
	if (file.state->standard_stream) {
		// The C runtime owns stdout/stderr. Pascal Close must not close their
		// descriptors; flushing is the only host I/O.
		if (!file.state->handle) {
			return 103;
		}
		errno = 0;
		if (std::fflush(file.state->handle) == 0) {
			return 0;
		}
		return m_consume_stdio_error(file.state->handle, 101);
	}
	return m_do_close_text_handle(*file.state);
}

inline void p_close(t_text& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_close(file));
}

inline void m_unchecked_close(t_text& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_close(file));
}

inline t_word m_do_seek(t_file& file, t_int64 record_position) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return open_error;
	}
	if (record_position < 0 || record_position > std::numeric_limits<long>::max() / file.state->record_size) {
		return 156;
	}
	const long byte_position = static_cast<long>(record_position * file.state->record_size);
	errno = 0;
	if (std::fseek(file.state->handle, byte_position, SEEK_SET) != 0) {
		return m_consume_stdio_error(file.state->handle, 156);
	}
	return 0;
}

inline void p_seek(t_file& file, t_int64 record_position) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_seek(file, record_position));
}

inline void m_unchecked_seek(t_file& file, t_int64 record_position) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_seek(file, record_position));
}

inline m_io_result<t_int64> m_do_filepos(t_file& file) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return {-1, open_error};
	}
	errno = 0;
	const long position = std::ftell(file.state->handle);
	if (position < 0) {
		return {-1, m_consume_stdio_error(file.state->handle, 156)};
	}
	return {static_cast<t_int64>(position / file.state->record_size), 0};
}

inline t_int64 p_filepos(t_file& file) {
	m_raise_pending_io_error();
	return m_finish_checked_io(m_do_filepos(file));
}

inline t_int64 m_unchecked_filepos(t_file& file) {
	if (m_inoutres != 0) {
		return -1;
	}
	return m_finish_unchecked_io(m_do_filepos(file));
}

inline m_io_result<t_int64> m_do_filesize(t_file& file) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return {-1, open_error};
	}
	errno = 0;
	const long original = std::ftell(file.state->handle);
	if (original < 0) {
		return {-1, m_consume_stdio_error(file.state->handle, 156)};
	}
	errno = 0;
	if (std::fseek(file.state->handle, 0, SEEK_END) != 0) {
		return {-1, m_consume_stdio_error(file.state->handle, 156)};
	}
	errno = 0;
	const long end = std::ftell(file.state->handle);
	if (end < 0) {
		const t_word error = m_consume_stdio_error(file.state->handle, 156);
		// Best effort: a failed size query must not unnecessarily leave the
		// caller at the temporary end-of-file position.
		errno = 0;
		if (std::fseek(file.state->handle, original, SEEK_SET) != 0) {
			(void)m_consume_stdio_error(file.state->handle, 156);
		}
		return {-1, error};
	}
	errno = 0;
	if (std::fseek(file.state->handle, original, SEEK_SET) != 0) {
		return {-1, m_consume_stdio_error(file.state->handle, 156)};
	}
	return {static_cast<t_int64>(end / file.state->record_size), 0};
}

inline t_int64 p_filesize(t_file& file) {
	m_raise_pending_io_error();
	return m_finish_checked_io(m_do_filesize(file));
}

inline t_int64 m_unchecked_filesize(t_file& file) {
	if (m_inoutres != 0) {
		return -1;
	}
	return m_finish_unchecked_io(m_do_filesize(file));
}

inline m_io_result<t_boolean> m_do_eof(t_file& file) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return {p_true, open_error};
	}
	if (!file.state->readable) {
		return {p_true, 104};
	}
	errno = 0;
	const long original = std::ftell(file.state->handle);
	if (original < 0) {
		return {p_true, m_consume_stdio_error(file.state->handle, 156)};
	}
	errno = 0;
	if (std::fseek(file.state->handle, 0, SEEK_END) != 0) {
		return {p_true, m_consume_stdio_error(file.state->handle, 156)};
	}
	errno = 0;
	const long end = std::ftell(file.state->handle);
	if (end < 0) {
		const t_word error = m_consume_stdio_error(file.state->handle, 156);
		errno = 0;
		if (std::fseek(file.state->handle, original, SEEK_SET) != 0) {
			(void)m_consume_stdio_error(file.state->handle, 156);
		}
		return {p_true, error};
	}
	errno = 0;
	if (std::fseek(file.state->handle, original, SEEK_SET) != 0) {
		return {p_true, m_consume_stdio_error(file.state->handle, 156)};
	}
	return {tpcc_bool_to_boolean(original >= end), 0};
}

inline t_boolean p_eof(t_file& file) {
	m_raise_pending_io_error();
	return m_finish_checked_io(m_do_eof(file));
}

inline t_boolean m_unchecked_eof(t_file& file) {
	if (m_inoutres != 0) {
		return p_true;
	}
	return m_finish_unchecked_io(m_do_eof(file));
}

inline m_io_result<t_boolean> m_do_eof(t_text& file) {
	if (!file.state || file.state->mode == text_file_mode::Closed || !file.state->handle) {
		return {p_true, 103};
	}
	if (file.state->mode != text_file_mode::Input) {
		return {p_true, 104};
	}
	errno = 0;
	const int value = std::fgetc(file.state->handle);
	if (value == EOF) {
		// fgetc uses EOF for both normal end-of-file and an input error.
		const int saved_errno = errno;
		if (std::ferror(file.state->handle)) {
			return {p_true, m_consume_stdio_error(file.state->handle, saved_errno, 100)};
		}
		return {p_true, 0};
	}
	errno = 0;
	if (std::ungetc(value, file.state->handle) == EOF) {
		return {p_true, m_consume_stdio_error(file.state->handle, 100)};
	}
	return {p_false, 0};
}

inline t_boolean p_eof(t_text& file) {
	m_raise_pending_io_error();
	return m_finish_checked_io(m_do_eof(file));
}

inline t_boolean m_unchecked_eof(t_text& file) {
	if (m_inoutres != 0) {
		return p_true;
	}
	return m_finish_unchecked_io(m_do_eof(file));
}

inline void p_settextbuf(t_text&, tpcc_storage_ref, t_sizeint) noexcept {
	// The three-argument operation is semantically only a buffering request.
	// The C stdio implementation retains its own buffer for now; evaluating
	// and validating the Pascal var arguments still happens at the call site.
}

inline m_io_result<std::string> m_do_readln_bytes(t_text& file) {
	if (!file.state || file.state->mode == text_file_mode::Closed || !file.state->handle) {
		return {{}, 103};
	}
	if (file.state->mode != text_file_mode::Input) {
		return {{}, 104};
	}

	std::string bytes;
	for (;;) {
		errno = 0;
		const int value = std::fgetc(file.state->handle);
		if (value == EOF) {
			// fgetc uses EOF for both normal end-of-file and an input error.
			const int saved_errno = errno;
			if (std::ferror(file.state->handle)) {
				return {{}, m_consume_stdio_error(file.state->handle, saved_errno, 100)};
			}
			break;
		}
		if (value == '\n') {
			break;
		}
		if (value == '\r') {
			errno = 0;
			const int following = std::fgetc(file.state->handle);
			if (following != '\n' && following != EOF) {
				errno = 0;
				if (std::ungetc(following, file.state->handle) == EOF) {
					return {{}, m_consume_stdio_error(file.state->handle, 100)};
				}
			}
			if (following == EOF) {
				const int saved_errno = errno;
				if (std::ferror(file.state->handle)) {
					return {{}, m_consume_stdio_error(file.state->handle, saved_errno, 100)};
				}
			}
			break;
		}
		bytes.push_back(static_cast<char>(static_cast<unsigned char>(value)));
	}
	return {std::move(bytes), 0};
}

template <std::size_t Capacity> inline void m_store_text_line(const std::string& bytes, t_shortstring<Capacity>& destination) {
	const std::size_t count = std::min(bytes.size(), Capacity);
	destination.length = t_char{static_cast<uint8_t>(count)};
	for (std::size_t i = 0; i < count; ++i) {
		destination.data[i] = t_char{static_cast<uint8_t>(static_cast<unsigned char>(bytes[i]))};
	}
}

inline void m_store_text_line(const std::string& bytes, t_ansistring& destination) {
	std::vector<t_char> stored;
	stored.reserve(bytes.size() + 1);
	for (unsigned char value : bytes) {
		stored.push_back(t_char{value});
	}
	stored.push_back(t_char{0});
	destination.storage.m_replace(std::move(stored));
}

template <typename PascalString> inline t_word m_do_readln(t_text& file, PascalString& destination) {
	auto line = m_do_readln_bytes(file);
	if (line.error != 0) {
		return line.error;
	}
	m_store_text_line(line.value, destination);
	return 0;
}

template <typename PascalString> inline void p_readln(t_text& file, PascalString& destination) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_readln(file, destination));
}

template <typename PascalString> inline void m_unchecked_readln(t_text& file, PascalString& destination) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_readln(file, destination));
}

inline t_word m_do_truncate(t_file& file) {
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return open_error;
	}
	if (!file.state->writable) {
		return 105;
	}
	errno = 0;
	const long position = std::ftell(file.state->handle);
	if (position < 0) {
		return m_consume_stdio_error(file.state->handle, 101);
	}
	errno = 0;
	if (std::fflush(file.state->handle) != 0) {
		return m_consume_stdio_error(file.state->handle, 101);
	}
	std::error_code error;
	std::filesystem::resize_file(file.state->name, static_cast<std::uintmax_t>(position), error);
	if (error) {
		return 101;
	}
	errno = 0;
	if (std::fseek(file.state->handle, position, SEEK_SET) != 0) {
		return m_consume_stdio_error(file.state->handle, 156);
	}
	return 0;
}

inline void p_truncate(t_file& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_truncate(file));
}

inline void m_unchecked_truncate(t_file& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_truncate(file));
}

template <typename Count> inline t_word m_binary_transfer_count(Count count, t_longint record_size, std::size_t* records, std::size_t* bytes) {
	if constexpr (std::is_signed_v<Count>) {
		if (count < 0) {
			return 106;
		}
	}
	const auto unsigned_count = static_cast<std::make_unsigned_t<Count>>(count);
	if (unsigned_count > std::numeric_limits<std::size_t>::max()) {
		return 106;
	}
	*records = static_cast<std::size_t>(unsigned_count);
	if (*records > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(record_size)) {
		return 106;
	}
	*bytes = *records * static_cast<std::size_t>(record_size);
	return 0;
}

template <typename Count, typename Result> inline t_word m_do_blockread(t_file& file, tpcc_storage_ref buffer, Count count, Result& result) {
	result = 0;
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return open_error;
	}
	if (!file.state->readable) {
		return 104;
	}
	std::size_t records;
	std::size_t bytes;
	const t_word count_error = m_binary_transfer_count(count, file.state->record_size, &records, &bytes);
	if (count_error != 0) {
		return count_error;
	}
	if (bytes > buffer.size) {
		return 100;
	}
	errno = 0;
	const std::size_t transferred = std::fread(buffer.data, static_cast<std::size_t>(file.state->record_size), records, file.state->handle);
	const int saved_errno = errno;
	result = static_cast<Result>(transferred);
	// A short fread can mean either ordinary end-of-file or an input error.
	// A complete (including zero-length) transfer needs no sticky-state test.
	if (transferred != records && std::ferror(file.state->handle)) {
		return m_consume_stdio_error(file.state->handle, saved_errno, 100);
	}
	return 0;
}

template <typename Count, typename Result> inline void p_blockread(t_file& file, tpcc_storage_ref buffer, Count count, Result& result) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_blockread(file, buffer, count, result));
}

template <typename Count, typename Result> inline void m_unchecked_blockread(t_file& file, tpcc_storage_ref buffer, Count count, Result& result) {
	result = 0;
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_blockread(file, buffer, count, result));
}

template <typename Count> inline t_word m_do_blockread(t_file& file, tpcc_storage_ref buffer, Count count) {
	Count transferred = 0;
	const t_word error = m_do_blockread(file, buffer, count, transferred);
	if (error != 0) {
		return error;
	}
	return transferred == count ? 0 : 100;
}

template <typename Count> inline void p_blockread(t_file& file, tpcc_storage_ref buffer, Count count) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_blockread(file, buffer, count));
}

template <typename Count> inline void m_unchecked_blockread(t_file& file, tpcc_storage_ref buffer, Count count) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_blockread(file, buffer, count));
}

template <typename Count, typename Result> inline t_word m_do_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count, Result& result) {
	result = 0;
	const t_word open_error = m_require_open_binary_file(file);
	if (open_error != 0) {
		return open_error;
	}
	if (!file.state->writable) {
		return 105;
	}
	std::size_t records;
	std::size_t bytes;
	const t_word count_error = m_binary_transfer_count(count, file.state->record_size, &records, &bytes);
	if (count_error != 0) {
		return count_error;
	}
	if (bytes > buffer.size) {
		return 101;
	}
	errno = 0;
	const std::size_t transferred = std::fwrite(buffer.data, static_cast<std::size_t>(file.state->record_size), records, file.state->handle);
	result = static_cast<Result>(transferred);
	// fwrite's count describes this operation. Do not consult ferror here:
	// its sticky indicator could only describe an already-translated failure.
	if (transferred != records) {
		return m_consume_stdio_error(file.state->handle, 101);
	}
	return 0;
}

template <typename Count, typename Result> inline void p_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count, Result& result) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_blockwrite(file, buffer, count, result));
}

template <typename Count, typename Result> inline void m_unchecked_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count, Result& result) {
	result = 0;
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_blockwrite(file, buffer, count, result));
}

template <typename Count> inline t_word m_do_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count) {
	Count transferred = 0;
	const t_word error = m_do_blockwrite(file, buffer, count, transferred);
	if (error != 0) {
		return error;
	}
	return transferred == count ? 0 : 101;
}

template <typename Count> inline void p_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_blockwrite(file, buffer, count));
}

template <typename Count> inline void m_unchecked_blockwrite(t_file& file, tpcc_const_storage_ref buffer, Count count) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_blockwrite(file, buffer, count));
}

/** One already-grouped Pascal `value[:width[:precision]]` item.
 *
 * Both Write/WriteLn and Str consume this representation. Formatting is
 * deliberately independent of the eventual stream or ShortString sink. */
template <typename T> struct tpcc_formatted_value {
	T value;
	bool has_width;
	t_sizeint width;
	bool has_precision;
	t_sizeint precision;
};

template <typename T> inline auto tpcc_make_formatted_value(T&& value) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_formatted_value<value_type>{std::forward<T>(value), false, 0, false, 0};
}

template <typename T> inline auto tpcc_make_formatted_value(T&& value, t_sizeint width) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_formatted_value<value_type>{std::forward<T>(value), true, width, false, 0};
}

template <typename T> inline auto tpcc_make_formatted_value(T&& value, t_sizeint width, t_sizeint precision) {
	using value_type = std::remove_cvref_t<T>;
	return tpcc_formatted_value<value_type>{std::forward<T>(value), true, width, true, precision};
}

template <typename> inline constexpr bool tpcc_dependent_false = false;

inline std::string tpcc_render_default_extended(t_extended value) {
	// The default Extended format uses all 21 significant decimal digits of
	// an 80-bit value and always emits four exponent digits.
	if (!__builtin_isfinite(value)) {
		const bool nan = __builtin_isnan(value);
		const std::size_t spaces = nan ? 26 : 25;
		std::string result(spaces, ' ');
		if (!nan) {
			result.push_back(__builtin_signbit(value) ? '-' : '+');
		}
		result.append(nan ? "Nan" : "Inf");
		return result;
	}

	char digits[64];
	auto [end, error] = std::to_chars(digits, digits + sizeof(digits), value, std::chars_format::scientific, 20);
	if (error != std::errc()) {
		throw std::runtime_error("could not format Extended value");
	}

	std::string result;
	if (digits[0] != '-') {
		result.push_back(' ');
	}
	const char* exponent = std::find(digits, end, 'e');
	if (exponent == end) {
		throw std::runtime_error("formatter produced malformed Extended output");
	}
	result.append(digits, static_cast<std::size_t>(exponent - digits));
	result.push_back('E');
	result.push_back(exponent[1]);
	const std::ptrdiff_t exponent_digits = end - (exponent + 2);
	for (std::ptrdiff_t i = exponent_digits; i < 4; ++i) {
		result.push_back('0');
	}
	result.append(exponent + 2, static_cast<std::size_t>(exponent_digits));
	return result;
}

template <typename T> inline std::string tpcc_render_unpadded_value(const T& value, bool has_precision, t_sizeint precision) {
	std::ostringstream out;
	if constexpr (std::is_same_v<T, std::string>) {
		// Generated enum-name selection produces an internal C++ string
		// after validating its ordinal. It remains inside the formatter and
		// never becomes Pascal-managed storage.
		return value;
	} else if constexpr (tpcc_is_shortstring_v<T> || std::is_same_v<T, t_ansistring>) {
		return value.m_string();
	} else if constexpr (std::is_same_v<T, t_char>) {
		return std::string(1, static_cast<char>(value.value));
	} else if constexpr (std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, t_char>) {
		// PChar is a borrowed zero-terminated character sequence. Unlike
		// counted Pascal strings, its first zero ends the projection; nil is
		// the empty sequence. No encoding conversion is possible because the
		// pointer carries bytes but no code-page metadata.
		if (!value) {
			return {};
		}
		std::string result;
		for (std::size_t i = 0; value[i].value != 0; ++i) {
			result.push_back(static_cast<char>(value[i].value));
		}
		return result;
	} else if constexpr (std::is_same_v<T, t_boolean>) {
		// Boolean is a truth value rather than a general named enumeration:
		// zero formats as FALSE and every nonzero carrier formats as TRUE.
		return value != p_false ? "TRUE" : "FALSE";
	} else if constexpr (std::is_integral_v<T>) {
		// uint8_t/int8_t stream as characters, so widen every Pascal
		// integer carrier before insertion.
		if constexpr (std::is_signed_v<T>) {
			out << static_cast<long long>(value);
		} else {
			out << static_cast<unsigned long long>(value);
		}
	} else if constexpr (std::is_same_v<T, t_extended>) {
		if (!has_precision) {
			return tpcc_render_default_extended(value);
		}
		out << std::fixed << std::setprecision(static_cast<int>(std::max<t_sizeint>(0, precision))) << value;
	} else if constexpr (std::is_floating_point_v<T>) {
		if (has_precision) {
			out << std::fixed << std::setprecision(static_cast<int>(std::max<t_sizeint>(0, precision)));
		}
		out << value;
	} else {
		static_assert(tpcc_dependent_false<T>, "unsupported Pascal Write/WriteLn value type");
	}
	return out.str();
}

struct tpcc_rendered_formatted_value {
	std::size_t left_padding;
	std::string value;
};

template <typename T> inline tpcc_rendered_formatted_value tpcc_render_formatted_value(const tpcc_formatted_value<T>& argument) {
	std::string rendered = tpcc_render_unpadded_value(argument.value, argument.has_precision, argument.precision);
	std::size_t left_padding = 0;
	if (argument.has_width && argument.width > 0 && static_cast<std::make_unsigned_t<t_sizeint>>(argument.width) > rendered.size()) {
		left_padding = static_cast<std::size_t>(argument.width) - rendered.size();
	}
	return tpcc_rendered_formatted_value{left_padding, std::move(rendered)};
}

template <typename T> inline void p_str(const tpcc_formatted_value<T>& argument, t_ansistring& destination) {
	tpcc_rendered_formatted_value rendered = tpcc_render_formatted_value(argument);
	// AnsiString is the unbounded Str destination: retain the complete field.
	// Building the replacement before touching DESTINATION also makes
	// Str(S, S), and a PChar view into S, observe the complete source value.
	// The trailing zero belongs to the managed carrier and is not part of its
	// Pascal length.
	std::vector<t_char> replacement(rendered.left_padding + rendered.value.size() + 1, t_char{0});
	std::fill_n(replacement.data(), rendered.left_padding, t_char{static_cast<uint8_t>(' ')});
	for (std::size_t i = 0; i < rendered.value.size(); ++i) {
		replacement[rendered.left_padding + i] = t_char{static_cast<uint8_t>(static_cast<unsigned char>(rendered.value[i]))};
	}
	destination.storage.m_replace(std::move(replacement));
}

inline t_word tpcc_write_one(std::FILE* out, const t_ansistring& projected) {
	// Pascal overload resolution and the generated Str call have already
	// produced this counted field. The Text sink deliberately knows no source
	// type, so C++ overload lookup cannot select a different formatter.
	const std::size_t length = static_cast<std::size_t>(projected.m_length());
	// fputc and fwrite report this operation through their return values.
	// ferror is deliberately not used: unlike Pascal's IOResult status, it is
	// sticky and does not identify which host operation failed.
	if (length == 0) {
		return 0;
	}
	errno = 0;
	if (std::fwrite(projected.m_data(), 1, length, out) == length) {
		return 0;
	}
	return m_consume_stdio_error(out, 101);
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline t_word tpcc_write_many(std::FILE* out, const Projected&... arguments) {
	if (!out) {
		return 103;
	}
	t_word error = 0;
	if constexpr (sizeof...(Projected) > 0) {
		auto write_one = [out, &error](const auto& argument) {
			if (error != 0) {
				return;
			}
			error = tpcc_write_one(out, argument);
		};
		(write_one(arguments), ...);
	}
	return error;
}

inline m_io_result<std::FILE*> tpcc_text_output(t_text& file) {
	if (!file.state || !file.state->handle || file.state->mode == text_file_mode::Closed) {
		return {nullptr, 103};
	}
	if (file.state->mode != text_file_mode::Output) {
		return {nullptr, 105};
	}
	return {file.state->handle, 0};
}

inline t_word m_do_flush(t_text& file) {
	const auto output = tpcc_text_output(file);
	if (output.error != 0) {
		return output.error;
	}
	errno = 0;
	// fflush's return value reports this flush. A separately tested ferror
	// indicator could instead be residue from an earlier translated failure.
	if (std::fflush(output.value) == 0) {
		return 0;
	}
	return m_consume_stdio_error(output.value, 101);
}

inline void p_flush(t_text& file) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_flush(file));
}

inline void m_unchecked_flush(t_text& file) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_flush(file));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline t_word m_do_write(std::FILE* out, const Projected&... arguments) {
	return tpcc_write_many(out, arguments...);
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline t_word m_do_writeln(std::FILE* out, const Projected&... arguments) {
	const t_word write_error = tpcc_write_many(out, arguments...);
	if (write_error != 0) {
		return write_error;
	}
	errno = 0;
	if (std::fputc('\n', out) != EOF) {
		return 0;
	}
	return m_consume_stdio_error(out, 101);
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void p_write(const Projected&... arguments) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_write(m_stdout_text_state.handle, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void p_write(t_text& file, const Projected&... arguments) {
	m_raise_pending_io_error();
	const auto output = tpcc_text_output(file);
	m_finish_checked_io(output.error);
	m_finish_checked_io(m_do_write(output.value, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void m_unchecked_write(const Projected&... arguments) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_write(m_stdout_text_state.handle, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void m_unchecked_write(t_text& file, const Projected&... arguments) {
	if (m_inoutres != 0) {
		return;
	}
	const auto output = tpcc_text_output(file);
	if (output.error != 0) {
		m_finish_unchecked_io(output.error);
		return;
	}
	m_finish_unchecked_io(m_do_write(output.value, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void p_writeln(const Projected&... arguments) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_writeln(m_stdout_text_state.handle, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void p_writeln(t_text& file, const Projected&... arguments) {
	m_raise_pending_io_error();
	const auto output = tpcc_text_output(file);
	m_finish_checked_io(output.error);
	m_finish_checked_io(m_do_writeln(output.value, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void m_unchecked_writeln(const Projected&... arguments) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_writeln(m_stdout_text_state.handle, arguments...));
}

template <typename... Projected>
	requires((std::is_same_v<std::remove_cvref_t<Projected>, t_ansistring>) && ...)
inline void m_unchecked_writeln(t_text& file, const Projected&... arguments) {
	if (m_inoutres != 0) {
		return;
	}
	const auto output = tpcc_text_output(file);
	if (output.error != 0) {
		m_finish_unchecked_io(output.error);
		return;
	}
	m_finish_unchecked_io(m_do_writeln(output.value, arguments...));
}

inline void p_uniquestring(t_ansistring& value) {
	value.m_make_unique();
}

template <typename I> inline t_char& p_index(t_ansistring& value, I index) {
	p_uniquestring(value);
	return value.index(index);
}

template <typename I> inline const t_char& p_index(const t_ansistring& value, I index) {
	return value.index(index);
}

template <typename I> inline t_char& m_unchecked_index(t_ansistring& value, I index) {
	p_uniquestring(value);
	return value.m_writable_data()[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - 1)];
}

template <typename I> inline const t_char& m_unchecked_index(const t_ansistring& value, I index) {
	return value.m_data()[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index) - 1)];
}

template <typename I> inline tpcc_typed_storage_ref<t_char> tpcc_make_storage_ref(t_ansistring& value, I index) {
	p_uniquestring(value);
	t_char& selected = value.index(index);
	return tpcc_typed_storage_ref<t_char>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        value.storage_extent(index),
	    },
	    std::addressof(selected),
	};
}

template <typename I> inline tpcc_typed_const_storage_ref<t_char> tpcc_make_const_storage_ref(const t_ansistring& value, I index) {
	const t_char& selected = value.index(index);
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        value.storage_extent(index),
	    },
	    std::addressof(selected),
	};
}

template <typename I> inline tpcc_typed_storage_ref<t_char> m_unchecked_storage_ref(t_ansistring& value, I index) {
	t_char& selected = m_unchecked_index(value, index);
	const std::size_t actual = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_storage_ref<t_char>{
	    {
	        reinterpret_cast<std::byte*>(std::addressof(selected)),
	        static_cast<std::size_t>(value.m_length()) - actual + 2,
	    },
	    std::addressof(selected),
	};
}

template <typename I> inline tpcc_typed_const_storage_ref<t_char> m_unchecked_const_storage_ref(const t_ansistring& value, I index) {
	const t_char& selected = m_unchecked_index(value, index);
	const std::size_t actual = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(index));
	return tpcc_typed_const_storage_ref<t_char>{
	    {
	        reinterpret_cast<const std::byte*>(std::addressof(selected)),
	        static_cast<std::size_t>(value.m_length()) - actual + 2,
	    },
	    std::addressof(selected),
	};
}

template <typename I> inline t_char& tpcc_index_write(t_ansistring& value, I index) {
	p_uniquestring(value);
	return value.index(index);
}

template <std::size_t Capacity = 255> inline t_shortstring<Capacity> tpcc_shortstring_from_c(const char* s, std::size_t length) {
	t_shortstring<Capacity> result{};
	const std::size_t stored_length = std::min(length, Capacity);
	result.length = t_char{static_cast<uint8_t>(stored_length)};
	if (stored_length != 0) {
		memcpy(result.data, s, stored_length);
	}
	return result;
}

inline int tpcc_program_argc = 0;
inline char** tpcc_program_argv = nullptr;
inline t_shortstring<255> tpcc_program_executable_path{};

inline void m_set_program_arguments(int argc, char* argv[]) noexcept {
	tpcc_program_argc = std::max(argc, 0);
	tpcc_program_argv = argv;
	tpcc_program_executable_path = {};

	// FPC's Linux System unit obtains ParamStr(0) from /proc/self/exe rather
	// than trusting argv[0], which POSIX allows the caller to choose freely.
	// Keep the fixed 255-byte buffer: System.ParamStr returns ShortString and
	// FPC truncates the executable path to that carrier at startup.
	char executable_path[255];
	const ssize_t length = ::readlink("/proc/self/exe", executable_path, sizeof(executable_path));
	if (length > 0 && executable_path[0] == '/') {
		tpcc_program_executable_path = tpcc_shortstring_from_c(executable_path, static_cast<std::size_t>(length));
	}
}

inline t_shortstring<255> p_paramstr(t_longint index) {
	if (index == 0) {
		return tpcc_program_executable_path;
	}
	if (index < 0 || !tpcc_program_argv || index >= tpcc_program_argc || !tpcc_program_argv[index]) {
		return {};
	}
	const char* argument = tpcc_program_argv[index];
	return tpcc_shortstring_from_c(argument, std::strlen(argument));
}

inline t_longint p_paramcount() {
	// argc includes the executable element at index zero; Pascal's count is
	// the largest valid user-argument index and is never negative.
	return tpcc_program_argc > 0 ? static_cast<t_longint>(tpcc_program_argc - 1) : 0;
}

inline std::error_code m_getdir_bytes(std::string& bytes) {
	std::error_code error;
	const std::filesystem::path path = std::filesystem::current_path(error);
	if (error) {
		return error;
	}
	bytes = path.native();
	return {};
}

} // namespace u_system

namespace u_sysutils {

inline void m_expand_path_components(const std::string& path, std::size_t position, std::vector<std::string>& components) {
	while (position < path.size()) {
		while (position < path.size() && path[position] == '/') {
			++position;
		}
		const std::size_t start = position;
		while (position < path.size() && path[position] != '/') {
			++position;
		}
		if (start == position) {
			continue;
		}
		const std::string component = path.substr(start, position - start);
		if (component == ".") {
			continue;
		}
		if (component == "..") {
			if (!components.empty()) {
				components.pop_back();
			}
			continue;
		}
		components.push_back(component);
	}
}

inline void m_expand_rooted_path(const std::string& path, std::vector<std::string>& components) {
	std::size_t position = 0;
	while (position < path.size() && path[position] == '/') {
		++position;
	}
	// FPC retains one extra leading separator for every path which starts
	// with two or more. Treat it as a component so a leading `..` can
	// remove it just as FPC's textual reduction does.
	if (position >= 2) {
		components.emplace_back();
	}
	m_expand_path_components(path, position, components);
}

inline t_ansistring p_expandfilename(const t_ansistring& file_name) {
	std::string path = file_name.m_string();
	for (char& character : path) {
		if (character == '\\') {
			character = '/';
		}
	}
	const bool keep_trailing_separator = path.empty() || path.back() == '/';

	if (!path.empty() && path[0] == '~' && (path.size() == 1 || path[1] == '/')) {
		const char* environment_home = std::getenv("HOME");
		const std::string home = environment_home ? environment_home : "";
		if (home.empty() || (home == "/" && path.size() > 1)) {
			path.erase(0, 1);
		} else if (home.back() == '/') {
			path = home + (path.size() > 1 ? path.substr(2) : "");
		} else {
			path = home + path.substr(1);
		}
	}

	std::vector<std::string> components;
	if (path.empty() || path[0] != '/') {
		std::string current_directory;
		if (::u_system::m_getdir_bytes(current_directory)) {
			current_directory = "/";
		}
		m_expand_rooted_path(current_directory, components);
		m_expand_path_components(path, 0, components);
	} else {
		m_expand_rooted_path(path, components);
	}

	std::string result = "/";
	for (const std::string& component : components) {
		if (component.empty()) {
			result.push_back('/');
			continue;
		}
		if (result.back() != '/') {
			result.push_back('/');
		}
		result += component;
	}

	if (keep_trailing_separator) {
		if (result.back() != '/') {
			result.push_back('/');
		}
	} else if (result.size() > 1 && result.back() == '/') {
		result.pop_back();
	}
	return tpcc_ansistring_literal(result.data(), result.size());
}

} // namespace u_sysutils

namespace u_system {

inline void p_getdir(t_byte drive_number, t_shortstring<255>& directory) {
	// Unix has one directory tree, so the DOS drive selector has no
	// semantic effect.
	std::string bytes;
	const std::error_code error = m_getdir_bytes(bytes);
	if (error) {
		m_set_io_error(m_file_error_from_errno(error.value(), 3));
		return;
	}

	if (bytes.size() > 255) {
		// The ShortString overload must not truncate a directory name.
		// Preserve the destination and report the TP-compatible
		// "path not found" status, as FPC does.
		m_set_io_error(3);
		return;
	}
	directory = tpcc_shortstring_from_c(bytes.data(), bytes.size());
}

inline void p_getdir(t_byte drive_number, t_ansistring& directory) {
	std::string bytes;
	const std::error_code error = m_getdir_bytes(bytes);
	if (error) {
		m_set_io_error(m_file_error_from_errno(error.value(), 3));
		return;
	}
	directory = tpcc_ansistring_literal(bytes.data(), bytes.size());
}

inline t_word m_rmdir_error_from_errno(int error) {
	switch (error) {
	case ENAMETOOLONG:
		return 3;
	case EROFS:
	case EEXIST:
	case ENOTEMPTY:
	case EACCES:
	case EPERM:
	case EBUSY:
	case ENOTDIR:
	case EISDIR:
		return 5;
	default:
		return m_file_error_from_errno(error, error != 0 ? static_cast<t_word>(error) : static_cast<t_word>(5));
	}
}

template <typename PascalString> inline t_word m_do_rmdir(const PascalString& directory) {
	const std::string path = directory.m_string();
	if (path.empty()) {
		return 0;
	}
	if (path == ".") {
		return 16;
	}
	errno = 0;
	if (::rmdir(path.c_str()) == 0) {
		return 0;
	}
	return m_rmdir_error_from_errno(errno);
}

template <typename PascalString> inline void p_rmdir(const PascalString& directory) {
	m_raise_pending_io_error();
	m_finish_checked_io(m_do_rmdir(directory));
}

template <typename PascalString> inline void m_unchecked_rmdir(const PascalString& directory) {
	if (m_inoutres != 0) {
		return;
	}
	m_finish_unchecked_io(m_do_rmdir(directory));
}

inline t_shortstring<255> o_implicit(t_char value, m_conversion_target<t_shortstring<255>>) {
	t_shortstring<255> result{};
	result.length = 1;
	result.data[0] = value;
	return result;
}

template <typename T>
        requires std::is_integral_v<T>
inline t_char p_chr(T value) {
	return t_char{static_cast<t_byte>(value)};
}

inline void p_fillchar(tpcc_storage_ref destination, t_sizeint count, t_byte value) {
	if (count <= 0) {
		return;
	}
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > destination.size) {
		m_runtime_error(201);
	}
	std::memset(destination.data, value, byte_count);
}

inline void p_fillchar(tpcc_storage_ref destination, t_sizeint count, t_char value) {
	p_fillchar(destination, count, value.value);
}

inline void p_fillbyte(tpcc_storage_ref destination, t_sizeint count, t_byte value) {
	if (count <= 0) {
		return;
	}
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > destination.size) {
		m_runtime_error(201);
	}
	std::memset(destination.data, value, byte_count);
}

inline void p_filldword(tpcc_storage_ref destination, t_sizeint count, t_longword value) {
	if (count <= 0) {
		return;
	}
	constexpr std::size_t element_size = sizeof(t_longword);
	const std::size_t element_count = static_cast<std::size_t>(count);
	if (element_count > std::numeric_limits<std::size_t>::max() / element_size) {
		m_runtime_error(201);
	}
	const std::size_t byte_count = element_count * element_size;
	if (byte_count > destination.size) {
		m_runtime_error(201);
	}

	// A Pascal untyped `var` destination need not be aligned for DWord and
	// need not denote an existing C++ uint32_t object. Seed its first four
	// bytes with memcpy, then replicate that native-endian representation.
	// Casting destination.data to t_longword* would instead introduce both
	// alignment and object-lifetime/aliasing UB.
	std::memcpy(destination.data, std::addressof(value), element_size);
	std::size_t initialized = element_size;
	while (initialized < byte_count) {
		const std::size_t chunk = std::min(initialized, byte_count - initialized);
		std::memcpy(destination.data + initialized, destination.data, chunk);
		initialized += chunk;
	}
}

inline void p_prefetch(tpcc_const_storage_ref memory) {
	// __builtin_prefetch is a GCC/Clang language extension.
#if defined(__clang__)
#if __has_builtin(__builtin_prefetch)
	__builtin_prefetch(memory.data, 0, 0);
#endif
#elif defined(__GNUC__)
	__builtin_prefetch(memory.data, 0, 0);
#endif
}

inline void p_move(tpcc_const_storage_ref source, tpcc_storage_ref destination, t_sizeint count) {
	if (count <= 0) {
		return;
	}
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > source.size) {
		m_runtime_error(201);
	}
	if (byte_count > destination.size) {
		m_runtime_error(201);
	}
	std::memmove(destination.data, source.data, byte_count);
}

inline std::size_t tpcc_index_element_count(tpcc_const_storage_ref buffer, t_sizeint count, std::size_t element_size) {
	if (count == 0) {
		return 0;
	}
	if (count < 0) {
		// FPC deliberately interprets a negative IndexByte/IndexWord length as
		// an unsigned, effectively unbounded search. Emitted omitted-type
		// arguments retain their available byte extent, so search that entire
		// extent instead of allowing the C++ implementation to read beyond it.
		return buffer.size / element_size;
	}
	const std::size_t element_count = static_cast<std::size_t>(count);
	if (element_count > buffer.size / element_size) {
		m_runtime_error(201);
	}
	return element_count;
}

inline t_sizeint p_indexbyte(tpcc_const_storage_ref buffer, t_sizeint count, t_byte value) {
	const std::size_t element_count = tpcc_index_element_count(buffer, count, sizeof(t_byte));
	if (element_count == 0) {
		return -1;
	}
	const void* found = std::memchr(buffer.data, static_cast<int>(value), element_count);
	if (!found) {
		return -1;
	}
	return static_cast<t_sizeint>(static_cast<const std::byte*>(found) - buffer.data);
}

inline t_sizeint p_indexword(tpcc_const_storage_ref buffer, t_sizeint count, t_word value) {
	const std::size_t element_count = tpcc_index_element_count(buffer, count, sizeof(t_word));
	for (std::size_t index = 0; index < element_count; ++index) {
		t_word current;
		// Pascal permits the untyped buffer to be unaligned. memcpy reads the
		// native Word representation without creating a misaligned t_word*
		// or violating C++ object-lifetime and aliasing rules.
		std::memcpy(std::addressof(current), buffer.data + index * sizeof(t_word), sizeof(t_word));
		if (current == value) {
			return static_cast<t_sizeint>(index);
		}
	}
	return -1;
}

inline t_sizeint p_comparebyte(tpcc_const_storage_ref first, tpcc_const_storage_ref second, t_sizeint count) {
	if (count <= 0) {
		return 0;
	}
	const std::size_t byte_count = static_cast<std::size_t>(count);
	if (byte_count > first.size) {
		m_runtime_error(201);
	}
	if (byte_count > second.size) {
		m_runtime_error(201);
	}
	const int comparison = std::memcmp(first.data, second.data, byte_count);
	return comparison < 0 ? -1 : comparison > 0 ? 1 : 0;
}

inline t_sizeint p_comparechar(tpcc_const_storage_ref first, tpcc_const_storage_ref second, t_sizeint count) {
	return p_comparebyte(first, second, count);
}

inline t_sizeint p_compareword(tpcc_const_storage_ref first, tpcc_const_storage_ref second, t_sizeint count) {
	if (count <= 0) {
		return 0;
	}
	const std::size_t element_count = static_cast<std::size_t>(count);
	if (element_count > first.size / sizeof(t_word)) {
		m_runtime_error(201);
	}
	if (element_count > second.size / sizeof(t_word)) {
		m_runtime_error(201);
	}
	for (std::size_t index = 0; index < element_count; ++index) {
		t_word first_word;
		t_word second_word;
		// The untyped Pascal buffers need not be Word-aligned and need not
		// contain C++ t_word objects. Copying their representations avoids
		// alignment, lifetime, and strict-aliasing undefined behavior.
		std::memcpy(std::addressof(first_word), first.data + index * sizeof(t_word), sizeof(t_word));
		std::memcpy(std::addressof(second_word), second.data + index * sizeof(t_word), sizeof(t_word));
		if (first_word < second_word) {
			return -1;
		}
		if (first_word > second_word) {
			return 1;
		}
	}
	return 0;
}

template <typename Needle, typename Haystack>
        requires(tpcc_is_shortstring_v<Needle> || std::is_same_v<Needle, t_ansistring>) && (tpcc_is_shortstring_v<Haystack> || std::is_same_v<Haystack, t_ansistring>)
inline t_longint p_pos(const Needle& needle, const Haystack& haystack) {
	const std::size_t needle_length = static_cast<std::size_t>(needle.m_length());
	const std::size_t haystack_length = static_cast<std::size_t>(haystack.m_length());
	if (needle_length == 0) {
		return 0;
	}
	if (needle_length > haystack_length) {
		return 0;
	}
	const std::size_t last = haystack_length - needle_length;
	for (std::size_t offset = 0; offset <= last; ++offset) {
		if (std::memcmp(haystack.m_data() + offset, needle.m_data(), needle_length) == 0) {
			return static_cast<t_longint>(offset + 1);
		}
	}
	return 0;
}

template <typename Haystack>
        requires tpcc_is_shortstring_v<Haystack> || std::is_same_v<Haystack, t_ansistring>
inline t_longint p_pos(t_char needle, const Haystack& haystack) {
	const std::size_t length = static_cast<std::size_t>(haystack.m_length());
	for (std::size_t offset = 0; offset < length; ++offset) {
		if (haystack.m_data()[offset] == needle) {
			return static_cast<t_longint>(offset + 1);
		}
	}
	return 0;
}

// Pascal Copy uses one-based indices and returns the ordinary 255-byte
// ShortString type declared by System.
template <std::size_t Capacity> inline t_shortstring<255> p_copy(const t_shortstring<Capacity>& value, t_sizeint index, t_sizeint count) {
	t_shortstring<255> result{};
	if (count <= 0) {
		return result;
	}
	if (index < 1) {
		index = 1;
	}
	const std::size_t start = static_cast<std::size_t>(index - 1);
	const std::size_t source_length = value.length;
	if (start >= source_length) {
		return result;
	}
	const std::size_t requested = static_cast<std::size_t>(count);
	const std::size_t copied = std::min({
	    requested,
	    source_length - start,
	    decltype(result)::capacity,
	});
	result.length = t_char{static_cast<uint8_t>(copied)};
	if (copied != 0) {
		std::memcpy(result.data, value.data + start, copied);
	}
	return result;
}

inline t_ansistring p_copy(const t_ansistring& value, t_sizeint index, t_sizeint count) {
	return value.slice(index, count);
}

inline t_shortstring<255> p_copy(t_char value, t_sizeint index, t_sizeint count) {
	t_shortstring<255> source{};
	source.length = t_char{1};
	source.data[0] = value;
	return p_copy(source, index, count);
}

template <std::size_t Capacity> inline void p_delete(t_shortstring<Capacity>& value, t_sizeint index, t_sizeint count) {
	if (index < 1 || count <= 0) {
		return;
	}
	const std::size_t start = static_cast<std::size_t>(index - 1);
	const std::size_t length = value.length;
	if (start >= length) {
		return;
	}
	const std::size_t requested = static_cast<std::size_t>(count);
	const std::size_t removed = std::min(requested, length - start);
	const std::size_t tail = length - start - removed;
	std::memmove(value.data + start, value.data + start + removed, tail);
	value.length = static_cast<uint8_t>(length - removed);
}

inline void p_delete(t_ansistring& value, t_sizeint index, t_sizeint count) {
	value.erase(index, count);
}

template <typename T>
        requires tpcc_is_shortstring_v<T>
inline void p_delete(tpcc_typed_storage_ref<T> value, t_sizeint index, t_sizeint count) {
	// An omitted mutable System formal reaches the RTL as a typed storage
	// view.
	p_delete(*value.value, index, count);
}

template <std::size_t SourceCapacity, std::size_t DestinationCapacity> inline void p_insert(const t_shortstring<SourceCapacity>& source, t_shortstring<DestinationCapacity>& value, t_sizeint index) {
	if (source.length == 0) {
		return;
	}

	if (index < 1) {
		index = 1;
	}

	std::size_t start = static_cast<std::size_t>(index - 1);
	// Pascal Semantics: index > length acts as append
	if (start > value.length) {
		start = value.length;
	}

	// Copy the source payload before moving the destination tail. Besides the
	// ordinary Insert(S, S, ...) case, different-capacity references can alias
	// through Pascal pointer casts.
	t_shortstring<SourceCapacity> temp_source = source;
	const auto* p_src = &temp_source;

	// Truncation logic: Calculate how much of the source we can actually fit
	std::size_t max_insert = DestinationCapacity - start;
	std::size_t copy_count = std::min(static_cast<std::size_t>(p_src->length), max_insert);

	if (copy_count == 0) {
		return;
	}

	std::size_t max_tail = DestinationCapacity - (start + copy_count);
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

template <std::size_t Capacity> inline void p_insert(t_char source, t_shortstring<Capacity>& destination, t_sizeint index) {
	t_shortstring<1> one_character{};
	one_character.length = 1;
	one_character.data[0] = source;
	p_insert(one_character, destination, index);
}

inline void p_insert(const t_ansistring& source, t_ansistring& destination, t_sizeint index) {
	destination.insert(source, index);
}

template <typename Source, typename Destination>
        requires tpcc_is_shortstring_v<Destination> && (tpcc_is_shortstring_v<Source> || std::is_same_v<std::remove_cv_t<Source>, t_char>)
inline void p_insert(const Source& source, tpcc_typed_storage_ref<Destination> destination, t_sizeint index) {
	// As with Delete, preserve the actual destination String[N] selected by
	// Pascal and reuse the existing capacity-aware implementation.
	p_insert(source, *destination.value, index);
}

template <std::size_t ACapacity, std::size_t BCapacity> inline t_shortstring<255> m_shortstring_add(const t_shortstring<ACapacity>& a, const t_shortstring<BCapacity>& b) {
	t_shortstring<255> result{};
	const std::size_t result_length = std::min<std::size_t>(static_cast<std::size_t>(a.length) + static_cast<std::size_t>(b.length), decltype(result)::capacity);
	const std::size_t a_length = std::min<std::size_t>(a.length, result_length);
	const std::size_t b_length = result_length - a_length;
	result.length = static_cast<uint8_t>(result_length);
	memcpy(result.data, a.data, a_length);
	memcpy(&result.data[a_length], b.data, b_length);
	return result;
}

template <std::size_t ACapacity, std::size_t BCapacity> inline t_shortstring<255> o_unchecked_add(const t_shortstring<ACapacity>& a, const t_shortstring<BCapacity>& b) {
	return m_shortstring_add(a, b);
}

template <std::size_t ACapacity, std::size_t BCapacity> inline t_shortstring<255> o_add(const t_shortstring<ACapacity>& a, const t_shortstring<BCapacity>& b) {
	// String concatenation has no integer overflow distinction, but it still
	// occupies the checked family selected before operand overloads are known.
	return m_shortstring_add(a, b);
}

inline t_ansistring m_ansistring_add(const t_ansistring& a, const t_ansistring& b) {
	t_ansistring result;
	const std::size_t a_length = static_cast<std::size_t>(a.m_length());
	const std::size_t b_length = static_cast<std::size_t>(b.m_length());
	std::vector<t_char> replacement(a_length + b_length + 1, t_char{0});
	std::copy_n(a.m_data(), a_length, replacement.data());
	std::copy_n(b.m_data(), b_length, replacement.data() + a_length);
	result.storage.m_replace(std::move(replacement));
	return result;
}

inline t_ansistring o_unchecked_add(const t_ansistring& a, const t_ansistring& b) {
	return m_ansistring_add(a, b);
}

inline t_ansistring o_add(const t_ansistring& a, const t_ansistring& b) {
	return m_ansistring_add(a, b);
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline int tpcc_stringcmp(const A& a, const B& b) {
	const auto a_length = a.m_length();
	const auto b_length = b.m_length();
	int r = memcmp(a.m_data(), b.m_data(), static_cast<std::size_t>(std::min(a_length, b_length)));
	if (r == 0) {
		return a_length < b_length ? -1 : a_length > b_length ? 1 : 0;
	}
	return r;
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline t_boolean o_lessthan(const A& a, const B& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) < 0);
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline t_boolean o_lessthanorequal(const A& a, const B& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) <= 0);
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline t_boolean o_equal(const A& a, const B& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) == 0);
}

template <typename T> inline t_boolean o_equal(const t_dynamicarray<T>& a, const t_dynamicarray<T>& b) {
	return tpcc_bool_to_boolean(a.m_identity() == b.m_identity());
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline t_boolean o_greaterthan(const A& a, const B& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) > 0);
}

template <typename A, typename B>
        requires(tpcc_is_shortstring_v<A> || std::is_same_v<A, t_ansistring>) && (tpcc_is_shortstring_v<B> || std::is_same_v<B, t_ansistring>)
inline t_boolean o_greaterthanorequal(const A& a, const B& b) {
	return tpcc_bool_to_boolean(tpcc_stringcmp(a, b) >= 0);
}

inline t_char o_implicit(t_char value, m_conversion_target<t_char>) {
	return value;
}

template <std::size_t Capacity> inline t_ansistring o_implicit(t_shortstring<Capacity> value, m_conversion_target<t_ansistring>) {
	t_ansistring result{};
	result.assign(value);
	return result;
}

inline t_ansistring o_implicit(t_char* source, m_conversion_target<t_ansistring>) {
	t_ansistring result{};
	// Pascal defines a nil PChar as an empty string in this conversion.
	// Otherwise the terminator supplies the length which PChar does not carry.
	if (!source) {
		return result;
	}
	std::size_t length = 0;
	while (source[length].value != 0) {
		++length;
	}
	std::vector<t_char> replacement(length + 1, t_char{0});
	std::copy_n(source, length, replacement.data());
	result.storage.m_replace(std::move(replacement));
	return result;
}

inline t_boolean o_lessthan(t_char a, t_char b) {
	return tpcc_bool_to_boolean(a.value < b.value);
}

inline t_boolean o_lessthanorequal(t_char a, t_char b) {
	return tpcc_bool_to_boolean(a.value <= b.value);
}

inline t_boolean o_equal(t_char a, t_char b) {
	return tpcc_bool_to_boolean(a.value == b.value);
}

inline t_boolean o_greaterthan(t_char a, t_char b) {
	return tpcc_bool_to_boolean(a.value > b.value);
}

inline t_boolean o_greaterthanorequal(t_char a, t_char b) {
	return tpcc_bool_to_boolean(a.value >= b.value);
}

inline t_boolean o_lessthan(t_widechar a, t_widechar b) {
	return tpcc_bool_to_boolean(a.value < b.value);
}

inline t_boolean o_lessthanorequal(t_widechar a, t_widechar b) {
	return tpcc_bool_to_boolean(a.value <= b.value);
}

inline t_boolean o_equal(t_widechar a, t_widechar b) {
	return tpcc_bool_to_boolean(a.value == b.value);
}

inline t_boolean o_greaterthan(t_widechar a, t_widechar b) {
	return tpcc_bool_to_boolean(a.value > b.value);
}

inline t_boolean o_greaterthanorequal(t_widechar a, t_widechar b) {
	return tpcc_bool_to_boolean(a.value >= b.value);
}

// Pascal enumerations are nominal, but equality and ordering are defined
// between values of one exact enum type. The parser establishes that nominal
// identity; the C++ template merely implements the already-selected call.
template <typename T>
        requires std::is_enum_v<T>
inline t_boolean o_lessthan(T a, T b) {
	return tpcc_bool_to_boolean(a < b);
}

template <typename T>
        requires std::is_enum_v<T>
inline t_boolean o_lessthanorequal(T a, T b) {
	return tpcc_bool_to_boolean(a <= b);
}

template <typename T>
        requires std::is_enum_v<T>
inline t_boolean o_equal(T a, T b) {
	return tpcc_bool_to_boolean(a == b);
}

template <typename T>
        requires std::is_enum_v<T>
inline t_boolean o_greaterthan(T a, T b) {
	return tpcc_bool_to_boolean(a > b);
}

template <typename T>
        requires std::is_enum_v<T>
inline t_boolean o_greaterthanorequal(T a, T b) {
	return tpcc_bool_to_boolean(a >= b);
}

// PChar comparisons are address comparisons, not NUL-terminated string
// comparisons. std::less supplies the implementation's strict total pointer
// order even outside one C++ array object; == remains ordinary pointer identity.
// This is deliberately specific to PChar rather than a generic typed-pointer
// operator family.
inline t_boolean o_lessthan(t_char* a, t_char* b) {
	return tpcc_bool_to_boolean(std::less<t_char*>{}(a, b));
}

inline t_boolean o_lessthanorequal(t_char* a, t_char* b) {
	return tpcc_bool_to_boolean(!std::less<t_char*>{}(b, a));
}

inline t_boolean o_equal(t_char* a, t_char* b) {
	return tpcc_bool_to_boolean(a == b);
}

inline t_boolean o_greaterthan(t_char* a, t_char* b) {
	return tpcc_bool_to_boolean(std::less<t_char*>{}(b, a));
}

inline t_boolean o_greaterthanorequal(t_char* a, t_char* b) {
	return tpcc_bool_to_boolean(!std::less<t_char*>{}(a, b));
}

// Pascal Pointer equality compares pointer values; it does not inspect the
// pointed-to storage. Typed pointers reach this overload through Pascal's
// existing typed-pointer/untyped-Pointer compatibility conversion.
inline t_boolean o_equal(t_pointer a, t_pointer b) {
	return tpcc_bool_to_boolean(a == b);
}

template <typename T> inline t_longword p_ord(T x) {
	return static_cast<t_longword>(tpcc_ordinal_storage<T>::get(x));
}

template <typename T> inline t_longword p_ord(tpcc_typed_const_storage_ref<T> x) {
	return p_ord(*x.value);
}

template <typename T> inline T p_low() {
	if constexpr (std::is_same_v<T, t_char>) {
		return t_char{0};
	} else if constexpr (std::is_same_v<T, t_widechar>) {
		return t_widechar{0};
	} else {
		return std::numeric_limits<T>::lowest();
	}
}

template <typename T> inline T p_high() {
	if constexpr (std::is_same_v<T, t_char>) {
		return t_char{255};
	} else if constexpr (std::is_same_v<T, t_widechar>) {
		return t_widechar{65535};
	} else {
		return std::numeric_limits<T>::max();
	}
}

template <typename T>
        requires requires(const T& value) { value.m_low(); }
inline auto p_low(const T& value) {
	return value.m_low();
}

template <typename T>
        requires requires(const T& value) { value.m_high(); }
inline auto p_high(const T& value) {
	return value.m_high();
}

template <typename T>
        requires requires(const T& value) { value.m_length(); }
inline auto p_length(const T& value) {
	// Concrete ShortString and AnsiString declarations expose their native
	// Pascal result carriers; the generic sequence fallback uses SizeInt for
	// array families, whose m_length() already returns SizeInt.
	return value.m_length();
}

template <typename T> inline auto p_length(tpcc_typed_const_storage_ref<T> value) -> decltype(p_length(*value.value)) {
	return p_length(*value.value);
}

inline void p_setlength(t_ansistring& value, t_sizeint length) {
	value.m_resize(length);
}

inline void p_setstring(t_ansistring& destination, t_char* buffer, t_sizeint length) {
	// SetString consumes exactly `length` characters; a zero byte in that
	// range is data rather than a terminator. Constructing the replacement
	// before releasing destination also keeps a buffer which points into the
	// destination's current storage valid until the counted copy is complete.
	if (length <= 0) {
		destination.storage.m_replace({});
		return;
	}
	const std::size_t count = static_cast<std::size_t>(length);
	std::vector<t_char> replacement(count + 1, t_char{0});
	if (buffer) {
		std::copy_n(buffer, count, replacement.data());
	}
	// The final zero belongs to the AnsiString carrier, not its Pascal length.
	// With nil buffer the Pascal payload is unspecified; retaining the
	// value-initialized bytes avoids exposing uninitialized host memory.
	destination.storage.m_replace(std::move(replacement));
}

template <typename T>
        requires tpcc_is_shortstring_v<T>
inline void p_setstring(tpcc_typed_storage_ref<T> destination, t_char* buffer, t_sizeint length) {
	// String[N] has inline storage and therefore clamps the counted source to
	// its declared capacity. Unlike AnsiString, a nil source exposes the
	// existing inline payload after changing only the logical length.
	const std::size_t count = length <= 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(length), T::capacity);
	destination.value->m_resize(static_cast<t_sizeint>(count));
	if (buffer) {
		std::copy_n(buffer, count, destination.value->m_data());
	}
}

template <typename T>
inline void p_setlength(tpcc_typed_storage_ref<T> value, t_sizeint length)
        requires requires(T& sequence) { sequence.m_resize(length); }
{
	value.value->m_resize(length);
}

template <typename T> inline t_sizeint p_sizeof(tpcc_typed_const_storage_ref<T>) {
	return static_cast<t_sizeint>(sizeof(T));
}

template <typename T>
        requires std::is_integral_v<T>
inline T m_integer_from_bits(std::make_unsigned_t<T> bits) {
	if constexpr (std::is_signed_v<T>) {
		// C++ conversion from an out-of-range unsigned value to a signed type
		// is implementation-defined. bit_cast states the two's-complement
		// carrier operation TPCC needs for unchecked Pascal arithmetic.
		return std::bit_cast<T>(bits);
	} else {
		return bits;
	}
}

template <typename T>
        requires std::is_integral_v<T> && (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8)
inline T m_swap_endian_integer(T value) {
	using unsigned_type = std::make_unsigned_t<T>;
	unsigned_type remaining = static_cast<unsigned_type>(value);
	unsigned_type reversed = 0;
	// Work on the carrier bits rather than the numeric sign. Unsigned shifts
	// are defined modulo the carrier width, and m_integer_from_bits states the
	// resulting signed two's-complement representation without an
	// implementation-defined unsigned-to-signed conversion.
	for (std::size_t i = 0; i < sizeof(T); ++i) {
		reversed = static_cast<unsigned_type>((reversed << 8) | (remaining & unsigned_type{0xff}));
		remaining >>= 8;
	}
	return m_integer_from_bits<T>(reversed);
}

inline t_smallint p_swapendian(const t_smallint& value) {
	return m_swap_endian_integer(value);
}

inline t_word p_swapendian(const t_word& value) {
	return m_swap_endian_integer(value);
}

inline t_longint p_swapendian(const t_longint& value) {
	return m_swap_endian_integer(value);
}

inline t_longword p_swapendian(const t_longword& value) {
	return m_swap_endian_integer(value);
}

inline t_int64 p_swapendian(const t_int64& value) {
	return m_swap_endian_integer(value);
}

inline t_qword p_swapendian(const t_qword& value) {
	return m_swap_endian_integer(value);
}

template <typename Result, typename Operand>
        requires std::is_integral_v<Result>
inline Result m_arithmetic_operand(Operand value) {
	return static_cast<Result>(value);
}

template <typename Result, typename Operand>
        requires std::is_integral_v<Result>
inline std::make_unsigned_t<Result> m_arithmetic_operand_bits(Operand value) {
	return static_cast<std::make_unsigned_t<Result>>(m_arithmetic_operand<Result>(value));
}

#define TPCC_DEFINE_INTEGER_ARITHMETIC_OPERATIONS(T, ARITH_RESULT, DIV_RESULT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	inline ARITH_RESULT o_unchecked_add(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
		using U = std::make_unsigned_t<ARITH_RESULT>;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return m_integer_from_bits<ARITH_RESULT>(static_cast<U>(m_arithmetic_operand_bits<ARITH_RESULT>(a) + m_arithmetic_operand_bits<ARITH_RESULT>(b)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline ARITH_RESULT o_add(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		ARITH_RESULT result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		if (__builtin_add_overflow(m_arithmetic_operand<ARITH_RESULT>(a), m_arithmetic_operand<ARITH_RESULT>(b), &result))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
			m_runtime_error(215);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline ARITH_RESULT o_unchecked_subtract(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		using U = std::make_unsigned_t<ARITH_RESULT>;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return m_integer_from_bits<ARITH_RESULT>(static_cast<U>(m_arithmetic_operand_bits<ARITH_RESULT>(a) - m_arithmetic_operand_bits<ARITH_RESULT>(b)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline ARITH_RESULT o_subtract(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		ARITH_RESULT result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		if (__builtin_sub_overflow(m_arithmetic_operand<ARITH_RESULT>(a), m_arithmetic_operand<ARITH_RESULT>(b), &result))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
			m_runtime_error(215);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_positive(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_unchecked_negative(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		using U = std::make_unsigned_t<T>;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return m_integer_from_bits<T>(static_cast<U>(U{0} - m_arithmetic_operand_bits<T>(b)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_negative(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		T result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
		if (__builtin_sub_overflow(T{0}, b, &result))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
			m_runtime_error(215);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline ARITH_RESULT o_unchecked_multiply(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		using U = std::make_unsigned_t<ARITH_RESULT>;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return m_integer_from_bits<ARITH_RESULT>(static_cast<U>(m_arithmetic_operand_bits<ARITH_RESULT>(a) * m_arithmetic_operand_bits<ARITH_RESULT>(b)));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline ARITH_RESULT o_multiply(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		ARITH_RESULT result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		if (__builtin_mul_overflow(m_arithmetic_operand<ARITH_RESULT>(a), m_arithmetic_operand<ARITH_RESULT>(b), &result))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
			m_runtime_error(215);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		return result;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline DIV_RESULT o_divide(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
		return static_cast<DIV_RESULT>(a) / static_cast<DIV_RESULT>(b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_implicit(T source, m_conversion_target<T>) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		T target = source;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return target;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_lessthan(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		return tpcc_bool_to_boolean(a < b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_lessthanorequal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
		return tpcc_bool_to_boolean(a <= b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_equal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		return tpcc_bool_to_boolean(a == b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_greaterthan(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return tpcc_bool_to_boolean(a > b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_greaterthanorequal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
		return tpcc_bool_to_boolean(a >= b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}

#define TPCC_DEFINE_REAL_ARITHMETIC_OPERATIONS(T)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
	inline T o_unchecked_add(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		return a + b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_add(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return a + b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_unchecked_subtract(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
		return a - b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_subtract(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
		return a - b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_positive(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_unchecked_negative(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		return -b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_negative(T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return -b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_unchecked_multiply(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
		return a * b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_multiply(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
		return a * b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_divide(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
		return a / b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline T o_implicit(T source, m_conversion_target<T>) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		T target = source;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return target;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_lessthan(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		return tpcc_bool_to_boolean(a < b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_lessthanorequal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
		return tpcc_bool_to_boolean(a <= b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_equal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		return tpcc_bool_to_boolean(a == b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_greaterthan(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
		return tpcc_bool_to_boolean(a > b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline t_boolean o_greaterthanorequal(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
		return tpcc_bool_to_boolean(a >= b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
	}

template <typename Result>
        requires std::is_integral_v<Result>
inline Result m_unchecked_intdivide(Result a, Result b) {
	if (b == 0) {
		m_runtime_error(200);
	}
	if constexpr (std::is_signed_v<Result>) {
		if (a == std::numeric_limits<Result>::min() && b == Result{-1}) {
			// The mathematical positive result has the same low bits as
			// Low(Result). Return those bits without executing C++'s
			// undefined minimum/-1 division.
			return std::numeric_limits<Result>::min();
		}
	}
	return a / b;
}

template <typename Result>
        requires std::is_integral_v<Result>
inline Result m_checked_intdivide(Result a, Result b) {
	if (b == 0) {
		m_runtime_error(200);
	}
	if constexpr (std::is_signed_v<Result>) {
		if (a == std::numeric_limits<Result>::min() && b == Result{-1}) {
			m_runtime_error(215);
		}
	}
	return a / b;
}

template <typename Result>
        requires std::is_integral_v<Result>
inline Result m_modulus(Result a, Result b) {
	if (b == 0) {
		m_runtime_error(200);
	}
	if constexpr (std::is_signed_v<Result>) {
		if (a == std::numeric_limits<Result>::min() && b == Result{-1}) {
			// Pascal's remainder is exactly zero here, but evaluating the
			// equivalent C++ `%` expression would still be undefined.
			return 0;
		}
	}
	return a % b;
}

template <typename Result, typename Count>
        requires std::is_integral_v<Result> && std::is_integral_v<Count>
inline Result m_leftshift(Result value, Count count) {
	using UResult = std::make_unsigned_t<Result>;
	constexpr unsigned width = std::numeric_limits<UResult>::digits;
	using UCount = std::make_unsigned_t<Count>;
	// Pascal specifies results only for counts below the promoted result
	// width. Masking outside that domain merely keeps the C++ implementation
	// from executing an undefined host shift; it is not a Pascal guarantee.
	UCount amount = static_cast<UCount>(count) & static_cast<UCount>(width - 1);
	return m_integer_from_bits<Result>(static_cast<UResult>(static_cast<UResult>(value) << amount));
}

template <typename Result, typename Count>
        requires std::is_integral_v<Result> && std::is_integral_v<Count>
inline Result m_rightshift(Result value, Count count) {
	using UResult = std::make_unsigned_t<Result>;
	constexpr unsigned width = std::numeric_limits<UResult>::digits;
	using UCount = std::make_unsigned_t<Count>;
	UCount amount = static_cast<UCount>(count) & static_cast<UCount>(width - 1);
	return m_integer_from_bits<Result>(static_cast<UResult>(value) >> amount);
}

#define TPCC_DEFINE_INTEGER_OPERATIONS(T, INTEGER_RESULT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	/* Delphi calls unary `not` LogicalNot even for integer bitwise complement; there is no separate BitwiseNot overload name. */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
	inline T o_logicalnot(T a) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		return static_cast<T>(~a);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_bitwiseand(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
		return static_cast<INTEGER_RESULT>(a) & static_cast<INTEGER_RESULT>(b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_bitwiseor(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
		return static_cast<INTEGER_RESULT>(a) | static_cast<INTEGER_RESULT>(b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_bitwisexor(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
		return static_cast<INTEGER_RESULT>(a) ^ static_cast<INTEGER_RESULT>(b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_unchecked_intdivide(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		return m_unchecked_intdivide(m_arithmetic_operand<INTEGER_RESULT>(a), m_arithmetic_operand<INTEGER_RESULT>(b));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_intdivide(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
		return m_checked_intdivide(m_arithmetic_operand<INTEGER_RESULT>(a), m_arithmetic_operand<INTEGER_RESULT>(b));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline INTEGER_RESULT o_modulus(T a, T b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
		return m_modulus(m_arithmetic_operand<INTEGER_RESULT>(a), m_arithmetic_operand<INTEGER_RESULT>(b));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	}

// Shift promotion differs from the other integer operations for unsigned
// Byte and Word. The count domain is independently and uniformly QWord, so
// keep that contract separate from INTEGER_RESULT.
#define TPCC_DEFINE_SHIFT_OPERATIONS(T, SHIFT_RESULT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
	inline SHIFT_RESULT o_leftshift(T a, t_qword b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
		return m_leftshift(m_arithmetic_operand<SHIFT_RESULT>(a), b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
	}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
	inline SHIFT_RESULT o_rightshift(T a, t_qword b) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
		return m_rightshift(m_arithmetic_operand<SHIFT_RESULT>(a), b);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	}

#define TPCC_DEFINE_INTEGRAL_OPERATIONS(T, INTEGER_RESULT)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
	TPCC_DEFINE_INTEGER_ARITHMETIC_OPERATIONS(T, INTEGER_RESULT, t_double)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
	TPCC_DEFINE_INTEGER_OPERATIONS(T, INTEGER_RESULT)

TPCC_DEFINE_INTEGRAL_OPERATIONS(t_byte, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_shortint, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_word, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_smallint, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_longword, t_longword)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_integer, t_integer)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_int64, t_int64)
TPCC_DEFINE_INTEGRAL_OPERATIONS(t_qword, t_qword)
TPCC_DEFINE_SHIFT_OPERATIONS(t_byte, t_longword)
TPCC_DEFINE_SHIFT_OPERATIONS(t_shortint, t_integer)
TPCC_DEFINE_SHIFT_OPERATIONS(t_word, t_longword)
TPCC_DEFINE_SHIFT_OPERATIONS(t_smallint, t_integer)
TPCC_DEFINE_SHIFT_OPERATIONS(t_longword, t_longword)
TPCC_DEFINE_SHIFT_OPERATIONS(t_integer, t_integer)
TPCC_DEFINE_SHIFT_OPERATIONS(t_int64, t_int64)
TPCC_DEFINE_SHIFT_OPERATIONS(t_qword, t_qword)
TPCC_DEFINE_REAL_ARITHMETIC_OPERATIONS(t_single)
TPCC_DEFINE_REAL_ARITHMETIC_OPERATIONS(t_double)
TPCC_DEFINE_REAL_ARITHMETIC_OPERATIONS(t_extended)

template <typename T>
        requires std::is_integral_v<T>
inline T o_power(T base, t_integer exponent) {
	if (exponent < 0) {
		m_runtime_error(201);
	}
	T result = 1;
	while (exponent != 0) {
		if ((exponent & 1) != 0) {
			T next;
			if (__builtin_mul_overflow(result, base, &next)) {
				m_runtime_error(215);
			}
			result = next;
		}
		exponent >>= 1;
		if (exponent != 0) {
			T next;
			if (__builtin_mul_overflow(base, base, &next)) {
				m_runtime_error(215);
			}
			base = next;
		}
	}
	return result;
}

inline t_extended o_power(t_extended base, t_extended exponent) {
	return ::powl(base, exponent);
}

// Floating-to-integer conversion is undefined in C++ when the finite value is
// outside the destination range (and for NaN/infinity). Check before casting
// so Pascal Trunc/Round never rely on C++ undefined behavior.
inline t_int64 tpcc_checked_real_to_int64(t_extended value) {
	constexpr t_extended limit = 0x1p63L;
	if (!__builtin_isfinite(value) || value < -limit || value >= limit) {
		m_runtime_error(201);
	}
	return static_cast<t_int64>(value);
}

inline t_int64 p_trunc(t_extended value) {
	return tpcc_checked_real_to_int64(::truncl(value));
}

inline t_int64 p_round(t_extended value) {
	// Pascal Round follows the active floating-point rounding mode. nearbyint
	// does likewise and therefore gives ties-to-even under the default mode.
	return tpcc_checked_real_to_int64(::nearbyintl(value));
}

inline t_extended p_frac(t_extended value) {
	t_extended integral = 0.0L;
	return ::modfl(value, &integral);
}

inline t_integer p_sqr(t_integer value) {
	return o_unchecked_multiply(value, value);
}

inline t_int64 p_sqr(t_int64 value) {
	return o_unchecked_multiply(value, value);
}

inline t_qword p_sqr(t_qword value) {
	return o_unchecked_multiply(value, value);
}

inline t_extended p_sqr(t_extended value) {
	return o_unchecked_multiply(value, value);
}

inline t_extended p_sqrt(t_extended value) {
	return ::sqrtl(value);
}

inline t_extended p_exp(t_extended value) {
	return ::expl(value);
}

inline t_extended p_ln(t_extended value) {
	return ::logl(value);
}

template <typename T> constexpr auto tpcc_for_ordinal_value(T value) {
	return tpcc_ordinal_storage<T>::get(value);
}

template <typename T> constexpr t_boolean tpcc_for_less_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) <= tpcc_for_ordinal_value(b));
}

template <typename T> constexpr t_boolean tpcc_for_greater_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) >= tpcc_for_ordinal_value(b));
}

template <typename T> constexpr t_boolean tpcc_for_equal(T a, T b) {
	return tpcc_bool_to_boolean(tpcc_for_ordinal_value(a) == tpcc_for_ordinal_value(b));
}

inline t_boolean o_logicalnot(t_boolean a) {
	return tpcc_bool_to_boolean(!a);
}

inline t_boolean o_logicalxor(t_boolean a, t_boolean b) {
	return tpcc_bool_to_boolean(((a != 0) ^ (b != 0)) != 0);
}

inline t_boolean o_implicit(t_boolean b, m_conversion_target<t_boolean>) {
	return b;
}

inline t_boolean p_assigned(const void* p) {
	return tpcc_bool_to_boolean(p != nullptr);
}

template <typename Signature> inline t_boolean p_assigned(m_proc<Signature> p) {
	return tpcc_bool_to_boolean(p != nullptr);
}

template <typename Signature> inline t_boolean p_assigned(const m_method<Signature>& p) {
	return tpcc_bool_to_boolean(p.p_code != nullptr);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T m_unchecked_ordinal_step(T value, t_integer amount, bool subtract) {
	using traits = tpcc_ordinal_storage<T>;
	using raw_type = typename traits::type;
	using unsigned_type = std::make_unsigned_t<raw_type>;
	unsigned_type bits = static_cast<unsigned_type>(traits::get(value));
	unsigned_type delta = static_cast<unsigned_type>(amount);
	unsigned_type stepped = subtract ? bits - delta : bits + delta;
	raw_type raw;
	if constexpr (std::is_signed_v<raw_type>) {
		raw = std::bit_cast<raw_type>(stepped);
	} else {
		raw = static_cast<raw_type>(stepped);
	}
	return traits::make(raw);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T m_checked_ordinal_step(T value, t_integer amount, bool subtract) {
	using traits = tpcc_ordinal_storage<T>;
	using raw_type = typename traits::type;
	raw_type result;
	const bool overflow = subtract ? __builtin_sub_overflow(traits::get(value), amount, &result) : __builtin_add_overflow(traits::get(value), amount, &result);
	if (overflow) {
		m_runtime_error(215);
	}
	return traits::make(result);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T m_unchecked_abs(T value) {
	using traits = tpcc_ordinal_storage<T>;
	using raw_type = typename traits::type;
	if constexpr (!std::is_signed_v<raw_type>) {
		return value;
	} else {
		const raw_type raw = traits::get(value);
		if (raw >= 0) {
			return value;
		}
		using unsigned_type = std::make_unsigned_t<raw_type>;
		// Negating Low(signed) in its signed carrier is C++ undefined
		// behavior. Unchecked Pascal Abs instead keeps the carrier's low
		// bits, so perform the negation modulo the corresponding unsigned
		// type and state the two's-complement result with bit_cast.
		const unsigned_type absolute = unsigned_type{0} - static_cast<unsigned_type>(raw);
		return traits::make(m_integer_from_bits<raw_type>(absolute));
	}
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T p_abs(T value) {
	using traits = tpcc_ordinal_storage<T>;
	using raw_type = typename traits::type;
	if constexpr (!std::is_signed_v<raw_type>) {
		return value;
	} else {
		const raw_type raw = traits::get(value);
		if (raw == std::numeric_limits<raw_type>::min()) {
			m_runtime_error(215);
		}
		return raw < 0 ? traits::make(static_cast<raw_type>(-raw)) : value;
	}
}

template <typename T>
        requires std::is_floating_point_v<T>
inline T m_unchecked_abs(T value) {
	return std::fabs(value);
}

template <typename T>
        requires std::is_floating_point_v<T>
inline T p_abs(T value) {
	// {$Q} is integer overflow checking. Both names remain necessary because
	// the call-site decision is made before the operand's exact predefined
	// numeric type is known; floating Abs simply has no overflow case.
	return std::fabs(value);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T m_unchecked_succ(T value) {
	return m_unchecked_ordinal_step(value, 1, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T p_succ(T value) {
	return m_checked_ordinal_step(value, 1, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T m_unchecked_pred(T value) {
	return m_unchecked_ordinal_step(value, 1, true);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T p_pred(T value) {
	return m_checked_ordinal_step(value, 1, true);
}

template <typename T, typename Amount>
        requires(!std::is_void_v<T> && std::is_integral_v<typename tpcc_ordinal_storage<Amount>::type>)
inline T* m_pointer_step(T* value, Amount amount, bool subtract) {
	// Pascal ^T stepping has the same element unit as C++ T* arithmetic.
	// Preserve the pointer itself: converting through an address integer would
	// discard C++ provenance and incorrectly define arithmetic on null or
	// unrelated storage. The Pascal program therefore inherits the C++ rule
	// that the result remains within the same array object or one-past it.
	const auto raw_amount = tpcc_ordinal_storage<Amount>::get(amount);
	return subtract ? value - raw_amount : value + raw_amount;
}

template <typename T>
        requires(!std::is_void_v<T>)
inline t_ptrint m_pointer_difference(T* first, T* second) {
	// C++ defines this only for pointers into the same array object (or
	// one-past). Its ptrdiff_t result is already measured in T elements,
	// exactly matching Pascal's typed-pointer subtraction contract.
	return static_cast<t_ptrint>(first - second);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_unchecked_inc(T value) {
	return m_unchecked_ordinal_step(value, 1, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_inc(T value) {
	return m_checked_ordinal_step(value, 1, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_unchecked_dec(T value) {
	return m_unchecked_ordinal_step(value, 1, true);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_dec(T value) {
	return m_checked_ordinal_step(value, 1, true);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_unchecked_add(T value, t_integer amount) {
	return m_unchecked_ordinal_step(value, amount, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_add(T value, t_integer amount) {
	return m_checked_ordinal_step(value, amount, false);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_unchecked_subtract(T value, t_integer amount) {
	return m_unchecked_ordinal_step(value, amount, true);
}

template <typename T>
        requires std::is_integral_v<typename tpcc_ordinal_storage<T>::type>
inline T o_subtract(T value, t_integer amount) {
	return m_checked_ordinal_step(value, amount, true);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline T* o_unchecked_inc(T* value) {
	return m_pointer_step(value, 1, false);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline T* o_inc(T* value) {
	return m_pointer_step(value, 1, false);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline T* o_unchecked_dec(T* value) {
	return m_pointer_step(value, 1, true);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline T* o_dec(T* value) {
	return m_pointer_step(value, 1, true);
}

template <typename T, typename Amount>
        requires(!std::is_void_v<T> && std::is_integral_v<typename tpcc_ordinal_storage<Amount>::type>)
inline T* o_unchecked_add(T* value, Amount amount) {
	return m_pointer_step(value, amount, false);
}

template <typename T, typename Amount>
        requires(!std::is_void_v<T> && std::is_integral_v<typename tpcc_ordinal_storage<Amount>::type>)
inline T* o_add(T* value, Amount amount) {
	return m_pointer_step(value, amount, false);
}

template <typename T, typename Amount>
        requires(!std::is_void_v<T> && std::is_integral_v<typename tpcc_ordinal_storage<Amount>::type>)
inline T* o_unchecked_subtract(T* value, Amount amount) {
	return m_pointer_step(value, amount, true);
}

template <typename T, typename Amount>
        requires(!std::is_void_v<T> && std::is_integral_v<typename tpcc_ordinal_storage<Amount>::type>)
inline T* o_subtract(T* value, Amount amount) {
	return m_pointer_step(value, amount, true);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline t_ptrint o_unchecked_subtract(T* first, T* second) {
	return m_pointer_difference(first, second);
}

template <typename T>
        requires(!std::is_void_v<T>)
inline t_ptrint o_subtract(T* first, T* second) {
	return m_pointer_difference(first, second);
}

template <typename T, std::size_t Capacity> inline void p_str(const tpcc_formatted_value<T>& argument, t_shortstring<Capacity>& destination) {
	// Bounded Str keeps the prefix which fits. Write instead calls the
	// AnsiString overload above, so its Text sink never truncates the field.
	// Rendering precedes every destination write, preserving a source which
	// aliases this string directly or through a PChar view.
	tpcc_rendered_formatted_value rendered = tpcc_render_formatted_value(argument);
	const std::size_t spaces = std::min<std::size_t>(rendered.left_padding, Capacity);
	const std::size_t copied = spaces == Capacity ? 0 : std::min<std::size_t>(rendered.value.size(), Capacity - spaces);
	memset(destination.data, ' ', spaces);
	if (copied != 0) {
		memcpy(destination.data + spaces, rendered.value.data(), copied);
	}
	destination.length = static_cast<uint8_t>(spaces + copied);
}

// Convenience entry points retain the direct RTL surface while delegating
// every formatting decision to the same representation used by Write.
template <typename T, std::size_t Capacity>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
inline void p_str(T value, t_shortstring<Capacity>& destination) {
	p_str(tpcc_make_formatted_value(value), destination);
}

template <typename T, std::size_t Capacity>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
inline void p_str(T value, tpcc_typed_storage_ref<t_shortstring<Capacity>> destination) {
	p_str(value, *destination.value);
}

template <typename T, std::size_t Capacity>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
inline void p_str(T value, t_sizeint width, t_shortstring<Capacity>& destination) {
	p_str(tpcc_make_formatted_value(value, width), destination);
}

struct tpcc_val_prefix {
	std::size_t position;
	unsigned base;
	bool negative;
};

template <typename T> inline constexpr bool tpcc_val_source_v = tpcc_is_shortstring_v<T> || std::is_same_v<std::remove_cv_t<T>, t_ansistring>;

template <typename Source>
        requires tpcc_val_source_v<Source>
inline tpcc_val_prefix tpcc_val_parse_prefix(const Source& source, t_integer& code) {
	const std::size_t length = static_cast<std::size_t>(source.m_length());
	const t_char* data = source.m_data();
	std::size_t position = 0;
	while (position < length && (data[position].value == ' ' || data[position].value == '\t')) {
		++position;
	}

	bool negative = false;
	if (position < length && (data[position].value == '+' || data[position].value == '-')) {
		negative = data[position].value == '-';
		++position;
	}

	unsigned base = 10;
	if (position < length) {
		switch (data[position].value) {
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
			if (position + 1 < length && (data[position + 1].value == 'x' || data[position + 1].value == 'X')) {
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
	if (character >= '0' && character <= '9') {
		return character - '0';
	}
	if (character >= 'A' && character <= 'F') {
		return character - 'A' + 10;
	}
	if (character >= 'a' && character <= 'f') {
		return character - 'a' + 10;
	}
	return 16;
}

template <typename Source, typename T>
        requires tpcc_val_source_v<Source> && std::is_integral_v<typename tpcc_ordinal_storage<T>::type> && (!std::is_same_v<typename tpcc_ordinal_storage<T>::type, bool>)
inline void p_val(const Source& source, T& destination, t_integer& code) {
	using traits = tpcc_ordinal_storage<T>;
	using storage_type = typename traits::type;
	destination = traits::make(storage_type{0});
	tpcc_val_prefix prefix = tpcc_val_parse_prefix(source, code);
	const std::size_t length = static_cast<std::size_t>(source.m_length());
	const t_char* data = source.m_data();
	std::size_t position = prefix.position;
	if (position >= length) {
		return;
	}

	using unsigned_type = std::make_unsigned_t<storage_type>;
	constexpr unsigned_type unsigned_max = std::numeric_limits<unsigned_type>::max();
	unsigned_type limit = unsigned_max;
	if constexpr (std::is_signed_v<storage_type>) {
		if (prefix.base == 10 || prefix.negative) {
			const unsigned_type signed_max = static_cast<unsigned_type>(std::numeric_limits<storage_type>::max());
			limit = prefix.negative ? signed_max + 1 : signed_max;
		}
	} else if (prefix.negative) {
		return;
	}

	unsigned_type magnitude = 0;
	bool saw_digit = false;
	for (; position < length; ++position) {
		const uint8_t character = data[position].value;
		if (character == 0) {
			break;
		}
		const unsigned digit = tpcc_val_digit(character);
		code = static_cast<t_integer>(position + 1);
		if (digit >= prefix.base) {
			return;
		}
		const unsigned_type typed_digit = static_cast<unsigned_type>(digit);
		if (magnitude > (limit - typed_digit) / prefix.base) {
			return;
		}
		magnitude = static_cast<unsigned_type>(magnitude * static_cast<unsigned_type>(prefix.base) + typed_digit);
		saw_digit = true;
	}
	if (!saw_digit) {
		return;
	}

	storage_type parsed{};
	if constexpr (std::is_signed_v<storage_type>) {
		if (prefix.negative) {
			const unsigned_type minimum_magnitude = static_cast<unsigned_type>(std::numeric_limits<storage_type>::max()) + 1;
			if (magnitude == minimum_magnitude) {
				parsed = std::numeric_limits<storage_type>::min();
			} else {
				parsed = static_cast<storage_type>(-static_cast<storage_type>(magnitude));
			}
		} else if (prefix.base != 10) {
			parsed = std::bit_cast<storage_type>(magnitude);
		} else {
			parsed = static_cast<storage_type>(magnitude);
		}
	} else {
		parsed = static_cast<storage_type>(magnitude);
	}
	destination = traits::make(parsed);
	code = 0;
}

template <typename Source, typename T>
        requires tpcc_val_source_v<Source> && std::is_floating_point_v<T>
inline void p_val(const Source& source, T& destination, t_integer& code) {
	destination = 0;
	const std::size_t length = static_cast<std::size_t>(source.m_length());
	const t_char* data = source.m_data();
	std::size_t position = 0;
	while (position < length && (data[position].value == ' ' || data[position].value == '\t')) {
		++position;
	}

	if (position < length && data[position].value == '+') {
		++position;
	}
	std::string text;
	text.reserve(length - position);
	for (std::size_t i = position; i < length; ++i) {
		if (data[i].value == 0) {
			break;
		}
		text.push_back(static_cast<char>(data[i].value));
	}
	code = static_cast<t_integer>(position + 1);
	if (text.empty()) {
		return;
	}
	const std::size_t first_digit = text[0] == '-' ? 1 : 0;
	if (first_digit >= text.size() || !((text[first_digit] >= '0' && text[first_digit] <= '9') || text[first_digit] == '.')) {
		return;
	}

	T parsed = 0;
	auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::general);
	code = static_cast<t_integer>(position + (end - text.data()) + 1);
	if (error != std::errc() || end != text.data() + text.size()) {
		return;
	}
	destination = parsed;
	code = 0;
}

template <typename Source, typename T, typename Code>
        requires tpcc_val_source_v<Source> && ((std::is_integral_v<typename tpcc_ordinal_storage<T>::type> && (!std::is_same_v<typename tpcc_ordinal_storage<T>::type, bool>)) || std::is_floating_point_v<T>) && std::is_integral_v<typename tpcc_ordinal_storage<Code>::type> && (!std::is_same_v<typename tpcc_ordinal_storage<Code>::type, bool>)
inline void p_val(const Source& source, T& destination, tpcc_typed_storage_ref<Code> code) {
	using code_traits = tpcc_ordinal_storage<Code>;
	using code_storage = typename code_traits::type;
	t_integer parsed_code = 0;
	p_val(source, destination, parsed_code);
	*code.value = code_traits::make(static_cast<code_storage>(parsed_code));
}

template <typename Source, typename T>
        requires tpcc_val_source_v<Source> && ((std::is_integral_v<typename tpcc_ordinal_storage<T>::type> && (!std::is_same_v<typename tpcc_ordinal_storage<T>::type, bool>)) || std::is_floating_point_v<T>)
inline void p_val(const Source& source, T& destination) {
	t_integer code = 0;
	p_val(source, destination, code);
}

inline t_char tpcc_hex_digit(uint8_t value) {
	return t_char{static_cast<uint8_t>(value < 10 ? static_cast<uint8_t>('0') + value : static_cast<uint8_t>('A') + value - 10)};
}

template <typename T>
        requires std::is_integral_v<T>
inline t_shortstring<255> tpcc_hexstr_bits(T value, t_byte count) {
	using unsigned_type = std::make_unsigned_t<T>;
	// HexStr exposes the source carrier's bits, so signed inputs first become
	// the corresponding unsigned bit pattern.  Logical shifts then discard
	// left-hand nibbles and, when count exceeds the carrier width, zero-fill.
	unsigned_type bits;
	if constexpr (std::is_signed_v<T>) {
		bits = std::bit_cast<unsigned_type>(value);
	} else {
		bits = value;
	}

	t_shortstring<255> result{};
	result.length = t_char{count};
	for (std::size_t i = count; i != 0; --i) {
		result.data[i - 1] = tpcc_hex_digit(static_cast<uint8_t>(bits & unsigned_type{15}));
		bits >>= 4;
	}
	return result;
}

inline t_shortstring<255> p_hexstr(t_longint value, t_byte count) {
	return tpcc_hexstr_bits(value, count);
}

inline t_shortstring<255> p_hexstr(t_int64 value, t_byte count) {
	return tpcc_hexstr_bits(value, count);
}

inline t_shortstring<255> p_hexstr(t_qword value, t_byte count) {
	return tpcc_hexstr_bits(value, count);
}

inline t_shortstring<255> p_hexstr(t_pointer value) {
	static_assert(sizeof(t_pointer) * 2 <= 255, "a complete pointer must fit in ShortString");
	// Unlike the counted overloads, Pascal defines the pointer form as a
	// complete representation of the target pointer carrier.
	constexpr t_byte count = static_cast<t_byte>(sizeof(t_pointer) * 2);
	return tpcc_hexstr_bits(reinterpret_cast<std::uintptr_t>(value), count);
}

template <typename T>
        requires std::is_signed_v<T> && std::is_integral_v<T>
inline t_shortstring<255> tpcc_octstr_signed(T value, t_byte count) {
	using unsigned_type = std::make_unsigned_t<T>;
	constexpr unsigned width = std::numeric_limits<unsigned_type>::digits;
	unsigned_type bits = std::bit_cast<unsigned_type>(value);
	const bool negative = value < 0;

	t_shortstring<255> result{};
	result.length = t_char{count};
	for (std::size_t i = count; i != 0; --i) {
		result.data[i - 1] = t_char{static_cast<uint8_t>('0' + (bits & unsigned_type{7}))};
		bits >>= 3;
		if (negative) {
			bits |= static_cast<unsigned_type>(~unsigned_type{0} << (width - 3));
		}
	}
	return result;
}

inline t_shortstring<255> p_octstr(t_longint value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring<255> p_octstr(t_int64 value, t_byte count) {
	return tpcc_octstr_signed(value, count);
}

inline t_shortstring<255> p_octstr(t_qword value, t_byte count) {
	// FPC's QWord overload delegates through Int64, preserving the QWord bit
	// pattern and therefore sign-extending values whose top bit is set.
	return tpcc_octstr_signed(std::bit_cast<t_int64>(value), count);
}

inline t_sizeint p_strlen(const t_char* value) {
	if (!value) {
		return 0;
	}
	t_sizeint length = 0;
	while (value[length].value != 0) {
		++length;
	}
	return length;
}

template <typename Size>
        requires std::is_integral_v<Size> && (!std::is_same_v<std::remove_cv_t<Size>, bool>)
inline std::size_t m_allocation_size(Size size) {
	if constexpr (std::is_signed_v<Size>) {
		if (size < 0) {
			m_runtime_error(203);
		}
	}
	using Unsigned = std::make_unsigned_t<Size>;
	const Unsigned unsigned_size = static_cast<Unsigned>(size);
	if constexpr (sizeof(Unsigned) > sizeof(std::size_t)) {
		if (unsigned_size > static_cast<Unsigned>(std::numeric_limits<std::size_t>::max())) {
			m_runtime_error(203);
		}
	}
	return static_cast<std::size_t>(unsigned_size);
}

template <typename T, typename Size>
        requires(std::is_object_v<T> || std::is_void_v<T>) && std::is_integral_v<Size>
inline void p_getmem(T*& destination, Size size) {
	const std::size_t requested = m_allocation_size(size);
	destination = static_cast<T*>(std::malloc(requested));
	if (!destination && requested != 0) {
		m_runtime_error(203);
	}
}

template <typename Size>
        requires std::is_integral_v<Size>
inline t_pointer p_getmem(Size size) {
	const std::size_t requested = m_allocation_size(size);
	t_pointer result = std::malloc(requested);
	if (!result && requested != 0) {
		m_runtime_error(203);
	}
	return result;
}

template <typename Size>
        requires std::is_integral_v<Size>
inline t_pointer p_allocmem(Size size) {
	const std::size_t requested = m_allocation_size(size);
	t_pointer result = std::calloc(1, requested);
	if (!result && requested != 0) {
		m_runtime_error(203);
	}
	return result;
}

template <typename T, typename Size>
        requires(std::is_object_v<T> || std::is_void_v<T>) && std::is_integral_v<Size>
inline T* p_reallocmem(T*& destination, Size size) {
	const std::size_t requested = m_allocation_size(size);
	if (requested == 0) {
		// C and C++ leave realloc(p, 0) implementation-dependent. Pascal's
		// storage operation needs one stable postcondition: release the old
		// allocation, clear the var parameter, and return that same nil value.
		std::free(destination);
		destination = nullptr;
		return nullptr;
	}

	// ReAllocMem belongs to the same malloc/free allocation family as GetMem.
	// Do not overwrite DESTINATION until realloc succeeds: on allocation
	// failure realloc leaves the old block live, and the runtime-error path may
	// inspect or unwind past this frame.
	void* replacement = std::realloc(destination, requested);
	if (!replacement) {
		m_runtime_error(203);
	}
	destination = static_cast<T*>(replacement);
	return destination;
}

template <typename Size>
        requires std::is_integral_v<Size>
inline void p_freemem(t_pointer value, Size size) {
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
template <typename T> inline T* m_allocate_object() {
	if constexpr (std::is_abstract_v<T>) {
		return nullptr;
	} else {
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
template <typename Object> inline Object* m_new_instance(m_classref<Object>* meta) {
	return static_cast<typename Object::m_meta*>(meta)->m_allocate();
}

// Pascal Free releases the instance through the virtual FreeInstance hook.
// TObject.FreeInstance (emitted as p_freeinstance) ends in "delete this", so a
// plain object is freed here; an override (for example a refcounted TSymtable)
// may defer or veto the storage release. Nil-safe: the receiver is tested
// before dispatch.
template <typename Object> inline void m_free_object(Object* object) {
	if (!object) {
		return;
	}
	object->p_freeinstance();
}

// Plain Pascal New allocates the exact pointed-to carrier. Deliberately omit
// braces: old-style object and record scalar storage is not generally
// zero-initialized. Native C++ default initialization still constructs
// managed carrier members and, for polymorphic objects, installs the exact
// vptr before any Pascal initializer method runs.
template <typename T> inline T* m_new_value() {
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
template <typename Initializer> inline void m_invoke_initializer(Initializer&& initializer) {
	try {
		std::forward<Initializer>(initializer)();
	} catch (const tpcc_constructor_fail&) {
	}
}

// Old-style object New owns allocation but not the initializer declaration.
// T is the exact pointed-to object, while Initializer may name an inherited
// nonvirtual constructor. Applying that base member pointer to T preserves
// the already-installed most-derived C++ virtual dispatch inside its body.
template <typename T, auto Initializer, typename... Args> inline T* m_new_object(Args&&... args) {
	std::unique_ptr<T> object;
	try {
		object.reset(new T);
	} catch (const std::bad_alloc&) {
		m_runtime_error(203);
	}
	try {
		(object.get()->*Initializer)(std::forward<Args>(args)...);
	} catch (const tpcc_constructor_fail&) {
		return nullptr;
	}
	return object.release();
}

template <typename T> inline void m_dispose_value(T* object) {
	delete object;
}

// Pascal Done is an ordinary, possibly virtual method. The unique_ptr is
// armed before entering it so carrier/managed-field teardown still occurs if
// Done raises. Its hidden C++ virtual destructor contains no Pascal body and
// solely makes deletion through a VMT-bearing ancestor exact.
template <auto Finalizer, typename T> inline void m_dispose_object(T* object) {
	if (!object) {
		return;
	}
	std::unique_ptr<T> storage(object);
	(object->*Finalizer)();
}

// Construct is the one allocation boundary. Initializer remains an ordinary
// Unit-returning object method, so inherited and virtual constructor bodies
// use the already allocated most-derived C++ object. The pointer-to-member
// template argument is the declaration selected by Pascal overload
// resolution; C++ is not asked to select the overload again.
template <typename Result, auto Initializer, typename Meta, typename... Args> inline Result* m_construct(Meta* meta, Args&&... args) {
	auto* object = static_cast<Result*>(meta->p_newinstance());
	if (!object) {
		m_runtime_error(203);
	}
	try {
		(object->*Initializer)(std::forward<Args>(args)...);
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

// #define class_instance_new(X) (new X)

} // namespace u_system
