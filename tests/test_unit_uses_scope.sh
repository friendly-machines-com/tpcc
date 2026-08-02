#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-unit-uses-scope-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/positive" "$tmp/no-interface-reexport" \
	"$tmp/no-implementation-reexport" "$tmp/cycles" \
	"$tmp/explicit" "$tmp/shadow-type" "$tmp/shadow-value" \
	"$tmp/late-cycle"

cd "$root"

./mp -Furtl -Futests/unit_uses_scope \
	-o"$tmp/positive/scope_main.cc" \
	tests/unit_uses_scope/scope_main.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-I"$tmp/positive" \
	-Irtl \
	"$tmp/positive"/*.cc \
	-o "$tmp/positive/scope_main"

ASAN_OPTIONS=detect_leaks=1 "$tmp/positive/scope_main"

for case in explicit shadow-type shadow-value
do
	source=$(printf '%s' "$case" | tr '-' '_')
	./mp -Furtl -Futests/unit_uses_scope \
		-o"$tmp/$case/main.cc" \
		"tests/unit_uses_scope/scope_$source.pp"
	"${CXX:-g++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-fsanitize=address,undefined \
		-I"$tmp/$case" \
		-Irtl \
		"$tmp/$case"/*.cc \
		-o "$tmp/$case/main"
	ASAN_OPTIONS=detect_leaks=1 "$tmp/$case/main"
done

if ./mp -Furtl -Futests/unit_uses_scope \
	-o"$tmp/no-interface-reexport/main.cc" \
	tests/unit_uses_scope/scope_no_interface_reexport.pp \
	>"$tmp/no-interface-reexport/stdout" \
	2>"$tmp/no-interface-reexport/stderr"
then
	echo "interface uses was incorrectly re-exported" >&2
	exit 1
fi
if ! rg -Fq "unresolved value identifier: bvalue" \
	"$tmp/no-interface-reexport/stderr"
then
	echo "wrong diagnostic for an interface-use name in a user of the unit" >&2
	sed -n '1,20p' "$tmp/no-interface-reexport/stderr" >&2
	exit 1
fi

if ./mp -Furtl -Futests/unit_uses_scope \
	-o"$tmp/no-implementation-reexport/main.cc" \
	tests/unit_uses_scope/scope_no_implementation_reexport.pp \
	>"$tmp/no-implementation-reexport/stdout" \
	2>"$tmp/no-implementation-reexport/stderr"
then
	echo "implementation uses was incorrectly re-exported" >&2
	exit 1
fi
if ! rg -Fq "unresolved value identifier: cvalue" \
	"$tmp/no-implementation-reexport/stderr"
then
	echo "wrong diagnostic for an implementation-use name in a user of the unit" >&2
	sed -n '1,20p' "$tmp/no-implementation-reexport/stderr" >&2
	exit 1
fi

./mp -Furtl -Futests/07_mutual_impl \
	-o"$tmp/cycles/implementation.cc" \
	tests/07_mutual_impl/a.pp

./mp -Furtl -Futests/unit_uses_scope \
	-o"$tmp/late-cycle/main.cc" \
	tests/unit_uses_scope/late_cycle_main.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-I"$tmp/late-cycle" \
	-Irtl \
	"$tmp/late-cycle"/*.cc \
	-o "$tmp/late-cycle/main"
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/late-cycle/main"

if ./mp -Furtl -Futests/08_mutual_iface \
	-o"$tmp/cycles/interface.cc" \
	tests/08_mutual_iface/a.pp \
	>"$tmp/cycles/stdout" \
	2>"$tmp/cycles/stderr"
then
	echo "accepted an interface dependency cycle" >&2
	exit 1
fi
if ! rg -Fq "circular interface dependency" "$tmp/cycles/stderr"
then
	echo "wrong diagnostic for an interface dependency cycle" >&2
	sed -n '1,20p' "$tmp/cycles/stderr" >&2
	exit 1
fi

echo "unit uses scope tests passed"
