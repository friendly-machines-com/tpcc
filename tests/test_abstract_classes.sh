#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/abstract_classes.cc" \
	tests/abstract_classes.pp

if grep -Eq '\) = 0;' "$tmp/abstract_classes.cc" ||
   grep -Fq 'p_runerror(' "$tmp/abstract_classes.cc"
then
	echo "class abstract incorrectly changed C++ class or method emission" >&2
	exit 1
fi

tpcc_build "$tmp/abstract_classes" \
	"$tmp/abstract_classes.cc" \
	"$tmp/system.cc"

actual=$("$tmp/abstract_classes")
expected='concrete
concrete
concrete'
if test "$actual" != "$expected"
then
	echo "class abstract changed native construction behavior" >&2
	printf 'expected:\n%s\nactual:\n%s\n' \
		"$expected" "$actual" >&2
	exit 1
fi

echo "abstract class tests passed"
