#pragma once
#include <cstdint>
#include <string>
#include <map>

class Node;

class Type {
};

class Scope;
class StorageSlot;

struct BoundedCardinalType: public Type {
	uint64_t lower_bound;
	uint64_t higher_bound;
};

struct FixedArrayType: public Type {
	Node* bounds;
	Type* item_type;
};

struct FixedSetType: public Type {
	Type* item_type;
};

struct RecordType: public Type {
	Scope* children;
};

struct ClassType: public Type {
	Scope* children;
};

struct PointerType: public Type {
	Type* item_type;
};

struct UnitType: public Type {
	Scope* interface_children;
	Scope* implementation_children;
};

class Scope /*: public Type*/ {
private:
	std::map<std::string, Type*> type_items;
	std::map<std::string, Type*> value_items;
public:
	Scope* parent;
public:
    // TODO: kind of scope (unit, record, class, ...)

    Scope(Scope* parent);
    Type* lookup_type(std::string name) const;
    Node* lookup_value(std::string name) const;
    bool register_type(std::string name, Type* ty);
    bool register_variable(std::string name, StorageSlot* v);
};
