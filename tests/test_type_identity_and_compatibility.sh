#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_build_native "$tmp/type_conversion_algebra" \
	-Isrc \
	-Irtl \
	tests/type_conversion_algebra.cc \
	src/cst.o \
	src/directive_expr.o \
	src/frame.o \
	src/types.o \
	src/evaluator.o \
	src/numeric_constants.o \
	src/builtins.o \
	src/operators.o \
	src/units.o \
	src/emit.o \
	src/diagnostic.o

"$tmp/type_conversion_algebra"

tpcc_translate -o"$tmp/type_identity.cc" \
	tests/type_identity_and_compatibility.pp

for required in \
	'o_implicit(' \
	'm_conversion_target<t_tconversionresulta>' \
	'm_conversion_target<t_tconversionresultb>'
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

tpcc_build "$tmp/type_identity" \
	"$tmp/type_identity.cc" \
	"$tmp/system.cc"

tpcc_run "$tmp/type_identity"

tpcc_translate -o"$tmp/explicit_ordinal_casts.cc" \
	tests/explicit_ordinal_casts.pp
tpcc_build "$tmp/explicit_ordinal_casts" \
	"$tmp/explicit_ordinal_casts.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/explicit_ordinal_casts"

tpcc_translate -o"$tmp/contextual_values.cc" \
	tests/contextual_value_immutability.pp
tpcc_build "$tmp/contextual_values" \
	"$tmp/contextual_values.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/contextual_values"

if tpcc_translate -o"$tmp/class_metaclass.cc" \
	tests/class_instance_overload_category_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "formed one overload set from instance and class methods" >&2
	exit 1
fi
for required in \
	'incompatible routine categories' \
	'incoming declaration:' \
	'conflicting declaration:' \
	'existing overload family:' \
	'kind: method' \
	'kind: class_method' \
	'where' \
	'source:'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete mixed-routine-kind diagnostic: $required" >&2
		sed -n '1,140p' "$tmp/stderr" >&2
		exit 1
	fi
done

for source in fixed_array_assignment_rejected set_narrowing_rejected
do
	if tpcc_translate -o"$tmp/$source.cc" \
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
	if tpcc_translate -o"$tmp/$source.cc" \
		"tests/$source.pp" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid contextual conversion: $source" >&2
		exit 1
	fi
	case "$source" in
	ambiguous_user_conversion_rejected)
		# TBoth -> ILeft/IRight -> TResult would be a two-edge implicit
		# conversion chain. Neither interface conversion is a candidate.
		expected='no implicit conversion'
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

for kind in POINTER STRING SET ARRAY FILE ROUTINE CLASSREF
do
	if tpcc_translate -d"TEST_$kind" \
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

done

for kind in POINTER STRING SET RANGE ARRAY FILE ROUTINE CLASSREF
do
	if tpcc_translate -d"TEST_$kind" \
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

if tpcc_translate \
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
	'Pascal distinguishes these conversion operators by destination type' \
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

if tpcc_translate \
	-dTEST_EXPLICIT_CONVERSION \
	-o"$tmp/explicit_conversion_erased_target.cc" \
	tests/type_carrier_collision_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted indistinguishable explicit-conversion target tags" >&2
	exit 1
fi
for required in \
	'incoming declaration:' \
	'conflicting declaration:' \
	'existing overload family:' \
	'Pascal distinguishes these conversion operators by destination type' \
	'hidden destination tags still have the same C++ carrier' \
	'type tconversionsource' \
	'type tstringa' \
	'type tstringb'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete explicit-conversion carrier diagnostic: $required" >&2
		sed -n '1,120p' "$tmp/stderr" >&2
		exit 1
	fi
done
if rg -Fq '<unregistered' "$tmp/stderr"
then
	echo "unresolved reference in explicit conversion-carrier diagnostic" >&2
	sed -n '1,120p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/accidental_override.cc" \
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
