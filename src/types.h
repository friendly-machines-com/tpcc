#pragma once
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Frame;
class Node;
class StorageSlot;
class Property;
class Method;
class ErrorLetContext;
class Unit;
class Type;

struct TypeLayout {
	uint64_t size;
	uint64_t alignment;
};

/** Storage description for a decimal fixed-point Pascal domain.
 *
 * This describes representation and exact-origin materialization only. It
 * deliberately does not imply assignment or operator availability: those
 * language edges still have to be declared in Pascal. */
struct FixedDecimalFormat {
	Type* raw_type;
	unsigned decimal_scale;
};

enum class ValueConversionClass {
	// Distinct Pascal definitions whose constructor explicitly permits a
	// representation-preserving value match. This is not type identity.
	Direct,
	// A value-changing or representation-adjusting implicit conversion.
	Convert,
};

struct ValueConversion {
	ValueConversionClass kind;
	unsigned distance = 0;
};

/** One predefined value-assignment relation, classified for overload
 * selection as well as fixed-destination storage. Exact Pascal Type*
 * identity and expression-dependent contextual matches are handled by the
 * parser before this type-only query. */
enum class AssignmentConversionClass {
	Equal,
	Widening,
	Narrowing,
};

struct AssignmentConversion {
	AssignmentConversionClass kind;
	unsigned distance = 0;
};

struct SourceLocation {
	std::string file_name;
	// Parser-created locations are 1-based. 0 means the location is unknown or
	// not backed by a Pascal source line, e.g. compiler builtin/internal types.
	int line_number = 0;

	SourceLocation() = default;

	SourceLocation(std::string file_name, int line_number) : file_name(std::move(file_name)), line_number(line_number) {
	}

	bool operator<(const SourceLocation& other) const {
		if (file_name != other.file_name) {
			return file_name < other.file_name;
		}
		return line_number < other.line_number;
	}

	static SourceLocation builtin() {
		return SourceLocation("<builtin>", 0);
	}

	static SourceLocation internal() {
		return SourceLocation("<internal>", 0);
	}
};

class Type {
      public:
	SourceLocation source_location;
	std::optional<FixedDecimalFormat> fixed_decimal_format;
	// The one FPC-style default indexed property declared by this type.
	// Descendant lookup walks the static type hierarchy when this is null.
	// Built-in indexable types receive a compiler-synthesized property lazily.
	Property* default_property = nullptr;
	// Unit which owns this source-defined named type. Null for anonymous,
	// local, aggregate-member, compiler-builtin, and external carrier types.
	Unit* owning_unit = nullptr;

	explicit Type(SourceLocation source_location);
	virtual ~Type() = default;
	/** Diagnostic graph invariant: every Type* or Node* rendered by
	 * print_diagnostic_definition() through known_*_ref() must be contributed
	 * by collect_diagnostic_edges(). Frame traversal is only naming evidence. */
	virtual const char* diagnostic_kind() const = 0;
	virtual void collect_diagnostic_edges(ErrorLetContext* ctx) const = 0;
	// ErrorLetContext prints diagnostic_kind() as the type-definition head.
	// These methods append only kind-specific details after that head; they must
	// not repeat the head.
	virtual void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const = 0;
	virtual void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	/** Pascal type identity is exactly Type* identity. These two legacy
	 * relations implement the ordinary and additional narrowing portions of
	 * predefined assignment. New semantic callers use
	 * assignment_conversion_from(), which classifies their union; keeping the
	 * lower-level split here lets individual types describe the extra
	 * destination operations without giving calls and stores different
	 * assignment languages. Explicit casts have a separate relation below.
	 * Subtyping is a
	 * reflexive/transitive preorder: distinct definitions such as two
	 * occurrences of 1..10 can be mutual subtypes without becoming identical.
	 * Typed var/out matching, routine identity, overload ranking, and C++
	 * carrier identity must not substitute either relation for their own
	 * rules. */
	virtual std::optional<ValueConversion> value_conversion_from(const Type* source) const;
	/** Additional predefined assignment operations, including narrowing.
	 * Implementations conventionally return value_conversion_from() first so
	 * this is the complete legacy destination relation. */
	virtual std::optional<ValueConversion> destination_conversion_from(const Type* source) const;
	/** Complete predefined assignment compatibility and its overload quality.
	 * This is the type-level relation shared by fixed destinations and value
	 * formals. Declared implicit conversion operators and expression-specific
	 * contextual construction are layered around it by the parser. */
	std::optional<AssignmentConversion> assignment_conversion_from(const Type* source) const;
	/** Whether source syntax `ThisType(value)` has one predefined direct
	 * conversion edge after source-defined Explicit/Implicit contracts have
	 * failed. This is deliberately separate from assignment compatibility:
	 * explicit syntax additionally admits representation operations such as
	 * ordinal casts, related downcasts, pointer crossings, and packed
	 * overlays. Implementations must inspect only SOURCE and this
	 * destination; applying another conversion first would turn the language
	 * into an accidental A -> B -> C conversion search. */
	virtual bool predefined_explicit_conversion_from(const Type* source) const;
	virtual bool is_subtype_of(const Type* target) const;
	/** Exact contract identity for a type written directly in a routine
	 * formal. Most Pascal types use definition identity. Open arrays override
	 * this because separately parsed `array of T` formals describe the same
	 * call contract when their element Type* is identical, even though neither
	 * occurrence is a storable Pascal type. */
	virtual bool same_formal_contract_as(const Type* other) const;

	/** Sequence operations are properties of the Pascal semantic type, not of
	 * a C++ spelling recognized by the parser. A null element type means this
	 * type does not support indexing/Length or built-in sequence iteration. */
	virtual Type* sequence_element_type() const {
		return nullptr;
	}

	// Non-null only for actual array constructors. Strings are sequences but
	// do not become open-array storage merely because their elements are
	// characters.
	virtual Type* array_element_type() const {
		return nullptr;
	}

	virtual Type* sequence_index_type() const {
		return nullptr;
	}

	virtual Type* sequence_length_type() const {
		return nullptr;
	}

	virtual bool sequence_is_resizable() const {
		return false;
	}

	// True when ordinary C++ construction, copy, or destruction participates
	// in the Pascal value's lifetime. Bytewise packed-field projection cannot
	// safely manufacture such a value by memcpy.
	virtual bool has_managed_lifetime() const {
		return false;
	}

	/** Whether a value of this type contains Pascal file state.
	 *
	 * ISO 7185 permits assignment/value-parameter copying only for types
	 * which are permissible as a file component; a file, or a structured
	 * value containing one recursively, is not.  Keep that semantic property
	 * separate from backend carrier layout so a pointer-sized file handle
	 * cannot accidentally become copyable merely because C++ can copy its
	 * bytes. */
	virtual bool contains_file_state() const {
		return false;
	}

	/** Whether this type and OTHER have the same C++ type spelling.
	 * This backend equivalence never participates in Pascal lookup,
	 * conversion, var/out matching, or signature identity; it exists to
	 * diagnose source overloads the current C++ lowering cannot represent.
	 * A concrete subrange definition has its own C++ carrier and therefore
	 * reaches this relation by ordinary Type* identity like a record or enum. */
	bool same_cxx_carrier_as(const Type* other) const;
	/** Constructor-specific half of same_cxx_carrier_as(). The public wrapper
	 * first handles Type* identity; each remaining type constructor describes
	 * only its own C++ carrier. */
	virtual bool same_cxx_carrier_definition_as(const Type* other) const;

	// True iff a variable of this type is represented in C++ emission as a
	// pointer (i.e. emission in storage position is `t_foo*`, member access
	// uses `->`, `nil` is a legal value). Pascal `class` and `interface` are
	// reference types like user `^T`; `record` and `object` are not.
	virtual bool is_reference_type() const {
		return false;
	}
};

/** Placeholder created internally while parsing one type block. Sources are
 *  implicit RHS forward references (`^TFoo` before TFoo is declared) and LHS
 *  pre-registration for self-recursive definitions. It must be resolved by
 *  type-block end. An explicit Pascal `TFoo = class;` instead publishes an
 *  incomplete ClassType: FPC permits its definition in a later type section,
 *  and intervening declarations already need its class identity. */
struct IncompleteType : public Type {
	std::string name;
	Type* resolved;
	IncompleteType(SourceLocation source_location, std::string name);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** FPC `T = type Base`: a fresh Pascal identity over Base's representation.
 *
 * This is not an ordinary alias: exact overload matching observes this
 * Type*. It is also not an opaque newtype: Pascal assignment and var/out
 * permit the base representation in both directions, and predefined
 * operations use Base's family. The C++ backend therefore spells a named
 * `using` for the shared carrier while retaining this separate semantic node.
 */
struct DistinctType : public Type {
	std::string cxx_name;
	Type* base_type;

	DistinctType(SourceLocation source_location, std::string cxx_name, Type* base_type);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	std::optional<ValueConversion> destination_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
	Type* sequence_element_type() const override;
	Type* array_element_type() const override;
	Type* sequence_index_type() const override;
	Type* sequence_length_type() const override;
	bool sequence_is_resizable() const override;
	bool has_managed_lifetime() const override;
	bool contains_file_state() const override;
	bool is_reference_type() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Strip one or more FPC `type Base` identities to their shared storage type.
 * This is representation inspection, not a conversion search. */
Type* distinct_storage_type(Type* type);
const Type* distinct_storage_type(const Type* type);
const FixedDecimalFormat* fixed_decimal_format(const Type* type);
std::optional<uint64_t> fixed_decimal_scale_factor(const Type* type);

/** One exact value in a Pascal ordinal domain. Signed magnitude keeps every
 * predefined 64-bit integer endpoint representable without using a wider host
 * integer, and it is also the value representation used for Char and enum
 * constants after their semantic type has been recorded separately. */
struct OrdinalValue {
	bool negative = false;
	uint64_t magnitude = 0;
};

OrdinalValue ordinal_value(bool negative, uint64_t magnitude);
OrdinalValue ordinal_value(int64_t value);
int compare_ordinal_values(OrdinalValue left, OrdinalValue right);

enum class OrdinalFamily {
	Integer,
	Character,
	Enumeration,
};

/** Semantic ordinal classification of TYPE. nominal_root is Char or the
 * defining EnumType for nominal ordinal families, and null for the predefined
 * integer family. The caller retains the original Type* when exact identity
 * matters; this query only answers the domain-family question. */
struct OrdinalTypeDomain {
	OrdinalFamily family;
	Type* nominal_root = nullptr;
};

std::optional<OrdinalTypeDomain> ordinal_type_domain(Type* type);

struct OrdinalRange {
	Type* index_type = nullptr;
	Type* base_type = nullptr;
	Node* lower_bound = nullptr;
	Node* upper_bound = nullptr;
	OrdinalValue lower_ordinal;
	OrdinalValue upper_ordinal;
	uint64_t length = 0;
};

/** Pascal ShortString is one family of inline, length-prefixed byte strings.
 *  `ShortString`, `string` under {$H-}, and `string[255]` all have capacity
 *  255; `string[N]` is the same semantic type constructor with capacity N.
 *  Capacity is payload bytes, so the physical size is capacity + one length
 *  byte. */
struct ShortStringType : public Type {
	uint8_t capacity;
	ShortStringType(SourceLocation source_location, uint8_t capacity);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	std::optional<ValueConversion> destination_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
	Type* sequence_element_type() const override;
	Type* sequence_index_type() const override;
	Type* sequence_length_type() const override;

	bool sequence_is_resizable() const override {
		// SetLength changes the logical length stored in byte zero; it does
		// not change this type's compile-time payload capacity.
		return true;
	}

	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct FixedArrayType : public Type {
	Type* bounds;
	Type* item_type;
	OrdinalRange range;
	FixedArrayType(SourceLocation source_location, Type* bounds, OrdinalRange range, Type* item_type);
	const char* diagnostic_kind() const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;

	Type* sequence_element_type() const override {
		return item_type;
	}

	Type* array_element_type() const override {
		return item_type;
	}

	Type* sequence_index_type() const override {
		return range.base_type;
	}

	Type* sequence_length_type() const override;
	bool has_managed_lifetime() const override;
	bool contains_file_state() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** A managed `array of T` value. Every constructor occurrence is a distinct
 * Pascal definition; item-carrier equality below is backend erasure only. */
struct DynamicArrayType : public Type {
	Type* item_type;
	DynamicArrayType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;

	Type* sequence_element_type() const override {
		return item_type;
	}

	Type* array_element_type() const override {
		return item_type;
	}

	Type* sequence_index_type() const override;
	Type* sequence_length_type() const override;

	bool sequence_is_resizable() const override {
		return true;
	}

	bool has_managed_lifetime() const override {
		return true;
	}

	bool contains_file_state() const override {
		return item_type && item_type->contains_file_state();
	}

	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** A directly written formal `array of T`. It is a non-owning call contract,
 * never a storage or result type. Separate occurrences correspond by exact
 * element Type* rather than by their own generative identities. */
struct OpenArrayType : public Type {
	Type* item_type;
	OpenArrayType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	bool same_formal_contract_as(const Type* other) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;

	Type* sequence_element_type() const override {
		return item_type;
	}

	Type* array_element_type() const override {
		return item_type;
	}

	Type* sequence_index_type() const override;
	Type* sequence_length_type() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct FixedSetType : public Type {
	Type* item_type;
	FixedSetType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
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
struct TypedFileType : public Type {
	Type* item_type;
	TypedFileType(SourceLocation source_location, Type* item_type);
	const char* diagnostic_kind() const override;

	bool has_managed_lifetime() const override {
		return true;
	}

	bool contains_file_state() const override {
		return true;
	}

	bool same_cxx_carrier_definition_as(const Type* other) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

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

struct EnumType : public Type {
	// C++ identifier emitted for this enum. Empty until the containing
	// type-block declaration assigns it (parse_type_block).
	std::string cxx_name;
	// Ordinal representation shared by layout, explicit-cast folding, and C++
	// enum emission. Source enums use signed 32-bit storage; predefined enums
	// such as Boolean may specify another fixed carrier.
	unsigned carrier_bits = 32;
	bool carrier_signed = true;

	struct Member {
		// Normalized Pascal lookup spelling. Enum text formatting uses
		// display_name because identifier lookup is case-insensitive while
		// the string returned by Str is observable data.
		std::string pas_name;
		std::string display_name;
		std::string cxx_name;
		int64_t value;
		// True for source forms `Member := constant` and
		// `Member = constant`. The emitter only has to spell those values;
		// following implicit C++ enumerators advance from them naturally.
		bool explicit_value = false;
	};

      private:
	// Source order, not ordinal order. Explicit values may jump or decrease,
	// but one ordinal always denotes exactly one declared member.
	std::vector<Member> member_list;
	std::unordered_map<int64_t, std::size_t> member_index_by_value;

      public:
	// Returns the existing member on an ordinal collision; otherwise inserts
	// MEMBER and returns null. Keeping construction behind this operation
	// makes uniqueness an EnumType invariant rather than a parser convention.
	const Member* add_member(Member member);
	const Member* member_for_value(int64_t value) const;

	const std::vector<Member>& members() const {
		return member_list;
	}

	const Member* min_member() const;
	const Member* max_member() const;
	EnumType(SourceLocation source_location, std::string cxx_name, std::string a, std::string b, unsigned carrier_bits = 32, bool carrier_signed = true);
	EnumType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

// Return the nominal enum supplying the value names for TYPE. Ordinary aliases
// already share their Type*; this additionally peels strong storage aliases
// and enum subranges so every enum consumer uses one definition of the domain.
EnumType* enum_root_type(Type* type);

struct RecordType : public Type {
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
	bool has_managed_lifetime() const override;
	bool contains_file_state() const override;
};

/** A byte-packed Pascal record.
 *
 * This is deliberately NOT derived from RecordType: ordinary records emit
 * actual C++ fields and use C++ layout, while packed records emit one opaque
 * byte carrier and generated accessors. Keeping the types disjoint prevents
 * an unimplemented packed operation from silently entering ordinary-record
 * lowering through dynamic_cast<RecordType*>.
 */
struct PackedRecordType : public Type {
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
	bool has_managed_lifetime() const override;
	bool contains_file_state() const override;
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
// PACKED_CONTAINER is whether the use of the type is inside a packed container.
std::optional<TypeLayout> type_layout(bool packed_container, Type* ty);
std::optional<RecordLayout> record_layout(RecordType* record);
std::optional<RecordLayout> packed_record_layout(PackedRecordType* record);
// Pascal permits an equal-sized fixed array of exactly Byte to view a
// trivially copyable scalar's object representation. This is a directed
// storage-view relation, not a general equal-layout value conversion.
bool predefined_byte_array_storage_view(const Type* target, const Type* source);
// A cast of a class variable of type S to class T, where S is a subtype of T,
// re-views one pointer-sized object handle and materializes no new value, so
// the view aliases the operand's storage. Narrowing or unrelated casts are
// not a storage view.
bool class_widening_storage_view(const Type* target, const Type* source);

struct InterfaceType : public Type {
	Frame* children;
	std::string cxx_name;
	std::vector<InterfaceType*> super_interfaces; // FIXME: not transitive ?
	// Source spelling from the optional `['...']` clause. It is metadata for
	// interface-to-GUID/string conversions, not part of the C++ carrier.
	std::optional<std::string> guid_literal;
	InterfaceType(SourceLocation source_location, Frame* children, std::vector<InterfaceType*> super_interfaces);
	InterfaceType(SourceLocation source_location, std::string cxx_name, Frame* children, std::vector<InterfaceType*> super_interfaces);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;

	bool is_reference_type() const override {
		return true;
	}
};

struct ClassType : public Type {
	Frame* children;
	std::string cxx_name;
	std::vector<InterfaceType*> implemented_interfaces; // FIXME: not transitive ?
	ClassType* super;
	// `TFoo = class;` publishes this same ClassType object immediately, so
	// intervening fields/variables can retain stable `TFoo` type identity.
	// The later full declaration fills this object and clears the flag.
	bool is_forward_declaration = false;
	std::string forward_name;
	// Source-level `class abstract` marker. Native FPC uses this only for a
	// controllable warning on direct exact-class construction; it does not
	// alter the VMT, methods, C++ carrier, or descendant class flags.
	bool is_abstract = false;
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
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;

	bool is_reference_type() const override {
		return true;
	}
};

struct ClassRefType : public Type // metaclass
{
	// May be an IncompleteType only while the containing type block is open;
	// TypeBlockResolver replaces it with a ClassType before later semantics.
	Type* target;
	std::string cxx_name;

	explicit ClassRefType(SourceLocation source_location, Type* c) : Type(std::move(source_location)), target(c) {
	}

	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;

	bool is_reference_type() const override {
		return true;
	}
};

struct ObjectType : public Type {
	Frame* children;
	std::string cxx_name;
	ObjectType* super;
	bool needs_vmt = false;
	// Source-order aggregate-member fields, mirroring RecordType::fields. The
	// Frame lookup table's std::map ordering must not drive C++ member emission
	// or layout reconstruction (see RecordType::fields for the same rule).
	// Sometimes objects are used as fields inside (non-packed) records.
	std::vector<AggregateField> fields;

	ObjectType(SourceLocation source_location, Frame* children, ObjectType* super);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	bool has_managed_lifetime() const override;
	bool contains_file_state() const override;
};

struct PointerType : public Type {
	// Null denotes Pascal's builtin untyped Pointer. A non-null item_type
	// denotes the ordinary `^T` construction. Both are pointers semantically;
	// only the untyped form lacks a readable pointee value.
	Type* item_type;
	std::string cxx_name;
	PointerType(SourceLocation source_location, Type* item_type, std::string cxx_name = {});

	bool is_untyped() const {
		return item_type == nullptr;
	}

	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;

	bool is_reference_type() const override {
		return true;
	}
};

/** The type of a Pascal `unit X;` module. Renamed from UnitType to avoid
 *  colliding with the type-theoretic UnitType (one-inhabitant type) below. */
struct ModuleType : public Type {
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
struct UnitType : public Type {
	UnitType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** The type of a numeric literal before context pins it to a specific integer
 *  type. The expression matcher retains the signed magnitude, rejects
 *  destinations which cannot contain it, and ranks fitting destinations from
 *  its Delphi natural constant type. Shared singleton in the root frame; not
 *  registered under any Pascal name. */
struct UntypedIntegerType : public Type {
	UntypedIntegerType(SourceLocation source_location);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Exact decimal real origin before a context selects a concrete Pascal real
 *  domain. This type is internal, has no storage layout or C++ carrier, and is
 *  never registered under a Pascal identifier. */
struct UntypedRealType : public Type {
	UntypedRealType(SourceLocation source_location);
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

	Parameter(std::string pas_name, std::string cxx_name, Type* ty, ParamMode mode, Node* default_value) : pas_name(std::move(pas_name)), cxx_name(std::move(cxx_name)), ty(ty), mode(mode), default_value(default_value) {
	}
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
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	/** Names, defaults, and the result type are excluded. Parameter modes and
	 * source-visible type contracts are exact; subranges retain their
	 * established overload identity through their base ordinal type. */
	bool same_parameter_types_as(const RoutineType* other) const;
	/** Names and default expressions are not part of a routine's type.
	 * Parameter modes and Type* identities are; the result is exact as well.
	 * This shape helper deliberately leaves the representation category to
	 * the caller, because a static class-owned Method has receiverless ROUTINE
	 * ABI while its declaration is still class-owned. */
	bool same_parameter_and_result_types_as(const RoutineType* other) const;
	/** Exact routine-type signature, including plain versus receiver-bearing
	 * representation. The compiler currently has one Pascal calling
	 * convention; when conventions become source-visible they belong here. */
	bool same_signature_as(const RoutineType* other) const;
	/** Whether two declarations under one already-selected Pascal name have
	 * the same overload key. Delphi overload identity uses only the exact
	 * Type* list of source-visible parameters: modes, defaults, result,
	 * receiver category, and constructor/method kind do not distinguish it. */
	bool same_overload_signature_as(const RoutineType* other) const;
	/** Directional routine-value compatibility. CLASS_METHOD declarations
	 * become receiver-bearing METHOD values once bound; no other declaration
	 * category is silently reclassified. */
	bool accepts_routine_value_from(const RoutineType* source) const;
	/** Compatibility for a user-written routine-value cast. Ordinary
	 * assignment remains exact. An explicit cast may additionally replace a
	 * by-value data-pointer formal with another data-pointer type when the
	 * result, arity, modes, and plain-versus-bound representation remain exact.
	 * The backend documents this deliberately ABI-level operation separately
	 * because calling the retyped code pointer is not portable ISO C++20. */
	bool accepts_explicit_routine_cast_from(const RoutineType* source) const;
	/** C++ overload identity ignores a function result and does include the
	 * adjusted parameter carriers. Pascal has already selected by its own
	 * signatures before this backend-only collision check runs. */
	bool same_cxx_parameter_list_as(const RoutineType* other) const;
	bool same_cxx_carrier_definition_as(const Type* other) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class SubrangeType : public Type {
      public:
	// Canonical compiler-private C++ tag for this generative Pascal
	// definition. Pascal aliases retain this tag rather than creating another
	// carrier, exactly as aliases of named records retain one RecordType.
	std::string cxx_name;
	Node* lower_bound; // its type is base_type
	Node* upper_bound; // its type is base_type
	Type* base_type;   /* NOT a subrange type */

	SubrangeType(SourceLocation source_location, std::string cxx_name, Type* base_type, Node* lower_bound, Node* upper_bound);
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion> value_conversion_from(const Type* source) const override;
	std::optional<ValueConversion> destination_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	bool same_formal_contract_as(const Type* other) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
	void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};
