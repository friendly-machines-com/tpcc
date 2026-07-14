#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-function-pointers-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/function_pointers.cc" \
	tests/function_pointers.pp

if ! rg -q \
	'pas::m_proc<void\(pas::t_integer\)> p_plainprocedure' \
	"$tmp/function_pointers.cc"
then
	echo "plain routine type did not use m_proc<Signature>" >&2
	exit 1
fi
if ! rg -q \
	'pas::m_method<void\(pas::t_integer\)> p_boundprocedure' \
	"$tmp/function_pointers.cc"
then
	echo "method routine type did not use m_method<Signature>" >&2
	exit 1
fi
if ! rg -q 'pas::m_bind_method<static_cast<' \
	"$tmp/function_pointers.cc"
then
	echo "method binding did not use the template adapter" >&2
	exit 1
fi
if rg -q '\[[^]]*\].*p_accumulate' \
	"$tmp/function_pointers.cc"
then
	echo "method binding emitted a capture lambda" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-fsanitize=address,undefined \
	-Irtl \
	-I"$tmp" \
	tests/function_pointers_runtime.cc \
	"$tmp/system.cc" \
	-o "$tmp/function_pointers"
ASAN_OPTIONS=detect_leaks=1 "$tmp/function_pointers"

for source in \
	tests/function_pointer_explicit_cross_kind_rejected.pp \
	tests/function_pointer_global_to_method_rejected.pp \
	tests/function_pointer_method_to_global_rejected.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! rg -F -q -- "$expected" "$tmp/stderr"
	then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "function pointer tests passed"
