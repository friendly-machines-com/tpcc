#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


# One run must report every semantic error, not only the first, and exit
# nonzero through the error counter. This covers both the type-family
# diagnostics and the overload-resolution family.
if tpcc_translate -o"$tmp/multi_error_reporting.cc" \
    tests/multi_error_reporting.pp \
    >"$tmp/multi_error_reporting.out" 2>&1
then
	echo "semantic errors were accepted" >&2
	exit 1
fi

for expected in \
	"invalid explicit conversion: expected type integer but got type shortstring" \
	"invalid explicit conversion: expected type integer but got type untyped_real" \
	"no matching overload for 'p'"
do
	if ! grep -Fq "$expected" "$tmp/multi_error_reporting.out"; then
		echo "missing error report: $expected" >&2
		cat "$tmp/multi_error_reporting.out" >&2
		exit 1
	fi
done

p_reports=$(grep -Fc "no matching overload for 'p'" "$tmp/multi_error_reporting.out")
if [ "$p_reports" -lt 2 ]; then
	echo "overload errors after the first were not reported" >&2
	exit 1
fi

echo "multi-error reporting tests passed"
