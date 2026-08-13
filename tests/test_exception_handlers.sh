#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/exception_handlers.cc" \
	tests/exception_handlers.pp
tpcc_build "$tmp/exception_handlers" \
	"$tmp/exception_handlers.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/exception_handlers"

tpcc_translate -o"$tmp/unhandled_exception.cc" \
	tests/unhandled_exception.pp
tpcc_build "$tmp/unhandled_exception" \
	"$tmp/unhandled_exception.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"
status=0
tpcc_run "$tmp/unhandled_exception" \
	>"$tmp/unhandled.stdout" || status=$?
if test "$status" -ne 217; then
	echo "unhandled Pascal exception returned $status, expected 217" >&2
	exit 1
fi
if ! grep -Fq 'Unhandled Pascal exception' \
	"$tmp/unhandled.stdout"; then
	echo "SysUtils unhandled-exception hook did not run" >&2
	exit 1
fi

for source in \
	tests/goto_out_of_exception_block.pp \
	tests/goto_into_exception_block.pp \
	tests/reraise_outside_handler.pp \
	tests/reraise_inside_nested_try.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
	    >"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! grep -Fq -- "$expected" "$tmp/stderr"; then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "exception-handler tests passed"
