#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/function_pointers.cc" \
	tests/function_pointers.pp

if ! rg -q \
	'::u_system::m_proc<void\(::u_system::t_integer\)> p_plainprocedure' \
	"$tmp/function_pointers.cc"
then
	echo "plain routine type did not use m_proc<Signature>" >&2
	exit 1
fi
if ! rg -q \
	'::u_system::m_method<void\(::u_system::t_integer\)> p_boundprocedure' \
	"$tmp/function_pointers.cc"
then
	echo "method routine type did not use m_method<Signature>" >&2
	exit 1
fi
if ! rg -q '::u_system::m_bind_method<static_cast<' \
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
if ! rg -q \
	'::u_system::m_explicit_routine_cast<void\(::u_system::t_pointer, ::u_system::t_pointer\)>\(p_objectcallback\)' \
	"$tmp/function_pointers.cc"
then
	echo "plain explicit pointer-parameter routine cast was not preserved" >&2
	exit 1
fi
if ! rg -q \
	'::u_system::m_explicit_routine_cast<void\(::u_system::t_pointer, ::u_system::t_pointer\)>\(p_objectmethodcallback\)' \
	"$tmp/function_pointers.cc"
then
	echo "method explicit pointer-parameter routine cast was not preserved" >&2
	exit 1
fi

tpcc_build "$tmp/function_pointers" \
	tests/function_pointers_runtime.cpp \
	"$tmp/system.cc"
tpcc_run "$tmp/function_pointers"

tpcc_translate -o"$tmp/routine_value_overload_categories.cc" \
	tests/routine_value_overload_categories.pp

if ! rg -q \
	'p_foreachcall\(::u_system::m_proc<void\(.*t_pointer.*t_pointer.*\)>' \
	"$tmp/routine_value_overload_categories.cc"
then
	echo "plain callback overload did not retain its m_proc carrier" >&2
	exit 1
fi
if ! rg -q \
	'p_foreachcall\(::u_system::m_method<void\(.*t_pointer.*t_pointer.*\)>' \
	"$tmp/routine_value_overload_categories.cc"
then
	echo "bound callback overload did not retain its m_method carrier" >&2
	exit 1
fi

tpcc_build "$tmp/routine_value_overload_categories" \
	"$tmp/routine_value_overload_categories.cc" \
	"$tmp/system.cc"
tpcc_run \
	"$tmp/routine_value_overload_categories"

mkdir -p "$tmp/routine-const"
tpcc_translate -Futests/routine_const \
	-o"$tmp/routine-const/program.cc" \
	tests/routine_const/routine_const_program.pp

if ! rg -Fq \
	'p_dostatus = &::u_routineconstunit::p_defstatus' \
	"$tmp/routine-const/routineconstunit.h"
then
	echo "typed routine constant did not preserve @Routine" >&2
	exit 1
fi

tpcc_build "$tmp/routine-const/program" \
	-I"$tmp/routine-const" \
	"$tmp/routine-const"/*.cc
tpcc_run \
	"$tmp/routine-const/program"

for source in \
	tests/function_pointer_explicit_cross_kind_rejected.pp \
	tests/function_pointer_explicit_nonpointer_rejected.pp \
	tests/function_pointer_implicit_pointer_mismatch_rejected.pp \
	tests/function_pointer_global_to_method_rejected.pp \
	tests/function_pointer_method_to_global_rejected.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	while IFS= read -r expected
	do
		test -z "$expected" && continue
		if ! rg -F -q -- "$expected" "$tmp/stderr"
		then
			echo "wrong diagnostic for $source; expected: $expected" >&2
			sed -n '1,40p' "$tmp/stderr" >&2
			exit 1
		fi
	done < "$base.error"
done

echo "function pointer tests passed"
