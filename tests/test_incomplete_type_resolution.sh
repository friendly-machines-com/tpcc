#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/incomplete_type_resolution.cc" \
	tests/incomplete_type_resolution.pp

if ! rg -Fq 'struct t_tsecond;' \
	"$tmp/incomplete_type_resolution.cc"
then
	echo "later aggregate did not receive a C++ forward declaration" >&2
	exit 1
fi
if ! rg -Fq 't_tsecond* p_next;' \
	"$tmp/incomplete_type_resolution.cc"
then
	echo "aggregate member forward reference was not resolved" >&2
	exit 1
fi
if ! rg -Fq 'inline static t_tvalue& p_seven()' \
	"$tmp/incomplete_type_resolution.cc" ||
   ! rg -Fq 'static t_tvalue m_value = ' \
	"$tmp/incomplete_type_resolution.cc"
then
	echo "typed aggregate constant did not receive stable deferred storage" >&2
	exit 1
fi
if ! rg -Fq 'struct t_timplicitclass;' \
	"$tmp/incomplete_type_resolution.cc"
then
	echo "class-of target did not receive a C++ forward declaration" >&2
	exit 1
fi

tpcc_build "$tmp/incomplete_type_resolution" \
	"$tmp/incomplete_type_resolution.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/incomplete_type_resolution"

tpcc_translate \
	-o"$tmp/incomplete_type_callable_normalization.cc" \
	tests/incomplete_type_callable_normalization.pp

if ! rg -Fq 'p_getcopy() override;' \
	"$tmp/incomplete_type_callable_normalization.cc"
then
	echo "same-block self-result override was not retained after normalization" >&2
	exit 1
fi

tpcc_build "$tmp/incomplete_type_callable_normalization" \
	"$tmp/incomplete_type_callable_normalization.cc" \
	"$tmp/system.cc"
"$tmp/incomplete_type_callable_normalization"

if tpcc_translate \
	-o"$tmp/rejected.cc" \
	tests/aggregate_true_constant_address_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted address of a true aggregate constant" >&2
	exit 1
fi
expected=$(sed -n '1p' \
	tests/aggregate_true_constant_address_rejected.error)
if ! rg -F -q -- "$expected" "$tmp/stderr"
then
	echo "wrong true-constant address diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/rejected.cc" \
	tests/incomplete_type_by_value_cycle.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted a type containing itself by value" >&2
	exit 1
fi
expected=$(sed -n '1p' \
	tests/incomplete_type_by_value_cycle.error)
if ! rg -F -q -- "$expected" "$tmp/stderr"
then
	echo "wrong by-value recursion diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi
if rg -Fq 'internal compiler error' "$tmp/stderr"
then
	echo "by-value recursion escaped to C++ layout" >&2
	exit 1
fi

expected=$(sed -n '1p' \
	tests/invalid_implicit_type_forwards.error)
for define in \
	TEST_ARRAY_FORWARD \
	TEST_SET_FORWARD \
	TEST_FILE_FORWARD \
	TEST_ALIAS_FORWARD
do
	if tpcc_translate -d"$define" \
		-o"$tmp/rejected.cc" \
		tests/invalid_implicit_type_forwards.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid implicit type forward: $define" >&2
		exit 1
	fi
	if ! rg -F -q -- "$expected" "$tmp/stderr"
	then
		echo "wrong implicit-forward diagnostic for $define; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

expected=$(sed -n '1p' \
	tests/invalid_recursive_type_equations.error)
for define in \
	TEST_POINTER_SELF \
	TEST_FILE_SELF \
	TEST_SET_SELF \
	TEST_ROUTINE_SELF \
	TEST_ARRAY_SELF
do
	if tpcc_translate -d"$define" \
		-o"$tmp/rejected.cc" \
		tests/invalid_recursive_type_equations.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid recursive type equation: $define" >&2
		exit 1
	fi
	if ! rg -F -q -- "$expected" "$tmp/stderr"
	then
		echo "wrong recursive-type diagnostic for $define; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "incomplete type resolution tests passed"
