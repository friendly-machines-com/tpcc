#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/pointer_arithmetic.cc" \
	tests/pointer_arithmetic.pp

if grep -Fq 'reinterpret_cast<uintptr_t>' \
	"$root/rtl/rtl.h" \
	"$tmp/pointer_arithmetic.cc" "$tmp/system.cc"
then
	echo "pointer arithmetic converted a pointer to an integer" >&2
	exit 1
fi

tpcc_build "$tmp/pointer_arithmetic" \
	-fsanitize=address,undefined,pointer-overflow,pointer-subtract \
	"$tmp/pointer_arithmetic.cc" \
	"$tmp/system.cc"

ASAN_OPTIONS=detect_leaks=1:detect_invalid_pointer_pairs=2 tpcc_run \
	"$tmp/pointer_arithmetic"

for rejection in \
	DIFFERENT_TYPES \
	UNTYPED_DIFFERENCE
do
	if tpcc_translate -dREJECTION_ONLY -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" \
		tests/pointer_arithmetic.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid pointer arithmetic: $rejection" >&2
		exit 1
	fi
	if ! grep -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong pointer-arithmetic diagnostic: $rejection" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

if tpcc_translate -dREJECTION_ONLY -dREJECT_UNTYPED_STEP \
	-o"$tmp/rejected.cc" \
	tests/pointer_arithmetic.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted invalid pointer arithmetic: UNTYPED_STEP" >&2
	exit 1
fi
if ! grep -Fq \
	'inc requires an ordinal or typed pointer argument' \
	"$tmp/stderr"
then
	echo "wrong pointer-arithmetic diagnostic: UNTYPED_STEP" >&2
	sed -n '1,180p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -dREJECTION_ONLY -dREJECT_POINTER_CAST_ADDRESS \
	-o"$tmp/rejected.cc" \
	tests/pointer_arithmetic.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted address of a pointer-value cast" >&2
	exit 1
fi
if ! grep -Fq \
	'address requires a storage-backed expression' \
	"$tmp/stderr"
then
	echo "wrong pointer-cast address diagnostic" >&2
	sed -n '1,180p' "$tmp/stderr" >&2
	exit 1
fi

echo "pointer arithmetic tests passed"
