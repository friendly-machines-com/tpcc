#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/directive_push_pop.cc" \
	tests/directive_push_pop.pp

for selected in \
	p_innerq \
	p_innerr \
	p_innero \
	p_nestedlocalrestored \
	p_includechangedi \
	p_restoredq \
	p_restoredr \
	p_restoredi \
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

tpcc_build "$tmp/directive_push_pop" \
	"$tmp/directive_push_pop.cc" \
	"$tmp/system.cc"
"$tmp/directive_push_pop"

for source in \
	tests/directive_pop_underflow.pp \
	tests/directive_switch_list_invalid.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
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
