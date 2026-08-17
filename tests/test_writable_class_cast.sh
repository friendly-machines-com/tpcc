#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/writable_class_cast.cc" tests/writable_class_cast.pp
if ! grep -Fq 'reinterpret_cast<t_tnode*&>(p_temp)' "$tmp/writable_class_cast.cc"; then
	echo "widening class cast did not lower to the direct storage alias" >&2
	exit 1
fi

tpcc_build "$tmp/writable_class_cast" \
	tests/writable_class_cast_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/writable_class_cast"

if tpcc_translate -o"$tmp/rejected.cc" \
    tests/writable_class_cast_narrow_rejected.pp \
    >"$tmp/rejected.out" 2>&1
then
	echo "narrowing class cast was accepted as var argument" >&2
	exit 1
fi
if ! grep -Fq "argument for var/out parameter 'p' is not a storage-backed expression" "$tmp/rejected.out"; then
	echo "narrowing class cast produced the wrong diagnostic" >&2
	cat "$tmp/rejected.out" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/rejected.cc" \
    tests/writable_class_cast_unrelated_rejected.pp \
    >"$tmp/rejected.out" 2>&1
then
	echo "unrelated class cast was accepted as var argument" >&2
	exit 1
fi

echo "writable-class-cast tests passed"
