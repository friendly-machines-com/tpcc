#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <sstream>
#include <utility>

class Frame;
class Node;
class StorageSlot;
class Property;
class Method;
class ErrorLetContext;
class Unit;

struct TypeLayout {
	uint64_t size;
	uint64_t alignment;
};

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
	// The one FPC-style default indexed property declared by this type.
	// Descendant lookup walks the static type hierarchy when this is null.
	// Built-in indexable types receive a compiler-synthesized property lazily.
	Property* default_property = nullptr;
	// Unit which owns this source-defined named type. Null for anonymous,
	// local, aggregate-member, compiler-builtin, and external carrier types.
	Unit* owning_unit = nullptr;

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

/** Placeholder created internally while parsing one type block. Sources are
 *  implicit RHS forward references (`^TFoo` before TFoo is declared) and LHS
 *  pre-registration for self-recursive definitions. It must be resolved by
 *  type-block end. An explicit Pascal `TFoo = class;` instead publishes an
 *  incomplete ClassType: FPC permits its definition in a later type section,
 *  and intervening declarations already need its class identity. */
struct IncompleteType: public Type {
	std::string name;
	Type* resolved;
	IncompleteType(SourceLocation source_location, std::string name);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct OrdinalRange {
	struct Value {
		bool negative = false;
		uint64_t magnitude = 0;
	};
	Type* index_type = nullptr;
	Type* base_type = nullptr;
	Node* lower_bound = nullptr;
	Node* upper_bound = nullptr;
	Value lower_ordinal;
	Value upper_ordinal;
	uint64_t length = 0;
};

/** Pascal ShortString is one family of inline, length-prefixed byte strings.
 *  `ShortString`, `string` under {$H-}, and `string[255]` all have capacity
 *  255; `string[N]` is the same semantic type constructor with capacity N.
 *  Capacity is payload bytes, so the physical size is capacity + one length
 *  byte. */
struct ShortStringType: public Type {
	uint8_t capacity;
	ShortStringType(SourceLocation source_location, uint8_t capacity);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct FixedArrayType: public Type {
	Type* bounds;
	Type* item_type;
	OrdinalRange range;
	FixedArrayType(SourceLocation source_location, Type* bounds, OrdinalRange range, Type* item_type);
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

/** A Pascal typed binary file, written `file of T`.
 *
 * Untyped `File` is a separate intrinsic type, and text files use the
 * existing Text intrinsic. Keeping this construction distinct prevents any
 * of those three incompatible Pascal file categories from collapsing merely
 * because their runtime handles have the same size.
 */
struct TypedFileType: public Type {
	Type* item_type;
	TypedFileType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

// Canonical constructor for `file of T`. Equivalent element constructions
// (for example, two independently parsed `^Integer` types) share one typed
// file Type*, while incompatible element types remain distinct.
TypedFileType* typed_file_type(
    SourceLocation source_location, Type* item_type);

struct AggregateField {
	std::string pas_name;
	StorageSlot* slot;
	Type* ty;
};

struct VariantPart;

struct VariantArm {
	std::vector<AggregateField> fields;
	// A variant part, when present, is the last item in this arm's field
	// sequence. Its selector follows the preceding fields; its arms then
	// overlap at that new position.
	VariantPart* variant = nullptr;
};

struct VariantPart {
	bool has_selector = false;
	std::string selector_name;
	std::string selector_cxx_name;
	Type* selector_type = nullptr;
	StorageSlot* selector_slot = nullptr;
	std::vector<VariantArm> arms;
};

struct EnumType: public Type {
	// C++ identifier emitted for this enum. Empty until the containing
	// type-block declaration assigns it (parse_type_block).
	std::string cxx_name;
	struct Member {
		std::string pas_name;
		std::string cxx_name;
		int64_t value;
		// True for source forms `Member := constant` and
		// `Member = constant`. The emitter only has to spell those values;
		// following implicit C++ enumerators advance from them naturally.
		bool explicit_value = false;
	};
	// Source order, not ordinal order. Explicit values may jump, decrease, or
	// repeat; emission preserves declaration order while Low/High and enum
	// index ranges query min_member()/max_member().
	std::vector<Member> members;
	const Member* min_member() const;
	const Member* max_member() const;
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
	// Pascal declaration order. Frame remains lookup-only: its map ordering
	// must never influence C++ member emission or layout reconstruction.
	std::vector<AggregateField> fields;

	// Top-level variant part. Pascal allows AT MOST ONE here, declared last
	// in the record body as `case [<sel_name> ':'] <TagType> of <arms>`;
	// each arm may recursively end in another VariantPart:
	//
	//   fixed_field_a: Integer;
	//   fixed_field_b: Real;
	//   case discriminator: Boolean of     // <-- "discriminator" is the
	//                                       //     selector; omitting it
	//                                       //     gives a tag-less variant
	//     false: (uvalue: QWord);
	//     true:  (svalue: Int64);
	//
	// Each variant arm is a sequential C++ struct; one union contains those
	// arm structs, so different arms overlap while fields within an arm retain
	// Pascal declaration order. The selector, when present, is an ordinary
	// field emitted ahead of that union.
	VariantPart* variant = nullptr;

	RecordType(SourceLocation source_location, Frame* children);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** A byte-packed Pascal record.
 *
 * This is deliberately NOT derived from RecordType: ordinary records emit
 * actual C++ fields and use C++ layout, while packed records emit one opaque
 * byte carrier and generated accessors. Keeping the types disjoint prevents
 * an unimplemented packed operation from silently entering ordinary-record
 * lowering through dynamic_cast<RecordType*>.
 */
struct PackedRecordType: public Type {
	Frame* children;
	std::string cxx_name;
	// Pascal source order. Frame is for lookup and intentionally cannot be
	// used for layout because it stores values in name order.
	std::vector<AggregateField> fields;
	VariantPart* variant = nullptr;

	PackedRecordType(SourceLocation source_location, Frame* children);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct AggregateFieldLayout {
	StorageSlot* slot;
	Type* ty;
	uint64_t offset;
	uint64_t size;
};

struct RecordLayout {
	TypeLayout type;
	std::vector<AggregateFieldLayout> fields;
};

// Compiler-side target layout used by SizeOf constant evaluation and by the
// independent assertions emitted for ordinary C++ records.
std::optional<TypeLayout> type_layout(Type* ty);
std::optional<RecordLayout> record_layout(RecordType* record);
std::optional<RecordLayout> packed_record_layout(
    PackedRecordType* record);

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
	// `TFoo = class;` publishes this same ClassType object immediately, so
	// intervening fields/variables can retain stable `TFoo` type identity.
	// The later full declaration fills this object and clears the flag.
	bool is_forward_declaration = false;
	std::string forward_name;
	// A Pascal `class constructor Name` is a lifecycle hook, not a value
	// member named Name. Keeping it out of `children` makes it impossible for
	// ordinary member lookup, calls, or routine references to expose it.
	Method* class_constructor = nullptr;
	// Class destructors are the finalization half of the same lifecycle
	// mechanism. They likewise have no Pascal value identity: the source name
	// exists only to match the declaration with its implementation.
	Method* class_destructor = nullptr;
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
	bool needs_vmt = false;
	ObjectType(SourceLocation source_location, Frame* children, ObjectType* super);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct PointerType: public Type {
	// Null denotes Pascal's builtin untyped Pointer. A non-null item_type
	// denotes the ordinary `^T` construction. Both are pointers semantically;
	// only the untyped form lacks a readable pointee value.
	Type* item_type;
	std::string cxx_name;
	PointerType(SourceLocation source_location, Type* item_type,
	            std::string cxx_name = {});
	bool is_untyped() const { return item_type == nullptr; }
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool is_reference_type() const override { return true; }
};

/** The type of a Pascal `unit X;` module. Renamed from UnitType to avoid
 *  colliding with the type-theoretic UnitType (one-inhabitant type) below. */
struct ModuleType: public Type {
	Frame* children;
	ModuleType(SourceLocation source_location, Frame* children);
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
	CLASS_CONSTRUCTOR,
	CLASS_DESTRUCTOR,
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

// Routine values are structural within one representation category.
// Parameter names/defaults are deliberately excluded; modes, parameter types,
// result type, and plain-vs-of-object category are included.
bool routine_types_compatible(
    const RoutineType* from, const RoutineType* to);

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

// Cost of converting FROM to TO: 0 = same (or Untyped fits), positive =
// implicit conversion, -1 = no implicit conversion. Integer conversions are
// ordered by target range distance so overload resolution can prefer the
// closest fitting ordinal type.
int conversion_cost(Type* from, Type* to);

// A dominates B iff A's cost is <= B's on every position AND strictly < on
// at least one. Different-length vectors don't compare (ambiguity later).
bool dominates(const std::vector<int>& a, const std::vector<int>& b);
