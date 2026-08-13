#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/normal" "$tmp/partial"


tpcc_translate -Futests/unit_lifecycle \
	-o"$tmp/normal/lifecycle.cc" \
	tests/unit_lifecycle/lifecycle.pp

tpcc_build "$tmp/normal/lifecycle" \
	-I"$tmp/normal" \
	"$tmp/normal"/*.cc \
	tests/unit_lifecycle/system_stub.cc

tpcc_run \
	"$tmp/normal/lifecycle" >"$tmp/normal/output"
if [ "$(cat "$tmp/normal/output")" != "ABECDMdcrseba" ]; then
	echo "wrong initialization/finalization order:" >&2
	cat "$tmp/normal/output" >&2
	exit 1
fi

tpcc_translate -Futests/unit_lifecycle \
	-o"$tmp/partial/partial.cc" \
	tests/unit_lifecycle/partial.pp

tpcc_build "$tmp/partial/partial" \
	-I"$tmp/partial" \
	"$tmp/partial"/*.cc \
	tests/unit_lifecycle/system_stub.cc

set +e
tpcc_run \
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
