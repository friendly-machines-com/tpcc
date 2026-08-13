#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/unix_fpsystem.cc" \
	tests/unix_fpsystem.pp

if ! rg -Fq '::u_unix::p_fpsystem' \
	"$tmp/unix_fpsystem.cc"
then
	echo "Unix.FpSystem did not use p_fpsystem" >&2
	exit 1
fi

tpcc_build "$tmp/unix_fpsystem" \
	"$tmp/unix_fpsystem.cc" \
	"$tmp/unix.cc" \
	"$tmp/system.cc"

TPCC_FPSYSTEM_INHERITED=inherited \
tpcc_run \
	"$tmp/unix_fpsystem"

echo "Unix.FpSystem tests passed"
