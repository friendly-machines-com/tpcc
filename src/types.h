#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Frame;
class StorageSlot;

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

struct VariantArm {
	struct Field {
		StorageSlot* slot;
		Type* ty;
	};
	std::vector<Field> fields;
};

struct EnumType: public Type {
	// C++ identifier emitted for this enum. Empty until the containing
	// type-block declaration assigns it (parse_type_block).
	std::string cxx_name;
	struct Member {
		std::string cxx_name;
		// FIXME: explicit member values (`Red = 5`) are not parsed yet --
		// every member takes the next sequential value from 0. Add an
		// optional `= <const-expr>` after the member name and evaluate it
		// at type-block end.
		int64_t value;
	};
	// Source order, not sorted -- the default value of member N is N, and
	// emission preserves declaration order.
	std::vector<Member> members;
};

struct RecordType: public Type {
	Frame* children;
	// C++ identifier emitted for this record. Empty until the containing
	// type-block declaration assigns it (parse_type_block).
	std::string cxx_name;
	bool packed = false;

	// Variant part. Pascal allows AT MOST ONE variant part, declared last
	// in the record body as `case [<sel_name> ':'] <TagType> of <arms>`:
	//
	//   fixed_field_a: Integer;
	//   fixed_field_b: Real;
	//   case discriminator: Boolean of     // <-- "discriminator" is the
	//                                       //     selector; omitting it
	//                                       //     gives a tag-less variant
	//     false: (uvalue: QWord);
	//     true:  (svalue: Int64);
	//
	// The variant arms overlap in memory (C++ anon-union). The selector,
	// when present, is just an ordinary field the program can read/write;
	// we emit it as a regular struct member ahead of the union. The arms
	// themselves don't influence layout beyond "these slots overlap".
	bool has_selector = false;
	std::string selector_cxx_name;
	Type* selector_type = nullptr;
	std::vector<VariantArm> arms;

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

enum class ParamMode { Value, Var, Out, Const };

class Node;

struct Parameter {
    std::string pas_name;
    std::string cxx_name;
    Type* ty;
    ParamMode mode;
    Node* default_value; // null if none
    Parameter(std::string pas_name,
              std::string cxx_name,
              Type* ty,
              ParamMode mode,
              Node* default_value)
        : pas_name(std::move(pas_name)),
          cxx_name(std::move(cxx_name)),
          ty(ty),
          mode(mode),
          default_value(default_value) {}
};

enum RoutineKind {
	CONSTRUCTOR,
	DESTRUCTOR,
	METHOD,
	ROUTINE,
	CLASS_METHOD,
};

class RoutineType : public Type {
public:
	std::vector<Parameter> formals;
	Type* return_type;
	RoutineKind kind;

	RoutineType(std::vector<Parameter> formals, Type* return_type, RoutineKind kind);
};

// Result type of an arithmetic/bitwise binary op given operand types. Handles
// UntypedInteger adaptation and integer widening; returns nullptr if the two
// types don't combine (caller decides whether that's an error).
Type* common_arith_type(Type* a, Type* b);

// Cost of converting FROM to TO: 0 = same (or Untyped fits), 1 = widening,
// -1 = no implicit conversion. Used by both call-site coercion and overload
// ranking.
int conversion_cost(Type* from, Type* to);
