#include "frame.h"
#include "builtins.h"
#include "cst.h"
#include <cassert>
#include <cstdio>

FrameValueEntry::FrameValueEntry() : value(nullptr), ty(nullptr) {}

FrameValueEntry::FrameValueEntry(Node* value, Type* ty) {
	this->value = value;
	this->ty = ty;
}

Frame::Frame(Frame* parent) { this->parent = parent; }

std::optional<Binding> Frame::lookup_type_or_value_local(std::string name) const {
	auto found = items.find(name);
	if (found == items.end())
		return std::nullopt;
	return found->second;
}

std::optional<Binding> Frame::lookup_type_or_value(std::string name) const {
	std::vector<Callable*> callables;
	for (const Frame* current = this; current; current = current->parent) {
		auto found = current->lookup_type_or_value_local(name);
		if (!found)
			continue;
		if (auto type = std::get_if<Type*>(&*found)) {
			if (callables.empty())
				return Binding{std::in_place_type<Type*>, *type};
			break;
		}

		Node* binding = std::get<Node*>(*found);
		auto callable = dynamic_cast<Callable*>(binding);
		auto overloads = dynamic_cast<OverloadSet*>(binding);
		if (!callable && !overloads) {
			if (callables.empty())
				return Binding{std::in_place_type<Node*>, binding};
			break;
		}
		Callable* representative = callable ? callable : overloads->members.front();
		if (!callables.empty() && !same_callable_lookup_family(callables.front(), representative))
			break;
		if (callable)
			callables.push_back(callable);
		else
			callables.insert(callables.end(), overloads->members.begin(), overloads->members.end());
		if (!callable_binding_opens_parent(binding))
			break;
	}
	if (callables.empty())
		return std::nullopt;
	Node* result = callables.size() == 1 ? static_cast<Node*>(callables.front()) : static_cast<Node*>(new OverloadSet(std::move(callables)));
	return Binding{std::in_place_type<Node*>, result};
}

Type* Frame::lookup_type(std::string name) const {
	for (const Frame* current = this; current; current = current->parent) {
		auto found = current->lookup_type_or_value_local(name);
		if (!found)
			continue;
		if (auto type = std::get_if<Type*>(&*found)) {
			assert(*type);
			return *type;
		}
		// A value is irrelevant in a required type position. Continue to
		// the enclosing structural Frame so `field: field` remains legal.
	}
	return nullptr;
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
	for (const Frame* current = this; current; current = current->parent) {
		auto found = current->lookup_type_or_value_local(name);
		if (!found)
			continue;
		auto value = std::get_if<Node*>(&*found);
		if (!value)
			continue;
		Node* binding = *value;
		assert(binding);

		auto callable = dynamic_cast<Callable*>(binding);
		auto overloads = dynamic_cast<OverloadSet*>(binding);
		if (!callable && !overloads) {
			if (callables.empty())
				return binding;
			break;
		}
		Callable* representative = callable ? callable : overloads->members.front();
		if (!callables.empty() && !same_callable_lookup_family(callables.front(), representative))
			// An identically named but incompatible routine category in
			// an ancestor is not another candidate of this family. The
			// already-selected descendant family shadows it.
			break;
		if (callable)
			callables.push_back(callable);
		else
			callables.insert(callables.end(), overloads->members.begin(), overloads->members.end());

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
	auto found = items.find(name);
	assert(found != items.end());
	assert(std::holds_alternative<Type*>(found->second));
	found->second = Binding{std::in_place_type<Type*>, ty};
}

void Frame::rebind_value_type(std::string name, Type* ty) {
	auto found = items.find(name);
	assert(found != items.end());
	auto value = std::get_if<Node*>(&found->second);
	assert(value && *value);
	Node* node = *value;
	if (auto callable = dynamic_cast<Callable*>(node)) {
		assert(!ty || (callable->ty && callable->ty->return_type == ty));
		return;
	}
	if (node->ty)
		assert(!ty || node->ty == ty);
	else
		node->ty = ty;
}

/** returns whether it was registered anew, with type TY */
bool Frame::register_type(std::string name, Type* ty) {
	auto iter = items.find(name);
	if (iter != items.end()) {
		return false;
	} else {
		if (parent && parent->lookup_type(name)) {
			fprintf(stderr, "warning: Type name '%s' shadows another type of the same name\n", name.c_str());
		}
		items.emplace(std::move(name), Binding{std::in_place_type<Type*>, ty});
		return true;
	}
}

/** returns whether it was registered anew, with value V of type T */
bool Frame::register_variable(std::string name, Node* v, Type* ty) {
	auto iter = items.find(name);
	if (iter != items.end()) {
		return false;
	} else {
		if (parent && parent->lookup_value(name)) {
			fprintf(stderr, "warning: Name '%s' shadows another type of the same name in a super\n", name.c_str());
		}
		assert(v);
		assert(!ty || !v->ty || ty == v->ty);
		if (!v->ty)
			v->ty = ty;
		items.emplace(std::move(name), Binding{std::in_place_type<Node*>, v});
		return true;
	}
}

static bool same_emitted_callable_name(Callable* a, Callable* b) {
	auto a_method = dynamic_cast<Method*>(a);
	auto b_method = dynamic_cast<Method*>(b);
	if (a_method && b_method && a->ty->kind == DESTRUCTOR && b->ty->kind == DESTRUCTOR && dynamic_cast<ClassType*>(a_method->owner_class) && a_method->owner_class == b_method->owner_class)
		return true;
	return a->cxx_name == b->cxx_name;
}

bool same_callable_overload_category(Callable* a, Callable* b) {
	if (!a || !b || !a->ty || !b->ty)
		return false;
	auto a_method = dynamic_cast<Method*>(a);
	auto b_method = dynamic_cast<Method*>(b);
	if (static_cast<bool>(a_method) != static_cast<bool>(b_method))
		return false;
	if (a->ty->kind != b->ty->kind)
		return false;
	if (a_method && a_method->is_static != b_method->is_static)
		return false;
	return true;
}

static bool is_legacy_predefined_callable(Callable* callable) {
	const BuiltinDesc* desc = callable ? callable->builtin_desc : nullptr;
	return desc && (desc->syntax_kind != BuiltinSyntaxKind::None || desc->type_bound_kind.has_value());
}

bool same_callable_lookup_family(Callable* a, Callable* b) { return same_callable_overload_category(a, b) && is_legacy_predefined_callable(a) == is_legacy_predefined_callable(b); }

bool cxx_callable_signatures_collide(Callable* a, Callable* b) {
	if (!a || !b || !same_emitted_callable_name(a, b))
		return false;
	const bool a_conversion = a->is_conversion_operator();
	const bool b_conversion = b->is_conversion_operator();
	// A contextual conversion has one extra C++ parameter carrying the
	// compiler-selected destination. A non-conversion with the same emitted
	// name therefore has a different C++ arity. Between two conversions, the
	// destination tag distinguishes results exactly when their emitted target
	// carriers are distinct. This applies equally to Explicit: Pascal typecast
	// syntax supplies its result context before selecting the declaration.
	if (a_conversion != b_conversion)
		return false;
	if (a_conversion && !a->ty->return_type->same_cxx_carrier_as(b->ty->return_type))
		return false;
	return a->ty->same_cxx_parameter_list_as(b->ty);
}

static bool same_emitted_declaration_scope(Callable* a, Callable* b) {
	if (!a || !b || a->is_external || b->is_external)
		return false;
	auto a_method = dynamic_cast<Method*>(a);
	auto b_method = dynamic_cast<Method*>(b);
	if (a_method || b_method) {
		if (!a_method || !b_method || a_method->owner_class != b_method->owner_class)
			return false;
		auto in_metaclass = [](const Method* method) { return method->ty->kind == CLASS_METHOD || method->ty->kind == CLASS_CONSTRUCTOR || method->ty->kind == CLASS_DESTRUCTOR; };
		return in_metaclass(a_method) == in_metaclass(b_method);
	}
	// Two standalone declarations registered in one Frame emit into the same
	// unit namespace (or the same nested C++ block). Unit lookup may later
	// combine declarations from different Frames, but registration never does.
	return true;
}

CallableRegistration::Kind validate_callable_pair(Callable* existing, Callable* incoming) {
	if (!same_callable_overload_category(existing, incoming))
		return CallableRegistration::Kind::Rejected;
	if (existing->ty->same_overload_signature_as(incoming->ty)) {
		// Explicit, checked, unchecked, and legacy conversion declarations all
		// receive their destination from value context rather than from an
		// ordinary source argument. Within one canonical operator family,
		// exact result Type* identity is therefore part of the Pascal overload
		// key. Ordinary routines and every other operator cannot overload by
		// result.
		const bool distinct_conversion_results = existing->is_conversion_operator() && incoming->is_conversion_operator() && existing->ty->return_type != incoming->ty->return_type;
		if (!distinct_conversion_results)
			return CallableRegistration::Kind::Rejected;
	}
	if (same_emitted_declaration_scope(existing, incoming) && cxx_callable_signatures_collide(existing, incoming)) {
		// C++ does not use a function result to distinguish overloads, and
		// several generative Pascal types erase to the same carrier. The
		// emitted owner is part of this test: one Pascal class Frame lowers
		// instance methods and class methods into different C++ classes.
		// Renaming would disconnect declarations, implementations, and virtual
		// dispatch, so reject only a collision in one actual C++ scope.
		return CallableRegistration::Kind::CxxCarrierCollision;
	}
	return CallableRegistration::Kind::Added;
}

CallableRegistration Frame::collect_callable(std::string name, Callable* c) {
	auto iter = items.find(name);
	if (iter == items.end()) {
		items.emplace(std::move(name), Binding{std::in_place_type<Node*>, c});
		return {CallableRegistration::Kind::Added};
	}
	auto existing_value = std::get_if<Node*>(&iter->second);
	if (!existing_value)
		return {CallableRegistration::Kind::Rejected};
	Node* existing = *existing_value;
	if (auto ec = dynamic_cast<Callable*>(existing)) {
		// This is only a declaration collection step. In particular, EC and C
		// may temporarily have incompatible categories or duplicate signatures:
		// deciding that requires canonical Type* identity, which an open type
		// block does not yet provide. Post-normalization aggregate validation
		// establishes the public OverloadSet invariant before emission.
		auto set = new OverloadSet(std::vector<Callable*>{ec, c});
		iter->second = Binding{std::in_place_type<Node*>, set};
		return {CallableRegistration::Kind::Added};
	}
	if (auto os = dynamic_cast<OverloadSet*>(existing)) {
		os->members.push_back(c);
		return {CallableRegistration::Kind::Added};
	}
	return {CallableRegistration::Kind::Rejected, existing};
}

CallableRegistration Frame::register_callable(std::string name, Callable* c) {
	auto iter = items.find(name);
	if (iter == items.end())
		return collect_callable(std::move(name), c);

	auto existing_value = std::get_if<Node*>(&iter->second);
	if (!existing_value)
		return {CallableRegistration::Kind::Rejected};
	Node* existing = *existing_value;
	if (auto ec = dynamic_cast<Callable*>(existing)) {
		auto result = validate_callable_pair(ec, c);
		if (result != CallableRegistration::Kind::Added)
			return {result, existing, ec};
	} else if (auto os = dynamic_cast<OverloadSet*>(existing)) {
		for (Callable* member : os->members) {
			auto result = validate_callable_pair(member, c);
			if (result != CallableRegistration::Kind::Added)
				return {result, existing, member};
		}
	} else {
		return {CallableRegistration::Kind::Rejected, existing};
	}
	return collect_callable(std::move(name), c);
}

static Type* declaration_value_type(Node* value) {
	if (auto callable = dynamic_cast<Callable*>(value))
		return callable->ty ? callable->ty->return_type : nullptr;
	return value ? value->ty : nullptr;
}

std::vector<std::pair<std::string, Type*>> Frame::type_declarations() const {
	std::vector<std::pair<std::string, Type*>> result;
	for (const auto& item : items)
		if (auto type = std::get_if<Type*>(&item.second))
			result.emplace_back(item.first, *type);
	return result;
}

std::vector<std::pair<std::string, FrameValueEntry>> Frame::value_declarations() const {
	std::vector<std::pair<std::string, FrameValueEntry>> result;
	for (const auto& item : items)
		if (auto value = std::get_if<Node*>(&item.second))
			result.emplace_back(item.first, FrameValueEntry{*value, declaration_value_type(*value)});
	return result;
}

std::vector<NamedBinding> Frame::bindings_local() const {
	std::vector<NamedBinding> result;
	result.reserve(items.size());
	for (const auto& item : items)
		result.push_back(item);
	return result;
}
