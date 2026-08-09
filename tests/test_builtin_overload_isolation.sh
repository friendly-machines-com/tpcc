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
user dispose 37'
		;;
	write)
		expected=''
		;;
	str)
		expected='17'
		;;
	sizeof)
		expected='21'
		;;
	trunc_round)
		expected='21
22'
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
		echo "builtin-name shadowing failed to compile: $source" >&2
		sed -n '1,80p' "$tmp/$name.compile.err" >&2
		return 1
	fi

	if ! "${CXX:-g++}" \
	    -std=c++20 \
	    -Wall \
	    -Wextra \
	    -Wpedantic \
	    -fsanitize=address,undefined \
	    -fno-sanitize-recover=all \
	    -Irtl \
	    -I"$tmp" \
	    "$tmp/$name.cc" \
	    "$tmp/system.cc" \
	    -o "$tmp/$name"
	then
		echo "builtin-name shadowing emitted invalid C++: $source" >&2
		return 1
	fi

	if ! actual=$(ASAN_OPTIONS=detect_leaks=1 "$tmp/$name")
	then
		echo "builtin-name shadowing executable failed: $source" >&2
		return 1
	fi
	if test "$actual" != "$expected"
	then
		echo "wrong builtin-name shadowing selection: $source" >&2
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

source=tests/qualified_builtin_syntax.pp
if ! ./mp -Furtl -o"$tmp/qualified.cc" "$source" \
    >"$tmp/qualified.compile.out" 2>"$tmp/qualified.compile.err"
then
	echo "qualified builtin syntax failed to compile: $source" >&2
	sed -n '1,80p' "$tmp/qualified.compile.err" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/qualified.cc" \
	"$tmp/system.cc" \
	-o "$tmp/qualified"

if ! actual=$(ASAN_OPTIONS=detect_leaks=1 "$tmp/qualified")
then
	echo "qualified builtin syntax executable failed: $source" >&2
	exit 1
fi
if test "$actual" != "qualified 7"
then
	echo "wrong qualified builtin output: $source" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
	    "qualified 7" "$actual" >&2
	exit 1
fi

echo "builtin-name shadowing tests passed"
