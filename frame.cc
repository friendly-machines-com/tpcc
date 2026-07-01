#include <cassert>
#include <cstdio>
#include "frame.h"

FrameValueEntry::FrameValueEntry() : value(nullptr), ty(nullptr) {}

FrameValueEntry::FrameValueEntry(Node* value, Type* ty) {
	this->value = value;
	this->ty = ty;
}

Frame::Frame(Frame* parent) {
	this->parent = parent;
}

Type* Frame::lookup_type(std::string name) const {
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
Node* Frame::lookup_value(std::string name) const {
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
bool Frame::register_type(std::string name, Type* ty) {
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
bool Frame::register_variable(std::string name, Node* v, Type* ty) {
	auto iter = value_items.find(name);
	if (iter != value_items.end()) {
		abort(); // duplicate type name
		return false;
	} else {
		if (parent && parent->lookup_value(name)) {
			fprintf(stderr, "warning: Name '%s' shadows another type of the same name in a super\n", name.c_str());
		}
		value_items[name] = FrameValueEntry(v, ty);
		return true;
	}
}
