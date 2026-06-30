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

ScopeValueEntry::ScopeValueEntry(Node* value, Type* ty) {
	this->value = value;
	this->ty = ty;
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
Node* Scope::lookup_value(std::string name) const {
	auto iter = value_items.find(name);
	if (iter != value_items.end()) {
		auto value_entry = *iter;
		auto result = value_entry.value;
/*
		if (value_entry.auto_deref) {
			// TODO: I am not sure the cmplexity of this feature is worth it.
			// The idea is when you do "uses foo", then all of foo's stuff is now accessible WITHOUT qualification, but also WITH qualification.  TODO: check what happens if you have a member "foo" in the unit "foo".  Also, for "with foo", I think all of foo's stuff is now accessible WITHOUT qualification, but NOT with qualification.  In any case, this feature here would be for the "accessible WITHOUT qualification" case, where you register only the qualification and it auto-derefs.
			auto ty = value_entry.ty;
			abort();
		}
*/
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
bool Scope::register_variable(std::string name, Node* v, Type* ty) {
	auto iter = value_items.find(name);
	if (iter != value_items.end()) {
		abort(); // duplicate type name
		return false;
	} else {
		if (parent && parent->lookup_value(name)) {
			fprintf(stderr, "warning: Name '%s' shadows another type of the same name in a super\n", name.c_str());
		}
		value_items[name] = ScopeValueEntry(v, ty);
		return true;
	}
}
