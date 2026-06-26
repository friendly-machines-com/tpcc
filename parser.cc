#include <cassert>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
#include "parser.h"
#include "cst.h"
#include "scope.h"
#include "evaluator.h"

Parser::Parser(Scope* scope, ValueScope* value_scope) {
	this->scope = scope;
	this->value_scope = value_scope;
}
void Parser::pop_input_file() {
	input_files.pop_back();
	auto p = input_files.back();
	input_file = p.input_file;
	input_file_line_number = p.input_file_line_number;
}
int Parser::consume_lowlevel() {
	int result = input_char;
	if (result == '\n') {
		++this->input_file_line_number;
	}
	input_char = fgetc(input_file);
	if (input_char == EOF) {
		if (input_files.size()) {
			pop_input_file();
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

void Parser::push_scope(Scope* scope) {
	assert(scope->parent == nullptr || scope->parent == this->scope);
	scope->parent = this->scope;
	this->scope = scope;
}

void Parser::pop_scope() {
	assert(this->scope);
	this->scope = this->scope->parent;
}

Node* Parser::raise_parse_error(std::string message) {
	std::stringstream sst;
	sst << input_file_name << '(' << this->input_file_line_number << ')' << ':' << ' ';
	sst << message << std::endl;
	std::string r = sst.str();
	fprintf(stderr, "%s\n", r.c_str());
	fflush(stderr);
	abort();
	return nullptr;
}

std::string Parser::consume() {
	auto result = input_token;
	std::stringstream sst;
	sst.str("");
	while (input_char == ' ' || input_char == '\n' || input_char == '\r') {
		consume_lowlevel();
	}
	if ((input_char >= 'a' && input_char <= 'z') | (input_char >= 'A' && input_char <= 'Z') || input_char == '_') {
		while ((input_char >= 'a' && input_char <= 'z') || (input_char >= 'A' && input_char <= 'Z') || input_char == '_') {
			sst << (char) tolower(input_char);
			consume_lowlevel();
		}
	} else if ((input_char >= '0' && input_char <= '9') | input_char == '.' || input_char == '_') {
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
	} else if (input_char != EOF && strchr("=;,[]()@*+-/:^", input_char)) {
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
	input_char = fgetc(input_file);
	consume();
}
bool Parser::peek_keyword(std::string s) {
	if (input_token == s) { // FIXME case insensitive
		consume();
		return true;
	} else {
		return false;
	}
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
Node* Parser::maybe_parse_statement() {
	if (input_token == "end") {
		return nullptr;
	} else {
		raise_parse_error("missing statement");
	}
}

std::string Parser::parse_identifier() {
	consume(); // FIXME!!! detect and reject keywords
}

Node* Parser::parse_value() {
	if (maybe_parse_opening_paren()) { // grouping paren
		auto result = parse_expression();
		parse_closing_paren();
		return result;
	} else {
		auto id = parse_identifier(); // FIXME: or numeral or bool as well; FIXME: resolve identifier
		auto fn = id;
		auto args = parse_expression();
		if (maybe_parse_opening_paren()) { // function call
			auto call = new ProcCall(args);
			std::optional<uint64_t> value = evaluate(scope, call);
			if (value) {
				return new Constant(*value);
			} else {
				return new ProcCall(args);
			}
		} else {
			return id;
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
bool Parser::parse_period() {
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
	// FIXME ** - +
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
			result = new Mod(result, parse_power());
		} else if (maybe_parse_keyword("shl")) {
			result = new Mod(result, parse_power());
		} else if (maybe_parse_keyword("shr")) {
			result = new Mod(result, parse_power());
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

Type* Parser::parse_aggregate_type_body() {
	push_scope(new Scope(this->scope));
	std::string visibility = "published";
	do {
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
			auto ty = parse_type_expression();
			scope->register_variable(member_name, new StorageSlot(ty));
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
}

Type* Parser::parse_class_type() {
	parse_keyword("class");
	if (maybe_parse_opening_paren()) {
		// FIXME
	}
	//push_scope();
	parse_aggregate_type_body();
	//pop_scope();
	parse_keyword("end");
}

Type* Parser::parse_record_type() {
	parse_keyword("record");
	if (maybe_parse_opening_paren()) {
		// FIXME
	}
	parse_aggregate_type_body();
	parse_keyword("end");
}

Type* Parser::parse_object_type() {
	parse_keyword("object");
	if (maybe_parse_opening_paren()) {
		// FIXME
	}
	// FIXME
	parse_aggregate_type_body();
	parse_keyword("end");
}

Type* Parser::parse_array_type() {
	parse_keyword("array");
	parse_opening_bracket();
	auto bounds_type = parse_type_expression();
	parse_closing_bracket();
	parse_keyword("of");
	auto item_type = parse_type_expression();
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
}

Type* Parser::parse_type_expression() {
	if (maybe_parse_opening_paren()) {
		return parse_enum_type();
	} else if (maybe_parse_circumflex()) {
		return new PointerType(parse_type_expression());
	} else if (peek_keyword("string")) {
		parse_keyword("string");
		parse_opening_bracket();
		parse_expression();
		parse_closing_bracket();
	} else if (peek_keyword("set")) {
		parse_keyword("set");
		parse_keyword("of");
		return new FixedSetType(parse_type_expression());
	} else if (peek_keyword("array")) {
		return parse_array_type();
	} else if (peek_keyword("object")) {
		return parse_object_type();
	} else if (peek_keyword("record")) {
		return parse_record_type();
	} else if (peek_keyword("class")) {
		return parse_class_type();
	} else {
		// FIXME: do constant folding and then BoundedCardinalType; 2..5 as type => BoundedCardinalType
		// FIXME: probably need a similar thing for string[7]
		auto id = parse_identifier(); // FIXME: resolve.
		return id;
	}
}

Node* Parser::parse_statement() {
	auto result = maybe_parse_statement();
	if (result == nullptr) {
		raise_parse_error("missing statement");
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
		} else {
			auto id = parse_identifier(); // FIXME resolve
			Node* storage = nullptr; // FIXME
			parse_colon_equals();
			auto value = parse_expression();
			return new Assign(storage, value);
		}
	}
}
Node* Parser::parse_block_body() {
	while (input_token.size()) {
		maybe_parse_statement();
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
}
Scope* Parser::parse_const_block() {
	parse_keyword("const");
	auto scope = new Scope(this->scope);
	push_scope(scope);
	do {
		auto name = parse_identifier();
		// FIXME: handle actual compile-time consts which have no colon (and are no variables).
		parse_colon();
		auto ty = parse_type_expression();
		scope->register_variable(name, new StorageSlot(ty));
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	return scope;
}
Scope* Parser::maybe_parse_const_block() {
	if (peek_keyword("const")) {
		return parse_const_block();
	} else {
		return nullptr;
	}
}
/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually.

DELPHI_AUTO_END: will automatically stop at some aggregate control directives (like "public" etc).
 */
Scope* Parser::parse_type_block(bool delphi_auto_end) {
	auto scope = new Scope(this->scope);
	parse_keyword("type");
	push_scope(scope);
	// FIXME: here, it's allowed to have the special cases: "type PX = ^TX; TX = record" and "type TFoo = class x: TFoo"
	do {
		auto name = parse_identifier();
		parse_equals();
		auto ty = parse_type_expression();
		scope->register_type(name, ty);
		if (!maybe_parse_comma())
			break;
	} while (true);
	return scope;
}
/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually */
Scope* Parser::maybe_parse_type_block(bool delphi_auto_end) {
	if (peek_keyword("type")) {
		return parse_type_block(delphi_auto_end);
	} else {
		return nullptr;
	}
}
Scope* Parser::parse_var_block() {
	parse_keyword("var");
	auto scope = new Scope(this->scope);
	push_scope(scope);
	do {
		auto name = parse_identifier();
		parse_colon();
		auto ty = parse_type_expression();
		scope->register_variable(name, new StorageSlot(ty));
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	return scope;
}
Scope* Parser::maybe_parse_var_block() {
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
	maybe_parse_type_block(false);
	maybe_parse_const_block();
	maybe_parse_var_block();
	parse_keyword("begin");
	parse_block_body();
	parse_keyword("end");
	pop_scope(); // Note: Was pushed by parse_type_block
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
		auto ty = parse_type_expression();
		auto storage = new StorageSlot(ty);
		scope->register_variable(id, storage);
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
	auto result_type = parse_type_expression();
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

Node* Parser::parse_unit() {
	parse_keyword("unit");
	parse_keyword("interface");
	parse_keyword("implementation");
	parse_keyword("initialization");
	parse_keyword("finalization");
	parse_keyword("end");
	parse_period();
}

Node* Parser::parse_program_or_unit() {
	if (peek_keyword("program")) {
		consume();
		auto name = consume();
		parse_semicolon();
		parse_block();
		auto result = parse_block();
		parse_keyword("end");
		parse_period();
		return result;
	} else if (peek_keyword("unit")) {
		return parse_unit();
	} else {
		raise_parse_error("unknown input token");
	}
}
