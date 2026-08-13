#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/abstract_methods.cc" \
	tests/abstract_methods.pp

if grep -Fq 'p_missing() = 0' \
	"$tmp/abstract_methods.cc"
then
	echo "class abstract method was still emitted as pure virtual" >&2
	exit 1
fi
if ! grep -Fq \
	'::u_system::m_runtime_error(211);' \
	"$tmp/abstract_methods.cc"
then
	echo "class abstract method did not receive the runtime-error stub" >&2
	exit 1
fi
if ! grep -Fq 'virtual void p_interfacemethod() = 0;' \
	"$tmp/abstract_methods.cc"
then
	echo "interface method stopped being pure virtual" >&2
	exit 1
fi

tpcc_build "$tmp/abstract_methods" \
	"$tmp/abstract_methods.cc" \
	"$tmp/system.cc"

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

tpcc_translate -o"$tmp/abstract_call.cc" \
	tests/abstract_method_call.pp
tpcc_build "$tmp/abstract_call" \
	"$tmp/abstract_call.cc" \
	"$tmp/system.cc"
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
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid abstract method source: $source" >&2
		exit 1
	fi
	expected_error=$(sed -n '1p' "$base.error")
	if ! grep -Fq -- "$expected_error" "$tmp/stderr"
	then
		echo "wrong abstract-method diagnostic; expected: $expected_error" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "abstract method tests passed"
