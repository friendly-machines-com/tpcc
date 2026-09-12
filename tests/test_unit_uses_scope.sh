#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"
mkdir -p "$tmp/positive" "$tmp/no-interface-reexport" \
	"$tmp/no-implementation-reexport" "$tmp/cycles" \
	"$tmp/explicit" "$tmp/shadow-type" "$tmp/shadow-value" \
	"$tmp/late-cycle" "$tmp/impl-private" "$tmp/impl-private-absent" \
	"$tmp/dup-cross-section" "$tmp/impl-overload/ok" "$tmp/impl-overload/bad" \
	"$tmp/impl-private-self" "$tmp/uses-program"


tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/positive/scope_main.cc" \
	tests/unit_uses_scope/scope_main.pp

tpcc_build "$tmp/positive/scope_main" \
	-I"$tmp/positive" \
	"$tmp/positive"/*.cc

tpcc_run "$tmp/positive/scope_main"

for case in explicit shadow-type shadow-value
do
	source=$(printf '%s' "$case" | tr '-' '_')
	tpcc_translate -Futests/unit_uses_scope \
		-o"$tmp/$case/main.cc" \
		"tests/unit_uses_scope/scope_$source.pp"
	tpcc_build "$tmp/$case/main" \
		-I"$tmp/$case" \
		"$tmp/$case"/*.cc
	tpcc_run "$tmp/$case/main"
done

if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/no-interface-reexport/main.cc" \
	tests/unit_uses_scope/scope_no_interface_reexport.pp \
	>"$tmp/no-interface-reexport/stdout" \
	2>"$tmp/no-interface-reexport/stderr"
then
	echo "interface uses was incorrectly re-exported" >&2
	exit 1
fi
if ! grep -Fq "unresolved value identifier: bvalue" \
	"$tmp/no-interface-reexport/stderr"
then
	echo "wrong diagnostic for an interface-use name in a user of the unit" >&2
	sed -n '1,20p' "$tmp/no-interface-reexport/stderr" >&2
	exit 1
fi

if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/no-implementation-reexport/main.cc" \
	tests/unit_uses_scope/scope_no_implementation_reexport.pp \
	>"$tmp/no-implementation-reexport/stdout" \
	2>"$tmp/no-implementation-reexport/stderr"
then
	echo "implementation uses was incorrectly re-exported" >&2
	exit 1
fi
if ! grep -Fq "unresolved value identifier: cvalue" \
	"$tmp/no-implementation-reexport/stderr"
then
	echo "wrong diagnostic for an implementation-use name in a user of the unit" >&2
	sed -n '1,20p' "$tmp/no-implementation-reexport/stderr" >&2
	exit 1
fi

tpcc_translate -Futests/07_mutual_impl \
	-o"$tmp/cycles/implementation.cc" \
	tests/07_mutual_impl/a.pp

tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/late-cycle/main.cc" \
	tests/unit_uses_scope/late_cycle_main.pp

tpcc_build "$tmp/late-cycle/main" \
	-I"$tmp/late-cycle" \
	"$tmp/late-cycle"/*.cc
tpcc_run \
	"$tmp/late-cycle/main"

if tpcc_translate -Futests/08_mutual_iface \
	-o"$tmp/cycles/interface.cc" \
	tests/08_mutual_iface/a.pp \
	>"$tmp/cycles/stdout" \
	2>"$tmp/cycles/stderr"
then
	echo "accepted an interface dependency cycle" >&2
	exit 1
fi
if ! grep -Fq "circular interface dependency" "$tmp/cycles/stderr"
then
	echo "wrong diagnostic for an interface dependency cycle" >&2
	sed -n '1,20p' "$tmp/cycles/stderr" >&2
	exit 1
fi

# An implementation `uses` must not shadow an interface `uses`: when both
# expose the same name, the interface-unit declaration wins.
tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/impl-private/main.cc" \
	tests/unit_uses_scope/scope_impl_private_main.pp
tpcc_build "$tmp/impl-private/main" \
	-I"$tmp/impl-private" \
	"$tmp/impl-private"/*.cc
tpcc_run "$tmp/impl-private/main"

# A used unit's implementation-only declaration is not visible to a unit that
# implementation-uses it.
if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/impl-private-absent/main.cc" \
	tests/unit_uses_scope/scope_impl_private_absent_main.pp \
	>"$tmp/impl-private-absent/stdout" \
	2>"$tmp/impl-private-absent/stderr"
then
	echo "implementation-only declaration of a used unit was visible" >&2
	exit 1
fi
if ! grep -Fq "unresolved value identifier: privatevictim" \
	"$tmp/impl-private-absent/stderr"
then
	echo "wrong diagnostic for a used unit's implementation-only name" >&2
	sed -n '1,20p' "$tmp/impl-private-absent/stderr" >&2
	exit 1
fi

# Interface and implementation are one declaration scope for the unit itself:
# a duplicate across the boundary is an error, not silent shadowing.
if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/dup-cross-section/unit.cc" \
	tests/unit_uses_scope/scope_dup_cross_section.pp \
	>"$tmp/dup-cross-section/stdout" \
	2>"$tmp/dup-cross-section/stderr"
then
	echo "duplicate across interface/implementation was accepted" >&2
	exit 1
fi
if ! grep -Fq "duplicate identifier: dupname" \
	"$tmp/dup-cross-section/stderr"
then
	echo "wrong diagnostic for a cross-section duplicate" >&2
	sed -n '1,20p' "$tmp/dup-cross-section/stderr" >&2
	exit 1
fi

# An implementation-only overload joins the unit's own overload family, but is
# not exported through `uses`.
tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/impl-overload/ok/main.cc" \
	tests/unit_uses_scope/scope_impl_overload_ok.pp
tpcc_build "$tmp/impl-overload/ok/main" \
	-I"$tmp/impl-overload/ok" \
	"$tmp/impl-overload/ok"/*.cc
tpcc_run "$tmp/impl-overload/ok/main"

if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/impl-overload/bad/main.cc" \
	tests/unit_uses_scope/scope_impl_overload_bad.pp \
	>"$tmp/impl-overload/bad/stdout" \
	2>"$tmp/impl-overload/bad/stderr"
then
	echo "implementation-only overload was exported" >&2
	exit 1
fi
if ! grep -Fq "no matching overload for 'overloadfoo'" \
	"$tmp/impl-overload/bad/stderr"
then
	echo "wrong diagnostic for a private implementation overload" >&2
	sed -n '1,20p' "$tmp/impl-overload/bad/stderr" >&2
	exit 1
fi

# An implementation-only declaration is visible inside its own unit.
tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/impl-private-self/main.cc" \
	tests/unit_uses_scope/scope_impl_private_self_main.pp
tpcc_build "$tmp/impl-private-self/main" \
	-I"$tmp/impl-private-self" \
	"$tmp/impl-private-self"/*.cc
tpcc_run "$tmp/impl-private-self/main"

# A program cannot be imported through `uses`: load_or_get_unit must reject a
# source that does not begin with `unit`.
if tpcc_translate -Futests/unit_uses_scope \
	-o"$tmp/uses-program/unit.cc" \
	tests/unit_uses_scope/scope_uses_program.pp \
	>"$tmp/uses-program/stdout" \
	2>"$tmp/uses-program/stderr"
then
	echo "a program was accepted through uses" >&2
	exit 1
fi
if ! grep -Fq "used source file must declare a unit" \
	"$tmp/uses-program/stderr"
then
	echo "wrong diagnostic for using a program as a unit" >&2
	sed -n '1,20p' "$tmp/uses-program/stderr" >&2
	exit 1
fi

echo "unit uses scope tests passed"
