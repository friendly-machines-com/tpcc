#!/bin/sh
set -eu

. "$(dirname -- "$0")/testlib.sh"


tpcc_translate -o"$tmp/type_block_publication.cc" \
	tests/type_block_publication.pp

if ! grep -Fq 'struct t_tbase : public ::u_system::t_tobject' \
	"$tmp/type_block_publication.cc"
then
	echo "bare class did not implicitly inherit System.TObject" >&2
	exit 1
fi
if grep -Eq '^struct t_tobject :' "$tmp/system.h"
then
	echo "System.TObject incorrectly received an implicit superclass" >&2
	exit 1
fi
if ! grep -Fq 'struct t_tchild : public t_tbase' \
	"$tmp/type_block_publication.cc"
then
	echo "same-block completed superclass was not emitted as the base" >&2
	exit 1
fi
if ! grep -Fq 't_tbase::p_getvalue()' \
	"$tmp/type_block_publication.cc"
then
	echo "inherited method did not resolve through the completed superclass" >&2
	exit 1
fi
if ! grep -Fq 'this->p_setvalue(' \
	"$tmp/type_block_publication.cc"
then
	echo "implicit Self did not resolve an inherited-only method" >&2
	exit 1
fi
if ! grep -Fq 'this->p_value = ' \
	"$tmp/type_block_publication.cc"
then
	echo "implicit Self did not resolve an inherited-only field" >&2
	exit 1
fi
if ! grep -Fq 'struct t_tforward;' \
	"$tmp/type_block_publication.cc"
then
	echo "explicit class forward did not emit a C++ forward declaration" >&2
	exit 1
fi
if ! grep -Fq 't_tforward* p_ref;' \
	"$tmp/type_block_publication.cc"
then
	echo "an earlier class did not retain the forward class identity" >&2
	exit 1
fi
if ! grep -Fq \
	'::u_system::m_classref<t_tforward>* p_meta;' \
	"$tmp/type_block_publication.cc"
then
	echo "class-of-forward did not use the incomplete-safe class-reference carrier" >&2
	exit 1
fi
if ! grep -Fq \
	'::u_system::m_classref<t_trightforward>* p_rightclass;' \
	"$tmp/type_block_publication.cc" ||
   ! grep -Fq \
	'::u_system::m_classref<t_tleftforward>* p_leftclass;' \
	"$tmp/type_block_publication.cc"
then
	echo "mutually dependent class-of-forward carriers were not emitted" >&2
	exit 1
fi
if ! grep -Fq 't_tforward* p_forwardglobal;' \
	"$tmp/type_block_publication.cc"
then
	echo "a variable between forward declaration and completion lost its class type" >&2
	exit 1
fi
if ! grep -Fq 't_tforwarduser* p_user;' \
	"$tmp/type_block_publication.cc"
then
	echo "the completed class did not retain the mutually-referencing class type" >&2
	exit 1
fi

tpcc_build "$tmp/type_block_publication" \
	"$tmp/type_block_publication.cc" \
	"$tmp/system.cc"
tpcc_run "$tmp/type_block_publication"

for test_case in \
	TEST_UNRESOLVED_CLASS_FORWARD \
	TEST_DUPLICATE_CLASS_FORWARD \
	TEST_WRONG_CLASS_FORWARD_COMPLETION \
	TEST_FORWARD_CLASS_SUPER \
	TEST_EXTERNAL_NIL_REJECTED
do
	if tpcc_translate -d"$test_case" \
		-o"$tmp/rejected.cc" \
		tests/type_block_publication.pp \
		>"$tmp/stdout" 2>"$tmp/stderr"
	then
		echo "accepted invalid class-forward case $test_case" >&2
		exit 1
	fi
done

tpcc_translate -o"$tmp/inherited_overload_scope.cc" \
	tests/inherited_overload_scope.pp
if ! grep -Fq 'this->p_base_select(p_i)' \
	"$tmp/inherited_overload_scope.cc"
then
	echo "inherited member overload was absent from the class Frame lookup" >&2
	exit 1
fi
if ! grep -Fq 'this->p_child_select(p_q)' \
	"$tmp/inherited_overload_scope.cc"
then
	echo "local member overload was absent from the class Frame lookup" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/member_global_scope.cc" \
	tests/member_global_scope.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "mixed a member overload family with a global family" >&2
	exit 1
fi
expected=$(sed -n '1p' tests/member_global_scope.error)
if ! grep -Fq "$expected" "$tmp/stderr"
then
	echo "wrong member/global scope diagnostic; expected: $expected" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

if tpcc_translate -o"$tmp/rejected.cc" \
	tests/type_block_unresolved_super.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted an unresolved forward declaration as a superclass" >&2
	exit 1
fi
if ! grep -Fq \
	"superclass forward declaration 'tfuture' must be resolved before it is inherited" \
	"$tmp/stderr"
then
	echo "wrong diagnostic for unresolved forward superclass" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

if ./mp -Futests/system_without_tobject \
	-o"$tmp/missing_tobject.cc" \
	tests/system_without_tobject/implicit_class.pp \
	>"$tmp/stdout" 2>"$tmp/stderr"
then
	echo "accepted implicit inheritance without System.TObject" >&2
	exit 1
fi
if ! grep -Fq \
	"implicit class inheritance requires System.TObject" \
	"$tmp/stderr"
then
	echo "wrong diagnostic for missing System.TObject" >&2
	sed -n '1,20p' "$tmp/stderr" >&2
	exit 1
fi

echo "type-block publication tests passed"
