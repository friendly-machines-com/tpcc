#include "directive_expr.h"
#include <cctype>
#include <charconv>
#include <cstdint>
#include <functional>
#include <system_error>

bool eval_directive_expr(const std::string& expr,
                         const std::map<std::string, std::string>& defines)
{
	size_t p = 0;
	auto skip_ws = [&]() {
		while (p < expr.size() && (expr[p] == ' ' || expr[p] == '\t')) p++;
	};
	auto peek_word = [&]() -> std::string {
		skip_ws();
		size_t q = p;
		while (q < expr.size() && (isalnum((unsigned char)expr[q]) || expr[q] == '_')) q++;
		return expr.substr(p, q - p);
	};
	auto lower = [](const std::string& s) {
		std::string r;
		for (char c : s) r.push_back((char)tolower((unsigned char)c));
		return r;
	};
	auto require_bool = [&](int64_t v, const char* where) -> bool {
		if (v == 0) return false;
		if (v == 1) return true;
		throw DirectiveExprError{
			std::string("non-boolean value ") + std::to_string(v)
			+ " where boolean required (" + where + ") in {$if ...}"};
	};
	auto lookup_ident = [&](const std::string& name) -> int64_t {
		auto it = defines.find(name);
		if (it == defines.end()) {
			throw DirectiveExprError{"undefined identifier '" + name + "' in {$if ...}"};
		}
		const std::string& s = it->second;
		if (s.empty()) {
			throw DirectiveExprError{"identifier '" + name
				+ "' has no value (defined without :=) but is used numerically in {$if ...}"};
		}
		int64_t v;
		auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
		if (ec != std::errc() || ptr != s.data() + s.size()) {
			throw DirectiveExprError{"value '" + s + "' of '" + name
				+ "' is not an integer in {$if ...}"};
		}
		return v;
	};
	std::function<int64_t()> parse_or;
	std::function<int64_t()> parse_term = [&]() -> int64_t {
		skip_ws();
		if (p < expr.size() && expr[p] == '(') {
			p++;
			int64_t v = parse_or();
			skip_ws();
			if (p >= expr.size() || expr[p] != ')') {
				throw DirectiveExprError{"missing ')' in {$if ...} expression"};
			}
			p++;
			return v;
		}
		if (p < expr.size() && (isdigit((unsigned char)expr[p])
			|| (expr[p] == '-' && p + 1 < expr.size() && isdigit((unsigned char)expr[p+1])))) {
			size_t q = p;
			if (expr[q] == '-') q++;
			while (q < expr.size() && isdigit((unsigned char)expr[q])) q++;
			int64_t v;
			auto [ptr, ec] = std::from_chars(expr.data() + p, expr.data() + q, v);
			if (ec != std::errc() || ptr != expr.data() + q) {
				throw DirectiveExprError{"malformed integer '" + expr.substr(p, q - p) + "' in {$if ...}"};
			}
			p = q;
			return v;
		}
		std::string w = peek_word();
		std::string lw = lower(w);
		if (lw == "not") {
			p += w.size();
			return require_bool(parse_term(), "operand of 'not'") ? 0 : 1;
		}
		if (lw == "defined") {
			p += w.size();
			skip_ws();
			if (p >= expr.size() || expr[p] != '(') {
				throw DirectiveExprError{"expected '(' after 'defined' in {$if ...}"};
			}
			p++;
			std::string sym = peek_word();
			if (sym.empty()) throw DirectiveExprError{"expected identifier in defined(...)"};
			p += sym.size();
			skip_ws();
			if (p >= expr.size() || expr[p] != ')') {
				throw DirectiveExprError{"missing ')' in defined(...)"};
			}
			p++;
			return defines.count(sym) > 0 ? 1 : 0;
		}
		if (!w.empty()) {
			p += w.size();
			return lookup_ident(w);
		}
		throw DirectiveExprError{"unrecognised token in {$if ...}: '" + expr.substr(p) + "'"};
	};
	auto parse_cmp = [&]() -> int64_t {
		int64_t a = parse_term();
		skip_ws();
		if (p < expr.size() && (expr[p] == '<' || expr[p] == '>' || expr[p] == '=')) {
			char op = expr[p++];
			int64_t b = parse_term();
			switch (op) {
				case '<': return a <  b ? 1 : 0;
				case '>': return a >  b ? 1 : 0;
				case '=': return a == b ? 1 : 0;
			}
		}
		return a;
	};
	auto parse_and = [&]() -> int64_t {
		bool v = require_bool(parse_cmp(), "operand of 'and'");
		while (true) {
			skip_ws();
			if (lower(peek_word()) != "and") break;
			p += 3;
			v = require_bool(parse_cmp(), "operand of 'and'") && v;
		}
		return v ? 1 : 0;
	};
	parse_or = [&]() -> int64_t {
		bool v = require_bool(parse_and(), "operand of 'or'");
		while (true) {
			skip_ws();
			if (lower(peek_word()) != "or") break;
			p += 2;
			v = require_bool(parse_and(), "operand of 'or'") || v;
		}
		return v ? 1 : 0;
	};
	int64_t result = parse_or();
	skip_ws();
	if (p != expr.size()) {
		throw DirectiveExprError{"unrecognised trailing form in {$if ...}: '"
			+ expr.substr(p) + "'"};
	}
	return require_bool(result, "top-level {$if ...} value");
}
