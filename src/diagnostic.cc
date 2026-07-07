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

static std::string unquote_diagnostic_name_for_composition(const std::string& s) {
	// When composing a larger diagnostic name (e.g. an operator name plus a
	// routine signature), use the inner text of an Ada-quoted name so the final
	// outer name is \:= : routine(...)\ rather than nested/doubled quoting.
	if (s.size() >= 2 && s.front() == '\\' && s.back() == '\\') {
		std::string r;
		for (size_t i = 1; i + 1 < s.size(); ++i) {
			if (s[i] == '\\' && i + 2 < s.size() && s[i + 1] == '\\') {
				r.push_back('\\');
				++i;
			} else {
				r.push_back(s[i]);
			}
		}
		return r;
	}
	return s;
}

static const char* diagnostic_param_mode_text(ParamMode mode) {
	switch (mode) {
	case ParamMode::Value: return "";
	case ParamMode::Var: return "var ";
	case ParamMode::Out: return "out ";
	case ParamMode::Const: return "const ";
	}
	return "";
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

std::string ErrorLetContext::sanitize(std::string s, const char* fallback) {
	// Diagnostic-let names are allowed to be quoted with Ada extended-identifier
	// syntax instead of inventing lossy aliases for operators. Thus a Pascal
	// operator named := becomes the diagnostic variable \:=\ rather than a
	// made-up `assign`.
	return diagnostic_name_token(std::move(s), fallback);
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
	if (!n.referenced) {
		n.referenced = true;
		type_order.push_back(ty);
	}
	n.min_depth = std::min(n.min_depth, depth);
	if (depth >= max_depth) {
		n.truncated = true;
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
}

void ErrorLetContext::discover_value(const Node* node, unsigned depth) {
	// GRAPH DISCOVERY. This is a real edge/root in the diagnostic graph; only
	// nodes that pass through here may get `let value ...` definitions.
	if (!node)
		return;
	ValueNode& n = ensure_value(node);
	if (!n.referenced) {
		n.referenced = true;
		value_order.push_back(node);
	}
	n.min_depth = std::min(n.min_depth, depth);
	if (depth >= max_depth) {
		n.truncated = true;
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

std::string ErrorLetContext::choose_type_base(const TypeNode& n) {
	if (!n.type_names.empty())
		return sanitize(n.type_names.front(), n.kind.c_str());
	if (!n.value_names.empty())
		return sanitize("typeof_" + n.value_names.front(), n.kind.c_str());
	if (!n.member_names.empty())
		return sanitize("member_" + n.member_names.front(), n.kind.c_str());
	if (auto rt = dynamic_cast<const RoutineType*>(n.ty)) {
		// Hard-coded inline special case: routine types are usually clearer as
		// their signature than as routine#N. The signature still reuses the
		// diagnostic let refs of parameter/result types, so recursive/sharing
		// handling remains centralized in the graph context.
		std::string r = "routine(";
		for (size_t i = 0; i < rt->formals.size(); ++i) {
			if (i)
				r += "; ";
			const auto& p = rt->formals[i];
			r += diagnostic_param_mode_text(p.mode);
			if (!p.pas_name.empty()) {
				r += p.pas_name;
				r += ": ";
			}
			r += unquote_diagnostic_name_for_composition(known_type_ref(p.ty));
		}
		r += ")";
		if (rt->return_type) {
			r += ": ";
			r += unquote_diagnostic_name_for_composition(known_type_ref(rt->return_type));
		}
		return sanitize(r, n.kind.c_str());
	}
	return sanitize(n.kind, "type");
}

std::string ErrorLetContext::choose_value_base(const ValueNode& n) const {
	if (!n.value_names.empty())
		return sanitize(n.value_names.front(), n.kind.c_str());
	if (!n.member_names.empty())
		return sanitize("member_" + n.member_names.front(), n.kind.c_str());
	// Overload-set members are often not directly present as Frame::values_local()
	// entries: the frame stores the OverloadSet under the source name, while its
	// Callable members only carry Callable::pas_name. Use that before falling
	// back to the bland dynamic kind ("procedure", "method", ...), otherwise
	// diagnostics say `value procedure =` for every operator candidate.
	if (auto c = dynamic_cast<const Callable*>(n.node)) {
		if (!c->pas_name.empty()) {
			// Overloaded callables with the same Pascal name need better diagnostic
			// variable names than just \:=\, \:=\#2, ... . By the time value names
			// are assigned, assign_names() has already assigned all referenced Type*
			// names, so include the RoutineType let-ref when available. This reuses
			// the surrounding type graph instead of expanding the signature inline.
			if (c->ty) {
				auto it = type_nodes.find(c->ty);
				if (it != type_nodes.end() && !it->second.name.empty())
					return sanitize(c->pas_name + " : " + unquote_diagnostic_name_for_composition(it->second.name), n.kind.c_str());
			}
			return sanitize(c->pas_name, n.kind.c_str());
		}
		if (!c->cxx_name.empty())
			return sanitize(c->cxx_name, n.kind.c_str());
	}
	if (auto s = dynamic_cast<const StorageSlot*>(n.node)) {
		if (!s->cxx_name.empty())
			return sanitize(s->cxx_name, n.kind.c_str());
	}
	return sanitize(n.kind, "value");
}

std::string ErrorLetContext::uniquify(std::string base) {
	unsigned& count = used_names[base];
	count++;
	if (count == 1)
		return base;

	std::string suffix = "#" + std::to_string(count);
	// If the base is an Ada-quoted diagnostic name, keep the uniqueness suffix
	// inside the quotes: \routine(...)#2\, not \routine(...)\#2. The latter
	// is no longer one diagnostic variable name and composes badly when another
	// quoted name embeds this ref.
	if (base.size() >= 2 && base.front() == '\\' && base.back() == '\\') {
		base.insert(base.end() - 1, suffix.begin(), suffix.end());
		return base;
	}
	return base + suffix;
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
		if (!n.referenced || !n.name.empty())
			continue;
		n.name = uniquify(choose_type_base(n));
	}
	for (const Type* ty : type_order) {
		if (!ty || !dynamic_cast<const RoutineType*>(ty))
			continue;
		TypeNode& n = type_nodes[ty];
		if (!n.referenced || !n.name.empty())
			continue;
		n.name = uniquify(choose_type_base(n));
	}
	for (const Node* node : value_order) {
		if (!node)
			continue;
		ValueNode& n = value_nodes[node];
		if (!n.referenced || !n.name.empty())
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
	if (it == type_nodes.end() || it->second.name.empty())
		return "<unregistered type>";
	return it->second.name;
}

std::string ErrorLetContext::known_value_ref(const Node* node) const {
	if (!node)
		return "<null value>";
	auto it = value_nodes.find(node);
	if (it == value_nodes.end() || it->second.name.empty())
		return "<unregistered value>";
	return it->second.name;
}

void ErrorLetContext::indent(std::ostringstream& out, unsigned level) const {
	for (unsigned i = 0; i < level; i++)
		out << "  ";
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
	out << "\ndiagnostic details:\n";
	out << "  let\n";

	for (const Type* ty : type_order) {
		if (!ty)
			continue;
		const TypeNode& n = type_nodes.at(ty);
		if (!n.referenced || n.name.empty())
			continue;
		indent(out, 2);
		out << "type " << n.name << " =\n";
		indent(out, 3);
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
		if (!n.referenced || n.name.empty())
			continue;
		indent(out, 2);
		out << "value " << n.name << " =\n";
		indent(out, 3);
		if (n.truncated)
			node->print_diagnostic_stub(this, out, 3);
		else
			node->print_diagnostic_definition(this, out, 3);
		out << "\n\n";
	}

	return out.str();
}
