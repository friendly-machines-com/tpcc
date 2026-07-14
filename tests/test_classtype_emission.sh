#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-classtype-emission-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/system.cc" rtl/system.pp
./mp -Furtl -o"$tmp/class_method.cc" tests/16_class_method.pp

if rg -q 'm_meta_instance' "$tmp"
then
	echo "obsolete m_meta_instance helper was emitted" >&2
	exit 1
fi
classtype_count=$(
	rg -F -c 'inline static m_meta* p_classtype()' \
		"$tmp/system.h" || true
)
if test "${classtype_count:-0}" -ne 1
then
	echo "System did not emit exactly one outer p_classtype accessor" >&2
	exit 1
fi
if ! rg -q 'virtual inline .*p_classtype' "$tmp/system.h"
then
	echo "System metaclass did not emit the ordinary virtual ClassType operation" >&2
	exit 1
fi
if rg -q 'm_classref|m_allocate|m_construct' "$tmp"
then
	echo "unapproved metaclass bridge or allocation helper was emitted" >&2
	exit 1
fi
if ! rg -Fq 't_tobject::m_meta* t_tfoo::m_meta::p_meta()' \
	"$tmp/class_method.cc"
then
	echo "class method body was not emitted as an ordinary m_meta method" >&2
	exit 1
fi
if rg -q 'inline static .*p_meta' "$tmp/class_method.cc"
then
	echo "outer class-method proxy was emitted" >&2
	exit 1
fi
if ! rg -Fq 'return t_tobject::p_classtype();' \
	"$tmp/class_method.cc"
then
	echo "derived ClassParent did not use the parent p_classtype" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Werror \
	-I"$tmp" \
	-Irtl \
	"$tmp/class_method.cc" \
	"$tmp/system.cc" \
	-o "$tmp/class_method"
"$tmp/class_method"

echo "ClassType emission tests passed"
