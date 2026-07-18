#pragma once

#include <climits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

class Frame;
class Node;
class Type;

// Role of a Frame edge in the diagnostic graph. This is not stored on Frame;
// it describes why this traversal is looking at that Frame.
enum class DiagnosticFrameUse {
	NamingScope,
	AggregateMembers,
	ModuleMembers,
	RoutineLocals,
};

struct DiagnosticScope {
	const Frame* frame;
	const Node* qualifier = nullptr;
};

class ErrorLetContext {
public:
	ErrorLetContext(const Frame* naming_frame, unsigned max_depth);
	ErrorLetContext(std::vector<DiagnosticScope> scopes, unsigned max_depth);

	std::string type_ref(const Type* ty);
	std::string value_ref(const Node* node);
	std::string ref(const Type* ty) { return type_ref(ty); }
	std::string ref(const Node* node) { return value_ref(node); }

	std::string notes();

	// Called by Type/Node implementations while graph-discovering. These are
	// the ONLY public operations (besides type_ref/value_ref roots) that may
	// make a Type*/Node* appear as a `let` definition in notes().
	void add_type_edge(const Type* ty);
	void add_value_edge(const Node* node);

	// Index names from a Frame reached while discovering a type/value. This must
	// remain NAME-EVIDENCE ONLY: it may attach source names to Type*/Node* table
	// entries, but it must not by itself make those entries printable. If a type
	// body needs field/member types in the graph, that owning Type must add those
	// Type* edges explicitly from collect_diagnostic_edges().
	void add_frame_edge(const Frame* frame, DiagnosticFrameUse use);

	// Called by Type/Node implementations while printing definitions.
	std::string known_type_ref(const Type* ty) const;
	std::string known_value_ref(const Node* node) const;
	std::string known_type_display(const Type* ty) const;
	// Aggregate frames are name evidence, not graph roots. Their printers use
	// this query to omit declarations whose values were not reached through an
	// explicit diagnostic edge; printing known_value_ref() for such a value
	// would create an identifier with no corresponding definition.
	bool has_known_value_ref(const Node* node) const;
	void indent(std::ostringstream& out, unsigned level) const;
	void print_frame_members(std::ostringstream& out, const Frame* frame, unsigned indent_level) const;

private:
	struct DiagnosticName {
		std::string head;
		std::string detail;
		unsigned suffix = 0;
		bool assigned = false;
	};

	struct NameBase {
		std::string head;
		std::string detail;
	};

	struct TypeNode {
		const Type* ty = nullptr;
		DiagnosticName name;
		std::string kind;
		bool referenced = false;
		bool ordered = false;
		bool discovered = false;
		bool discovering = false;
		bool truncated = false;
		unsigned min_depth = UINT_MAX;
		std::vector<std::string> type_names;
		std::vector<std::string> value_names;
		std::vector<std::string> member_names;
	};

	struct ValueNode {
		const Node* node = nullptr;
		DiagnosticName name;
		std::string kind;
		bool referenced = false;
		bool ordered = false;
		bool discovered = false;
		bool discovering = false;
		bool truncated = false;
		unsigned min_depth = UINT_MAX;
		std::vector<std::string> value_names;
		std::vector<std::string> member_names;
	};

	std::vector<DiagnosticScope> naming_scopes;
	unsigned max_depth;
	unsigned current_depth = 0;
	bool prepared = false;

	std::map<const Type*, TypeNode> type_nodes;
	std::map<const Node*, ValueNode> value_nodes;
	std::vector<const Type*> type_order;
	std::vector<const Node*> value_order;
	std::set<std::pair<const Frame*, DiagnosticFrameUse>> indexed_frames;
	std::map<std::string, unsigned> used_names;

	// ensure_* only creates bookkeeping entries so frame/scope indexing can
	// attach possible names. It must not set referenced and must not affect
	// output order.
	TypeNode& ensure_type(const Type* ty);
	ValueNode& ensure_value(const Node* node);

	// discover_* marks real graph reachability. Only referenced nodes are named
	// for output and emitted in notes().
	void discover_type(const Type* ty, unsigned depth);
	void discover_value(const Node* node, unsigned depth);

	// index_frame is intentionally not graph discovery. It records local names
	// from Frame's declaration ranges under the supplied edge role.
	void index_frame(const Frame* frame, DiagnosticFrameUse use);
	void prepare();
	void assign_names();
	NameBase choose_type_base(const TypeNode& n);
	NameBase choose_value_base(const ValueNode& n) const;
	DiagnosticName uniquify(NameBase base);
	std::string render_name(const DiagnosticName& name) const;
	std::string render_name_display(const DiagnosticName& name) const;
	static std::string name_component(std::string s, const char* fallback);
};
