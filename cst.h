#pragma once
#include <string>

class Type;

class Node {
public:
	std::string str() const;
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
};

class Subtract: public BinaryOperation {
};

class Multiply: public BinaryOperation {
};

class Divide: public BinaryOperation {
};

class Xor: public BinaryOperation {
};

class And: public BinaryOperation {
};

class Or: public BinaryOperation {
};

class Not: public UnaryOperation {
};

class ProcCall: public BinaryOperation {
};

class Dereference: public UnaryOperation {
};

class Return: public UnaryOperation {
};

class Assign: public BinaryOperation {
};

class Negate: public UnaryOperation {
};

class Positivize: public UnaryOperation {
};

struct StorageSlot: public Node {
	Type* ty;
};

class ShiftLeft: public BinaryOperation {
};

class ShiftRight: public BinaryOperation {
};

class Div: public BinaryOperation {
};

class Mod: public BinaryOperation {
};

class Coerce: public BinaryOperation {
};

class AddrOf: public UnaryOperation {
};

class Equal: public BinaryOperation {
};
class NotEqual: public BinaryOperation {
};
class Less: public BinaryOperation {
};
class LessOrEqual: public BinaryOperation {
};
class Greater: public BinaryOperation {
};
class GreaterOrEqual: public BinaryOperation {
};
