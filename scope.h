#pragma once
#include <cstdint>
#include <string>
#include <map>

class Node;

class Type {
public:
	virtual ~Type() = default;
};

class Scope;
class StorageSlot;

struct BoundedCardinalType: public Type {
	uint64_t lower_bound;
	uint64_t higher_bound;
	BoundedCardinalType(uint64_t lower_bound, uint64_t higher_bound);
};

struct FixedArrayType: public Type {
	Type* bounds;
	Type* item_type;
	FixedArrayType(Type* bounds, Type* item_type);
};

struct FixedSetType: public Type {
	Type* item_type;
	FixedSetType(Type* item_type);
};

struct RecordType: public Type {
	Scope* children;
	RecordType(Scope* children);
};

struct ClassType: public Type {
	Scope* children;
	ClassType(Scope* children);
};

struct PointerType: public Type {
	Type* item_type;
	PointerType(Type* item_type);
};

struct UnitType: public Type {
	Scope* interface_children;
	Scope* implementation_children;
	UnitType(Scope* interface_children, Scope* implementation_children);
};

struct ScopeValueEntry {
	Node* value;
	Type* ty;
	ScopeValueEntry();
	ScopeValueEntry(Node* value, Type* ty);
};

class Scope /*: public Type*/ {
private:
	std::map<std::string, Type*> type_items;
	std::map<std::string, ScopeValueEntry> value_items;
public:
	Scope* parent; // NOT invasive from Parser
public:
    // TODO: kind of scope (unit, record, class, ...); maybe also bool auto_unwrap; for "uses" and "with" blocks

    Scope(Scope* parent);
    Type* lookup_type(std::string name) const; /* TODO: or maybe a lookup with flags whether type and/or value is okay */
    Node* lookup_value(std::string name) const; /* result: usually a StorageSlot */
    bool register_type(std::string name, Type* ty);
    bool register_variable(std::string name, Node* v, Type* ty); // FIXME: StorageSlot would already have ty
};
