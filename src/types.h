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
	// C++ identifier emitted for this record. Empty until the containing
	// type-block declaration assigns it (parse_type_block).
	std::string cxx_name;
	bool packed = false;
	RecordType(Frame* children, bool packed);
};

struct ClassType: public Type {
	Frame* children;
	std::string cxx_name;
	ClassType(Frame* children);
};

struct ObjectType: public Type {
	Frame* children;
	std::string cxx_name;
	ObjectType(Frame* children);
};

struct PointerType: public Type {
	Type* item_type;
	PointerType(Type* item_type);
};

/** The type of a Pascal `unit X;` module. Renamed from UnitType to avoid
 *  colliding with the type-theoretic UnitType (one-inhabitant type) below. */
struct ModuleType: public Type {
	Frame* interface_children;
	Frame* implementation_children;
	ModuleType(Frame* interface_children, Frame* implementation_children);
};

/** The type-theoretic Unit (one inhabitant). Represents the "return type" of
 *  a Pascal procedure -- procedures do return, they just return no meaningful
 *  value. Not a Pascal-visible type; a shared singleton instance is registered
 *  in the root frame under no Pascal name. Emitted as C++ `void`. */
struct UnitType: public Type {
	UnitType();
};

/** The type of a numeric literal before context pins it to a specific integer
 *  type. Widens to any concrete integer type at conversion cost 0 when the
 *  literal value fits. Shared singleton in the root frame; not registered
 *  under any Pascal name. */
struct UntypedIntegerType: public Type {
	UntypedIntegerType();
};
