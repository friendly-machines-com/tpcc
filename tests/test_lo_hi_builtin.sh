#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/lo_hi_builtin.cc" \
	tests/lo_hi_builtin.pp

# Lo and Hi are ordinary System unit routines: calls must resolve inside the
# System namespace rather than to any external runtime entry point.
if ! grep -Fq '::u_system::p_lo(' "$tmp/lo_hi_builtin.cc"; then
	echo "Lo calls did not resolve in the System namespace" >&2
	exit 1
fi
if ! grep -Fq '::u_system::p_hi(' "$tmp/lo_hi_builtin.cc"; then
	echo "Hi calls did not resolve in the System namespace" >&2
	exit 1
fi

# The bodies must come from the compiled System unit implementation, not from
# hand-written C++ runtime code. Unit-internal definitions are not
# namespace-qualified, so match the definition shape directly.
if ! grep -Fq ' p_lo(' "$tmp/system.cc"; then
	echo "System.Lo bodies missing from the compiled unit" >&2
	exit 1
fi
if ! grep -Fq ' p_hi(' "$tmp/system.cc"; then
	echo "System.Hi bodies missing from the compiled unit" >&2
	exit 1
fi

tpcc_build "$tmp/lo_hi_builtin" \
	"$tmp/lo_hi_builtin.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/lo_hi_builtin"

echo "Lo/Hi builtin tests passed"
