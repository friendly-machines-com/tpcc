#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/work/directory"

printf regular >"$tmp/work/regular"
printf target >"$tmp/work/target"
ln -s target "$tmp/work/file-link"
ln -s missing "$tmp/work/broken-link"


tpcc_translate -o"$tmp/delete_file.cc" tests/delete_file.pp

if ! rg -Fq '::u_sysutils::p_deletefile(' "$tmp/delete_file.cc"; then
	echo "SysUtils.DeleteFile did not resolve in the SysUtils namespace" >&2
	exit 1
fi

tpcc_build "$tmp/delete_file" \
	"$tmp/delete_file.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

cd "$tmp/work"
tpcc_run \
	"$tmp/delete_file"

test ! -e regular
test ! -L file-link
test ! -L broken-link
test -f target
test -d directory

echo "DeleteFile tests passed"
