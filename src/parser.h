#pragma once
#include "ci_less.h"
#include "frame.h"
#include "types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

class Node;

// Number of non-fatal semantic errors reported so far. The driver exits
// nonzero when it is positive; poison values keep compilation going so one
// run reports every semantic error.
extern int tpcc_reported_error_count;
class Mutation;
class Symbol;
class Type;
class RecordType;
class PackedRecordType;
class Unit;
class UnitRegistry;
class Emitter;
struct Parameter;
struct FormattedValue;
class RoutineType;
class Procedure;
class Callable;
class Method;
class Property;
class PropertyAccess;
class Builtin;
struct BuiltinDesc;
class RoutineRef;
class UnitRef;
class ErrorLetContext;
struct CallableRegistration;

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

struct MatchRank {
	enum class ContextualConstruction {
		OrdinaryOrSet,
		Array,
	};
	enum class Tier {
		Exact,
		Equal,
		Convert,
		ConvertNarrowing,
		Generic,
	};
	Tier tier;
	uint64_t distance = 0;
	// Bracket syntax has a historical direct set interpretation. Array
	// construction is contextual and therefore loses to a viable set
	// interpretation before ordinary per-element ranks are compared.
	ContextualConstruction contextual_construction = ContextualConstruction::OrdinaryOrSet;
	// An integer actual retains its carrier's signedness when two otherwise
	// incomparable widening destinations are available. Identity, subtype,
	// and direct assignment-edge direction are compared first.
	bool integer_sign_mismatch = false;
	// Rounded decimal origins prefer the more informative real destination,
	// the reverse of the usual exact-destination specificity direction.
	bool rounded_real_origin = false;
	// True when this actual-to-formal conversion narrows a typed domain, rounds
	// an origin, or reinterprets a based integer origin as a signed bit pattern.
	// Common-domain selection first seeks a proposal which preserves both
	// operand values.
	bool information_losing = false;
};

/** One candidate's treatment of one source argument. Matching never mutates
 * the source CST node. `value` is the candidate-local expression to use if
 * that candidate wins; it retains contextual literal typing or the exact
 * selected declared conversion. */
struct ArgumentMatch {
	MatchRank rank;
	Node* value;
};

struct CallableMatch {
	std::vector<MatchRank> ranks;
	std::vector<Node*> arguments;
	// Dominance compares the formal types at each original argument position.
	// Keeping them parallel to ranks prevents a candidate-wide score from
	// erasing the subtype/assignment relation which made that argument viable.
	std::vector<Type*> formal_types;
};

/** Candidate information retained when an implicit conversion search fails.
 * Matching uses this data only for diagnostics after the surrounding
 * expression has no viable interpretation; individual overload candidates
 * must be allowed to reject an argument without emitting an error. */
struct DeclaredConversionFailure {
	std::vector<Callable*> candidates;
	std::vector<std::pair<Callable*, CallableMatch>> viable;
	std::vector<Callable*> non_dominated;
	bool ambiguous = false;
};

enum class MatchFailure {
	Incompatible,
	NotStorageBacked,
	PackedProjection,
	OrdinalRequired,
	AmbiguousConversion,
};

/** Candidate-shape restrictions applied by the shared callable resolver.
 * Ordinary calls allow each formal coordinate to choose its own destination.
 * Common binary operators require one formal operand type once conversion is
 * needed. IntegerBinary restricts both formal domains to integers while
 * retaining heterogeneous declarations such as integer exponentiation;
 * CommonIntegerBinary additionally requires one common integer type;
 * CommonRealBinary does the same for ordinary binary-real types.
 * Pointer +/- offset is the agreed asymmetric exception. */
enum class OverloadResolutionPolicy {
	Ordinary,
	CommonBinary,
	IntegerBinary,
	CommonIntegerBinary,
	CommonRealBinary,
	CommonBinaryPointerLeft,
	CommonBinaryPointerLeftOrEnumStep,
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

struct ScopeValueLookup {
	Node* binding;
	// True only when this completed binding is allowed to attach overloads
	// from the next lexical/unit ScopeEntry.
	bool opens_parent;
};

/** One environment in the active name-lookup path. `qualifier` optionally
 *  binds values found in that environment to an expression; a resolved value
 *  is then represented as MemberAccess(qualifier, value). Lookup environments
 *  never own declarations. */
struct ScopeEntry {
	const Frame* frame;
	Node* qualifier = nullptr;

	/** True for an environment opened by an object/class/record receiver.
	 *  Unit qualifiers deliberately remain lexical overload environments:
	 *  unlike a class receiver, they have no structural inheritance chain. */
	bool is_receiver_environment() const;
	/** Resolve one complete binding in this environment. Frame lookup already
	 *  includes structural parents; opens_parent says whether lexical lookup
	 *  may continue to attach another overload family. */
	ScopeValueLookup lookup_value(const std::string& name) const;
};

enum class DirectiveSwitchCategory {
	Unsupported,
	Local,
	Module,
	Optimizer,
};

enum class InterfaceModel {
	COM,
	CORBA,
};

/** Local directive state captured at the source token which owns a construct.
 *
 *  The scanner remains preprocessing-like, so directives encountered while
 *  parsing child expressions persist afterward. They must not, however,
 *  retroactively change the already-started parent construct. Keeping the
 *  relevant switches together prevents each new caller-sensitive directive
 *  from growing a parallel set of designator-parser parameters. */
struct LeadingTokenDirectives {
	bool overflow_checks;
	bool io_checks;
};

/** Source-visible compiler-directive state.
 *
 *  The category split is semantic: local and representation settings are
 *  scoped by PUSH/POP, while module and optimizer settings persist. IFOPT and
 *  switch assignment consult this same state instead of maintaining a second
 *  table of answers. The interface model is likewise persistent across
 *  PUSH/POP: it selects the model of each interface declaration encountered
 *  after the directive rather than describing a locally scoped code-generation
 *  option. */
class DirectiveState {
	friend class SavedDirectiveState;
	std::array<bool, 26> local_switches{};
	std::array<bool, 26> module_switches{};
	std::array<bool, 26> optimizer_switches{};
	// Minimum byte size for an enum carrier; range-implied growth is applied
	// at declaration time so values that don't fit still widen the carrier.
	int packenum = 4;
	InterfaceModel interface_model = InterfaceModel::COM;

      public:
	DirectiveState();
	bool switch_enabled(char letter) const;
	bool switch_supported(char letter) const;
	void set_switch(char letter, bool enabled);

	int get_packenum() const {
		return packenum;
	}

	void set_packenum(int value) {
		packenum = value;
	}

	LeadingTokenDirectives leading_token_directives() const {
		return LeadingTokenDirectives{switch_enabled('q'), switch_enabled('i')};
	}

	InterfaceModel get_interface_model() const {
		return interface_model;
	}

	void set_interface_model(InterfaceModel model) {
		interface_model = model;
	}
};

/** Exactly the PUSH/POP-scoped subset of DirectiveState. */
class SavedDirectiveState {
	std::array<bool, 26> local_switches;
	int packenum;

      public:
	explicit SavedDirectiveState(const DirectiveState& state);
	void restore(DirectiveState& state) const;
};

class Parser {
      private:
	FILE* input_file;
	std::string input_file_name;
	int input_file_line_number;
	int input_char;
	int peek_lowlevel();
	int consume_lowlevel();
	std::string consume();
	bool peek_keyword(std::string s);
	bool peek_directive(std::string s);
	void parse_keyword(std::string s);
	bool maybe_parse_keyword(std::string s);
	std::vector<ParserInputFile> input_files; // TODO: stack
	std::vector<ScopeEntry> scopes;           // name-lookup stack
	// Declaration ownership is deliberately independent of name lookup.
	// `uses`, `with`, and implicit Self push lookup entries only; actual
	// declaration constructs push this stack explicitly.
	std::vector<Frame*> declaration_frames;
	// Each active Pascal `type` block owns the implicit forward names created
	// anywhere in its RHS, including inside a record/class body. An aggregate
	// body has its own declaration frame for members, but `field: ^TLater`
	// still refers to a sibling declaration in the surrounding type block.
	// A stack, rather than a boolean, preserves that ownership when an
	// aggregate contains a nested `type` section of its own.
	std::vector<Frame*> type_block_frames;
	// Aggregate bodies parsed inside an open type block cannot finalize
	// overload, property-accessor, override, or C++-carrier semantics when
	// their stored signatures may still contain IncompleteType edges. Record
	// each such declaration frame here; the type block that opened the
	// context drains this list after recursive normalization and before
	// emission.
	// This is phase-local parser work, not source metadata on a Type or Frame.
	std::vector<Frame*> type_block_deferred_aggregates;
	// Declarations of the currently open type-block context. A type section
	// nested in an aggregate body contributes to this list exactly like the
	// aggregate body itself contributes to type_block_deferred_aggregates:
	// the names it may mention include the enclosing block's declarations
	// that have not been read yet, so it cannot finalize at its own close.
	// The section that opened the context finalizes everything once.
	struct PendingTypeDecl {
		enum class Kind {
			Definition,
			ClassForward,
		};
		std::string name;
		std::string cxx;
		Frame* scope = nullptr;
		IncompleteType* lhs_placeholder = nullptr;
		Type* rhs = nullptr;
		Kind kind = Kind::Definition;
		bool alias = false;
	};
	std::vector<PendingTypeDecl> type_block_pending;
	// LHS name whose type expression is currently being parsed. Class parsing
	// uses this to distinguish the one root declaration `System.TObject =
	// class ... end` from every other bare class, which implicitly inherits
	// System.TObject. Saved/restored around each RHS because aggregate bodies
	// may contain nested type blocks.
	std::string current_type_declaration_name;
	// The Callable whose body is currently being parsed, or nullptr outside
	// any routine body. Set in parse_routine_body; used by parse_value's
	// `inherited` branch to walk the parent type's method table and to decide
	// the destructor-auto-chain drop.
	Callable* current_routine = nullptr;
	// Number of enclosing statement loops. break/continue are invalid at zero.
	unsigned loop_depth = 0;
	// Protected Pascal try bodies currently being streamed. A nonlocal
	// Exit/break/continue records this depth in a private C++ transfer so each
	// crossed try can perform its except/finally semantics before the actual
	// control transfer occurs.
	unsigned protected_try_depth = 0;

	// A source loop normally gives break and continue the same protected-try
	// target. A compiler-generated cleanup can instead wrap the loop: break
	// leaves that cleanup region, while continue remains inside it. Keeping
	// both targets is what lets the shared control-transfer emitter implement
	// both shapes without treating the generated loop body as source text.
	struct LoopTryTargets {
		unsigned break_depth;
		unsigned continue_depth;
	};

	std::vector<LoopTryTargets> loop_try_targets;
	// FPC permits bare `raise;` only in the statement sequence belonging
	// directly to an except clause. Entering a nested try clears this even
	// when that try occurs lexically inside an outer handler.
	bool bare_raise_allowed = false;

	// FPC assigns distinct identities to a try's protected body and to its
	// except/finally region. A goto may remain within one identity but may not
	// enter or leave it. Keep only label metadata required to validate forward
	// gotos; ordinary statement emission remains single pass.
	struct LabelExceptionState {
		std::optional<unsigned> definition_block;
		std::vector<std::pair<unsigned, SourceLocation>> goto_blocks;
	};

	struct StatementControlContext {
		unsigned next_exception_block = 0;
		unsigned current_exception_block = 0;
		std::map<std::string, LabelExceptionState> labels;
	};

	std::vector<StatementControlContext> statement_control_contexts;
	// Loop depth on entry to each currently parsed finally body. FPC permits
	// break/continue for a loop wholly inside finally, but forbids control
	// flow from leaving finally. Exit always leaves it.
	std::vector<unsigned> finally_loop_depths;
	UnitRegistry* unit_registry;
	// The unit/program whose declarations this Parser instance is consuming.
	// Sub-parsers have their own Parser and therefore their own current_unit.
	Unit* current_unit = nullptr;
	// May be null. When non-null, emission hooks in the parser call into it
	// as declarations and statements are parsed. Null is used only by
	// sub-parsers loading a `uses`d unit until unit-level emission (.h/.cc
	// per unit) is implemented.
	Emitter* emitter;
	// Shared across the top-level parser and any sub-parsers it spawns.
	CompilerOptions* options;
	DirectiveState directive_state;
	// This stack belongs to the source parser rather than an input file:
	// include files participate in their parent's directive scope.
	std::vector<SavedDirectiveState> saved_directive_states;

	// One frame per open {$ifdef}/{$if}/{$ifndef}/{$ifopt}. Empty = top of
	// file, always active. `outer` records the enclosing state at push time so
	// $else and $elseif can restore correctly. `taken` records whether any
	// prior branch at this level has been taken (so $else after a taken $if
	// doesn't re-activate). `active` is the current visible state; the
	// tokenizer skips tokens whenever the top frame's active is false.
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
	// trailing `}`). Handles ifdef/ifndef/if/ifopt/else/elseif/endif,
	// define/undef, option switches, and include; other directives are
	// consumed and ignored.
	void handle_directive(const std::string& body, SourceLocation directive_location);
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
	Type* lookup_external_type(const char* lib, std::string cxx_name);
	/** OVERFLOW_CHECKS is the directive state captured at the operator's
	 * leading source token. Operand parsing may encounter directives for
	 * nested subtrees, so these builders must never reread mutable scanner
	 * state after receiving their operands. */
	Node* mk_arith(std::string id, Node* a, Node* b, LeadingTokenDirectives directives, bool mutation_step = false);
	Node* mk_compare(std::string id, Node* a, Node* b, LeadingTokenDirectives directives);
	Node* mk_membership(Node* item, Node* set, LeadingTokenDirectives directives);
	Node* mk_unary_same(std::string id, Node* x, LeadingTokenDirectives directives);
	Node* mk_assign(Node* a, Node* b);
	std::optional<ArgumentMatch> match_argument(const Parameter& formal, Node* actual, const BuiltinDesc* builtin, size_t parameter_index, bool allow_declared_conversion = true, MatchFailure* failure = nullptr, DeclaredConversionFailure* conversion_failure = nullptr, bool allow_routine_autocall = true);
	std::optional<CallableMatch> match_callable_arguments(Callable* callable, const std::vector<Node*>& args, bool allow_declared_conversion = true, OverloadResolutionPolicy resolution_policy = OverloadResolutionPolicy::Ordinary);
	std::optional<ArgumentMatch> match_declared_conversion(Node* actual, Type* target, std::string_view operator_identifier, MatchFailure* failure, DeclaredConversionFailure* conversion_failure, MatchRank::Tier conversion_tier = MatchRank::Tier::Convert);
	bool has_direct_assignment_edge(Type* source, Type* target);
	Node* match_explicit_conversion(Node* actual, Type* target, bool implicit_fallback);
	Node* make_implicit_cast(Node* value, Type* target);
	Node* cast(Node* a, Type* target_ty);
	Node* cast_for_destination(Node* a, Type* target_ty);
	Node* cast_impl(Node* a, Type* target_ty);
	Node* resolve_routine_reference(RoutineRef* reference, RoutineType* target_ty);
	Node* try_resolve_routine_reference(RoutineRef* reference, RoutineType* target_ty, bool* ambiguous);
	Node* try_resolve_routine_code_reference(RoutineRef* reference);
	Node* resolve_routine_code_reference(RoutineRef* reference);
	Node* routine_code_pointer(Node* value, RoutineType* contextual_type = nullptr);
	Node* try_auto_call_routine_value(Node* value, LeadingTokenDirectives directives);
	uint64_t next_subrange_type_number = 0;
	std::string next_subrange_cxx_name();
	Type* parse_subrange_type(Node* lower_bound, Node* upper_bound);
	ClassType* lookup_implicit_tobject_superclass();
	Node* active_function_result_lvalue(Callable* c) const;
	Node* active_function_result_lvalue_from_binding(Node* binding) const;

      protected:
	std::string input_token;
	// Identifiers retain their source spelling separately from input_token.
	// Pascal lookup continues using the normalized token; observable features
	// such as enum Str use the spelling that appeared in the declaration.
	std::string input_token_spelling;
	void parse_block_body();
	void parse_unit_statement_sequence(bool stop_at_finalization);
	void maybe_parse_const_block();
	void maybe_parse_type_block(bool delphi_auto_end);
	void maybe_parse_var_block();
	void parse_const_block(Type* aggregate_owner = nullptr);
	void parse_label_block();
	void parse_type_block(bool delphi_auto_end);
	void parse_var_block();
	StorageSlot* resolve_absolute_target(const std::string& target_name, Type* declared_type);
	/** Parse declarations into the current declaration frame. */
	void parse_decl_blocks(bool is_decl_only);
	void validate_class_forwards(Frame* frame);
	void parse_block();
	void parse_semicolon();
	void maybe_parse_statement();
	Node* parse_mutation_statement(std::string spelling, SourceLocation call_location, LeadingTokenDirectives directives);
	std::optional<std::string> maybe_parse_identifier();
	std::string parse_identifier();
	Node* maybe_parse_numeral();
	Node* parse_numeral();
	Node* parse_bracket_literal();
	Node* parse_storage_initializer(Type* ty);
	Node* resolve_lvalue(std::string name);
	std::optional<Binding> maybe_resolve_type_or_value(std::string name);
	Node* maybe_resolve_value(std::string name);
	Node* resolve_value(std::string name);
	Type* maybe_resolve_type(std::string name);
	Type* resolve_type(std::string name, bool allow_forward);
	bool maybe_parse_directive(std::string directive);
	void parse_directive(std::string s);
	void parse_operator(std::string s);
	Node* parse_value(LeadingTokenDirectives* leading_directives);
	Node* parse_explicit_conversion_to(Type* target_ty);
	Node* parse_value_from_identifier(std::string id, LeadingTokenDirectives identifier_directives, LeadingTokenDirectives* leading_directives);
	/** Parse an identifier-starting argument in a grammar which accepts either
	 *  a type or a value.  The returned Binding contains the parsed Type* or
	 *  Node*, not merely the result of name lookup. */
	std::optional<Binding> maybe_parse_named_type_or_expression();
	Node* parse_new_or_dispose(bool is_new);
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
	Node* parse_designator(LeadingTokenDirectives* leading_directives = nullptr);
	Node* parse_designator_tail(Node* result, LeadingTokenDirectives& leading_directives);
	Node* parse_member_selection(Node* base, LeadingTokenDirectives* leading_directives);
	/** Parse `LHS.RHS` in a type context. Resolves LHS through the same
	 *  kind-dispatch as the value-context member path, then returns either a
	 *  nested type or the declared type of a value member. Raises if LHS has
	 *  no member environment or RHS is absent. */
	Type* parse_qualified_type_member(std::string lhs_name);
	/** Apply ordinary postfix type projections. Unlike a value designator,
	 *  these operations inspect declarations and produce another Type*:
	 *  `P^` is the pointed-to type, `A[constant]` is an array's element type,
	 *  and `R.Field` is the declared member type. The syntax is available
	 *  everywhere a type expression is accepted; SizeOf has no private copy
	 *  of this grammar. */
	Type* parse_type_projection_tail(Type* type);
	Type* parse_type_member_projection(Type* type);
	/** Resolve NAME in RECEIVER's ordinary structural member environment and
	 * bind the result to RECEIVER. This is the non-token-consuming half of
	 * parse_member_selection, used by compiler-defined protocols which must
	 * obey the same inheritance, overload, property, and receiver rules as a
	 * source `Receiver.Name` expression. */
	Node* maybe_bind_member(Node* receiver, const std::string& name);

	struct CustomForInResolution {
		Node* get_enumerator;
		Node* move_next;
		Node* current_assignment;
		Node* cleanup;
		bool nullable;
		// Presence distinguishes a malformed declared GetEnumerator protocol
		// from absence of that protocol; only absence permits native fallback.
		bool poisoned;
	};

	/** Try the ordinary member-based for-in protocol. Null means the
	 * collection has no GetEnumerator member and builtin iteration may be
	 * considered; a malformed member protocol is diagnosed here. */
	std::optional<CustomForInResolution> maybe_resolve_custom_for_in(Node* collection, Node* control);
	/** Force NODE to produce an ordinary expression value. Bare declarations
	 *  and runtime routine values are called with no explicit arguments;
	 *  defaults are materialized by finalize_call. Routine-valued actuals are
	 *  deliberately not sent here until their destination is known, because
	 *  a routine-typed assignment or formal consumes the routine value itself.
	 */
	Node* maybe_auto_call(Node* n, LeadingTokenDirectives directives);
	/** True when NODE is a syntactic form assignable to via `:=`: a bare
	 *  StorageSlot, a MemberAccess whose member is a StorageSlot, a
	 *  Dereference, or an Index. Everything else (constants, calls,
	 *  callable references) rejects. */
	bool is_assignable(Node* n);
	/** True when NODE's storage path crosses a packed-record field.  Such a
	 *  field is a read/write projection implemented by copying bytes, not a
	 *  C++ lvalue to which a reference or pointer can bind.  Dereference ends
	 *  the path: a pointer read from packed storage may still designate an
	 *  ordinary object elsewhere. */
	bool contains_packed_projection(Node* n);
	/** Packed lowering can write a direct field or one indexed component of
	 *  that field when the packed carrier is a stable assignable place.
	 *  Projections crossing another packed field still need nested copyback. */
	bool is_supported_packed_assignment(Node* n);
	/** Enforce the complete place boundary shared by `:=` and read/modify/write
	 *  mutation before either construct builds its store. */
	bool validate_writable_destination(Node* target, SourceLocation error_location, std::string not_assignable_message);
	Node* parse_expression_after_identifier(std::string id, LeadingTokenDirectives identifier_directives);
	Node* parse_comparison();
	Node* parse_comparison_tail(Node* result);
	Node* parse_power();
	Node* parse_power_tail(Node* result, LeadingTokenDirectives leading_directives);
	Node* parse_product();
	Node* parse_product_tail(Node* result);
	Node* parse_subrange_bound_expression();
	Node* parse_subrange_bound_expression_after_identifier(std::string id, LeadingTokenDirectives identifier_directives);
	Node* parse_sum();
	Node* parse_sum_tail(Node* result);
	Type* parse_array_type(bool direct_formal = false);
	Type* parse_object_type();
	Type* parse_record_type();
	Type* parse_procedure_type();
	Type* parse_function_type();
	Type* parse_operator_type();
	Type* parse_class_type(ClassType* completing_forward = nullptr, bool allow_forward_declaration = false);
	Type* parse_interface_type();
	Type* parse_enum_type();
	Type* parse_type_expression(bool allow_forward);
	Type* parse_type_expression_from_identifier(std::string id, const LeadingTokenDirectives& identifier_directives, bool allow_forward);
	Type* parse_formal_type_expression();
	Node* parse_expression();
	/** Shared grammar used by Write/WriteLn and Str:
	 *
	 *      value [ : width [ : precision ] ]
	 *
	 * This parses syntax and gives width/precision their SizeInt context.
	 * The caller remains responsible for contextualizing and validating the
	 * value because Write has no formal parameter while Str performs ordinary
	 * overload resolution. */
	FormattedValue parse_formatted_value();
	void parse_statement();
	Frame* parse_aggregate_type_body(Type* owner_class);
	Node* parse_property_accessor_reference(Frame* body);
	void parse_property_declaration(Frame* body, Type* owner_type);
	void validate_property_declaration(Property* property);
	void validate_method_ancestor_semantics(Method* method, Frame* owner_body);
	void validate_aggregate_declaration_semantics(Frame* owner_body);
	Property* default_property_for_type(Type* ty);
	PropertyAccess* apply_property(Node* receiver, Property* property, std::vector<Node*> indexes);
	bool property_read_is_place(PropertyAccess* access);
	bool is_referenceable(Node* n);
	bool is_static_storage_place(Node* n);
	bool is_symbolic_static_initializer(Node* n);
	VariantPart* parse_record_variant(Type* owner, Frame* body);
	/** Parse a method prototype inside a class/record/object body. It only
	 *  collects the Method in BODY. The completed aggregate validates its
	 *  local overload family and ancestor relationship after any enclosing
	 *  type block has recursively normalized every signature edge. */
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

	/** Parse a unit from its name through the complete interface, leaving this
	 *  parser suspended at `implementation`. The published Unit frame can then
	 *  satisfy dependants before this unit's implementation is resumed. */
	Unit* parse_unit_interface_body();
	/** Resume a parser suspended by parse_unit_interface_body and finish that
	 *  unit. Interface and implementation dependencies are completed only
	 *  after UNIT has entered ImplementationInProgress, which terminates legal
	 *  implementation-only cycles. */
	void parse_unit_implementation_body(Unit* unit);
	/** Complete a recursively loaded InterfaceDone unit. Done units and the
	 *  active side of an implementation cycle require no action. */
	void complete_unit(Unit* unit);
	/** Parse a comma-separated `uses A, B, C` list (the `uses` keyword must
	 *  have been consumed by the caller). Loads each named unit if necessary
	 *  and returns the Units in source order. This does not mutate `scopes`:
	 *  uses makes another unit available for lookup, but does not make that
	 *  unit the owner of declarations which follow the uses clause. The caller
	 *  opens each returned unit frame below its own declaration frame. */
	std::vector<Unit*> parse_uses_clause(bool in_interface, std::string current_name);
	/** Implicitly load the `system` unit. USER_NAME is the unit/program being
	 *  parsed; if it case-insensitively equals "system" we're parsing system
	 *  itself and must not recurse. Like parse_uses_clause, this returns a
	 *  lookup source without installing it in `scopes`. */
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
		// A qualifier which Pascal evaluates even though the selected callable
		// has no receiver ABI. make_call lowers it through EvaluateThen.
		Node* qualifier_effect = nullptr;
		// Set when overload resolution reported a non-fatal error and returned
		// a poison result instead of selecting a callee. Callers must return
		// their own poison value rather than read callee.
		bool poisoned = false;
	};

	/** Given a resolved target (Callable, OverloadSet, MemberAccess-wrapping
	 *  either of those, or a Builtin) and parsed args, peel any MemberAccess
	 *  to extract a receiver, run overload ranking if the target is a set,
	 *  materialize defaults, and insert Cast coercions where needed. Errors
	 *  on no-match, ambiguous overload, or bad args. */
	FinalizedCall finalize_call(Node* target, std::vector<Node*>& args, std::string name_for_error, SourceLocation error_location, Type* expected_return_type = nullptr, OverloadResolutionPolicy resolution_policy = OverloadResolutionPolicy::Ordinary);
	/** Form the semantic application after overload selection. Constructor
	 *  selection through a class reference becomes Construct; every other
	 *  selected callable remains ProcCall. OVERFLOW_CHECKS was captured at
	 *  the call construct's leading designator token, before arguments were
	 *  parsed; only compiler-owned direct operations consume it. */
	Node* make_call(FinalizedCall finalized, std::vector<Node*> args, LeadingTokenDirectives directives);
	bool maybe_parse_plus();
	bool maybe_parse_minus();
	bool maybe_parse_star();
	bool maybe_parse_slash();
	bool maybe_parse_circumflex();
	bool maybe_parse_at();
	bool maybe_parse_less_less();
	bool maybe_parse_greater_greater();
	bool maybe_parse_star_star();
	bool maybe_parse_symdiff();
	bool maybe_parse_equal();
	bool maybe_parse_less_greater();
	bool maybe_parse_less();
	bool maybe_parse_greater();
	bool maybe_parse_less_equal();
	bool maybe_parse_greater_equal();
	bool maybe_parse_period();
	void parse_period();
	bool maybe_parse_period_period();
	void parse_period_period();
	void push_statement_control_context();
	void pop_statement_control_context();
	unsigned current_exception_block() const;
	unsigned enter_exception_block();
	void restore_exception_block(unsigned block);
	void record_label_definition(const std::string& name, SourceLocation location);
	void record_goto(const std::string& name, SourceLocation location);
	/** Add/remove a lookup environment, optionally selected through an
	 *  expression. These never change declaration ownership. */
	void push_scope(const Frame* scope, Node* qualifier = nullptr);
	void pop_scope();
	/** Bind a lookup result to its selecting expression and enforce whether
	 *  that expression denotes an instance/class receiver or only a static
	 *  type-member environment. */
	Node* bind_lookup_result(Node* qualifier, Node* binding);
	/** Enter/leave the frame which owns declarations currently being parsed. */
	void push_declaration_frame(Frame* frame);
	void pop_declaration_frame();
	Frame* current_declaration_frame() const;
	/** Return the source unit which directly owns declarations entering FRAME.
	 * Programs, routine locals, and aggregate members have no unit namespace
	 * owner here. */
	Unit* declaration_unit(Frame* frame) const;
	void maybe_parse_proc_attributes();
	RoutineType* parse_routine_signature(bool is_class, bool is_function, bool allow_of_object, RoutineKind kind, Type* owner = nullptr);
	void parse_routine_body(Callable* target, Frame* owner_frame);
	void parse_class_lifecycle_prototype(ClassType* owner_class, RoutineKind kind);
	Procedure* match_or_create_procedure(const std::string& pas_name, const std::vector<std::string>& frame_names, const std::string& cxx_name, RoutineType* sig, bool had_paren, bool has_overload, bool short_form_implementation);
	/** Parse `procedure NAME(...);` (is_function=false) or
	 *  `function NAME(...): T;` (is_function=true). Attribute list (`overload;`)
	 *  is consumed after the terminating `;`. If followed by a body, parses
	 *  it into a fresh body_frame; if followed by `forward;`, leaves body
	 *  null. Registers the resulting Procedure in the current scope and emits
	 *  the signature/body when an emitter is attached. */
	void parse_procedure_or_function(bool is_class, bool is_function, bool is_decl_only);
	std::vector<Parameter> parse_proc_formal_parameters();

	[[noreturn]] void emit_parse_error_at(SourceLocation loc, std::string message);
	/** Finish a parser diagnostic with its enclosing source context and the
	 * one diagnostic graph shared by the primary message, context references,
	 * and exactly one trailing `where` block. */
	[[noreturn]] void emit_parse_error_at(SourceLocation loc, std::string message, ErrorLetContext& ctx);
	[[noreturn]] void emit_fatal_error_at(SourceLocation loc, std::string message);
	std::string complete_diagnostic_message(std::string message, ErrorLetContext& ctx) const;
	std::string enclosing_diagnostic_references(ErrorLetContext& ctx) const;
	[[noreturn]] void raise_parse_error(std::string message);
	[[noreturn]] Type* raise_type_parse_error(std::string message);
	// A report_* call rejects an otherwise structurally complete declaration.
	// A raise_* call additionally supplies the sentinel required by the
	// enclosing expression/type computation; [[nodiscard]] prevents that
	// recovery state from being accidentally discarded again.
	void report_type_mismatch(std::string message, Type* expected, Type* got);
	void report_type_mismatch_at(SourceLocation location, std::string message, Type* expected, Type* got);
	void report_type_kind_mismatch(std::string message, const char* expected_kind, Type* got);
	void report_type_kind_mismatch_at(SourceLocation location, std::string message, const char* expected_kind, Type* got);
	void report_type_error(std::string message, Type* relevant);
	void report_type_error_at(SourceLocation location, std::string message, Type* relevant);
	[[nodiscard]] Type* raise_type_mismatch(std::string message, Type* expected, Type* got);
	[[nodiscard]] Type* raise_type_mismatch_at(SourceLocation location, std::string message, Type* expected, Type* got);
	[[nodiscard]] Type* raise_type_kind_mismatch(std::string message, const char* expected_kind, Type* got);
	[[nodiscard]] Type* raise_type_kind_mismatch_at(SourceLocation location, std::string message, const char* expected_kind, Type* got);
	[[nodiscard]] Type* raise_type_error(std::string message, Type* relevant);
	[[nodiscard]] Type* raise_type_error_at(SourceLocation location, std::string message, Type* relevant);
	// The value-family diagnostics report and return an ErrorValue poison
	// node so parsing continues; the run fails once at the end through
	// tpcc_reported_error_count.
	void report_value_error(std::string message, Node* relevant);
	void report_value_error_at(SourceLocation location, std::string message, Node* relevant);
	void report_values_error(std::string message, const std::vector<std::pair<std::string, Node*>>& relevant);
	[[nodiscard]] Node* raise_value_error(std::string message, Node* relevant);
	[[nodiscard]] Node* raise_value_error_at(SourceLocation location, std::string message, Node* relevant);
	[[nodiscard]] Node* raise_values_error(std::string message, const std::vector<std::pair<std::string, Node*>>& relevant);
	[[nodiscard]] Node* raise_routine_reference_error(std::string message, RoutineRef* reference, Type* destination_type);
	[[nodiscard]] Node* raise_no_matching_overload(std::string name, Node* receiver, const std::vector<Node*>& args);
	void report_overload_resolution_error(SourceLocation error_location, std::string name, Node* receiver, const std::vector<Node*>& args, Type* expected_return_type, const std::vector<Callable*>& candidates, const std::vector<std::pair<Callable*, CallableMatch>>& viable, const std::vector<Callable*>& non_dominated, bool ambiguous, std::string failure_description = {});
	[[nodiscard]] Node* raise_overload_resolution_error(SourceLocation error_location, std::string name, Node* receiver, const std::vector<Node*>& args, Type* expected_return_type, const std::vector<Callable*>& candidates, const std::vector<std::pair<Callable*, CallableMatch>>& viable, const std::vector<Callable*>& non_dominated, bool ambiguous, std::string failure_description = {});
	void report_cxx_carrier_collision(const std::string& name, Callable* incoming, const CallableRegistration& registration);
	void report_callable_registration_error(const std::string& name, Callable* incoming, const CallableRegistration& registration);

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
