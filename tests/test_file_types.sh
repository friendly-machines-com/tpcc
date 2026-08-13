#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/file_types.cc" tests/file_types.pp

if ! rg -Fq '::u_system::t_file p_binaryfile;' "$tmp/file_types.cc"
then
	echo "File did not emit the untyped binary-file carrier" >&2
	exit 1
fi
if ! rg -Fq '::u_system::t_text p_textalias;' "$tmp/file_types.cc"
then
	echo "TextFile did not alias Text" >&2
	exit 1
fi
if ! rg -Fq \
	'::u_system::t_typedfile<::u_system::t_integer> p_integers;' \
	"$tmp/file_types.cc"
then
	echo "file of Integer did not emit a parameterized typed-file carrier" >&2
	exit 1
fi
if ! rg -Fq \
	'::u_system::t_typedfile<t_tnode> p_nodes;' \
	"$tmp/file_types.cc"
then
	echo "typed-file element type was not emitted" >&2
	exit 1
fi

tpcc_build "$tmp/file_types" \
	"$tmp/file_types.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/file_types"

for source in \
	tests/file_types_mismatch.pp \
	tests/file_types_identity_mismatch.pp
do
	if tpcc_translate -o"$tmp/mismatch.cc" \
		"$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted incompatible typed-file var argument: $source" >&2
		exit 1
	fi
	if ! rg -Fq 'no matching overload' "$tmp/stderr"
	then
		echo "wrong diagnostic for incompatible typed files: $source" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "file type tests passed"
