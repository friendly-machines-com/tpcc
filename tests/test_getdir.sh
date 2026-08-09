#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-getdir-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/work"

long="$tmp/long/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/cccccccccccccccccccccccccccccccccccccccc/dddddddddddddddddddddddddddddddddddddddd/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/ffffffffffffffffffffffffffffffffffffffff"
mkdir -p "$long"

cd "$root"

./mp -Furtl -o"$tmp/getdir.cc" tests/getdir.pp
if ! rg -Fq '::u_system::p_getdir' "$tmp/getdir.cc"
then
	echo "GetDir did not lower to its System RTL operation" >&2
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
	"$tmp/getdir.cc" \
	"$tmp/system.cc" \
	-o "$tmp/getdir"

work=$(CDPATH= cd -- "$tmp/work" && pwd -P)
actual=$(cd "$work" &&
	ASAN_OPTIONS=detect_leaks=1 "$tmp/getdir")
expected="$work
0
$work
0
$work
0
$work
0"
if test "$actual" != "$expected"
then
	echo "unexpected GetDir result in ordinary directory" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

actual=$(cd / &&
	ASAN_OPTIONS=detect_leaks=1 "$tmp/getdir")
expected='/
0
/
0
/
0
/
0'
if test "$actual" != "$expected"
then
	echo "unexpected GetDir result at filesystem root" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

actual=$(cd "$long" &&
	ASAN_OPTIONS=detect_leaks=1 "$tmp/getdir")
expected="unchanged
3
unchanged
3
$long
0
$long
0"
if test "$actual" != "$expected"
then
	echo "unexpected GetDir result for a path longer than ShortString" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

gone="$tmp/gone"
mkdir "$gone"
actual=$(
	cd "$gone"
	rmdir "$gone"
	ASAN_OPTIONS=detect_leaks=1 "$tmp/getdir"
)
expected='unchanged
2
unchanged
2
unchanged
2
unchanged
2'
if test "$actual" != "$expected"
then
	echo "unexpected GetDir result after current-directory lookup failure" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "GetDir tests passed"
