#pragma once
#include <cstdio>
#include <string>
#include <stack>
#include <vector>
#include <optional>

class Node;
class Symbol;
class Type;
class Unit;
class UnitRegistry;
class Emitter;
struct Parameter;

class ParserInputFile {
public:
	FILE* input_file;
	std::string input_file_name;
	int input_file_line_number;
};

class Frame;

/** One entry on the parser's scope stack. `frame` is the declaration frame
 *  (locals, unit interface, record body, etc.). `unwrap_via` is null for
 *  every kind of scope except a `with` push: when non-null, a resolve hit in
 *  this frame is wrapped as `MemberAccess(unwrap_via, hit)` before being
 *  returned to the caller, so `field` inside `with rec do ...` produces
 *  `rec.field` at emit time. */
struct ScopeEntry {
	const Frame* frame;
	Node* unwrap_via;
};

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
	std::vector<ScopeEntry> scopes; // TODO: stack
	// The type-block scope currently being parsed, or nullptr. Used as the
	// registration site for implicit forward references (`^TFoo` before TFoo
	// is declared); those must land in the enclosing type block's scope, not
	// in whatever inner scope (record/class body) happens to be on top.
	Frame* current_type_block = nullptr;
	UnitRegistry* unit_registry;
	// May be null. When non-null, emission hooks in the parser call into it
	// as declarations and statements are parsed. Null is used only by
	// sub-parsers loading a `uses`d unit until unit-level emission (.h/.cc
	// per unit) is implemented.
	Emitter* emitter;
protected:
	std::string input_token;
	Node* parse_block_body();
	Frame* maybe_parse_const_block();
	Frame* maybe_parse_type_block(bool delphi_auto_end);
	Frame* maybe_parse_var_block();
	Frame* parse_const_block();
	Frame* parse_type_block(bool delphi_auto_end);
	Frame* parse_var_block();
	/** Parse a sequence of top-of-block declarations in any order (Pascal
	 *  allows `type`, `const`, `var` blocks and `procedure`/`function` decls
	 *  interleaved freely). Returns the count of scopes pushed so the caller
	 *  can pop that many after the body. */
	size_t parse_decl_blocks();
	Node* parse_block();
	void parse_semicolon();
	Node* maybe_parse_statement();
	std::optional<std::string> maybe_parse_identifier();
	std::string parse_identifier();
	Node* maybe_parse_numeral();
	Node* parse_numeral();
	Node* resolve_lvalue(std::string name);
	Node* resolve_value(std::string name);
	Type* resolve_type(std::string name, bool allow_forward);
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
	Type* parse_type_expression(bool allow_forward);
	Node* parse_expression();
	Node* parse_statement();
	Frame* parse_aggregate_type_body();
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
	Node* parse_unit_body();
	/** Parse a comma-separated `uses A, B, C` list (the `uses` keyword must
	 *  have been consumed by the caller). Loads each named unit if not already
	 *  in the registry, checks the phase rules (interface-position use of an
	 *  InterfaceInProgress unit is a circular-dep error), and pushes each
	 *  loaded unit's interface_frame onto the scope stack. Returns the count
	 *  pushed so the caller can pop the same number at section end. */
	size_t parse_uses_clause(bool in_interface, std::string current_name);
	/** Return the Unit for NAME, loading its source from disk if it isn't
	 *  already registered. Search order for the file: directory of the current
	 *  input file, then CWD. */
	Unit* load_or_get_unit(std::string name);
	/** Result of call finalization: the concrete callee to place in
	 *  ProcCall.callee, plus the receiver expression if the call carries one
	 *  (method calls). receiver is null for standalone calls. */
	struct FinalizedCall {
		Node* receiver;
		Node* callee;
	};
	/** Given a resolved target (Callable, OverloadSet, MemberAccess-wrapping
	 *  either of those, or a Builtin) and parsed args, peel any MemberAccess
	 *  to extract a receiver, run overload ranking if the target is a set,
	 *  materialize defaults, and insert Cast coercions where needed. Errors
	 *  on no-match, ambiguous overload, or bad args. */
	FinalizedCall finalize_call(Node* target, std::vector<Node*>& args, std::string name_for_error);
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
	/** Push a plain declaration frame. */
	void push_scope(const Frame* scope);
	/** Push a frame that participates in resolution as a `with` binding:
	 *  hits in FRAME are wrapped as MemberAccess(UNWRAP_VIA, hit). */
	void push_with_scope(const Frame* scope, Node* unwrap_via);
	void pop_scope();
	Node* maybe_parse_proc_attributes();
	/** Parse `procedure NAME(...);` (is_function=false) or
	 *  `function NAME(...): T;` (is_function=true). Attribute list (`overload;`)
	 *  is consumed after the terminating `;`. If followed by a body, parses
	 *  it into a fresh body_frame; if followed by `forward;`, leaves body
	 *  null. Registers the resulting Procedure in the current scope and emits
	 *  the signature/body when an emitter is attached. */
	void parse_procedure_or_function(bool is_function);
	Node* parse_constructor_prototype();
	Node* parse_constructor();
	Node* parse_destructor_prototype();
	Node* parse_destructor();
	std::vector<Parameter> parse_proc_formal_parameters();

	[[noreturn]] Node* raise_parse_error(std::string message);
	[[noreturn]] Type* raise_type_parse_error(std::string message);

public:
	Parser(UnitRegistry* unit_registry, Emitter* emitter);
	void push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number);
	void pop_input_file();
	void start();
	Node* parse_program_or_unit();
};
