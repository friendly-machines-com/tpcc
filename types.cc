#include <cassert>
#include "types.h"

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

RecordType::RecordType(Frame* children) {
	this->children = children;
}

ClassType::ClassType(Frame* children) {
	this->children = children;
}

UnitType::UnitType(Frame* interface_children, Frame* implementation_children) {
	this->interface_children = interface_children;
	this->implementation_children = implementation_children;
}

BoundedCardinalType::BoundedCardinalType(uint64_t lower_bound, uint64_t higher_bound) {
	this->lower_bound = lower_bound;
	this->higher_bound = higher_bound;
	assert(higher_bound >= lower_bound);
}
