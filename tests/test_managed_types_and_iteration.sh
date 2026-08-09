#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-managed-types-iteration-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/managed.cc" \
	tests/managed_types_and_iteration.pp
./mp -Furtl -o"$tmp/custom.cc" \
	tests/custom_enumerators.pp

for required in \
	'm_openarray_const_view(' \
	'm_openarray_value_copy(' \
	'm_openarray_mutable_view(' \
	'm_openarray_out_view(' \
	'm_enumerate('
do
	if ! rg -Fq "$required" "$tmp/managed.cc"
	then
		echo "missing managed sequence lowering: $required" >&2
		exit 1
	fi
done

for required in \
	'auto tpcc_for_enumerator = ' \
	'::u_system::m_free_object(tpcc_for_enumerator)' \
	'tpcc_for_enumerator.p_done()'
do
	if ! rg -Fq "$required" "$tmp/custom.cc"
	then
		echo "missing custom-enumerator lowering: $required" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/managed.cc" \
	"$tmp/system.cc" \
	-o "$tmp/managed"

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/custom.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/custom"

ASAN_OPTIONS=detect_leaks=1 "$tmp/managed"
ASAN_OPTIONS=detect_leaks=1 "$tmp/custom"

for source in \
	tests/for_in_sparse_enum_rejected.pp \
	tests/custom_enumerator_movenext_rejected.pp \
	tests/dynamic_array_assignment_rejected.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
	    >"$tmp/rejected.out" 2>"$tmp/rejected.err"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected" "$tmp/rejected.err"
	then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,80p' "$tmp/rejected.err" >&2
		exit 1
	fi
done

echo "managed-type and iteration tests passed"
