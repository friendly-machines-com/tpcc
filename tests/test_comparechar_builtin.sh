#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/comparechar_builtin.cc" tests/comparechar_builtin.pp

if ! grep -Eq '::u_system::p_comparechar' "$tmp/comparechar_builtin.cc"; then
	echo "CompareChar did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Eq '::u_system::p_comparebyte' "$tmp/comparechar_builtin.cc"; then
	echo "CompareByte did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Eq '::u_system::p_indexbyte' "$tmp/comparechar_builtin.cc"; then
	echo "IndexByte did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Eq '::u_system::p_indexword' "$tmp/comparechar_builtin.cc"; then
	echo "IndexWord did not lower through the RTL" >&2
	exit 1
fi
if ! grep -Eq '::u_system::p_compareword' "$tmp/comparechar_builtin.cc"; then
	echo "CompareWord did not lower through the RTL" >&2
	exit 1
fi

tpcc_build "$tmp/comparechar_builtin_pascal" \
	"$tmp/comparechar_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/comparechar_builtin_pascal"

tpcc_build "$tmp/comparechar_builtin" \
	tests/comparechar_builtin_runtime.cpp
tpcc_run "$tmp/comparechar_builtin"

echo "CompareChar/CompareByte/CompareWord/IndexByte/IndexWord builtin tests passed"
