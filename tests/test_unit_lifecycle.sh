#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-unit-lifecycle-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/normal" "$tmp/partial"

cd "$root"

./mp -Furtl -Futests/unit_lifecycle \
	-o"$tmp/normal/lifecycle.cc" \
	tests/unit_lifecycle/lifecycle.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-I"$tmp/normal" \
	-Irtl \
	"$tmp/normal"/*.cc \
	tests/unit_lifecycle/system_stub.cc \
	-o "$tmp/normal/lifecycle"

ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/normal/lifecycle" >"$tmp/normal/output"
if [ "$(cat "$tmp/normal/output")" != "ABECDMdceba" ]; then
	echo "wrong initialization/finalization order:" >&2
	cat "$tmp/normal/output" >&2
	exit 1
fi

./mp -Furtl -Futests/unit_lifecycle \
	-o"$tmp/partial/partial.cc" \
	tests/unit_lifecycle/partial.pp

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-I"$tmp/partial" \
	-Irtl \
	"$tmp/partial"/*.cc \
	tests/unit_lifecycle/system_stub.cc \
	-o "$tmp/partial/partial"

set +e
ASAN_OPTIONS=detect_leaks=1 \
	"$tmp/partial/partial" >"$tmp/partial/output"
status=$?
set -e
if [ "$status" -ne 7 ]; then
	echo "Halt during initialization returned status $status instead of 7" >&2
	exit 1
fi
if [ "$(cat "$tmp/partial/output")" != "AXa" ]; then
	echo "a partially initialized unit was finalized:" >&2
	cat "$tmp/partial/output" >&2
	exit 1
fi

echo "unit lifecycle tests passed"
