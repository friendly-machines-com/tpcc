#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-inherited-calls-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/inherited_calls.cc" \
	tests/inherited_calls.pp

if ! rg -Fq 't_tbase::p_insert(' \
	"$tmp/inherited_calls.cc"
then
	echo "parenthesized inherited instance call was not emitted" >&2
	exit 1
fi
if ! rg -Fq 't_tbase::p_select(' \
	"$tmp/inherited_calls.cc"
then
	echo "inherited overload was not selected" >&2
	exit 1
fi
if ! rg -Fq 't_tbase::p_withdefault(7ull)' \
	"$tmp/inherited_calls.cc"
then
	echo "inherited call did not materialize a default argument" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Irtl \
	-I"$tmp" \
	"$tmp/inherited_calls.cc" \
	"$tmp/system.cc" \
	-o "$tmp/inherited_calls"

actual=$("$tmp/inherited_calls")
test "$actual" = 'inherited calls passed'

echo "inherited call tests passed"
