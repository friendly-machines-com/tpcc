#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-builtin-overload-isolation-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

run_case() {
	name=$1
	case "$name" in
	new_dispose)
		expected='user new 31
5
user dispose 37'
		;;
	write)
		expected='builtin output
11
13'
		;;
	str)
		expected='17
123'
		;;
	sizeof)
		expected='21
1'
		;;
	trunc_round)
		expected='21
22
4
5'
		;;
	*)
		echo "unknown builtin-overload isolation case: $name" >&2
		return 1
		;;
	esac

	source="tests/builtin_overload_${name}.pp"
	if ! ./mp -Furtl -o"$tmp/$name.cc" "$source" \
	    >"$tmp/$name.compile.out" 2>"$tmp/$name.compile.err"
	then
		echo "mixed builtin/user overloads failed to compile: $source" >&2
		sed -n '1,80p' "$tmp/$name.compile.err" >&2
		return 1
	fi

	if ! "${CXX:-g++}" \
	    -std=c++20 \
	    -Wall \
	    -Wextra \
	    -Wpedantic \
	    -Werror \
	    -fsanitize=address,undefined \
	    -fno-sanitize-recover=all \
	    -Irtl \
	    -I"$tmp" \
	    "$tmp/$name.cc" \
	    "$tmp/system.cc" \
	    -o "$tmp/$name"
	then
		echo "mixed builtin/user overloads emitted invalid C++: $source" >&2
		return 1
	fi

	if ! actual=$(ASAN_OPTIONS=detect_leaks=1 "$tmp/$name")
	then
		echo "mixed builtin/user overload executable failed: $source" >&2
		return 1
	fi
	if test "$actual" != "$expected"
	then
		echo "wrong mixed builtin/user overload selection: $source" >&2
		printf 'expected:\n%s\nactual:\n%s\n' \
		    "$expected" "$actual" >&2
		return 1
	fi
}

status=0
for name in \
	new_dispose \
	write \
	str \
	sizeof \
	trunc_round
do
	if ! run_case "$name"
	then
		status=1
	fi
done

if test "$status" -ne 0
then
	exit "$status"
fi

echo "builtin overload isolation tests passed"
