#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate -o"$tmp/type_projections.cc" \
	tests/type_projections.pp
tpcc_build "$tmp/type_projections" \
	"$tmp/type_projections.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/type_projections"

for rejection in \
	NON_POINTER \
	UNTYPED_POINTER \
	NON_ARRAY \
	NON_CONSTANT_INDEX \
	WRONG_INDEX_TYPE \
	MISSING_FIELD
do
	if tpcc_translate -dREJECT_"$rejection" \
		-o"$tmp/type_projection_rejected.cc" \
		tests/type_projection_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid type projection: $rejection" >&2
		exit 1
	fi
done

echo "type projection tests passed"
