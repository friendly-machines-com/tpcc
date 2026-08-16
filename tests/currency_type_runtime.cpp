#include <type_traits>

#define TPCC_TEST_GENERATED_PROGRAM "currency_type.cc"
#include "generated_program_runtime.h"

int main() {
	static_assert(sizeof(::u_system::t_currency) == 8);
	static_assert(alignof(::u_system::t_currency) == alignof(::u_system::t_int64));
	static_assert(std::is_trivially_copyable_v<::u_system::t_currency>);
	static_assert(std::is_standard_layout_v<::u_system::t_currency>);

	if (tpcc_run_generated_program() != 0) {
		return 1;
	}
	if (::u_system::m_currency_raw(p_exacttenth) != 1000 || ::u_system::m_currency_raw(p_halftoevenzero) != 0 || ::u_system::m_currency_raw(p_halftoeventwo) != 2) {
		return 2;
	}
	if (::u_system::m_currency_raw(p_foldedsum) != 37500 || ::u_system::m_currency_raw(p_runtimesum) != 37500) {
		return 3;
	}
	if (::u_system::m_currency_raw(p_foldedproduct) != 24690 || ::u_system::m_currency_raw(p_runtimeproduct) != 25000) {
		return 4;
	}
	if (::u_system::m_currency_raw(p_frominteger) != 420000 || ::u_system::m_currency_raw(p_fromintegeralias) != 420000 || ::u_system::m_currency_raw(p_fromreal) != 31250 || ::u_system::m_currency_raw(p_fromcall) != 1000) {
		return 5;
	}
	if (p_asdouble != 1.25 || p_asinteger != 1 || p_quotient != 1.5) {
		return 6;
	}
	if (p_currencyless != ::u_system::p_true || p_currencysize != 8 || p_raw != 1000) {
		return 7;
	}
	if (p_literaldomain != 3 || p_literalaliasdomain != 3 || p_typedcurrencydomain != 4) {
		return 8;
	}
	if (p_parsedexactcode != 0 || p_parsedhalfevenzerocode != 0 || p_parsedhalfeventwocode != 0 || p_parsedexponentcode != 0 || p_parsedmaximumcode != 0 || p_parsedansicode != 0) {
		return 9;
	}
	if (::u_system::m_currency_raw(p_parsedexact) != 1234567 || ::u_system::m_currency_raw(p_parsedhalfevenzero) != 0 || ::u_system::m_currency_raw(p_parsedhalfeventwo) != 2 || ::u_system::m_currency_raw(p_parsedexponent) != -1234565 || ::u_system::m_currency_raw(p_parsedmaximum) != INT64_MAX || ::u_system::m_currency_raw(p_parsedwithoutcode) != 31250 || ::u_system::m_currency_raw(p_parsedansi) != -2) {
		return 10;
	}
	if (p_parsedinvalidcode != 3 || p_parsedoverflowcode == 0 || ::u_system::m_currency_raw(p_parsedinvalid) != 0 || ::u_system::m_currency_raw(p_parsedoverflow) != 0) {
		return 11;
	}
	if (p_formattedrounded.m_string() != "26.00" || p_formattedhalf.m_string() != "1.00" || p_formattedmaximum.m_string() != "922337203685477.5807" || p_roundtriptext.m_string() != "922337203685477.5807" || p_roundtripcode != 0 || ::u_system::m_currency_raw(p_roundtripvalue) != INT64_MAX) {
		return 12;
	}
	return 0;
}
