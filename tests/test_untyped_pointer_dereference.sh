#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/untyped_pointer_dereference.cc" \
	tests/untyped_pointer_dereference.pp

if ! rg -Fq \
	'::u_system::tpcc_dereference_storage(p_rawdestination)' \
	"$tmp/untyped_pointer_dereference.cc"
then
	echo "untyped Pointer^ did not produce a raw storage place" >&2
	exit 1
fi
if rg -Fq '*p_rawdestination' \
	"$tmp/untyped_pointer_dereference.cc"
then
	echo "untyped Pointer^ was emitted as C++ unary dereference" >&2
	exit 1
fi

tpcc_build "$tmp/untyped_pointer_dereference" \
	"$tmp/untyped_pointer_dereference.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run \
	"$tmp/untyped_pointer_dereference")
expected='42
17'
if test "$actual" != "$expected"
then
	echo "unexpected untyped Pointer^ result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "untyped pointer dereference tests passed"
