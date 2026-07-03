#pragma once
#include <map>
#include <string>
#include "ci_less.h"

// Thrown by eval_directive_expr on any grammar or semantic error. The caller
// is expected to catch it and attach location context (file, line) before
// surfacing the message to the user.
struct DirectiveExprError {
	std::string message;
};

// Evaluate a `{$if ...}` condition body (the text between `{$if ` and `}`,
// with directive name and trailing space already stripped). Grammar:
//   expr    := or_expr
//   or_expr := and_expr ('or' and_expr)*
//   and_expr:= cmp_expr ('and' cmp_expr)*
//   cmp_expr:= term (('<'|'>'|'=') term)?
//   term    := 'not' term
//            | 'defined' '(' IDENT ')'
//            | '(' expr ')'
//            | INT
//            | IDENT
// Values internally are int64. `defined()` and compares yield 0/1.
// `not`/`and`/`or` require operands in {0,1}. IDENT resolves via DEFINES:
// its stored value is parsed as int64; empty or absent value is an error.
// Top-level result must itself be 0 or 1.
bool eval_directive_expr(const std::string& expr,
                         const std::map<std::string, std::string, CILess>& defines);
