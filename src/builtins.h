// Builtins and intrinsics: things that exist in the root Frame before any
// Pascal source is parsed.
//
// Two descriptor tables (defined in builtins.cc) bind each Pascal name to a
// C++ symbol from rtl.h. Adding an entry means appending a row here AND
// implementing the rtl.h symbol; the linker catches a mismatch when emitted
// output is compiled.
//
//   kIntrinsicTypes   - Pascal type names visible at the root (Integer, etc.)
//   kBuiltins         - Pascal procedures/functions visible at the root
//                       (Ord, Inc, Dec, ...)
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include "cst.h"
#include "frame.h"

struct ConstEvalContext;
struct ConstEvalResult;
using BuiltinConstFold = ConstEvalResult (*)(ConstEvalContext& ctx, Type* result_ty, const std::vector<Node*>& args);

enum class BuiltinGenericKind {
	None,
	OrdinalValue,
	// Unary Inc/Dec preserve the exact operand type for every ordinal and
	// pointer type. Pascal declarations cannot quantify that T -> T
	// relationship, so root-frame callables use an omitted formal.
	UnaryOrdinalOrPointerStep,
	// Inc/Dec distance syntax uses Add/Subtract. Numeric and Char cases have
	// ordinary System declarations; only enum and pointer families still need
	// an otherwise-unspellable (T, Integer) -> T root fallback. Restricting
	// this family also prevents a generic candidate from participating in
	// ordinary numeric arithmetic.
	EnumOrPointerStep,
	// Subtracting two compatible pointers is not the step relation above:
	// it has the otherwise-unspellable generic signature
	//
	//   (a, b: ^T) -> PtrInt
	//
	// and measures T elements rather than bytes. The root-frame declaration
	// uses Pointer formals only to give this distinct relation an ordinary
	// overload signature; candidate matching preserves the common typed
	// pointer so the C++ operation retains both element size and provenance.
	PointerDifference,
	// tpcc does not yet support generic Pascal routine declarations, so the
	// ordinary system.pp declaration cannot express the relationship
	//
	//   values: set of T; item: T
	//
	// in system.pp. Include/Exclude therefore use omitted-type formals there,
	// while this semantic category tells call checking that argument one
	// must be a set and argument two must convert to that set's item type.
	SetMutation,
	// System's membership declaration has the equally unspellable relation
	//
	//   item: T; values: set of T
	//
	// but in source order rather than mutation order. Typed custom `operator
	// In` declarations remain ordinary candidates; this category constrains
	// only System's omitted-type fallback and keeps it at Generic rank.
	SetMembership,
	// Set union and difference have the generic relation
	//
	//   (a, b: set of T) -> set of T
	//
	// which current Pascal declarations cannot quantify. The root candidates
	// carry `set of unknown` formals only to form an ordinary overload;
	// candidate matching chooses one real set type and restores it as the
	// omitted result after selection.
	SetUnionOrDifference,
	// Assigned accepts object pointers, plain routine values, and method
	// routine values. system.pp can only spell its Pointer overload.
	Assigned,
	// GetMem's `out Pointer` is explicitly raw pointer storage: the RTL
	// template may write a typed pointer variable without pretending that
	// ordinary typed var/out parameters are covariant.
	PointerStorageOut,
	// Val parses directly into the carrier of an ordinal subrange. This is an
	// intrinsic storage contract, not general var/out covariance: the formal
	// must be the subrange's exact compiler-selected base carrier.
	ValOutput,
	// Pascal declarations cannot spell "any sequence type". Length's omitted
	// formal is therefore accepted only when the actual semantic Type
	// supplies the sequence contract used by indexing and iteration.
	SequenceLength,
	// SetLength has the narrower omitted-type contract "a writable sequence
	// whose logical length can change". The Type owns that property; this tag
	// merely selects the question for the otherwise unexpressible generic
	// formal. For ShortString, logical resize does not change fixed capacity.
	SequenceResize,
};

enum class BuiltinSyntaxKind {
	None,
	SizeOf,
	Write,
	WriteLn,
	NewValue,
	DisposeValue,
};

enum class BuiltinCallConvention {
	Function,
	// The Pascal declaration is a method, but the RTL implementation is a
	// free C++ function whose first argument is the Pascal receiver. This is
	// needed for operations such as TObject.Free: Pascal permits nil.Free,
	// whereas entering a C++ member function through a null pointer is
	// undefined behavior even if the function body checks `this`.
	ReceiverFirst,
};

struct BuiltinDesc {
	std::string_view cxx_name;    // e.g. "::u_system::p_ord"
	BuiltinConstFold const_fold;  // nullptr when this builtin is not foldable
	std::optional<TypeBoundKind> type_bound_kind = {};
	BuiltinGenericKind generic_kind = BuiltinGenericKind::None;
	BuiltinSyntaxKind syntax_kind = BuiltinSyntaxKind::None;
	BuiltinCallConvention call_convention =
	    BuiltinCallConvention::Function;
};

struct IntrinsicTypeDesc {
	std::string_view pas_name;    // lowercase
	std::string_view cxx_name;    // e.g. "::u_system::t_integer"
};

struct OrdinalBounds {
	bool signed_type;
	uint64_t min_magnitude; // only meaningful for signed_type: magnitude of minimum negative value
	uint64_t max_positive;
};

/** Actual C++ scalar type after `using` aliases are resolved on the supported
 * SysV x86-64 target. C++ overload identity uses this carrier, not the alias
 * spelling (`t_integer` and `t_longint`, for example). */
enum class IntrinsicCarrier {
	UInt8,
	Int8,
	UInt16,
	Int16,
	UInt32,
	Int32,
	UInt64,
	Int64,
	Float,
	Double,
	LongDouble,
	Character,
	AnsiString,
	Text,
	File,
};

class IntrinsicType: public Type {
public:
	std::string cxx_name;
	std::optional<int> rank;
	std::optional<OrdinalBounds> ordinal_bounds;
	std::optional<TypeLayout> layout;
	std::optional<IntrinsicCarrier> carrier;
	IntrinsicType(SourceLocation source_location,
	              std::string cxx_name,
	              std::optional<int> rank,
	              std::optional<OrdinalBounds> ordinal_bounds = {},
	              std::optional<TypeLayout> layout = {},
	              std::optional<IntrinsicCarrier> carrier = {});
	const char* diagnostic_kind() const override;
	std::optional<ValueConversion>
	value_conversion_from(const Type* source) const override;
	bool predefined_explicit_conversion_from(
	    const Type* source) const override;
	bool is_subtype_of(const Type* target) const override;
	bool same_cxx_carrier_definition_as(
	    const Type* other) const override;
	Type* sequence_element_type() const override;
	Type* sequence_index_type() const override;
	Type* sequence_length_type() const override;
	bool sequence_is_resizable() const override;
	bool has_managed_lifetime() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Builtin: public Node {
public:
	const BuiltinDesc* desc;
	Builtin(const BuiltinDesc* desc);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

// The single, program-wide root frame. Holds intrinsic types (Integer,
// Boolean, ...) and compiler-provided declarations which Pascal source cannot
// express generically.
// Function-local static: initialized on first call, no cross-TU static-init
// order dependency. Every Parser pushes this frame at the bottom of its
// scope stack, so intrinsic Type* identities are shared across parsers.
const Frame& root_frame();

// Shared singletons for internal-only types not registered under any Pascal
// name. Callers compare by identity (pointer equality) against &unit_type()
// or &untyped_integer_type().
// FIXME: Non-const because the callers store the address
// in Type* fields (Node::ty etc.); the underlying objects have no non-const
// methods, so returning non-const doesn't risk mutation of the singleton.
UnitType& unit_type();
UntypedIntegerType& untyped_integer_type();

// Cached lookups of frequently-referenced intrinsics from root_frame().
Type* byte_type();
Type* shortint_type();
Type* word_type();
Type* smallint_type();
Type* cardinal_type();
Type* integer_type();
Type* longint_type();
Type* sizeint_type();
Type* qword_type();
Type* int64_type();
Type* pointer_type();
Type* ptrint_type();
Type* ptruint_type();
Type* boolean_type();
Type* char_type();
ShortStringType* shortstring_type(uint8_t capacity = 255);
Type* ansistring_type();
Type* text_type();
Type* file_type();
Type* single_type();
Type* double_type();
Type* extended_type();
Type* set_type();
Type* fixedarray_type();
Type* unknown_type();
RecordType* tmethod_type();
StorageSlot* tmethod_code_field();
StorageSlot* tmethod_data_field();

bool intrinsic_ordinal_bounds(Type* ty, OrdinalBounds* out);
bool integer_bounds(const Type* ty, OrdinalBounds* out);
Type* lookup_builtin_type(std::string cxx_name);
const BuiltinDesc* lookup_builtin_desc(std::string_view cxx_name);
Builtin* create_builtin_value(std::string cxx_name);
