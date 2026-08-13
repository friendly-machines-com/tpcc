#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_build "$tmp/file_error_state" \
	tests/file_error_state_runtime.cpp

tpcc_run \
	"$tmp/file_error_state"

echo "file error-state tests passed"
