#!/bin/sh
set -eu
. "$(dirname -- "$0")/testlib.sh"
ulimit -c 0

tpcc_translate -o"$tmp/main.cc" tests/sysutils_fileops.pp
tpcc_build "$tmp/main" "$tmp"/*.cc
(cd "$tmp" && tpcc_run "$tmp/main") >"$tmp/stdout"
if [ "$(cat "$tmp/stdout")" != 'sysutils file ops ok' ]; then
  printf 'SysUtils file ops failed\n' >&2
  cat "$tmp/stdout" >&2
  exit 1
fi
printf 'SysUtils file operations tests passed\n'
