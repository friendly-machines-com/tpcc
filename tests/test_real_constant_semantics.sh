#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/real_constant_semantics.cc" \
	tests/real_constant_semantics.pp

# Both procedures selected Domain(Single). R changes only the constructed
# unchecked value versus checked-failure expression after overload selection.
rg -Fq 'p_domain(std::numeric_limits<::u_system::t_single>::infinity())' \
	"$tmp/real_constant_semantics.cc"
rg -Fq 'p_domain(([]() -> ::u_system::t_single { ::u_system::m_runtime_error(201); return {}; }()))' \
	"$tmp/real_constant_semantics.cc"

tpcc_translate -dCPUX86_64 \
	-o"$tmp/x86_64.cc" \
	tests/real_constant_semantics.pp
tpcc_translate -dCPUAARCH64 \
	-o"$tmp/aarch64.cc" \
	tests/real_constant_semantics.pp
cmp "$tmp/x86_64.cc" "$tmp/aarch64.cc"

tpcc_build "$tmp/real_constant_semantics" \
	tests/real_constant_semantics_runtime.cpp \
	"$tmp/system.cc"

"$tmp/real_constant_semantics"

if tpcc_translate \
	-o"$tmp/out_of_domain.cc" \
	tests/real_constant_out_of_domain.pp \
	>"$tmp/out" 2>"$tmp/err"
then
	echo "accepted integer origin below Int64 minimum" >&2
	exit 1
fi

rg -Fq 'integer constant is below Int64 minimum' "$tmp/err"

echo "real constant semantics tests passed"
