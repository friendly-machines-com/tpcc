#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
	EXIT HUP INT TERM


tpcc_build "$tmp/file_lifecycle" \
	tests/file_lifecycle_runtime.cpp

tpcc_run \
	"$tmp/file_lifecycle"

echo "file lifecycle tests passed"
