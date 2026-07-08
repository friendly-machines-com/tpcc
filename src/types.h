#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <utility>

class Frame;
class StorageSlot;
class ErrorLetContext;

struct SourceLocation {
	std::string file_name;
	// Parser-created locations are 1-based. 0 means the location is unknown or
	// not backed by a Pascal source line, e.g. compiler builtin/internal types.
	int line_number = 0;

	SourceLocation() = default;
	SourceLocation(std::string file_name, int line_number)
	    : file_name(std::move(file_name)), line_number(line_number) {}

	bool operator<(const SourceLocation& other) const {
		if (file_name != other.file_name)
			return file_name < other.file_name;
		return line_number < other.line_number;
	}

	static SourceLocation builtin() { return SourceLocation("<builtin>", 0); }
	static SourceLocation internal() { return SourceLocation("<internal>", 0); }
};

class Type {
public:
	SourceLocation source_location;

	explicit Type(SourceLocation source_location);
	virtual ~Type() = default;
	virtual const char* diagnostic_kind() const = 0;
	virtual void collect_diagnostic_edges(ErrorLetContext* ctx) const = 0;
	// ErrorLetContext prints diagnostic_kind() as the type-definition head.
	// These methods append only kind-specific details after that head; they must
	// not repeat the head.
	virtual void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const = 0;
	virtual void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	// True iff a variable of this type is represented in C++ emission as a
	// pointer (i.e. emission in storage position is `t_foo*`, member access
	// uses `->`, `nil` is a legal value). Pascal `class` and `interface` are
	// reference types like user `^T`; `record` and `object` are not.
	virtual bool is_reference_type() const { return false; }
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
	IncompleteType(SourceLocation source_location, std::string name);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct FixedArrayType: public Type {
	Type* bounds;
	Type* item_type;
	FixedArrayType(SourceLocation source_location, Type* bounds, Type* item_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct FixedSetType: public Type {
	Type* item_type;
	FixedSetType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct VariantArm {
	struct Field {
		std::string pas_name;
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
		std::string pas_name;
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
	EnumType(SourceLocation source_location, std::string cxx_name, std::string a, std::string b);
	EnumType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
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
	std::string selector_name;
	std::string selector_cxx_name;
	Type* selector_type = nullptr;
	std::vector<VariantArm> arms;

	RecordType(SourceLocation source_location, Frame* children, bool packed);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct InterfaceType: public Type {
	Frame* children;
	std::string cxx_name;
	std::vector<InterfaceType*> super_interfaces; // FIXME: not transitive ?
	InterfaceType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> super_interfaces);
	InterfaceType(SourceLocation source_location, std::string cxx_name, Frame* children, std::vector<InterfaceType*> super_interfaces);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool is_reference_type() const override { return true; }
};

struct ClassType: public Type {
	Frame* children;
	std::string cxx_name;
	std::vector<InterfaceType*> implemented_interfaces; // FIXME: not transitive ?
	ClassType* super;
	ClassType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> implemented_interfaces, ClassType* super);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool is_reference_type() const override { return true; }
};

struct ClassRefType : public Type // metaclass
{
	Type* target; // ClassType or IncompleteType
	std::string cxx_name;

	explicit ClassRefType(SourceLocation source_location, Type* c)
	   : Type(std::move(source_location)), target(c) {
	}
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool is_reference_type() const override { return true; }
};

struct ObjectType: public Type {
	Frame* children;
	std::string cxx_name;
	ObjectType* super;
	ObjectType(SourceLocation source_location, Frame* children, ObjectType* super);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct PointerType: public Type {
	Type* item_type;
	PointerType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool is_reference_type() const override { return true; }
};

/** The type of a Pascal `unit X;` module. Renamed from UnitType to avoid
 *  colliding with the type-theoretic UnitType (one-inhabitant type) below. */
struct ModuleType: public Type {
	Frame* interface_children;
	Frame* implementation_children;
	ModuleType(SourceLocation source_location, Frame* interface_children, Frame* implementation_children);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** The type-theoretic Unit (one inhabitant). Represents the "return type" of
 *  a Pascal procedure -- procedures do return, they just return no meaningful
 *  value. Not a Pascal-visible type; a shared singleton instance is registered
 *  in the root frame under no Pascal name. Emitted as C++ `void`. */
struct UnitType: public Type {
	UnitType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** The type of a numeric literal before context pins it to a specific integer
 *  type. Widens to any concrete integer type at conversion cost 0 when the
 *  literal value fits. Shared singleton in the root frame; not registered
 *  under any Pascal name. */
struct UntypedIntegerType: public Type {
	UntypedIntegerType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
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

	RoutineType(SourceLocation source_location, std::vector<Parameter> formals, Type* return_type, RoutineKind kind);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class SubrangeType : public Type {
public:
	Node* lower_bound; // its type is base_type
	Node* upper_bound; // its type is base_type
	Type* base_type; /* NOT a subrange type */

	SubrangeType(SourceLocation source_location, Type* base_type, Node* lower_bound, Node* upper_bound);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

// Result type of an arithmetic/bitwise binary op given operand types. Handles
// UntypedInteger adaptation and integer widening; returns nullptr if the two
// types don't combine (caller decides whether that's an error).
Type* common_arith_type(Type* a, Type* b);

// Cost of converting FROM to TO: 0 = same (or Untyped fits), 1 = widening,
// -1 = no implicit conversion. Used by both call-site coercion and overload
// ranking.
int conversion_cost(Type* from, Type* to);

// A dominates B iff A's cost is <= B's on every position AND strictly < on
// at least one. Different-length vectors don't compare (ambiguity later).
bool dominates(const std::vector<int>& a, const std::vector<int>& b);
