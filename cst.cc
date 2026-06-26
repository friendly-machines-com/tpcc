#include <sstream>
#include "cst.h"

std::string Node::str() const {
	std::stringstream sst;
	sst << (void*) this;
	return sst.str();
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

