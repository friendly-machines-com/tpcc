#!/bin/sh
set -eu
. "$(dirname -- "$0")/testlib.sh"
ulimit -c 0

tpcc_translate -o"$tmp/main.cc" tests/freeandnil.pp
tpcc_build "$tmp/main" "$tmp"/*.cc
for case in nil normal raising; do
  tpcc_run "$tmp/main" "$case" >"$tmp/stdout"
  if [ "$(cat "$tmp/stdout")" != 'freeandnil ok' ]; then
    printf 'wrong FreeAndNil result for %s\n' "$case" >&2
    exit 1
  fi
done
# The builtin passes actual class-reference storage, never an erased buffer.
if grep -Fq 'p_freeandnil(::u_system::tpcc_make_storage_ref' "$tmp/main.cc"; then
  printf 'FreeAndNil erased the class-reference type\n' >&2
  exit 1
fi
for case in INTEGER_ARG POINTER_ARG RECORD_ARG METACLASS_ARG NIL_ARG TEMPORARY_ARG PROPERTY_ARG ORDINARY_VAR; do
  if tpcc_translate -d"$case" -o"$tmp/rejected.cc" tests/freeandnil_rejected.pp >"$tmp/rejected.log" 2>&1; then
    printf 'accepted unsafe FreeAndNil/var argument: %s\n' "$case" >&2
    exit 1
  fi
  if ! grep -Eq 'no matching overload|not a storage-backed expression' "$tmp/rejected.log"; then
    printf 'wrong rejection for %s\n' "$case" >&2
    cat "$tmp/rejected.log" >&2
    exit 1
  fi
done
printf 'FreeAndNil runtime and argument-checking tests passed\n'
