#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/tdatetime.cc" \
	tests/tdatetime.pp

if ! rg -Fq \
	'using t_tdatetime = ::u_system::t_double;' \
	"$tmp/system.h"
then
	echo "TDateTime does not use the Double carrier" >&2
	exit 1
fi

tpcc_build "$tmp/tdatetime" \
	"$tmp/tdatetime.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/tdatetime"

echo "TDateTime tests passed"
