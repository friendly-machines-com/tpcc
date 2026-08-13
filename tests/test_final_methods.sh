#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/final_methods.cc" \
	tests/final_methods.pp

if test "$(rg -F -c ' override final;' \
	"$tmp/final_methods.cc")" -ne 2
then
	echo "instance and class override-final declarations were not emitted" >&2
	exit 1
fi
if ! rg -Fq 'virtual void p_directfinal() final;' \
	"$tmp/final_methods.cc"
then
	echo "direct virtual-final declaration was not emitted" >&2
	exit 1
fi
tpcc_build "$tmp/final_methods" \
	"$tmp/final_methods.cc" \
	"$tmp/system.cc"
"$tmp/final_methods"

for define in TEST_INSTANCE TEST_CLASS
do
	if tpcc_translate -d"$define" \
		-o"$tmp/rejected.cc" \
		tests/final_method_override_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted an override of a final Pascal method: $define" >&2
		exit 1
	fi
	if ! rg -Fq 'overrides a final method' "$tmp/stderr"
	then
		echo "wrong final-override diagnostic: $define" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

if tpcc_translate \
	-o"$tmp/rejected.cc" \
	tests/final_nonvirtual_rejected.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted final on a nonvirtual method" >&2
	exit 1
fi
expected=$(sed -n '1p' \
	tests/final_nonvirtual_rejected.error)
if ! rg -Fq -- "$expected" "$tmp/stderr"
then
	echo "wrong nonvirtual-final diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "final method tests passed"
