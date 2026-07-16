#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-directive-push-pop-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/directive_push_pop.cc" \
	tests/directive_push_pop.pp

for selected in \
	p_innerq \
	p_innerr \
	p_innerd \
	p_innero \
	p_innera \
	p_innerz \
	p_nestedlocalrestored \
	p_includechangedi \
	p_restoredq \
	p_restoredr \
	p_restoredi \
	p_restoreda \
	p_restoredz \
	p_persistentd \
	p_persistento \
	p_defineremovalpersists \
	p_defineadditionpersists \
	p_inactivebranchignored \
	p_crossincludepop
do
	if ! rg -q "\\b${selected}\\b" \
		"$tmp/directive_push_pop.cc"
	then
		echo "missing directive-selected declaration: $selected" >&2
		exit 1
	fi
done

if rg -q '\\bp_wrong' "$tmp/directive_push_pop.cc"
then
	echo "an unselected directive branch was emitted" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-Irtl \
	-I"$tmp" \
	"$tmp/directive_push_pop.cc" \
	"$tmp/system.cc" \
	-o "$tmp/directive_push_pop"
"$tmp/directive_push_pop"

for source in \
	tests/directive_pop_underflow.pp \
	tests/directive_switch_list_invalid.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid directive source: $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong directive diagnostic; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "directive PUSH/POP tests passed"
