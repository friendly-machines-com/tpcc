#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_build_native "$tmp/child with space" \
	tests/execute_process_child.cpp

tpcc_translate -o"$tmp/execute_process.cc" \
	tests/execute_process.pp
for required in \
	'::u_sysutils::p_executeprocess_commandline' \
	'::u_sysutils::p_executeprocess_arguments'
do
	if ! grep -Fq "$required" "$tmp/sysutils.cc"
	then
		echo "ExecuteProcess did not use $required" >&2
		exit 1
	fi
done

tpcc_build "$tmp/execute_process" \
	"$tmp/execute_process.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

TPCC_EXECUTE_PROCESS_CHILD="$tmp/child with space" \
TPCC_EXECUTE_PROCESS_INHERITED=present \
tpcc_run \
	"$tmp/execute_process"

echo "ExecuteProcess tests passed"
