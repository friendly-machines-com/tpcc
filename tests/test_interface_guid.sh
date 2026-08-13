#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/interface_guid.cc" \
	tests/interface_guid.pp

if ! rg -Fq 'virtual void p_base() = 0;' \
	"$tmp/interface_guid.cc" ||
   ! rg -Fq 'virtual void p_child() = 0;' \
	"$tmp/interface_guid.cc"
then
	echo "GUID clause changed interface member emission" >&2
	exit 1
fi

tpcc_build "$tmp/interface_guid" \
	"$tmp/interface_guid.cc" \
	"$tmp/system.cc"
"$tmp/interface_guid"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/interface_guid_nonstring_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted a non-string interface GUID clause" >&2
	exit 1
fi
expected=$(sed -n '1p' \
	tests/interface_guid_nonstring_rejected.error)
if ! rg -Fq -- "$expected" "$tmp/stderr"
then
	echo "wrong non-string GUID diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "interface GUID tests passed"
