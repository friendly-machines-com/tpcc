#include "cst.h"
#include <sstream>

std::string Node::str() const {
	std::stringstream sst;
	sst << (void*)this;
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
ProcCall::ProcCall(Node* receiver, Node* callee, std::vector<Node*> args)
    : receiver(receiver), callee(callee), args(std::move(args)) {}
Assign::Assign(Node* a, Node* b) : BinaryOperation(a, b) {}
MemberAccess::MemberAccess(Node* a, Node* b) : BinaryOperation(a, b) {}
Index::Index(Node* a, Node* b) : BinaryOperation(a, b) {}
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
Cast::Cast(Node* value, Type* target) : UnaryOperation(value) { this->ty = target; }

StorageSlot::StorageSlot(std::string cxx_name, Type* ty) {
	this->cxx_name = cxx_name;
	this->ty = ty;
}

EnumMemberRef::EnumMemberRef(std::string cxx_name, int64_t value, Type* ty) {
	this->cxx_name = cxx_name;
	this->value = value;
	this->ty = ty;
}

Integer::Integer(uint64_t value, Type* ty) {
	this->value = value;
	this->ty = ty;
}

String::String(std::string value, Type* ty) {
	this->value = std::move(value);
	this->ty = ty;
}

Callable::Callable(std::string pas_name,
		   std::string cxx_name,
		   std::vector<Parameter> formals,
		   Type* return_type,
		   bool has_overload_directive)
    : pas_name(std::move(pas_name)),
      cxx_name(std::move(cxx_name)),
      formals(std::move(formals)),
      return_type(return_type),
      has_overload_directive(has_overload_directive),
      has_body(false),
      body_frame(nullptr) {
	this->ty = return_type;
}

Procedure::Procedure(std::string pas_name,
		     std::string cxx_name,
		     std::vector<Parameter> formals,
		     Type* return_type,
		     bool has_overload_directive)
    : Callable(std::move(pas_name), std::move(cxx_name), std::move(formals),
	       return_type, has_overload_directive) {}

Method::Method(std::string pas_name,
	       std::string cxx_name,
	       std::vector<Parameter> formals,
	       Type* return_type,
	       bool has_overload_directive,
	       Type* owner_class,
	       VirtualKind virtual_kind)
    : Callable(std::move(pas_name), std::move(cxx_name), std::move(formals),
	       return_type, has_overload_directive),
      owner_class(owner_class),
      virtual_kind(virtual_kind),
      vtable_slot(-1) {}

OverloadSet::OverloadSet(std::string pas_name, std::vector<Callable*> members)
    : pas_name(std::move(pas_name)), members(std::move(members)) {}
