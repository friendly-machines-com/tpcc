#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/errorcode_builtin.cc" \
	tests/errorcode_builtin.pp
tpcc_build "$tmp/errorcode_builtin" \
	tests/errorcode_builtin_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/errorcode_builtin"

for builtin in halt runerror
do
	tpcc_translate -o"$tmp/${builtin}_builtin.cc" \
		"tests/${builtin}_builtin.pp"
	tpcc_build "$tmp/${builtin}_builtin" \
		"$tmp/${builtin}_builtin.cc" \
		"$tmp/system.cc"
done

set +e
"$tmp/halt_builtin"
halt_status=$?
"$tmp/runerror_builtin"
runerror_status=$?
set -e

if [ "$halt_status" -ne 7 ]; then
	echo "Halt returned status $halt_status instead of 7" >&2
	exit 1
fi
if [ "$runerror_status" -ne 9 ]; then
	echo "RunError returned status $runerror_status instead of 9" >&2
	exit 1
fi

echo "ErrorCode/RunError/Halt tests passed"
