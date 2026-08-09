#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-frame-intrinsics-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl \
	-o"$tmp/frame_intrinsics.cc" \
	tests/frame_intrinsics.pp

for macro_call in \
	'm_get_frame()' \
	'm_get_caller_addr(' \
	'm_get_caller_frame('
do
	if ! rg -Fq "$macro_call" \
		"$tmp/frame_intrinsics.cc"
	then
		echo "generated code omitted frame macro call: $macro_call" >&2
		exit 1
	fi
done
if rg -Fq '::u_system::m_get_' \
	"$tmp/frame_intrinsics.cc"
then
	echo "generated code namespace-qualified a frame macro" >&2
	exit 1
fi
if rg -q '\bp_get_(frame|caller_addr|caller_frame)\b' \
	"$tmp/frame_intrinsics.cc"
then
	echo "generated code used the ordinary-function prefix for a frame macro" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-O2 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-fno-omit-frame-pointer \
	-Irtl \
	-I"$tmp" \
	"$tmp/frame_intrinsics.cc" \
	"$tmp/system.cc" \
	-o "$tmp/frame_intrinsics"
"$tmp/frame_intrinsics"

echo "frame intrinsic tests passed"
