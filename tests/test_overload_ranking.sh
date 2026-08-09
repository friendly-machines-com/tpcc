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

echo "overload ranking tests passed"
