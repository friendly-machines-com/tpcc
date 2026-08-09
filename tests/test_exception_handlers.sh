#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-exception-handlers-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/exception_handlers.cc" \
	tests/exception_handlers.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/exception_handlers.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/exception_handlers"
ASAN_OPTIONS=detect_leaks=1 "$tmp/exception_handlers"

./mp -Furtl -o"$tmp/unhandled_exception.cc" \
	tests/unhandled_exception.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/unhandled_exception.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc" \
	-o "$tmp/unhandled_exception"
status=0
ASAN_OPTIONS=detect_leaks=1 "$tmp/unhandled_exception" \
	>"$tmp/unhandled.stdout" || status=$?
if test "$status" -ne 217; then
	echo "unhandled Pascal exception returned $status, expected 217" >&2
	exit 1
fi
if ! rg -Fq 'Unhandled Pascal exception' \
	"$tmp/unhandled.stdout"; then
	echo "SysUtils unhandled-exception hook did not run" >&2
	exit 1
fi

for source in \
	tests/goto_out_of_exception_block.pp \
	tests/goto_into_exception_block.pp \
	tests/reraise_outside_handler.pp \
	tests/reraise_inside_nested_try.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
	    >"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "exception-handler tests passed"
