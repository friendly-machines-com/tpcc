#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/omitted_out_byte_pointer.cc" \
	tests/omitted_out_byte_pointer.pp

if ! grep -Fq \
	'::u_system::tpcc_omitted_formal_byte_pointer<::u_system::t_byte>(p_buffer)' \
	"$tmp/omitted_out_byte_pointer.cc"
then
	echo "omitted out formal did not expose the caller's Byte storage" >&2
	exit 1
fi
if ! grep -Fq \
	'::u_system::tpcc_omitted_formal_byte_pointer<::u_system::t_char>(p_buffer)' \
	"$tmp/omitted_out_byte_pointer.cc"
then
	echo "omitted out formal did not expose the caller's AnsiChar storage" >&2
	exit 1
fi
if test "$(grep -Fc \
	'::u_system::tpcc_omitted_formal_byte_pointer<::u_system::t_byte>(p_buffer)' \
	"$tmp/omitted_out_byte_pointer.cc")" -lt 4
then
	echo "omitted const formal did not expose the caller's Byte storage" >&2
	exit 1
fi
if grep -Fq 'std::addressof(p_buffer)' "$tmp/omitted_out_byte_pointer.cc"
then
	echo "omitted out formal exposed its storage-ref descriptor" >&2
	exit 1
fi

tpcc_build "$tmp/omitted_out_byte_pointer" \
	"$tmp/omitted_out_byte_pointer.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run "$tmp/omitted_out_byte_pointer")
expected='42
17
65
66
101'
if test "$actual" != "$expected"
then
	echo "unexpected omitted-out byte-pointer result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

for rejection in WORD_POINTER CONST_WORD_POINTER VAR_FORMAL EXPLICIT_POINTER
do
	if tpcc_translate -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" tests/omitted_out_byte_pointer.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted unsupported omitted-formal pointer variant: $rejection" >&2
		exit 1
	fi
done

echo "omitted out byte-pointer tests passed"
