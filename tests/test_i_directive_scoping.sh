#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=${TMPDIR:-/tmp}/tpcc-i-directive-scoping-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

cd "$root"

./mp -Furtl -o"$tmp/i_directive_scoping.cc" \
	tests/i_directive_scoping.pp

generated=$tmp/i_directive_scoping.cc

for expectation in \
	'::u_system::p_reset(p_defaultresetfile' \
	'::u_system::p_reset(p_checkedresetfile' \
	'::u_system::m_unchecked_reset(p_uncheckedresetfile' \
	'::u_system::p_rewrite(p_checkedrewritefile' \
	'::u_system::m_unchecked_rewrite(p_uncheckedrewritefile' \
	'::u_system::p_close(p_checkedclosefile' \
	'::u_system::m_unchecked_close(p_uncheckedclosefile' \
	'::u_system::p_seek(p_checkedseekfile' \
	'::u_system::m_unchecked_seek(p_uncheckedseekfile' \
	'::u_system::p_filepos(p_checkedpositionfile' \
	'::u_system::m_unchecked_filepos(p_uncheckedpositionfile' \
	'::u_system::p_filesize(p_checkedsizefile' \
	'::u_system::m_unchecked_filesize(p_uncheckedsizefile' \
	'::u_system::p_eof(p_checkedeoffile' \
	'::u_system::m_unchecked_eof(p_uncheckedeoffile' \
	'::u_system::p_truncate(p_checkedtruncatefile' \
	'::u_system::m_unchecked_truncate(p_uncheckedtruncatefile' \
	'::u_system::p_blockread(p_checkedreadfile' \
	'::u_system::m_unchecked_blockread(p_uncheckedreadfile' \
	'::u_system::p_blockwrite(p_checkedwritefile' \
	'::u_system::m_unchecked_blockwrite(p_uncheckedwritefile' \
	'::u_system::p_write(' \
	'::u_system::m_unchecked_write(' \
	'::u_system::p_writeln(' \
	'::u_system::m_unchecked_writeln(' \
	'::u_system::p_reset(p_anchoredcheckedfile' \
	'::u_system::m_unchecked_reset(p_anchoreduncheckedfile' \
	'::u_system::m_unchecked_reset(p_aliasuncheckedfile' \
	'::u_system::p_reset(p_aliascheckedfile' \
	'p_resetproc = &::u_system::p_reset' \
	'p_defaultiison'
do
	if ! rg -Fq "$expectation" "$generated"
	then
		echo "missing I/O directive lowering: $expectation" >&2
		exit 1
	fi
done

if test "$(rg -Fc 'p_reset(p_marker)' "$generated")" -ne 2
then
	echo "a user Reset declaration was changed by caller I/O state" >&2
	exit 1
fi
if rg -Fq 'm_unchecked_reset(p_marker)' "$generated"
then
	echo "a user Reset declaration acquired System I/O lowering" >&2
	exit 1
fi

"${CXX:-g++}" \
	-std=c++20 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Irtl \
	-I"$tmp" \
	-fsyntax-only \
	"$generated" \
	"$tmp/system.cc"

if ./mp -Furtl -o"$tmp/rejected.cc" \
	tests/iochecks_invalid.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an invalid IOCHECKS setting" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/iochecks_invalid.error)
if ! rg -Fq -- "$expected" "$tmp/stderr"
then
	echo "wrong IOCHECKS diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "I directive-scoping tests passed"
