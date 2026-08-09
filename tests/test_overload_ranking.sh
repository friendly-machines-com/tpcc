#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-overload-ranking.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/overload_ranking.cc" \
	tests/overload_ranking.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Wno-unused-parameter \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/overload_ranking.cc" \
	"$tmp/system.cc" \
	-o "$tmp/overload_ranking"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/overload_ranking"

if ./mp -Furtl \
	-o"$tmp/generic_ambiguous.cc" \
	tests/overload_ranking_generic_ambiguous.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted crossed per-argument catch-all ranks" >&2
	exit 1
fi
for required in \
	'ambiguous overload' \
	'viable ranks [exact, generic]' \
	'viable ranks [generic, exact]'
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete per-argument catch-all diagnostic" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

if ./mp -Furtl \
	-o"$tmp/signedness_ambiguous.cc" \
	tests/overload_ranking_signedness_ambiguous.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted crossed per-argument signedness preferences" >&2
	exit 1
fi
for required in \
	'ambiguous overload' \
	'[ambiguous]' \
	'viable ranks [convert, convert]' \
	'conflicting argument preferences:' \
	'arg 1 prefers' \
	'arg 2 prefers' \
	"preserves the actual integer type's signedness"
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete equal-rank ambiguity diagnostic: $required" >&2
		sed -n '1,220p' "$tmp/stderr" >&2
		exit 1
	fi
done
if rg -Fq 'convert+' "$tmp/stderr"
then
	echo "overload diagnostic exposed a raw numeric tie-breaker" >&2
	sed -n '1,220p' "$tmp/stderr" >&2
	exit 1
fi

./mp -Furtl \
	-o"$tmp/untyped_constants.cc" \
	tests/overload_ranking_untyped_constants.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/untyped_constants.cc" \
	"$tmp/system.cc" \
	-o "$tmp/untyped_constants"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/untyped_constants"

echo "overload ranking tests passed"
