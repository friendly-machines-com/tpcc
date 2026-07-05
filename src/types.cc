#include "types.h"
#include "builtins.h"
#include <cassert>

IncompleteType::IncompleteType(std::string name) : name(name), resolved(nullptr) {}

FixedArrayType::FixedArrayType(Type* bounds, Type* item_type) {
	this->bounds = bounds;
	this->item_type = item_type;
}

FixedSetType::FixedSetType(Type* item_type) {
	this->item_type = item_type;
}

PointerType::PointerType(Type* item_type) {
	this->item_type = item_type;
}

RecordType::RecordType(Frame* children, bool packed) {
	this->children = children;
	this->packed = packed;
}

ClassType::ClassType(Frame* children) {
	this->children = children;
}

ObjectType::ObjectType(Frame* children) {
	this->children = children;
}

ModuleType::ModuleType(Frame* interface_children, Frame* implementation_children) {
	this->interface_children = interface_children;
	this->implementation_children = implementation_children;
}

UnitType::UnitType() = default;
UntypedIntegerType::UntypedIntegerType() = default;

BoundedCardinalType::BoundedCardinalType(uint64_t lower_bound, uint64_t higher_bound) {
	this->lower_bound = lower_bound;
	this->higher_bound = higher_bound;
	assert(higher_bound >= lower_bound);
}

RoutineType::RoutineType(std::vector<Parameter> formals, Type* return_type, RoutineKind kind) {
	this->formals = std::move(formals);
	this->return_type = return_type;
	this->kind = kind;
}

// Integer widening rank; -1 for non-integer types.
static int integer_widening_rank(Type* ty) {
	auto it = dynamic_cast<IntrinsicType*>(ty);
	if (!it)
		return -1;
	if (!it->rank)
		return -1;
	return *(it->rank);
}

Type* common_arith_type(Type* a, Type* b) {
	if (!a || !b)
		return nullptr;
	if (a == b)
		return a;
	if (a == shortstring_type() && b == char_type()) {
		return shortstring_type();
	} else if (a == char_type() && b == shortstring_type()) {
		return shortstring_type();
	}
	if (a == &untyped_integer_type())
		return b;
	if (b == &untyped_integer_type())
		return a;
	int ra = integer_widening_rank(a), rb = integer_widening_rank(b);
	if (ra < 0 || rb < 0)
		return nullptr;
	return (ra >= rb) ? a : b;
}

// FIXME: add enums, sets
int conversion_cost(Type* from, Type* to) {
	if (!from || !to)
		return -1;
	if (from == to)
		return 0;
	if (from == &untyped_integer_type())
		return 0; // literal adapts to any int
	int rfrom = integer_widening_rank(from), rto = integer_widening_rank(to);
	if (rfrom >= 0 && rto >= 0 && rto >= rfrom)
		return 1;
	return -1;
}
