#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate \
	-o"$tmp/class_pointer_conversion.cc" \
	tests/class_pointer_conversion.pp

for required in \
	'static_cast<::u_system::t_pointer>' \
	'p_returnpointer' \
	'p_takepointer' \
	'p_takeconstpointer'
do
	if ! grep -Fq "$required" \
		"$tmp/class_pointer_conversion.cc"
	then
		echo "missing class-to-Pointer lowering: $required" >&2
		exit 1
	fi
done

tpcc_build "$tmp/class_pointer_conversion" \
	"$tmp/class_pointer_conversion.cc" \
	"$tmp/system.cc"

tpcc_run \
	"$tmp/class_pointer_conversion"

for kind in \
	TYPED_POINTER \
	CLASSREF_TYPED_POINTER \
	VAR \
	OUT \
	OLD_OBJECT \
	INTERFACE \
	CHAIN
do
	if tpcc_translate -d"TEST_$kind" \
		-o"$tmp/rejected.cc" \
		tests/class_pointer_conversion_rejected.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted forbidden class-to-Pointer conversion: $kind" >&2
		exit 1
	fi
	if ! grep -Eq \
		'no implicit conversion|no matching overload' \
		"$tmp/stderr"
	then
		echo "wrong class-to-Pointer rejection diagnostic: $kind" >&2
		sed -n '1,100p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "class-to-Pointer conversion tests passed"
