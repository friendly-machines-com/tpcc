#pragma once
#include <cstdio>
#include <string>
#include <stack>
#include <vector>
#include <optional>

class Node;
class Symbol;
class Type;

class ParserInputFile {
public:
	FILE* input_file;
	std::string input_file_name;
	int input_file_line_number;
};

class Frame;

class Parser {
private:
	FILE* input_file;
	std::string input_file_name;
	int input_file_line_number;
	int input_char;
	int consume_lowlevel();
	std::string consume();
	bool peek_keyword(std::string s);
	void parse_keyword(std::string s);
	bool maybe_parse_keyword(std::string s);
	std::vector<ParserInputFile> input_files; // TODO: stack
	std::vector<Frame*> scopes; // TODO: stack
protected:
	std::string input_token;
	Node* parse_block_body();
	Frame* maybe_parse_const_block();
	Frame* maybe_parse_type_block(bool delphi_auto_end);
	Frame* maybe_parse_var_block();
	Frame* parse_const_block();
	Frame* parse_type_block(bool delphi_auto_end);
	Frame* parse_var_block();
	Node* parse_block();
	void parse_semicolon();
	Node* maybe_parse_statement();
	std::optional<std::string> maybe_parse_identifier();
	std::string parse_identifier();
	Node* maybe_parse_numeral();
	Node* parse_numeral();
	Node* resolve_lvalue(std::string name);
	Node* resolve_value(std::string name);
	Type* resolve_type(std::string name);
    bool maybe_parse_directive(std::string directive);
	Node* parse_value();
	Node* parse_comparison();
	Node* parse_power();
	Node* parse_product();
	Node* parse_sum();
	Type* parse_array_type();
	Type* parse_object_type();
	Type* parse_record_type();
	Type* parse_class_type();
	Type* parse_enum_type();
	Type* parse_type_expression();
	Node* parse_expression();
	Node* parse_statement();
	Type* parse_aggregate_type_body();
	bool maybe_parse_semicolon();
	bool maybe_parse_opening_paren();
	void parse_opening_paren();
	void parse_closing_paren();
	bool maybe_parse_opening_bracket();
	void parse_opening_bracket();
	void parse_closing_bracket();
	void parse_colon_equals();
	void parse_colon();
	void parse_equals();
	bool maybe_parse_comma();
	Node* parse_unit();
	bool maybe_parse_plus();
	bool maybe_parse_minus();
	bool maybe_parse_star();
	bool maybe_parse_slash();
	bool maybe_parse_circumflex();
	bool maybe_parse_at();
	bool maybe_parse_less_less();
	bool maybe_parse_greater_greater();
	bool maybe_parse_equal();
	bool maybe_parse_less_greater();
	bool maybe_parse_less();
	bool maybe_parse_greater();
	bool maybe_parse_less_equal();
	bool maybe_parse_greater_equal();
	bool maybe_parse_period();
	void parse_period();
	void push_scope(Frame* scope);
	void pop_scope();
	Node* maybe_parse_proc_attributes();
	Node* parse_procedure_prototype();
	Node* parse_procedure();
	Node* parse_function_prototype();
	Node* parse_function();
	Node* parse_constructor_prototype();
	Node* parse_constructor();
	Node* parse_destructor_prototype();
	Node* parse_destructor();
	Node* parse_proc_formal_parameters();

	[[noreturn]] Node* raise_parse_error(std::string message);
	[[noreturn]] Type* raise_type_parse_error(std::string message);

public:
	Parser();
	void push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number);
	void pop_input_file();
	void start();
	Node* parse_program_or_unit();
};
