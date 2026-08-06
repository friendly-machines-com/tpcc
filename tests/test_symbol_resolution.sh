#!/bin/sh
# Coverage for the target Pascal symbol-resolution rules, with each rule
# pinned by a fixture derived from FPC 3.2.2 behaviour (verified via
# `guix shell fpc -- fpc`):
#
#   2a own impl decls > impl-uses decls        sr_own_impl_beats_impl_uses_clash
#   2b own iface decls > impl-uses decls       sr_own_iface_beats_impl_uses_clash
#                                              (the lexical-intuition violator)
#   2d impl-uses > iface-uses                  sr_impl_uses_beats_iface_uses_clash,
#                                              sr_iface_uses_shadows_via_carrier
#   2e last-listed wins within one uses clause sr_unqualified_shadow_last
#   2f iface section cannot see impl-uses      sr_iface_cannot_see_impl_uses,
#                                              sr_iface_cannot_see_impl_uses_clash
#   3a-f Unit.Symbol by kind + chained member  sr_qualified_kinds
#   3g/4 self-qualification                    sr_self_qual_user (drives
#                                              sr_self_qual which self-qualifies)
#   5  unit name shadowable by local           sr_unit_name_shadowable
#   6b type identifier as value rejected       sr_type_in_value_position
#   3.case4 `.` on non-record LHS rejected     sr_value_qualifier_not_a_unit
#   3.sanity Unit.NoSuchMember names the miss  sr_qualified_unit_no_such_member
#   1  single namespace per scope              sr_single_namespace_clash
#
# Driver convention follows tests/test_unit_uses_scope.sh: each positive main
# program is translated to a temp dir, then every .cc in that dir is compiled
# and the program is run; any non-zero exit fails the test. Each negative
# fixture has a matching .error whose first line is matched as a substring
# against mp's stderr.

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fixtures="$root/tests/symbol_resolution"
tmp=${TMPDIR:-/tmp}/tpcc-symbol-resolution-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

# Compile a positive main + its used-unit closure, link, run, expect exit 0.
run_positive() {
	name="$1"
	main="$2"
	bin="$tmp/$name"
	mkdir -p "$bin"
	./mp -Furtl -Fu"$fixtures" -o"$bin/main.cc" "$main"
	"${CXX:-g++}" \
		-std=c++20 -Wall -Wextra -Werror \
		-fsanitize=address,undefined \
		-I"$bin" -Irtl \
		"$bin"/*.cc \
		-o "$bin/run"
	ASAN_OPTIONS=detect_leaks=1 "$bin/run"
}

# Compile a negative main; expect mp to fail with .error[1] as substring.
run_negative() {
	name="$1"
	main="$2"
	bin="$tmp/$name"
	mkdir -p "$bin"
	expected=$(sed -n '1p' "$fixtures/$name.error")
	set +e
	./mp -Furtl -Fu"$fixtures" -o"$bin/main.cc" "$main" \
		>"$bin/stdout" 2>"$bin/stderr"
	status=$?
	set -e
	if [ "$status" -eq 0 ]; then
		echo "negative fixture $name compiled; expected failure" >&2
		exit 1
	fi
	if ! rg -Fq -- "$expected" "$bin/stderr"; then
		echo "wrong diagnostic for $name; expected substring: $expected" >&2
		sed -n '1,30p' "$bin/stderr" >&2
		exit 1
	fi
}

run_positive sr_unqualified_shadow_last \
	"$fixtures/sr_unqualified_shadow_last.pp"

run_positive sr_qualified_kinds \
	"$fixtures/sr_qualified_kinds.pp"

run_positive sr_self_qual_user \
	"$fixtures/sr_self_qual_user.pp"

run_positive sr_iface_uses_shadows_via_carrier \
	"$fixtures/sr_iface_uses_shadows_via_carrier.pp"

run_positive sr_impl_uses_beats_iface_uses_clash_main \
	"$fixtures/sr_impl_uses_beats_iface_uses_clash_main.pp"

run_positive sr_own_iface_beats_impl_uses_clash_main \
	"$fixtures/sr_own_iface_beats_impl_uses_clash_main.pp"

run_positive sr_own_impl_beats_impl_uses_clash_main \
	"$fixtures/sr_own_impl_beats_impl_uses_clash_main.pp"

run_positive sr_unit_name_shadowable \
	"$fixtures/sr_unit_name_shadowable.pp"

run_negative sr_single_namespace_clash \
	"$fixtures/sr_single_namespace_clash.pp"

run_negative sr_type_in_value_position \
	"$fixtures/sr_type_in_value_position.pp"

run_negative sr_iface_cannot_see_impl_uses \
	"$fixtures/sr_iface_cannot_see_impl_uses.pp"

run_negative sr_iface_cannot_see_impl_uses_clash \
	"$fixtures/sr_iface_cannot_see_impl_uses_clash.pp"

run_negative sr_qualified_unit_no_such_member \
	"$fixtures/sr_qualified_unit_no_such_member.pp"

run_negative sr_value_qualifier_not_a_unit \
	"$fixtures/sr_value_qualifier_not_a_unit.pp"

echo "symbol resolution tests passed"
