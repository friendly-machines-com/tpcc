#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Type;

class Node {
public:
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

class ProcCall: public BinaryOperation {
public:
	ProcCall(Node* a, Node* b);
};

class Dereference: public UnaryOperation {
public:
	Dereference(Node* a);
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

struct StorageSlot: public Node {
	Type* ty;
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
	Constant(uint64_t value);
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
