#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/work"

long="$tmp/long/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/cccccccccccccccccccccccccccccccccccccccc/dddddddddddddddddddddddddddddddddddddddd/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee/ffffffffffffffffffffffffffffffffffffffff"
mkdir -p "$long"


tpcc_translate -o"$tmp/getdir.cc" tests/getdir.pp
if ! rg -Fq '::u_system::p_getdir' "$tmp/getdir.cc"
then
	echo "GetDir did not lower to its System RTL operation" >&2
	exit 1
fi

tpcc_build "$tmp/getdir" \
	"$tmp/getdir.cc" \
	"$tmp/system.cc"

work=$(CDPATH= cd -- "$tmp/work" && pwd -P)
actual=$(cd "$work" &&
	tpcc_run "$tmp/getdir")
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
	tpcc_run "$tmp/getdir")
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
	tpcc_run "$tmp/getdir")
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
	tpcc_run "$tmp/getdir"
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
