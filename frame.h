#pragma once
#include <string>
#include <map>
#include "types.h"

class Node;
class StorageSlot;

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
