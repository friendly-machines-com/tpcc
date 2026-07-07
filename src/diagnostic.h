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
	const Node* unwrap_via; // non-null for Parser with-scopes; otherwise null
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

	// Called by Type/Node implementations while graph-discovering.
	void add_type_edge(const Type* ty);
	void add_value_edge(const Node* node);
	void add_frame_edge(const Frame* frame, DiagnosticFrameUse use);

	// Called by Type/Node implementations while printing definitions.
	std::string known_type_ref(const Type* ty) const;
	std::string known_value_ref(const Node* node) const;
	void indent(std::ostringstream& out, unsigned level) const;
	void print_frame_members(std::ostringstream& out, const Frame* frame, unsigned indent_level) const;

private:
	struct TypeNode {
		const Type* ty = nullptr;
		std::string name;
		std::string kind;
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
		std::string name;
		std::string kind;
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
	std::set<const Frame*> indexed_frames;
	std::map<std::string, unsigned> used_names;

	TypeNode& ensure_type(const Type* ty);
	ValueNode& ensure_value(const Node* node);
	void discover_type(const Type* ty, unsigned depth);
	void discover_value(const Node* node, unsigned depth);
	void index_frame(const Frame* frame, DiagnosticFrameUse use);
	void prepare();
	void assign_names();
	std::string choose_type_base(const TypeNode& n) const;
	std::string choose_value_base(const ValueNode& n) const;
	std::string uniquify(std::string base);
	static std::string sanitize(std::string s, const char* fallback);
};
