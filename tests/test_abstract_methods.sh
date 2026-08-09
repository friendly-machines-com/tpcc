#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-abstract-method-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/abstract_methods.cc" \
	tests/abstract_methods.pp

if rg -Fq 'p_missing() = 0' \
	"$tmp/abstract_methods.cc"
then
	echo "class abstract method was still emitted as pure virtual" >&2
	exit 1
fi
if ! rg -Fq \
	'::u_system::m_runtime_error(211);' \
	"$tmp/abstract_methods.cc"
then
	echo "class abstract method did not receive the runtime-error stub" >&2
	exit 1
fi
if ! rg -Fq 'virtual void p_interfacemethod() = 0;' \
	"$tmp/abstract_methods.cc"
then
	echo "interface method stopped being pure virtual" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Irtl \
	-I"$tmp" \
	"$tmp/abstract_methods.cc" \
	"$tmp/system.cc" \
	-o "$tmp/abstract_methods"

actual=$("$tmp/abstract_methods")
expected='concrete
implemented
class implemented'
if test "$actual" != "$expected"
then
	echo "unexpected abstract-method override behavior" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

./mp -Furtl -o"$tmp/abstract_call.cc" \
	tests/abstract_method_call.pp
"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Irtl \
	-I"$tmp" \
	"$tmp/abstract_call.cc" \
	"$tmp/system.cc" \
	-o "$tmp/abstract_call"
set +e
"$tmp/abstract_call"
status=$?
set -e
if test "$status" -ne 211
then
	echo "abstract method call exited with $status instead of 211" >&2
	exit 1
fi

for source in \
	tests/abstract_nonvirtual_rejected.pp \
	tests/abstract_implementation_rejected.pp
do
	base=${source%.pp}
	if ./mp -Furtl -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid abstract method source: $source" >&2
		exit 1
	fi
	expected_error=$(sed -n '1p' "$base.error")
	if ! rg -Fq -- "$expected_error" "$tmp/stderr"
	then
		echo "wrong abstract-method diagnostic; expected: $expected_error" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "abstract method tests passed"
