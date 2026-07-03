#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Type;
class Frame;

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

class Add: public BinaryOperation {
public:
	Add(Node* a, Node* b);
};

class Subtract: public BinaryOperation {
public:
	Subtract(Node* a, Node* b);
};

class Multiply: public BinaryOperation {
public:
	Multiply(Node* a, Node* b);
};

class Divide: public BinaryOperation {
public:
	Divide(Node* a, Node* b);
};

class Xor: public BinaryOperation {
public:
	Xor(Node* a, Node* b);
};

class And: public BinaryOperation {
public:
	And(Node* a, Node* b);
};

class Or: public BinaryOperation {
public:
	Or(Node* a, Node* b);
};

class Not: public UnaryOperation {
public:
	Not(Node* a);
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

class Dereference: public UnaryOperation {
public:
	Dereference(Node* a);
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

class Assign: public BinaryOperation {
public:
	Assign(Node* a, Node* b);
};

class Negate: public UnaryOperation {
public:
	Negate(Node* a);
};

class Positivize: public UnaryOperation {
public:
	Positivize(Node* a);
};

/** Compiler-inserted implicit type conversion. Distinct from Coerce (which
 *  represents the user-written `x as T`). Target type held on Node::ty; the
 *  wrapped value is `a`. Emits as `static_cast<target>(a)`. */
class Cast: public UnaryOperation {
public:
	Cast(Node* value, Type* target);
};

struct StorageSlot: public Node {
	// Identifier as it will appear in the emitted C++ output. May diverge from
	// the Pascal source name (mangling for C++ reserved words, later
	// unit-name prefixing). Kept on the node so emission is a straight walk
	// without a separate reverse-lookup back to the Frame that holds this slot.
	std::string cxx_name;
	StorageSlot(std::string cxx_name, Type* ty);
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

class ShiftLeft: public BinaryOperation {
public:
	ShiftLeft(Node* a, Node* b);
};

class ShiftRight: public BinaryOperation {
public:
	ShiftRight(Node* a, Node* b);
};

class Div: public BinaryOperation {
public:
	Div(Node* a, Node* b);
};

class Mod: public BinaryOperation {
public:
	Mod(Node* a, Node* b);
};

class Coerce: public BinaryOperation {
public:
	Coerce(Node* a, Node* b);
};

class AddrOf: public UnaryOperation {
public:
	AddrOf(Node* a);
};

class Equal: public BinaryOperation {
public:
	Equal(Node* a, Node* b);
};
class NotEqual: public BinaryOperation {
public:
	NotEqual(Node* a, Node* b);
};
class Less: public BinaryOperation {
public:
	Less(Node* a, Node* b);
};
class LessOrEqual: public BinaryOperation {
public:
	LessOrEqual(Node* a, Node* b);
};
class Greater: public BinaryOperation {
public:
	Greater(Node* a, Node* b);
};
class GreaterOrEqual: public BinaryOperation {
public:
	GreaterOrEqual(Node* a, Node* b);
};

enum class ParamMode { Value, Var, Out, Const };

struct Parameter {
	std::string pas_name;
	std::string cxx_name;
	Type* ty;
	ParamMode mode;
	Node* default_value; // null if none
};

/** Shared base of standalone procedures/functions and methods. Holds
 *  everything call resolution and emission needs regardless of which of the
 *  two the callable is. `return_type` is unit_type() for procedures (Pascal
 *  `procedure`, no meaningful return); `body` is null on a forward
 *  declaration until the matching definition attaches it. */
class Callable: public Node {
public:
	std::string pas_name;
	std::string cxx_name;
	std::vector<Parameter> formals;
	Type* return_type;
	bool has_overload_directive;
	Node* body;
	Frame* body_frame;
	Callable(std::string pas_name,
	         std::string cxx_name,
	         std::vector<Parameter> formals,
	         Type* return_type,
	         bool has_overload_directive);
};

/** Standalone procedure or function (Pascal `procedure`/`function` at
 *  unit/program scope). Carries no extra state beyond Callable; its identity
 *  is what distinguishes it from Method for type-checking against procedural
 *  pointer types. */
class Procedure: public Callable {
public:
	Procedure(std::string pas_name,
	          std::string cxx_name,
	          std::vector<Parameter> formals,
	          Type* return_type,
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
	Method(std::string pas_name,
	       std::string cxx_name,
	       std::vector<Parameter> formals,
	       Type* return_type,
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
	std::string pas_name;
	std::vector<Callable*> members;
	OverloadSet(std::string pas_name, std::vector<Callable*> members);
};
