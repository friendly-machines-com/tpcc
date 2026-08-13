#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


if ! tpcc_translate -o"$tmp/ordinary_external.cc" \
	tests/ordinary_external.pp \
	>"$tmp/compile.out" 2>"$tmp/compile.err"
then
	echo "ordinary external failed to compile" >&2
	sed -n '1,80p' "$tmp/compile.err" >&2
	exit 1
fi

# An arbitrary external is an ordinary callable.  It owns its emitted name
# and has no BuiltinDesc; parsing its later call must not read fabricated
# compiler metadata.
if ! grep -Fq '::foreign_provider::p_value(' \
	"$tmp/ordinary_external.cc"
then
	echo "ordinary external name was not preserved" >&2
	exit 1
fi

if grep -Fq "doesn't have a registration" "$tmp/compile.err"
then
	echo "ordinary external was incorrectly treated as a compiler builtin" >&2
	exit 1
fi

echo "ordinary external test passed"
