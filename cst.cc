#include <sstream>
#include "cst.h"

std::string Node::str() const {
	std::stringstream sst;
	sst << (void*) this;
	return sst.str();
}

void Block::add(Node* stmt) {
	statements.push_back(stmt);
}

Symbol::Symbol(std::string text) {
	this->text = text;
}

std::string Symbol::str() const {
	return text;
}
UnaryOperation::UnaryOperation(Node* a) {
	this->a = a;
}

BinaryOperation::BinaryOperation(Node* a, Node* b) {
	this->a = a;
	this->b = b;
}

Add::Add(Node* a, Node* b) : BinaryOperation(a, b) {}
Subtract::Subtract(Node* a, Node* b) : BinaryOperation(a, b) {}
Multiply::Multiply(Node* a, Node* b) : BinaryOperation(a, b) {}
Divide::Divide(Node* a, Node* b) : BinaryOperation(a, b) {}
Xor::Xor(Node* a, Node* b) : BinaryOperation(a, b) {}
And::And(Node* a, Node* b) : BinaryOperation(a, b) {}
Or::Or(Node* a, Node* b) : BinaryOperation(a, b) {}
ProcCall::ProcCall(Node* a, Node* b) : BinaryOperation(a, b) {}
Assign::Assign(Node* a, Node* b) : BinaryOperation(a, b) {}
ShiftLeft::ShiftLeft(Node* a, Node* b) : BinaryOperation(a, b) {}
ShiftRight::ShiftRight(Node* a, Node* b) : BinaryOperation(a, b) {}
Div::Div(Node* a, Node* b) : BinaryOperation(a, b) {}
Mod::Mod(Node* a, Node* b) : BinaryOperation(a, b) {}
Coerce::Coerce(Node* a, Node* b) : BinaryOperation(a, b) {}
Equal::Equal(Node* a, Node* b) : BinaryOperation(a, b) {}
NotEqual::NotEqual(Node* a, Node* b) : BinaryOperation(a, b) {}
Less::Less(Node* a, Node* b) : BinaryOperation(a, b) {}
LessOrEqual::LessOrEqual(Node* a, Node* b) : BinaryOperation(a, b) {}
Greater::Greater(Node* a, Node* b) : BinaryOperation(a, b) {}
GreaterOrEqual::GreaterOrEqual(Node* a, Node* b) : BinaryOperation(a, b) {}

Not::Not(Node* a) : UnaryOperation(a) {}
Dereference::Dereference(Node* a) : UnaryOperation(a) {}
Return::Return(Node* a) : UnaryOperation(a) {}
Negate::Negate(Node* a) : UnaryOperation(a) {}
Positivize::Positivize(Node* a) : UnaryOperation(a) {}
AddrOf::AddrOf(Node* a) : UnaryOperation(a) {}

StorageSlot::StorageSlot(Type* ty) {
	this->ty = ty;
}

Constant::Constant(uint64_t value) {
	this->value = value;
}

