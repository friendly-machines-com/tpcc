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
	Node* callee;
	std::vector<Node*> args;
	ProcCall(Node* callee, std::vector<Node*> args);
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

class Constant: public Node {
public:
	uint64_t value;
	Constant(uint64_t value, Type* ty);
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

/** A user-defined procedure or function. `return_type` is unit_type() for
 *  procedures (Pascal-level `procedure`, no return value) and the declared
 *  result type for functions. `body` is null on a forward declaration until
 *  the matching definition attaches it. `body_frame` holds the parameter
 *  StorageSlots and any local declarations. */
class Procedure: public Node {
public:
	std::string pas_name;
	std::string cxx_name;
	std::vector<Parameter> formals;
	Type* return_type;
	bool has_overload_directive;
	Node* body;
	Frame* body_frame;
	Procedure(std::string pas_name,
	          std::string cxx_name,
	          std::vector<Parameter> formals,
	          Type* return_type,
	          bool has_overload_directive);
};

/** A collection of overload-marked Procedures that share one Pascal name.
 *  Every member has has_overload_directive == true. Produced by Frame's
 *  registration logic (when a second overload-marked procedure lands under
 *  the same name) and by resolve_value when it aggregates matches from
 *  multiple scopes. */
class OverloadSet: public Node {
public:
	std::string pas_name;
	std::vector<Procedure*> members;
	OverloadSet(std::string pas_name, std::vector<Procedure*> members);
};
