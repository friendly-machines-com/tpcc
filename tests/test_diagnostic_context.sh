#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-diagnostic-context-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

if ./mp -Furtl -Futests \
	-o"$tmp/diagnostic_context_rejected.cc" \
	tests/diagnostic_context_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted program with an unresolved method-body identifier" >&2
	exit 1
fi

for required in \
	"unresolved value identifier: missingname" \
	"  context:" \
	"    program: diagnosticcontextrejected" \
	"    routine: \\tcontext.trigger\\" \
	"    owner type: tcontext" \
	"  value diagnosticcontextrejected =" \
	"    unit_reference" \
	"      unit: 'diagnosticcontextrejected'" \
	"  value \\tcontext.trigger\\ =" \
	"    method trigger" \
	"      owner: tcontext"
do
	if ! rg -Fq "$required" "$tmp/stderr"
	then
		echo "incomplete enclosing diagnostic context: $required" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

where_count=$(rg -c '^  where$' "$tmp/stderr")
if test "$where_count" -ne 1
then
	echo "diagnostic emitted $where_count where blocks instead of one" >&2
	sed -n '1,180p' "$tmp/stderr" >&2
	exit 1
fi

for forbidden in \
	"<unregistered" \
	"value unit_reference =" \
	"value system =" \
	"unrelated:"
do
	if rg -Fq "$forbidden" "$tmp/stderr"
	then
		echo "diagnostic graph emitted forbidden text: $forbidden" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

if ./mp -Furtl \
	-o"$tmp/diagnostic_context_rich_rejected.cc" \
	tests/diagnostic_context_rich_rejected.pp \
	>"$tmp/rich.stdout" 2>"$tmp/rich.stderr"
then
	echo "accepted invalid assignment in method body" >&2
	exit 1
fi

for required in \
	"no implicit conversion" \
	"  context:" \
	"    program: diagnosticcontextrichrejected" \
	"    routine: \\tcontext.trigger\\" \
	"    owner type: tcontext"
do
	if ! rg -Fq "$required" "$tmp/rich.stderr"
	then
		echo "incomplete rich diagnostic context: $required" >&2
		sed -n '1,180p' "$tmp/rich.stderr" >&2
		exit 1
	fi
done

rich_where_count=$(rg -c '^  where$' "$tmp/rich.stderr")
if test "$rich_where_count" -ne 1
then
	echo "rich diagnostic emitted $rich_where_count where blocks instead of one" >&2
	sed -n '1,180p' "$tmp/rich.stderr" >&2
	exit 1
fi
if rg -Fq "<unregistered" "$tmp/rich.stderr"
then
	echo "rich diagnostic emitted an unregistered graph reference" >&2
	sed -n '1,180p' "$tmp/rich.stderr" >&2
	exit 1
fi

echo "diagnostic context tests passed"
