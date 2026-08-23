#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate -o"$tmp/widechar_type.cc" \
	tests/widechar_type.pp
tpcc_build "$tmp/widechar_type" \
	"$tmp/widechar_type.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/widechar_type"

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/widechar_word_var_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted WideChar storage as a var Word actual" >&2
	exit 1
fi
if ! grep -Fq "no matching overload for 'takeword'" "$tmp/stderr"
then
	echo "wrong WideChar/Word identity diagnostic" >&2
	sed -n '1,40p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/char_conversion.cc" \
	tests/widechar_char_conversion_unimplemented.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted Char-to-WideChar without an encoding conversion" >&2
	exit 1
fi
if ! grep -Fq "no implicit conversion" "$tmp/stderr"
then
	echo "wrong unsupported Char-to-WideChar diagnostic" >&2
	sed -n '1,40p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/packed_array.cc" \
	tests/widechar_packed_array_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an array of naturally aligned WideChar inside a packed record" >&2
	exit 1
fi
if ! grep -Fq "tests/widechar_packed_array_rejected.pp(7): error: packed record type 'tpacket' field 'values'" \
	"$tmp/stderr" ||
	! grep -Fq "requires alignment 2; arrays stored directly inside packed records require element alignment 1" \
	"$tmp/stderr"
then
	echo "wrong packed WideChar array diagnostic" >&2
	sed -n '1,40p' "$tmp/stderr" >&2
	exit 1
fi

echo "WideChar type tests passed"
