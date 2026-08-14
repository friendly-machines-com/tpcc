#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/system.cc" rtl/system.pp
tpcc_translate -o"$tmp/lifecycle.cc" \
	tests/old_object_lifecycle.pp
tpcc_translate -o"$tmp/shadow.cc" \
	tests/old_object_new_shadow.pp

if ! grep -Fq 'void t_tbase::p_init(' \
	"$tmp/lifecycle.cc"
then
	echo "old-object constructor was not emitted as a Unit method" >&2
	exit 1
fi
if ! grep -Fq 'virtual void p_done()' \
	"$tmp/lifecycle.cc"
then
	echo "old-object destructor was not emitted as a virtual ordinary method" >&2
	exit 1
fi
if grep -Fq '~t_tbase() {' \
	"$tmp/lifecycle.cc"
then
	echo "Pascal old-object destructor body became a C++ destructor" >&2
	exit 1
fi
if ! grep -Fq 'virtual ~t_tbase() = default;' \
	"$tmp/lifecycle.cc"
then
	echo "VMT-bearing old object lacks its hidden carrier destructor" >&2
	exit 1
fi
if ! grep -Fq 't_tbase::p_done();' \
	"$tmp/lifecycle.cc"
then
	echo "explicit inherited old-object destructor call was dropped" >&2
	exit 1
fi
if grep -Fq 'new t_tderived{}' \
	"$tmp/lifecycle.cc"
then
	echo "old-object allocation was value-initialized" >&2
	exit 1
fi
if ! grep -Fq '::u_system::m_new_object<t_tderived' \
	"$tmp/lifecycle.cc"
then
	echo "extended New did not preserve the exact pointed-to object type" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/bad.cc" \
	tests/old_object_virtual_constructor.pp \
	>"$tmp/bad.out" 2>"$tmp/bad.err"
then
	echo "virtual old-object constructor was accepted" >&2
	exit 1
fi
if ! grep -Fq 'old-style object constructors cannot be' \
	"$tmp/bad.err"
then
	echo "virtual old-object constructor diagnostic was not specific" >&2
	cat "$tmp/bad.err" >&2
	exit 1
fi

tpcc_build "$tmp/lifecycle" \
	"$tmp/lifecycle.cc" \
	"$tmp/system.cc"
tpcc_build "$tmp/shadow" \
	"$tmp/shadow.cc" \
	"$tmp/system.cc"

actual=$(tpcc_run "$tmp/lifecycle")
expected='base static
derived virtual
7
base static
derived virtual
11
derived done
base done
derived virtual
derived done
base done
derived virtual
1
1
0
0
1
23
29
31
37'
if test "$actual" != "$expected"
then
	echo "unexpected old-object lifecycle result" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

shadow_actual=$("$tmp/shadow")
shadow_expected='31
37'
if test "$shadow_actual" != "$shadow_expected"
then
	echo "New/Dispose builtin syntax bypassed ordinary shadowing" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$shadow_expected" "$shadow_actual" >&2
	exit 1
fi

echo "old-object lifecycle tests passed"
