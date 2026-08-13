#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/overload_ranking.cc" \
	tests/overload_ranking.pp

tpcc_build "$tmp/overload_ranking" \
	-Wno-unused-parameter \
	"$tmp/overload_ranking.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/overload_ranking"

if tpcc_translate \
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
	if ! grep -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete per-argument catch-all diagnostic" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

if tpcc_translate \
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
	if ! grep -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete equal-rank ambiguity diagnostic: $required" >&2
		sed -n '1,220p' "$tmp/stderr" >&2
		exit 1
	fi
done
if grep -Fq 'convert+' "$tmp/stderr"
then
	echo "overload diagnostic exposed a raw numeric tie-breaker" >&2
	sed -n '1,220p' "$tmp/stderr" >&2
	exit 1
fi

tpcc_translate \
	-o"$tmp/untyped_constants.cc" \
	tests/overload_ranking_untyped_constants.pp
tpcc_build "$tmp/untyped_constants" \
	"$tmp/untyped_constants.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/untyped_constants"

echo "overload ranking tests passed"
