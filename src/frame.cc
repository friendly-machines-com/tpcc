#include "frame.h"
#include "cst.h"
#include <cassert>
#include <cstdio>

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
		if (result == nullptr) {
			fprintf(stderr, "error: %s\n", name.c_str());
		}
		assert(result);
		return result;
	} else if (parent) {
		return parent->lookup_type(name);
	} else {
		return nullptr;
	}
}
bool callable_binding_opens_parent(Node* binding) {
	if (auto callable = dynamic_cast<Callable*>(binding))
		return callable->has_overload_directive;
	if (auto overloads = dynamic_cast<OverloadSet*>(binding)) {
		for (Callable* callable : overloads->members)
			if (callable->has_overload_directive)
				return true;
	}
	return false;
}

Node* Frame::lookup_value(std::string name) const {
	std::vector<Callable*> callables;
	for (const Frame* current = this; current;
	     current = current->parent) {
		auto found = current->value_items.find(name);
		if (found == current->value_items.end())
			continue;
		Node* binding = found->second.value;
		assert(binding);

		auto callable =
		    dynamic_cast<Callable*>(binding);
		auto overloads =
		    dynamic_cast<OverloadSet*>(binding);
		if (!callable && !overloads) {
			if (callables.empty())
				return binding;
			break;
		}
		if (callable)
			callables.push_back(callable);
		else
			callables.insert(
			    callables.end(),
			    overloads->members.begin(),
			    overloads->members.end());

		// `overload` opens only the structural family represented by this
		// Frame chain. Whether a completed family may continue into another
		// lexical/unit scope is decided by ScopeEntry, not by Frame.
		if (!callable_binding_opens_parent(binding))
			break;
	}
	if (callables.empty())
		return nullptr;
	if (callables.size() == 1)
		return callables.front();
	return new OverloadSet(std::move(callables));
}

void Frame::rebind_type(std::string name, Type* ty) {
	type_items[name] = ty;
}

void Frame::rebind_value_type(std::string name, Type* ty) {
	value_items[name].ty = ty;
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

static bool same_emitted_callable_name(
    Callable* a, Callable* b) {
	auto a_method = dynamic_cast<Method*>(a);
	auto b_method = dynamic_cast<Method*>(b);
	if (a_method && b_method &&
	    a->ty->kind == DESTRUCTOR &&
	    b->ty->kind == DESTRUCTOR &&
	    dynamic_cast<ClassType*>(
	        a_method->owner_class) &&
	    a_method->owner_class ==
	        b_method->owner_class)
		return true;
	return a->cxx_name == b->cxx_name;
}

bool cxx_callable_signatures_collide(
    Callable* a, Callable* b) {
	return a && b &&
	       same_emitted_callable_name(a, b) &&
	       a->ty->same_cxx_parameter_list_as(
	           b->ty);
}

static CallableRegistration callable_pair_result(
    Callable* existing, Callable* incoming) {
	if (existing->ty->same_signature_as(
	        incoming->ty))
		return CallableRegistration::Rejected;
	if (cxx_callable_signatures_collide(
	        existing, incoming)) {
		// C++ does not use a function result to distinguish overloads, and
		// several generative Pascal types erase to the same carrier. Renaming
		// here would disconnect declarations, implementations, and virtual
		// dispatch, so reject the unsupported lowering at registration.
		return CallableRegistration::
		    CxxCarrierCollision;
	}
	return CallableRegistration::Added;
}

CallableRegistration Frame::register_callable(
    std::string name, Callable* c) {
	auto iter = value_items.find(name);
	if (iter == value_items.end()) {
		value_items[name] = FrameValueEntry(c, static_cast<RoutineType*>(c->ty)->return_type);
		return CallableRegistration::Added;
	}
	Node* existing = iter->second.value;
	if (auto ec = dynamic_cast<Callable*>(existing)) {
		if (ec->has_overload_directive && c->has_overload_directive) {
			auto result =
			    callable_pair_result(ec, c);
			if (result !=
			    CallableRegistration::Added)
				return result;
			auto set = new OverloadSet(std::vector<Callable*>{ec, c});
			iter->second.value = set;
			iter->second.ty = nullptr;
			return CallableRegistration::Added;
		}
		return CallableRegistration::Rejected;
	}
	if (auto os = dynamic_cast<OverloadSet*>(existing)) {
		if (c->has_overload_directive) {
			for (Callable* member :
			     os->members) {
				auto result =
				    callable_pair_result(
				        member, c);
				if (result !=
				    CallableRegistration::Added)
					return result;
			}
			os->members.push_back(c);
			return CallableRegistration::Added;
		}
		return CallableRegistration::Rejected;
	}
	return CallableRegistration::Rejected;
}
