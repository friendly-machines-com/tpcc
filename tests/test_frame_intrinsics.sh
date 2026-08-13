#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/frame_intrinsics.cc" \
	tests/frame_intrinsics.pp

for macro_call in \
	'm_get_frame()' \
	'm_get_caller_addr(' \
	'm_get_caller_frame('
do
	if ! grep -Fq "$macro_call" \
		"$tmp/frame_intrinsics.cc"
	then
		echo "generated code omitted frame macro call: $macro_call" >&2
		exit 1
	fi
done
if grep -Fq '::u_system::m_get_' \
	"$tmp/frame_intrinsics.cc"
then
	echo "generated code namespace-qualified a frame macro" >&2
	exit 1
fi
if grep -Eq '\bp_get_(frame|caller_addr|caller_frame)\b' \
	"$tmp/frame_intrinsics.cc"
then
	echo "generated code used the ordinary-function prefix for a frame macro" >&2
	exit 1
fi

tpcc_build_unsanitized "$tmp/frame_intrinsics" \
	-O2 \
	-fno-omit-frame-pointer \
	"$tmp/frame_intrinsics.cc" \
	"$tmp/system.cc"
"$tmp/frame_intrinsics"

echo "frame intrinsic tests passed"
