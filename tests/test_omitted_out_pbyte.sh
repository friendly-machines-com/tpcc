#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/omitted_out_pbyte.cc" \
	tests/omitted_out_pbyte.pp

if ! grep -Fq \
	'::u_system::tpcc_omitted_out_pbyte(p_buffer)' \
	"$tmp/omitted_out_pbyte.cc"
then
	echo "omitted out formal did not expose the caller's Byte storage" >&2
	exit 1
fi
if grep -Fq 'std::addressof(p_buffer)' "$tmp/omitted_out_pbyte.cc"
then
	echo "omitted out formal exposed its storage-ref descriptor" >&2
	exit 1
fi

tpcc_build "$tmp/omitted_out_pbyte" \
	"$tmp/omitted_out_pbyte.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run "$tmp/omitted_out_pbyte")
expected='42
17'
if test "$actual" != "$expected"
then
	echo "unexpected omitted-out PByte result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

for rejection in WORD_POINTER VAR_FORMAL EXPLICIT_POINTER
do
	if tpcc_translate -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" tests/omitted_out_pbyte.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted unsupported omitted-formal pointer variant: $rejection" >&2
		exit 1
	fi
done

echo "omitted out PByte tests passed"
