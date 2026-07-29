#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-initialize-finalize.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/initialize_finalize.cc" \
	tests/initialize_finalize.pp

for required in \
	'::u_system::p_initialize(::u_system::tpcc_make_storage_ref(p_copy))' \
	'::u_system::p_finalize(::u_system::tpcc_make_storage_ref(p_copy))' \
	'inline void m_pascal_initialize(t_tmanaged& value) noexcept' \
	'inline void m_pascal_finalize(t_tmanaged& value) noexcept'
do
	if ! rg -Fq "$required" "$tmp/initialize_finalize.cc"
	then
		echo "missing Initialize/Finalize lowering: $required" >&2
		exit 1
	fi
done

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-fno-sanitize-recover=all \
	-I"$tmp" \
	-Irtl \
	"$tmp/initialize_finalize.cc" \
	"$tmp/system.cc" \
	-o "$tmp/initialize_finalize"

actual=$(ASAN_OPTIONS=detect_leaks=1 "$tmp/initialize_finalize")
if test "$actual" != 'Initialize/Finalize passed'
then
	echo "unexpected Initialize/Finalize output: $actual" >&2
	exit 1
fi

for intrinsic in INITIALIZE FINALIZE
do
	if ./mp -Furtl -dTEST_"$intrinsic"_COUNT \
		-o"$tmp/count_rejected.cc" \
		tests/initialize_finalize_count_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted unsupported two-argument $intrinsic" >&2
		exit 1
	fi
	if ! rg -Fqi "too many arguments to '${intrinsic}'" \
		"$tmp/stderr"
	then
		echo "wrong two-argument $intrinsic diagnostic" >&2
		sed -n '1,120p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "Initialize/Finalize tests passed"
