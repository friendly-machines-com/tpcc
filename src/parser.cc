#include "parser.h"
#include "builtins.h"
#include "cst.h"
#include "directive_expr.h"
#include "emit.h"
#include "evaluator.h"
#include "frame.h"
#include "units.h"
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_set>

static std::unordered_set<std::string> keywords = {
    "abstract",
    "and", // operator
    "array",
    "as",
    "begin",
    "case",
    "class",
    "const",
    "constructor",
    "destructor",
    "div", // operator
    "do",
    "dynamic", // FIXME
    "else",
    "end",
    "forward", // FIXME directive ?
    "function",
    "if",
    "implementation",
    "inherited",
    "interface",
    "mod", // operator
    "nil",
    "not", // operator
    "object",
    "of",
    "operator",
    "or",  // operator
    "out", // FIXME directive ?
    "overload",
    "override",
    "packed",
    "procedure",
    "program",
    "record",
    "repeat",
    "set",
    "shl", // operator
    "shr", // operator
    "string",
    "then",
    "type",
    "unit",
    "until",
    "uses",
    "var",
    "virtual", // FIXME directive ?
    "while",
    "with",
    "xor", // operator
};

Parser::Parser(UnitRegistry* unit_registry, Emitter* emitter, CompilerOptions* options)
    : unit_registry(unit_registry), emitter(emitter), options(options) {
}
void Parser::pop_input_file() {
	assert(!input_files.empty());
	fclose(input_files.back().input_file);
	input_files.pop_back();
	if (input_files.empty()) {
		input_file = nullptr;
		input_file_name = "";
		input_file_line_number = 0;
		input_char = EOF;
		return;
	}
	auto& p = input_files.back();
	input_file = p.input_file;
	input_file_name = p.input_file_name;
	input_file_line_number = p.input_file_line_number;
	input_char = fgetc(input_file);
}
int Parser::consume_lowlevel() {
	int result = input_char;
	if (result == '\n') {
		++this->input_file_line_number;
	}
	if (!input_file) {
		input_char = EOF;
		return result;
	}
	input_char = fgetc(input_file);
	// If the current source is exhausted and there's a parent to fall back
	// to, pop and re-seed input_char from the parent's stream (which has
	// the char that was pending at push time waiting via ungetc).
	while (input_char == EOF && input_files.size() > 1) {
		pop_input_file();
	}
	return result;
}
void Parser::push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number) {
	push_input_file_and_buffer(input_file, input_file_name, input_file_line_number, nullptr, 0);
}
void Parser::push_input_file_and_buffer(FILE* input_file, std::string input_file_name, int input_file_line_number, std::unique_ptr<char[]> buffer, size_t buffer_len) {
	// If we're mid-parse, the tokenizer has one character already read from
	// the current source sitting in input_char. Push it back onto that
	// FILE*'s stream so the parent resumes on exactly the right byte after
	// this new source is popped.
	if (!input_files.empty() && input_char != EOF) {
		ungetc(input_char, this->input_file);
	}
	// Freeze the current line number into the outgoing top entry so pop
	// restores the caller's position, not the caller's start line.
	if (!input_files.empty()) {
		input_files.back().input_file_line_number = this->input_file_line_number;
	}
	input_files.push_back(ParserInputFile{
	    .input_file = input_file,
	    .input_file_name = input_file_name,
	    .input_file_line_number = input_file_line_number,
	    .owned_buffer = std::move(buffer),
	    .owned_buffer_len = buffer_len,
	});
	this->input_file = input_file;
	this->input_file_name = input_file_name;
	this->input_file_line_number = input_file_line_number;
	input_char = fgetc(input_file);
}

void Parser::push_scope(const Frame* scope) {
	this->scopes.push_back(ScopeEntry{scope, nullptr, current_type_block});
	current_type_block = const_cast<Frame*>(scope);
}

// `with` introduces an alias overlay for value lookup only (member names of
// the with-target). It is NOT a declaration site -- new type/var/const decls
// encountered inside the with-body still belong to the enclosing declaration
// Frame, so we don't update current_type_block here. We DO push a ScopeEntry
// so name-resolution walks through the with-target's frame; on pop, the
// saved_type_block we record (the enclosing decl Frame, untouched) restores
// trivially.
void Parser::push_with_scope(const Frame* scope, Node* unwrap_via) {
	this->scopes.push_back(ScopeEntry{scope, unwrap_via, current_type_block});
}

void Parser::pop_scope() {
	if (this->scopes.empty()) {
		fprintf(stderr, "internal compiler error: pop_scope on empty scope stack\n");
		abort();
	}
	current_type_block = scopes.back().saved_type_block;
	this->scopes.pop_back();
}

[[noreturn]] static void emit_parse_error(const std::string& file, int line, const std::string& message) {
	std::stringstream sst;
	sst << file << '(' << line << ')' << ':' << ' ' << message << std::endl;
	std::string r = sst.str();
	fprintf(stderr, "%s\n", r.c_str());
	fflush(stderr);
	exit(1);
}

[[noreturn]] void Parser::raise_parse_error(std::string message) {
	emit_parse_error(input_file_name, input_file_line_number, message);
}

[[noreturn]] Type* Parser::raise_type_parse_error(std::string message) {
	emit_parse_error(input_file_name, input_file_line_number, message);
}

bool Parser::is_defined(const std::string& sym) const {
	return options && options->defines.count(sym) > 0;
}

// See directive_expr.h/cc for the grammar and semantics; this is only the
// glue that attaches the parser's file/line context to any error the
// standalone evaluator raises.
bool Parser::eval_directive_expr(const std::string& expr) {
	static const std::map<std::string, std::string, CILess> empty;
	try {
		return ::eval_directive_expr(expr, options ? options->defines : empty);
	} catch (const DirectiveExprError& e) {
		raise_parse_error(e.message);
	}
}

// Split BODY into (directive_name_lowercased, argument-after-name-trimmed).
static std::pair<std::string, std::string> split_directive(const std::string& body) {
	size_t p = 0;
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t'))
		p++;
	std::string name;
	while (p < body.size() && (isalnum((unsigned char)body[p]) || body[p] == '_')) {
		name.push_back((char)tolower((unsigned char)body[p]));
		p++;
	}
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t'))
		p++;
	std::string rest = body.substr(p);
	while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r' || rest.back() == '\n'))
		rest.pop_back();
	return {name, rest};
}

// Extract the content of a single-quoted string literal token (with '' escape).
// FIXME: Remove and use evaluate().
static std::string extract_string_literal(const std::string& token) {
	std::string s;
	for (size_t i = 1; i + 1 < token.size(); ++i) {
		s.push_back(token[i]);
		if (token[i] == '\'' && i + 2 < token.size() && token[i + 1] == '\'')
			++i;
	}
	return s;
}

// Try to open NAME (used verbatim -- extension is the caller's job) by
// searching the directory of CURRENT_INPUT first, then each entry of
// SEARCH_PATHS (normalized to end in '/'). Returns the opened FILE and the
// path that worked, or {nullptr, ""}.
static std::pair<FILE*, std::string> search_for_file(
    const std::string& name,
    const std::string& current_input,
    const std::vector<std::string>& search_paths) {
	std::vector<std::string> dirs;
	std::string cur_dir;
	auto slash = current_input.find_last_of('/');
	if (slash != std::string::npos)
		cur_dir = current_input.substr(0, slash + 1);
	dirs.push_back(cur_dir);
	for (auto& d : search_paths) {
		std::string s = d;
		if (!s.empty() && s.back() != '/')
			s.push_back('/');
		dirs.push_back(s);
	}
	for (auto& d : dirs) {
		std::string candidate = d + name;
		if (FILE* f = fopen(candidate.c_str(), "r"))
			return {f, candidate};
	}
	return {nullptr, ""};
}

std::string Parser::expand_include_macro(const std::string& rest) {
	if (rest != "%DATE%")
		raise_parse_error("unsupported include macro: " + rest);
	auto now = std::chrono::system_clock::now();
	auto zoned = std::chrono::current_zone()->to_local(now);
	auto today = std::chrono::floor<std::chrono::days>(zoned);
	std::chrono::year_month_day ymd{today};
	return std::format("'{:%Y/%m/%d}'", ymd);
}

void Parser::handle_directive(const std::string& body) {
	auto [name, rest] = split_directive(body);
	if (name == "ifdef" || name == "ifndef") {
		bool outer = current_active();
		bool cond = is_defined(rest);
		if (name == "ifndef")
			cond = !cond;
		ifdef_stack.push_back({outer, cond, outer && cond});
		return;
	}
	if (name == "if") {
		bool outer = current_active();
		bool cond = eval_directive_expr(rest);
		ifdef_stack.push_back({outer, cond, outer && cond});
		return;
	}
	if (name == "else") {
		if (ifdef_stack.empty())
			raise_parse_error("$else without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken;
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "elseif") {
		if (ifdef_stack.empty())
			raise_parse_error("$elseif without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken && eval_directive_expr(rest);
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "endif" || name == "ifend") {
		if (ifdef_stack.empty())
			raise_parse_error("$endif without matching $ifdef");
		ifdef_stack.pop_back();
		return;
	}
	if (!current_active())
		return;
	if (name == "define") {
		// `{$define X}` sets X with no value; `{$define X := VALUE}` stores
		// VALUE (trimmed) so numeric-compare {$if X < N} etc. can consume it.
		if (options) {
			auto eq = rest.find(":=");
			if (eq == std::string::npos) {
				options->defines[rest] = "";
			} else {
				std::string sym = rest.substr(0, eq);
				while (!sym.empty() && (sym.back() == ' ' || sym.back() == '\t'))
					sym.pop_back();
				std::string val = rest.substr(eq + 2);
				size_t v0 = 0;
				while (v0 < val.size() && (val[v0] == ' ' || val[v0] == '\t'))
					v0++;
				val.erase(0, v0);
				while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
					val.pop_back();
				options->defines[sym] = val;
			}
		}
		return;
	}
	if (name == "undef") {
		if (options)
			options->defines.erase(rest);
		return;
	}
	if (name == "i" || name == "include") {
		if (rest.size() >= 2 && rest.front() == '%' && rest.back() == '%') {
			std::string literal = expand_include_macro(rest);
			size_t n = literal.size();
			auto buf = std::make_unique<char[]>(n);
			memcpy(buf.get(), literal.data(), n);
			FILE* f = fmemopen(buf.get(), n, "r");
			if (!f)
				raise_parse_error("fmemopen failed for {$I " + rest + "}");
			push_input_file_and_buffer(f, "<" + rest + ">", 1, std::move(buf), n);
			return;
		}
		std::vector<std::string> empty;
		auto [f, path] = search_for_file(rest, input_file_name,
						 options ? options->include_search_paths : empty);
		if (!f)
			raise_parse_error("cannot open include file: " + rest);
		push_input_file(f, path, 1);
		return;
	}
	// Other directives are accepted here for now. As we hit code where the
	// current no-op is wrong, tighten by name.
}

std::string Parser::consume() {
	std::stringstream sst;
	sst.str("");
	while (input_char == ' ' || input_char == '\n' || input_char == '\r') {
		consume_lowlevel();
	}
	if (input_char == EOF) {
		input_token = "";
		return "";
	}
	if ((input_char >= 'a' && input_char <= 'z') | (input_char >= 'A' && input_char <= 'Z') || input_char == '_') {
		while ((input_char >= 'a' && input_char <= 'z') || (input_char >= 'A' && input_char <= 'Z') || input_char == '_' || (input_char >= '0' && input_char <= '9')) {
			sst << (char)tolower(input_char);
			consume_lowlevel();
		}
	} else if (input_char == '$') {
		sst << (char)input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || (input_char >= 'a' && input_char <= 'f') || (input_char >= 'A' && input_char <= 'F') || input_char == '.' || input_char == '_') {
			sst << (char)tolower(input_char);
			consume_lowlevel();
		}
	} else if ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
		while ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '#') {
		sst << (char)input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '<') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '<' || input_char == '>') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '>') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '>' || input_char == '<') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == ':') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '/') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '/') { // line comment
			sst << (char)input_char;
			consume_lowlevel();
			while (input_char != EOF && input_char != '\n') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char == '\n') {
				sst << (char)input_char;
				consume_lowlevel();
				return consume();
			} else {
				raise_parse_error("missing newline");
			}
		}
	} else if (input_char != EOF && strchr("=;,[]()@*+-^", input_char)) {
		sst << (char)input_char;
		consume_lowlevel();
	} else if (input_char == '\'') {
		sst << (char)input_char;
		consume_lowlevel();
		while (true) {
			while (input_char != EOF && input_char != '\'') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char != '\'') {
				raise_parse_error("missing end quote");
			}
			sst << (char)input_char;
			consume_lowlevel();
			// A doubled quote inside the string represents one literal
			// quote character; consume it and keep going.
			if (input_char != '\'')
				break;
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '{') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '$') {
			consume_lowlevel(); // skip $
			std::string body;
			while (input_char != EOF && input_char != '}') {
				body.push_back((char)input_char);
				consume_lowlevel();
			}
			if (input_char != '}')
				raise_parse_error("missing end comment");
			consume_lowlevel(); // skip }
			handle_directive(body);
			return consume();
		} else {
			while (input_char != EOF && input_char != '}') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char == '}') {
				sst << (char)input_char;
				consume_lowlevel();
				return consume();
			} else {
				raise_parse_error("missing end comment");
			}
		}
	} else {
		raise_parse_error("unknown input character");
	}
	auto text = sst.str();
	input_token = text;
	// Drop any token produced while an outer `{$ifdef}`/`{$if}` frame is
	// inactive. Directives are already handled in-line and never reach
	// here, so they still update the ifdef stack correctly.
	if (!current_active() && !text.empty())
		return consume();
	return text;
}
void Parser::start() {
	push_scope(&root_frame());
	consume();
}
bool Parser::peek_keyword(std::string s) {
	return input_token == s; // FIXME case insensitive
}
void Parser::parse_keyword(std::string s) {
	if (peek_keyword(s)) {
		consume();
	} else {
		raise_parse_error("expected keyword " + s);
	}
}
bool Parser::maybe_parse_keyword(std::string s) {
	if (peek_keyword(s)) {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_directive(std::string directive) {
	// A Pascal directive (private/public/protected/published/etc.) matches
	// like a keyword for our purposes. Distinguished from a true keyword in
	// that directives are only reserved within specific contexts (aggregate
	// type bodies, procedure attribute lists); outside those contexts they
	// remain usable as identifiers. The lexical match itself is identical.
	return maybe_parse_keyword(directive);
}
void Parser::parse_directive(std::string directive) {
	if (!maybe_parse_directive(directive)) {
		raise_parse_error("expected directive " + directive);
	}
}
bool Parser::peek_directive(std::string directive) {
	return peek_keyword(directive);
}
void Parser::parse_operator(std::string x) {
	/* TODO: Limit to:
*
+
-
-
/
:=
<
<=
=
>
>=
<>
and
div
mod
or
shl
shr
xor
*/
	if (!maybe_parse_keyword(x)) {
		raise_parse_error("expected operator " + x);
	}
}

/** Return the Frame that holds the fields/members of TY, or nullptr if TY
 *  doesn't have one (i.e. isn't a record/class/object). Transparently walks
 *  through an IncompleteType via `resolved`. Used by `with` to find the
 *  field namespace to push. */
static Frame* get_type_body_frame(Type* ty) {
	while (auto inc = dynamic_cast<IncompleteType*>(ty)) {
		if (!inc->resolved)
			return nullptr;
		ty = inc->resolved;
	}
	if (auto r = dynamic_cast<RecordType*>(ty))
		return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->children;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->children;
	return nullptr;
}

void Parser::maybe_parse_statement() {
	if (peek_keyword("end")) {
		return;
	}
	if (peek_keyword("return")) { // FIXME Exit
		consume();
		(void)parse_expression();
	} else if (peek_keyword("if")) {
		parse_keyword("if");
		auto condition = parse_expression();
		parse_keyword("then");
		if (emitter)
			emitter->emit_if_prologue(condition);
		parse_statement();
		if (maybe_parse_keyword("else")) {
			if (emitter)
				emitter->emit_if_else();
			parse_statement();
		}
		if (emitter)
			emitter->emit_if_epilogue();
	} else if (peek_keyword("while")) {
		parse_keyword("while");
		auto condition = parse_expression();
		parse_keyword("do");
		if (emitter)
			emitter->emit_while_prologue(condition);
		parse_statement();
		if (emitter)
			emitter->emit_while_epilogue();
	} else if (peek_keyword("repeat")) {
		parse_keyword("repeat");
		if (emitter)
			emitter->emit_repeat_prologue();
		parse_block_body();
		parse_keyword("until");
		auto condition = parse_expression();
		if (emitter)
			emitter->emit_repeat_epilogue(condition);
	} else if (peek_keyword("begin")) {
		parse_keyword("begin");
		parse_block_body();
		parse_keyword("end");
	} else if (peek_keyword("with")) {
		parse_keyword("with");
		// TODO: complex targets (`p^`, `arr[i]`, `f()`). For now the
		// target must be a simple variable so we can read Type* off its
		// StorageSlot; the alias-emission below is already correct for
		// arbitrary targets when we lift this restriction.
		auto id = parse_identifier();
		Node* target = resolve_value(id);
		auto target_slot = dynamic_cast<StorageSlot*>(target);
		if (!target_slot)
			raise_parse_error("with target must currently be a simple variable");
		Frame* body_frame = get_type_body_frame(target_slot->ty);
		if (!body_frame)
			raise_parse_error("with target's type has no field body");
		parse_keyword("do");
		std::string alias = emitter ? emitter->next_fresh_cxx_name("pas_with") : std::string("pas_with_x");
		auto alias_slot = new StorageSlot(cxx_value_name(alias), target_slot->ty);
		if (emitter)
			emitter->emit_with_prologue(alias_slot->cxx_name, target);
		push_with_scope(body_frame, alias_slot);
		parse_statement();
		pop_scope();
		if (emitter)
			emitter->emit_with_epilogue();
	} else {
		// A statement here is either an assignment (designator := expression)
		// or a call (designator, possibly with auto-call). Parse the LHS as
		// a raw designator so we don't auto-call in the assignment case.
		Node* lhs = parse_designator();
		if (maybe_parse_colon_equals()) {
			if (!is_assignable(lhs)) {
				raise_parse_error("LHS of ':=' is not assignable");
			}
			Node* rhs = parse_expression();
			auto assign = mk_assign(lhs, rhs);
			if (emitter)
				emitter->emit_statement(assign);
			return;
		}
		// Call statement: parse_designator already built the ProcCall for
		// explicit `foo(x)`; for bare `foo`, apply auto-call now.
		Node* call = maybe_auto_call(lhs);
		if (!dynamic_cast<ProcCall*>(call)) {
			raise_parse_error("statement is neither an assignment nor a call");
		}
		if (emitter)
			emitter->emit_statement(call);
	}
}

std::optional<std::string> Parser::maybe_parse_identifier() {
	auto result = input_token;
	if (keywords.find(result) != keywords.end()) {
		return {};
	}
	consume();
	return result;
}

std::string Parser::parse_identifier() {
	auto result = maybe_parse_identifier();
	if (!result) {
		(void)raise_parse_error("expected identifier");
	}
	return *result;
}

Node* Parser::maybe_parse_numeral() {
	auto input = input_token.data();
	auto input_size = input_token.size();
	uint64_t value;
	int base = 10;
	if (input_size > 0 && (isdigit(*input) || *input == '$' || *input == '.')) {
		if (*input == '$') {
			base = 16;
			++input;
			--input_size;
		}
		if (*input != '.') {
			auto [ptr, ec] = std::from_chars(input, input + input_size, value, base);
			if (ec != std::errc() || ptr != input + input_size) {
				raise_parse_error("malformed numeral: " + input_token);
			}
			// FIXME: continue for non-integer here.
			auto lit = new Integer(value, &untyped_integer_type());
			consume();
			return lit;
		} else {
			// FIXME: continue for non-integer here.
			raise_parse_error("unimplemented real numeral: " + input_token);
		}
	} else {
		return nullptr;
	}
}

Node* Parser::parse_numeral() {
	auto result = maybe_parse_numeral();
	if (!result) {
		raise_parse_error("expected numeral");
	} else {
		return result;
	}
}

/** Walk the scope stack top-down looking up a value-position name (variable,
 *  constant, procedure, function, builtin). Raise if not found. */
Node* Parser::resolve_value(std::string name) {
	std::vector<Callable*> collected;
	// Walk top-down. First hit shadows unless it's overload-marked; then
	// keep walking to aggregate additional overload-marked hits from lower
	// scopes (cross-unit overloading).
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		Node* hit = it->frame->lookup_value(name);
		if (!hit)
			continue;
		auto as_call = dynamic_cast<Callable*>(hit);
		auto as_set = dynamic_cast<OverloadSet*>(hit);
		if (collected.empty() && !as_call && !as_set) {
			// First (and terminating) hit is a non-callable value.
			if (it->unwrap_via) {
				auto m = new MemberAccess(it->unwrap_via, hit);
				m->ty = hit->ty;
				return m;
			}
			return hit;
		}
		if (as_call) {
			if (!as_call->has_overload_directive) {
				if (collected.empty())
					return as_call; // plain callable, first-hit wins
				break;			// shadowed by collected overloads above
			}
			collected.push_back(as_call);
		} else if (as_set) {
			// Every member of an OverloadSet already has has_overload_directive.
			for (auto* m : as_set->members)
				collected.push_back(m);
		} else {
			// Non-callable value below a collected overload block -- stop.
			break;
		}
	}
	if (collected.empty()) {
		raise_parse_error("unresolved value identifier: " + name);
		return nullptr;
	}
	if (collected.size() == 1)
		return collected[0];
	return new OverloadSet(std::move(collected));
}

/** value that can be assigned to */
Node* Parser::resolve_lvalue(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Node* hit = it->frame->lookup_value(name)) {
			if (it->unwrap_via) {
				auto m = new MemberAccess(it->unwrap_via, hit);
				m->ty = hit->ty;
				return m;
			}
			return hit;
		}
	}
	raise_parse_error("unresolved lvalue identifier: " + name);
	return nullptr;
}

/** Same as resolve_value but for type-position names.
 *  If allow_forward is true and NAME isn't in scope, register a fresh
 *  IncompleteType under NAME in current_type_block and return it. This is how
 *  `^TFoo` before TFoo is declared gets a placeholder. If allow_forward is
 *  false, an unresolved name is a hard error. */
Type* Parser::resolve_type(std::string name, bool allow_forward) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Type* hit = it->frame->lookup_type(name)) {
			return hit;
		}
	}
	if (allow_forward && current_type_block) {
		auto inc = new IncompleteType(name);
		current_type_block->register_type(name, inc);
		return inc;
	}
	raise_parse_error("unresolved type identifier: " + name);
	return nullptr;
}

Node* Parser::parse_value() {
	if (maybe_parse_opening_paren()) { // grouping paren
		auto result = parse_expression();
		parse_closing_paren();
		return result;
	}
	if (auto n = maybe_parse_numeral())
		return n;
	if (!input_token.empty() && input_token.front() == '\'') {
		std::string s;
		for (size_t i = 1; i + 1 < input_token.size(); ++i) {
			s.push_back(input_token[i]);
			if (input_token[i] == '\'' && i + 2 < input_token.size() && input_token[i + 1] == '\'')
				++i;
		}
		consume();
		return new String(std::move(s), shortstring_type());
	}
	// FIXME: bool literals also belong here (need enum-member support).
	auto id = parse_identifier();
	return resolve_value(id);
}

bool Parser::maybe_parse_at() {
	if (input_token == "@") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_period() {
	if (input_token == ".") {
		consume();
		return true;
	} else {
		return false;
	}
}
void Parser::parse_period() {
	if (!maybe_parse_period()) {
		raise_parse_error("missing period");
	}
}

bool Parser::maybe_parse_less_less() {
	if (input_token == "<<") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_greater_greater() {
	if (input_token == ">>") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_circumflex() {
	if (input_token == "^") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_equal() {
	if (input_token == "=") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_less_greater() {
	if (input_token == "<=") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_less() {
	if (input_token == "<") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_greater() {
	if (input_token == ">") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_less_equal() {
	if (input_token == "<=") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_greater_equal() {
	if (input_token == ">=") {
		consume();
		return true;
	} else {
		return false;
	}
}

// Small helper: is NODE a bare callable reference (Callable, OverloadSet, or
// a MemberAccess whose member is either)? Used both for the auto-call check
// and to decide whether to peel a MemberAccess in finalize_call.
static bool node_is_bare_callable(Node* n) {
	if (!n)
		return false;
	if (dynamic_cast<Callable*>(n) || dynamic_cast<OverloadSet*>(n))
		return true;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b);
	}
	return false;
}

Node* Parser::maybe_auto_call(Node* n) {
	if (!node_is_bare_callable(n))
		return n;
	// finalize_call handles the empty-args case: for a Callable it checks
	// that either no formals exist or all remaining formals have defaults;
	// for an OverloadSet it runs ranking and picks the parameterless winner.
	// A candidate that requires args will fail there with a clear error.
	std::vector<Node*> args;
	auto fc = finalize_call(n, args, /*name for error*/ "");
	auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
	call->ty = fc.callee ? fc.callee->ty : nullptr;
	return call;
}

// Static helpers used inside parse_designator's branches.
static Type* unwrap_incomplete(Type* ty) {
	while (auto inc = dynamic_cast<IncompleteType*>(ty)) {
		if (!inc->resolved)
			return ty;
		ty = inc->resolved;
	}
	return ty;
}
static Frame* body_frame_of(Type* ty) {
	if (auto r = dynamic_cast<RecordType*>(ty))
		return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->children;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->children;
	return nullptr;
}

Node* Parser::parse_designator() {
	Node* result = parse_value();
	while (true) {
		if (maybe_parse_period()) {
			// Binary infix: RHS is a single identifier token. Before applying,
			// if LHS is a bare callable it must be auto-called (else the `.`
			// would try to look up a member of a callable, which is nonsense).
			result = maybe_auto_call(result);
			std::string member_name = parse_identifier();
			Type* ct = unwrap_incomplete(result->ty);
			Frame* members = body_frame_of(ct);
			if (!members)
				raise_parse_error("member access on non-composite type");
			Node* member = members->lookup_value(member_name);
			if (!member)
				raise_parse_error("no member '" + member_name + "'");
			auto ma = new MemberAccess(result, member);
			ma->ty = member->ty;
			result = ma;
		} else if (maybe_parse_opening_paren()) {
			// Bracketed n-ary: RHS is a comma-separated list of expressions.
			// No auto-call before `(` -- this `(` IS the call.
			std::vector<Node*> args;
			if (input_token != ")") {
				args.push_back(parse_expression());
				while (maybe_parse_comma())
					args.push_back(parse_expression());
			}
			parse_closing_paren();
			auto fc = finalize_call(result, args, /*name for error*/ "");
			auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
			call->ty = fc.callee ? fc.callee->ty : nullptr;
			result = call;
		} else if (maybe_parse_opening_bracket()) {
			// Bracketed: RHS is a single expression. Auto-call bare callable
			// LHS first (indexing into a callable reference is nonsense).
			result = maybe_auto_call(result);
			Node* idx = parse_expression();
			parse_closing_bracket();
			Type* ct = unwrap_incomplete(result->ty);
			auto arr = dynamic_cast<FixedArrayType*>(ct);
			if (!arr)
				raise_parse_error("index on non-array type");
			auto ix = new Index(result, idx);
			ix->ty = arr->item_type;
			result = ix;
		} else if (maybe_parse_circumflex()) {
			// Postfix: no RHS. Auto-call bare callable LHS first (deref of a
			// callable reference is nonsense).
			result = maybe_auto_call(result);
			Type* ct = unwrap_incomplete(result->ty);
			auto p = dynamic_cast<PointerType*>(ct);
			if (!p)
				raise_parse_error("deref of non-pointer type");
			auto d = new Dereference(result);
			d->ty = p->item_type;
			result = d;
		} else {
			break;
		}
	}
	return result;
}

bool Parser::is_assignable(Node* n) {
	if (!n)
		return false;
	if (dynamic_cast<StorageSlot*>(n))
		return true;
	if (dynamic_cast<Dereference*>(n))
		return true;
	if (dynamic_cast<Index*>(n))
		return true;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<StorageSlot*>(ma->b) != nullptr;
	}
	return false;
}

Node* Parser::mk_arith(std::string id, Node* a, Node* b) {
    auto fn = resolve_value(id);
	std::vector<Node*> args;
	auto common_ty = common_arith_type(a->ty, b->ty);
	if (common_ty == nullptr) {
		args.push_back(a);
		args.push_back(b);
	} else {
		args.push_back(cast(a, common_ty));
		args.push_back(cast(b, common_ty));
	}
	auto fc = finalize_call(fn, args, /*name for error*/ "");
	auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
	call->ty = fc.callee ? fc.callee->ty : nullptr;
	return call;
}

Node* Parser::mk_assign(Node* a, Node* b) {
	// That's mostly a cast.
	auto fn = resolve_value(":=");
	std::vector<Node*> args;
	args.push_back(b);
	auto fc = finalize_call(fn, args, /*name for error*/ "");
	auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
	call->ty = fc.callee ? fc.callee->ty : nullptr;
	return new Assign(a, call);
}

Node* Parser::mk_compare(std::string id, Node* a, Node* b) {
    auto fn = resolve_value(id);
	std::vector<Node*> args;
	auto common_ty = common_arith_type(a->ty, b->ty);
	if (common_ty == nullptr) {
		args.push_back(a);
		args.push_back(b);
	} else {
		args.push_back(cast(a, common_ty));
		args.push_back(cast(b, common_ty));
	}
	auto fc = finalize_call(fn, args, /*name for error*/ "");
	auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
	call->ty = fc.callee ? fc.callee->ty : nullptr;
/*	if (call->ty->return_type != boolean_type()) {
		raise_parse_error("Custom comparison operator '" + id + "' return type should be Boolean but isn't");
	} FIXME */
	return call;
}

Node* Parser::mk_unary_same(std::string id, Node* x) {
    auto fn = resolve_value(id);
	std::vector<Node*> args;
	args.push_back(x);
	auto fc = finalize_call(fn, args, /*name for error*/ "");
	auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
	call->ty = fc.callee ? fc.callee->ty : nullptr;
/*	if (call->ty->return_type != x->ty) {
		raise_parse_error("Custom comparison operator '" + id + "' return type should be the same as arg type but isn't");
	} FIXME */
	return call;
}

Node* Parser::parse_power() {
	// FIXME **
	if (maybe_parse_keyword("not")) {
		return mk_unary_same("not", maybe_auto_call(parse_designator()));
	} else if (maybe_parse_at()) {
		auto x = parse_designator(); // @ takes a designator, not the auto-called value
		auto n = new AddrOf(x);
		n->ty = x->ty ? static_cast<Type*>(new PointerType(x->ty)) : nullptr;
		return n;
	} else if (maybe_parse_minus()) {
		return mk_unary_same("-", maybe_auto_call(parse_designator()));
	} else if (maybe_parse_plus()) {
		return mk_unary_same("+", maybe_auto_call(parse_designator()));
	} else {
		return maybe_auto_call(parse_designator());
	}
}

Node* Parser::parse_product() {
	auto result = parse_power();
	while (true) {
		if (maybe_parse_star()) {
			result = mk_arith("*", result, parse_power());
		} else if (maybe_parse_slash()) {
			result = mk_arith("/", result, parse_power());
		} else if (maybe_parse_keyword("div")) {
			result = mk_arith("div", result, parse_power());
		} else if (maybe_parse_keyword("mod")) {
			result = mk_arith("mod", result, parse_power());
		} else if (maybe_parse_keyword("and")) {
			result = mk_arith("and", result, parse_power());
		} else if (maybe_parse_keyword("shl")) {
			result = mk_arith("shl", result, parse_power());
		} else if (maybe_parse_keyword("shr")) {
			result = mk_arith("shr", result, parse_power());
		} else if (maybe_parse_keyword("as")) {
			// `x as T`: b is the parsed type-position expression whose ty is
			// the target. Result type is that target.
			auto rhs = parse_power();
			auto n = new Coerce(result, rhs);
			n->ty = rhs->ty;
			result = n;
		} else if (maybe_parse_less_less()) {
			result = mk_arith("shl", result, parse_power());
		} else if (maybe_parse_greater_greater()) {
			result = mk_arith("shr", result, parse_power());
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_sum() {
	auto result = parse_product();
	while (true) {
		if (maybe_parse_plus()) {
			result = mk_arith("+", result, parse_product());
		} else if (maybe_parse_minus()) {
			result = mk_arith("-", result, parse_product());
		} else if (maybe_parse_keyword("or")) {
			result = mk_arith("or", result, parse_product());
		} else if (maybe_parse_keyword("xor")) {
			result = mk_arith("xor", result, parse_product());
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_comparison() {
	auto result = parse_sum();
	while (true) {
		if (maybe_parse_equal()) {
			result = mk_compare("=", result, parse_sum());
		} else if (maybe_parse_less_greater()) {
			result = mk_compare("<>", result, parse_sum());
		} else if (maybe_parse_less()) {
			result = mk_compare("<", result, parse_sum());
		} else if (maybe_parse_greater()) {
			result = mk_compare(">", result, parse_sum());
		} else if (maybe_parse_less_equal()) {
			result = mk_compare("<=", result, parse_sum());
		} else if (maybe_parse_greater_equal()) {
			result = mk_compare(">=", result, parse_sum());
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_expression() {
	return parse_comparison();
}

/** Parse the body of a class/record/object. When `owner_class` is non-null,
 *  procedure/function declarations inside are parsed as method prototypes
 *  and registered with owner_class as their owner. */
Frame* Parser::parse_aggregate_type_body(Type* owner_class) {
	bool is_class = false; // "class method" etc.
	Frame* body = new Frame(nullptr);
	push_scope(body);
	std::string visibility = "published";
	do {
		if (peek_keyword("end"))
			break;
		if (maybe_parse_directive("published")) {
			visibility = "published";
			continue;
		} else if (maybe_parse_directive("public")) {
			visibility = "public";
			continue;
		} else if (maybe_parse_directive("protected")) {
			visibility = "protected";
			continue;
		} else if (maybe_parse_directive("private")) {
			visibility = "private";
			continue;
		} else if (peek_keyword("type")) {
			if (is_class) {
				raise_parse_error("class type unsupported");
			}
			parse_type_block(true);
			is_class = false;
		} else if (peek_keyword("const")) {
			if (is_class) {
				raise_parse_error("class const unsupported");
			}
			parse_const_block();
			is_class = false;
		} else if (peek_keyword("var")) {
			if (is_class) {
				raise_parse_error("class var unsupported");
			}
			parse_var_block();
			is_class = false;
		} else if (peek_keyword("class")) {
			if (is_class) {
				raise_parse_error("internal error: someone forgot to consume 'class'");
			}
			is_class = true;
			consume();
			if (dynamic_cast<ClassType*>(owner_class) != nullptr) {
				continue;
			} else {
				raise_parse_error("expected a class container");
			}
		} else if (peek_keyword("procedure") || peek_keyword("function") || peek_keyword("destructor") || peek_keyword("constructor")) {
			parse_method_prototype(body, owner_class, peek_keyword("function"), peek_keyword("destructor"), peek_keyword("constructor"), is_class);
			is_class = false;
			continue; // parse_method_prototype consumes its terminating ';'
		} else if (peek_keyword("case")) {
			auto rt = dynamic_cast<RecordType*>(owner_class);
			if (is_class) {
				raise_parse_error("variant part only valid in a record, not in a metaclass");
			}
			if (!rt)
				raise_parse_error("variant part only valid in a record");
			parse_record_variant(rt, body);
			break; // variant part must come last; do not require a trailing ';'
		} else {
			if (is_class) {
				raise_parse_error("class var unsupported");
			}
			// parse_var_block inlined
			auto member_name = parse_identifier();
			parse_colon();
			auto ty = parse_type_expression(false);
			body->register_variable(member_name, new StorageSlot(cxx_value_name(member_name), ty), ty);
		}
		if (input_token.size() && input_token != "end") {
			if (!maybe_parse_semicolon()) {
				raise_parse_error("missing semicolon");
			}
		} else {
			break;
		}
	} while (true);
	pop_scope();
	if (is_class) {
		raise_parse_error("internal error: someone forgot to consume 'class'");
	}
	return body;
}

void Parser::parse_record_variant(RecordType* rt, Frame* body) {
	parse_keyword("case");
	// `case <sel_name> ':' <TagType> of ...` introduces a selector field;
	// `case <TagType> of ...` is tag-less. Single-token lookahead: take an
	// identifier first; if ':' follows, it's the selector name (consume ':'
	// and parse the real tag type after it). Otherwise the identifier WAS
	// the tag type -- resolve it directly.
	auto first = parse_identifier();
	Type* tag_type;
	if (maybe_parse_colon()) {
		rt->has_selector = true;
		rt->selector_cxx_name = cxx_value_name(first);
		tag_type = parse_type_expression(false);
	} else {
		tag_type = resolve_type(first, false);
	}
	rt->selector_type = tag_type;
	parse_keyword("of");
	while (!peek_keyword("end")) {
		// Case label list -- comma-separated constant expressions. Values
		// are discarded: layout is a flat overlapping union regardless of
		// which label is active.
		parse_expression();
		while (maybe_parse_comma())
			parse_expression();
		parse_colon();
		parse_opening_paren();
		VariantArm arm;
		if (input_token != ")") {
			do {
				auto fname = parse_identifier();
				parse_colon();
				auto fty = parse_type_expression(false);
				auto slot = new StorageSlot(cxx_value_name(fname), fty);
				// Variant slots live in the SAME Frame as fixed fields
				// (Pascal requires globally-distinct field names within a
				// record, so duplicate-name detection via register_variable
				// is the language-level check we want) AND in the arm's
				// field list (preserves source order and arm grouping for
				// the emitter's union reconstruction). Storing the slot
				// pointer in both gives the emitter identity-based
				// "is this a variant slot?" without name lookups.
				body->register_variable(fname, slot, fty);
				arm.fields.push_back({slot, fty});
				if (!maybe_parse_semicolon())
					break;
			} while (input_token != ")");
		}
		parse_closing_paren();
		rt->arms.push_back(std::move(arm));
		if (!maybe_parse_semicolon())
			break;
	}
}

Type* Parser::parse_class_type() {
	parse_keyword("class");
	if (maybe_parse_keyword("of")) {
		auto target_ty = parse_type_expression(true);
		return lookup_builtin_type("pas::m_iobject");
		//return somehow target_ty->cxx_name + "::m_meta" but that would make the metaclass first-class;
	}
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("class inheritance (class(Parent)) not implemented yet");
	}
	auto ct = new ClassType(nullptr);
	ct->children = parse_aggregate_type_body(ct);
	parse_keyword("end");
	return ct;
}

Type* Parser::parse_record_type() {
	bool packed = false;
	if (maybe_parse_keyword("packed")) {
		packed = true;
	}
	parse_keyword("record");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("record with parenthesized header not implemented yet");
	}
	auto rt = new RecordType(nullptr, packed);
	rt->children = parse_aggregate_type_body(rt);
	parse_keyword("end");
	return rt;
}

Type* Parser::parse_object_type() {
	parse_keyword("object");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("object with parenthesized header not implemented yet");
	}
	auto ot = new ObjectType(nullptr);
	ot->children = parse_aggregate_type_body(ot);
	parse_keyword("end");
	return ot;
}

Type* Parser::parse_array_type() {
	parse_keyword("array");
	parse_opening_bracket();
	auto bounds_type = parse_type_expression(false);
	parse_closing_bracket();
	parse_keyword("of");
	auto item_type = parse_type_expression(false);
	return new FixedArrayType(bounds_type, item_type);
}

Type* Parser::parse_enum_type() {
	// Caller already consumed the `(` via maybe_parse_opening_paren in
	// parse_type_expression.
	auto et = new EnumType();
	int64_t next_value = 0;
	do {
		auto pas = parse_identifier();
		auto cxx = cxx_value_name(pas);
		// FIXME: explicit member values (`Red = 5`) accepted by ISO/FPC are
		// not parsed here -- every member takes next_value, then increments.
		et->members.push_back({cxx, next_value});
		// Register the member as a value in the enclosing scope so bare uses
		// (`c := Red`) resolve. Pascal's default is unscoped enum members:
		// they live in the same scope as the enum type itself, NOT inside
		// the type. (A future compiler might add `{$scopedenums+}` and route
		// them through the type; that is not this compiler.)
		auto ref = new EnumMemberRef(cxx, next_value, et);
		if (!current_type_block->register_variable(pas, ref, et))
			raise_parse_error("duplicate identifier: " + pas);
		++next_value;
		if (!maybe_parse_comma())
			break;
	} while (true);
	parse_closing_paren();
	return et;
}

std::string Parser::parse_string_literal() {
	if (input_token.empty() || input_token.front() != '\'') {
		raise_parse_error("expected string literal");
	}
	auto result = extract_string_literal(input_token);
	consume();
	return result;
}

/** allow_forward: if true, an unresolved identifier at this parse position is
 *  auto-registered as an IncompleteType in the current type block rather than
 *  raising. Only the pointer branch propagates true; compound-type sub-parses
 *  (record/array/set/etc.) reset to false because their contents need real,
 *  sized types. */
Type* Parser::parse_type_expression(bool allow_forward) {
	if (maybe_parse_opening_paren()) {
		return parse_enum_type();
	} else if (maybe_parse_circumflex()) {
		return new PointerType(parse_type_expression(true));
	} else if (peek_keyword("string")) {
		parse_keyword("string");
		parse_opening_bracket();
		parse_expression();
		parse_closing_bracket();
		return raise_type_parse_error("sized-string type (string[N]) not implemented yet");
	} else if (peek_keyword("set")) {
		parse_keyword("set");
		parse_keyword("of");
		return new FixedSetType(parse_type_expression(false));
	} else if (peek_keyword("array")) {
		return parse_array_type();
	} else if (peek_keyword("object")) {
		return parse_object_type();
	} else if (peek_keyword("packed") || peek_keyword("record")) {
		return parse_record_type();
	} else if (peek_keyword("class")) {
		return parse_class_type();
	} else if (peek_keyword("procedure")) {
		return parse_procedure_type();
	} else if (peek_keyword("function")) {
		return parse_function_type();
	} else if (peek_keyword("operator")) {
		return parse_operator_type();
	} else if (peek_directive("external")) { // usually primitive; otherwise we would miss a lot of info
		parse_directive("external");
		parse_keyword("nil"); // FIXME: allow string literals, eval.
		parse_directive("name");
		std::string cxx_name = parse_string_literal();

		auto intrinsic = lookup_external_type(nullptr, cxx_name);

		// auto intrinsic = new IntrinsicType(cxx_name); // FIXME: what? reuse or what?
		// lhs_placeholder->resolved = intrinsic;
		// scope->rebind_type(name, intrinsic);
		return intrinsic;
	} else {
		// FIXME: constant folding for ranges (2..5 -> BoundedCardinalType)
		auto id = parse_identifier();
		return resolve_type(id, allow_forward);
	}
}

void Parser::parse_statement() {
	maybe_parse_statement();
}
void Parser::parse_block_body() {
	while (input_token.size()) {
		maybe_parse_statement();
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
}
void Parser::parse_const_block() {
	parse_keyword("const");
	// See parse_var_block: register into the enclosing decl scope, no sub-frame.
	Frame* scope = const_cast<Frame*>(scopes.back().frame);
	do {
		auto name = parse_identifier();
		// FIXME: handle actual compile-time consts which have no colon (and are no variables).
		parse_colon();
		auto ty = parse_type_expression(false);
		scope->register_variable(name, new StorageSlot(cxx_value_name(name), ty), ty);
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
}
void Parser::maybe_parse_const_block() {
	if (peek_keyword("const")) {
		parse_const_block();
	}
}
/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually.

DELPHI_AUTO_END: will automatically stop at some aggregate control directives (like "public" etc).
 */
// Special cases this handles:
//   type PX = ^TX; TX = record ... end;   (cross-decl forward via pointer)
//   type TFoo = class x: TFoo end;        (self-recursive within one decl)
// Each LHS is pre-registered as an IncompleteType so self-refs in its RHS
// resolve. During RHS parsing, an unresolved identifier in pointer position
// is auto-registered as an IncompleteType in this scope (see resolve_type).
// After the RHS is parsed the LHS placeholder's `resolved` is filled and the
// frame slot is rebound to the real Type*. At block end any remaining
// unresolved IncompleteType in the scope is an error.
void Parser::parse_type_block(bool delphi_auto_end) {
	parse_keyword("type");
	// See parse_var_block: register into the enclosing decl scope, no sub-frame.
	// current_type_block still points at the enclosing decl Frame (set by the
	// most recent push_scope), which is what parse_enum_type and the IncompleteType
	// forward-decl machinery need: new types land in the same Frame that future
	// lookups (including cross-unit, after `uses`) walk.
	Frame* scope = const_cast<Frame*>(scopes.back().frame);
	// Track type names declared in THIS block so the unresolved-forward check
	// at the end can report a meaningful error rather than walking every type
	// in the enclosing scope (which would re-check already-defined siblings).
	std::vector<std::pair<std::string, IncompleteType*>> block_incompletes;
	do {
		auto name_optional = maybe_parse_identifier();
		if (!name_optional)
			break;
		auto name = *name_optional;
		parse_equals();
		Type* existing = scope->lookup_type(name); // FIXME: WTF
		IncompleteType* lhs_placeholder = nullptr;
		if (existing) {
			lhs_placeholder = dynamic_cast<IncompleteType*>(existing);
			if (!lhs_placeholder) {
				raise_type_parse_error("duplicate type name: " + name);
			}
		} else {
			lhs_placeholder = new IncompleteType(name);
			scope->register_type(name, lhs_placeholder);
		}
		block_incompletes.push_back({name, lhs_placeholder});
		Type* rhs = parse_type_expression(false);
		// Attach the LHS Pascal name (as its C++ identifier) to record-family
		// types and enums so emit_type_ref has a name to spell instead of
		// re-emitting the body inline at every use site.
		std::string cxx = cxx_type_name(name);
		// If rhs is an aggregate/enum that already carries a C++ name, this
		// binding is an alias (`type B = A;` where A was defined above). In
		// that case do NOT overwrite rhs->cxx_name (that would rename A's
		// canonical definition) and do NOT re-emit the body (ODR violation).
		// Emit a `using t_B = t_A;` instead. First-wins for the canonical
		// name; subsequent Pascal names become C++ aliases.
		std::string existing_cxx;
		if (auto r = dynamic_cast<RecordType*>(rhs))
			existing_cxx = r->cxx_name;
		else if (auto c = dynamic_cast<ClassType*>(rhs))
			existing_cxx = c->cxx_name;
		else if (auto o = dynamic_cast<ObjectType*>(rhs))
			existing_cxx = o->cxx_name;
		else if (auto e = dynamic_cast<EnumType*>(rhs))
			existing_cxx = e->cxx_name;
		if (!existing_cxx.empty() && existing_cxx != cxx) {
			if (emitter)
				emitter->emit_type_alias(cxx, existing_cxx);
		} else {
			if (auto r = dynamic_cast<RecordType*>(rhs))
				r->cxx_name = cxx;
			else if (auto c = dynamic_cast<ClassType*>(rhs))
				c->cxx_name = cxx;
			else if (auto o = dynamic_cast<ObjectType*>(rhs))
				o->cxx_name = cxx;
			else if (auto e = dynamic_cast<EnumType*>(rhs))
				e->cxx_name = cxx;
			if (emitter)
				emitter->emit_type_definition(cxx, rhs);
		}
		// Patch the placeholder's `resolved` pointer to the real Type*. Anyone
		// who captured the placeholder BEFORE this point (e.g. a `^TFoo` inside
		// the RHS that grabbed the placeholder through lookup_type) still holds
		// IncompleteType*; they deref through `resolved` to reach the real type.
		lhs_placeholder->resolved = rhs;
		// And replace the Frame binding (name -> placeholder) with (name -> rhs)
		// so future name lookups return the real Type* directly without needed
		// an unwrap step.
		scope->rebind_type(name, rhs);
		parse_semicolon();
	} while (true);
	for (auto& [name, inc] : block_incompletes) {
		if (inc && !inc->resolved) {
			raise_parse_error("forward-referenced type not defined in this type block: " + name);
		}
	}
}
/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually */
void Parser::maybe_parse_type_block(bool delphi_auto_end) {
	if (peek_keyword("type")) {
		parse_type_block(delphi_auto_end);
	}
}
void Parser::parse_var_block() {
	parse_keyword("var");
	// Register each var directly into the enclosing declaration scope
	// (unit interface_frame, program impl_frame, or procedure body_frame).
	// We deliberately do NOT create a sub-frame: the var decls must persist
	// past this block parse so callers in OTHER compilation units can resolve
	// them after `uses`. The `pushed++` in parse_decl_blocks used to compensate
	// for the push_scope here; with no push, parse_decl_blocks stays at 0 for
	// var blocks.
	Frame* scope = const_cast<Frame*>(scopes.back().frame);
	do {
		std::vector<std::string> names;
		auto name_optional = maybe_parse_identifier();
		if (!name_optional) {
			break;
		}
		auto name = *name_optional;
		names.push_back(name);
		while (maybe_parse_comma()) {
			auto name = parse_identifier();
			names.push_back(name);
		}
		parse_colon();
		auto ty = parse_type_expression(false);
		for (auto iter : names) {
			auto name = iter;
			auto slot = new StorageSlot(cxx_value_name(name), ty);
			scope->register_variable(name, slot, ty);
			if (emitter)
				emitter->emit_var_decl(slot->cxx_name, ty);
		}
		parse_semicolon();
	} while (true);
}
void Parser::maybe_parse_var_block() {
	if (peek_keyword("var")) {
		parse_var_block();
	}
}
bool Parser::maybe_parse_semicolon() {
	if (input_token == ";") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_plus() {
	if (input_token == "+") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_minus() {
	if (input_token == "-") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_star() {
	if (input_token == "*") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_slash() {
	if (input_token == "/") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_comma() {
	if (input_token == ",") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_colon() {
	if (input_token == ":") {
		consume();
		return true;
	} else {
		return false;
	}
}
bool Parser::maybe_parse_colon_equals() {
	if (input_token == ":=") {
		consume();
		return true;
	} else {
		return false;
	}
}
void Parser::parse_semicolon() {
	if (!maybe_parse_semicolon()) {
		raise_parse_error("missing semicolon");
	}
}
bool Parser::maybe_parse_opening_paren() {
	if (input_token == "(") {
		consume();
		return true;
	} else {
		return false;
	}
}
void Parser::parse_opening_paren() {
	if (!maybe_parse_opening_paren()) {
		raise_parse_error("missing opening paren");
	}
}
void Parser::parse_closing_paren() {
	if (input_token == ")") {
		consume();
	} else {
		raise_parse_error("missing closing paren");
	}
}
bool Parser::maybe_parse_opening_bracket() {
	if (input_token == "[") {
		consume();
		return true;
	} else {
		return false;
	}
}
void Parser::parse_opening_bracket() {
	if (!maybe_parse_opening_bracket()) {
		raise_parse_error("missing opening bracket");
	}
}
void Parser::parse_closing_bracket() {
	if (input_token == "]") {
		consume();
	} else {
		raise_parse_error("missing closing bracket");
	}
}

void Parser::parse_colon_equals() {
	if (input_token == ":=") {
		consume();
	} else {
		raise_parse_error("missing colon equals");
	}
}

void Parser::parse_colon() {
	if (input_token == ":") {
		consume();
	} else {
		raise_parse_error("missing colon");
	}
}
void Parser::parse_equals() {
	if (input_token == "=") {
		consume();
	} else {
		raise_parse_error("missing equals");
	}
}

size_t Parser::parse_decl_blocks() {
	size_t pushed = 0;
	bool is_class = false;
	while (true) {
		if (peek_keyword("type")) {
			if (is_class) {
				raise_parse_error("'class type' is not supported");
			}
			parse_type_block(false);
			// parse_type_block no longer pushes a sub-frame: it registers into
			// the enclosing decl scope. No push, no pop to account for.
		} else if (maybe_parse_keyword("class")) {
			if (is_class) {
				raise_parse_error("'class' prefix wasn't consumed");
			}
			is_class = true;
		} else if (peek_keyword("const")) {
			if (is_class) {
				raise_parse_error("'class const' is not supported");
			}
			parse_const_block();
		} else if (peek_keyword("var")) {
			if (is_class) {
				raise_parse_error("'class var' is not supported");
			}
			parse_var_block();
		} else if (peek_keyword("procedure")) {
			parse_procedure_or_function(is_class, false);
			is_class = false;
		} else if (peek_keyword("function")) {
			parse_procedure_or_function(is_class, true);
			is_class = false;
		} else if (peek_keyword("operator")) {
			if (is_class) {
				raise_parse_error("'class operator' is not supported");
			}
			parse_procedure_or_function(is_class, true);
			is_class = false;
		} else {
			break;
		}
	}
	if (is_class) {
		raise_type_parse_error("'class' prefix wasn't consumed");
	}
	return pushed;
}

void Parser::parse_block() {
	size_t pushed = parse_decl_blocks();
	parse_keyword("begin");
	parse_block_body();
	parse_keyword("end");
	for (size_t i = 0; i < pushed; i++)
		pop_scope();
}

void Parser::maybe_parse_proc_attributes() {
	// parse_semicolon();
	// TODO: inline
}

std::vector<Parameter> Parser::parse_proc_formal_parameters() {
	std::vector<Parameter> result;
	parse_opening_paren();
	if (input_token != ")") {
		do {
			ParamMode mode = ParamMode::Value;
			if (maybe_parse_keyword("var"))
				mode = ParamMode::Var;
			else if (maybe_parse_keyword("out"))
				mode = ParamMode::Out;
			else if (maybe_parse_keyword("const"))
				mode = ParamMode::Const;
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma())
				names.push_back(parse_identifier());
			Type* ty = maybe_parse_colon() ? parse_type_expression(false) : unknown_type();
			Node* default_value = nullptr;
			if (maybe_parse_equal()) {
				if (names.size() > 1) {
					raise_parse_error("default value not allowed with comma-grouped parameter names");
				}
				default_value = parse_expression();
			}
			for (auto& n : names) {
				result.push_back(Parameter{n, cxx_value_name(n), ty, mode, default_value});
			}
		} while (maybe_parse_semicolon());
	}
	parse_closing_paren();
	return result;
}

// is_class is whether there was a "class" prefix token
RoutineType* Parser::parse_routine_signature(bool is_class, bool is_function, bool allow_of_object, RoutineKind kind, Type* owner) {
 	std::vector<Parameter> formals;
	if (input_token == "(") {
		formals = parse_proc_formal_parameters();
	}
	Type* ret_ty = &unit_type();
	if (kind == CONSTRUCTOR) {
		if (auto ty = dynamic_cast<ClassType*>(owner)) {
			// That's so we can emit "(new X())->Create()".
			ret_ty = ty;
		} else {
			raise_type_parse_error("expected class as owner");
		}
	} else if (is_function) {
		parse_colon();
		ret_ty = parse_type_expression(false);
	}
	if (allow_of_object && peek_keyword("of")) {
		parse_keyword("of");
		parse_keyword("object");
		if (kind != METHOD) { // definitely not: CONSTRUCTOR, DESTRUCTOR, CLASS_METHOD
			raise_type_parse_error("expected method");
		}
		kind = METHOD;
	} else {
		if (auto ty = dynamic_cast<ClassType*>(owner)) {
		} else {
			if (kind != ROUTINE) {
				raise_type_parse_error("expected routine");
			}
			kind = ROUTINE;
		}
	}
	return new RoutineType(std::move(formals), ret_ty, kind);
}

Type* Parser::parse_procedure_type() {
	parse_keyword("procedure");
	return parse_routine_signature(false, false, true, METHOD);
}

Type* Parser::parse_function_type() {
	parse_keyword("function");
	return parse_routine_signature(false, true, true, METHOD);
}

Type* Parser::parse_operator_type() {
	parse_keyword("operator");
	// For now this is very similar to function.  Note: even parse_routine_signature uses parse_identifier() instead of parse_operator(), sigh.
	return parse_routine_signature(false, true, true, METHOD);
}

// Class Member Prototype Registration (Value Level)
void Parser::parse_method_prototype(Frame* body, Type* owner_class, bool is_function, bool is_destructor, bool is_constructor, bool is_class) {
	if (is_constructor) {
		parse_keyword("constructor");
	} else if (is_destructor) {
		parse_keyword("destructor");
	} else {
		parse_keyword(is_function ? "function" : "procedure");
	}
	std::string pas_name = parse_identifier();
	RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_destructor ? DESTRUCTOR : is_constructor ? CONSTRUCTOR : METHOD, owner_class);
	parse_semicolon();
	bool has_overload = false;
	Method::VirtualKind vk = Method::VirtualKind::None;
	while (true) {
		if (maybe_parse_keyword("overload")) {
			has_overload = true;
			parse_semicolon();
		} else if (maybe_parse_keyword("virtual")) {
			vk = Method::VirtualKind::Virtual;
			parse_semicolon();
		} else if (maybe_parse_keyword("override")) {
			vk = Method::VirtualKind::Override;
			parse_semicolon();
		} else if (maybe_parse_keyword("abstract")) {
			vk = Method::VirtualKind::Abstract;
			parse_semicolon();
		} else if (maybe_parse_keyword("dynamic")) {
			vk = Method::VirtualKind::Dynamic;
			parse_semicolon();
		} else
			break;
	}
	std::string cxx_name = cxx_value_name(pas_name);
	if (is_destructor) {
		if (ClassType* class_type = dynamic_cast<ClassType*>(owner_class)) {
			cxx_name = "~" + class_type->cxx_name;
		}
	}
	bool external = false;
	if (maybe_parse_keyword("external")) {
		parse_keyword("nil");
		parse_directive("name");
		cxx_name = parse_string_literal();
		parse_semicolon();
		external = true;
	}
	auto m = new Method(cxx_name, sig, has_overload, owner_class, vk);
	m->ty = sig; // The node's type IS the prototype.
	if (external) {
		m->has_body = true;
	}
	if (!body->register_callable(pas_name, m)) {
		raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
	}
}

// Helper to handle overload matching and short-form implementation resolution
Procedure* Parser::match_or_create_procedure(const std::string& pas_name, RoutineType* sig, bool had_paren, bool has_overload) {
	Frame* enclosing = const_cast<Frame*>(this->scopes.back().frame);
	Node* existing = enclosing->lookup_value(pas_name);

	auto sig_matches = [&](Callable* c) -> bool {
		auto rty = static_cast<RoutineType*>(c->ty);
		if (rty->formals.size() != sig->formals.size())
			return false;
		if (rty->return_type != sig->return_type)
			return false;
		for (size_t i = 0; i < sig->formals.size(); i++) {
			if (rty->formals[i].ty != sig->formals[i].ty)
				return false;
			if (rty->formals[i].mode != sig->formals[i].mode)
				return false;
		}
		return true;
	};

	auto attach_to = [&](Callable* c) -> Procedure* {
		if (c->has_body)
			raise_parse_error("duplicate implementation of '" + pas_name + "'");
		if (had_paren) {
			// Pascal allows impl parameter names to differ from interface names.
			// We update the prototype's names so local scope bindings match the body text.
			static_cast<RoutineType*>(c->ty)->formals = sig->formals;
		}
		auto p = dynamic_cast<Procedure*>(c);
		if (!p)
			raise_parse_error("'" + pas_name + "' is not a standalone procedure");
		return p;
	};

	Procedure* target = nullptr;
	if (existing) {
		if (auto ec = dynamic_cast<Callable*>(existing)) {
			if (!had_paren || sig_matches(ec))
				target = attach_to(ec);
		} else if (auto os = dynamic_cast<OverloadSet*>(existing)) {
			if (!had_paren) {
				Callable* pick = nullptr;
				for (auto* m : os->members) {
					if (!m->has_body) {
						if (pick)
							raise_parse_error("ambiguous short-form impl");
						pick = m;
					}
				}
				if (!pick)
					raise_parse_error("no unimplemented prototype");
				target = attach_to(pick);
			} else {
				for (auto* m : os->members) {
					if (sig_matches(m)) {
						if (target)
							raise_parse_error("ambiguous overload match");
						target = attach_to(m);
					}
				}
			}
		}
	}

	if (!target) {
		target = new Procedure(cxx_value_name(pas_name), sig, has_overload);
		target->ty = sig;
		if (!enclosing->register_callable(pas_name, target)) {
			raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
		}
	}
	return target;
}

void Parser::parse_routine_body(Callable* target, Frame* owner_frame) {
	Frame* enclosing = owner_frame ? owner_frame : const_cast<Frame*>(this->scopes.back().frame);
	Frame* body_frame = new Frame(enclosing);
	target->body_frame = body_frame;
	push_scope(body_frame);
	StorageSlot* self_slot = nullptr;
	if (auto m = dynamic_cast<Method*>(target)) {
		Type* self_ptr_ty = new PointerType(m->owner_class);
		self_slot = new StorageSlot("this", self_ptr_ty);
		body_frame->register_variable("self", self_slot, self_ptr_ty);
		push_with_scope(owner_frame, self_slot);
	}
	auto rty = static_cast<RoutineType*>(target->ty);
	for (auto& p : rty->formals) {
		body_frame->register_variable(p.pas_name, new StorageSlot(p.cxx_name, p.ty), p.ty);
	}
	if (emitter)
		emitter->emit_procedure_open(target);
	size_t pushed = parse_decl_blocks();
	parse_keyword("begin");
	parse_block_body();
	target->has_body = true;
	parse_keyword("end");
	parse_semicolon();
	if (emitter) {
		bool constructor = false;
		if (auto m = dynamic_cast<Method*>(target)) {
			if (auto ty = dynamic_cast<RoutineType*>(m)) {
				if (ty->kind == CONSTRUCTOR) {
					constructor = true;
				}
			}
		}
		emitter->emit_procedure_close(constructor);
	}
	for (size_t i = 0; i < pushed; i++)
		pop_scope();
	if (self_slot)
		pop_scope(); // pop the with_scope
	pop_scope();	     // pop body_frame
}

Type* Parser::lookup_external_type(const char* lib, std::string cxx_name) {
	if (lib == nullptr) {
		auto intrinsic = lookup_builtin_type(cxx_name);
		if (intrinsic == nullptr) {
			return raise_type_parse_error("unknown external type " + cxx_name);
		} else {
			return intrinsic;
		}
	} else {
		return raise_type_parse_error("unknown external library '" + std::string(lib) + "'");
	}
}

Builtin* Parser::lookup_external_value(const char* lib, std::string cxx_name) {
	if (lib == nullptr) {
		auto builtin = create_builtin_value(cxx_name);
		if (builtin == nullptr) {
			raise_parse_error("builtin '" + cxx_name + "' not found");
			return nullptr;
		} else {
			return builtin;
		}
	} else {
		raise_parse_error("external library not implemented");
		return nullptr;
	}
}

void Parser::parse_procedure_or_function(bool is_class, bool is_function) {
	bool has_overload = false;
	std::string first_name;
	bool is_destructor = false;
	bool is_constructor = false;
	if (peek_keyword("operator")) {
		if (!is_function) {
			raise_parse_error("custom operator should have a return value");
		}
		parse_keyword("operator");
		// Assumption: there are no method operators.
		first_name = input_token; // TODO: well, parse_operator();
		consume();
		has_overload = true; // I think those should be implicitly "overload;"
	} else if (maybe_parse_keyword("destructor")) {
		is_destructor = true;
		first_name = parse_identifier();
	} else if (maybe_parse_keyword("constructor")) {
		is_constructor = true;
		first_name = parse_identifier();
	} else {
		parse_keyword(is_function ? "function" : "procedure");
		first_name = parse_identifier();
	}

	// `procedure TFoo.Bar;`
	if (maybe_parse_period()) {
		std::string method_name = parse_identifier();
		Type* owner_ty = resolve_type(first_name, false);
		Frame* owner_frame = get_type_body_frame(owner_ty);
		if (!owner_frame)
			raise_parse_error("'" + first_name + "' is not a class/record/object");
		if (is_destructor) {
			if (auto class_type = dynamic_cast<ClassType*>(owner_ty)) {
				method_name = "~" + class_type->cxx_name;
			}
		}
		Node* hit = owner_frame->lookup_value(method_name);
		auto m = dynamic_cast<Method*>(hit);
		if (!m)
			raise_parse_error("no method '" + method_name + "' on '" + first_name + "'");
		RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_constructor ? CONSTRUCTOR : is_destructor ? DESTRUCTOR : METHOD, owner_ty);
		parse_semicolon();
		if (m->has_body)
			raise_parse_error("duplicate implementation of '" + method_name + "'");
		// If provided, update formal names for local body scope; FIXME: check count, types etc
		if (sig->formals.size() > 0) {
			static_cast<RoutineType*>(m->ty)->formals = sig->formals;
		}
		parse_routine_body(m, owner_frame);
		return;
	} else { // Standalone Routine
		bool had_paren = (input_token == "(");
		if (is_class) {
		    raise_parse_error("expected class method, not class routine");
		}
		RoutineType* sig = parse_routine_signature(is_class, is_function, false, ROUTINE);
		parse_semicolon();
		while (maybe_parse_keyword("overload")) {
			has_overload = true;
			parse_semicolon();
		}
		bool body_follows = peek_keyword("begin") || peek_keyword("var") ||
				    peek_keyword("const") || peek_keyword("type");
		if (maybe_parse_keyword("forward")) {
			parse_semicolon();
			body_follows = false;
		}
		Procedure* target = match_or_create_procedure(first_name, sig, had_paren, has_overload);
		if (maybe_parse_directive("external")) {
			parse_keyword("nil");
			parse_directive("name");
			std::string cxx_name = parse_string_literal();
			parse_semicolon();

			auto builtin = lookup_external_value(nullptr, cxx_name);
			if (builtin != nullptr) {
				// This is basically making TARGET an ALIAS for BUILTIN.
				target->has_body = true;
				if (auto qbuiltin = dynamic_cast<Builtin*>(builtin)) { // used
					// These Builtins are all polymorphic and C++ overloads will just have to adjust to us.
					auto desc = qbuiltin->desc;
					target->cxx_name = desc->cxx_name;
				} else {
					raise_parse_error("unknown intrinsic via external '" + cxx_name + "'");
				}
			}
		} else if (body_follows) {
			parse_routine_body(target, nullptr);
		}
	}
}

// Per-argument conversion costs for a candidate. Returns empty vector when
// the candidate isn't viable. Costs sized to formals.size() (+1 for Self
// when candidate is a Method with receiver); entries past args.size() are 0
// (default-supplied positions).
static std::vector<int> per_arg_costs(Callable* c, Node* receiver, const std::vector<Node*>& args) {
	auto rty = static_cast<RoutineType*>(c->ty);

	if (args.size() > rty->formals.size())
		return {};
	for (size_t i = args.size(); i < rty->formals.size(); i++) {
		if (!rty->formals[i].default_value)
			return {};
	}
	// Self position (if any) is prepended to the cost vector.
	auto m = dynamic_cast<Method*>(c);
	size_t self_slots = (m && receiver) ? 1 : 0;
	std::vector<int> costs(self_slots + rty->formals.size(), 0);
	if (self_slots) {
		int sc = conversion_cost(receiver ? receiver->ty : nullptr, m->owner_class);
		if (sc < 0)
			return {};
		costs[0] = sc;
	}
	for (size_t i = 0; i < args.size(); i++) {
		Type* from = args[i] ? args[i]->ty : nullptr;
		int cc = conversion_cost(from, rty->formals[i].ty);
		if (cc < 0)
			return {};
		costs[self_slots + i] = cc;
	}
	return costs;
}

// A dominates B iff A's cost is <= B's on every position AND strictly < on
// at least one. Different-length vectors don't compare (ambiguity later).
static bool dominates(const std::vector<int>& a, const std::vector<int>& b) {
	if (a.size() != b.size())
		return false;
	bool strict = false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i] > b[i])
			return false;
		if (a[i] < b[i])
			strict = true;
	}
	return strict;
}

Node* Parser::cast(Node* a, Type* target_ty) {
	if (a->ty == target_ty) {
		return a;
	} else if (target_ty == unknown_type()) { // this target is void* but the formal parameter is more like a reference
		return new Cast(new AddrOf(a), target_ty);
	} else {
		return new Cast(a, target_ty);
	}
}

Parser::FinalizedCall Parser::finalize_call(Node* target, std::vector<Node*>& args, std::string name_for_error) {
	// Peel MemberAccess: if the member is callable, its container is the
	// receiver and the member is the effective callee.
	Node* receiver = nullptr;
	if (auto ma = dynamic_cast<MemberAccess*>(target)) {
		if (dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b)) {
			receiver = ma->a;
			target = ma->b;
		}
	}
	Callable* chosen = nullptr;
	if (auto c = dynamic_cast<Callable*>(target)) {
		chosen = c;
	} else if (auto os = dynamic_cast<OverloadSet*>(target)) {
		std::vector<std::pair<Callable*, std::vector<int>>> viable;
		for (auto* c : os->members) {
			auto costs = per_arg_costs(c, receiver, args);
			if (!costs.empty())
				viable.push_back({c, std::move(costs)});
		}
		if (viable.empty()) {
			raise_parse_error("no matching overload for '" + name_for_error + "'");
		}
		std::vector<Callable*> non_dominated;
		for (size_t i = 0; i < viable.size(); i++) {
			bool dom = false;
			for (size_t j = 0; j < viable.size(); j++) {
				if (i != j && dominates(viable[j].second, viable[i].second)) {
					dom = true;
					break;
				}
			}
			if (!dom)
				non_dominated.push_back(viable[i].first);
		}
		if (non_dominated.size() != 1) {
			raise_parse_error("ambiguous overload for '" + name_for_error + "'");
		}
		chosen = non_dominated[0];
	} else {
		// Builtin or other opaque callable -- no ranking / defaults / coercion.
		return FinalizedCall{receiver, target};
	}
	// Materialize missing args from defaults.
	auto rty = static_cast<RoutineType*>(chosen->ty);
	while (args.size() < rty->formals.size()) {
		auto& p = rty->formals[args.size()];
		if (!p.default_value) {
			raise_parse_error("missing argument for parameter '" + p.pas_name + "' in call to '" + name_for_error + "'");
		}
		args.push_back(p.default_value);
	}
	if (args.size() > rty->formals.size()) {
		raise_parse_error("too many arguments to '" + name_for_error + "'");
	}
	// Insert Cast for any arg whose type differs from the formal.
	for (size_t i = 0; i < args.size(); i++) {
		Type* t = rty->formals[i].ty;
		args[i] = cast(args[i], t);
	}
	return FinalizedCall{receiver, chosen};
}

Unit* Parser::load_or_get_unit(std::string name) {
	if (Unit* existing = unit_registry->lookup(name))
		return existing;
	std::vector<std::string> empty;
	const auto& paths = options ? options->unit_search_paths : empty;
	FILE* f = nullptr;
	std::string opened;
	for (auto ext : {".pp", ".pas"}) {
		std::tie(f, opened) = search_for_file(name + ext, input_file_name, paths);
		if (f)
			break;
	}
	if (!f)
		raise_parse_error("cannot find unit file for: " + name);
	// Nested Parser so the sub-load has its own token/scope state; the shared
	// unit_registry is what lets circular-dep detection work across the two.
	Parser sub(unit_registry, nullptr, options);
	sub.push_input_file(f, opened, 1);
	sub.start();
	sub.parse_program_or_unit();
	Unit* loaded = unit_registry->lookup(name);
	if (!loaded)
		raise_parse_error("file '" + opened + "' did not declare 'unit " + name + ";'");
	return loaded;
}

size_t Parser::parse_uses_clause(bool in_interface, std::string current_name) {
	size_t pushed = 0;
	do {
		std::string name = parse_identifier();
		Unit* used = load_or_get_unit(name);
		if (in_interface && used->phase == UnitPhase::InterfaceInProgress) {
			raise_parse_error("circular interface dependency between '" + current_name + "' and '" + name + "'");
		}
		push_scope(used->interface_frame);
		pushed++;
		if (!maybe_parse_comma())
			break;
	} while (true);
	return pushed;
}

size_t Parser::implicit_uses(std::string user_name) {
	if (strcasecmp(user_name.c_str(), "system") == 0)
		return 0;
	Unit* sys = load_or_get_unit("system");
	push_scope(sys->interface_frame);
	return 1;
}

void Parser::parse_unit_body() {
	std::string name = parse_identifier();
	parse_semicolon();
	Frame* iface = new Frame(nullptr);
	// Implementation frame's structural parent is the interface frame, so
	// impl can transparently see interface decls via the parent chain.
	Frame* impl = new Frame(iface);
	Unit* unit = unit_registry->register_new(name, iface, impl);
	unit->phase = UnitPhase::InterfaceInProgress;

	parse_keyword("interface");
	push_scope(iface);
	// Implicit `system` first, so built-ins (Boolean, True, False, ...) resolve
	// in every unit's interface section. Stays on the scope stack through the
	// implementation section too (interface-side uses pop only at unit end).
	size_t iface_uses = implicit_uses(name);
	if (maybe_parse_keyword("uses")) {
		iface_uses += parse_uses_clause(true, name);
		parse_semicolon();
	}
	// Interface section: parse_decl_blocks handles type/const/var/proc/func.
	// Procedures parsed here fall out as prototypes because
	// parse_procedure_or_function's body_follows check sees no body-starter
	// after their `;` and returns without expecting a body.
	size_t iface_decls = parse_decl_blocks();
	unit->phase = UnitPhase::InterfaceDone;

	parse_keyword("implementation");
	push_scope(impl);
	unit->phase = UnitPhase::ImplementationInProgress;
	size_t impl_uses = 0;
	if (maybe_parse_keyword("uses")) {
		impl_uses = parse_uses_clause(false, name);
		parse_semicolon();
	}
	// Implementation section: same dispatcher; procedures with bodies attach
	// to the interface prototypes via the lookup-then-adopt path in
	// parse_procedure_or_function.
	size_t impl_decls = parse_decl_blocks();

	parse_keyword("end");
	parse_period();

	// Pop in reverse push order: impl-side decl blocks, impl-side uses,
	// impl frame, iface-side decl blocks, iface-side uses, iface frame.
	for (size_t i = 0; i < impl_decls; i++)
		pop_scope();
	for (size_t i = 0; i < impl_uses; i++)
		pop_scope();
	pop_scope(); // impl
	for (size_t i = 0; i < iface_decls; i++)
		pop_scope();
	for (size_t i = 0; i < iface_uses; i++)
		pop_scope();
	pop_scope(); // iface

	unit->phase = UnitPhase::Done;
}

void Parser::parse_program_or_unit() {
	if (maybe_parse_keyword("program")) {
		auto name = parse_identifier();
		parse_semicolon();
		Frame* impl = new Frame(nullptr);
		Unit* unit = unit_registry->register_new(name, nullptr, impl);
		unit->phase = UnitPhase::InterfaceInProgress;
		push_scope(impl);
		// Program-body `uses`: same shape as a unit's implementation-side
		// `uses` (no interface phase to worry about). Each named unit's
		// interface_frame gets pushed so its exports are visible to the
		// program body. Implicit `system` goes first so built-ins resolve
		// even with no `uses` clause.
		size_t prog_uses = implicit_uses(name);
		if (maybe_parse_keyword("uses")) {
			prog_uses += parse_uses_clause(false, name);
			parse_semicolon();
		}
		if (emitter)
			emitter->emit_program_prologue(name);
		// Inlined equivalent of parse_block; we need to bracket the body-block
		// with main() emission hooks, which parse_block itself doesn't know
		// about (it's also called from procedure bodies).
		size_t pushed = parse_decl_blocks();
		parse_keyword("begin");
		if (emitter)
			emitter->emit_main_prologue();
		parse_block_body();
		parse_keyword("end");
		if (emitter)
			emitter->emit_main_epilogue();
		for (size_t i = 0; i < pushed; i++)
			pop_scope();
		for (size_t i = 0; i < prog_uses; i++)
			pop_scope();
		parse_period();
		pop_scope();
		unit->phase = UnitPhase::Done;
	} else if (maybe_parse_keyword("unit")) {
		parse_unit_body();
	} else {
		raise_parse_error("unknown input token");
	}
}
