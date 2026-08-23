#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/sizeof_layout.cc" tests/sizeof_layout.pp

if [ "$(rg -F -c 'static_cast<::u_system::t_sizeint>(sizeof(' "$tmp/sizeof_layout.cc")" -ne 12 ]; then
	echo "SizeOf expressions did not remain C++ sizeof expressions" >&2
	exit 1
fi

if ! grep -Fq '::u_system::t_fixedarray<::u_system::t_byte, 4,' "$tmp/sizeof_layout.cc"; then
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
	'static_assert(alignof(t_tpacked) == 1' \
	'offsetof(t_tshortstringrecord, p_name) == 0' \
	'sizeof(decltype(t_tshortstringrecord::p_name)) == 3' \
	'offsetof(t_tshortstringrecord, p_enabled) == 3' \
	'offsetof(t_tshortstringrecord, p_define) == 4' \
	'sizeof(decltype(t_tshortstringrecord::p_define)) == 6' \
	'sizeof(t_tshortstringrecord) == 10' \
	'alignof(t_tshortstringrecord) == 1'
do
	if ! grep -Fq "$expected" "$tmp/sizeof_layout.cc"; then
		echo "missing layout assertion: $expected" >&2
		exit 1
	fi
done

if grep -Eq 'layout_oracle|gnu::packed' "$tmp/sizeof_layout.cc"; then
	echo "packed layout unexpectedly depends on a GNU oracle" >&2
	exit 1
fi

tpcc_build "$tmp/sizeof_layout" \
	"$tmp/sizeof_layout.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/sizeof_layout"

echo "SizeOf/layout tests passed"
