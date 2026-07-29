#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

class Type;
class Frame;
class RoutineType;
class Callable;
class Method;
class StorageSlot;
class Unit;
struct ClassType;
class ErrorLetContext;
struct BuiltinDesc;
struct ConstEvalContext;
struct ConstEvalResult;

enum class TypeBoundKind { Low, High };

class Node {
public:
	/** Diagnostic graph invariant: every Type* or Node* rendered by
	 * print_diagnostic_definition() through known_*_ref() must be contributed
	 * by collect_diagnostic_edges(). Frames and parser scopes provide names
	 * only; they never make an otherwise unrelated value printable. */
	virtual const char* diagnostic_kind() const;
	virtual void collect_diagnostic_edges(ErrorLetContext* ctx) const;
	virtual void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	virtual void print_diagnostic_stub(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const;
	virtual ConstEvalResult const_eval(ConstEvalContext& ctx) const;
	// Result type of the value this node produces. Filled by the Parser at
	// construction time; readers (emit, checker, evaluator) treat it as the
	// single source of truth. Null on statement nodes (Block, Assign,
	// Mutation, Return) -- those don't have a value type.
	Type* ty = nullptr;
	// Non-null only for declarations owned directly by a Pascal unit.
	// Aggregate members and routine locals are qualified through their
	// receiver/lexical context instead. External declarations keep this null
	// because their supplied C++ spelling already names the provider.
	Unit* owning_unit = nullptr;
	virtual ~Node() = default;
	std::string str() const;
};

class Block: public Node {
public:
	std::vector<Node*> statements;
	void add(Node* stmt);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Symbol: public Node {
private:
	std::string text;
public:
	Symbol(std::string text);
	std::string str() const;
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class UnaryOperation: public Node {
public:
       Node* a;
       UnaryOperation(Node* a);
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class BinaryOperation: public Node {
public:
       Node* a;
       Node* b;
       BinaryOperation(Node* a, Node* b);
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Compiler-generated sequencing expression. Evaluate `a` for its effects,
 *  discard its value, then evaluate and yield `b`. Pascal has no source comma
 *  operator; this node preserves evaluation required by constructs whose
 *  qualifier selects a declaration but is not part of that declaration's
 *  runtime ABI. */
class EvaluateThen: public BinaryOperation {
public:
	EvaluateThen(Node* a, Node* b);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
};

class ProcCall: public Node {
public:
	// null for standalone calls; the receiver expression for calls whose
	// callable carries a Self (methods, and later `procedure of object` runtime
	// values).
	Node* receiver;
	// Any expression that yields a callable at compile time or at runtime.
	// Compile-time: a Callable*. Runtime: a routine value.
	Node* callee;
	// Optional call-site implementation selected after ordinary Pascal lookup.
	// The callee remains the source declaration for identity and diagnostics;
	// this descriptor exists only for finite compiler-owned operations whose
	// one Pascal declaration has caller-directive checked/unchecked lowering.
	const BuiltinDesc* lowering_builtin_desc = nullptr;
	std::vector<Node*> args;
	ProcCall(Node* receiver, Node* callee, std::vector<Node*> args);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** A class name used as a value, as opposed to the same identifier in a type
 * position. `target` is the exact Pascal class, `ty` is `class of target`,
 * and emission obtains its one stable metaclass object through
 * `target::p_classtype()`. */
class ClassRefValue: public Node {
public:
	ClassType* target;
	explicit ClassRefValue(ClassType* target);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** A non-class aggregate type used only to select a static member, as in
 *  `TRecord.StaticMethod`. Unlike ClassRefValue this is not a Pascal runtime
 *  value and must disappear when the selected static operation is formed. */
class TypeMemberQualifier: public Node {
public:
	Type* target;
	explicit TypeMemberQualifier(Type* target);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Allocation plus one ordinary Pascal constructor initializer invocation.
 *  The class-reference receiver determines the allocated/result class;
 *  `initializer` remains a Unit-returning object method and may have been
 *  declared by an ancestor. */
class Construct: public Node {
public:
	Node* class_reference;
	Method* initializer;
	std::vector<Node*> args;
	Construct(Node* class_reference, Method* initializer,
	          std::vector<Node*> args, ClassType* result_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Typed-pointer allocation, optionally followed by one old-style object
 *  constructor invocation. Unlike Construct, the result is `^T`, not a
 *  Pascal class reference, and the exact pointee type determines storage. */
class NewValue: public Node {
public:
	Type* allocated_type;
	Method* initializer;
	std::vector<Node*> args;
	NewValue(Type* pointer_type, Type* allocated_type,
	         Method* initializer, std::vector<Node*> args);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Typed-pointer disposal, optionally preceded by one old-style object
 *  destructor invocation. Pascal destruction and C++ carrier deletion remain
 *  separate operations; direct calls of the destructor do not free storage. */
class DisposeValue: public Node {
public:
	Node* pointer;
	Method* finalizer;
	DisposeValue(Node* pointer, Method* finalizer);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** `Fail` in an ordinary Pascal constructor. Emission uses private
 *  constructor-control flow which is intercepted by allocation/application
 *  boundaries, never by a Pascal `except` handler. */
class ConstructorFail: public Node {
public:
	ConstructorFail();
	const char* diagnostic_kind() const override;
};

/** The canonical designator for a Pascal unit environment. It is neither a
 * runtime value nor a type. MemberAccess uses it as the base for the same
 * `base.member` representation used by records, objects, and class
 * references; emission lowers this base to the unit's static C++ namespace. */
class UnitRef: public Node {
public:
	Unit* unit;
	explicit UnitRef(Unit* unit);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** The shared semantic representation of Pascal's formatted-value grammar:
 *
 *      value [ : width [ : precision ] ]
 *
 * Write/WriteLn accept a sequence of these; Str accepts exactly one before
 * its destination. */
struct FormattedValue {
	Node* value;
	Node* width;
	Node* precision;
};

/** One Pascal Write/WriteLn invocation. These routines have compiler grammar,
 * not an ordinary RoutineType signature: the optional first Text argument and
 * every formatted value must remain grouped. */
class WriteCall: public Node {
public:
	bool newline;
	Node* file;
	// Write has special grammar rather than a RoutineType, but its one source
	// declaration still selects one complete checked or unchecked RTL entry
	// point at the leading token just like an ordinary direct builtin call.
	const BuiltinDesc* lowering_builtin_desc;
	std::vector<FormattedValue> items;

	WriteCall(
	    bool newline, Node* file,
	    const BuiltinDesc* lowering_builtin_desc,
	    std::vector<FormattedValue> items);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Compiler-owned Str invocation. `width` and `precision` are the colon
 * qualifiers attached to `value`, not ordinary routine arguments.
 * `destination` retains its exact bounded ShortString type and storage. */
class StrCall: public Node {
public:
	FormattedValue formatted;
	Node* destination;

	StrCall(
	    FormattedValue formatted,
	    Node* destination);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Compiler-owned Val invocation. The ordinary comma grammar has already
 * selected a predefined System.Val declaration; these operands retain their
 * exact Pascal types for destination- and code-directed lowering. */
class ValCall: public Node {
public:
	Node* source;
	Node* destination;
	Node* code;

	ValCall(Node* source, Node* destination, Node* code);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** `inherited Name(args)`. Calls the parent-type method the parser resolved
 *  at parse time (single-pass compiler -- emit doesn't re-resolve).
 *
 *  No `receiver` field, unlike ProcCall. Pascal `inherited X` carries
 *  implicit Self, but C++ emits this as a qualified-id
 *  `ParentClass::X(args)` (or `ParentClass::m_meta::X(args)` for a class
 *  method) -- not member-access `this->X(args)` or `receiver->X(args)`.
 *  Qualified-id member-call syntax in C++ implicitly uses `this`, so there's
 *  no receiver expression to spell. The parent class name is recovered at
 *  emit time from `resolved` (a Method*) -> `owner_class` ->
 *  `owner_cxx_name(...)`.
 *
 *  `dropped` is set only for class destructors: those currently lower to C++
 *  destructors and auto-chain. Old-style object destructors are ordinary
 *  methods, so their explicit inherited calls must remain. */
class InheritedCall: public Node {
public:
	Callable* resolved = nullptr;
	std::vector<Node*> args;
	bool dropped = false;
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Dereference: public UnaryOperation {
public:
	Dereference(Node* a);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Assign: public BinaryOperation {
public:
	Assign(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

/** One read/operate/write mutation such as Inc(X).
 *
 * `bindings` stabilize the runtime parts of the destination designator
 * (receiver, pointer, indexes, or packed-overlay source) before either its
 * read or write occurs. `target` is the equivalent designator rebuilt from
 * those aliases, `current` names its one snapshotted value, and `assignment`
 * stores the already-selected operator result through the ordinary Pascal
 * assignment path. Keeping the store as an Assign is important: properties,
 * writable casts, packed copyback, and {$R} conversion remain one mechanism
 * rather than acquiring mutation-only variants. */
class Mutation: public Node {
public:
	struct Binding {
		StorageSlot* alias;
		Node* initializer;
	};

	Node* source_target;
	std::vector<Binding> bindings;
	Node* target;
	StorageSlot* current;
	Assign* assignment;

	Mutation(Node* source_target,
	         std::vector<Binding> bindings,
	         Node* target,
	         StorageSlot* current,
	         Assign* assignment);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

enum ShortCircuitOperationKind {
	AND,
	OR,
};

class ShortCircuitOperation: public BinaryOperation {
public:
	enum ShortCircuitOperationKind kind;
public:
	ShortCircuitOperation(enum ShortCircuitOperationKind kind, Node* a, Node* b);
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** container.member. `a` is the container (usually a StorageSlot for the
 *  record variable or with-alias) and `b` is the member (usually the field's
 *  StorageSlot). Produced by resolve_value/resolve_lvalue when a name hits
 *  inside a with-pushed scope; will also carry explicit `rec.field` grammar
 *  when that lands. */
class MemberAccess: public BinaryOperation {
public:
	MemberAccess(Node* a, Node* b);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
};

/** A Pascal property declaration. Accessors are ordinary semantic symbols:
 *  a StorageSlot for a field, a Callable for a getter/setter, or a Builtin
 *  for a compiler-synthesized RTL accessor. */
class Property: public Node {
public:
	std::string pas_name;
	std::vector<Type*> index_types;
	Node* read_accessor;
	Node* write_accessor;
	bool is_default;

	Property(std::string pas_name,
	         Type* property_type,
	         std::vector<Type*> index_types,
	         Node* read_accessor,
	         Node* write_accessor,
	         bool is_default);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Application of a named or omitted-name property. Once constructed there
 *  is no semantic distinction between `object.Items[i]` and `object[i]`. */
class PropertyAccess: public Node {
public:
	Node* receiver;
	Property* property;
	std::vector<Node*> indexes;

	PropertyAccess(Node* receiver, Property* property, std::vector<Node*> indexes);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** array[index]. `a` is the array value; `b` is the index expression.
 *  Result type (Node::ty) is the array's element type, set by the parser at
 *  construction. */
class Index: public BinaryOperation {
public:
	Index(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

class Return: public UnaryOperation {
public:
	// `a == nullptr` means a void/procedure return.
	// `try_depth` is nonzero only when this return must cross that many
	// currently protected Pascal try bodies. The emitter then uses private
	// C++ control transfer until the outermost crossed try performs the real
	// return.
	unsigned try_depth;
	Return(Node* a, unsigned try_depth = 0);
	const char* diagnostic_kind() const override;
};

/** Pascal `raise`. `object == nullptr` is bare re-raise; otherwise the
 * object is already converted to System.TObject. Explicit `at` operands are
 * nullable independently and are evaluated once by the emitted helper call. */
class Raise: public Node {
public:
	Node* object;
	Node* address;
	Node* frame;
	Raise(Node* object, Node* address = nullptr,
	      Node* frame = nullptr);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Compiler-inserted implicit type conversion. Distinct from Coerce (which
 *  represents the user-written `x as T`). Target type held on Node::ty; the
 *  wrapped value is `a`. Emits as `static_cast<target>(a)`. */
class Cast: public UnaryOperation {
public:
	Cast(Node* value, Type* target);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** An implicit ordinal or real narrowing conversion selected while {$R+} is
 * active. Conversion viability is still decided by the ordinary Type
 * conversion rules; this node only preserves the destination-boundary check
 * until C++ emission. */
class RangeCheckedCast: public Cast {
public:
	RangeCheckedCast(Node* value, Type* target);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
};

/** Open-array arguments are views or copies of an existing array value; they
 * are not Pascal value conversions between the nominal source array and the
 * formal-only OpenArrayType. Separate nodes keep parameter-mode lifetime and
 * mutation semantics explicit in diagnostics and emission. */
class OpenArrayConstView: public UnaryOperation {
public:
	OpenArrayConstView(Node* value, Type* target);
	const char* diagnostic_kind() const override;
};

class OpenArrayMutableView: public UnaryOperation {
public:
	OpenArrayMutableView(Node* value, Type* target);
	const char* diagnostic_kind() const override;
};

class OpenArrayOutView: public UnaryOperation {
public:
	OpenArrayOutView(Node* value, Type* target);
	const char* diagnostic_kind() const override;
};

class OpenArrayValueCopy: public UnaryOperation {
public:
	OpenArrayValueCopy(Node* value, Type* target);
	const char* diagnostic_kind() const override;
};

/** User-written Pascal value cast `T(E)`. It shares carrier-directed emission
 * with Cast, but ordinal constant evaluation uses explicit truncation and
 * extension rather than implicit destination-range checking. */
class ExplicitCast: public Cast {
public:
	ExplicitCast(Node* value, Type* target);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
};

/** A true Pascal constant declared inside an aggregate (`const X = expr`).
 * Its folded value has no mutable storage and may be substituted at each use.
 * The colon form (`const X: T = expr`) instead declares initialized static
 * storage and remains a StorageSlot. */
class ConstantDecl: public Node {
public:
	std::string cxx_name;
	Node* initializer;
	Type* owner_type;
	ConstantDecl(
	    std::string cxx_name, Type* ty,
	    Node* initializer, Type* owner_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

struct StorageSlot: public Node {
	enum class Kind {
		Ordinary,
		AggregateMember,
		// Storage owned by the aggregate rather than by each instance.
		// Covers both `class var` and initialized storage declared in an
		// aggregate's const section.
		StaticMember,
	};
	std::string cxx_name;
	Kind kind;
	// Non-null for aggregate members and static members. Static storage is
	// owned by its declaring aggregate even when lookup reaches it through a
	// descendant class or a metaclass receiver.
	Type* owner_type;
	// Non-null for initialized storage declarations. Aggregate-owned
	// initializers stay in the semantic graph until the enclosing type block
	// is normalized and are emitted with that aggregate, never during parse.
	Node* initializer = nullptr;
	StorageSlot(std::string cxx_name, Type* ty,
	            Kind kind = Kind::Ordinary,
	            Type* owner_type = nullptr);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

struct EnumMemberRef: public Node {
	std::string cxx_name;
	int64_t value;
	EnumMemberRef(std::string cxx_name, int64_t value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Integer: public Node {
public:
	// Signed magnitude. Pascal integer constants are not typed until context fixes
	// them; this keeps -9223372036854775808 representable without pretending it
	// fits in int64_t before conversion.
	bool negative = false;
	uint64_t value = 0;
	Integer(uint64_t value, Type* ty, bool negative = false);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class String: public Node {
public:
	std::string value;
	String(std::string value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Real: public Node {
public:
	long double value = 0.0L;
	Real(long double value, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class FixedArrayLiteral: public Node {
public:
	std::vector<Node*> elements;
	FixedArrayLiteral(std::vector<Node*> elements, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Source bracket constructor before a destination is selected. Items retain
 * their original nodes and ranges so each overload candidate can construct a
 * set or array independently without mutating the shared expression. */
class BracketLiteral: public Node {
public:
	struct Item {
		Node* lower;
		Node* upper;
	};
	std::vector<Item> items;
	Type* default_set_item_type;
	BracketLiteral(
	    std::vector<Item> items,
	    Type* default_set_item_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** A bracket constructor selected as a dynamic-array value or as the
 * call-lifetime owner for an open-array formal. Node::ty distinguishes those
 * two ordinary semantic type constructors; no secondary kind is needed. */
class ArrayLiteral: public Node {
public:
	std::vector<Node*> elements;
	ArrayLiteral(
	    std::vector<Node*> elements, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** A type-directed Pascal record constant `(field: value; ...)`.
 *
 * Field names are resolved while parsing, so later phases use StorageSlot
 * identity rather than repeating record-member lookup. Values are recursively
 * parsed against the corresponding field type; this is what disambiguates
 * nested record constants from parenthesized expressions. */
class RecordLiteral: public Node {
public:
	struct Field {
		StorageSlot* slot;
		Node* value;
	};
	std::vector<Field> fields;
	RecordLiteral(std::vector<Field> fields, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Pascal set constructor. Each item is either a singleton (upper == nullptr)
 *  or an inclusive range. Node::ty is a FixedSetType whose item_type is the
 *  common ordinal type of all bounds, or is supplied contextually for `[]`. */
class SetLiteral: public Node {
public:
	struct Item {
		Node* lower;
		Node* upper;
	};
	std::vector<Item> items;
	SetLiteral(std::vector<Item> items, Type* ty);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Pascal `nil`. Constructed without a type: its type is fixed up by cast()
 *  to the surrounding reference-type target during assignment / argument
 *  passing. Emits as C++ `nullptr`. */
class NilLiteral: public Node {
public:
	NilLiteral() = default;
	const char* diagnostic_kind() const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Compiler intrinsic `Low(T)` / `High(T)`. The argument is a type, not a
 *  value expression, so this is not a ProcCall. It carries both the queried
 *  type (operand_type) and the expression result type (Node::ty), currently
 *  the same Type*. */
class TypeBound: public Node {
public:
	TypeBoundKind kind;
	Type* operand_type;
	TypeBound(TypeBoundKind kind, Type* operand_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Runtime-sized `Low(value)` / `High(value)`. Fixed bounds still use
 * TypeBound; this node preserves the one evaluated sequence expression needed
 * by dynamic arrays, open arrays, and strings. */
class ValueBound: public UnaryOperation {
public:
	TypeBoundKind kind;
	ValueBound(
	    TypeBoundKind kind, Node* value,
	    Type* result_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(
	    ConstEvalContext& ctx) const override;
};

/** Current value of the private RTL adapter in an emitted built-in for-in
 * loop. It is never registered in a Pascal Frame; its Type lets the ordinary
 * assignment matcher perform the loop-variable conversion before emission. */
class BuiltinEnumeratorCurrent: public Node {
public:
	explicit BuiltinEnumeratorCurrent(
	    Type* element_type);
	const char* diagnostic_kind() const override;
};

/** Pascal `SizeOf(T)` / `SizeOf(expression)`, normalized to the operand's
 * static type. Constant evaluation can query the compiler layout while the
 * original node remains available to emit C++ `sizeof(emitted-type)`. */
class SizeOf: public Node {
public:
	Type* operand_type;
	explicit SizeOf(Type* operand_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

class Coerce: public UnaryOperation {
public:
	Type* target_type;
	Coerce(Node* value, Type* target_type);
	const char* diagnostic_kind() const override;
	ConstEvalResult const_eval(ConstEvalContext& ctx) const override;
};

class CoerceCheck: public UnaryOperation {
public:
	Type* target_type;
	CoerceCheck(Node* value, Type* target_type);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(
	    ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

class AddrOf: public UnaryOperation {
public:
	AddrOf(Node* a);
	const char* diagnostic_kind() const override;
};

/** `@Routine` or `@Receiver.Method`.
 *
 * A routine name is not a storage address and must remain unresolved until a
 * destination routine type supplies both its category (plain vs of-object)
 * and its full signature. `candidates` is a Callable or OverloadSet;
 * `resolved` is filled exactly once by Parser::cast.
 */
class RoutineRef: public Node {
public:
	Node* receiver;
	Node* candidates;
	Callable* resolved = nullptr;
	// Pointer context asks for only the ABI code word. Routine-type context
	// leaves this false and emits the complete m_proc/m_method value.
	bool code_only = false;
	RoutineRef(Node* receiver, Node* candidates);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(
	    ErrorLetContext* ctx, std::ostringstream& out,
	    unsigned indent) const override;
};

/** Equality of compatible routine values. Method routine equality
 * follows FPC and compares Code only; the RTL implements that distinction. */
class RoutineEqual: public BinaryOperation {
public:
	RoutineEqual(Node* a, Node* b);
	const char* diagnostic_kind() const override;
};

/** Shared base of standalone procedures/functions and methods. Holds
 *  everything call resolution and emission needs regardless of which of the
 *  two the callable is. `return_type` is unit_type() for procedures (Pascal
 *  `procedure`, no meaningful return); `has_body` is false on a forward
 *  declaration until the matching definition attaches it. */
class Callable: public Node {
public:
	std::string cxx_name;
	// Pascal spelling of the routine name. cxx_name is the C++ identifier
	// (`p_foo`, `~t_foo`); pas_name is the original Pascal spelling (`Foo`).
	// Used by `parse_inherited`'s anonymous path (`inherited;` resolves to
	// the parent method of the same Pascal name as the enclosing routine).
	std::string pas_name;
	RoutineType* ty;
	bool has_overload_directive;
	// Non-null when this source declaration names an RTL/compiler builtin.
	// Semantic special forms dispatch through descriptor metadata, never by
	// comparing the emitted C++ spelling.
	const BuiltinDesc* builtin_desc = nullptr;
	bool is_external = false;
	bool has_body = false;
	Frame* body_frame;
	Callable(std::string cxx_name,
	         std::string pas_name,
	         RoutineType* ty,
	         bool has_overload_directive);
	/** True for contextual conversion operators: `Explicit`, checked and
	 * unchecked `Implicit`, and legacy FPC `:=`. Their requested destination
	 * participates in Pascal declaration identity and therefore also needs a
	 * hidden destination tag in the C++ signature. */
	bool is_conversion_operator() const;
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Standalone procedure or function (Pascal `procedure`/`function` at
 *  unit/program scope). Carries no extra state beyond Callable; its identity
 *  is what distinguishes it from Method for type-checking against routine
 *  pointer types. */
class Procedure: public Callable {
public:
	Procedure(std::string cxx_name,
	          std::string pas_name,
	          RoutineType* ty,
	          bool has_overload_directive);
	const char* diagnostic_kind() const override;
};

/** Method of a class/object/record. `owner_class` is the type it belongs to
 *  (used at overload-ranking time for the Self position). `virtual_kind`
 *  drives emission (`virtual`/`override`/etc. keywords in the C++ class). */
class Method: public Callable {
public:
	enum class VirtualKind { None, Virtual, Override, Abstract, Dynamic };
	Type* owner_class;
	VirtualKind virtual_kind;
	// A static class method remains a Method because it belongs to the
	// aggregate's member environment, but its RoutineType kind is ROUTINE:
	// it has neither an object nor a metaclass receiver.
	bool is_static;
	// Orthogonal to VirtualKind: the usual Pascal form is
	// `override; final`, which must emit both C++ virt-specifiers.
	bool is_final;
	int vtable_slot;   // -1 = unassigned; populated at class-layout time
	Method(std::string cxx_name,
	       std::string pas_name,
	       RoutineType* ty,
	       bool has_overload_directive,
	       Type* owner_class,
	       VirtualKind virtual_kind);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};

/** Overload set: multiple same-category Callables sharing one Pascal name.
 * Frame registration forms a local set from distinct signatures whether or
 * not they write `overload`; the directive controls whether lookup may append
 * a same-category family from another scope.
 *
 * During construction of an aggregate in an open type block, this node is
 * also the temporary declaration collection for that name. Its members are
 * intentionally not compared while their signatures can contain
 * IncompleteType edges. Recursive type-block normalization followed by
 * aggregate declaration validation establishes the same-category/distinct-
 * signature invariant before emission or statement semantics can use it. */
class OverloadSet: public Node {
public:
	std::vector<Callable*> members;
	OverloadSet(std::vector<Callable*> members);
	const char* diagnostic_kind() const override;
	void collect_diagnostic_edges(ErrorLetContext* ctx) const override;
	void print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const override;
};
