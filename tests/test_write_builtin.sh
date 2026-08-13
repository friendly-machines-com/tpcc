#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/write_builtin.cc" tests/write_builtin.pp
if ! rg -Fq '::u_system::p_write(' "$tmp/write_builtin.cc"; then
	echo "Write did not lower through the RTL" >&2
	exit 1
fi
if ! rg -Fq '::u_system::p_writeln(' "$tmp/write_builtin.cc"; then
	echo "WriteLn did not lower through the RTL" >&2
	exit 1
fi
if ! rg -Fq '::u_system::tpcc_make_formatted_value(' "$tmp/write_builtin.cc"; then
	echo "formatted output arguments lost their RTL descriptors" >&2
	exit 1
fi
if ! rg -Fq '::u_system::p_stdout' "$tmp/write_builtin.cc"; then
	echo "System.StdOut did not bind to the RTL standard-output Text" >&2
	exit 1
fi
if ! rg -Fq '::u_system::p_stderr' "$tmp/write_builtin.cc"; then
	echo "System.StdErr did not bind to the RTL standard-error Text" >&2
	exit 1
fi

tpcc_build "$tmp/write_builtin" \
	tests/write_builtin_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/write_builtin"

echo "Write/WriteLn builtin tests passed"
