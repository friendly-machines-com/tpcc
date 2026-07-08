#include "diagnostic.h"
#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "types.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

static bool diagnostic_ident_char(char ch) {
	unsigned char c = static_cast<unsigned char>(ch);
	return std::isalnum(c) || ch == '_';
}

static std::string quote_diagnostic_name(std::string s) {
	// Ada extended identifiers use backslash delimiters, e.g. \:=\. Use that
	// here so symbolic Pascal names can be diagnostic variables without lossy
	// renaming. Double any embedded backslash so the delimiter remains clear.
	std::string r = "\\";
	for (char ch : s) {
		if (ch == '\\')
			r += "\\\\";
		else
			r.push_back(ch);
	}
	r.push_back('\\');
	return r;
}

static std::string diagnostic_name_token(std::string s, const char* fallback) {
	if (s.empty())
		return fallback;
	bool identish = true;
	for (char ch : s) {
		if (!diagnostic_ident_char(ch)) {
			identish = false;
			break;
		}
	}
	if (identish && !std::isdigit(static_cast<unsigned char>(s.front())))
		return s;
	return quote_diagnostic_name(s);
}

static const char* diagnostic_param_mode_text(ParamMode mode) {
	switch (mode) {
	case ParamMode::Value:
		return "";
	case ParamMode::Var:
		return "var ";
	case ParamMode::Out:
		return "out ";
	case ParamMode::Const:
		return "const ";
	}
	return "";
}

static std::string routine_signature_detail(ErrorLetContext* ctx, const RoutineType* rt) {
	std::string r = "(";
	for (size_t i = 0; i < rt->formals.size(); ++i) {
		if (i)
			r += "; ";
		const auto& p = rt->formals[i];
		r += diagnostic_param_mode_text(p.mode);
		if (!p.pas_name.empty()) {
			r += p.pas_name;
			r += ": ";
		}
		r += ctx->known_type_display(p.ty);
	}
	r += ")";
	if (rt->return_type) {
		r += ": ";
		r += ctx->known_type_display(rt->return_type);
	}
	return r;
}

ErrorLetContext::ErrorLetContext(const Frame* naming_frame, unsigned max_depth)
    : ErrorLetContext(std::vector<DiagnosticScope>{{naming_frame, nullptr}}, max_depth) {}

ErrorLetContext::ErrorLetContext(std::vector<DiagnosticScope> scopes, unsigned max_depth)
    : naming_scopes(std::move(scopes)), max_depth(max_depth) {
	// Parser scopes are passed as an API-level naming context. They are not
	// Frame::parent, and they are not used for aggregate child frames. Process
	// from innermost to outermost so the first recorded name matches lookup
	// preference.
	for (auto it = naming_scopes.rbegin(); it != naming_scopes.rend(); ++it) {
		if (it->unwrap_via) {
			add_value_edge(it->unwrap_via);
			index_frame(it->frame, DiagnosticFrameUse::AggregateMembers);
		} else {
			index_frame(it->frame, DiagnosticFrameUse::NamingScope);
		}
	}
}

std::string ErrorLetContext::name_component(std::string s, const char* fallback) {
	// Internal diagnostic names are structured and unrendered. Preserve source
	// spelling here (including operators like :=); Ada quoting happens only in
	// render_name(), at the final output boundary.
	if (s.empty())
		return fallback;
	return s;
}

ErrorLetContext::TypeNode& ErrorLetContext::ensure_type(const Type* ty) {
	// NAME-EVIDENCE ONLY. Seeing a Type* in a scope/frame does not mean the
	// diagnostic references it. Do not push type_order here; discover_type() is
	// the only path that marks a Type* as printable.
	TypeNode& n = type_nodes[ty];
	if (!n.ty) {
		n.ty = ty;
		n.kind = ty ? ty->diagnostic_kind() : "type";
	}
	return n;
}

ErrorLetContext::ValueNode& ErrorLetContext::ensure_value(const Node* node) {
	// NAME-EVIDENCE ONLY. A scope/frame can contain huge unrelated values
	// (builtins, overload sets, locals). Merely seeing one must not emit it.
	// discover_value() is the only path that marks a Node* as printable.
	ValueNode& n = value_nodes[node];
	if (!n.node) {
		n.node = node;
		n.kind = node ? node->diagnostic_kind() : "value";
	}
	return n;
}

void ErrorLetContext::discover_type(const Type* ty, unsigned depth) {
	// GRAPH DISCOVERY. This is a real edge/root in the diagnostic graph; only
	// nodes that pass through here may get `let type ...` definitions.
	if (!ty)
		return;
	TypeNode& n = ensure_type(ty);
	if (!n.referenced)
		n.referenced = true;
	n.min_depth = std::min(n.min_depth, depth);
	if (depth >= max_depth) {
		n.truncated = true;
		if (!n.ordered) {
			n.ordered = true;
			type_order.push_back(ty);
		}
		return;
	}
	if (n.discovered || n.discovering)
		return;
	n.discovering = true;
	unsigned saved = current_depth;
	current_depth = depth;
	ty->collect_diagnostic_edges(this);
	current_depth = saved;
	n.discovering = false;
	n.discovered = true;
	// Emit in postorder: dependencies discovered while collecting this type are
	// ordered before the type body that refers to them. Recursive edges cannot be
	// topologically sorted, but acyclic references are definition-before-use.
	if (!n.ordered) {
		n.ordered = true;
		type_order.push_back(ty);
	}
}

void ErrorLetContext::discover_value(const Node* node, unsigned depth) {
	// GRAPH DISCOVERY. This is a real edge/root in the diagnostic graph; only
	// nodes that pass through here may get `let value ...` definitions.
	if (!node)
		return;
	ValueNode& n = ensure_value(node);
	if (!n.referenced)
		n.referenced = true;
	n.min_depth = std::min(n.min_depth, depth);
	if (depth >= max_depth) {
		n.truncated = true;
		if (!n.ordered) {
			n.ordered = true;
			value_order.push_back(node);
		}
		return;
	}
	if (n.discovered || n.discovering)
		return;
	n.discovering = true;
	unsigned saved = current_depth;
	current_depth = depth;
	node->collect_diagnostic_edges(this);
	current_depth = saved;
	n.discovering = false;
	n.discovered = true;
	// Values are also postordered so their type/default/value dependencies are
	// defined before the value definition that references them.
	if (!n.ordered) {
		n.ordered = true;
		value_order.push_back(node);
	}
}

void ErrorLetContext::add_type_edge(const Type* ty) {
	discover_type(ty, current_depth + 1);
}

void ErrorLetContext::add_value_edge(const Node* node) {
	discover_value(node, current_depth + 1);
}

void ErrorLetContext::add_frame_edge(const Frame* frame, DiagnosticFrameUse use) {
	index_frame(frame, use);
}

void ErrorLetContext::index_frame(const Frame* frame, DiagnosticFrameUse use) {
	// Invariant: frame indexing is NOT graph discovery. It is only how the
	// diagnostic context learns nice names for Type*/Node* pointers. Do not call
	// discover_type/discover_value here. Otherwise every visible value/member in
	// a scope or aggregate frame (including overload sets and builtins) becomes a
	// free-floating `let`, even when no printed type/value references it.
	if (!frame)
		return;
	auto key = std::make_pair(frame, use);
	if (indexed_frames.count(key))
		return;
	indexed_frames.insert(key);

	for (const auto& item : frame->types_local()) {
		const std::string& name = item.first;
		const Type* ty = item.second;
		if (!ty)
			continue;
		ensure_type(ty).type_names.push_back(name);
		if (auto inc = dynamic_cast<const IncompleteType*>(ty)) {
			if (inc->resolved)
				ensure_type(inc->resolved).type_names.push_back(name);
		}
	}

	for (const auto& item : frame->values_local()) {
		const std::string& name = item.first;
		const FrameValueEntry& entry = item.second;
		if (entry.ty) {
			TypeNode& tn = ensure_type(entry.ty);
			if (use == DiagnosticFrameUse::AggregateMembers)
				tn.member_names.push_back(name);
			else
				tn.value_names.push_back(name);
		}
		if (entry.value) {
			ValueNode& vn = ensure_value(entry.value);
			if (use == DiagnosticFrameUse::AggregateMembers)
				vn.member_names.push_back(name);
			else
				vn.value_names.push_back(name);
			// This is name evidence only. Do not discover every value in a frame:
			// scope and aggregate frames can contain huge unrelated value graphs
			// (including overload sets). Values become definitions only when a
			// diagnostic explicitly references them via value_ref/add_value_edge.
		}
	}

	(void)use;
}

ErrorLetContext::NameBase ErrorLetContext::choose_type_base(const TypeNode& n) {
	if (!n.type_names.empty())
		return NameBase{name_component(n.type_names.front(), n.kind.c_str()), ""};
	if (!n.value_names.empty())
		return NameBase{name_component("typeof_" + n.value_names.front(), n.kind.c_str()), ""};
	if (!n.member_names.empty())
		return NameBase{name_component("member_" + n.member_names.front(), n.kind.c_str()), ""};
	if (auto rt = dynamic_cast<const RoutineType*>(n.ty)) {
		// Hard-coded inline special case: routine type variables are named by
		// their signature. Keep this structured as head + detail so uniqueness
		// suffixes belong to the diagnostic variable head (`routine#2(...)`), not
		// to an embedded parameter/result type ref.
		return NameBase{name_component("routine", n.kind.c_str()), routine_signature_detail(this, rt)};
	}
	return NameBase{name_component(n.kind, "type"), ""};
}

ErrorLetContext::NameBase ErrorLetContext::choose_value_base(const ValueNode& n) const {
	if (!n.value_names.empty())
		return NameBase{name_component(n.value_names.front(), n.kind.c_str()), ""};

	// Derived expressions usually have no direct Frame entry. Give common
	// selector/conversion intrinsics source-shaped diagnostic-local names from
	// already-discovered operands. This is graph-local naming only: it neither
	// mutates IR nor parses rendered output strings. Values are named in
	// postorder, so operand names are normally assigned before their user.
	if (auto ma = dynamic_cast<const MemberAccess*>(n.node)) {
		auto ait = value_nodes.find(ma->a);
		auto bit = value_nodes.find(ma->b);
		if (ait != value_nodes.end() && ait->second.name.assigned && bit != value_nodes.end()) {
			std::string member;
			if (!bit->second.member_names.empty())
				member = bit->second.member_names.front();
			else if (!bit->second.value_names.empty())
				member = bit->second.value_names.front();
			else if (bit->second.name.assigned)
				member = render_name_display(bit->second.name);
			if (!member.empty())
				return NameBase{name_component(render_name_display(ait->second.name) + "." + member, n.kind.c_str()), ""};
		}
	}
	if (auto ix = dynamic_cast<const Index*>(n.node)) {
		auto ait = value_nodes.find(ix->a);
		auto bit = value_nodes.find(ix->b);
		if (ait != value_nodes.end() && ait->second.name.assigned &&
		    bit != value_nodes.end() && bit->second.name.assigned) {
			return NameBase{name_component(render_name_display(ait->second.name) + "[" +
							   render_name_display(bit->second.name) + "]",
						       n.kind.c_str()),
					""};
		}
	}
	if (auto d = dynamic_cast<const Dereference*>(n.node)) {
		auto ait = value_nodes.find(d->a);
		if (ait != value_nodes.end() && ait->second.name.assigned)
			return NameBase{name_component(render_name_display(ait->second.name) + "^", n.kind.c_str()), ""};
	}
	if (auto c = dynamic_cast<const Cast*>(n.node)) {
		auto ait = value_nodes.find(c->a);
		auto tit = type_nodes.find(c->ty);
		if (ait != value_nodes.end() && ait->second.name.assigned &&
		    tit != type_nodes.end() && tit->second.name.assigned) {
			return NameBase{name_component(render_name_display(tit->second.name) + "(" +
							   render_name_display(ait->second.name) + ")",
						       n.kind.c_str()),
					""};
		}
	}
	if (auto tb = dynamic_cast<const TypeBound*>(n.node)) {
		auto tit = type_nodes.find(tb->operand_type);
		if (tit != type_nodes.end() && tit->second.name.assigned) {
			std::string fn = tb->kind == TypeBoundKind::Low ? "low" : "high";
			return NameBase{name_component(fn + "(" + render_name_display(tit->second.name) + ")", n.kind.c_str()), ""};
		}
	}
	if (auto l = dynamic_cast<const Length*>(n.node)) {
		auto ait = value_nodes.find(l->a);
		if (ait != value_nodes.end() && ait->second.name.assigned)
			return NameBase{name_component("length(" + render_name_display(ait->second.name) + ")", n.kind.c_str()), ""};
	}
	if (auto pc = dynamic_cast<const ProcCall*>(n.node)) {
		auto callee_it = value_nodes.find(pc->callee);
		if (callee_it != value_nodes.end() && callee_it->second.name.assigned) {
			std::string rendered;
			if (pc->receiver) {
				auto receiver_it = value_nodes.find(pc->receiver);
				if (receiver_it == value_nodes.end() || !receiver_it->second.name.assigned)
					goto no_proc_call_name;
				rendered = render_name_display(receiver_it->second.name);
				rendered += ".";
			}
			rendered += render_name_display(callee_it->second.name);
			rendered += "(";
			for (size_t i = 0; i < pc->args.size(); ++i) {
				if (i)
					rendered += ", ";
				auto arg_it = value_nodes.find(pc->args[i]);
				if (arg_it != value_nodes.end() && arg_it->second.name.assigned)
					rendered += render_name_display(arg_it->second.name);
				else
					rendered += "...";
			}
			rendered += ")";
			return NameBase{name_component(rendered, n.kind.c_str()), ""};
		}
	}
no_proc_call_name:

	if (!n.member_names.empty())
		return NameBase{name_component("member_" + n.member_names.front(), n.kind.c_str()), ""};
	// Overload-set members are often not directly present as Frame::values_local()
	// entries: the frame stores the OverloadSet under the source name, while its
	// Callable members only carry Callable::pas_name. Use that before falling
	// back to the bland dynamic kind ("procedure", "method", ...), otherwise
	// diagnostics say `value procedure =` for every operator candidate.
	if (auto c = dynamic_cast<const Callable*>(n.node)) {
		if (!c->pas_name.empty()) {
			std::string detail;
			if (c->ty) {
				auto it = type_nodes.find(c->ty);
				if (it != type_nodes.end() && !!it->second.name.assigned) {
					detail = " : ";
					detail += render_name_display(it->second.name);
				}
			}
			return NameBase{name_component(c->pas_name, n.kind.c_str()), detail};
		}
		if (!c->cxx_name.empty())
			return NameBase{name_component(c->cxx_name, n.kind.c_str()), ""};
	}
	if (auto s = dynamic_cast<const StorageSlot*>(n.node)) {
		if (!s->cxx_name.empty())
			return NameBase{name_component(s->cxx_name, n.kind.c_str()), ""};
	}
	return NameBase{name_component(n.kind, "value"), ""};
}

ErrorLetContext::DiagnosticName ErrorLetContext::uniquify(NameBase base) {
	std::string key = base.head;
	key.push_back('\0');
	key += base.detail;
	unsigned& count = used_names[key];
	count++;

	DiagnosticName name;
	name.head = std::move(base.head);
	name.detail = std::move(base.detail);
	name.suffix = count == 1 ? 0 : count;
	name.assigned = true;
	return name;
}

std::string ErrorLetContext::render_name(const DiagnosticName& name) const {
	if (!name.assigned)
		return "<unnamed>";
	std::string rendered = name.head;
	if (name.suffix)
		rendered += "#" + std::to_string(name.suffix);
	rendered += name.detail;
	return diagnostic_name_token(std::move(rendered), "value");
}

std::string ErrorLetContext::render_name_display(const DiagnosticName& name) const {
	if (!name.assigned)
		return "<unnamed>";
	std::string rendered = name.head;
	if (name.suffix)
		rendered += "#" + std::to_string(name.suffix);
	rendered += name.detail;
	return rendered;
}

void ErrorLetContext::assign_names() {
	// Only referenced nodes get names. Name-evidence-only entries intentionally
	// remain unnamed and are skipped by notes().
	//
	// Names are diagnostic-local variable bindings. Once a referenced node has
	// been named, keep that binding stable: callers may already have embedded
	// the returned ref text in the main error message while later refs discover
	// more graph nodes. Do not clear used_names and do not rename old nodes.
	for (const Type* ty : type_order) {
		if (!ty || dynamic_cast<const RoutineType*>(ty))
			continue;
		TypeNode& n = type_nodes[ty];
		if (!n.referenced || n.name.assigned)
			continue;
		n.name = uniquify(choose_type_base(n));
	}
	for (const Type* ty : type_order) {
		if (!ty || !dynamic_cast<const RoutineType*>(ty))
			continue;
		TypeNode& n = type_nodes[ty];
		if (!n.referenced || n.name.assigned)
			continue;
		n.name = uniquify(choose_type_base(n));
	}
	for (const Node* node : value_order) {
		if (!node)
			continue;
		ValueNode& n = value_nodes[node];
		if (!n.referenced || n.name.assigned)
			continue;
		n.name = uniquify(choose_value_base(n));
	}
}

void ErrorLetContext::prepare() {
	assign_names();
	prepared = true;
}

std::string ErrorLetContext::type_ref(const Type* ty) {
	if (!ty)
		return "<unknown type>";
	discover_type(ty, 0);
	prepare();
	return known_type_ref(ty);
}

std::string ErrorLetContext::value_ref(const Node* node) {
	if (!node)
		return "<null value>";
	discover_value(node, 0);
	prepare();
	return known_value_ref(node);
}

std::string ErrorLetContext::known_type_ref(const Type* ty) const {
	if (!ty)
		return "<unknown type>";
	auto it = type_nodes.find(ty);
	if (it == type_nodes.end() || !it->second.name.assigned)
		return "<unregistered type>";
	return render_name(it->second.name);
}

std::string ErrorLetContext::known_type_display(const Type* ty) const {
	if (!ty)
		return "<unknown type>";
	auto it = type_nodes.find(ty);
	if (it == type_nodes.end() || !it->second.name.assigned)
		return "<unregistered type>";
	return render_name_display(it->second.name);
}

std::string ErrorLetContext::known_value_ref(const Node* node) const {
	if (!node)
		return "<null value>";
	auto it = value_nodes.find(node);
	if (it == value_nodes.end() || !it->second.name.assigned)
		return "<unregistered value>";
	return render_name(it->second.name);
}

void ErrorLetContext::indent(std::ostringstream& out, unsigned level) const {
	for (unsigned i = 0; i < level; i++)
		out << "  ";
}

static void print_source_location(std::ostringstream& out, const SourceLocation& loc) {
	std::string text = loc.file_name.empty() ? "<unknown>" : loc.file_name;
	if (loc.line_number != 0) {
		text += "(";
		text += std::to_string(loc.line_number);
		text += ")";
	}
	out << "'";
	for (char ch : text) {
		if (ch == '\'')
			out << "''";
		else
			out << ch;
	}
	out << "'";
}
void ErrorLetContext::print_frame_members(std::ostringstream& out, const Frame* frame, unsigned indent_level) const {
	if (!frame)
		return;
	for (const auto& item : frame->values_local()) {
		const std::string& name = item.first;
		const FrameValueEntry& entry = item.second;
		indent(out, indent_level);
		if (dynamic_cast<StorageSlot*>(entry.value)) {
			out << name << ": " << known_type_ref(entry.ty) << ";\n";
		} else if (dynamic_cast<Callable*>(entry.value) || dynamic_cast<OverloadSet*>(entry.value)) {
			out << name << ": " << known_value_ref(entry.value) << ";\n";
		} else if (entry.value) {
			out << name << ": " << known_value_ref(entry.value) << ";\n";
		}
	}
}

std::string ErrorLetContext::notes() {
	prepare();
	if (type_order.empty() && value_order.empty())
		return "";

	std::ostringstream out;
	out << "\n\n  where\n\n";

	for (const Type* ty : type_order) {
		if (!ty)
			continue;
		const TypeNode& n = type_nodes.at(ty);
		if (!n.referenced || !n.name.assigned)
			continue;
		indent(out, 2);
		out << "type " << render_name(n.name) << " =\n";
		indent(out, 3);
		// The type-form head is emitted here, once, for every Type. This is a
		// diagnostic format invariant, not a convention each Type subclass must
		// remember. Type::print_diagnostic_definition/stub print only details after
		// this head.
		out << ty->diagnostic_kind() << "\n";
		indent(out, 4);
		out << "source: ";
		print_source_location(out, ty->source_location);
		if (n.truncated)
			ty->print_diagnostic_stub(this, out, 3);
		else
			ty->print_diagnostic_definition(this, out, 3);
		out << "\n\n";
	}

	for (const Node* node : value_order) {
		if (!node)
			continue;
		const ValueNode& n = value_nodes.at(node);
		if (!n.referenced || !n.name.assigned)
			continue;
		indent(out, 2);
		out << "value " << render_name(n.name) << " =\n";
		indent(out, 3);
		if (n.truncated)
			node->print_diagnostic_stub(this, out, 3);
		else
			node->print_diagnostic_definition(this, out, 3);
		out << "\n\n";
	}

	return out.str();
}
