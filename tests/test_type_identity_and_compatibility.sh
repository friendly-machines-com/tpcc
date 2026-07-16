#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-type-identity-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Isrc \
	-Irtl \
	tests/type_conversion_algebra.cc \
	src/cst.o \
	src/directive_expr.o \
	src/frame.o \
	src/types.o \
	src/evaluator.o \
	src/builtins.o \
	src/units.o \
	src/emit.o \
	src/diagnostic.o \
	-o "$tmp/type_conversion_algebra"

"$tmp/type_conversion_algebra"

./mp -Furtl -o"$tmp/type_identity.cc" \
	tests/type_identity_and_compatibility.pp

for required in \
	'p_implicit(' \
	'm_implicit_target<t_tconversionresulta>' \
	'm_implicit_target<t_tconversionresultb>'
do
	if ! rg -Fq "$required" "$tmp/type_identity.cc"
	then
		echo "missing implicit-conversion lowering: $required" >&2
		exit 1
	fi
done

if rg -Fq 'p_operator_assign' "$tmp/type_identity.cc"
then
	echo "implicit conversion retained the old C++ operator name" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/type_identity.cc" \
	"$tmp/system.cc" \
	-o "$tmp/type_identity"

ASAN_OPTIONS=detect_leaks=1 "$tmp/type_identity"

./mp -Furtl -o"$tmp/explicit_ordinal_casts.cc" \
	tests/explicit_ordinal_casts.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/explicit_ordinal_casts.cc" \
	"$tmp/system.cc" \
	-o "$tmp/explicit_ordinal_casts"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/explicit_ordinal_casts"

for narrowing in \
	INTEGER_ASSIGNMENT \
	SIGNEDNESS_ASSIGNMENT \
	REAL_ASSIGNMENT \
	SUBRANGE_ASSIGNMENT \
	BASE_TO_SUBRANGE \
	SINGLETON_ARGUMENT
do
	if ./mp -Furtl -d"TEST_$narrowing" \
		-o"$tmp/type_narrowing_rejected.cc" \
		tests/type_narrowing_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted implicit narrowing: $narrowing" >&2
		exit 1
	fi
	if ! rg -q \
		'no implicit conversion|no matching overload' \
		"$tmp/stderr"
	then
		echo "wrong implicit-narrowing diagnostic: $narrowing" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi
done

./mp -Furtl -o"$tmp/contextual_values.cc" \
	tests/contextual_value_immutability.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/contextual_values.cc" \
	"$tmp/system.cc" \
	-o "$tmp/contextual_values"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/contextual_values"

./mp -Furtl -o"$tmp/class_metaclass.cc" \
	tests/class_metaclass_carrier_separation.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Irtl \
	-I"$tmp" \
	"$tmp/class_metaclass.cc" \
	"$tmp/system.cc" \
	-o "$tmp/class_metaclass"

for source in fixed_array_assignment_rejected set_narrowing_rejected
do
	if ./mp -Furtl -o"$tmp/$source.cc" \
		"tests/$source.pp" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid value assignment: $source" >&2
		exit 1
	fi
	if ! rg -Fq 'no implicit conversion' "$tmp/stderr"
	then
		echo "wrong value-assignment diagnostic: $source" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

for source in ambiguous_user_conversion_rejected integer_literal_range_rejected
do
	if ./mp -Furtl -o"$tmp/$source.cc" \
		"tests/$source.pp" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid contextual conversion: $source" >&2
		exit 1
	fi
	case "$source" in
	ambiguous_user_conversion_rejected)
		expected='ambiguous implicit conversion'
		;;
	integer_literal_range_rejected)
		expected='no implicit conversion'
		;;
	esac
	if ! rg -Fq "$expected" "$tmp/stderr"
	then
		echo "wrong contextual-conversion diagnostic: $source" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

for kind in POINTER STRING SET RANGE ARRAY FILE ROUTINE CLASSREF
do
	if ./mp -Furtl -d"TEST_$kind" \
		-o"$tmp/carrier_collision.cc" \
		tests/type_carrier_collision_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an erased C++ overload collision: $kind" >&2
		exit 1
	fi
	if ! rg -Fq 'same C++ parameter carriers' "$tmp/stderr"
	then
		echo "wrong C++ carrier-collision diagnostic: $kind" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
	for required in \
		'incoming declaration:' \
		'conflicting declaration:' \
		'existing overload family:' \
		'where' \
		'source:'
	do
		if ! rg -Fq "$required" "$tmp/stderr"
		then
			echo "incomplete C++ carrier-collision diagnostic: $kind" >&2
			sed -n '1,80p' "$tmp/stderr" >&2
			exit 1
		fi
	done
	if rg -Fq '<unregistered' "$tmp/stderr"
	then
		echo "unresolved reference in C++ carrier-collision diagnostic: $kind" >&2
		sed -n '1,80p' "$tmp/stderr" >&2
		exit 1
	fi

	if ./mp -Furtl -d"TEST_$kind" \
		-o"$tmp/var_identity.cc" \
		tests/type_var_identity_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted distinct Type* as typed var: $kind" >&2
		exit 1
	fi
	if ! rg -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong typed-var identity diagnostic: $kind" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

if ./mp -Furtl \
	-dTEST_CONVERSION \
	-o"$tmp/implicit_conversion_erased_target.cc" \
	tests/type_carrier_collision_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted indistinguishable implicit-conversion target tags" >&2
	exit 1
fi
for required in \
	"tests/type_carrier_collision_rejected.pp(59)" \
	"tests/type_carrier_collision_rejected.pp(60)" \
	'incoming declaration:' \
	'conflicting declaration:' \
	'existing overload family:' \
	'Pascal distinguishes these implicit conversions by destination type' \
	'hidden destination tags still have the same C++ carrier' \
	'type tconversionsource' \
	'type tstringa' \
	'type tstringb'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete implicit-conversion carrier diagnostic: $required" >&2
		sed -n '1,120p' "$tmp/stderr" >&2
		exit 1
	fi
done
if rg -Fq '<unregistered' "$tmp/stderr"
then
	echo "unresolved reference in conversion-carrier diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

if ./mp -Furtl -o"$tmp/accidental_override.cc" \
	tests/accidental_virtual_carrier_override_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an accidental C++ virtual override" >&2
	exit 1
fi
if ! rg -Fq 'would accidentally override an ancestor' "$tmp/stderr"
then
	echo "wrong accidental-override diagnostic" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "type identity and compatibility tests passed"
