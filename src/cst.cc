#include "cst.h"
#include <sstream>

std::string Node::str() const {
	std::stringstream sst;
	sst << (void*)this;
	return sst.str();
}

void Block::add(Node* stmt) {
	statements.push_back(stmt);
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

ProcCall::ProcCall(Node* receiver, Node* callee, std::vector<Node*> args)
    : receiver(receiver), callee(callee), args(std::move(args)) {}
Assign::Assign(Node* a, Node* b) : BinaryOperation(a, b) {}
ShortCircuitOperation::ShortCircuitOperation(enum ShortCircuitOperationKind, Node* a, Node* b): BinaryOperation(a, b) {
	this->kind = kind;
}
MemberAccess::MemberAccess(Node* a, Node* b) : BinaryOperation(a, b) {}
Index::Index(Node* a, Node* b) : BinaryOperation(a, b) {}
Coerce::Coerce(Node* a, Node* b) : BinaryOperation(a, b) {}
CoerceCheck::CoerceCheck(Node* a, Node* b) : BinaryOperation(a, b) {}

Dereference::Dereference(Node* a) : UnaryOperation(a) {}
Return::Return(Node* a) : UnaryOperation(a) {}
AddrOf::AddrOf(Node* a) : UnaryOperation(a) {}
Cast::Cast(Node* value, Type* target) : UnaryOperation(value) { this->ty = target; }

// Value-identifier ctors: take an OPTIONAL Pascal name. If non-empty, apply
// the `p_` prefix so the cxx identifier stays clear of C++ reserved words
// (`new`, `class`, `false`, ...). If empty (anonymous entity -- nameless
// parameter in a prototype, compiler temporary, procedural-type declaration
// `procedure of object`, etc.), cxx_name stays empty and emission skips it.
StorageSlot::StorageSlot(std::string cxx_name, Type* ty) {
	this->cxx_name = cxx_name;
	this->ty = ty;
}

EnumMemberRef::EnumMemberRef(std::string cxx_name, int64_t value, Type* ty) {
	this->cxx_name = cxx_name;
	this->value = value;
	this->ty = ty;
}

Integer::Integer(uint64_t value, Type* ty) {
	this->value = value;
	this->ty = ty;
}

String::String(std::string value, Type* ty) {
	this->value = std::move(value);
	this->ty = ty;
}

Callable::Callable(std::string cxx_name,
		   std::string pas_name,
		   RoutineType* ty,
		   bool has_overload_directive)
    : cxx_name(std::move(cxx_name)),
      pas_name(std::move(pas_name)),
      ty(ty),
      has_overload_directive(has_overload_directive),
      has_body(false),
      body_frame(nullptr) {
}

Procedure::Procedure(std::string cxx_name,
		     std::string pas_name,
		     RoutineType* ty,
		     bool has_overload_directive)
    : Callable(std::move(cxx_name),
	       std::move(pas_name),
	       ty,
	       has_overload_directive) {
}

Method::Method(std::string cxx_name,
	       std::string pas_name,
	       RoutineType* ty,
	       bool has_overload_directive,
	       Type* owner_class,
	       VirtualKind virtual_kind)
    : Callable(std::move(cxx_name),
	       std::move(pas_name),
	       ty,
	       has_overload_directive),
      owner_class(owner_class),
      virtual_kind(virtual_kind),
      vtable_slot(-1) {}

OverloadSet::OverloadSet(std::vector<Callable*> members)
    : members(std::move(members)) {}

#include "diagnostic.h"
#include "frame.h"
#include "types.h"
#include <algorithm>

const char* Node::diagnostic_kind() const { return "value"; }
void Node::collect_diagnostic_edges(ErrorLetContext* ctx) const { ctx->add_type_edge(ty); }
void Node::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << diagnostic_kind() << " : " << ctx->known_type_ref(ty); }
void Node::print_diagnostic_stub(ErrorLetContext*, std::ostringstream& out, unsigned) const { out << "value ..."; }

const char* Block::diagnostic_kind() const { return "block"; }
void Block::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); for (auto* s : statements) ctx->add_value_edge(s); }
void Block::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "block";
	for (auto* s : statements) { out << "\n"; ctx->indent(out, indent + 1); out << ctx->known_value_ref(s); }
	out << "\n"; ctx->indent(out, indent); out << "end";
}

const char* Symbol::diagnostic_kind() const { return "symbol"; }
void Symbol::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "symbol " << str() << " : " << ctx->known_type_ref(ty); }

void UnaryOperation::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); ctx->add_value_edge(a); }
void UnaryOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << diagnostic_kind() << " " << ctx->known_value_ref(a) << " : " << ctx->known_type_ref(ty); }

void BinaryOperation::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); ctx->add_value_edge(a); ctx->add_value_edge(b); }
void BinaryOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << diagnostic_kind() << " " << ctx->known_value_ref(a) << ", " << ctx->known_value_ref(b) << " : " << ctx->known_type_ref(ty); }

const char* ProcCall::diagnostic_kind() const { return "call"; }
void ProcCall::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); ctx->add_value_edge(receiver); ctx->add_value_edge(callee); for (auto* a : args) ctx->add_value_edge(a); }
void ProcCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "call\n"; ctx->indent(out, indent + 1); out << "callee: " << ctx->known_value_ref(callee) << "\n";
	if (receiver) { ctx->indent(out, indent + 1); out << "receiver: " << ctx->known_value_ref(receiver) << "\n"; }
	for (auto* a : args) { ctx->indent(out, indent + 1); out << "arg: " << ctx->known_value_ref(a) << "\n"; }
	ctx->indent(out, indent + 1); out << "returns: " << ctx->known_type_ref(ty);
}

const char* InheritedCall::diagnostic_kind() const { return "inherited_call"; }
void InheritedCall::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); ctx->add_value_edge(resolved); for (auto* a : args) ctx->add_value_edge(a); }
void InheritedCall::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "inherited call\n"; ctx->indent(out, indent + 1); out << "resolved: " << ctx->known_value_ref(resolved);
	for (auto* a : args) { out << "\n"; ctx->indent(out, indent + 1); out << "arg: " << ctx->known_value_ref(a); }
}

const char* Dereference::diagnostic_kind() const { return "deref"; }
void Dereference::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "deref " << ctx->known_value_ref(a) << " : " << ctx->known_type_ref(ty); }
const char* Assign::diagnostic_kind() const { return "assign"; }
const char* ShortCircuitOperation::diagnostic_kind() const { return kind == AND ? "and" : "or"; }
void ShortCircuitOperation::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { BinaryOperation::print_diagnostic_definition(ctx, out, 0); }
const char* MemberAccess::diagnostic_kind() const { return "member_access"; }
const char* Index::diagnostic_kind() const { return "index"; }
const char* Return::diagnostic_kind() const { return "return"; }

const char* Cast::diagnostic_kind() const { return "cast"; }
void Cast::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "cast " << ctx->known_value_ref(a) << " to " << ctx->known_type_ref(ty); }

const char* StorageSlot::diagnostic_kind() const { return "slot"; }
void StorageSlot::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "slot : " << ctx->known_type_ref(ty); }

const char* EnumMemberRef::diagnostic_kind() const { return "enum_member"; }
void EnumMemberRef::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "enum member = " << value << " : " << ctx->known_type_ref(ty); }

const char* Integer::diagnostic_kind() const { return "integer"; }
void Integer::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "integer " << value << " : " << ctx->known_type_ref(ty); }

const char* String::diagnostic_kind() const { return "string"; }
void String::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const {
	out << "string \"";
	for (size_t i = 0; i < std::min<size_t>(value.size(), 40); i++) out << value[i];
	if (value.size() > 40) out << "...";
	out << "\" : " << ctx->known_type_ref(ty);
}

const char* NilLiteral::diagnostic_kind() const { return "nil"; }
void NilLiteral::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned) const { out << "nil : " << ctx->known_type_ref(ty); }
const char* Coerce::diagnostic_kind() const { return "coerce"; }
const char* CoerceCheck::diagnostic_kind() const { return "coerce_check"; }
const char* AddrOf::diagnostic_kind() const { return "addr_of"; }


static bool diagnostic_pas_ident_char(char ch) {
	unsigned char c = static_cast<unsigned char>(ch);
	return std::isalnum(c) || ch == '_';
}

static std::string diagnostic_pas_name(std::string name) {
	if (name.empty())
		return "<anonymous>";
	bool ident = !std::isdigit(static_cast<unsigned char>(name.front()));
	for (char ch : name) {
		if (!diagnostic_pas_ident_char(ch)) {
			ident = false;
			break;
		}
	}
	if (ident)
		return name;
	// This is a displayed Pascal source name, not a diagnostic variable.
	// Use Pascal string quoting for symbolic names such as := so the
	// diagnostic says procedure ':=' rather than inventing a rename or using
	// C/C++ double-quoted spelling.
	std::string r = "'";
	for (char ch : name) {
		if (ch == '\'')
			r.push_back('\'');
		r.push_back(ch);
	}
	r.push_back('\'');
	return r;
}

const char* Callable::diagnostic_kind() const { return "callable"; }
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
	ctx->indent(out, indent + 1); out << "type: " << ctx->known_type_ref(ty) << "\n";
	ctx->indent(out, indent + 1); out << "external: " << (is_external ? "yes" : "no");
}
const char* Procedure::diagnostic_kind() const { return "procedure"; }

const char* Method::diagnostic_kind() const { return "method"; }
void Method::collect_diagnostic_edges(ErrorLetContext* ctx) const { Callable::collect_diagnostic_edges(ctx); ctx->add_type_edge(owner_class); }
void Method::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "method " << diagnostic_pas_name(pas_name) << "\n";
	ctx->indent(out, indent + 1); out << "owner: " << ctx->known_type_ref(owner_class) << "\n";
	ctx->indent(out, indent + 1); out << "type: " << ctx->known_type_ref(ty) << "\n";
	ctx->indent(out, indent + 1); out << "external: " << (is_external ? "yes" : "no") << "\n";
	ctx->indent(out, indent + 1); out << "virtual: ";
	switch (virtual_kind) { case VirtualKind::None: out << "none"; break; case VirtualKind::Virtual: out << "virtual"; break; case VirtualKind::Override: out << "override"; break; case VirtualKind::Abstract: out << "abstract"; break; case VirtualKind::Dynamic: out << "dynamic"; break; }
}

const char* OverloadSet::diagnostic_kind() const { return "overload_set"; }
void OverloadSet::collect_diagnostic_edges(ErrorLetContext* ctx) const { Node::collect_diagnostic_edges(ctx); for (auto* m : members) ctx->add_value_edge(m); }
void OverloadSet::print_diagnostic_definition(ErrorLetContext* ctx, std::ostringstream& out, unsigned indent) const {
	out << "overload set";
	for (auto* m : members) { out << "\n"; ctx->indent(out, indent + 1); out << "member: " << ctx->known_value_ref(m); }
}
