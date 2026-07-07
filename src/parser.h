#pragma once
#include <cstdio>
#include <cstddef>
#include <memory>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <optional>
#include "ci_less.h"
#include "types.h"

class Node;
class Symbol;
class Type;
class RecordType;
class Unit;
class UnitRegistry;
class Emitter;
struct Parameter;
class RoutineType;
class Procedure;
class Callable;
class Builtin;

/** Shared compiler-wide options set from the command line and consulted by
 *  the tokenizer's directive handling and by unit/include file lookup. One
 *  instance lives in main; every Parser (main plus sub-parsers spawned by
 *  `uses` loading) reads from it and can mutate `defines` via `{$define}` /
 *  `{$undef}` seen in source. */
struct CompilerOptions {
	// Symbol -> value ("" for boolean defines). Populated from -d<sym>[:=<val>]
	// and from {$define} directives. Case-insensitive per Pascal identifier
	// rules -- `defined(Unix)` matches `-dUNIX`.
	std::map<std::string, std::string, CILess> defines;
	// -Fu<path> entries. load_or_get_unit walks these when a `uses` name
	// isn't in the current-input dir or CWD.
	std::vector<std::string> unit_search_paths;
	// -Fi<path> entries. Used by {$i <file>} include-file resolution.
	std::vector<std::string> include_search_paths;
	// Directory where per-unit .h/.cc outputs land. Set by main from the
	// program's output path dirname. Empty means CWD.
	std::string output_dir;
	// Output path for the program's .cc when the top-level source is a
	// `program`. Set by main from -o or derived from the source path.
	std::string program_output_path;
};

class ParserInputFile {
public:
	FILE* input_file;
	std::string input_file_name;
	int input_file_line_number;
	// Backing bytes when input_file is an fmemopen over an in-memory buffer
	// this entry owns (macro expansion for `{$I %DATE%}` etc.); null for
	// real files. std::unique_ptr<char[]> is the array specialization
	// ([unique.ptr.runtime], C++11 20.7.1.3): its destructor calls
	// delete[] to match make_unique<char[]>(n) which uses new char[n], and
	// moving/reassigning it just transfers the raw pointer, so the heap
	// bytes stay at a fixed address for fmemopen's lifetime regardless of
	// what happens to the surrounding container.
	std::unique_ptr<char[]> owned_buffer;
	size_t owned_buffer_len;
};

class Frame;

/** One entry on the parser's scope stack. `frame` is the declaration frame
 *  (locals, unit interface, record body, etc.). `unwrap_via` is null for
 *  every kind of scope except a `with` push: when non-null, a resolve hit in
 *  this frame is wrapped as `MemberAccess(unwrap_via, hit)` before being
 *  returned to the caller, so `field` inside `with rec do ...` produces
 *  `rec.field` at emit time.
 *
 *  `saved_type_block` records the value of `Parser::current_type_block` at
 *  push time so pop_scope can restore it. Each Frame interleaves TWO name
 *  namespaces (types and values -- see Frame's `type_items` and
 *  `value_items`); `current_type_block` is the Frame new decls land in
 *  right now, i.e. whichever declaration Frame was most recently pushed.
 *  push_scope updates it; push_with_scope deliberately does NOT -- a `with`
 *  scope is an alias overlay for value lookup only, not a declaration
 *  site, so new type/var decls inside a `with` body still belong to the
 *  enclosing declaration Frame and must register there. */
struct ScopeEntry {
	const Frame* frame;
	Node* unwrap_via;
	Frame* saved_type_block;
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
	bool peek_directive(std::string s);
	void parse_keyword(std::string s);
	bool maybe_parse_keyword(std::string s);
	std::vector<ParserInputFile> input_files; // TODO: stack
	std::vector<ScopeEntry> scopes; // TODO: stack
	// The type-block scope currently being parsed, or nullptr. Used as the
	// registration site for implicit forward references (`^TFoo` before TFoo
	// is declared); those must land in the enclosing type block's scope, not
	// in whatever inner scope (record/class body) happens to be on top.
	Frame* current_type_block = nullptr;
	// The Callable whose body is currently being parsed, or nullptr outside
	// any routine body. Set in parse_routine_body; used by parse_value's
	// `inherited` branch to walk the parent type's method table and to decide
	// the destructor-auto-chain drop.
	Callable* current_routine = nullptr;
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
	// Expand a `%NAME%` argument in `{$I %NAME%}` to the source text spliced
	// at that position (a Pascal string literal for %DATE%). Only %DATE% is
	// handled; any other name raises a parse error.
	std::string expand_include_macro(const std::string& rest);
	// True when SYM was passed via `-d` or `{$define SYM}`.
	bool is_defined(const std::string& sym) const;
	// Tiny evaluator for `{$if ...}` conditions: supports `defined(X)`,
	// `not`, `and`, `or`, and parentheses. Numeric compares are not yet
	// implemented; a condition mp doesn't understand evaluates to true and
	// logs a note (so we don't silently drop needed code).
	bool eval_directive_expr(const std::string& expr);
	Builtin* lookup_external_value(const char* lib, std::string cxx_name);
	Type* lookup_external_type(const char* lib, std::string cxx_name);
	Node* mk_arith(std::string id, Node* a, Node* b);
	Node* mk_compare(std::string id, Node* a, Node* b);
	Node* mk_unary_same(std::string id, Node* x);
	Node* mk_assign(Node* a, Node* b);
	Node* cast(Node* a, Type* target_ty);

protected:
	std::string input_token;
	void parse_block_body();
	void maybe_parse_const_block();
	void maybe_parse_type_block(bool delphi_auto_end);
	void maybe_parse_var_block();
	void parse_const_block();
	void parse_type_block(bool delphi_auto_end);
	void parse_var_block();
	/** Parse a sequence of top-of-block declarations in any order (Pascal
	 *  allows `type`, `const`, `var` blocks and `procedure`/`function` decls
	 *  interleaved freely). Returns the count of scopes pushed so the caller
	 *  can pop that many after the body. */
	size_t parse_decl_blocks();
	void parse_block();
	void parse_semicolon();
	void maybe_parse_statement();
	std::optional<std::string> maybe_parse_identifier();
	std::string parse_identifier();
	Node* maybe_parse_numeral();
	Node* parse_numeral();
	Node* resolve_lvalue(std::string name);
	Node* resolve_value(std::string name);
	Type* resolve_type(std::string name, bool allow_forward);
    bool maybe_parse_directive(std::string directive);
	void parse_directive(std::string s);
	void parse_operator(std::string s);
	Node* parse_value();
	// Parse `inherited Name[(args)]` or anonymous `inherited;`. Returns an
	// InheritedCall node. The enclosing routine must be a Method on a
	// composite type with a parent (else: parse error).
	Node* parse_inherited();
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
	Type* parse_procedure_type();
	Type* parse_function_type();
	Type* parse_operator_type();
	Type* parse_class_type();
	Type* parse_interface_type();
	Type* parse_enum_type();
	Type* parse_type_expression(bool allow_forward);
	Node* parse_expression();
	void parse_statement();
	Frame* parse_aggregate_type_body(Type* owner_class);
	void parse_record_variant(RecordType* rt, Frame* body);
	/** Parse a method prototype inside a class/record/object body. Registers
	 *  the Method in BODY under its Pascal name (via register_callable, so
	 *  overload directives interact the same way as for standalone callables). */
	void parse_method_prototype(Frame* body, Type* owner_class, bool is_function, bool is_destructor, bool is_constructor, bool is_class);
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
	bool maybe_parse_colon();
	bool maybe_parse_colon_equals();
	std::string parse_string_literal();

	void parse_unit_body();
	/** Parse a comma-separated `uses A, B, C` list (the `uses` keyword must
	 *  have been consumed by the caller). Loads each named unit if not already
	 *  in the registry, checks the phase rules (interface-position use of an
	 *  InterfaceInProgress unit is a circular-dep error), and pushes each
	 *  loaded unit's interface_frame onto the scope stack. Returns the loaded
	 *  Units (in source order) so the caller can both pop the same number of
	 *  scopes at section end and emit `#include "<name>.h"` for each. */
	std::vector<Unit*> parse_uses_clause(bool in_interface, std::string current_name);
	/** Implicitly load and push the `system` unit's interface frame at the
	 *  front of the uses list so built-in identifiers (Boolean, True, False,
	 *  etc.) resolve in every program and unit. USER_NAME is the unit/program
	 *  being parsed; if it case-insensitively equals "system" we're parsing
	 *  system itself and must not recurse into another load. Returns the
	 *  loaded Unit (nullptr if user_name == "system"). The push happens BEFORE
	 *  any user-written `uses` clause so user-named units can shadow system. */
	Unit* implicit_uses(std::string user_name);
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
	FinalizedCall finalize_call(Node* target, std::vector<Node*>& args, std::string name_for_error, SourceLocation error_location);
	bool maybe_parse_plus();
	bool maybe_parse_minus();
	bool maybe_parse_star();
	bool maybe_parse_slash();
	bool maybe_parse_circumflex();
	bool maybe_parse_at();
	bool maybe_parse_less_less();
	bool maybe_parse_greater_greater();
	bool maybe_parse_star_star();
	bool maybe_parse_ampersand();
	bool maybe_parse_pipe();
	bool maybe_parse_symdiff();
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
	void maybe_parse_proc_attributes();
	RoutineType* parse_routine_signature(bool is_class, bool is_function, bool allow_of_object, RoutineKind kind, Type* owner = nullptr);
	void parse_routine_body(Callable* target, Frame* owner_frame);
	Procedure* match_or_create_procedure(const std::string& pas_name, RoutineType* sig, bool had_paren, bool has_overload);
	/** Parse `procedure NAME(...);` (is_function=false) or
	 *  `function NAME(...): T;` (is_function=true). Attribute list (`overload;`)
	 *  is consumed after the terminating `;`. If followed by a body, parses
	 *  it into a fresh body_frame; if followed by `forward;`, leaves body
	 *  null. Registers the resulting Procedure in the current scope and emits
	 *  the signature/body when an emitter is attached. */
	void parse_procedure_or_function(bool is_class, bool is_function);
	std::vector<Parameter> parse_proc_formal_parameters();

	[[noreturn]] void emit_parse_error_at(SourceLocation loc, std::string message);
	[[noreturn]] void raise_parse_error(std::string message);
	[[noreturn]] Type* raise_type_parse_error(std::string message);
	Type* raise_type_mismatch(std::string message, Type* expected, Type* got);
	Type* raise_type_kind_mismatch(std::string message, const char* expected_kind, Type* got);
	[[noreturn]] void raise_no_matching_overload(std::string name, Node* receiver, const std::vector<Node*>& args);
	[[noreturn]] void raise_overload_resolution_error(SourceLocation error_location,
	                                                  std::string name,
	                                                  Node* receiver,
	                                                  const std::vector<Node*>& args,
	                                                  const std::vector<Callable*>& candidates,
	                                                  const std::vector<std::pair<Callable*, std::vector<int>>>& viable,
	                                                  const std::vector<Callable*>& non_dominated,
	                                                  bool ambiguous);

public:
	Parser(UnitRegistry* unit_registry, Emitter* emitter, CompilerOptions* options);
	SourceLocation current_location() const;
	// Push a source onto the input stack and make it current. Reads one
	// byte from `input_file` into `input_char` so the tokenizer sees the
	// new source's first character on its next consume_lowlevel call. If a
	// parent source was active, its pending `input_char` is pushed back
	// onto its own FILE* via ungetc so it resumes exactly on that byte
	// after pop_input_file.
	void push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number);
	// Same as push_input_file, but transfers ownership of a heap buffer
	// that backs `input_file` (an fmemopen result). The buffer stays alive
	// with the input entry and is released when the entry is popped.
	void push_input_file_and_buffer(FILE* input_file, std::string input_file_name, int input_file_line_number, std::unique_ptr<char[]> buffer, size_t buffer_len);
	void pop_input_file();
	void start();
	void parse_program_or_unit();
};
