#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/same" "$tmp/merge" "$tmp/shadow" "$tmp/legacy"


tpcc_translate \
	-o"$tmp/same/main.cc" \
	tests/global_overload_scope/same_scope.pp
tpcc_build "$tmp/same/main" \
	-I"$tmp/same" \
	"$tmp/same"/*.cc
tpcc_run "$tmp/same/main"

tpcc_translate -Futests/global_overload_scope \
	-o"$tmp/merge/main.cc" \
	tests/global_overload_scope/merge.pp
tpcc_build "$tmp/merge/main" \
	-I"$tmp/merge" \
	"$tmp/merge"/*.cc
tpcc_run "$tmp/merge/main"

if tpcc_translate -Futests/global_overload_scope \
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

tpcc_translate \
	-o"$tmp/legacy/main.cc" \
	tests/global_overload_scope/legacy_shadow.pp
tpcc_build "$tmp/legacy/main" \
	-I"$tmp/legacy" \
	"$tmp/legacy"/*.cc
tpcc_run "$tmp/legacy/main"

if tpcc_translate \
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
