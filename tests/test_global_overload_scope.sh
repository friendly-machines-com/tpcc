#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-global-overload-scope.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/same" "$tmp/merge" "$tmp/shadow" "$tmp/legacy"

cd "$root"

./mp -Furtl \
	-o"$tmp/same/main.cc" \
	tests/global_overload_scope/same_scope.pp
"${CXX:-g++}" \
	-std=c++20 -Wall -Wextra \
	-fsanitize=address,undefined \
	-Irtl -I"$tmp/same" \
	"$tmp/same"/*.cc \
	-o "$tmp/same/main"
ASAN_OPTIONS=detect_leaks=1 "$tmp/same/main"

./mp -Furtl -Futests/global_overload_scope \
	-o"$tmp/merge/main.cc" \
	tests/global_overload_scope/merge.pp
"${CXX:-g++}" \
	-std=c++20 -Wall -Wextra \
	-fsanitize=address,undefined \
	-Irtl -I"$tmp/merge" \
	"$tmp/merge"/*.cc \
	-o "$tmp/merge/main"
ASAN_OPTIONS=detect_leaks=1 "$tmp/merge/main"

if ./mp -Furtl -Futests/global_overload_scope \
	-o"$tmp/shadow/main.cc" \
	tests/global_overload_scope/shadow.pp \
	>"$tmp/shadow/stdout" 2>"$tmp/shadow/stderr"
then
	echo "outer routine was implicitly merged without overload" >&2
	exit 1
fi
if ! rg -Fq "no matching overload for 'pick'" \
	"$tmp/shadow/stderr"
then
	echo "wrong cross-scope shadowing diagnostic" >&2
	sed -n '1,40p' "$tmp/shadow/stderr" >&2
	exit 1
fi

./mp -Furtl \
	-o"$tmp/legacy/main.cc" \
	tests/global_overload_scope/legacy_shadow.pp
"${CXX:-g++}" \
	-std=c++20 -Wall -Wextra \
	-fsanitize=address,undefined \
	-Irtl -I"$tmp/legacy" \
	"$tmp/legacy"/*.cc \
	-o "$tmp/legacy/main"
ASAN_OPTIONS=detect_leaks=1 "$tmp/legacy/main"

if ./mp -Furtl \
	-o"$tmp/legacy/rejected.cc" \
	tests/global_overload_scope/legacy_no_fallback.pp \
	>"$tmp/legacy/rejected.stdout" \
	2>"$tmp/legacy/rejected.stderr"
then
	echo "legacy builtin was used as an ordinary overload fallback" >&2
	exit 1
fi
if ! rg -Fq "too many arguments to 'write'" \
	"$tmp/legacy/rejected.stderr"
then
	echo "wrong legacy builtin shadowing diagnostic" >&2
	sed -n '1,40p' "$tmp/legacy/rejected.stderr" >&2
	exit 1
fi
if rg -Fq "rtl/system.pp" \
	"$tmp/legacy/rejected.stderr"
then
	echo "legacy System.Write leaked into the ordinary overload family" >&2
	sed -n '1,80p' "$tmp/legacy/rejected.stderr" >&2
	exit 1
fi

echo "global overload scope tests passed"
