#include <cstdint>
#include <cassert>
#include "type_scope.h"

FixedArrayType::FixedArrayType(Node* bounds, Type* item_type) {
	this->bounds = bounds;
	this->item_type = item_type;
}

BoundedCardinalType::BoundedCardinalType(uint64_t lower_bound, uint64_t higher_bound) {
    this->lower_bound = lower_bound;
    this->higher_bound = higher_bound;
	assert(higher_bound >= lower_bound);
}

Scope::Scope(Scope* parent) {
	this->parent = parent;
}

Type* Scope::lookup_type(std::string name) const {
	auto iter = type_items.find(name);
	if (iter != type_items.end()) {
		auto result = *iter;
		assert(result);
		return result;
	} else if (parent) {
		return parent->lookup_type(name);
	} else {
		return nullptr;
	}
}
Type* Scope::lookup_value(std::string name) const {
	auto iter = value_items.find(name);
	if (iter != value_items.end()) {
		auto result = *iter;
		assert(result);
		return result;
	} else if (parent) {
		return parent->lookup_value(name);
	} else {
		return nullptr;
	}
}

/** returns whether it was registered anew, with type TY */
bool Scope::register_type(std::string name, Type* ty) {
	auto iter = type_items.find(name);
	if (iter != type_items.end()) {
		abort(); // duplicate type name
		return false;
	} else {
		if (parent && parent->lookup_type(name)) {
			fprintf(stderr, "warning: Type name '%s' shadows another type of the same name\n", name.c_str());
		}
		type_items[name] = ty;
		return true;
	}
}

/** returns whether it was registered anew, with value V of type T */
bool Scope::register_variable(std::string name, StorageSlot* v) {
	auto iter = value_items.find(name);
	if (iter != value_items.end()) {
		abort(); // duplicate type name
		return false;
	} else {
		if (parent && parent->lookup_value(name)) {
			fprintf(stderr, "warning: Name '%s' shadows another type of the same name\n", name.c_str());
		}
		// FIXME: also store ty
		value_items[name] = v;
		return true;
	}
}
