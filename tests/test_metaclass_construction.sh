#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/construction.cc" \
	tests/metaclass_construction.pp

if ! rg -Fq 'void t_tbase::p_create(' \
	"$tmp/construction.cc"
then
	echo "ordinary constructor body was not emitted as a Unit initializer" >&2
	exit 1
fi
if rg -Fq 'return this;' "$tmp/construction.cc"
then
	echo "ordinary constructor initializer still returns this" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_construct<t_tbase' \
	"$tmp/construction.cc"
then
	echo "class-reference constructor call did not emit Construct" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_free_object(p_nilinstance)' \
	"$tmp/construction.cc"
then
	echo "nil-safe Free did not use the receiver-first RTL operation" >&2
	exit 1
fi
if rg -Fq -- '->p_free(' "$tmp/construction.cc"
then
	echo "Free still enters a C++ member function through its receiver" >&2
	exit 1
fi
if ! rg -Fq '::u_system::m_bind_receiver_function<static_cast<' \
	"$tmp/construction.cc"
then
	echo "method reference to Free did not use the receiver-first adapter" >&2
	exit 1
fi

tpcc_build "$tmp/construction" \
	"$tmp/construction.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run "$tmp/construction")
expected='7
2
1'
if test "$actual" != "$expected"
then
	echo "unexpected dynamic construction result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
	exit 1
fi

echo "metaclass construction tests passed"
