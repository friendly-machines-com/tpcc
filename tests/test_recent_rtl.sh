#!/bin/sh
set -eu
. "$(dirname -- "$0")/testlib.sh"
# Keep all cases independent: one broken routine must not hide failures in the
# other recent additions. Compilation, linking, stdout and exit status are all
# checked, not just successful translation.
ulimit -c 0
failures=0
build_case()
{
  name=$1
  dir="$tmp/$name"
  mkdir -p "$dir"
  if ! tpcc_translate -o"$dir/main.cc" "tests/recent_rtl/$name.pp" >"$dir/build.log" 2>&1; then
    printf 'FAIL %s: translation\n' "$name"
    sed -n '1,12p' "$dir/build.log"
    failures=$((failures + 1))
    return 1
  fi
  if ! tpcc_build "$dir/main" -I"$dir" -include tests/recent_rtl/math_inputs.h "$dir"/*.cc >>"$dir/build.log" 2>&1; then
    printf 'FAIL %s: generated C++ build\n' "$name"
    tail -n 16 "$dir/build.log"
    failures=$((failures + 1))
    return 1
  fi
}
run_case()
{
  name=$1; arg=$2; expected=$3; output=$4
  dir="$tmp/$name"
  work="$dir/run-$arg"
  mkdir -p "$work"
  status=0
  (cd "$work"; tpcc_run "$dir/main" "$arg") >"$work/stdout" 2>"$work/stderr" || status=$?
  if [ "$status" != "$expected" ] || [ "$(cat "$work/stdout")" != "$output" ]; then
    printf 'FAIL %s/%s: status %s (expected %s)\n' "$name" "$arg" "$status" "$expected"
    printf 'stdout: %s\n' "$(cat "$work/stdout")"
    sed -n '1,8p' "$work/stderr"
    failures=$((failures + 1))
  else
    printf 'PASS %s/%s\n' "$name" "$arg"
  fi
}
if build_case values; then run_case values all 0 'values ok'; fi
if build_case bits; then run_case bits all 0 'bits ok'; fi
if build_case math_values; then run_case math_values all 0 ''; fi
if build_case method_codes; then run_case method_codes all 0 ''; fi
if build_case strong_word; then run_case strong_word all 0 ''; fi
if build_case strong_word_checked; then run_case strong_word_checked all 201 ''; fi
mkdir -p "$tmp/method-rejected"
if tpcc_translate -o"$tmp/method-rejected/main.cc" tests/recent_rtl/method_call_rejected.pp >"$tmp/method-rejected/log" 2>&1; then
  printf 'FAIL method reference: instance call through class accepted\n'
  failures=$((failures + 1))
elif ! grep -q 'class methods' "$tmp/method-rejected/log"; then
  printf 'FAIL method reference: wrong rejection diagnostic\n'
  failures=$((failures + 1))
else
  printf 'PASS method reference rejects missing receiver\n'
fi
if build_case paths; then
  for case in extension scan scan-nul separators compare; do run_case paths "$case" 0 ''; done
fi
if build_case exits; then
  run_case exits normal 7 ''
  run_case exits halt 9 ''
  run_case exits chain 0 "$(printf 'first\nlast')"
  run_case exits halt-chain 9 "$(printf 'first\nlast')"
  run_case exits observe 9 '9'
  run_case exits change 23 ''
  run_case exits error 201 ''
  run_case exits output 0 'output ok'
  run_case exits assert-ok 0 ''
  # Failed assertions must never signal success; don't prescribe the host's
  # signal-vs-runtime-error representation here.
  for case in assert-fail assert-message; do
    status=0
    tpcc_run "$tmp/exits/main" "$case" >"$tmp/assert.out" 2>"$tmp/assert.err" || status=$?
    if [ "$status" = 0 ]; then
      printf 'FAIL exits/%s: assertion returned success\n' "$case"
      failures=$((failures + 1))
    else
      printf 'PASS exits/%s (status %s)\n' "$case" "$status"
    fi
  done
fi
if build_case files; then
  for case in append append-missing erase erase-open mkdir mkdir-error; do run_case files "$case" 0 ''; done
  if [ "$(cat "$tmp/files/run-append/append.txt")" != "$(printf 'first\nsecond')" ] ||
     [ -e "$tmp/files/run-erase/text.txt" ] || [ -e "$tmp/files/run-erase/binary.dat" ] ||
     [ ! -d "$tmp/files/run-mkdir/directory" ]; then
    printf 'FAIL files: filesystem contents\n'; failures=$((failures + 1))
  fi
fi
if build_case unix_ops; then
  for case in chmod signal text-write text-read file-read text-status file-status; do run_case unix_ops "$case" 0 ''; done
  if [ "$(stat -c %a "$tmp/unix_ops/run-chmod/mode.txt")" != 600 ] ||
     [ "$(cat "$tmp/unix_ops/run-text-write/pipe.txt")" != 'pipe data' ]; then
    printf 'FAIL unix_ops: permissions or pipe contents\n'; failures=$((failures + 1))
  fi
fi
if [ "$failures" != 0 ]; then
  printf '%s recent RTL regression checks failed\n' "$failures"
  exit 1
fi
printf 'recent RTL runtime tests passed\n'
