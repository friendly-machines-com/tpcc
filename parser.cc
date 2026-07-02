#include <cassert>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <charconv>
#include <format>
#include <functional>
#include <optional>
#include <sstream>
#include <set>
#include <unordered_set>
#include "parser.h"
#include "cst.h"
#include "frame.h"
#include "evaluator.h"
#include "builtins.h"
#include "units.h"
#include "emit.h"

static std::unordered_set<std::string> keywords = {
	"program",
	"unit",
	"interface",
	"implementation",
	"record",
	"object",
	"class",
	"inherited",
	"interface",
	"procedure",
	"function",
	"constructor",
	"destructor",
	"if",
	"then",
	"else",
	"while",
	"do",
	"repeat",
	"until",
	"begin",
	"end",
	"var",
	"type",
	"const",
	"with",
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
	// If we're mid-parse, the tokenizer has one character already read from
	// the current source sitting in input_char. Push it back onto that
	// FILE*'s stream so the parent resumes on exactly the right byte after
	// this new source is popped.
	if (!input_files.empty() && input_char != EOF) {
		ungetc(input_char, this->input_file);
	}
	input_files.push_back(ParserInputFile {
		.input_file = input_file,
		.input_file_name = input_file_name,
		.input_file_line_number = input_file_line_number
	});
	this->input_file = input_file;
	this->input_file_name = input_file_name;
	this->input_file_line_number = input_file_line_number;
	input_char = fgetc(input_file);
}

void Parser::push_scope(const Frame* scope) {
	this->scopes.push_back(ScopeEntry{scope, nullptr});
}

void Parser::push_with_scope(const Frame* scope, Node* unwrap_via) {
	this->scopes.push_back(ScopeEntry{scope, unwrap_via});
}

void Parser::pop_scope() {
	if (this->scopes.empty()) {
		fprintf(stderr, "internal compiler error: pop_scope on empty scope stack\n");
		abort();
	}
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

[[noreturn]] Node* Parser::raise_parse_error(std::string message) {
	emit_parse_error(input_file_name, input_file_line_number, message);
}

[[noreturn]] Type* Parser::raise_type_parse_error(std::string message) {
	emit_parse_error(input_file_name, input_file_line_number, message);
}

bool Parser::is_defined(const std::string& sym) const {
	return options && options->defines.count(sym) > 0;
}

// Evaluate a `{$if ...}` condition. Grammar: `defined(X)` or bare `X` (short
// for `defined(X)`), combined with `not`, `and`, `or`, and parentheses.
// Unrecognised syntax raises a parse error.
bool Parser::eval_directive_expr(const std::string& expr) {
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
	std::function<bool()> parse_or;
	std::function<bool()> parse_atom = [&]() -> bool {
		skip_ws();
		if (p < expr.size() && expr[p] == '(') {
			p++;
			bool v = parse_or();
			skip_ws();
			if (p >= expr.size() || expr[p] != ')') {
				raise_parse_error("missing ')' in {$if ...} expression");
			}
			p++;
			return v;
		}
		std::string w = peek_word();
		std::string lw = lower(w);
		if (lw == "not") { p += w.size(); return !parse_atom(); }
		if (lw == "defined") {
			p += w.size();
			skip_ws();
			if (p >= expr.size() || expr[p] != '(') {
				raise_parse_error("expected '(' after 'defined' in {$if ...}");
			}
			p++;
			std::string sym = peek_word();
			if (sym.empty()) raise_parse_error("expected identifier in defined(...)");
			p += sym.size();
			skip_ws();
			if (p >= expr.size() || expr[p] != ')') {
				raise_parse_error("missing ')' in defined(...)");
			}
			p++;
			return is_defined(sym);
		}
		if (!w.empty()) {
			p += w.size();
			return is_defined(w);
		}
		raise_parse_error("unrecognised token in {$if ...}: '" + expr.substr(p) + "'");
	};
	auto parse_and = [&]() -> bool {
		bool v = parse_atom();
		while (true) {
			skip_ws();
			if (lower(peek_word()) != "and") break;
			p += peek_word().size();
			bool r = parse_atom();
			v = v && r;
		}
		return v;
	};
	parse_or = [&]() -> bool {
		bool v = parse_and();
		while (true) {
			skip_ws();
			if (lower(peek_word()) != "or") break;
			p += peek_word().size();
			bool r = parse_and();
			v = v || r;
		}
		return v;
	};
	bool result = parse_or();
	skip_ws();
	if (p != expr.size()) {
		raise_parse_error("unrecognised trailing form in {$if ...}: '"
		                  + expr.substr(p) + "'");
	}
	return result;
}

// Split BODY into (directive_name_lowercased, argument-after-name-trimmed).
static std::pair<std::string, std::string> split_directive(const std::string& body) {
	size_t p = 0;
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) p++;
	std::string name;
	while (p < body.size() && (isalnum((unsigned char)body[p]) || body[p] == '_')) {
		name.push_back((char)tolower((unsigned char)body[p]));
		p++;
	}
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) p++;
	std::string rest = body.substr(p);
	while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t'
	                         || rest.back() == '\r' || rest.back() == '\n')) rest.pop_back();
	return {name, rest};
}

// Search for an include file: current input file's directory, then each
// entry in include search paths. The filename is used as given (no
// extension guessing).
static std::pair<FILE*, std::string> resolve_include(
    const std::string& name,
    const std::string& current_input,
    const std::vector<std::string>& search_paths)
{
	std::vector<std::string> dirs;
	std::string cur_dir;
	auto slash = current_input.find_last_of('/');
	if (slash != std::string::npos) cur_dir = current_input.substr(0, slash + 1);
	dirs.push_back(cur_dir);
	for (auto& d : search_paths) {
		std::string s = d;
		if (!s.empty() && s.back() != '/') s.push_back('/');
		dirs.push_back(s);
	}
	for (auto& d : dirs) {
		std::string candidate = d + name;
		if (FILE* f = fopen(candidate.c_str(), "r")) return {f, candidate};
	}
	return {nullptr, ""};
}

std::string Parser::expand_include_macro(const std::string& rest) {
	if (rest != "%DATE%") raise_parse_error("unsupported include macro: " + rest);
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
		if (name == "ifndef") cond = !cond;
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
		if (ifdef_stack.empty()) raise_parse_error("$else without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken;
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "elseif") {
		if (ifdef_stack.empty()) raise_parse_error("$elseif without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken && eval_directive_expr(rest);
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "endif" || name == "ifend") {
		if (ifdef_stack.empty()) raise_parse_error("$endif without matching $ifdef");
		ifdef_stack.pop_back();
		return;
	}
	if (!current_active()) return;
	if (name == "define") {
		if (options) options->defines[rest] = "";
		return;
	}
	if (name == "undef") {
		if (options) options->defines.erase(rest);
		return;
	}
	if (name == "i" || name == "include") {
		// {$I %MACRO%} is a distinct form: NAME is looked up as a compiler
		// macro (%DATE%, %TIME%, %LINE%, %FILE%, ...) or environment variable
		// and expanded as an inline Pascal string literal, not a file lookup.
		// Not yet implemented; hard-error separately so a real missing-file
		// diagnostic doesn't get muddled with an unsupported macro form.
		if (rest.size() >= 2 && rest.front() == '%' && rest.back() == '%') {
			raise_parse_error("{$I " + rest + "} macro form not implemented");
		}
		std::vector<std::string> empty;
		auto [f, path] = resolve_include(rest, input_file_name,
		                                 options ? options->include_search_paths : empty);
		if (!f) raise_parse_error("cannot open include file: " + rest);
		push_input_file(f, path, 1);
		return;
	}
	// Other directives are accepted here for now. As we hit code where the
	// current no-op is wrong, tighten by name.
}

std::string Parser::consume() {
	auto result = input_token;
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
		while ((input_char >= 'a' && input_char <= 'z') || (input_char >= 'A' && input_char <= 'Z') || input_char == '_') {
			sst << (char) tolower(input_char);
			consume_lowlevel();
		}
	} else if (input_char == '$') {
		sst << (char) input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || (input_char >= 'a' && input_char <= 'f') || (input_char >= 'A' && input_char <= 'F') || input_char == '.' || input_char == '_') {
			sst << (char) tolower(input_char);
			consume_lowlevel();
		}
	} else if ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
		while ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
			sst << (char) input_char;
			consume_lowlevel();
		}
	} else if (input_char == '#') {
		sst << (char) input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || input_char == '.' || input_char == '_') {
			sst << (char) input_char;
			consume_lowlevel();
		}
	} else if (input_char == '<') {
		sst << (char) input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '<' || input_char == '>') {
			sst << (char) input_char;
			consume_lowlevel();
		}
	} else if (input_char == '>') {
		sst << (char) input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '>' || input_char == '<') {
			sst << (char) input_char;
			consume_lowlevel();
		}
	} else if (input_char == ':') {
		sst << (char) input_char;
		consume_lowlevel();
		if (input_char == '=') {
			sst << (char) input_char;
			consume_lowlevel();
		}
	} else if (input_char != EOF && strchr("=;,[]()@*+-/^", input_char)) {
		sst << (char) input_char;
		consume_lowlevel();
	} else if (input_char == '\'') {
		sst << (char) input_char;
		consume_lowlevel();
		while (input_char != EOF && input_char != '\'') {
			sst << (char) input_char;
			consume_lowlevel();
		}
		if (input_char == '\'') {
			sst << (char) input_char;
			consume_lowlevel();
		} else {
			raise_parse_error("missing end quote");
		}
	} else if (input_char == '{') {
		sst << (char) input_char;
		consume_lowlevel();
		if (input_char == '$') { // {$I ...
			sst << (char) input_char;
			consume_lowlevel();
			while (input_char != EOF && input_char != '}') {
				sst << (char) input_char;
				consume_lowlevel();
			}
			if (input_char == '}') {
				sst << (char) input_char;
				consume_lowlevel();
				// FIXME: push_input_file
				return consume();
			} else {
				raise_parse_error("missing end comment");
			}
		} else {
			while (input_char != EOF && input_char != '}') {
				sst << (char) input_char;
				consume_lowlevel();
			}
			if (input_char == '}') {
				sst << (char) input_char;
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
		raise_parse_error("missing keyword: " + s);
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
/** Return the Frame that holds the fields/members of TY, or nullptr if TY
 *  doesn't have one (i.e. isn't a record/class/object). Transparently walks
 *  through an IncompleteType via `resolved`. Used by `with` to find the
 *  field namespace to push. */
static Frame* get_type_body_frame(Type* ty) {
	while (auto inc = dynamic_cast<IncompleteType*>(ty)) {
		if (!inc->resolved) return nullptr;
		ty = inc->resolved;
	}
	if (auto r = dynamic_cast<RecordType*>(ty)) return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty)) return c->children;
	if (auto o = dynamic_cast<ObjectType*>(ty)) return o->children;
	return nullptr;
}

Node* Parser::maybe_parse_statement() {
	if (peek_keyword("end")) {
		return nullptr;
	} else {
		if (peek_keyword("return")) { // FIXME Exit
			consume();
			return new Return(parse_expression());
		} else if (peek_keyword("if")) {
			parse_keyword("if");
			auto condition = parse_expression();
			parse_keyword("then");
			parse_statement();
			parse_keyword("else");
			parse_statement();
		} else if (peek_keyword("while")) {
			parse_keyword("while");
			auto condition = parse_expression();
			parse_keyword("do");
			auto body = parse_statement();
			// FIXME
		} else if (peek_keyword("repeat")) {
			parse_keyword("repeat");
			auto body = parse_block_body();
			parse_keyword("until");
			auto condition = parse_expression();
		} else if (peek_keyword("begin")) {
			parse_keyword("begin");
			auto body = parse_block_body();
			parse_keyword("end");
			return body;
		} else if (peek_keyword("with")) {
			parse_keyword("with");
			// TODO: complex targets (`p^`, `arr[i]`, `f()`). For now the
			// target must be a simple variable so we can read Type* off its
			// StorageSlot; the alias-emission below is already correct for
			// arbitrary targets when we lift this restriction.
			auto id = parse_identifier();
			Node* target = resolve_value(id);
			auto target_slot = dynamic_cast<StorageSlot*>(target);
			if (!target_slot) raise_parse_error("with target must currently be a simple variable");
			Frame* body_frame = get_type_body_frame(target_slot->ty);
			if (!body_frame) raise_parse_error("with target's type has no field body");
			parse_keyword("do");
			std::string alias = emitter ? emitter->next_fresh_cxx_name("pas_with") : std::string("pas_with_x");
			auto alias_slot = new StorageSlot(alias, target_slot->ty);
			if (emitter) emitter->emit_with_prologue(alias, target);
			push_with_scope(body_frame, alias_slot);
			parse_statement();
			pop_scope();
			if (emitter) emitter->emit_with_epilogue();
			return nullptr;
		} else {
			// A statement here is either an assignment (designator := expression)
			// or a call (designator, possibly with auto-call). Parse the LHS as
			// a raw designator so we don't auto-call in the assignment case.
			Node* lhs = parse_designator();
			if (input_token == ":=") {
				parse_colon_equals();
				if (!is_assignable(lhs)) {
					raise_parse_error("LHS of ':=' is not assignable");
				}
				Node* rhs = parse_expression();
				auto assign = new Assign(lhs, rhs);
				if (emitter) emitter->emit_statement(assign);
				return assign;
			}
			// Call statement: parse_designator already built the ProcCall for
			// explicit `foo(x)`; for bare `foo`, apply auto-call now.
			Node* call = maybe_auto_call(lhs);
			if (!dynamic_cast<ProcCall*>(call)) {
				raise_parse_error("statement is neither an assignment nor a call");
			}
			if (emitter) emitter->emit_statement(call);
			return call;
		}
		// FIXME: raise_parse_error("missing statement");
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
		(void) raise_parse_error("expected identifier");
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
				return raise_parse_error("malformed numeral: " + input_token);
			}
			// FIXME: continue for non-integer here.
			auto lit = new Constant(value, &untyped_integer_type());
			consume();
			return lit;
		} else {
			// FIXME: continue for non-integer here.
			return raise_parse_error("unimplemented real numeral: " + input_token);
		}
	} else {
		return nullptr;
	}
}

Node* Parser::parse_numeral() {
	auto result = maybe_parse_numeral();
	if (!result) {
		return raise_parse_error("expected numeral");
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
		if (!hit) continue;
		auto as_call = dynamic_cast<Callable*>(hit);
		auto as_set  = dynamic_cast<OverloadSet*>(hit);
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
				if (collected.empty()) return as_call;   // plain callable, first-hit wins
				break;                                   // shadowed by collected overloads above
			}
			collected.push_back(as_call);
		} else if (as_set) {
			// Every member of an OverloadSet already has has_overload_directive.
			for (auto* m : as_set->members) collected.push_back(m);
		} else {
			// Non-callable value below a collected overload block -- stop.
			break;
		}
	}
	if (collected.empty()) {
		raise_parse_error("unresolved value identifier: " + name);
		return nullptr;
	}
	if (collected.size() == 1) return collected[0];
	return new OverloadSet(name, std::move(collected));
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
	} else {
		auto result = maybe_parse_numeral();
		if (result) {
			return result;
		} else {
			// FIXME: bool literals also belong here
			auto id = parse_identifier();
			return resolve_value(id);
		}
	}
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

// Helpers that construct a Node and set its result type in one expression.
template<typename T> static Node* mk_arith(Node* a, Node* b) {
	auto n = new T(a, b);
	n->ty = common_arith_type(a->ty, b->ty);
	return n;
}
template<typename T> static Node* mk_compare(Node* a, Node* b) {
	auto n = new T(a, b);
	n->ty = boolean_type();
	return n;
}
template<typename T> static Node* mk_unary_same(Node* x) {
	auto n = new T(x);
	n->ty = x->ty;
	return n;
}

// Small helper: is NODE a bare callable reference (Callable, OverloadSet, or
// a MemberAccess whose member is either)? Used both for the auto-call check
// and to decide whether to peel a MemberAccess in finalize_call.
static bool node_is_bare_callable(Node* n) {
	if (!n) return false;
	if (dynamic_cast<Callable*>(n) || dynamic_cast<OverloadSet*>(n)) return true;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b);
	}
	return false;
}

Node* Parser::maybe_auto_call(Node* n) {
	if (!node_is_bare_callable(n)) return n;
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
		if (!inc->resolved) return ty;
		ty = inc->resolved;
	}
	return ty;
}
static Frame* body_frame_of(Type* ty) {
	if (auto r = dynamic_cast<RecordType*>(ty)) return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty)) return c->children;
	if (auto o = dynamic_cast<ObjectType*>(ty)) return o->children;
	return nullptr;
}

Node* Parser::parse_designator() {
	Node* result = parse_value();
	while (true) {
		if (input_token == ".") {
			// Binary infix: RHS is a single identifier token. Before applying,
			// if LHS is a bare callable it must be auto-called (else the `.`
			// would try to look up a member of a callable, which is nonsense).
			result = maybe_auto_call(result);
			consume();
			std::string member_name = parse_identifier();
			Type* ct = unwrap_incomplete(result->ty);
			Frame* members = body_frame_of(ct);
			if (!members) raise_parse_error("member access on non-composite type");
			Node* member = members->lookup_value(member_name);
			if (!member) raise_parse_error("no member '" + member_name + "'");
			auto ma = new MemberAccess(result, member);
			ma->ty = member->ty;
			result = ma;
		} else if (input_token == "(") {
			// Bracketed n-ary: RHS is a comma-separated list of expressions.
			// No auto-call before `(` -- this `(` IS the call.
			consume();
			std::vector<Node*> args;
			if (input_token != ")") {
				args.push_back(parse_expression());
				while (maybe_parse_comma()) args.push_back(parse_expression());
			}
			parse_closing_paren();
			auto fc = finalize_call(result, args, /*name for error*/ "");
			auto call = new ProcCall(fc.receiver, fc.callee, std::move(args));
			call->ty = fc.callee ? fc.callee->ty : nullptr;
			result = call;
		} else if (input_token == "[") {
			// Bracketed: RHS is a single expression. Auto-call bare callable
			// LHS first (indexing into a callable reference is nonsense).
			result = maybe_auto_call(result);
			consume();
			Node* idx = parse_expression();
			parse_closing_bracket();
			Type* ct = unwrap_incomplete(result->ty);
			auto arr = dynamic_cast<FixedArrayType*>(ct);
			if (!arr) raise_parse_error("index on non-array type");
			auto ix = new Index(result, idx);
			ix->ty = arr->item_type;
			result = ix;
		} else if (input_token == "^") {
			// Postfix: no RHS. Auto-call bare callable LHS first (deref of a
			// callable reference is nonsense).
			result = maybe_auto_call(result);
			consume();
			Type* ct = unwrap_incomplete(result->ty);
			auto p = dynamic_cast<PointerType*>(ct);
			if (!p) raise_parse_error("deref of non-pointer type");
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
	if (!n) return false;
	if (dynamic_cast<StorageSlot*>(n)) return true;
	if (dynamic_cast<Dereference*>(n)) return true;
	if (dynamic_cast<Index*>(n)) return true;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<StorageSlot*>(ma->b) != nullptr;
	}
	return false;
}

Node* Parser::parse_power() {
	// FIXME **
	if (maybe_parse_keyword("not")) {
		return mk_unary_same<Not>(maybe_auto_call(parse_designator()));
	} else if (maybe_parse_at()) {
		auto x = parse_designator();   // @ takes a designator, not the auto-called value
		auto n = new AddrOf(x);
		n->ty = x->ty ? static_cast<Type*>(new PointerType(x->ty)) : nullptr;
		return n;
	} else if (maybe_parse_minus()) {
		return mk_unary_same<Negate>(maybe_auto_call(parse_designator()));
	} else if (maybe_parse_plus()) {
		return mk_unary_same<Positivize>(maybe_auto_call(parse_designator()));
	} else {
		return maybe_auto_call(parse_designator());
	}
}

Node* Parser::parse_product() {
	auto result = parse_power();
	while (true) {
		if (maybe_parse_star()) {
			result = mk_arith<Multiply>(result, parse_power());
		} else if (maybe_parse_slash()) {
			// TODO: Pascal `/` returns Real regardless of operand types; needs
			// a Real intrinsic before we can set ty correctly. Using
			// common_arith_type as a placeholder.
			result = mk_arith<Divide>(result, parse_power());
		} else if (maybe_parse_keyword("div")) {
			result = mk_arith<Div>(result, parse_power());
		} else if (maybe_parse_keyword("mod")) {
			result = mk_arith<Mod>(result, parse_power());
		} else if (maybe_parse_keyword("and")) {
			result = mk_arith<And>(result, parse_power());
		} else if (maybe_parse_keyword("shl")) {
			result = mk_arith<ShiftLeft>(result, parse_power());
		} else if (maybe_parse_keyword("shr")) {
			result = mk_arith<ShiftRight>(result, parse_power());
		} else if (maybe_parse_keyword("as")) {
			// `x as T`: b is the parsed type-position expression whose ty is
			// the target. Result type is that target.
			auto rhs = parse_power();
			auto n = new Coerce(result, rhs);
			n->ty = rhs->ty;
			result = n;
		} else if (maybe_parse_less_less()) {
			result = mk_arith<ShiftLeft>(result, parse_power());
		} else if (maybe_parse_greater_greater()) {
			result = mk_arith<ShiftRight>(result, parse_power());
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
			result = mk_arith<Add>(result, parse_product());
		} else if (maybe_parse_minus()) {
			result = mk_arith<Subtract>(result, parse_product());
		} else if (maybe_parse_keyword("or")) {
			result = mk_arith<Or>(result, parse_product());
		} else if (maybe_parse_keyword("xor")) {
			result = mk_arith<Xor>(result, parse_product());
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
			result = mk_compare<Equal>(result, parse_sum());
		} else if (maybe_parse_less_greater()) {
			result = mk_compare<NotEqual>(result, parse_sum());
		} else if (maybe_parse_less()) {
			result = mk_compare<Less>(result, parse_sum());
		} else if (maybe_parse_greater()) {
			result = mk_compare<Greater>(result, parse_sum());
		} else if (maybe_parse_less_equal()) {
			result = mk_compare<LessOrEqual>(result, parse_sum());
		} else if (maybe_parse_greater_equal()) {
			result = mk_compare<GreaterOrEqual>(result, parse_sum());
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
	Frame* body = new Frame(nullptr);
	push_scope(body);
	std::string visibility = "published";
	do {
		if (peek_keyword("end")) break;
		if (maybe_parse_directive("published")) {
			visibility = "published";
		} else if (maybe_parse_directive("public")) {
			visibility = "public";
		} else if (maybe_parse_directive("protected")) {
			visibility = "protected";
		} else if (maybe_parse_directive("private")) {
			visibility = "private";
		} else if (peek_keyword("type")) {
			parse_type_block(true);
		} else if (peek_keyword("const")) {
			parse_const_block();
		} else if (peek_keyword("var")) {
			parse_var_block();
		} else if (peek_keyword("procedure") || peek_keyword("function")) {
			parse_method_prototype(body, owner_class, peek_keyword("function"));
			continue;   // parse_method_prototype consumes its terminating ';'
		} else {
			auto member_name = parse_identifier();
			parse_colon();
			auto ty = parse_type_expression(false);
			body->register_variable(member_name, new StorageSlot(pascal_to_cxx_name(member_name), ty), ty);
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
	return body;
}

void Parser::parse_method_prototype(Frame* body, Type* owner_class, bool is_function) {
	parse_keyword(is_function ? "function" : "procedure");
	std::string pas_name = parse_identifier();
	std::vector<Parameter> formals;
	if (input_token == "(") formals = parse_proc_formal_parameters();
	Type* return_type = &unit_type();
	if (is_function) {
		parse_colon();
		return_type = parse_type_expression(false);
	}
	parse_semicolon();
	bool has_overload = false;
	Method::VirtualKind vk = Method::VirtualKind::None;
	while (true) {
		if (peek_keyword("overload")) { parse_keyword("overload"); has_overload = true; parse_semicolon(); }
		else if (peek_keyword("virtual"))  { parse_keyword("virtual");  vk = Method::VirtualKind::Virtual;  parse_semicolon(); }
		else if (peek_keyword("override")) { parse_keyword("override"); vk = Method::VirtualKind::Override; parse_semicolon(); }
		else if (peek_keyword("abstract")) { parse_keyword("abstract"); vk = Method::VirtualKind::Abstract; parse_semicolon(); }
		else if (peek_keyword("dynamic"))  { parse_keyword("dynamic");  vk = Method::VirtualKind::Dynamic;  parse_semicolon(); }
		else break;
	}
	auto m = new Method(pas_name, pascal_to_cxx_name(pas_name),
	                    std::move(formals), return_type, has_overload,
	                    owner_class, vk);
	if (!body->register_callable(pas_name, m)) {
		raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
	}
}

Type* Parser::parse_class_type() {
	parse_keyword("class");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("class inheritance (class(Parent)) not implemented yet");
	}
	auto ct = new ClassType(nullptr);
	ct->children = parse_aggregate_type_body(ct);
	parse_keyword("end");
	return ct;
}

Type* Parser::parse_record_type() {
	parse_keyword("record");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("record with parenthesized header not implemented yet");
	}
	auto rt = new RecordType(nullptr);
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
	do {
		parse_identifier();
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	parse_closing_paren();
	return raise_type_parse_error("parse_enum_type: EnumType construction not implemented yet");
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
	} else if (peek_keyword("record")) {
		return parse_record_type();
	} else if (peek_keyword("class")) {
		return parse_class_type();
	} else {
		// FIXME: constant folding for ranges (2..5 -> BoundedCardinalType)
		auto id = parse_identifier();
		return resolve_type(id, allow_forward);
	}
}

Node* Parser::parse_statement() {
	auto result = maybe_parse_statement();
	if (result == nullptr) {
		return raise_parse_error("missing statement");
	}
	return result;
}
Node* Parser::parse_block_body() {
	Block* block = new Block();
	while (input_token.size()) {
		Node* stmt = maybe_parse_statement();
		if (stmt) {
			block->add(stmt);
		}
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
	return block;
}
Frame* Parser::parse_const_block() {
	parse_keyword("const");
	auto scope = new Frame(nullptr);
	push_scope(scope);
	do {
		auto name = parse_identifier();
		// FIXME: handle actual compile-time consts which have no colon (and are no variables).
		parse_colon();
		auto ty = parse_type_expression(false);
		scope->register_variable(name, new StorageSlot(pascal_to_cxx_name(name), ty), ty);
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	return scope;
}
Frame* Parser::maybe_parse_const_block() {
	if (peek_keyword("const")) {
		return parse_const_block();
	} else {
		return nullptr;
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
Frame* Parser::parse_type_block(bool delphi_auto_end) {
	auto scope = new Frame(nullptr);
	parse_keyword("type");
	push_scope(scope);
	Frame* prev_type_block = current_type_block;
	current_type_block = scope;
	do {
		auto name_optional = maybe_parse_identifier();
		if (!name_optional) break;
		auto name = *name_optional;
		parse_equals();
		Type* existing = scope->lookup_type(name);
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
		Type* rhs = parse_type_expression(false);
		// Attach the LHS Pascal name (as its C++ identifier) to record-family
		// types so emit_type_ref has a name to spell.
		std::string cxx = pascal_to_cxx_name(name);
		if (auto r = dynamic_cast<RecordType*>(rhs)) r->cxx_name = cxx;
		else if (auto c = dynamic_cast<ClassType*>(rhs)) c->cxx_name = cxx;
		else if (auto o = dynamic_cast<ObjectType*>(rhs)) o->cxx_name = cxx;
		lhs_placeholder->resolved = rhs;
		scope->rebind_type(name, rhs);
		if (emitter) emitter->emit_type_definition(cxx, rhs);
		parse_semicolon();
	} while (true);
	for (auto& kv : scope->types()) {
		if (auto inc = dynamic_cast<IncompleteType*>(kv.second)) {
			if (!inc->resolved) {
				raise_parse_error("forward-referenced type not defined in this type block: " + kv.first);
			}
		}
	}
	current_type_block = prev_type_block;
	return scope;
}
/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually */
Frame* Parser::maybe_parse_type_block(bool delphi_auto_end) {
	if (peek_keyword("type")) {
		return parse_type_block(delphi_auto_end);
	} else {
		return nullptr;
	}
}
Frame* Parser::parse_var_block() {
	parse_keyword("var");
	auto scope = new Frame(nullptr);
	push_scope(scope);
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
			auto slot = new StorageSlot(pascal_to_cxx_name(name), ty);
			scope->register_variable(name, slot, ty);
			if (emitter) emitter->emit_var_decl(slot->cxx_name, ty);
		}
		parse_semicolon();
	} while (true);
	return scope;
}
Frame* Parser::maybe_parse_var_block() {
	if (peek_keyword("var")) {
		return parse_var_block();
	} else {
		return nullptr;
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
	while (true) {
		if (peek_keyword("type")) {
			parse_type_block(false);
			pushed++;
		} else if (peek_keyword("const")) {
			parse_const_block();
			pushed++;
		} else if (peek_keyword("var")) {
			parse_var_block();
			pushed++;
		} else if (peek_keyword("procedure")) {
			parse_procedure_or_function(false);
		} else if (peek_keyword("function")) {
			parse_procedure_or_function(true);
		} else {
			break;
		}
	}
	return pushed;
}

Node* Parser::parse_block() {
	size_t pushed = parse_decl_blocks();
	parse_keyword("begin");
	auto body = parse_block_body();
	parse_keyword("end");
	for (size_t i = 0; i < pushed; i++) pop_scope();
	return body;
}

Node* Parser::maybe_parse_proc_attributes() {
	// parse_semicolon();
	// TODO: inline
}

std::vector<Parameter> Parser::parse_proc_formal_parameters() {
	std::vector<Parameter> result;
	parse_opening_paren();
	if (input_token != ")") {
		do {
			ParamMode mode = ParamMode::Value;
			if (maybe_parse_keyword("var"))        mode = ParamMode::Var;
			else if (maybe_parse_keyword("out"))   mode = ParamMode::Out;
			else if (maybe_parse_keyword("const")) mode = ParamMode::Const;
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma()) names.push_back(parse_identifier());
			parse_colon();
			Type* ty = parse_type_expression(false);
			Node* default_value = nullptr;
			if (input_token == "=") {
				if (names.size() > 1) {
					raise_parse_error("default value not allowed with comma-grouped parameter names");
				}
				consume();
				default_value = parse_expression();
			}
			for (auto& n : names) {
				result.push_back(Parameter{n, pascal_to_cxx_name(n), ty, mode, default_value});
			}
		} while (maybe_parse_semicolon());
	}
	parse_closing_paren();
	return result;
}

void Parser::parse_procedure_or_function(bool is_function) {
	parse_keyword(is_function ? "function" : "procedure");
	std::string first_name = parse_identifier();
	// Qualified name -> external method body: `procedure TFoo.Bar(...);`
	// The Method entity was already registered inside the class body; we
	// look it up, re-parse the formals for the syntactic hit, and attach the
	// body to the existing Method.
	if (input_token == ".") {
		consume();
		std::string method_name = parse_identifier();
		Type* owner_ty = resolve_type(first_name, false);
		Frame* owner_frame = nullptr;
		if (auto r = dynamic_cast<RecordType*>(owner_ty)) owner_frame = r->children;
		else if (auto c = dynamic_cast<ClassType*>(owner_ty)) owner_frame = c->children;
		else if (auto o = dynamic_cast<ObjectType*>(owner_ty)) owner_frame = o->children;
		if (!owner_frame) raise_parse_error("'" + first_name + "' is not a class/record/object");
		Node* hit = owner_frame->lookup_value(method_name);
		auto m = dynamic_cast<Method*>(hit);
		if (!m) raise_parse_error("no method '" + method_name + "' on '" + first_name + "'");
		std::vector<Parameter> formals;
		if (input_token == "(") formals = parse_proc_formal_parameters();
		// TODO: verify formals match the prototype; for now we accept whatever
		// the definition site provided and use the prototype's formals as the
		// source of truth for later resolution.
		Type* return_type = &unit_type();
		if (is_function) {
			parse_colon();
			return_type = parse_type_expression(false);
		}
		parse_semicolon();
		// Body_frame parented at the class body so members are visible via
		// parent chain. Push it, plus an implicit Self-with-scope so bare
		// field/method references inside the body auto-wrap as
		// MemberAccess(SelfSlot, member).
		Frame* body_frame = new Frame(owner_frame);
		m->body_frame = body_frame;
		Type* self_ptr_ty = new PointerType(owner_ty);
		auto self_slot = new StorageSlot("this", self_ptr_ty);
		body_frame->register_variable("self", self_slot, self_ptr_ty);
		for (auto& p : m->formals) {
			body_frame->register_variable(p.pas_name, new StorageSlot(p.cxx_name, p.ty), p.ty);
		}
		push_scope(body_frame);
		push_with_scope(owner_frame, self_slot);
		if (emitter) emitter->emit_procedure_open(m);
		size_t pushed = parse_decl_blocks();
		parse_keyword("begin");
		m->body = parse_block_body();
		parse_keyword("end");
		parse_semicolon();
		if (emitter) emitter->emit_procedure_close();
		for (size_t i = 0; i < pushed; i++) pop_scope();
		pop_scope();   // with-scope
		pop_scope();   // body_frame
		return;
	}
	std::string pas_name = std::move(first_name);
	std::vector<Parameter> formals;
	// Pascal allows omitting the empty parameter list: `procedure foo;`.
	if (input_token == "(") formals = parse_proc_formal_parameters();
	Type* return_type = &unit_type();
	if (is_function) {
		parse_colon();
		return_type = parse_type_expression(false);
	}
	parse_semicolon();
	bool has_overload = false;
	while (peek_keyword("overload")) {
		parse_keyword("overload");
		has_overload = true;
		parse_semicolon();
	}
	auto proc = new Procedure(pas_name, pascal_to_cxx_name(pas_name),
	                          std::move(formals), return_type, has_overload);
	Frame* enclosing = const_cast<Frame*>(this->scopes.back().frame);
	if (!enclosing->register_callable(pas_name, proc)) {
		raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
	}
	if (peek_keyword("forward")) {
		parse_keyword("forward");
		parse_semicolon();
		// Prototype-only registration; body attached by a later definition.
		// TODO: match a later definition against this prototype instead of
		// treating it as a fresh registration (which currently errors as
		// duplicate).
		return;
	}
	Frame* body_frame = new Frame(enclosing);
	proc->body_frame = body_frame;
	push_scope(body_frame);
	for (auto& p : proc->formals) {
		body_frame->register_variable(p.pas_name, new StorageSlot(p.cxx_name, p.ty), p.ty);
	}
	if (emitter) emitter->emit_procedure_open(proc);
	size_t pushed = parse_decl_blocks();
	parse_keyword("begin");
	proc->body = parse_block_body();
	parse_keyword("end");
	parse_semicolon();
	if (emitter) emitter->emit_procedure_close();
	for (size_t i = 0; i < pushed; i++) pop_scope();
	pop_scope(); // body_frame
}

Node* Parser::parse_constructor_prototype() {
	parse_keyword("constructor");
	auto id = parse_identifier();
    auto formal_parameters = parse_proc_formal_parameters();
	maybe_parse_proc_attributes();
}

Node* Parser::parse_constructor() {
	parse_constructor_prototype();
	parse_block();
}

Node* Parser::parse_destructor_prototype() {
	parse_keyword("destructor");
	auto id = parse_identifier();
    auto formal_parameters = parse_proc_formal_parameters();
	maybe_parse_proc_attributes();
}

Node* Parser::parse_destructor() {
	parse_destructor_prototype();
	parse_block();
}

// Per-argument conversion costs for a candidate. Returns empty vector when
// the candidate isn't viable. Costs sized to formals.size() (+1 for Self
// when candidate is a Method with receiver); entries past args.size() are 0
// (default-supplied positions).
static std::vector<int> per_arg_costs(Callable* c, Node* receiver, const std::vector<Node*>& args) {
	if (args.size() > c->formals.size()) return {};
	for (size_t i = args.size(); i < c->formals.size(); i++) {
		if (!c->formals[i].default_value) return {};
	}
	// Self position (if any) is prepended to the cost vector.
	auto m = dynamic_cast<Method*>(c);
	size_t self_slots = (m && receiver) ? 1 : 0;
	std::vector<int> costs(self_slots + c->formals.size(), 0);
	if (self_slots) {
		int sc = conversion_cost(receiver ? receiver->ty : nullptr, m->owner_class);
		if (sc < 0) return {};
		costs[0] = sc;
	}
	for (size_t i = 0; i < args.size(); i++) {
		Type* from = args[i] ? args[i]->ty : nullptr;
		int cc = conversion_cost(from, c->formals[i].ty);
		if (cc < 0) return {};
		costs[self_slots + i] = cc;
	}
	return costs;
}

// A dominates B iff A's cost is <= B's on every position AND strictly < on
// at least one. Different-length vectors don't compare (ambiguity later).
static bool dominates(const std::vector<int>& a, const std::vector<int>& b) {
	if (a.size() != b.size()) return false;
	bool strict = false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i] > b[i]) return false;
		if (a[i] < b[i]) strict = true;
	}
	return strict;
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
			if (!costs.empty()) viable.push_back({c, std::move(costs)});
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
			if (!dom) non_dominated.push_back(viable[i].first);
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
	while (args.size() < chosen->formals.size()) {
		auto& p = chosen->formals[args.size()];
		if (!p.default_value) {
			raise_parse_error("missing argument for parameter '" + p.pas_name + "' in call to '" + name_for_error + "'");
		}
		args.push_back(p.default_value);
	}
	if (args.size() > chosen->formals.size()) {
		raise_parse_error("too many arguments to '" + name_for_error + "'");
	}
	// Insert Cast for any arg whose type differs from the formal.
	for (size_t i = 0; i < args.size(); i++) {
		Type* t = chosen->formals[i].ty;
		if (args[i]->ty != t) args[i] = new Cast(args[i], t);
	}
	return FinalizedCall{receiver, chosen};
}

Unit* Parser::load_or_get_unit(std::string name) {
	if (Unit* existing = unit_registry->lookup(name)) return existing;
	// Search dir of the current input file, then CWD.
	std::string dir;
	auto slash = input_file_name.find_last_of('/');
	if (slash != std::string::npos) dir = input_file_name.substr(0, slash + 1);
	std::vector<std::string> candidates;
	if (!dir.empty()) candidates.push_back(dir + name + ".pp");
	candidates.push_back(name + ".pp");
	FILE* f = nullptr;
	std::string opened;
	for (auto& p : candidates) {
		f = fopen(p.c_str(), "r");
		if (f) { opened = p; break; }
	}
	if (!f) raise_parse_error("cannot find unit file for: " + name);
	// Nested Parser so the sub-load has its own token/scope state; the shared
	// unit_registry is what lets circular-dep detection work across the two.
	Parser sub(unit_registry, nullptr, options);
	sub.push_input_file(f, opened, 1);
	sub.start();
	sub.parse_program_or_unit();
	Unit* loaded = unit_registry->lookup(name);
	if (!loaded) raise_parse_error("file '" + opened + "' did not declare 'unit " + name + ";'");
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
		if (!maybe_parse_comma()) break;
	} while (true);
	return pushed;
}

Node* Parser::parse_unit_body() {
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
	size_t iface_uses = 0;
	if (maybe_parse_keyword("uses")) {
		iface_uses = parse_uses_clause(true, name);
		parse_semicolon();
	}
	// TODO(later phase): parse interface declarations (types, consts, vars,
	// procedure/function prototypes) here.
	unit->phase = UnitPhase::InterfaceDone;

	parse_keyword("implementation");
	push_scope(impl);
	unit->phase = UnitPhase::ImplementationInProgress;
	size_t impl_uses = 0;
	if (maybe_parse_keyword("uses")) {
		impl_uses = parse_uses_clause(false, name);
		parse_semicolon();
	}
	// TODO(later phase): parse implementation declarations + init/final bodies.

	parse_keyword("end");
	parse_period();

	// Pop in reverse push order.
	for (size_t i = 0; i < impl_uses; i++) pop_scope();
	pop_scope(); // impl
	for (size_t i = 0; i < iface_uses; i++) pop_scope();
	pop_scope(); // iface

	unit->phase = UnitPhase::Done;
	return nullptr;
}

Node* Parser::parse_program_or_unit() {
	if (maybe_parse_keyword("program")) {
		auto name = parse_identifier();
		parse_semicolon();
		Frame* impl = new Frame(nullptr);
		Unit* unit = unit_registry->register_new(name, nullptr, impl);
		unit->phase = UnitPhase::InterfaceInProgress;
		push_scope(impl);
		if (peek_keyword("uses")) {
			raise_parse_error("`uses` clauses are not yet supported");
		}
		if (emitter) emitter->emit_program_prologue(name);
		// Inlined equivalent of parse_block; we need to bracket the body-block
		// with main() emission hooks, which parse_block itself doesn't know
		// about (it's also called from procedure bodies).
		size_t pushed = parse_decl_blocks();
		parse_keyword("begin");
		if (emitter) emitter->emit_main_prologue();
		auto body = parse_block_body();
		parse_keyword("end");
		if (emitter) emitter->emit_main_epilogue();
		for (size_t i = 0; i < pushed; i++) pop_scope();
		parse_period();
		pop_scope();
		unit->phase = UnitPhase::Done;
		return body;
	} else if (maybe_parse_keyword("unit")) {
		return parse_unit_body();
	} else {
		return raise_parse_error("unknown input token");
	}
}
