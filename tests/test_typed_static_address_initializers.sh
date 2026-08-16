#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"

tpcc_translate \
	-o"$tmp/typed_static_address_initializers.cc" \
	tests/typed_static_address_initializers.pp

tpcc_build "$tmp/typed_static_address_initializers" \
	"$tmp/typed_static_address_initializers.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/typed_static_address_initializers"

check_rejected()
{
	source=$1
	expected=$2
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	if ! grep -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,60p' "$tmp/stderr" >&2
		exit 1
	fi
}

check_rejected \
	tests/typed_static_address_automatic_rejected.pp \
	"typed address initializer requires static storage or a receiverless routine"
check_rejected \
	tests/untyped_data_address_constant_rejected.pp \
	"constant expression expected"
check_rejected \
	tests/untyped_routine_address_constant_rejected.pp \
	"constant expression expected"

echo "typed static address initializer tests passed"
