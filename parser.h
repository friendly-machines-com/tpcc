#pragma once
#include <cstdio>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <optional>

class Node;
class Symbol;
class Type;
class Unit;
class UnitRegistry;
class Emitter;
struct Parameter;

/** Shared compiler-wide options set from the command line and consulted by
 *  the tokenizer's directive handling and by unit/include file lookup. One
 *  instance lives in main; every Parser (main plus sub-parsers spawned by
 *  `uses` loading) reads from it and can mutate `defines` via `{$define}` /
 *  `{$undef}` seen in source. */
struct CompilerOptions {
	// Symbol -> value ("" for boolean defines). Populated from -d<sym>[:=<val>]
	// and from {$define} directives.
	std::map<std::string, std::string> defines;
	// -Fu<path> entries. load_or_get_unit walks these when a `uses` name
	// isn't in the current-input dir or CWD.
	std::vector<std::string> unit_search_paths;
	// -Fi<path> entries. Used by {$i <file>} include-file resolution.
	std::vector<std::string> include_search_paths;
};

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
	// Shared across the top-level parser and any sub-parsers it spawns.
	CompilerOptions* options;
	// One frame per open {$ifdef}/{$if}/{$ifndef}. Empty = top of file, always
	// active. `outer` records the enclosing state at push time so $else and
	// $elseif can restore correctly. `taken` records whether any prior branch
	// at this level has been taken (so $else after a taken $if doesn't
	// re-activate). `active` is the current visible state; the tokenizer skips
	// tokens whenever the top frame's active is false.
	struct IfdefFrame {
		bool outer;
		bool taken;
		bool active;
	};
	std::vector<IfdefFrame> ifdef_stack;
	// True when no frame is inactive (or the stack is empty). Tokenizer drops
	// non-directive tokens when this is false.
	bool current_active() const {
		return ifdef_stack.empty() || ifdef_stack.back().active;
	}
	// Interpret the body of a `{$...}` directive (without the leading `$` or
	// trailing `}`). Handles ifdef/ifndef/if/else/elseif/endif/define/undef/
	// include; other directives are consumed and ignored.
	void handle_directive(const std::string& body);
	// True when SYM was passed via `-d` or `{$define SYM}`.
	bool is_defined(const std::string& sym) const;
	// Tiny evaluator for `{$if ...}` conditions: supports `defined(X)`,
	// `not`, `and`, `or`, and parentheses. Numeric compares are not yet
	// implemented; a condition mp doesn't understand evaluates to true and
	// logs a note (so we don't silently drop needed code).
	bool eval_directive_expr(const std::string& expr) const;
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
	/** Designator: value followed by zero-or-more selectors.
	 *  Selectors:
	 *    `.` identifier                 - MemberAccess (binary, RHS = ident)
	 *    `(` [ expr {,expr} ] `)`       - ProcCall (bracketed arg list)
	 *    `[` expression `]`             - Index (bracketed single expr)
	 *    `^`                            - Dereference (postfix, no RHS)
	 *  Sits between parse_power (unary) and parse_value (primary).
	 *  Selectors bind tighter than unary: `not a.b` = `not (a.b)`.
	 *
	 *  Per-step auto-call: before applying `.`, `[`, or `^`, if the
	 *  accumulated result is a bare callable and can be invoked with zero
	 *  arguments, an implicit no-arg call is inserted first. Before `(` no
	 *  auto-call happens because that `(` IS the call. End-of-designator
	 *  auto-call is the caller's decision (value context yes, lvalue no)
	 *  via maybe_auto_call. */
	Node* parse_designator();
	/** If NODE is a bare callable (Callable, OverloadSet, or MemberAccess
	 *  whose member is either) AND at least one candidate can be invoked
	 *  parameterlessly (no formals or all formals defaulted), wrap it in a
	 *  no-arg ProcCall via finalize_call and return that. Otherwise return
	 *  NODE unchanged. */
	Node* maybe_auto_call(Node* n);
	/** True when NODE is a syntactic form assignable to via `:=`: a bare
	 *  StorageSlot, a MemberAccess whose member is a StorageSlot, a
	 *  Dereference, or an Index. Everything else (constants, calls,
	 *  callable references) rejects. */
	bool is_assignable(Node* n);
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
	Frame* parse_aggregate_type_body(Type* owner_class);
	/** Parse a method prototype inside a class/record/object body. Registers
	 *  the Method in BODY under its Pascal name (via register_callable, so
	 *  overload directives interact the same way as for standalone callables). */
	void parse_method_prototype(Frame* body, Type* owner_class, bool is_function);
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
	Parser(UnitRegistry* unit_registry, Emitter* emitter, CompilerOptions* options);
	void push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number);
	void pop_input_file();
	void start();
	Node* parse_program_or_unit();
};
