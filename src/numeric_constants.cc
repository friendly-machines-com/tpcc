#include "numeric_constants.h"

#include "builtins.h"
#include "types.h"

#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <system_error>
#include <type_traits>

static_assert(std::numeric_limits<float>::radix == 2 && std::numeric_limits<float>::digits == 24 && std::numeric_limits<float>::max_exponent == 128, "TPCC bootstrap requires IEEE binary32 float");
static_assert(std::numeric_limits<double>::radix == 2 && std::numeric_limits<double>::digits == 53 && std::numeric_limits<double>::max_exponent == 1024, "TPCC bootstrap requires IEEE binary64 double");
static_assert(std::numeric_limits<long double>::radix == 2 && std::numeric_limits<long double>::digits >= 64 && std::numeric_limits<long double>::max_exponent >= 16384, "TPCC bootstrap long double must losslessly contain Pascal binary80");

namespace {

bool checked_add_int64(int64_t a, int64_t b, int64_t* result) {
	if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
		return false;
	}
	*result = a + b;
	return true;
}

unsigned decimal_divide_small(std::string& digits, unsigned divisor) {
	unsigned remainder = 0;
	for (char& ch : digits) {
		unsigned value = remainder * 10 + static_cast<unsigned>(ch - '0');
		ch = static_cast<char>('0' + value / divisor);
		remainder = value % divisor;
	}
	size_t first = digits.find_first_not_of('0');
	if (first == std::string::npos) {
		digits = "0";
	} else if (first != 0) {
		digits.erase(0, first);
	}
	return remainder;
}

bool decimal_to_u64(const std::string& digits, uint64_t* value) {
	uint64_t result = 0;
	for (char ch : digits) {
		unsigned digit = static_cast<unsigned>(ch - '0');
		if (result > (UINT64_MAX - digit) / 10) {
			return false;
		}
		result = result * 10 + digit;
	}
	*value = result;
	return true;
}

unsigned bit_width_u64(uint64_t value) {
	unsigned result = 0;
	while (value != 0) {
		++result;
		value >>= 1;
	}
	return result;
}

struct BinaryFormat {
	unsigned precision;
	int minimum_subnormal_exponent;
	int maximum_normal_exponent;
};

// Describe Pascal value domains, not whichever floating formats the compiler
// host happens to use. The host types below are only lossless working
// containers; System declarations decide which of these domains are visible.
std::optional<BinaryFormat> binary_format(const Type* type) {
	type = distinct_storage_type(type);
	if (type == single_type()) {
		return BinaryFormat{
		    static_cast<unsigned>(std::numeric_limits<float>::digits),
		    std::numeric_limits<float>::min_exponent - std::numeric_limits<float>::digits,
		    std::numeric_limits<float>::max_exponent - 1,
		};
	}
	if (type == double_type()) {
		return BinaryFormat{
		    static_cast<unsigned>(std::numeric_limits<double>::digits),
		    std::numeric_limits<double>::min_exponent - std::numeric_limits<double>::digits,
		    std::numeric_limits<double>::max_exponent - 1,
		};
	}
	if (type == extended_type()) {
		// Pascal Extended is the architecture-neutral x87 binary80 value
		// domain. A System unit which wants another source-visible meaning can
		// alias or omit this intrinsic; compiler-host long double never changes
		// the domain described here.
		return BinaryFormat{64, -16445, 16383};
	}
	return std::nullopt;
}

long double round_to_binary_format(long double value, const BinaryFormat& format) {
	if (value == 0.0L || !__builtin_isfinite(value)) {
		return value;
	}
	const bool negative = __builtin_signbit(value);
	long double magnitude = negative ? -value : value;
	int exponent = 0;
	(void)::frexpl(magnitude, &exponent);
	if (exponent - 1 > format.maximum_normal_exponent) {
		const long double infinity = std::numeric_limits<long double>::infinity();
		return negative ? -infinity : infinity;
	}
	const int quantum_exponent = std::max(exponent - static_cast<int>(format.precision), format.minimum_subnormal_exponent);
	// At this scale, adjacent integers are adjacent representable values in the
	// destination format. nearbyintl therefore supplies the one required
	// rounding step, including subnormals. scalbnl is deliberately used for the
	// exact power-of-two scaling: on one bootstrap libc, the ldexpl symbol path
	// returned a binary64-rounded significand even for long-double arguments.
	const long double scaled = ::scalbnl(magnitude, -quantum_exponent);
	const long double rounded = ::nearbyintl(scaled);
	long double result = ::scalbnl(rounded, quantum_exponent);
	int rounded_exponent = 0;
	if (result != 0.0L) {
		(void)::frexpl(result, &rounded_exponent);
		if (rounded_exponent - 1 > format.maximum_normal_exponent) {
			result = std::numeric_limits<long double>::infinity();
		}
	}
	return negative ? -result : result;
}

bool decimal_is_exact_binary(const DecimalOrigin& origin, const BinaryFormat& format) {
	if (origin.is_zero()) {
		return true;
	}

	std::string numerator = origin.digits;
	int64_t binary_exponent = 0;

	// A finite decimal is exactly binary only after every denominator factor 5
	// has cancelled. What remains is an integer times a power of two; its bit
	// width and exponent can then be checked without approximate arithmetic or
	// a multiprecision dependency.
	if (origin.exponent10 >= 0) {
		while (numerator != "0" && ((numerator.back() - '0') & 1) == 0) {
			decimal_divide_small(numerator, 2);
			if (binary_exponent == INT64_MAX) {
				return false;
			}
			++binary_exponent;
		}

		uint64_t odd_significand = 0;
		if (!decimal_to_u64(numerator, &odd_significand)) {
			return false;
		}
		for (int64_t i = 0; i < origin.exponent10; ++i) {
			if (odd_significand > UINT64_MAX / 5) {
				return false;
			}
			odd_significand *= 5;
			if (bit_width_u64(odd_significand) > format.precision) {
				return false;
			}
		}
		if (!checked_add_int64(binary_exponent, origin.exponent10, &binary_exponent)) {
			return false;
		}
		numerator = std::to_string(odd_significand);
	} else {
		if (origin.exponent10 == INT64_MIN) {
			return false;
		}
		const int64_t denominator_power = -origin.exponent10;
		for (int64_t i = 0; i < denominator_power; ++i) {
			if (decimal_divide_small(numerator, 5) != 0) {
				return false;
			}
		}
		binary_exponent = -denominator_power;
		while (numerator != "0" && ((numerator.back() - '0') & 1) == 0) {
			decimal_divide_small(numerator, 2);
			if (binary_exponent == INT64_MAX) {
				return false;
			}
			++binary_exponent;
		}
	}

	uint64_t significand = 0;
	if (!decimal_to_u64(numerator, &significand)) {
		return false;
	}
	const unsigned bits = bit_width_u64(significand);
	if (bits == 0 || bits > format.precision) {
		return false;
	}
	const __int128 highest_exponent = static_cast<__int128>(binary_exponent) + bits - 1;
	return binary_exponent >= format.minimum_subnormal_exponent && highest_exponent <= format.maximum_normal_exponent;
}

template <typename T> RealMaterialization parse_as(const DecimalOrigin& origin, Type* target) {
	const std::string text = decimal_origin_text(origin);
	T value = 0;
	if constexpr (std::is_same_v<T, long double>) {
		// libstdc++ versions used to bootstrap TPCC have implemented
		// from_chars(long double) through binary64 on some hosts. strtold in
		// the process's initial C locale is the standard no-dependency direct
		// conversion and preserves the actual long-double carrier.
		char* end = nullptr;
		errno = 0;
		value = ::strtold(text.c_str(), &end);
		if (errno == ERANGE && !__builtin_isfinite(value)) {
			const long double infinity = std::numeric_limits<long double>::infinity();
			return RealMaterialization{RealMaterializationKind::OutOfRange, origin.negative ? -infinity : infinity};
		}
		// ERANGE with a finite result is underflow, not overflow. Keep the
		// returned subnormal or signed zero and let the Pascal-format rounding
		// below classify/materialize it normally.
		if (!end || end != text.c_str() + text.size()) {
			return RealMaterialization{RealMaterializationKind::InvalidTarget, 0.0L};
		}
	} else {
		auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
		if (ec == std::errc::result_out_of_range) {
			// Every supported binary format has values both below 1 and above
			// 1. Thus the exact decimal's scientific exponent unambiguously
			// distinguishes an underflow report from an overflow report.
			const __int128 scientific_exponent = static_cast<__int128>(origin.exponent10) + static_cast<__int128>(origin.digits.size()) - 1;
			if (scientific_exponent < 0) {
				value = origin.negative ? -T{0} : T{0};
			} else {
				const long double infinity = static_cast<long double>(std::numeric_limits<T>::infinity());
				return RealMaterialization{RealMaterializationKind::OutOfRange, origin.negative ? -infinity : infinity};
			}
		}
		if ((ec != std::errc() && ec != std::errc::result_out_of_range) || end != text.data() + text.size()) {
			return RealMaterialization{RealMaterializationKind::InvalidTarget, 0.0L};
		}
	}
	auto format = binary_format(target);
	if (!format) {
		return RealMaterialization{RealMaterializationKind::InvalidTarget, 0.0L};
	}
	// Parsing supplies the nearest host-carrier value. Exactness is classified
	// independently from the normalized decimal origin, so a lucky rounded
	// host value can never be mistaken for an exact Pascal materialization.
	const long double rounded = round_to_binary_format(static_cast<long double>(value), *format);
	return RealMaterialization{
	    decimal_is_exact_binary(origin, *format) ? RealMaterializationKind::Exact : RealMaterializationKind::Rounded,
	    rounded,
	};
}

template <typename T> long double eval_binary(RealBinaryOperation operation, long double left, long double right) {
	// The CST's long double fields are containers, not an instruction to do all
	// arithmetic in Extended. Re-enter the selected Pascal carrier before the
	// operation so folding has the same operand and result boundaries as the
	// selected runtime equation.
	const T a = static_cast<T>(left);
	const T b = static_cast<T>(right);
	T result = 0;
	switch (operation) {
	case RealBinaryOperation::Add:
		result = static_cast<T>(a + b);
		break;
	case RealBinaryOperation::Subtract:
		result = static_cast<T>(a - b);
		break;
	case RealBinaryOperation::Multiply:
		result = static_cast<T>(a * b);
		break;
	case RealBinaryOperation::Divide:
		result = static_cast<T>(a / b);
		break;
	}
	return static_cast<long double>(result);
}

} // namespace

std::optional<DecimalOrigin> parse_decimal_origin(std::string_view spelling, std::string* error) {
	auto fail = [&](std::string message) -> std::optional<DecimalOrigin> {
		if (error) {
			*error = std::move(message);
		}
		return std::nullopt;
	};

	DecimalOrigin result;
	size_t position = 0;
	if (position < spelling.size() && (spelling[position] == '+' || spelling[position] == '-')) {
		result.negative = spelling[position] == '-';
		++position;
	}
	if (position == spelling.size()) {
		return fail("missing decimal digits");
	}

	size_t exponent_position = spelling.find_first_of("eE", position);
	std::string_view significand = spelling.substr(position, exponent_position == std::string_view::npos ? spelling.size() - position : exponent_position - position);
	int64_t explicit_exponent = 0;
	if (exponent_position != std::string_view::npos) {
		std::string_view exponent = spelling.substr(exponent_position + 1);
		if (exponent.empty()) {
			return fail("missing decimal exponent");
		}
		if (exponent.front() == '+') {
			exponent.remove_prefix(1);
			if (exponent.empty()) {
				return fail("missing decimal exponent");
			}
		}
		auto [end, ec] = std::from_chars(exponent.data(), exponent.data() + exponent.size(), explicit_exponent);
		if (ec != std::errc() || end != exponent.data() + exponent.size()) {
			return fail("decimal exponent is out of range");
		}
	}

	bool saw_digit = false;
	bool saw_point = false;
	int64_t fractional_digits = 0;
	std::string digits;
	for (char ch : significand) {
		if (ch == '.') {
			if (saw_point) {
				return fail("more than one decimal point");
			}
			saw_point = true;
			continue;
		}
		if (ch < '0' || ch > '9') {
			return fail("invalid decimal digit");
		}
		saw_digit = true;
		digits.push_back(ch);
		if (saw_point) {
			if (fractional_digits == INT64_MAX) {
				return fail("decimal literal is too long");
			}
			++fractional_digits;
		}
	}
	if (!saw_digit) {
		return fail("missing decimal digits");
	}
	if (!checked_add_int64(explicit_exponent, -fractional_digits, &result.exponent10)) {
		return fail("decimal exponent is out of range");
	}

	size_t first = digits.find_first_not_of('0');
	if (first == std::string::npos) {
		result.digits = "0";
		result.exponent10 = 0;
		return result;
	}
	digits.erase(0, first);
	while (digits.size() > 1 && digits.back() == '0') {
		digits.pop_back();
		if (result.exponent10 == INT64_MAX) {
			return fail("decimal exponent is out of range");
		}
		++result.exponent10;
	}
	result.digits = std::move(digits);
	return result;
}

std::string decimal_origin_text(const DecimalOrigin& origin) {
	std::string result;
	if (origin.negative) {
		result.push_back('-');
	}
	result += origin.digits;
	if (!origin.is_zero() && origin.exponent10 != 0) {
		result.push_back('e');
		result += std::to_string(origin.exponent10);
	}
	return result;
}

int real_semantic_rank(const Type* type) {
	type = distinct_storage_type(type);
	if (type == single_type()) {
		return 0;
	}
	if (type == double_type()) {
		return 1;
	}
	if (type == extended_type()) {
		return 2;
	}
	return -1;
}

bool is_real_semantic_type(const Type* type) {
	return real_semantic_rank(type) >= 0;
}

bool integer_domain_is_exact_in_real(const Type* integer_type, const Type* real_type) {
	OrdinalBounds bounds;
	auto format = binary_format(real_type);
	if (!format || !integer_bounds(integer_type, &bounds)) {
		return false;
	}
	uint64_t largest_magnitude = bounds.max_positive;
	if (bounds.signed_type) {
		largest_magnitude = std::max(largest_magnitude, bounds.min_magnitude);
	}
	return bit_width_u64(largest_magnitude) <= format->precision;
}

RealMaterialization materialize_decimal_origin(const DecimalOrigin& origin, Type* target) {
	Type* storage = distinct_storage_type(target);
	if (storage == single_type()) {
		return parse_as<float>(origin, storage);
	}
	if (storage == double_type()) {
		return parse_as<double>(origin, storage);
	}
	if (storage == extended_type()) {
		return parse_as<long double>(origin, storage);
	}
	return RealMaterialization{RealMaterializationKind::InvalidTarget, 0.0L};
}

namespace {
static uint64_t currency_raw_limit(bool negative) {
	return negative ? uint64_t{1} << 63 : static_cast<uint64_t>(INT64_MAX);
}

static bool append_decimal_digit(uint64_t* value, unsigned digit, uint64_t limit) {
	if (*value > (limit - digit) / 10) {
		return false;
	}
	*value = *value * 10 + digit;
	return true;
}

static int compare_decimal_remainder_with_half(std::string_view remainder) {
	if (remainder.empty()) {
		return -1;
	}
	if (remainder.front() < '5') {
		return -1;
	}
	if (remainder.front() > '5') {
		return 1;
	}
	for (char digit : remainder.substr(1)) {
		if (digit != '0') {
			return 1;
		}
	}
	return 0;
}

static int64_t signed_currency_raw(uint64_t magnitude, bool negative) {
	if (!negative) {
		return static_cast<int64_t>(magnitude);
	}
	if (magnitude == (uint64_t{1} << 63)) {
		return INT64_MIN;
	}
	return -static_cast<int64_t>(magnitude);
}

static int64_t wrapped_currency_raw(uint64_t magnitude_modulo_2_64, bool negative) {
	const uint64_t bits = negative ? uint64_t{0} - magnitude_modulo_2_64 : magnitude_modulo_2_64;
	return std::bit_cast<int64_t>(bits);
}
} // namespace

CurrencyMaterialization materialize_currency_origin(const DecimalOrigin& origin) {
	if (origin.is_zero()) {
		// Currency has one zero representation, unlike IEEE binary reals.
		return CurrencyMaterialization{RealMaterializationKind::Exact, 0};
	}

	const uint64_t limit = currency_raw_limit(origin.negative);
	if (origin.exponent10 > INT64_MAX - 4) {
		// 10^64 and every greater decimal power are divisible by 2^64.
		// The unchecked scaled result therefore has zero low bits.
		return CurrencyMaterialization{RealMaterializationKind::OutOfRange, 0};
	}
	if (origin.exponent10 < INT64_MIN + 4) {
		return CurrencyMaterialization{RealMaterializationKind::Rounded, 0};
	}
	const int64_t scaled_exponent = origin.exponent10 + 4;

	if (scaled_exponent >= 0) {
		uint64_t wrapped_magnitude = 0;
		for (char digit : origin.digits) {
			wrapped_magnitude = wrapped_magnitude * 10 + static_cast<unsigned>(digit - '0');
		}
		if (scaled_exponent >= 64) {
			wrapped_magnitude = 0;
		} else {
			for (int64_t i = 0; i < scaled_exponent; ++i) {
				wrapped_magnitude *= 10;
			}
		}
		const int64_t unchecked = wrapped_currency_raw(wrapped_magnitude, origin.negative);

		// A nonzero in-range Currency raw value has at most 19 decimal digits.
		// Establish that bound before the checked accumulation so a huge source
		// exponent never causes an exponent-sized loop.
		if (scaled_exponent > 19 || origin.digits.size() > 19 - static_cast<size_t>(scaled_exponent)) {
			return CurrencyMaterialization{RealMaterializationKind::OutOfRange, unchecked};
		}
		uint64_t magnitude = 0;
		for (char digit : origin.digits) {
			if (!append_decimal_digit(&magnitude, static_cast<unsigned>(digit - '0'), limit)) {
				return CurrencyMaterialization{RealMaterializationKind::OutOfRange, unchecked};
			}
		}
		for (int64_t i = 0; i < scaled_exponent; ++i) {
			if (!append_decimal_digit(&magnitude, 0, limit)) {
				return CurrencyMaterialization{RealMaterializationKind::OutOfRange, unchecked};
			}
		}
		return CurrencyMaterialization{
		    RealMaterializationKind::Exact,
		    signed_currency_raw(magnitude, origin.negative),
		};
	}

	const uint64_t discarded_digits = scaled_exponent == INT64_MIN ? UINT64_MAX : static_cast<uint64_t>(-scaled_exponent);
	const size_t digit_count = origin.digits.size();
	size_t quotient_digits = 0;
	if (discarded_digits < digit_count) {
		quotient_digits = digit_count - static_cast<size_t>(discarded_digits);
	}
	uint64_t wrapped_magnitude = 0;
	uint64_t magnitude = 0;
	bool in_range = quotient_digits <= 19;
	for (size_t i = 0; i < quotient_digits; ++i) {
		const unsigned digit = static_cast<unsigned>(origin.digits[i] - '0');
		wrapped_magnitude = wrapped_magnitude * 10 + digit;
		if (in_range && !append_decimal_digit(&magnitude, digit, limit)) {
			in_range = false;
		}
	}

	bool remainder_nonzero = false;
	int half_comparison = -1;
	if (discarded_digits > digit_count) {
		// At least one leading zero exists after the decimal point, so the
		// discarded fraction is strictly below one half.
		remainder_nonzero = true;
	} else {
		const size_t remainder_start = quotient_digits;
		std::string_view remainder(origin.digits.data() + remainder_start, digit_count - remainder_start);
		for (char digit : remainder) {
			remainder_nonzero = remainder_nonzero || digit != '0';
		}
		half_comparison = compare_decimal_remainder_with_half(remainder);
	}
	if (half_comparison > 0 || (half_comparison == 0 && (wrapped_magnitude & 1) != 0)) {
		++wrapped_magnitude;
		if (in_range) {
			if (magnitude == limit) {
				in_range = false;
			} else {
				++magnitude;
			}
		}
	}
	if (!in_range) {
		return CurrencyMaterialization{
		    RealMaterializationKind::OutOfRange,
		    wrapped_currency_raw(wrapped_magnitude, origin.negative),
		};
	}
	return CurrencyMaterialization{
	    remainder_nonzero ? RealMaterializationKind::Rounded : RealMaterializationKind::Exact,
	    signed_currency_raw(magnitude, origin.negative),
	};
}

CurrencyMaterialization materialize_currency_integer(uint64_t magnitude, bool negative) {
	DecimalOrigin origin;
	origin.negative = negative;
	origin.digits = std::to_string(magnitude);
	origin.exponent10 = 0;
	return materialize_currency_origin(origin);
}

CurrencyMaterialization materialize_currency_real(long double value) {
	if (!__builtin_isfinite(value)) {
		return CurrencyMaterialization{RealMaterializationKind::OutOfRange, 0, false};
	}
	const long double scaled = ::nearbyintl(value * 10000.0L);
	constexpr long double positive_limit = 0x1p63L;
	if (scaled < -positive_limit || scaled >= positive_limit) {
		if (!__builtin_isfinite(scaled)) {
			// A finite source whose scaled value exceeds the compiler's
			// lossless working container has no defined unchecked result.
			return CurrencyMaterialization{RealMaterializationKind::OutOfRange, 0, false};
		}
		long double remainder = ::fmodl(scaled, 0x1p64L);
		if (remainder < 0) {
			remainder += 0x1p64L;
		}
		return CurrencyMaterialization{
		    RealMaterializationKind::OutOfRange,
		    std::bit_cast<int64_t>(static_cast<uint64_t>(remainder)),
		};
	}
	return CurrencyMaterialization{
	    scaled == value * 10000.0L ? RealMaterializationKind::Exact : RealMaterializationKind::Rounded,
	    static_cast<int64_t>(scaled),
	};
}

DecimalOrigin currency_decimal_origin(int64_t raw) {
	DecimalOrigin origin;
	origin.negative = raw < 0;
	uint64_t magnitude;
	if (raw == INT64_MIN) {
		magnitude = uint64_t{1} << 63;
	} else {
		magnitude = static_cast<uint64_t>(origin.negative ? -raw : raw);
	}
	origin.digits = std::to_string(magnitude);
	origin.exponent10 = -4;
	return origin;
}

bool is_currency_semantic_type(const Type* type) {
	return distinct_storage_type(type) == currency_type();
}

std::optional<long double> round_typed_real(long double value, Type* target) {
	Type* storage = distinct_storage_type(target);
	if (storage == single_type()) {
		return static_cast<long double>(static_cast<float>(value));
	}
	if (storage == double_type()) {
		return static_cast<long double>(static_cast<double>(value));
	}
	if (storage == extended_type()) {
		return round_to_binary_format(value, *binary_format(storage));
	}
	return std::nullopt;
}

bool typed_real_out_of_range(long double value, Type* target) {
	if (!__builtin_isfinite(value)) {
		return false;
	}
	Type* storage = distinct_storage_type(target);
	if (storage == single_type()) {
		return value < -static_cast<long double>(std::numeric_limits<float>::max()) || value > static_cast<long double>(std::numeric_limits<float>::max());
	}
	if (storage == double_type()) {
		return value < -static_cast<long double>(std::numeric_limits<double>::max()) || value > static_cast<long double>(std::numeric_limits<double>::max());
	}
	if (storage == extended_type()) {
		return false;
	}
	return true;
}

std::optional<long double> eval_typed_real_binary(RealBinaryOperation operation, long double left, long double right, Type* result_type) {
	Type* storage = distinct_storage_type(result_type);
	if (storage == single_type()) {
		return eval_binary<float>(operation, left, right);
	}
	if (storage == double_type()) {
		return eval_binary<double>(operation, left, right);
	}
	if (storage == extended_type()) {
		return round_typed_real(eval_binary<long double>(operation, left, right), storage);
	}
	return std::nullopt;
}
