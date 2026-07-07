#include "diagnostic.h"
#include "builtins.h"
#include "cst.h"
#include "frame.h"
#include "types.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

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
	std::string r;
	for (char ch : s) {
		unsigned char c = static_cast<unsigned char>(ch);
		if (std::isalnum(c) || ch == '_')
			r.push_back(ch);
		else
			r.push_back('_');
	}
	while (!r.empty() && r.front() == '_')
		r.erase(r.begin());
	if (r.empty())
		r = fallback;
	if (std::isdigit(static_cast<unsigned char>(r.front())))
		r = std::string(fallback) + "_" + r;
	return r;
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

std::string ErrorLetContext::choose_type_base(const TypeNode& n) const {
	if (!n.type_names.empty())
		return sanitize(n.type_names.front(), n.kind.c_str());
	if (!n.value_names.empty())
		return sanitize("typeof_" + n.value_names.front(), n.kind.c_str());
	if (!n.member_names.empty())
		return sanitize("member_" + n.member_names.front(), n.kind.c_str());
	return sanitize(n.kind, "type");
}

std::string ErrorLetContext::choose_value_base(const ValueNode& n) const {
	if (!n.value_names.empty())
		return sanitize(n.value_names.front(), n.kind.c_str());
	if (!n.member_names.empty())
		return sanitize("member_" + n.member_names.front(), n.kind.c_str());
	return sanitize(n.kind, "value");
}

std::string ErrorLetContext::uniquify(std::string base) {
	unsigned& count = used_names[base];
	count++;
	if (count == 1)
		return base;
	return base + "#" + std::to_string(count);
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
		if (!ty)
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
