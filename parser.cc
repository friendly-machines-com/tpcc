#include <cassert>
#include <cstdlib>
#include <cstring>
#include <charconv>
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

Parser::Parser(UnitRegistry* unit_registry, Emitter* emitter)
	: unit_registry(unit_registry), emitter(emitter) {
}
void Parser::pop_input_file() {
	assert(!input_files.empty());
	fclose(input_files.back().input_file);
	input_files.pop_back();
	if (input_files.empty()) {
		input_file = nullptr;
		input_file_name = "";
		input_file_line_number = 0;
		return;
	}
	auto& p = input_files.back();
	input_file = p.input_file;
	input_file_name = p.input_file_name;
	input_file_line_number = p.input_file_line_number;
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
	if (input_char == EOF) {
		pop_input_file();
		if (input_file) {
			consume_lowlevel();
		}
	}
	return result;
}
void Parser::push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number) {
	input_files.push_back(ParserInputFile {
		.input_file = input_file,
		.input_file_name = input_file_name,
		.input_file_line_number = input_file_line_number
	});
	this->input_file = input_file;
	this->input_file_name = input_file_name;
	this->input_file_line_number = input_file_line_number;
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
	input_char = fgetc(input_file);
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
			auto id = parse_identifier();
			Node* storage = resolve_lvalue(id);
			parse_colon_equals();
			auto value = parse_expression();
			auto assign = new Assign(storage, value);
			if (emitter) emitter->emit_statement(assign);
			return assign;
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
			auto lit = new Constant(value);
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
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Node* hit = it->frame->lookup_value(name)) {
			if (it->unwrap_via) return new MemberAccess(it->unwrap_via, hit);
			return hit;
		}
	}
	raise_parse_error("unresolved value identifier: " + name);
	return nullptr;
}

/** value that can be assigned to */
Node* Parser::resolve_lvalue(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Node* hit = it->frame->lookup_value(name)) {
			if (it->unwrap_via) return new MemberAccess(it->unwrap_via, hit);
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
			Node* fn = resolve_value(id);
			if (maybe_parse_opening_paren()) { // function/procedure call
				auto args = parse_expression();
				parse_closing_paren();
				return new ProcCall(fn, args);
			} else {
				return fn;
			}
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

Node* Parser::parse_power() {
	// FIXME **
	if (maybe_parse_keyword("not")) {
		return new Not(parse_value());
	} else if (maybe_parse_at()) {
		return new AddrOf(parse_value());
	} else if (maybe_parse_minus()) {
		return new Negate(parse_value());
	} else if (maybe_parse_plus()) {
		return new Positivize(parse_value());
	} else {
		return parse_value();
	}
}

Node* Parser::parse_product() {
	auto result = parse_value();
	while (true) {
		if (maybe_parse_star()) {
			result = new Multiply(result, parse_power());
		} else if (maybe_parse_slash()) {
			result = new Divide(result, parse_power());
		} else if (maybe_parse_keyword("div")) {
			result = new Div(result, parse_power());
		} else if (maybe_parse_keyword("mod")) {
			result = new Mod(result, parse_power());
		} else if (maybe_parse_keyword("and")) {
			result = new And(result, parse_power());
		} else if (maybe_parse_keyword("shl")) {
			result = new ShiftLeft(result, parse_power());
		} else if (maybe_parse_keyword("shr")) {
			result = new ShiftRight(result, parse_power());
		} else if (maybe_parse_keyword("as")) {
			result = new Coerce(result, parse_power());
		} else if (maybe_parse_less_less()) {
			result = new ShiftLeft(result, parse_power());
		} else if (maybe_parse_greater_greater()) {
			result = new ShiftRight(result, parse_power());
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
			result = new Add(result, parse_product());
		} else if (maybe_parse_minus()) {
			result = new Subtract(result, parse_product());
		} else if (maybe_parse_keyword("or")) {
			result = new Or(result, parse_product());
		} else if (maybe_parse_keyword("xor")) {
			result = new Xor(result, parse_product());
		// ><
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
			result = new Equal(result, parse_sum());
		} else if (maybe_parse_less_greater()) {
			result = new NotEqual(result, parse_sum());
		} else if (maybe_parse_less()) {
			result = new Less(result, parse_sum());
		} else if (maybe_parse_greater()) {
			result = new Greater(result, parse_sum());
		} else if (maybe_parse_less_equal()) {
			result = new LessOrEqual(result, parse_sum());
		} else if (maybe_parse_greater_equal()) {
			result = new GreaterOrEqual(result, parse_sum());
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_expression() {
	return parse_comparison();
}

Frame* Parser::parse_aggregate_type_body() {
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

Type* Parser::parse_class_type() {
	parse_keyword("class");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("class inheritance (class(Parent)) not implemented yet");
	}
	Frame* body = parse_aggregate_type_body();
	parse_keyword("end");
	return new ClassType(body);
}

Type* Parser::parse_record_type() {
	parse_keyword("record");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("record with parenthesized header not implemented yet");
	}
	Frame* body = parse_aggregate_type_body();
	parse_keyword("end");
	return new RecordType(body);
}

Type* Parser::parse_object_type() {
	parse_keyword("object");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("object with parenthesized header not implemented yet");
	}
	Frame* body = parse_aggregate_type_body();
	parse_keyword("end");
	return new ObjectType(body);
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
		lhs_placeholder->resolved = rhs;
		scope->rebind_type(name, rhs);
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

Node* Parser::parse_block() {
	auto type_scope = maybe_parse_type_block(false);
	auto const_scope = maybe_parse_const_block();
	auto var_scope = maybe_parse_var_block();
	parse_keyword("begin");
	auto body = parse_block_body();
	parse_keyword("end");
	// pop in reverse push order
	if (var_scope) pop_scope();
	if (const_scope) pop_scope();
	if (type_scope) pop_scope();
	return body;
}

Node* Parser::maybe_parse_proc_attributes() {
	// parse_semicolon();
	// TODO: inline
}

Node* Parser::parse_proc_formal_parameters() {
	parse_opening_paren();
	do {
		// TODO: var, out.
		// TODO: a,b: Integer;
		auto id = parse_identifier();
		// TODO: ",b,c,d"
		parse_colon();
		auto ty = parse_type_expression(false);
		auto storage = new StorageSlot(pascal_to_cxx_name(id), ty);
		// TODO: this registers formals into the enclosing scope, which is
		// wrong (formals belong in the procedure's own body_frame). Fixed by
		// the procedure refactor, which will pass an explicit target Frame*
		// here. Until then, the target is scopes.back() and must be treated
		// as non-const; the const_cast marks that this is a known escape.
		const_cast<Frame*>(this->scopes.back().frame)->register_variable(id, storage, ty);
		if (!maybe_parse_semicolon()) {
			break;
		}
	} while (true);
	parse_closing_paren();
}

Node* Parser::parse_procedure_prototype() {
	parse_keyword("procedure");
	auto id = parse_identifier();
	auto formal_parameters = parse_proc_formal_parameters();
	maybe_parse_proc_attributes();
}

Node* Parser::parse_procedure() {
	parse_procedure_prototype();
	parse_block();
}

Node* Parser::parse_function_prototype() {
	parse_keyword("function");
	auto id = parse_identifier();
	auto formal_parameters = parse_proc_formal_parameters();
	parse_colon();
	auto result_type = parse_type_expression(false);
	maybe_parse_proc_attributes();
}

Node* Parser::parse_function() {
	parse_function_prototype();
	parse_block();
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
	Parser sub(unit_registry, nullptr);
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
		auto type_scope = maybe_parse_type_block(false);
		auto const_scope = maybe_parse_const_block();
		auto var_scope = maybe_parse_var_block();
		parse_keyword("begin");
		if (emitter) emitter->emit_main_prologue();
		auto body = parse_block_body();
		parse_keyword("end");
		if (emitter) emitter->emit_main_epilogue();
		if (var_scope) pop_scope();
		if (const_scope) pop_scope();
		if (type_scope) pop_scope();
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
