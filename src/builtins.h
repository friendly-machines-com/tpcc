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
	OrdinalMutation,
	// tpcc does not yet support generic Pascal routine declarations, so the
	// ordinary system.pp declaration cannot express the relationship
	//
	//   values: set of T; item: T
	//
	// in system.pp. Include/Exclude therefore use omitted-type formals there,
	// while this semantic category tells call checking that argument one
	// must be a set and argument two must convert to that set's item type.
	SetMutation,
	// Assigned accepts object pointers, plain routine values, and method
	// routine values. system.pp can only spell its Pointer overload.
	Assigned,
};

enum class BuiltinSyntaxKind {
	None,
	SizeOf,
	Write,
	WriteLn,
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

class IntrinsicType: public Type {
public:
	std::string cxx_name;
	std::optional<int> rank;
	std::optional<OrdinalBounds> ordinal_bounds;
	std::optional<TypeLayout> layout;
	IntrinsicType(SourceLocation source_location,
	              std::string cxx_name,
	              std::optional<int> rank,
	              std::optional<OrdinalBounds> ordinal_bounds = {},
	              std::optional<TypeLayout> layout = {});
	const char* diagnostic_kind() const override;
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
// Boolean, ...) and builtin procedures/functions (Ord, Inc, Dec, ...).
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
bool integer_bounds(Type* ty, OrdinalBounds* out);
Type* lookup_builtin_type(std::string cxx_name);
const BuiltinDesc* lookup_builtin_desc(std::string_view cxx_name);
Builtin* create_builtin_value(std::string cxx_name);
