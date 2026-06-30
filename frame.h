#pragma once
#include <cstdint>
#include <string>
#include <map>

class Node;

class Type {
public:
	virtual ~Type() = default;
};

class Frame;
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
	Frame* children;
	RecordType(Frame* children);
};

struct ClassType: public Type {
	Frame* children;
	ClassType(Frame* children);
};

struct PointerType: public Type {
	Type* item_type;
	PointerType(Type* item_type);
};

struct UnitType: public Type {
	Frame* interface_children;
	Frame* implementation_children;
	UnitType(Frame* interface_children, Frame* implementation_children);
};

struct FrameValueEntry {
	Node* value;
	Type* ty;
	FrameValueEntry();
	FrameValueEntry(Node* value, Type* ty);
};

/** A Frame is the storage for one declaration block (the result of `var x,y,z:
 *  Integer;` or the body of a record/class/object/unit). It owns name-to-entity
 *  maps. A frame's optional `parent` pointer captures STRUCTURAL relationships
 *  (nested class, subclass-to-superclass), not lexical lookup chains; those
 *  live in the Parser's `scopes` stack of frames. */
class Frame {
private:
	std::map<std::string, Type*> type_items;
	std::map<std::string, FrameValueEntry> value_items;
public:
	Frame* parent; // NOT invasive from Parser
public:
    // TODO: kind of frame (unit, record, class, ...); maybe also bool auto_unwrap; for "uses" and "with" blocks

    Frame(Frame* parent);
    Type* lookup_type(std::string name) const; /* TODO: or maybe a lookup with flags whether type and/or value is okay */
    Node* lookup_value(std::string name) const; /* result: usually a StorageSlot */
    bool register_type(std::string name, Type* ty);
    bool register_variable(std::string name, Node* v, Type* ty); // FIXME: StorageSlot would already have ty
};
