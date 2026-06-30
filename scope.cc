#include <cstdint>
#include <cassert>
#include <cstdio>
#include "scope.h"

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

RecordType::RecordType(Scope* children) {
	this->children = children;
}

ClassType::ClassType(Scope* children) {
	this->children = children;
}

UnitType::UnitType(Scope* interface_children, Scope* implementation_children) {
	this->interface_children = interface_children;
	this->implementation_children = implementation_children;
}

BoundedCardinalType::BoundedCardinalType(uint64_t lower_bound, uint64_t higher_bound) {
    this->lower_bound = lower_bound;
    this->higher_bound = higher_bound;
	assert(higher_bound >= lower_bound);
}

ScopeValueEntry::ScopeValueEntry() : value(nullptr), ty(nullptr) {}

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
		auto result = iter->second;
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
		auto result = iter->second.value;
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
