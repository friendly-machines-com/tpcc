#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Type;
class Frame;
class RoutineType;
class Callable;

class Node {
public:
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
};

class Symbol: public Node {
private:
	std::string text;
public:
	Symbol(std::string text);
	std::string str() const;
};

class UnaryOperation: public Node {
public:
       Node* a;
       UnaryOperation(Node* a);
};

class BinaryOperation: public Node {
public:
       Node* a;
       Node* b;
       BinaryOperation(Node* a, Node* b);
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
};

class Dereference: public UnaryOperation {
public:
	Dereference(Node* a);
};

class Assign: public BinaryOperation {
public:
	Assign(Node* a, Node* b);
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
};

/** container.member. `a` is the container (usually a StorageSlot for the
 *  record variable or with-alias) and `b` is the member (usually the field's
 *  StorageSlot). Produced by resolve_value/resolve_lvalue when a name hits
 *  inside a with-pushed scope; will also carry explicit `rec.field` grammar
 *  when that lands. */
class MemberAccess: public BinaryOperation {
public:
	MemberAccess(Node* a, Node* b);
};

/** array[index]. `a` is the array value; `b` is the index expression.
 *  Result type (Node::ty) is the array's element type, set by the parser at
 *  construction. */
class Index: public BinaryOperation {
public:
	Index(Node* a, Node* b);
};

class Return: public UnaryOperation {
public:
	Return(Node* a);
};

/** Compiler-inserted implicit type conversion. Distinct from Coerce (which
 *  represents the user-written `x as T`). Target type held on Node::ty; the
 *  wrapped value is `a`. Emits as `static_cast<target>(a)`. */
class Cast: public UnaryOperation {
public:
	Cast(Node* value, Type* target);
};

struct StorageSlot: public Node {
	std::string cxx_name;
	StorageSlot(std::string cxx_name, Type* ty);
};

struct EnumMemberRef: public Node {
	std::string cxx_name;
	int64_t value;
	EnumMemberRef(std::string cxx_name, int64_t value, Type* ty);
};

class Integer: public Node {
public:
	uint64_t value;
	Integer(uint64_t value, Type* ty);
};

class String: public Node {
public:
	std::string value;
	String(std::string value, Type* ty);
};

/** Pascal `nil`. Constructed without a type: its type is fixed up by cast()
 *  to the surrounding reference-type target during assignment / argument
 *  passing. Emits as C++ `nullptr`. */
class NilLiteral: public Node {
public:
	NilLiteral() = default;
};

class Coerce: public BinaryOperation {
public:
	Coerce(Node* a, Node* b);
};

class CoerceCheck: public BinaryOperation {
public:
	CoerceCheck(Node* a, Node* b);
};

class AddrOf: public UnaryOperation {
public:
	AddrOf(Node* a);
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
	bool is_external = false;
	bool has_body = false;
	Frame* body_frame;
	Callable(std::string cxx_name,
	         std::string pas_name,
	         RoutineType* ty,
	         bool has_overload_directive);
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
};
