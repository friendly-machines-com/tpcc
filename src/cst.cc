#include "cst.h"
#include "builtins.h"
#include "units.h"
#include <iomanip>
#include <limits>
#include <sstream>

std::string Node::str() const {
	std::stringstream sst;
	sst << (void*)this;
	return sst.str();
}

void Block::add(Node* stmt) {
	statements.push_back(stmt);
}

const char* EmptyStatement::diagnostic_kind() const {
	return "empty_statement";
}

void EmptyStatement::print_diagnostic_definition(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << diagnostic_kind();
}

Symbol::Symbol(std::string text) {
	this->text = text;
}

std::string Symbol::str() const {
	return text;
}

UnaryOperation::UnaryOperation(Node* a) {
	this->a = a;
}

BinaryOperation::BinaryOperation(Node* a, Node* b) {
	this->a = a;
	this->b = b;
}

EvaluateThen::EvaluateThen(Node* a, Node* b) : BinaryOperation(a, b) {
	this->ty = b ? b->ty : nullptr;
}

ProcCall::ProcCall(Node* receiver, Node* callee, std::vector<Node*> args) : receiver(receiver), callee(callee), args(std::move(args)) {
}

ClassRefValue::ClassRefValue(ClassType* target) : target(target) {
	this->ty = new ClassRefType(target ? target->source_location : SourceLocation{}, target);
}

TypeMemberQualifier::TypeMemberQualifier(Type* target) : target(target) {
	this->ty = target;
}

Construct::Construct(Node* class_reference, Method* initializer, std::vector<Node*> args, ClassType* result_type) : class_reference(class_reference), initializer(initializer), args(std::move(args)) {
	this->ty = result_type;
}

NewValue::NewValue(Type* pointer_type, Type* allocated_type, Method* initializer, std::vector<Node*> args) : allocated_type(allocated_type), initializer(initializer), args(std::move(args)) {
	this->ty = pointer_type;
}

DisposeValue::DisposeValue(Node* pointer, Method* finalizer) : pointer(pointer), finalizer(finalizer) {
}

ConstructorFail::ConstructorFail() {
	this->ty = &unit_type();
}

UnitRef::UnitRef(Unit* unit) : unit(unit) {
}

WriteCall::WriteCall(bool newline, Node* file, const BuiltinDesc* lowering_builtin_desc, std::vector<FormattedValue> items) : newline(newline), file(file), lowering_builtin_desc(lowering_builtin_desc), items(std::move(items)) {
}

StrCall::StrCall(FormattedValue formatted, Node* destination) : formatted(formatted), destination(destination) {
	this->ty = &unit_type();
}

ValCall::ValCall(Node* source, Node* destination, Node* code) : source(source), destination(destination), code(code) {
	this->ty = &unit_type();
}

Assign::Assign(Node* a, Node* b) : BinaryOperation(a, b) {
}

Mutation::Mutation(Node* source_target, std::vector<Binding> bindings, Node* target, StorageSlot* current, Assign* assignment) : source_target(source_target), bindings(std::move(bindings)), target(target), current(current), assignment(assignment) {
}

ShortCircuitOperation::ShortCircuitOperation(enum ShortCircuitOperationKind kind, Node* a, Node* b) : BinaryOperation(a, b) {
	this->kind = kind;
}

MemberAccess::MemberAccess(Node* a, Node* b) : BinaryOperation(a, b) {
}

Property::Property(std::string pas_name, Type* property_type, std::vector<Type*> index_types, Node* read_accessor, Node* write_accessor, bool is_default) : pas_name(std::move(pas_name)), index_types(std::move(index_types)), read_accessor(read_accessor), write_accessor(write_accessor), is_default(is_default) {
	this->ty = property_type;
}

PropertyAccess::PropertyAccess(Node* receiver, Property* property, std::vector<Node*> indexes) : receiver(receiver), property(property), indexes(std::move(indexes)) {
	this->ty = property ? property->ty : nullptr;
}

Index::Index(Node* a, Node* b) : BinaryOperation(a, b) {
}

Coerce::Coerce(Node* value, Type* target_type) : UnaryOperation(value), target_type(target_type) {
	this->ty = target_type;
}

CoerceCheck::CoerceCheck(Node* value, Type* target_type) : UnaryOperation(value), target_type(target_type) {
}

Dereference::Dereference(Node* a) : UnaryOperation(a) {
}

Return::Return(Node* a, unsigned try_depth) : UnaryOperation(a), try_depth(try_depth) {
}

Raise::Raise(Node* object, Node* address, Node* frame) : object(object), address(address), frame(frame) {
}

AddrOf::AddrOf(Node* a) : UnaryOperation(a) {
}

RoutineRef::RoutineRef(Node* receiver, Node* candidates) : receiver(receiver), candidates(candidates) {
}

RoutineCode::RoutineCode(Node* value) : UnaryOperation(value) {
}

Cast::Cast(Node* value, Type* target) : UnaryOperation(value) {
	this->ty = target;
}

RangeCheckedCast::RangeCheckedCast(Node* value, Type* target) : Cast(value, target) {
}

OpenArrayConstView::OpenArrayConstView(Node* value, Type* target) : UnaryOperation(value) {
	this->ty = target;
}

OpenArrayMutableView::OpenArrayMutableView(Node* value, Type* target) : UnaryOperation(value) {
	this->ty = target;
}

OpenArrayOutView::OpenArrayOutView(Node* value, Type* target) : UnaryOperation(value) {
	this->ty = target;
}

OpenArrayValueCopy::OpenArrayValueCopy(Node* value, Type* target) : UnaryOperation(value) {
	this->ty = target;
}

ExplicitCast::ExplicitCast(Node* value, Type* target) : Cast(value, target) {
}

TypeBound::TypeBound(TypeBoundKind kind, Type* operand_type) : kind(kind), operand_type(operand_type) {
	auto array = dynamic_cast<FixedArrayType*>(operand_type);
	this->ty = array ? array->bounds : operand_type;
}

ValueBound::ValueBound(TypeBoundKind kind, Node* value, Type* result_type) : UnaryOperation(value), kind(kind) {
	this->ty = result_type;
}

BuiltinEnumeratorCurrent::BuiltinEnumeratorCurrent(Type* element_type) {
	this->ty = element_type;
}

SizeOf::SizeOf(Type* operand_type) : operand_type(operand_type) {
	this->ty = sizeint_type();
}

// Value-identifier ctors: take an OPTIONAL Pascal name. If non-empty, apply
// the `p_` prefix so the cxx identifier stays clear of C++ reserved words
// (`new`, `class`, `false`, ...). If empty (anonymous entity -- nameless
// parameter in a prototype, compiler temporary, routine-type declaration
// `procedure of object`, etc.), cxx_name stays empty and emission skips it.
StorageSlot::StorageSlot(std::string cxx_name, Type* ty, Kind kind, Type* owner_type) : kind(kind), owner_type(owner_type) {
	this->cxx_name = cxx_name;
	this->ty = ty;
	has_static_storage_duration = kind == Kind::StaticMember;
}

EnumMemberRef::EnumMemberRef(std::string cxx_name, int64_t value, Type* ty) {
	this->cxx_name = cxx_name;
	this->value = value;
	this->ty = ty;
}

Integer::Integer(uint64_t value, Type* ty, bool negative, bool based_literal) {
	this->negative = negative && value != 0;
	this->value = value;
	this->based_literal = based_literal;
	this->ty = ty;
}

String::String(std::string value, Type* ty, bool contextual_literal) : contextual_literal(contextual_literal) {
	this->value = std::move(value);
	this->ty = ty;
}

Real::Real(long double value, Type* ty) {
	this->value = value;
	this->ty = ty;
}

Real::Real(DecimalOrigin origin) : origin(std::move(origin)) {
	this->ty = &untyped_real_type();
}

FixedArrayLiteral::FixedArrayLiteral(std::vector<Node*> elements, Type* ty) : elements(std::move(elements)) {
	this->ty = ty;
}

BracketLiteral::BracketLiteral(std::vector<Item> items, Type* default_set_item_type, FixedArrayType* default_array_type) : items(std::move(items)), default_set_item_type(default_set_item_type), default_array_type(default_array_type) {
	if (default_array_type) {
		this->ty = default_array_type;
	} else if (default_set_item_type && default_set_item_type != unknown_type()) {
		this->ty = new FixedSetType(default_set_item_type->source_location, default_set_item_type);
	} else {
		this->ty = unknown_type();
	}
}

ArrayLiteral::ArrayLiteral(std::vector<Node*> elements, Type* ty) : elements(std::move(elements)) {
	this->ty = ty;
}

RecordLiteral::RecordLiteral(std::vector<Field> fields, Type* ty) : fields(std::move(fields)) {
	this->ty = ty;
}

SetLiteral::SetLiteral(std::vector<Item> items, Type* ty) : items(std::move(items)) {
	this->ty = ty;
}

Callable::Callable(std::string cxx_name, std::string pas_name, RoutineType* ty, bool has_overload_directive) : cxx_name(std::move(cxx_name)), pas_name(std::move(pas_name)), ty(ty), has_overload_directive(has_overload_directive), has_body(false), body_frame(nullptr) {
}

bool Callable::is_conversion_operator() const {
	return pas_name == ":explicit" || pas_name == ":implicit" || pas_name == ":uncheckedimplicit" || pas_name == ":=";
}

Procedure::Procedure(std::string cxx_name, std::string pas_name, RoutineType* ty, bool has_overload_directive) : Callable(std::move(cxx_name), std::move(pas_name), ty, has_overload_directive) {
}

Method::Method(std::string cxx_name, std::string pas_name, RoutineType* ty, bool has_overload_directive, Type* owner_class, VirtualKind virtual_kind) : Callable(std::move(cxx_name), std::move(pas_name), ty, has_overload_directive), owner_class(owner_class), virtual_kind(virtual_kind), is_static(false), is_final(false), vtable_slot(-1) {
}

OverloadSet::OverloadSet(std::vector<Callable*> members) : members(std::move(members)) {
}

#include "diagnostic.h"
#include "evaluator.h"
#include "frame.h"
#include "types.h"
#include <algorithm>

static std::string diagnostic_string_literal(const std::string& text) {
	std::string r = "'";
	for (char ch : text) {
		if (ch == '\'') {
			r.push_back('\'');
		}
		r.push_back(ch);
	}
	r.push_back('\'');
	return r;
}

const char* Node::diagnostic_kind() const {
	return "value";
}

void Node::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	ctx->add_type_edge(ty);
}

void Node::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << diagnostic_kind() << " : " << ctx->known_type_ref(ty);
}

void Node::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const {
	out << "value ...";
}

ConstEvalResult Node::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::not_constant();
}

const char* Block::diagnostic_kind() const {
	return "block";
}

void Block::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (auto* s : statements) {
		ctx->add_value_edge(s);
	}
}

void Block::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "block";
	for (auto* s : statements) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << ctx->known_value_ref(s);
	}
	out << "\n";
	ctx->indent(out, indent);
	out << "end";
}

const char* Symbol::diagnostic_kind() const {
	return "symbol";
}

void Symbol::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "symbol " << diagnostic_string_literal(str()) << " : " << ctx->known_type_ref(ty);
}

void UnaryOperation::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(a);
}

void UnaryOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << diagnostic_kind() << " " << ctx->known_value_ref(a) << " : " << ctx->known_type_ref(ty);
}

void BinaryOperation::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(a);
	ctx->add_value_edge(b);
}

void BinaryOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << diagnostic_kind() << " " << ctx->known_value_ref(a) << ", " << ctx->known_value_ref(b) << " : " << ctx->known_type_ref(ty);
}

const char* ProcCall::diagnostic_kind() const {
	return "call";
}

const char* EvaluateThen::diagnostic_kind() const {
	return "evaluate_then";
}

ConstEvalResult EvaluateThen::const_eval(ConstEvalContext&) const {
	// The left expression exists specifically for its runtime effects.
	return ConstEvalResult::not_constant();
}

void ProcCall::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(receiver);
	ctx->add_value_edge(callee);
	for (auto* a : args) {
		ctx->add_value_edge(a);
	}
}

ConstEvalResult ProcCall::const_eval(ConstEvalContext& ctx) const {
	auto c = dynamic_cast<Callable*>(callee);
	if (!c) {
		return ConstEvalResult::not_constant();
	}
	const BuiltinDesc* desc = lowering_builtin_desc ? lowering_builtin_desc : c->builtin_desc;
	if (!desc || !desc->const_fold) {
		return ConstEvalResult::not_constant();
	}
	std::vector<Node*> folded;
	folded.reserve(args.size());
	for (auto* arg : args) {
		ConstEvalResult r = arg ? arg->const_eval(ctx) : ConstEvalResult::not_constant();
		if (r.kind != ConstEvalResult::Kind::Success) {
			return r;
		}
		folded.push_back(r.node);
	}
	// Generic root declarations use an intentionally omitted return type to
	// express exact T -> T relations which Pascal cannot yet quantify. Call
	// construction restores that concrete result before evaluation, so the
	// call node--not the declaration placeholder--is authoritative here.
	return desc->const_fold(ctx, ty, folded);
}

void ProcCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "call\n";
	ctx->indent(out, indent + 1);
	out << "callee: " << ctx->known_value_ref(callee) << "\n";
	if (receiver) {
		ctx->indent(out, indent + 1);
		out << "receiver: " << ctx->known_value_ref(receiver) << "\n";
	}
	for (auto* a : args) {
		ctx->indent(out, indent + 1);
		out << "arg: " << ctx->known_value_ref(a) << "\n";
	}
	ctx->indent(out, indent + 1);
	out << "returns: " << ctx->known_type_ref(ty);
}

const char* ClassRefValue::diagnostic_kind() const {
	return "class_reference_value";
}

const char* TypeMemberQualifier::diagnostic_kind() const {
	return "type_member_qualifier";
}

const char* Construct::diagnostic_kind() const {
	return "construction";
}

const char* NewValue::diagnostic_kind() const {
	return "new";
}

const char* DisposeValue::diagnostic_kind() const {
	return "dispose";
}

const char* ConstructorFail::diagnostic_kind() const {
	return "constructor_fail";
}

const char* UnitRef::diagnostic_kind() const {
	return "unit_reference";
}

void UnitRef::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	// ErrorLetContext binds this node under Unit::name, while the quoted field
	// records which semantic unit the node designates. The quotes matter: a
	// bare unit name here would look like a graph reference and would require a
	// second value definition.
	out << diagnostic_kind() << "\n";
	ctx->indent(out, indent + 1);
	out << "unit: " << diagnostic_string_literal(unit ? unit->name : "<null>");
}

void ClassRefValue::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(target);
}

void TypeMemberQualifier::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(target);
}

void Construct::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(class_reference);
	ctx->add_value_edge(initializer);
	for (Node* arg : args) {
		ctx->add_value_edge(arg);
	}
}

void NewValue::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(allocated_type);
	ctx->add_value_edge(initializer);
	for (Node* arg : args) {
		ctx->add_value_edge(arg);
	}
}

void DisposeValue::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(pointer);
	ctx->add_value_edge(finalizer);
}

void ClassRefValue::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "class reference " << ctx->known_type_ref(target) << " : " << ctx->known_type_ref(ty);
}

void TypeMemberQualifier::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "type member qualifier " << ctx->known_type_ref(target);
}

void Construct::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "construct " << ctx->known_type_ref(ty);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "class reference: " << ctx->known_value_ref(class_reference);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "initializer: " << ctx->known_value_ref(initializer);
	for (Node* arg : args) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "arg: " << ctx->known_value_ref(arg);
	}
}

void NewValue::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "new " << ctx->known_type_ref(allocated_type);
	if (initializer) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "initializer: " << ctx->known_value_ref(initializer);
	}
	for (Node* arg : args) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "arg: " << ctx->known_value_ref(arg);
	}
	out << " : " << ctx->known_type_ref(ty);
}

void DisposeValue::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "dispose " << ctx->known_value_ref(pointer);
	if (finalizer) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "finalizer: " << ctx->known_value_ref(finalizer);
	}
}

const char* WriteCall::diagnostic_kind() const {
	return newline ? "writeln" : "write";
}

void WriteCall::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(file);
	for (const FormattedValue& item : items) {
		ctx->add_value_edge(item.value);
		ctx->add_value_edge(item.width);
		ctx->add_value_edge(item.precision);
		ctx->add_value_edge(item.str_callee);
	}
}

void WriteCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << diagnostic_kind();
	if (file) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "file: " << ctx->known_value_ref(file);
	}
	for (const FormattedValue& item : items) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "item: " << ctx->known_value_ref(item.value);
		if (item.width) {
			out << " width " << ctx->known_value_ref(item.width);
		}
		if (item.precision) {
			out << " precision " << ctx->known_value_ref(item.precision);
		}
		out << " via " << ctx->known_value_ref(item.str_callee);
	}
}

const char* StrCall::diagnostic_kind() const {
	return "str";
}

void StrCall::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(formatted.value);
	ctx->add_value_edge(formatted.width);
	ctx->add_value_edge(formatted.precision);
	ctx->add_value_edge(formatted.str_callee);
	ctx->add_value_edge(destination);
}

void StrCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "str\n";
	ctx->indent(out, indent + 1);
	out << "value: " << ctx->known_value_ref(formatted.value);
	if (formatted.width) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "width: " << ctx->known_value_ref(formatted.width);
	}
	if (formatted.precision) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "precision: " << ctx->known_value_ref(formatted.precision);
	}
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "callee: " << ctx->known_value_ref(formatted.str_callee);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "destination: " << ctx->known_value_ref(destination);
}

const char* ValCall::diagnostic_kind() const {
	return "val";
}

void ValCall::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(source);
	ctx->add_value_edge(destination);
	ctx->add_value_edge(code);
}

void ValCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "val\n";
	ctx->indent(out, indent + 1);
	out << "source: " << ctx->known_value_ref(source);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "destination: " << ctx->known_value_ref(destination);
	if (code) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "code: " << ctx->known_value_ref(code);
	}
}

const char* InheritedCall::diagnostic_kind() const {
	return "inherited_call";
}

void InheritedCall::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(resolved);
	for (auto* a : args) {
		ctx->add_value_edge(a);
	}
}

void InheritedCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "inherited call\n";
	ctx->indent(out, indent + 1);
	out << "resolved: " << ctx->known_value_ref(resolved);
	for (auto* a : args) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "arg: " << ctx->known_value_ref(a);
	}
}

const char* Dereference::diagnostic_kind() const {
	return "deref";
}

void Dereference::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "deref " << ctx->known_value_ref(a) << " : " << ctx->known_type_ref(ty);
}

const char* Assign::diagnostic_kind() const {
	return "assign";
}

const char* Mutation::diagnostic_kind() const {
	return "mutation";
}

void Mutation::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(source_target);
	for (const Binding& binding : bindings) {
		ctx->add_value_edge(binding.alias);
		ctx->add_value_edge(binding.initializer);
	}
	ctx->add_value_edge(target);
	ctx->add_value_edge(current);
	ctx->add_value_edge(assignment);
}

void Mutation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "mutate " << ctx->known_value_ref(source_target);
	for (const Binding& binding : bindings) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "bind " << ctx->known_value_ref(binding.alias) << " = " << ctx->known_value_ref(binding.initializer);
	}
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "current: " << ctx->known_value_ref(current);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "store: " << ctx->known_value_ref(assignment);
}

const char* ShortCircuitOperation::diagnostic_kind() const {
	return kind == AND ? "and" : "or";
}

void ShortCircuitOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	BinaryOperation::print_diagnostic_definition(ctx, out, 0);
}

const char* MemberAccess::diagnostic_kind() const {
	return "member_access";
}

ConstEvalResult MemberAccess::const_eval(ConstEvalContext& ctx) const {
	// Aggregate static constants remain constants when selected through an
	// instance. The left operand is only a qualifier; FPC does not evaluate
	// it, just as it does not evaluate an instance qualifier for class/static
	// storage.
	if (auto constant = dynamic_cast<ConstantDecl*>(b)) {
		return constant->const_eval(ctx);
	}
	return ConstEvalResult::not_constant();
}

const char* Index::diagnostic_kind() const {
	return "index";
}

const char* Return::diagnostic_kind() const {
	return "return";
}

const char* Raise::diagnostic_kind() const {
	return "raise";
}

void Raise::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(object);
	ctx->add_value_edge(address);
	ctx->add_value_edge(frame);
}

void Raise::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "raise";
	if (object) {
		out << " " << ctx->known_value_ref(object);
	}
	if (address) {
		out << " at " << ctx->known_value_ref(address);
	}
	if (frame) {
		out << ", " << ctx->known_value_ref(frame);
	}
}

const char* Cast::diagnostic_kind() const {
	return "cast";
}

ConstEvalResult Cast::const_eval(ConstEvalContext& ctx) const {
	ConstEvalResult r = a ? a->const_eval(ctx) : ConstEvalResult::not_constant();
	if (r.kind != ConstEvalResult::Kind::Success) {
		return r;
	}
	if (auto set = dynamic_cast<SetLiteral*>(r.node); set && dynamic_cast<FixedSetType*>(set->ty) && dynamic_cast<FixedSetType*>(ty)) {
		// Set conversions preserve ordinal membership keys. Retagging the
		// already-folded literal is the constant form of the runtime
		// m_set_cast operation; the bounds themselves need no value
		// conversion.
		return ConstEvalResult::success(new SetLiteral(set->items, ty));
	}
	if (auto ordinal = folded_ordinal_value(r.node)) {
		ConstEvalResult converted = const_explicit_ordinal_cast(ordinal->value.magnitude, ordinal->value.negative, ty);
		if (converted.kind != ConstEvalResult::Kind::Error) {
			return converted;
		}
		if (auto i = dynamic_cast<Integer*>(r.node); i && ordinal_type_domain(i->ty) && ordinal_type_domain(i->ty)->family == OrdinalFamily::Integer) {
			return const_convert_integer(i->value, i->negative, i->ty, ty);
		}
	}
	if (auto real = dynamic_cast<Real*>(r.node)) {
		if (real->is_origin() && is_real_semantic_type(ty)) {
			RealMaterialization converted = materialize_decimal_origin(*real->origin, ty);
			if (converted.kind == RealMaterializationKind::OutOfRange) {
				return ConstEvalResult::error("real constant out of range for target type");
			}
			if (converted.kind != RealMaterializationKind::InvalidTarget) {
				return ConstEvalResult::success(new Real(converted.value, ty));
			}
		} else if (is_real_semantic_type(real->ty) && is_real_semantic_type(ty)) {
			auto converted = round_typed_real(real->value, ty);
			if (converted) {
				return ConstEvalResult::success(new Real(*converted, ty));
			}
		}
	}
	if (auto string = dynamic_cast<String*>(r.node)) {
		ConstEvalResult converted = const_convert_string(string->value, ty);
		if (converted.kind != ConstEvalResult::Kind::Error) {
			return converted;
		}
	}
	return ConstEvalResult::not_constant();
}

void Cast::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "cast " << ctx->known_value_ref(a) << " to " << ctx->known_type_ref(ty);
}

const char* RangeCheckedCast::diagnostic_kind() const {
	return "range_checked_cast";
}

ConstEvalResult RangeCheckedCast::const_eval(ConstEvalContext& ctx) const {
	ConstEvalResult value = a ? a->const_eval(ctx) : ConstEvalResult::not_constant();
	if (value.kind != ConstEvalResult::Kind::Success) {
		return value;
	}
	if (auto real = dynamic_cast<Real*>(value.node)) {
		if (real->is_origin()) {
			RealMaterialization converted = materialize_decimal_origin(*real->origin, ty);
			if (converted.kind == RealMaterializationKind::OutOfRange) {
				return ConstEvalResult::error("real constant out of range for target type");
			}
		} else if (typed_real_out_of_range(real->value, ty)) {
			return ConstEvalResult::error("real constant out of range for target type");
		}
		return Cast::const_eval(ctx);
	} else {
		auto ordinal = folded_ordinal_value(value.node);
		if (!ordinal) {
			return Cast::const_eval(ctx);
		}
		ConstEvalResult lower = const_eval_type_bound(TypeBoundKind::Low, ty);
		ConstEvalResult upper = const_eval_type_bound(TypeBoundKind::High, ty);
		if (lower.kind != ConstEvalResult::Kind::Success) {
			return lower;
		}
		if (upper.kind != ConstEvalResult::Kind::Success) {
			return upper;
		}
		auto lower_ordinal = folded_ordinal_value(lower.node);
		auto upper_ordinal = folded_ordinal_value(upper.node);
		if (!lower_ordinal || !upper_ordinal) {
			return ConstEvalResult::error("range-checked conversion has non-ordinal bounds");
		}
		if (compare_ordinal_values(ordinal->value, lower_ordinal->value) < 0 || compare_ordinal_values(ordinal->value, upper_ordinal->value) > 0) {
			return ConstEvalResult::error("integer constant out of range for target type");
		}
		return const_explicit_ordinal_cast(ordinal->value.magnitude, ordinal->value.negative, ty);
	}
}

const char* ExplicitCast::diagnostic_kind() const {
	return "explicit_cast";
}

ConstEvalResult ExplicitCast::const_eval(ConstEvalContext& ctx) const {
	ConstEvalResult value = a ? a->const_eval(ctx) : ConstEvalResult::not_constant();
	if (value.kind != ConstEvalResult::Kind::Success) {
		return value;
	}
	if (auto ordinal = folded_ordinal_value(value.node)) {
		return const_explicit_ordinal_cast(ordinal->value.magnitude, ordinal->value.negative, ty);
	}
	return Cast::const_eval(ctx);
}

ConstantDecl::ConstantDecl(std::string cxx_name, Type* ty, Node* initializer, Type* owner_type) : cxx_name(std::move(cxx_name)), initializer(initializer), owner_type(owner_type) {
	this->ty = ty;
}

const char* ConstantDecl::diagnostic_kind() const {
	return "constant";
}

ConstEvalResult ConstantDecl::const_eval(ConstEvalContext& ctx) const {
	return initializer ? initializer->const_eval(ctx) : ConstEvalResult::not_constant();
}

void ConstantDecl::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(initializer);
	ctx->add_type_edge(owner_type);
}

void ConstantDecl::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "constant " << ctx->known_value_ref(initializer) << " : " << ctx->known_type_ref(ty);
}

const char* StorageSlot::diagnostic_kind() const {
	return "slot";
}

void StorageSlot::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(owner_type);
	ctx->add_value_edge(initializer);
}

void StorageSlot::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "slot : " << ctx->known_type_ref(ty);
}

const char* Property::diagnostic_kind() const {
	return "property";
}

void Property::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(read_accessor);
	ctx->add_value_edge(write_accessor);
	for (Type* index_type : index_types) {
		ctx->add_type_edge(index_type);
	}
}

void Property::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "property " << pas_name << " : " << ctx->known_type_ref(ty);
}

const char* PropertyAccess::diagnostic_kind() const {
	return "property_access";
}

void PropertyAccess::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(receiver);
	ctx->add_value_edge(property);
	for (Node* index : indexes) {
		ctx->add_value_edge(index);
	}
}

void PropertyAccess::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "property access " << ctx->known_value_ref(property) << " on " << ctx->known_value_ref(receiver);
}

const char* EnumMemberRef::diagnostic_kind() const {
	return "enum_member";
}

ConstEvalResult EnumMemberRef::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::success(new EnumMemberRef(cxx_name, value, ty));
}

void EnumMemberRef::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "enum member = " << value << " : " << ctx->known_type_ref(ty);
}

const char* Integer::diagnostic_kind() const {
	return "integer";
}

ConstEvalResult Integer::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::success(new Integer(value, ty, negative, based_literal));
}

void Integer::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "integer " << (negative ? "-" : "") << value << " : " << ctx->known_type_ref(ty);
}

const char* String::diagnostic_kind() const {
	return "string";
}

ConstEvalResult String::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::success(new String(value, ty, contextual_literal));
}

void String::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	std::string text = value.substr(0, std::min<size_t>(value.size(), 40));
	if (value.size() > 40) {
		text += "...";
	}
	out << "string " << diagnostic_string_literal(text) << " : " << ctx->known_type_ref(ty);
}

const char* Real::diagnostic_kind() const {
	return "real";
}

ConstEvalResult Real::const_eval(ConstEvalContext&) const {
	if (origin) {
		return ConstEvalResult::success(new Real(*origin));
	}
	return ConstEvalResult::success(new Real(value, ty));
}

void Real::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	if (origin) {
		out << "real_origin " << decimal_origin_text(*origin) << " : " << ctx->known_type_ref(ty);
	} else {
		out << "real " << std::setprecision(std::numeric_limits<long double>::max_digits10) << value << " : " << ctx->known_type_ref(ty);
	}
}

const char* FixedArrayLiteral::diagnostic_kind() const {
	return "fixed_array_literal";
}

ConstEvalResult FixedArrayLiteral::const_eval(ConstEvalContext& ctx) const {
	std::vector<Node*> folded;
	folded.reserve(elements.size());
	for (Node* element : elements) {
		ConstEvalResult r = element ? element->const_eval(ctx) : ConstEvalResult::not_constant();
		if (r.kind != ConstEvalResult::Kind::Success) {
			return r;
		}
		folded.push_back(r.node);
	}
	return ConstEvalResult::success(new FixedArrayLiteral(std::move(folded), ty));
}

void FixedArrayLiteral::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (Node* element : elements) {
		ctx->add_value_edge(element);
	}
}

void FixedArrayLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "fixed array literal : " << ctx->known_type_ref(ty);
	for (Node* element : elements) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "element: " << ctx->known_value_ref(element);
	}
}

const char* RecordLiteral::diagnostic_kind() const {
	return "record_literal";
}

ConstEvalResult RecordLiteral::const_eval(ConstEvalContext& ctx) const {
	std::vector<Field> folded;
	folded.reserve(fields.size());
	for (const Field& field : fields) {
		ConstEvalResult value = field.value ? field.value->const_eval(ctx) : ConstEvalResult::not_constant();
		if (value.kind != ConstEvalResult::Kind::Success) {
			return value;
		}
		folded.push_back(Field{field.slot, value.node});
	}
	return ConstEvalResult::success(new RecordLiteral(std::move(folded), ty));
}

void RecordLiteral::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (const Field& field : fields) {
		ctx->add_value_edge(field.slot);
		ctx->add_value_edge(field.value);
	}
}

void RecordLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "record literal : " << ctx->known_type_ref(ty);
	for (const Field& field : fields) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << ctx->known_value_ref(field.slot) << ": " << ctx->known_value_ref(field.value);
	}
}

const char* SetLiteral::diagnostic_kind() const {
	return "set_literal";
}

ConstEvalResult SetLiteral::const_eval(ConstEvalContext& ctx) const {
	std::vector<Item> folded;
	folded.reserve(items.size());
	for (const Item& item : items) {
		ConstEvalResult lower = item.lower ? item.lower->const_eval(ctx) : ConstEvalResult::not_constant();
		if (lower.kind != ConstEvalResult::Kind::Success) {
			return lower;
		}
		Node* upper_node = nullptr;
		if (item.upper) {
			ConstEvalResult upper = item.upper->const_eval(ctx);
			if (upper.kind != ConstEvalResult::Kind::Success) {
				return upper;
			}
			upper_node = upper.node;
		}
		folded.push_back(Item{lower.node, upper_node});
	}
	return ConstEvalResult::success(new SetLiteral(std::move(folded), ty));
}

void SetLiteral::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (const Item& item : items) {
		ctx->add_value_edge(item.lower);
		ctx->add_value_edge(item.upper);
	}
}

void SetLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "set literal : " << ctx->known_type_ref(ty);
	for (const Item& item : items) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "item: " << ctx->known_value_ref(item.lower);
		if (item.upper) {
			out << " .. " << ctx->known_value_ref(item.upper);
		}
	}
}

const char* BracketLiteral::diagnostic_kind() const {
	return "bracket_literal";
}

ConstEvalResult BracketLiteral::const_eval(ConstEvalContext& ctx) const {
	std::vector<SetLiteral::Item> folded;
	folded.reserve(items.size());
	for (const Item& item : items) {
		ConstEvalResult lower = item.lower ? item.lower->const_eval(ctx) : ConstEvalResult::not_constant();
		if (lower.kind != ConstEvalResult::Kind::Success) {
			return lower;
		}
		Node* upper = nullptr;
		if (item.upper) {
			ConstEvalResult result = item.upper->const_eval(ctx);
			if (result.kind != ConstEvalResult::Kind::Success) {
				return result;
			}
			upper = result.node;
		}
		folded.push_back(SetLiteral::Item{lower.node, upper});
	}
	return ConstEvalResult::success(new SetLiteral(std::move(folded), new FixedSetType(default_set_item_type ? default_set_item_type->source_location : SourceLocation::internal(), default_set_item_type)));
}

void BracketLiteral::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(default_set_item_type);
	ctx->add_type_edge(default_array_type);
	for (const Item& item : items) {
		ctx->add_value_edge(item.lower);
		ctx->add_value_edge(item.upper);
	}
}

void BracketLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "bracket literal";
	for (const Item& item : items) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "item: " << ctx->known_value_ref(item.lower);
		if (item.upper) {
			out << " .. " << ctx->known_value_ref(item.upper);
		}
	}
}

const char* ArrayLiteral::diagnostic_kind() const {
	return "array_literal";
}

ConstEvalResult ArrayLiteral::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::not_constant();
}

void ArrayLiteral::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (Node* element : elements) {
		ctx->add_value_edge(element);
	}
}

void ArrayLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "array literal : " << ctx->known_type_ref(ty);
	for (Node* element : elements) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "item: " << ctx->known_value_ref(element);
	}
}

const char* NilLiteral::diagnostic_kind() const {
	return "nil";
}

ConstEvalResult NilLiteral::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::success(const_cast<NilLiteral*>(this));
}

void NilLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "nil : " << ctx->known_type_ref(ty);
}

const char* OpenArrayConstView::diagnostic_kind() const {
	return "open_array_const_view";
}

const char* OpenArrayMutableView::diagnostic_kind() const {
	return "open_array_mutable_view";
}

const char* OpenArrayOutView::diagnostic_kind() const {
	return "open_array_out_view";
}

const char* OpenArrayValueCopy::diagnostic_kind() const {
	return "open_array_value_copy";
}

const char* TypeBound::diagnostic_kind() const {
	return kind == TypeBoundKind::Low ? "low" : "high";
}

void TypeBound::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(operand_type);
}

ConstEvalResult TypeBound::const_eval(ConstEvalContext&) const {
	return const_eval_type_bound(kind, operand_type);
}

void TypeBound::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << diagnostic_kind() << "(" << ctx->known_type_ref(operand_type) << ") : " << ctx->known_type_ref(ty);
}

const char* ValueBound::diagnostic_kind() const {
	return kind == TypeBoundKind::Low ? "low" : "high";
}

ConstEvalResult ValueBound::const_eval(ConstEvalContext&) const {
	return ConstEvalResult::not_constant();
}

const char* BuiltinEnumeratorCurrent::diagnostic_kind() const {
	return "builtin_enumerator_current";
}

const char* SizeOf::diagnostic_kind() const {
	return "sizeof";
}

ConstEvalResult SizeOf::const_eval(ConstEvalContext&) const {
	auto layout = type_layout(false, operand_type);
	if (!layout) {
		return ConstEvalResult::not_constant();
	}
	return ConstEvalResult::success(new Integer(layout->size, sizeint_type()));
}

void SizeOf::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(operand_type);
}

void SizeOf::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "sizeof(" << ctx->known_type_ref(operand_type) << ") : " << ctx->known_type_ref(ty);
}

const char* Coerce::diagnostic_kind() const {
	return "coerce";
}

ConstEvalResult Coerce::const_eval(ConstEvalContext& ctx) const {
	return Cast(a, target_type).const_eval(ctx);
}

const char* CoerceCheck::diagnostic_kind() const {
	return "coerce_check";
}

void CoerceCheck::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	UnaryOperation::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(target_type);
}

void CoerceCheck::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "coerce_check " << ctx->known_value_ref(a) << " is " << ctx->known_type_ref(target_type) << " : " << ctx->known_type_ref(ty);
}

const char* AddrOf::diagnostic_kind() const {
	return "addr_of";
}

const char* RoutineRef::diagnostic_kind() const {
	return "routine_ref";
}

void RoutineRef::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_value_edge(receiver);
	ctx->add_value_edge(candidates);
	ctx->add_value_edge(resolved);
}

void RoutineRef::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "routine_ref : " << ctx->known_type_ref(ty);
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "candidates: " << ctx->known_value_ref(candidates);
	if (receiver) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "receiver: " << ctx->known_value_ref(receiver);
	}
	if (resolved) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "resolved: " << ctx->known_value_ref(resolved);
	}
}

const char* RoutineCode::diagnostic_kind() const {
	return "routine_code";
}

ConstEvalResult RoutineCode::const_eval(ConstEvalContext& ctx) const {
	ConstEvalResult folded = a->const_eval(ctx);
	if (folded.kind != ConstEvalResult::Kind::Success) {
		return folded;
	}
	auto result = new RoutineCode(folded.node);
	result->ty = ty;
	return ConstEvalResult::success(result);
}

static bool diagnostic_pas_ident_char(char ch) {
	unsigned char c = static_cast<unsigned char>(ch);
	return std::isalnum(c) || ch == '_';
}

static std::string diagnostic_pas_name(std::string name) {
	if (name.empty()) {
		return "<anonymous>";
	}
	bool ident = !std::isdigit(static_cast<unsigned char>(name.front()));
	for (char ch : name) {
		if (!diagnostic_pas_ident_char(ch)) {
			ident = false;
			break;
		}
	}
	if (ident) {
		return name;
	}
	// This is a displayed Pascal source name, not a diagnostic variable.
	// Use Pascal string quoting for symbolic names such as := so the
	// diagnostic says procedure ':=' rather than inventing a rename or using
	// C/C++ double-quoted spelling.
	std::string r = "'";
	for (char ch : name) {
		if (ch == '\'') {
			r.push_back('\'');
		}
		r.push_back(ch);
	}
	r.push_back('\'');
	return r;
}

const char* Callable::diagnostic_kind() const {
	return "callable";
}

void Callable::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(ty);
	// Do not traverse body_frame here. A callable candidate is identified by its
	// signature for overload/type diagnostics; walking routine locals would dump
	// unrelated implementation-scope values (including overload sets) into the
	// diagnostic graph.
}

void Callable::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << diagnostic_kind() << " " << diagnostic_pas_name(pas_name) << "\n";
	ctx->indent(out, indent + 1);
	out << "type: " << ctx->known_type_ref(ty) << "\n";
	ctx->indent(out, indent + 1);
	out << "external: " << (is_external ? "yes" : "no");
}

const char* Procedure::diagnostic_kind() const {
	return "procedure";
}

const char* Method::diagnostic_kind() const {
	return "method";
}

void Method::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Callable::collect_diagnostic_edges(ctx);
	ctx->add_type_edge(owner_class);
}

void Method::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "method " << diagnostic_pas_name(pas_name) << "\n";
	ctx->indent(out, indent + 1);
	out << "owner: " << ctx->known_type_ref(owner_class) << "\n";
	ctx->indent(out, indent + 1);
	out << "type: " << ctx->known_type_ref(ty) << "\n";
	ctx->indent(out, indent + 1);
	out << "external: " << (is_external ? "yes" : "no") << "\n";
	ctx->indent(out, indent + 1);
	out << "virtual: ";
	switch (virtual_kind) {
	case VirtualKind::None:
		out << "none";
		break;
	case VirtualKind::Virtual:
		out << "virtual";
		break;
	case VirtualKind::Override:
		out << "override";
		break;
	case VirtualKind::Abstract:
		out << "abstract";
		break;
	case VirtualKind::Dynamic:
		out << "dynamic";
		break;
	}
	out << "\n";
	ctx->indent(out, indent + 1);
	out << "static: " << (is_static ? "yes" : "no") << "\n";
	ctx->indent(out, indent + 1);
	out << "final: " << (is_final ? "yes" : "no");
}

const char* OverloadSet::diagnostic_kind() const {
	return "overload_set";
}

void OverloadSet::collect_diagnostic_edges(ErrorLetContext* ctx) const {
	Node::collect_diagnostic_edges(ctx);
	for (auto* m : members) {
		ctx->add_value_edge(m);
	}
}

void OverloadSet::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "overload set";
	for (auto* m : members) {
		out << "\n";
		ctx->indent(out, indent + 1);
		out << "member: " << ctx->known_value_ref(m);
	}
}
