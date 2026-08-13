#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/typed_const_aggregates.cc" \
	tests/typed_const_aggregates.pp

for expected in \
	't_touter p_outer = [](' \
	'::u_system::tpcc_make_set<t_tflag>' \
	'p_packedvalues = {{[](' \
	'tpcc_record.m_set_p_code' \
	'tpcc_record.m_set_p_value' \
	'::u_system::tpcc_ansistring_literal(' \
	'::u_system::o_implicit(' \
	'::u_system::tpcc_shortstring_cast<3>'
do
	if ! grep -Fq "$expected" "$tmp/typed_const_aggregates.cc"; then
		echo "missing typed-constant lowering: $expected" >&2
		exit 1
	fi
done

tpcc_build "$tmp/typed_const_aggregates" \
	"$tmp/typed_const_aggregates.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/typed_const_aggregates"

if tpcc_translate \
	-o"$tmp/custom_conversion_constant_rejected.cc" \
	tests/custom_conversion_constant_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "user-written conversion body was executed during constant evaluation" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/custom_conversion_constant_rejected.error)
if ! grep -Fq "$expected" "$tmp/stderr"
then
	echo "wrong custom-conversion constant diagnostic; expected: $expected" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/character_array_string_too_long.cc" \
	tests/character_array_string_too_long.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "overlong character-array string initializer was accepted" >&2
	exit 1
fi
if ! grep -Fq \
	"string length is larger than character-array length" \
	"$tmp/stderr"
then
	echo "wrong overlong character-array diagnostic" >&2
	sed -n '1,80p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate \
	-o"$tmp/byte_array_string_initializer_rejected.cc" \
	tests/byte_array_string_initializer_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "string shortcut was incorrectly accepted for array of Byte" >&2
	exit 1
fi

echo "typed aggregate constant tests passed"
