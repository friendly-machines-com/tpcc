#pragma once
#include <cstdint>
#include <string>

class Frame;

class Type {
public:
	virtual ~Type() = default;
};

/** Placeholder for a type name that has been introduced but whose full
 *  definition has not yet arrived. Sources: implicit forward reference in
 *  pointer position (`^TFoo` before TFoo is declared), explicit class-forward
 *  (`TFoo = class;`), and LHS pre-registration to permit self-recursive RHS.
 *  Must be patched (resolved != nullptr) by the end of the containing type
 *  block; any Type* consumer that needs semantic information should unwrap
 *  through `resolved`. */
struct IncompleteType: public Type {
	std::string name;
	Type* resolved;
	IncompleteType(std::string name);
};

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

struct ObjectType: public Type {
	Frame* children;
	ObjectType(Frame* children);
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
