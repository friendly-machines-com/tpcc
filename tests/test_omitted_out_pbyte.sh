#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-omitted-out-pbyte-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/omitted_out_pbyte.cc" \
	tests/omitted_out_pbyte.pp

if ! rg -Fq \
	'::u_system::tpcc_omitted_out_pbyte(p_buffer)' \
	"$tmp/omitted_out_pbyte.cc"
then
	echo "omitted out formal did not expose the caller's Byte storage" >&2
	exit 1
fi
if rg -Fq 'std::addressof(p_buffer)' "$tmp/omitted_out_pbyte.cc"
then
	echo "omitted out formal exposed its storage-ref descriptor" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-Irtl \
	-I"$tmp" \
	"$tmp/omitted_out_pbyte.cc" \
	"$tmp/system.cc" \
	-o "$tmp/omitted_out_pbyte"

actual=$(ASAN_OPTIONS=detect_leaks=1 "$tmp/omitted_out_pbyte")
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
	if ./mp -Furtl -dREJECT_"$rejection" \
		-o"$tmp/rejected.cc" tests/omitted_out_pbyte.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted unsupported omitted-formal pointer variant: $rejection" >&2
		exit 1
	fi
done

echo "omitted out PByte tests passed"
