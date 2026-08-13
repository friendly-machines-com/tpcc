#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p \
	"$tmp/work/short-empty" \
	"$tmp/work/ansi-empty" \
	"$tmp/work/pending-empty" \
	"$tmp/work/nonempty/child"
printf data >"$tmp/work/regular-file"


tpcc_translate -o"$tmp/rmdir.cc" tests/rmdir.pp
if ! rg -Fq '::u_system::p_rmdir' "$tmp/rmdir.cc"
then
	echo "checked RmDir did not use its System RTL operation" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_unchecked_rmdir' "$tmp/rmdir.cc"
then
	echo "unchecked RmDir did not use its System RTL operation" >&2
	exit 1
fi

tpcc_build "$tmp/rmdir" \
	"$tmp/rmdir.cc" \
	"$tmp/sysutils.cc" \
	"$tmp/system.cc"

cd "$tmp/work"
tpcc_run "$tmp/rmdir"

echo "RmDir tests passed"
