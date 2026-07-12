#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

class Type;
class Frame;
class RoutineType;
class Callable;
class ErrorLetContext;
struct BuiltinDesc;
struct ConstEvalContext;
struct ConstEvalResult;

enum class TypeBoundKind { Low, High };

class Node {
public:
	virtual const char* diagnostic_kind() const;
	virtual void collect_diagnostic_edges(ErrorLetContext* ctx) const;
	virtual void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	virtual void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	virtual ConstEvalResult const_eval(ConstEvalContext& ctx) const;
	// Result type of the value this node produces. Filled by the Parser at
	// construction time; readers (emit, checker, evaluator) treat it as the
	// single source of truth. Null on statement nodes (Block, Assign,
	// Return) -- those don't have a value type.
	Type* ty = nullptr;
	virtual ~Node() = default;
	std::string str() const;
};

class Block: public Node {
public:
	std::vector<Node*> statements;
	void add(Node* stmt);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Symbol: public Node {
private:
	std::string text;
public:
	Symbol(std::string text);
	std::string str() const;
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class UnaryOperation: public Node {
public:
       Node* a;
       UnaryOperation(Node* a);
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class BinaryOperation: public Node {
public:
       Node* a;
       Node* b;
       BinaryOperation(Node* a, Node* b);
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class ProcCall: public Node {
public:
	// null for standalone calls; the receiver expression for calls whose
	// callable carries a Self (methods, and later `procedure of object` runtime
	// values).
	Node* receiver;
	// Any expression that yields a callable at compile time or at runtime.
	// Compile-time: a Callable*. Runtime (future): a procedural value.
	Node* callee;
	std::vector<Node*> args;
	ProcCall(Node* receiver, Node* callee, std::vector<Node*> args);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** One Pascal Write/WriteLn invocation. These routines have compiler grammar,
 * not an ordinary RoutineType signature: the optional first Text argument and
 * every value's `:width[:precision]` qualifiers must remain grouped. */
class WriteCall: public Node {
public:
	struct Item {
		Node* value;
		Node* width;
		Node* precision;
	};

	bool newline;
	Node* file;
	std::vector<Item> items;

	WriteCall(bool newline, Node* file, std::vector<Item> items);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** `inherited Name(args)`. Calls the parent-type method the parser resolved
 *  at parse time (single-pass compiler -- emit doesn't re-resolve).
 *
 *  No `receiver` field, unlike ProcCall. Pascal `inherited X` carries
 *  implicit Self, but C++ emits this as a qualified-id `ParentClass::X(args)`
 *  -- not member-access `this->X(args)` or `receiver->X(args)`. Qualified-id
 *  member-call syntax in C++ implicitly uses `this`, so there's no receiver
 *  expression to spell. The parent class name is recovered at emit time from
 *  `resolved` (a Method*) -> `owner_class` -> `owner_cxx_name(...)`.
 *
 *  `dropped` is set when the enclosing routine is a destructor AND `resolved`
 *  is a destructor -- C++ destructors auto-chain (base destructors run
 *  automatically after derived body), so emit produces nothing. */
class InheritedCall: public Node {
public:
	Callable* resolved = nullptr;
	std::vector<Node*> args;
	bool dropped = false;
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Dereference: public UnaryOperation {
public:
	Dereference(Node* a);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Assign: public BinaryOperation {
public:
	Assign(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

enum ShortCircuitOperationKind {
	AND,
	OR,
};

class ShortCircuitOperation: public BinaryOperation {
public:
	enum ShortCircuitOperationKind kind;
public:
	ShortCircuitOperation(enum ShortCircuitOperationKind kind, Node* a, Node* b);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** container.member. `a` is the container (usually a StorageSlot for the
 *  record variable or with-alias) and `b` is the member (usually the field's
 *  StorageSlot). Produced by resolve_value/resolve_lvalue when a name hits
 *  inside a with-pushed scope; will also carry explicit `rec.field` grammar
 *  when that lands. */
class MemberAccess: public BinaryOperation {
public:
	MemberAccess(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

/** A Pascal property declaration. Accessors are ordinary semantic symbols:
 *  a StorageSlot for a field, a Callable for a getter/setter, or a Builtin
 *  for a compiler-synthesized RTL accessor. */
class Property: public Node {
public:
	std::string pas_name;
	std::vector<Type*> index_types;
	Node* read_accessor;
	Node* write_accessor;
	bool is_default;

	Property(std::string pas_name,
	         Type* property_type,
	         std::vector<Type*> index_types,
	         Node* read_accessor,
	         Node* write_accessor,
	         bool is_default);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Application of a named or omitted-name property. Once constructed there
 *  is no semantic distinction between `object.Items[i]` and `object[i]`. */
class PropertyAccess: public Node {
public:
	Node* receiver;
	Property* property;
	std::vector<Node*> indexes;

	PropertyAccess(Node* receiver, Property* property, std::vector<Node*> indexes);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** array[index]. `a` is the array value; `b` is the index expression.
 *  Result type (Node::ty) is the array's element type, set by the parser at
 *  construction. */
class Index: public BinaryOperation {
public:
	Index(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

class Return: public UnaryOperation {
public:
	// `a == nullptr` means a void/procedure return.
	Return(Node* a);
	const char* diagnostic_kind() const override;
};

/** Compiler-inserted implicit type conversion. Distinct from Coerce (which
 *  represents the user-written `x as T`). Target type held on Node::ty; the
 *  wrapped value is `a`. Emits as `static_cast<target>(a)`. */
class Cast: public UnaryOperation {
public:
	Cast(Node* value, Type* target);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct StorageSlot: public Node {
	std::string cxx_name;
	StorageSlot(std::string cxx_name, Type* ty);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct EnumMemberRef: public Node {
	std::string cxx_name;
	int64_t value;
	EnumMemberRef(std::string cxx_name, int64_t value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Integer: public Node {
public:
	// Signed magnitude. Pascal integer constants are not typed until context fixes
	// them; this keeps -9223372036854775808 representable without pretending it
	// fits in int64_t before conversion.
	bool negative = false;
	uint64_t value = 0;
	Integer(uint64_t value, Type* ty, bool negative = false);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class String: public Node {
public:
	std::string value;
	String(std::string value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Real: public Node {
public:
	long double value = 0.0L;
	Real(long double value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class FixedArrayLiteral: public Node {
public:
	std::vector<Node*> elements;
	FixedArrayLiteral(std::vector<Node*> elements, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Pascal set constructor. Each item is either a singleton (upper == nullptr)
 *  or an inclusive range. Node::ty is a FixedSetType whose item_type is the
 *  common ordinal type of all bounds, or is supplied contextually for `[]`. */
class SetLiteral: public Node {
public:
	struct Item {
		Node* lower;
		Node* upper;
	};
	std::vector<Item> items;
	SetLiteral(std::vector<Item> items, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Pascal `nil`. Constructed without a type: its type is fixed up by cast()
 *  to the surrounding reference-type target during assignment / argument
 *  passing. Emits as C++ `nullptr`. */
class NilLiteral: public Node {
public:
	NilLiteral() = default;
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Compiler intrinsic `Low(T)` / `High(T)`. The argument is a type, not a
 *  value expression, so this is not a ProcCall. It carries both the queried
 *  type (operand_type) and the expression result type (Node::ty), currently
 *  the same Type*. */
class TypeBound: public Node {
public:
	TypeBoundKind kind;
	Type* operand_type;
	TypeBound(TypeBoundKind kind, Type* operand_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Pascal `SizeOf(T)` / `SizeOf(expression)`, normalized to the operand's
 * static type. Constant evaluation can query the compiler layout while the
 * original node remains available to emit C++ `sizeof(emitted-type)`. */
class SizeOf: public Node {
public:
	Type* operand_type;
	explicit SizeOf(Type* operand_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Coerce: public BinaryOperation {
public:
	Coerce(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

class CoerceCheck: public BinaryOperation {
public:
	CoerceCheck(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

class AddrOf: public UnaryOperation {
public:
	AddrOf(Node* a);
	const char* diagnostic_kind() const override;
};

/** Shared base of standalone procedures/functions and methods. Holds
 *  everything call resolution and emission needs regardless of which of the
 *  two the callable is. `return_type` is unit_type() for procedures (Pascal
 *  `procedure`, no meaningful return); `has_body` is false on a forward
 *  declaration until the matching definition attaches it. */
class Callable: public Node {
public:
	std::string cxx_name;
	// Pascal spelling of the routine name. cxx_name is the C++ identifier
	// (`p_foo`, `~t_foo`); pas_name is the original Pascal spelling (`Foo`).
	// Used by `parse_inherited`'s anonymous path (`inherited;` resolves to
	// the parent method of the same Pascal name as the enclosing routine).
	std::string pas_name;
	RoutineType* ty;
	bool has_overload_directive;
	// Non-null when this source declaration names an RTL/compiler builtin.
	// Semantic special forms dispatch through descriptor metadata, never by
	// comparing the emitted C++ spelling.
	const BuiltinDesc* builtin_desc = nullptr;
	bool is_external = false;
	bool has_body = false;
	Frame* body_frame;
	Callable(std::string cxx_name,
	         std::string pas_name,
	         RoutineType* ty,
	         bool has_overload_directive);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Standalone procedure or function (Pascal `procedure`/`function` at
 *  unit/program scope). Carries no extra state beyond Callable; its identity
 *  is what distinguishes it from Method for type-checking against procedural
 *  pointer types. */
class Procedure: public Callable {
public:
	Procedure(std::string cxx_name,
	          std::string pas_name,
	          RoutineType* ty,
	          bool has_overload_directive);
	const char* diagnostic_kind() const override;
};

/** Method of a class/object/record. `owner_class` is the type it belongs to
 *  (used at overload-ranking time for the Self position). `virtual_kind`
 *  drives emission (`virtual`/`override`/etc. keywords in the C++ class). */
class Method: public Callable {
public:
	enum class VirtualKind { None, Virtual, Override, Abstract, Dynamic };
	Type* owner_class;
	VirtualKind virtual_kind;
	int vtable_slot;   // -1 = unassigned; populated at class-layout time
	Method(std::string cxx_name,
	       std::string pas_name,
	       RoutineType* ty,
	       bool has_overload_directive,
	       Type* owner_class,
	       VirtualKind virtual_kind);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Overload set: multiple Callables (Procedures or Methods) sharing one Pascal
 *  name, each with `has_overload_directive` set. Produced by Frame's
 *  registration when a second overload-marked callable is registered under
 *  the same name and by resolve_value when it aggregates matches across
 *  scopes. */
class OverloadSet: public Node {
public:
	std::vector<Callable*> members;
	OverloadSet(std::vector<Callable*> members);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};
