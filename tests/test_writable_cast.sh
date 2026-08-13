#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/writable_cast.cc" tests/writable_cast.pp
if ! grep -Fq '::u_system::tpcc_store_writable_cast<' "$tmp/writable_cast.cc"; then
	echo "writable cast did not lower through typed RTL storage" >&2
	exit 1
fi
if ! grep -Fq '::u_system::tpcc_byte_array_storage_view<' "$tmp/writable_cast.cc"; then
	echo "scalar Byte-array cast did not lower through a direct storage view" >&2
	exit 1
fi

tpcc_build "$tmp/writable_cast" \
	tests/writable_cast_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/writable_cast"

for rejected in temporary size
do
	if tpcc_translate -o"$tmp/rejected.cc" \
	    "tests/writable_cast_${rejected}_rejected.pp" \
	    >"$tmp/rejected.out" 2>&1
	then
		echo "invalid writable $rejected cast was accepted" >&2
		exit 1
	fi
	if ! grep -Fq "LHS of ':=' is not assignable" "$tmp/rejected.out"; then
		echo "invalid writable $rejected cast produced the wrong diagnostic" >&2
		cat "$tmp/rejected.out" >&2
		exit 1
	fi
done

if tpcc_translate -o"$tmp/rejected.cc" \
    tests/writable_byte_array_temporary_rejected.pp \
    >"$tmp/rejected.out" 2>&1
then
	echo "writable Byte-array view of a temporary was accepted" >&2
	exit 1
fi
if ! grep -Fq "LHS of ':=' is not assignable" "$tmp/rejected.out"; then
	echo "writable Byte-array temporary produced the wrong diagnostic" >&2
	cat "$tmp/rejected.out" >&2
	exit 1
fi

echo "writable-cast tests passed"
