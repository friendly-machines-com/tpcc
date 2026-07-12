#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-runerror-halt-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

for builtin in halt runerror
do
	./mp -Furtl -o"$tmp/${builtin}_builtin.cc" \
		"tests/${builtin}_builtin.pp"
	"${CXX:-g++}" \
		-std=c++20 \
		-Wall \
		-Wextra \
		-Irtl \
		"$tmp/${builtin}_builtin.cc" \
		rtl/system.cc \
		-o "$tmp/${builtin}_builtin"
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

echo "RunError/Halt tests passed"
