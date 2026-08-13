#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


compile_and_run()
{
	name=$1
	tpcc_translate -o"$tmp/$name.cc" "tests/$name.pp"
	tpcc_build "$tmp/$name" \
		"$tmp/$name.cc" \
		"$tmp/system.cc"
	tpcc_run \
		"$tmp/$name"
}

compile_and_run boolean_short_circuit
for operator in '&&' '||'
do
	if ! rg -F \
		"tpcc_bool_to_boolean" \
		"$tmp/boolean_short_circuit.cc" |
		rg -Fq "$operator"
	then
		echo "Boolean $operator did not retain Pascal Boolean result type" >&2
		exit 1
	fi
done

compile_and_run const_address
if ! rg -Fq \
	'const_cast<' \
	"$tmp/const_address.cc" ||
   ! rg -Fq \
	'std::addressof(' \
	"$tmp/const_address.cc"
then
	echo "address of a const formal did not make C++ cv removal explicit" >&2
	exit 1
fi

compile_and_run selected_callable_emission
if ! rg -Fq \
	'static_cast<::u_system::t_integer>(9ull)' \
	"$tmp/selected_callable_emission.cc"
then
	echo "contextual const-formal literal was not pinned to Integer" >&2
	exit 1
fi

echo "emission semantic-boundary tests passed"
