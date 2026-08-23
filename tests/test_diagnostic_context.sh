#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


if tpcc_translate -Futests \
	-o"$tmp/diagnostic_context_rejected.cc" \
	tests/diagnostic_context_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted program with an unresolved method-body identifier" >&2
	exit 1
fi

for required in \
	"unresolved value identifier: missingname" \
	"  program: diagnosticcontextrejected" \
	"  routine: \\tcontext.trigger\\" \
	"  value diagnosticcontextrejected =" \
	"    unit_reference" \
	"      unit: 'diagnosticcontextrejected'" \
	"  value \\tcontext.trigger\\ =" \
	"    method trigger" \
	"      owner: tcontext"
do
	if ! grep -Fq "$required" "$tmp/stderr"
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
	"  context:" \
	"  owner type:" \
	"value unit_reference =" \
	"value system =" \
	"unrelated:"
do
	if grep -Fq "$forbidden" "$tmp/stderr"
	then
		echo "diagnostic graph emitted forbidden text: $forbidden" >&2
		sed -n '1,180p' "$tmp/stderr" >&2
		exit 1
	fi
done

if tpcc_translate \
	-o"$tmp/diagnostic_context_rich_rejected.cc" \
	tests/diagnostic_context_rich_rejected.pp \
	>"$tmp/rich.stdout" 2>"$tmp/rich.stderr"
then
	echo "accepted invalid assignment in method body" >&2
	exit 1
fi

for required in \
	"no implicit conversion" \
	"  program: diagnosticcontextrichrejected" \
	"  routine: \\tcontext.trigger\\"
do
	if ! grep -Fq "$required" "$tmp/rich.stderr"
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
if grep -Fq "<unregistered" "$tmp/rich.stderr"
then
	echo "rich diagnostic emitted an unregistered graph reference" >&2
	sed -n '1,180p' "$tmp/rich.stderr" >&2
	exit 1
fi
if grep -Fq "  context:" "$tmp/rich.stderr" ||
   grep -Fq "  owner type:" "$tmp/rich.stderr"
then
	echo "rich diagnostic emitted a redundant context section or owner reference" >&2
	sed -n '1,180p' "$tmp/rich.stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/diagnostic_owner_context_rejected.cc" \
	tests/diagnostic_owner_context_rejected.pp \
	>"$tmp/owner.stdout" 2>"$tmp/owner.stderr"
then
	echo "accepted an unresolved field type" >&2
	exit 1
fi

for required in \
	"unresolved type identifier: missingtype" \
	"  program: diagnosticownercontextrejected" \
	"  owner type: tcontext" \
	"  type tcontext ="
do
	if ! grep -Fq "$required" "$tmp/owner.stderr"
	then
		echo "incomplete owner-type diagnostic context: $required" >&2
		sed -n '1,180p' "$tmp/owner.stderr" >&2
		exit 1
	fi
done

owner_where_count=$(rg -c '^  where$' "$tmp/owner.stderr")
if test "$owner_where_count" -ne 1
then
	echo "owner diagnostic emitted $owner_where_count where blocks instead of one" >&2
	sed -n '1,180p' "$tmp/owner.stderr" >&2
	exit 1
fi
for forbidden in \
	"<unregistered" \
	"  context:" \
	"  routine:"
do
	if grep -Fq "$forbidden" "$tmp/owner.stderr"
	then
		echo "owner diagnostic emitted forbidden text: $forbidden" >&2
		sed -n '1,180p' "$tmp/owner.stderr" >&2
		exit 1
	fi
done

if tpcc_translate \
	-o"$tmp/unresolved_function_result_type_rejected.cc" \
	tests/unresolved_function_result_type_rejected.pp \
	>"$tmp/result-type.stdout" 2>"$tmp/result-type.stderr"
then
	echo "accepted an unresolved function result type" >&2
	exit 1
fi
if ! grep -Fq "unresolved type identifier: missingresulttype" \
	"$tmp/result-type.stderr"
then
	echo "wrong diagnostic for an unresolved function result type" >&2
	sed -n '1,180p' "$tmp/result-type.stderr" >&2
	exit 1
fi
if ! grep -Fq "unresolved type identifier: missingmethodresulttype" \
	"$tmp/result-type.stderr"
then
	echo "wrong diagnostic for an unresolved method result type" >&2
	sed -n '1,180p' "$tmp/result-type.stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/recovery_poison_rejected.cc" \
	tests/recovery_poison_rejected.pp \
	>"$tmp/recovery.stdout" 2>"$tmp/recovery.stderr"
then
	echo "accepted malformed input intended to exercise poison recovery" >&2
	exit 1
fi

# These independent diagnostics occur after earlier poisoned declarations and
# expressions. Reaching all of them proves that recovery preserved parser
# structure instead of returning null, indexing a missing formal, or
# reinterpreting a malformed custom enumerator as native sequence iteration.
for required in \
	"postfix type dereference: expected pointer type" \
	"shortstring capacity must be a constant integer" \
	"subrange bounds must be constant expressions" \
	"explicit enum value must be constant" \
	"read accessor for property 'value' has the wrong number of parameters" \
	"unresolved lvalue identifier: missingdestination" \
	"break outside loop" \
	"missing argument for parameter 'value'" \
	"for-in collection: expected string, array, or set type" \
	"GetEnumerator result must be a record, object, class, or interface" \
	"with target: expected class, record, or object type" \
	"New first operand: expected typed pointer type" \
	"no implicit conversion to the required type"
do
	if ! grep -Fq "$required" "$tmp/recovery.stderr"
	then
		echo "poison recovery stopped before diagnostic: $required" >&2
		sed -n '1,260p' "$tmp/recovery.stderr" >&2
		exit 1
	fi
done

echo "diagnostic context tests passed"
