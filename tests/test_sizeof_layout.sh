#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-sizeof-layout-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/sizeof_layout.cc" tests/sizeof_layout.pp

if [ "$(rg -F -c 'static_cast<pas::t_sizeint>(sizeof(' "$tmp/sizeof_layout.cc")" -ne 8 ]; then
	echo "SizeOf expressions did not remain C++ sizeof expressions" >&2
	exit 1
fi

if ! rg -Fq 'pas::t_fixedarray<pas::t_byte, 4,' "$tmp/sizeof_layout.cc"; then
	echo "SizeOf constant evaluation did not construct the array bound" >&2
	exit 1
fi

for expected in \
	'offsetof(t_tpadded, p_first) == 0' \
	'offsetof(t_tpadded, p_middle) == 4' \
	'offsetof(t_tpadded, p_last) == 8' \
	'sizeof(t_tpadded) == 12' \
	'alignof(t_tpadded) == 4' \
	'offsetof(t_tvarsectionpadded, p_zed) == 0' \
	'offsetof(t_tvarsectionpadded, p_alpha) == 4' \
	'offsetof(t_tvarsectionpadded, p_omega) == 8' \
	'sizeof(t_tvarsectionpadded) == 12' \
	'offsetof(t_tvariant, m_variant) + offsetof(t_tvariant::m_variant_arm_0_type, p_number) == 12' \
	'sizeof(t_tvariant) == 16' \
	'alignof(t_tvariant) == 8' \
	'static_assert(sizeof(t_tpacked) == t_tpacked::m_storage_size' \
	'static_assert(alignof(t_tpacked) == 1'
do
	if ! rg -Fq "$expected" "$tmp/sizeof_layout.cc"; then
		echo "missing layout assertion: $expected" >&2
		exit 1
	fi
done

if rg -q 'layout_oracle|gnu::packed' "$tmp/sizeof_layout.cc"; then
	echo "packed layout unexpectedly depends on a GNU oracle" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	"$tmp/sizeof_layout.cc" \
	rtl/system.cc \
	-o "$tmp/sizeof_layout"
ASAN_OPTIONS=detect_leaks=1 "$tmp/sizeof_layout"

echo "SizeOf/layout tests passed"
