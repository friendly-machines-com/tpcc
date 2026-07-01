#pragma once
#include <cstdint>

class Frame;

class Type {
public:
	virtual ~Type() = default;
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

struct PointerType: public Type {
	Type* item_type;
	PointerType(Type* item_type);
};

struct UnitType: public Type {
	Frame* interface_children;
	Frame* implementation_children;
	UnitType(Frame* interface_children, Frame* implementation_children);
};
