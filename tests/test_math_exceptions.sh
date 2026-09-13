#!/bin/sh
set -eu
. "$(dirname -- "$0")/testlib.sh"
ulimit -c 0

tpcc_translate -o"$tmp/main.cc" tests/mathexceptions.pp
tpcc_build "$tmp/main" "$tmp"/*.cc
tpcc_run "$tmp/main" >"$tmp/stdout"
if [ "$(cat "$tmp/stdout")" != 'math exceptions ok' ]; then
  printf 'Math FPU exception mask tests failed\n' >&2
  cat "$tmp/stdout" >&2
  exit 1
fi
printf 'Math FPU exception mask tests passed\n'
