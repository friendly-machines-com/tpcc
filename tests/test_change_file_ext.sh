#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/change_file_ext.cc" \
	tests/change_file_ext.pp

tpcc_build "$tmp/change_file_ext" \
	"$tmp/change_file_ext.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

tpcc_run "$tmp/change_file_ext"

echo "ChangeFileExt tests passed"
