#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
	EXIT HUP INT TERM


tpcc_translate -o"$tmp/io_checking.cc" \
	tests/io_checking.pp

tpcc_build "$tmp/io_checking" \
	"$tmp/io_checking.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/io_checking"

echo "I/O checking tests passed"
