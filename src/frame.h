#pragma once
#include <string>
#include <map>
#include "types.h"

class Node;
class StorageSlot;
class Callable;

struct FrameValueEntry {
	Node* value;
	Type* ty;
	FrameValueEntry();
	FrameValueEntry(Node* value, Type* ty);
};

/** A Frame is the storage for one declaration block (the result of `var x,y,z:
 *  Integer;` or the body of a record/class/object/unit). A frame's optional
 *  `parent` pointer captures STRUCTURAL relationships (nested class,
 *  subclass-to-superclass), not lexical lookup chains; those live in the
 *  Parser's `scopes` stack of frames.
 *
 *  A Frame interleaves TWO independent name namespaces: `type_items` (for
 *  type identifiers like `TFoo`) and `value_items` (for vars, consts, enum
 *  members, callables). They are separate maps ON PURPOSE: Pascal allows
 *  `type Foo = Integer; var Foo: Foo;` in the same scope, where the `Foo`
 *  type and the `Foo` variable share an identifier but refer to unrelated
 *  entities. Merging the maps would break that. Each map enforces its own
 *  duplicate rule (two types with the same name, or two values with the
 *  same name, are duplicates; a type and a value sharing a name is not). */
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
    Node* lookup_value_local(std::string name) const; /* result: usually a StorageSlot */
    bool register_type(std::string name, Type* ty);
    /** Replace an existing type binding (used when patching a placeholder with
     *  its real Type* at type-block-end). No-op-safe for a fresh name. */
    void rebind_type(std::string name, Type* ty);
    bool register_variable(std::string name, Node* v, Type* ty); // FIXME: StorageSlot would already have ty
    /** Register a Callable (Procedure or Method) under NAME, applying Pascal's
     *  overload rules: a second registration succeeds only if both the
     *  existing and new declarations have has_overload_directive set (in
     *  which case the slot promotes from Callable to OverloadSet, or the new
     *  entry is appended to an existing OverloadSet). Otherwise it's a
     *  duplicate identifier. Returns true on success, false on a duplicate /
     *  overload-mismatch error (caller reports the diagnostic with location). */
    bool register_callable(std::string name, Callable* c);
    const std::map<std::string, Type*>& types_local() const { return type_items; }
    const std::map<std::string, FrameValueEntry>& values_local() const { return value_items; }
};
