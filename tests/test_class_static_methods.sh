#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/class_static_methods.cc" \
	tests/class_static_methods.pp

tpcc_translate -o"$tmp/class_var_visibility_boundary.cc" \
	tests/class_var_visibility_boundary.pp

if ! grep -Eq 'static .* p_staticvalue\(' \
	"$tmp/class_static_methods.cc"
then
	echo "static class method was not emitted as a native C++ static member" >&2
	exit 1
fi
if grep -Fq 'm_meta::p_staticvalue' \
	"$tmp/class_static_methods.cc"
then
	echo "static class method was emitted into the metaclass" >&2
	exit 1
fi
if ! grep -Fq '&t_tbase::p_staticvalue' \
	"$tmp/class_static_methods.cc"
then
	echo "static class method reference was not emitted as a plain function pointer" >&2
	exit 1
fi
if ! grep -Fq 'static_cast<void>(p_getobject())' \
	"$tmp/class_static_methods.cc"
then
	echo "runtime static-method qualifier was not sequenced" >&2
	exit 1
fi
if ! grep -Fq 'static_cast<void>(p_getobject()), &t_tbase::p_staticvalue' \
	"$tmp/class_static_methods.cc"
then
	echo "runtime static-method-reference qualifier was not sequenced" >&2
	exit 1
fi

tpcc_build "$tmp/class_static_methods" \
	"$tmp/class_static_methods.cc" \
	"$tmp/system.cc"

actual=$("$tmp/class_static_methods")
test "$actual" = 'class static methods passed'

for source in \
	tests/class_static_self_rejected.pp \
	tests/class_static_instance_rejected.pp \
	tests/class_static_virtual_rejected.pp \
	tests/class_static_to_method_rejected.pp \
	tests/class_method_to_plain_rejected.pp
do
	base=${source%.pp}
	if tpcc_translate -o"$tmp/rejected.cc" "$source" \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "expected tpcc to reject $source" >&2
		exit 1
	fi
	expected=$(sed -n '1p' "$base.error")
	if ! grep -Fq -- "$expected" "$tmp/stderr"
	then
		echo "wrong diagnostic for $source; expected: $expected" >&2
		sed -n '1,20p' "$tmp/stderr" >&2
		exit 1
	fi
done

echo "class static method tests passed"
