#include "parser.h"
#include "builtins.h"
#include "cst.h"
#include "diagnostic.h"
#include "directive_expr.h"
#include "emit.h"
#include "evaluator.h"
#include "frame.h"
#include "operators.h"
#include "units.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// Forward declarations of file-static helpers defined further down.
// Needed because lookup_method_in_ancestors and parse_inherited (defined
// earlier in the file) reference these before their definitions.
static Frame* body_frame_of(Type* ty);
static Type* call_result_type(Node* callee);
static bool ordinal_range_for_type(Type* ty, OrdinalRange* out, std::string* error);
static bool enum_range_has_gaps(Type* ty, const OrdinalRange& range);
static Type* subrange_range_type(Type* ty);
static Type* overload_rank_type(Type* type);
static Type* integer_literal_natural_type(const Integer* literal);
static Integer* untyped_integer_constant(Node* expression);
static bool is_ordinal_intrinsic_argument(Type* ty);
static bool rank_less(const MatchRank& a, Type* a_formal, const MatchRank& b, Type* b_formal, const std::function<bool(Type*, Type*)>& direct_assignment_edge);
enum class BracketIntegerPreference {
	Array,
	Set,
};
static Type* infer_bracket_common_item_type(const std::vector<BracketLiteral::Item>& items, BracketIntegerPreference preference);

static ErrorLetContext make_error_let_context_from_scopes(const std::vector<ScopeEntry>& scopes, unsigned max_depth) {
	std::vector<DiagnosticScope> diagnostic_scopes;
	diagnostic_scopes.reserve(scopes.size());
	for (const ScopeEntry& scope : scopes) {
		diagnostic_scopes.push_back(DiagnosticScope{scope.frame, scope.qualifier});
	}
	return ErrorLetContext(std::move(diagnostic_scopes), max_depth);
}

static std::unordered_set<std::string> keywords = {
    "abstract",
    "and", // operator
    "array",       "as",
    "begin",       "bitpacked",
    "break",       "case",
    "class",       "const",
    "constructor", "continue",
    "destructor",
    "div", // operator
    "do",          "downto",
    "dynamic", // FIXME
    "else",        "end",
    "except",      "file",
    "final",       "finally",
    "forward", // FIXME directive ?
    "for",         "function",
    "goto",        "if",
    "in", // operator
    "inline",      "implementation",
    "inherited",   "interface",
    "is", // operator
    "label",
    "mod", // operator
    "nil",         "noreturn",
    "not", // operator
    "object",      "of",
    "operator",
    "or",  // operator
    "out", // FIXME directive ?
    "overload",    "override",
    "packed",      "procedure",
    "program",     "raise",
    "record",      "repeat",
    "set",
    "shl", // operator
    "shr", // operator
    "string",      "then",
    "to",          "try",
    "type",        "unit",
    "until",       "uses",
    "var",
    "virtual", // FIXME directive ?
    "while",       "with",
    "xor", // operator
};

static bool token_is_identifier(const std::string& token) {
	if (token.empty() || keywords.find(token) != keywords.end()) {
		return false;
	}
	const auto is_letter = [](char value) { return std::isalpha(static_cast<unsigned char>(value)); };
	const auto is_identifier_character = [&](char value) { return is_letter(value) || std::isdigit(static_cast<unsigned char>(value)) || value == '_'; };
	return (is_letter(token.front()) || token.front() == '_') && std::all_of(token.begin() + 1, token.end(), is_identifier_character);
}

Parser::Parser(UnitRegistry* unit_registry, Emitter* emitter, CompilerOptions* options) : unit_registry(unit_registry), emitter(emitter), options(options) {
}

static std::string cxx_label_name(std::string pas_name) {
	return "pas_label_" + pas_name;
}

void Parser::pop_input_file() {
	assert(!input_files.empty());
	fclose(input_files.back().input_file);
	input_files.pop_back();
	if (input_files.empty()) {
		input_file = nullptr;
		input_file_name = "";
		input_file_line_number = 0;
		input_char = EOF;
	} else {
		auto& p = input_files.back();
		input_file = p.input_file;
		input_file_name = p.input_file_name;
		input_file_line_number = p.input_file_line_number;
		input_char = fgetc(input_file);
	}
}

/** LL(2) lookahead to be able to identify ".." and disambiguate it from "5.." */
int Parser::peek_lowlevel() {
	int c = fgetc(input_file);
	if (c != EOF) {
		ungetc(c, input_file);
	}
	return c;
}

int Parser::consume_lowlevel() {
	int result = input_char;
	if (result == '\n') {
		++this->input_file_line_number;
	}
	if (!input_file) {
		input_char = EOF;
		return result;
	}
	input_char = fgetc(input_file);
	// If the current source is exhausted and there's a parent to fall back
	// to, pop and re-seed input_char from the parent's stream (which has
	// the char that was pending at push time waiting via ungetc).
	while (input_char == EOF && input_files.size() > 1) {
		pop_input_file();
	}
	return result;
}

void Parser::push_input_file(FILE* input_file, std::string input_file_name, int input_file_line_number) {
	push_input_file_and_buffer(input_file, input_file_name, input_file_line_number, nullptr, 0);
}

void Parser::push_input_file_and_buffer(FILE* input_file, std::string input_file_name, int input_file_line_number, std::unique_ptr<char[]> buffer, size_t buffer_len) {
	// If we're mid-parse, the tokenizer has one character already read from
	// the current source sitting in input_char. Push it back onto that
	// FILE*'s stream so the parent resumes on exactly the right byte after
	// this new source is popped.
	if (!input_files.empty() && input_char != EOF) {
		ungetc(input_char, this->input_file);
	}
	// Freeze the current line number into the outgoing top entry so pop
	// restores the caller's position, not the caller's start line.
	if (!input_files.empty()) {
		input_files.back().input_file_line_number = this->input_file_line_number;
	}
	input_files.push_back(ParserInputFile{
	    .input_file = input_file,
	    .input_file_name = input_file_name,
	    .input_file_line_number = input_file_line_number,
	    .owned_buffer = std::move(buffer),
	    .owned_buffer_len = buffer_len,
	});
	this->input_file = input_file;
	this->input_file_name = input_file_name;
	this->input_file_line_number = input_file_line_number;
	input_char = fgetc(input_file);
}

void Parser::push_scope(const Frame* scope, Node* qualifier) {
	this->scopes.push_back(ScopeEntry{scope, qualifier});
}

void Parser::pop_scope() {
	if (this->scopes.empty()) {
		fprintf(stderr, "internal compiler error: pop_scope on empty scope stack\n");
		abort();
	}
	this->scopes.pop_back();
}

void Parser::push_declaration_frame(Frame* frame) {
	if (!frame) {
		fprintf(stderr, "internal compiler error: null declaration frame\n");
		abort();
	}
	declaration_frames.push_back(frame);
}

void Parser::pop_declaration_frame() {
	if (declaration_frames.empty()) {
		fprintf(stderr, "internal compiler error: pop on empty declaration-frame stack\n");
		abort();
	}
	declaration_frames.pop_back();
}

Frame* Parser::current_declaration_frame() const {
	if (declaration_frames.empty()) {
		fprintf(stderr, "internal compiler error: no current declaration frame\n");
		abort();
	}
	return declaration_frames.back();
}

Unit* Parser::declaration_unit(Frame* frame) const {
	if (!current_unit || current_unit->is_program) {
		return nullptr; // program or no active source unit
	}
	if (frame == current_unit->frame) {
		return current_unit;
	}
	return nullptr;
}

SourceLocation Parser::current_location() const {
	return SourceLocation(input_file_name, input_file_line_number);
}

[[noreturn]] static void emit_diagnostic_at(const SourceLocation& loc, const char* severity, const std::string& message) {
	std::stringstream sst;
	if (!loc.file_name.empty()) {
		sst << loc.file_name;
		if (loc.line_number != 0) {
			sst << '(' << loc.line_number << ')';
		}
		sst << ": ";
	}
	sst << severity << ": " << message << std::endl;
	std::string r = sst.str();
	fprintf(stderr, "%s\n", r.c_str());
	fflush(stderr);
	exit(1);
}

[[noreturn]] static void emit_parse_error_at(const SourceLocation& loc, const std::string& message) {
	emit_diagnostic_at(loc, "error", message);
}

[[noreturn]] static void emit_fatal_error_at(const SourceLocation& loc, const std::string& message) {
	emit_diagnostic_at(loc, "fatal", message);
}

std::string Parser::enclosing_diagnostic_references(ErrorLetContext& ctx) const {
	std::stringstream sst;

	if (current_unit && current_unit->reference) {
		sst << "\n  " << (current_unit->is_program ? "program" : "unit") << ": " << ctx.value_ref(current_unit->reference);
	}
	if (current_routine) {
		// A Method's ordinary diagnostic definition already references its
		// owner type. Repeating that type in the primary message would add no
		// context; the routine reference makes the owner reachable in `where`.
		sst << "\n  routine: " << ctx.value_ref(current_routine);
	} else if (!current_type_declaration_name.empty()) {
		// While a named type RHS is open, its completed Type may not have been
		// published yet. The surrounding declaration frame nevertheless holds
		// the named placeholder. Use that existing graph node as the owner
		// reference; do not invent a second diagnostic-only type identity.
		Type* owner = nullptr;
		for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
			owner = it->frame->lookup_type(current_type_declaration_name);
			if (owner) {
				break;
			}
		}
		if (owner) {
			sst << "\n  owner type: " << ctx.type_ref(owner);
		}
	}
	return sst.str();
}

[[noreturn]] void Parser::emit_parse_error_at(SourceLocation loc, std::string message, ErrorLetContext& ctx) {
	::emit_parse_error_at(loc, complete_diagnostic_message(std::move(message), ctx));
}

std::string Parser::complete_diagnostic_message(std::string message, ErrorLetContext& ctx) const {
	// The primary message contains ordinary references to its enclosing unit,
	// routine, or owner type. Their definitions and all other details live in
	// this same graph's single `where` block. Appending notes anywhere else
	// would permit duplicate blocks or references whose definitions live in a
	// different ErrorLetContext.
	message += enclosing_diagnostic_references(ctx);
	message += ctx.notes();
	return message;
}

[[noreturn]] void Parser::emit_parse_error_at(SourceLocation loc, std::string message) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	emit_parse_error_at(std::move(loc), std::move(message), ctx);
}

[[noreturn]] void Parser::emit_fatal_error_at(SourceLocation loc, std::string message) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	::emit_fatal_error_at(loc, complete_diagnostic_message(std::move(message), ctx));
}

[[noreturn]] void Parser::raise_parse_error(std::string message) {
	emit_parse_error_at(current_location(), message);
}

[[noreturn]] Type* Parser::raise_type_parse_error(std::string message) {
	emit_parse_error_at(current_location(), message);
}

Type* Parser::raise_type_mismatch(std::string message, Type* expected, Type* got) {
	return raise_type_mismatch_at(current_location(), std::move(message), expected, got);
}

Type* Parser::raise_type_mismatch_at(SourceLocation location, std::string message, Type* expected, Type* got) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::string expected_ref = ctx.type_ref(expected);
	std::string got_ref = ctx.type_ref(got);
	std::stringstream sst;
	sst << message << ": expected type " << expected_ref << " but got type " << got_ref;
	emit_parse_error_at(std::move(location), sst.str(), ctx);
	return expected; // future non-fatal diagnostics can continue with the expected type
}

Type* Parser::raise_type_kind_mismatch(std::string message, const char* expected_kind, Type* got) {
	return raise_type_kind_mismatch_at(current_location(), std::move(message), expected_kind, got);
}

Type* Parser::raise_type_kind_mismatch_at(SourceLocation location, std::string message, const char* expected_kind, Type* got) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::string got_ref = ctx.type_ref(got);
	std::stringstream sst;
	sst << message << ": expected " << expected_kind << " type but got " << got_ref;
	emit_parse_error_at(std::move(location), sst.str(), ctx);
	return got; // future non-fatal diagnostics can continue with the parsed type
}

Type* Parser::raise_type_error(std::string message, Type* relevant) {
	return raise_type_error_at(current_location(), std::move(message), relevant);
}

Type* Parser::raise_type_error_at(SourceLocation location, std::string message, Type* relevant) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << message << "\n  type: " << ctx.type_ref(relevant);
	emit_parse_error_at(std::move(location), sst.str(), ctx);
	return relevant;
}

[[noreturn]] void Parser::raise_value_error(std::string message, Node* relevant) {
	raise_value_error_at(current_location(), std::move(message), relevant);
}

[[noreturn]] void Parser::raise_value_error_at(SourceLocation location, std::string message, Node* relevant) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << message << "\n  value: " << ctx.value_ref(relevant);
	emit_parse_error_at(std::move(location), sst.str(), ctx);
}

[[noreturn]] void Parser::raise_values_error(std::string message, const std::vector<std::pair<std::string, Node*>>& relevant) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << message;
	for (const auto& [label, value] : relevant) {
		sst << "\n  " << label << ": " << ctx.value_ref(value);
	}
	emit_parse_error_at(current_location(), sst.str(), ctx);
}

[[noreturn]] void Parser::raise_routine_reference_error(std::string message, RoutineRef* reference, Type* destination_type) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << message;
	if (destination_type) {
		sst << "\n  destination type: " << ctx.type_ref(destination_type);
	}
	if (reference) {
		sst << "\n  routine reference: " << ctx.value_ref(reference);
	}
	emit_parse_error_at(current_location(), sst.str(), ctx);
}

static const char* match_tier_name(MatchRank::Tier tier) {
	switch (tier) {
	case MatchRank::Tier::Exact:
		return "exact";
	case MatchRank::Tier::Equal:
		return "equal";
	case MatchRank::Tier::Convert:
		return "convert";
	case MatchRank::Tier::ConvertNarrowing:
		return "convert-narrowing";
	case MatchRank::Tier::Generic:
		return "generic";
	}
	return "invalid";
}

static void append_match_rank_vector(std::stringstream& sst, const std::vector<MatchRank>& ranks) {
	sst << "[";
	for (size_t i = 0; i < ranks.size(); ++i) {
		if (i) {
			sst << ", ";
		}
		sst << match_tier_name(ranks[i].tier);
	}
	sst << "]";
}

static const SourceLocation& callable_source_location(Callable* c) {
	static const SourceLocation unknown;
	if (!c || !c->ty) {
		return unknown;
	}
	return c->ty->source_location;
}

static bool callable_source_less(Callable* a, Callable* b) {
	const SourceLocation& la = callable_source_location(a);
	const SourceLocation& lb = callable_source_location(b);
	if (la < lb) {
		return true;
	}
	if (lb < la) {
		return false;
	}
	return false;
}

static void append_callable_source_prefix(std::stringstream& sst, Callable* c, bool is_viable, bool is_ambiguous = false) {
	const SourceLocation& loc = callable_source_location(c);
	if (!loc.file_name.empty()) {
		sst << loc.file_name << "(" << loc.line_number << ")";
	}
	if (is_ambiguous) {
		sst << "[ambiguous]";
	} else if (is_viable) {
		sst << "[viable]";
	}
	if (!loc.file_name.empty() || is_viable || is_ambiguous) {
		sst << ": ";
	}
}

static std::string match_preference_reason(const MatchRank& preferred, Type* preferred_formal, const MatchRank& other, Type* other_formal, Node* actual, ErrorLetContext& ctx, const std::function<bool(Type*, Type*)>& direct_assignment_edge) {
	if (preferred.tier != other.tier) {
		return std::string(match_tier_name(preferred.tier)) + " is better than " + match_tier_name(other.tier);
	}
	if (preferred.contextual_construction != other.contextual_construction) {
		return "uses the preferred contextual interpretation";
	}
	preferred_formal = overload_rank_type(preferred_formal);
	other_formal = overload_rank_type(other_formal);
	if (preferred_formal && other_formal && preferred_formal != other_formal) {
		const bool preferred_to_other = direct_assignment_edge(preferred_formal, other_formal);
		const bool other_to_preferred = direct_assignment_edge(other_formal, preferred_formal);
		if (preferred_to_other != other_to_preferred) {
			return "formal type " + ctx.type_ref(preferred_formal) + " is more specific than " + ctx.type_ref(other_formal);
		}
	}
	if (preferred.integer_sign_mismatch != other.integer_sign_mismatch) {
		if (untyped_integer_constant(actual)) {
			return "preserves the signedness of the literal's natural integer type";
		}
		return "preserves the actual integer type's signedness";
	}
	if (preferred.distance != other.distance) {
		if (untyped_integer_constant(actual)) {
			return "uses the smaller fitting integer destination";
		}
		return "is the closer match within the same quality";
	}
	return "has the better per-argument match";
}

[[noreturn]] void Parser::raise_overload_resolution_error(SourceLocation error_location, std::string name, Node* receiver, const std::vector<Node*>& args, Type* expected_return_type, const std::vector<Callable*>& candidates, const std::vector<std::pair<Callable*, CallableMatch>>& viable, const std::vector<Callable*>& non_dominated, bool ambiguous, std::string failure_description) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	if (failure_description.empty()) {
		failure_description = ambiguous ? "ambiguous overload" : "no matching overload";
	}
	sst << failure_description << " for '" << name << "'";

	if (receiver) {
		sst << "\n  receiver: " << ctx.value_ref(receiver) << " : " << ctx.type_ref(receiver->ty);
	}
	if (expected_return_type) {
		sst << "\n  expected return type: " << ctx.type_ref(expected_return_type);
	}
	for (size_t i = 0; i < args.size(); ++i) {
		sst << "\n  arg " << (i + 1) << ": " << ctx.value_ref(args[i]) << " : " << ctx.type_ref(args[i] ? args[i]->ty : nullptr);
	}

	std::vector<Callable*> sorted_candidates = candidates;
	std::stable_sort(sorted_candidates.begin(), sorted_candidates.end(), callable_source_less);

	sst << "\n  all candidates:";
	for (Callable* c : sorted_candidates) {
		const CallableMatch* viable_match = nullptr;
		for (const auto& v : viable) {
			if (v.first == c) {
				viable_match = &v.second;
				break;
			}
		}
		const bool ambiguous_survivor = std::ranges::find(non_dominated, c) != non_dominated.end();

		sst << "\n    ";
		append_callable_source_prefix(sst, c, viable_match != nullptr, ambiguous_survivor);
		sst << ctx.value_ref(c) << " : " << ctx.type_ref(c ? c->ty : nullptr);
		if (viable_match) {
			sst << " viable ranks ";
			append_match_rank_vector(sst, viable_match->ranks);
		} else {
			sst << " not viable";
		}
	}

	if (ambiguous && non_dominated.size() > 1) {
		auto viable_match_for = [&](Callable* candidate) -> const CallableMatch* {
			for (const auto& entry : viable) {
				if (entry.first == candidate) {
					return &entry.second;
				}
			}
			return nullptr;
		};

		bool wrote_heading = false;
		for (size_t arg_index = 0; arg_index < args.size(); ++arg_index) {
			for (size_t first_index = 0; first_index < non_dominated.size(); ++first_index) {
				Callable* first = non_dominated[first_index];
				const CallableMatch* first_match = viable_match_for(first);
				if (!first_match || arg_index >= first_match->ranks.size() || arg_index >= first_match->formal_types.size()) {
					continue;
				}
				for (size_t second_index = first_index + 1; second_index < non_dominated.size(); ++second_index) {
					Callable* second = non_dominated[second_index];
					const CallableMatch* second_match = viable_match_for(second);
					if (!second_match || arg_index >= second_match->ranks.size() || arg_index >= second_match->formal_types.size()) {
						continue;
					}
					const auto direct_edge = [this](Type* source, Type* destination) { return has_direct_assignment_edge(source, destination); };
					const bool first_better = rank_less(first_match->ranks[arg_index], first_match->formal_types[arg_index], second_match->ranks[arg_index], second_match->formal_types[arg_index], direct_edge);
					const bool second_better = rank_less(second_match->ranks[arg_index], second_match->formal_types[arg_index], first_match->ranks[arg_index], first_match->formal_types[arg_index], direct_edge);
					if (first_better == second_better) {
						continue;
					}
					if (!wrote_heading) {
						sst << "\n  conflicting argument preferences:";
						wrote_heading = true;
					}
					Callable* preferred = first_better ? first : second;
					const CallableMatch* preferred_match = first_better ? first_match : second_match;
					const CallableMatch* other_match = first_better ? second_match : first_match;
					sst << "\n    arg " << (arg_index + 1) << " prefers ";
					const SourceLocation& preferred_location = callable_source_location(preferred);
					if (!preferred_location.file_name.empty()) {
						sst << preferred_location.file_name << "(" << preferred_location.line_number << ")";
					} else {
						sst << ctx.value_ref(preferred);
					}
					sst << ": " << match_preference_reason(preferred_match->ranks[arg_index], preferred_match->formal_types[arg_index], other_match->ranks[arg_index], other_match->formal_types[arg_index], args[arg_index], ctx, direct_edge);
				}
			}
		}
	}

	emit_parse_error_at(error_location, sst.str(), ctx);
}

[[noreturn]] void Parser::raise_no_matching_overload(std::string name, Node* receiver, const std::vector<Node*>& args) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << "no matching overload for '" << name << "'";
	if (receiver) {
		sst << "\n  receiver: " << ctx.value_ref(receiver) << " : " << ctx.type_ref(receiver->ty);
	}
	for (size_t i = 0; i < args.size(); ++i) {
		sst << "\n  arg " << (i + 1) << ": " << ctx.value_ref(args[i]) << " : " << ctx.type_ref(args[i] ? args[i]->ty : nullptr);
	}
	emit_parse_error_at(current_location(), sst.str(), ctx);
}

[[noreturn]] void Parser::raise_cxx_carrier_collision(const std::string& name, Callable* incoming, const CallableRegistration& registration) {
	Callable* conflicting = registration.conflicting_callable;
	assert(incoming && incoming->ty && conflicting && conflicting->ty);

	// The declarations and their RoutineType nodes are roots in the normal
	// diagnostic graph. That printer, rather than a second signature formatter
	// here, expands aliases, formal/result types, aggregate members, and every
	// other referenced definition transitively.
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::string incoming_ref = ctx.value_ref(incoming);
	std::string incoming_type_ref = ctx.type_ref(incoming->ty);
	std::string conflicting_ref = ctx.value_ref(conflicting);
	std::string conflicting_type_ref = ctx.type_ref(conflicting->ty);
	std::string family_ref = ctx.value_ref(registration.existing_binding);

	std::stringstream sst;
	sst << "overload '" << name << "' cannot be represented by the C++ backend";
	sst << "\n  incoming declaration: ";
	append_callable_source_prefix(sst, incoming, false);
	sst << incoming_ref << " : " << incoming_type_ref;
	sst << "\n  conflicting declaration: ";
	append_callable_source_prefix(sst, conflicting, false);
	sst << conflicting_ref << " : " << conflicting_type_ref;
	sst << "\n  existing overload family: " << family_ref;
	sst << "\n  emitted C++ name: '" << incoming->cxx_name << "'";

	RoutineType* incoming_type = incoming->ty;
	RoutineType* conflicting_type = conflicting->ty;
	if (incoming_type->same_overload_signature_as(conflicting_type) && incoming_type->return_type != conflicting_type->return_type) {
		sst << "\n  Pascal distinguishes these conversion "
		       "operators by destination type: "
		    << conflicting_type_ref << " returns " << ctx.type_ref(conflicting_type->return_type) << ", while " << incoming_type_ref << " returns " << ctx.type_ref(incoming_type->return_type)
		    << ". Their hidden destination tags still "
		       "have the same C++ carrier, so this "
		       "backend cannot distinguish them.";
	} else {
		sst << "\n  distinct Pascal parameter types "
		       "collapse to the same C++ parameter "
		       "carriers:";
		for (size_t i = 0; i < incoming_type->formals.size(); ++i) {
			Type* incoming_formal = incoming_type->formals[i].ty;
			Type* conflicting_formal = conflicting_type->formals[i].ty;
			if (incoming_formal == conflicting_formal || !incoming_formal->same_cxx_carrier_as(conflicting_formal)) {
				continue;
			}
			sst << "\n    parameter " << (i + 1) << ": " << ctx.type_ref(conflicting_formal) << " and " << ctx.type_ref(incoming_formal);
		}
	}
	emit_parse_error_at(callable_source_location(incoming), sst.str(), ctx);
}

[[noreturn]] void Parser::raise_callable_registration_error(const std::string& name, Callable* incoming, const CallableRegistration& registration) {
	if (registration.kind == CallableRegistration::Kind::CxxCarrierCollision) {
		raise_cxx_carrier_collision(name, incoming, registration);
	}

	assert(registration.kind == CallableRegistration::Kind::Rejected);
	if (!registration.existing_binding) {
		emit_parse_error_at(callable_source_location(incoming), "duplicate identifier: " + name);
	}
	assert(incoming && incoming->ty && registration.existing_binding);

	// Registration already retained both the complete family and the exact
	// conflicting declaration. Root those existing CST nodes in the normal
	// diagnostic graph instead of reconstructing a lossy signature string at
	// the parser call site.
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << "callable declaration conflicts with existing declaration for '" << name << "'";
	sst << "\n  incoming declaration: ";
	append_callable_source_prefix(sst, incoming, false);
	sst << ctx.value_ref(incoming) << " : " << ctx.type_ref(incoming->ty);
	if (Callable* conflicting = registration.conflicting_callable) {
		sst << "\n  conflicting declaration: ";
		append_callable_source_prefix(sst, conflicting, false);
		sst << ctx.value_ref(conflicting) << " : " << ctx.type_ref(conflicting->ty);
		if (!same_callable_overload_category(incoming, conflicting)) {
			sst << "\n  reason: the declarations have incompatible "
			       "routine categories";
		} else if (incoming->ty->same_overload_signature_as(conflicting->ty)) {
			sst << "\n  reason: both declarations have the same "
			       "Pascal overload signature";
		}
	}
	sst << "\n  existing overload family: " << ctx.value_ref(registration.existing_binding);
	emit_parse_error_at(callable_source_location(incoming), sst.str(), ctx);
}

bool Parser::is_defined(const std::string& sym) const {
	return options && options->defines.count(sym) > 0;
}

// See directive_expr.h/cc for the grammar and semantics; this is only the
// glue that attaches the parser's file/line context to any error the
// standalone evaluator raises.
bool Parser::eval_directive_expr(const std::string& expr) {
	static const std::map<std::string, std::string, CILess> empty;
	try {
		return ::eval_directive_expr(expr, options ? options->defines : empty);
	} catch (const DirectiveExprError& e) {
		raise_parse_error(e.message);
	}
}

// Split BODY into (directive_name_lowercased, argument-after-name-trimmed).
static std::pair<std::string, std::string> split_directive(const std::string& body) {
	size_t p = 0;
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) {
		p++;
	}
	std::string name;
	while (p < body.size() && (isalnum((unsigned char)body[p]) || body[p] == '_')) {
		name.push_back((char)tolower((unsigned char)body[p]));
		p++;
	}
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) {
		p++;
	}
	std::string rest = body.substr(p);
	while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r' || rest.back() == '\n')) {
		rest.pop_back();
	}
	return {name, rest};
}

static std::string compact_directive_argument(const std::string& argument) {
	std::string compact;
	for (unsigned char ch : argument) {
		if (!std::isspace(ch)) {
			compact.push_back(static_cast<char>(std::tolower(ch)));
		}
	}
	return compact;
}

static constexpr std::array<DirectiveSwitchCategory, 26> directive_switch_categories = {
    DirectiveSwitchCategory::Unsupported, // A; local
    DirectiveSwitchCategory::Unsupported, // B; local
    DirectiveSwitchCategory::Unsupported, // C; local
    DirectiveSwitchCategory::Unsupported, // D; module
    DirectiveSwitchCategory::Unsupported, // E; module
    DirectiveSwitchCategory::Unsupported, // F
    DirectiveSwitchCategory::Unsupported, // G; local
    DirectiveSwitchCategory::Local,       // H
    DirectiveSwitchCategory::Local,       // I
    DirectiveSwitchCategory::Unsupported, // J; local
    DirectiveSwitchCategory::Unsupported, // K
    DirectiveSwitchCategory::Unsupported, // L
    DirectiveSwitchCategory::Unsupported, // M; local
    DirectiveSwitchCategory::Unsupported, // N
    DirectiveSwitchCategory::Optimizer,   // O; not stored by push
    DirectiveSwitchCategory::Unsupported, // P; module
    DirectiveSwitchCategory::Local,       // Q
    DirectiveSwitchCategory::Local,       // R
    DirectiveSwitchCategory::Unsupported, // S; local
    DirectiveSwitchCategory::Unsupported, // T; local
    DirectiveSwitchCategory::Unsupported, // U
    DirectiveSwitchCategory::Unsupported, // V; local
    DirectiveSwitchCategory::Unsupported, // W; local
    DirectiveSwitchCategory::Unsupported, // X; module
    DirectiveSwitchCategory::Unsupported, // Y
    DirectiveSwitchCategory::Unsupported, // Z
};

static std::optional<size_t> directive_switch_index(char letter) {
	unsigned char ch = static_cast<unsigned char>(letter);
	ch = static_cast<unsigned char>(std::tolower(ch));
	if (ch < 'a' || ch > 'z') {
		return std::nullopt;
	}
	return static_cast<size_t>(ch - 'a');
}

DirectiveState::DirectiveState() {
	// I/O checking is the safety-default Pascal switch. Unlike arithmetic
	// overflow and range checks, a failed checked I/O operation has no useful
	// result to continue with; source which deliberately uses IOResult opts
	// out locally with {$I-}.
	local_switches[static_cast<size_t>('i' - 'a')] = true;
}

bool DirectiveState::switch_supported(char letter) const {
	auto index = directive_switch_index(letter);
	if (!index) {
		return false;
	}
	switch (directive_switch_categories[*index]) {
	case DirectiveSwitchCategory::Unsupported:
		return false;
	default:
		return true;
	}
}

bool DirectiveState::switch_enabled(char letter) const {
	auto index = directive_switch_index(letter);
	if (!index) {
		return false;
	}
	switch (directive_switch_categories[*index]) {
	case DirectiveSwitchCategory::Local:
		return local_switches[*index];
	case DirectiveSwitchCategory::Module:
		return module_switches[*index];
	case DirectiveSwitchCategory::Optimizer:
		return optimizer_switches[*index];
	case DirectiveSwitchCategory::Unsupported:
		return false;
	}
	return false;
}

void DirectiveState::set_switch(char letter, bool enabled) {
	auto index = directive_switch_index(letter);
	if (!index) {
		return;
	}
	switch (directive_switch_categories[*index]) {
	case DirectiveSwitchCategory::Local:
		local_switches[*index] = enabled;
		break;
	case DirectiveSwitchCategory::Module:
		module_switches[*index] = enabled;
		break;
	case DirectiveSwitchCategory::Optimizer:
		optimizer_switches[*index] = enabled;
		break;
	case DirectiveSwitchCategory::Unsupported:
		break;
	}
}

SavedDirectiveState::SavedDirectiveState(const DirectiveState& state) : local_switches(state.local_switches), packenum(state.packenum) {
}

void SavedDirectiveState::restore(DirectiveState& state) const {
	state.local_switches = local_switches;
	state.packenum = packenum;
}

// Extract the content of a single-quoted string literal token (with '' escape).
// FIXME: Remove and use evaluate().
static std::string extract_string_literal(const std::string& token) {
	std::string s;
	for (size_t i = 1; i + 1 < token.size(); ++i) {
		s.push_back(token[i]);
		if (token[i] == '\'' && i + 2 < token.size() && token[i + 1] == '\'') {
			++i;
		}
	}
	return s;
}

// Try to open NAME (used verbatim -- extension is the caller's job) by
// searching the directory of CURRENT_INPUT first, then each entry of
// SEARCH_PATHS (normalized to end in '/'). Returns the opened FILE and the
// path that worked, or {nullptr, ""}.
static std::pair<FILE*, std::string> search_for_file(const std::string& name, const std::string& current_input, const std::vector<std::string>& search_paths) {
	std::vector<std::string> dirs;
	std::string cur_dir;
	auto slash = current_input.find_last_of('/');
	if (slash != std::string::npos) {
		cur_dir = current_input.substr(0, slash + 1);
	}
	dirs.push_back(cur_dir);
	for (auto& d : search_paths) {
		std::string s = d;
		if (!s.empty() && s.back() != '/') {
			s.push_back('/');
		}
		dirs.push_back(s);
	}
	for (auto& d : dirs) {
		std::string candidate = d + name;
		if (FILE* f = fopen(candidate.c_str(), "r")) {
			return {f, candidate};
		}
	}
	return {nullptr, ""};
}

std::string Parser::expand_include_macro(const std::string& rest) {
	if (rest != "%DATE%") {
		raise_parse_error("unsupported include macro: " + rest);
	}
	auto now = std::chrono::system_clock::now();
	auto zoned = std::chrono::current_zone()->to_local(now);
	auto today = std::chrono::floor<std::chrono::days>(zoned);
	std::chrono::year_month_day ymd{today};
	return std::format("'{:%Y/%m/%d}'", ymd);
}

void Parser::handle_directive(const std::string& body, SourceLocation directive_location) {
	auto [name, rest] = split_directive(body);
	if (name == "ifdef" || name == "ifndef") {
		bool outer = current_active();
		bool cond = is_defined(rest);
		if (name == "ifndef") {
			cond = !cond;
		}
		ifdef_stack.push_back({outer, cond, outer && cond});
	} else if (name == "if") {
		bool outer = current_active();
		bool cond = eval_directive_expr(rest);
		ifdef_stack.push_back({outer, cond, outer && cond});
	} else if (name == "ifopt") {
		const std::string option = compact_directive_argument(rest);
		if (option.size() != 2 || option[0] < 'a' || option[0] > 'z' || (option[1] != '+' && option[1] != '-')) {
			raise_parse_error("$ifopt expects one option letter followed by + or -");
		}
		const bool requested = option[1] == '+';
		const bool cond = directive_state.switch_supported(option[0]) ? directive_state.switch_enabled(option[0]) == requested : false;
		const bool outer = current_active();
		ifdef_stack.push_back({outer, cond, outer && cond});
	} else if (name == "else") {
		if (ifdef_stack.empty()) {
			raise_parse_error("$else without matching $ifdef");
		}
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken;
		f.active = now;
		f.taken = f.taken || now;
	} else if (name == "elseif") {
		if (ifdef_stack.empty()) {
			raise_parse_error("$elseif without matching $ifdef");
		}
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken && eval_directive_expr(rest);
		f.active = now;
		f.taken = f.taken || now;
	} else if (name == "endif" || name == "ifend") {
		if (ifdef_stack.empty()) {
			raise_parse_error("$endif without matching $ifdef");
		}
		ifdef_stack.pop_back();
	} else if (current_active()) {
		const bool single_letter = name.size() == 1 && name[0] >= 'a' && name[0] <= 'z';
		const std::string switches = single_letter ? name + compact_directive_argument(rest) : "";
		const bool switch_directive = switches.size() >= 2 && (switches[1] == '+' || switches[1] == '-');

		if (name == "error") {
			emit_parse_error_at(directive_location, "user-defined: " + rest);
		} else if (name == "fatal") {
			emit_fatal_error_at(directive_location, "user-defined: " + rest);
		} else if (name == "push") {
			saved_directive_states.emplace_back(directive_state);
		} else if (name == "pop") {
			if (saved_directive_states.empty()) {
				raise_parse_error("$pop without preceding $push");
			}
			saved_directive_states.back().restore(directive_state);
			saved_directive_states.pop_back();
		} else if (name == "interfaces") {
			const std::string model = compact_directive_argument(rest);
			if (model == "corba") {
				directive_state.set_interface_model(InterfaceModel::CORBA);
			} else if (model == "com" || model == "default") {
				// Native Pascal's initial interface model is COM. TPCC has no
				// command-line override for that initial setting, so DEFAULT
				// returns to COM rather than to the most recent directive.
				directive_state.set_interface_model(InterfaceModel::COM);
			} else {
				emit_parse_error_at(directive_location, "$interfaces expects COM, CORBA, or DEFAULT");
			}
		} else if (name == "iochecks") {
			const std::string setting = compact_directive_argument(rest);
			if (setting == "on") {
				directive_state.set_switch('i', true);
			} else if (setting == "off") {
				directive_state.set_switch('i', false);
			} else {
				emit_parse_error_at(directive_location, "$iochecks expects ON or OFF");
			}
		} else if (name == "longstrings") {
			const std::string setting = compact_directive_argument(rest);
			if (setting == "on") {
				directive_state.set_switch('h', true);
			} else if (setting == "off") {
				directive_state.set_switch('h', false);
			} else {
				emit_parse_error_at(directive_location, "$longstrings expects ON or OFF");
			}
		} else if (name == "packenum" || name == "minenumsize") {
			const std::string arg = compact_directive_argument(rest);
			if (arg == "normal" || arg == "default") {
				directive_state.set_packenum(4);
			} else if (arg == "1" || arg == "2" || arg == "4") {
				directive_state.set_packenum(std::stoi(arg));
			} else {
				emit_parse_error_at(directive_location, "$" + name + " expects 1, 2, 4, NORMAL, or DEFAULT");
			}
		} else if (name == "define") {
			// `{$define X}` sets X with no value; `{$define X := VALUE}` stores
			// VALUE (trimmed) so numeric-compare {$if X < N} etc. can consume it.
			if (options) {
				auto eq = rest.find(":=");
				if (eq == std::string::npos) {
					options->defines[rest] = "";
				} else {
					std::string sym = rest.substr(0, eq);
					while (!sym.empty() && (sym.back() == ' ' || sym.back() == '\t')) {
						sym.pop_back();
					}
					std::string val = rest.substr(eq + 2);
					size_t v0 = 0;
					while (v0 < val.size() && (val[v0] == ' ' || val[v0] == '\t')) {
						v0++;
					}
					val.erase(0, v0);
					while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) {
						val.pop_back();
					}
					options->defines[sym] = val;
				}
			}
		} else if (name == "undef") {
			if (options) {
				options->defines.erase(rest);
			}
		} else if (switch_directive) {
			size_t position = 0;
			while (position < switches.size()) {
				if (position + 1 >= switches.size() || switches[position] < 'a' || switches[position] > 'z' || (switches[position + 1] != '+' && switches[position + 1] != '-')) {
					raise_parse_error("malformed option switch list");
				}
				if (directive_state.switch_supported(switches[position])) {
					directive_state.set_switch(switches[position], switches[position + 1] == '+');
				} else {
					raise_parse_error("switch unsupported");
				}
				position += 2;
				if (position == switches.size()) {
					break;
				}
				if (switches[position] != ',') {
					raise_parse_error("malformed option switch list");
				}
				++position;
				if (position == switches.size()) {
					raise_parse_error("malformed option switch list");
				}
			}
		} else if (name == "i" || name == "include") {
			if (rest.size() >= 2 && rest.front() == '%' && rest.back() == '%') {
				std::string literal = expand_include_macro(rest);
				size_t n = literal.size();
				auto buf = std::make_unique<char[]>(n);
				memcpy(buf.get(), literal.data(), n);
				FILE* f = fmemopen(buf.get(), n, "r");
				if (!f) {
					raise_parse_error("fmemopen failed for {$I " + rest + "}");
				}
				push_input_file_and_buffer(f, "<" + rest + ">", 1, std::move(buf), n);
			} else {
				std::vector<std::string> empty;
				auto [f, path] = search_for_file(rest, input_file_name, options ? options->include_search_paths : empty);
				if (!f) {
					raise_parse_error("cannot open include file: " + rest);
				}
				push_input_file(f, path, 1);
			}
		}
	}
	// Other directives are accepted here for now. As we hit code where the
	// current no-op is wrong, tighten by name.
}

std::string Parser::consume() {
	std::stringstream sst;
	sst.str("");
	while (input_char == ' ' || input_char == '\n' || input_char == '\r' || input_char == '\t') {
		consume_lowlevel();
	}
	if (input_char == EOF) {
		input_token = "";
		return "";
	}
	if ((input_char >= 'a' && input_char <= 'z') | (input_char >= 'A' && input_char <= 'Z') || input_char == '_') {
		while ((input_char >= 'a' && input_char <= 'z') || (input_char >= 'A' && input_char <= 'Z') || input_char == '_' || (input_char >= '0' && input_char <= '9')) {
			sst << (char)tolower(input_char);
			consume_lowlevel();
		}
	} else if (input_char == '$') {
		sst << (char)input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || (input_char >= 'a' && input_char <= 'f') || (input_char >= 'A' && input_char <= 'F') || input_char == '.' || input_char == '_') {
			sst << (char)tolower(input_char);
			consume_lowlevel();
		}
	} else if (input_char == '%') {
		sst << (char)input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || input_char == '_') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if ((input_char >= '0' && input_char <= '9') || input_char == '_') {
		/* consume integer part first */
		while ((input_char >= '0' && input_char <= '9') || input_char == '_') {
			sst << (char)input_char;
			consume_lowlevel();
		}
		if (input_char == '.') {
			int next_char = peek_lowlevel(); // LL(2). Sigh.
			if (next_char >= '0' && next_char <= '9') {
				sst << (char)input_char; // Append the '.'
				consume_lowlevel();      // Consume the '.' so input_char becomes the digit
				// Consume the fractional part
				while ((input_char >= '0' && input_char <= '9') || input_char == '_') {
					sst << (char)input_char;
					consume_lowlevel();
				}
			}
		}
		if (input_char == 'e' || input_char == 'E') {
			// Peek to ensure it's actually an exponent, not just random garbage.
			// In Pascal, an exponent MUST be followed by a digit, '+', or '-'.
			int next_char = peek_lowlevel();
			if ((next_char >= '0' && next_char <= '9') || next_char == '+' || next_char == '-') {
				sst << (char)input_char; // Append the 'e' or 'E'
				consume_lowlevel();
				if (input_char == '+' || input_char == '-') {
					sst << (char)input_char;
					consume_lowlevel();
				}
				// Consume the exponent digits
				while ((input_char >= '0' && input_char <= '9') || input_char == '_') {
					sst << (char)input_char;
					consume_lowlevel();
				}
			}
		}
	} else if (input_char == '#') {
		sst << (char)input_char;
		consume_lowlevel();
		while ((input_char >= '0' && input_char <= '9') || input_char == '_') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '<') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '<' || input_char == '>') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '>') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=' || input_char == '>' || input_char == '<') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == ':') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '=') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '.') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '.') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '/') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '/') { // line comment
			sst << (char)input_char;
			consume_lowlevel();
			while (input_char != EOF && input_char != '\n') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char == '\n') {
				sst << (char)input_char;
				consume_lowlevel();
				return consume();
			} else {
				raise_parse_error("missing newline");
			}
		}
	} else if (input_char == '*') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '*') {
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '(') {
		SourceLocation directive_location = current_location();
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '*') {
			sst << (char)input_char;
			consume_lowlevel();
			const bool is_directive = input_char == '$';
			if (is_directive) {
				consume_lowlevel(); // skip $
			}
			std::string body;
			while (input_char != EOF && !(input_char == '*' && peek_lowlevel() == ')')) {
				if (is_directive) {
					body.push_back((char)input_char);
				}
				consume_lowlevel();
			}
			if (input_char == EOF) {
				raise_parse_error("missing end comment");
			}
			consume_lowlevel(); // skip *
			consume_lowlevel(); // skip )
			if (is_directive) {
				handle_directive(body, directive_location);
			}
			return consume();
		}
	} else if (input_char != EOF && strchr("=;,[])@+-^|&", input_char)) {
		sst << (char)input_char;
		consume_lowlevel();
	} else if (input_char == '\'') {
		sst << (char)input_char;
		consume_lowlevel();
		while (true) {
			while (input_char != EOF && input_char != '\'') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char != '\'') {
				raise_parse_error("missing end quote");
			}
			sst << (char)input_char;
			consume_lowlevel();
			// A doubled quote inside the string represents one literal
			// quote character; consume it and keep going.
			if (input_char != '\'') {
				break;
			}
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '{') {
		SourceLocation directive_location = current_location();
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '$') {
			consume_lowlevel(); // skip $
			std::string body;
			while (input_char != EOF && input_char != '}') {
				body.push_back((char)input_char);
				consume_lowlevel();
			}
			if (input_char != '}') {
				raise_parse_error("missing end comment");
			}
			consume_lowlevel(); // skip }
			handle_directive(body, directive_location);
			return consume();
		} else {
			while (input_char != EOF && input_char != '}') {
				sst << (char)input_char;
				consume_lowlevel();
			}
			if (input_char == '}') {
				sst << (char)input_char;
				consume_lowlevel();
				return consume();
			} else {
				raise_parse_error("missing end comment");
			}
		}
	} else {
		fprintf(stderr, "CHAR >%c< %d\n", input_char, input_char);
		raise_parse_error("unknown input character");
	}
	auto text = sst.str();
	input_token = text;
	// Drop any token produced while an outer `{$ifdef}`/`{$if}` frame is
	// inactive. Directives are already handled in-line and never reach
	// here, so they still update the ifdef stack correctly.
	if (!current_active() && !text.empty()) {
		return consume();
	}
	return text;
}

void Parser::start() {
	push_scope(&root_frame());
	consume();
}

bool Parser::peek_keyword(std::string s) {
	return input_token == s; // FIXME case insensitive
}

void Parser::parse_keyword(std::string s) {
	if (peek_keyword(s)) {
		consume();
	} else {
		raise_parse_error("expected keyword " + s);
	}
}

bool Parser::maybe_parse_keyword(std::string s) {
	if (peek_keyword(s)) {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_directive(std::string directive) {
	// A Pascal directive (private/public/protected/published/etc.) matches
	// like a keyword for our purposes. Distinguished from a true keyword in
	// that directives are only reserved within specific contexts (aggregate
	// type bodies, procedure attribute lists); outside those contexts they
	// remain usable as identifiers. The lexical match itself is identical.
	return maybe_parse_keyword(directive);
}

void Parser::parse_directive(std::string directive) {
	if (!maybe_parse_directive(directive)) {
		raise_parse_error("expected directive " + directive);
	}
}

bool Parser::peek_directive(std::string directive) {
	return peek_keyword(directive);
}

void Parser::parse_operator(std::string x) {
	/* TODO: Limit to:
*
+
-
-
/
:=
<
<=
=
>
>=
<>
and
div
mod
or
shl
shr
xor
*/
	if (!maybe_parse_keyword(x)) {
		raise_parse_error("expected operator " + x);
	}
}

/** Return the Frame that holds the fields/members of TY, or nullptr if TY
 *  doesn't have one (i.e. isn't a record/class/object). Type-block forward
 *  placeholders are resolved before statements are parsed; an IncompleteType
 *  here is therefore a parser bug, not an ordinary lookup path. */
static Frame* get_type_body_frame(Type* ty) {
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		return r->children;
	} else if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
		return r->children;
	} else if (auto c = dynamic_cast<ClassType*>(ty)) {
		return c->children;
	} else if (auto i = dynamic_cast<InterfaceType*>(ty)) {
		return i->children;
	} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
		return o->children;
	}
	return nullptr;
}

void Parser::maybe_parse_statement() {
	if (peek_keyword("end") || peek_keyword("until") || peek_keyword("except") || peek_keyword("finally")) {
		return;
	}
	if (peek_keyword("raise")) {
		parse_keyword("raise");
		Node* object = nullptr;
		Node* address = nullptr;
		Node* frame = nullptr;
		const bool bare = input_token == ";" || peek_keyword("end") || peek_keyword("else") || peek_keyword("until") || peek_keyword("except") || peek_keyword("finally");
		if (bare) {
			if (!bare_raise_allowed) {
				raise_parse_error("re-raise is only valid directly inside an except handler");
			}
		} else {
			ClassType* tobject = lookup_implicit_tobject_superclass();
			object = cast(parse_expression(), tobject);
			if (maybe_parse_directive("at")) {
				address = cast(parse_expression(), pointer_type());
				if (maybe_parse_comma()) {
					frame = cast(parse_expression(), pointer_type());
				}
			}
		}
		if (emitter) {
			emitter->emit_statement(new Raise(object, address, frame));
		}
	} else if (peek_directive("fail") && current_routine && current_routine->ty->kind == CONSTRUCTOR) {
		parse_directive("fail");
		if (emitter) {
			emitter->emit_statement(new ConstructorFail());
		}
	} else if (peek_keyword("break") || peek_keyword("continue")) {
		bool is_break = peek_keyword("break");
		consume();
		if (loop_depth == 0) {
			raise_parse_error(is_break ? "break outside loop" : "continue outside loop");
		}
		if (!finally_loop_depths.empty() && loop_depth <= finally_loop_depths.back()) {
			raise_parse_error(is_break ? "break cannot leave a finally block" : "continue cannot leave a finally block");
		}
		if (emitter) {
			emitter->emit_loop_control(is_break, protected_try_depth, is_break ? loop_try_targets.back().break_depth : loop_try_targets.back().continue_depth);
		}
	} else if (peek_keyword("return")) { // FIXME Exit
		if (!finally_loop_depths.empty()) {
			raise_parse_error("return cannot leave a finally block");
		}
		consume();
		parse_expression();
	} else if (peek_directive("exit")) {
		parse_directive("exit");
		if (!finally_loop_depths.empty()) {
			raise_parse_error("exit cannot leave a finally block");
		}
		if (!current_routine) {
			raise_parse_error("exit outside routine");
		}
		Type* ret_ty = current_routine->ty->return_type;
		Node* value = nullptr;
		if (maybe_parse_opening_paren()) {
			if (input_token != ")") {
				value = parse_expression();
			}
			parse_closing_paren();
			if (ret_ty == &unit_type()) {
				if (value) {
					raise_parse_error("exit(value) in procedure");
				}
			} else {
				value = value ? cast_for_destination(value, ret_ty) : resolve_value("result");
			}
		} else if (ret_ty != &unit_type()) {
			value = resolve_value("result");
		}
		if (emitter) {
			emitter->emit_statement(new Return(value, protected_try_depth));
		}
	} else if (peek_keyword("goto")) {
		SourceLocation goto_location = current_location();
		parse_keyword("goto");
		std::string label = parse_identifier();
		record_goto(label, goto_location);
		if (emitter) {
			emitter->emit_goto(cxx_label_name(label));
		}
	} else if (peek_keyword("try")) {
		const unsigned enclosing_exception_block = current_exception_block();
		parse_keyword("try");
		const unsigned this_try_depth = protected_try_depth + 1;
		const bool try_is_inside_loop = loop_depth != 0;
		const bool saved_bare_raise = bare_raise_allowed;
		if (emitter) {
			emitter->emit_try_prologue();
		}
		enter_exception_block();
		++protected_try_depth;
		bare_raise_allowed = false;
		parse_block_body();
		--protected_try_depth;

		if (maybe_parse_keyword("except")) {
			enter_exception_block();
			if (emitter) {
				emitter->emit_try_except_prologue();
			}
			bool typed_handlers = false;
			bool has_default = false;
			if (peek_directive("on")) {
				typed_handlers = true;
				bool first_handler = true;
				while (peek_directive("on")) {
					parse_directive("on");
					std::string first = parse_identifier();
					std::optional<std::string> variable_name;
					Type* exception_type = nullptr;
					if (maybe_parse_colon()) {
						variable_name = first;
						exception_type = parse_type_expression(false);
					} else if (maybe_parse_period()) {
						exception_type = parse_qualified_type_member(first);
					} else {
						exception_type = resolve_type(first, false);
					}
					if (!dynamic_cast<ClassType*>(exception_type)) {
						raise_type_kind_mismatch("exception handler type must be a class", "class", exception_type);
					}
					parse_keyword("do");

					auto handler_frame = new Frame(nullptr);
					StorageSlot* variable = nullptr;
					if (variable_name) {
						variable = new StorageSlot(cxx_value_name(*variable_name), exception_type);
						if (!handler_frame->register_variable(*variable_name, variable, exception_type)) {
							raise_parse_error("duplicate identifier: " + *variable_name);
						}
					}
					if (emitter) {
						emitter->emit_exception_handler_prologue(exception_type, variable ? variable->cxx_name : "", first_handler);
					}
					push_scope(handler_frame);
					bare_raise_allowed = true;
					const bool empty_handler = input_token == ";" || peek_directive("on") || peek_keyword("else") || peek_keyword("end");
					if (!empty_handler) {
						parse_statement();
					}
					bare_raise_allowed = false;
					pop_scope();
					if (emitter) {
						emitter->emit_exception_handler_epilogue();
					}
					first_handler = false;

					const bool separated = maybe_parse_semicolon();
					while (maybe_parse_semicolon()) {
					}
					if (peek_directive("on") && !separated) {
						raise_parse_error("missing semicolon between exception handlers");
					}
				}
				if (maybe_parse_keyword("else")) {
					has_default = true;
					if (emitter) {
						emitter->emit_exception_default_prologue();
					}
					bare_raise_allowed = true;
					parse_block_body();
					bare_raise_allowed = false;
					if (emitter) {
						emitter->emit_exception_default_epilogue();
					}
				}
			} else {
				has_default = true;
				bare_raise_allowed = true;
				parse_block_body();
				bare_raise_allowed = false;
			}
			bare_raise_allowed = saved_bare_raise;
			parse_keyword("end");
			if (emitter) {
				emitter->emit_try_except_epilogue(typed_handlers, has_default);
			}
		} else if (maybe_parse_keyword("finally")) {
			enter_exception_block();
			if (emitter) {
				emitter->emit_try_finally_prologue();
			}
			bare_raise_allowed = false;
			finally_loop_depths.push_back(loop_depth);
			parse_block_body();
			finally_loop_depths.pop_back();
			bare_raise_allowed = saved_bare_raise;
			parse_keyword("end");
			if (emitter) {
				emitter->emit_try_finally_epilogue();
			}
		} else {
			raise_parse_error("expected except or finally after try block");
		}
		restore_exception_block(enclosing_exception_block);
		if (emitter) {
			emitter->emit_try_control_epilogue(this_try_depth, current_routine ? current_routine->ty : nullptr, try_is_inside_loop);
		}
	} else if (peek_keyword("if")) {
		parse_keyword("if");
		auto condition = parse_expression();
		parse_keyword("then");
		if (emitter) {
			emitter->emit_if_prologue(condition);
		}
		// Match FPC's pstatmnt.if_statement: a token in `endtokens`
		// means the then branch is absent. Leave the delimiter unconsumed
		// for the surrounding if, block, repeat, or exception parser.
		const bool empty_then = input_token == ";" || peek_keyword("end") || peek_keyword("else") || peek_keyword("until") || peek_keyword("except") || peek_keyword("finally");
		if (!empty_then) {
			parse_statement();
		}
		if (maybe_parse_keyword("else")) {
			if (emitter) {
				emitter->emit_if_else();
			}
			parse_statement();
		}
		if (emitter) {
			emitter->emit_if_epilogue();
		}
	} else if (peek_keyword("case")) {
		parse_keyword("case");
		Node* selector = parse_expression();
		parse_keyword("of");

		// Every emitted case owns a C++ block, so a fixed tpcc-owned
		// name is sufficient: sibling blocks do not overlap and nested blocks
		// may shadow it. Pascal values are emitted with `p_`, preventing a
		// source identifier from colliding with this spelling.
		std::string selector_name = "tpcc_case_selector";
		auto selector_slot = new StorageSlot(selector_name, selector->ty);
		if (emitter) {
			emitter->emit_case_prologue(selector_name, selector);
		}

		bool has_arm = false;
		while (!peek_keyword("end") && !peek_keyword("else") && !peek_directive("otherwise")) {
			Node* arm_condition = nullptr;
			do {
				Node* lower = parse_subrange_bound_expression();
				lower = cast(lower, selector->ty);
				Node* label_condition;
				if (maybe_parse_period_period()) {
					Node* upper = parse_subrange_bound_expression();
					upper = cast(upper, selector->ty);
					auto lower_test = mk_compare(">=", selector_slot, lower, directive_state.leading_token_directives());
					auto upper_test = mk_compare("<=", selector_slot, upper, directive_state.leading_token_directives());
					auto both = new ShortCircuitOperation(AND, lower_test, upper_test);
					both->ty = boolean_type();
					label_condition = both;
				} else {
					label_condition = mk_compare("=", selector_slot, lower, directive_state.leading_token_directives());
				}
				if (arm_condition) {
					auto either = new ShortCircuitOperation(OR, arm_condition, label_condition);
					either->ty = boolean_type();
					arm_condition = either;
				} else {
					arm_condition = label_condition;
				}
			} while (maybe_parse_comma());
			parse_colon();

			if (emitter) {
				emitter->emit_case_arm_prologue(arm_condition, !has_arm);
			}
			parse_statement();
			if (emitter) {
				emitter->emit_case_arm_epilogue();
			}
			has_arm = true;

			// A semicolon separates arms. It is also accepted immediately
			// before ELSE/OTHERWISE, as in normal Pascal source.
			if (!maybe_parse_semicolon() && !peek_keyword("end") && !peek_keyword("else") && !peek_directive("otherwise")) {
				raise_parse_error("missing semicolon between case arms");
			}
		}

		if (peek_keyword("else") || peek_directive("otherwise")) {
			if (peek_keyword("else")) {
				parse_keyword("else");
			} else {
				parse_directive("otherwise");
			}
			if (emitter) {
				emitter->emit_case_else_prologue(has_arm);
			}
			parse_block_body();
			if (emitter) {
				emitter->emit_case_arm_epilogue();
			}
		}
		parse_keyword("end");
		if (emitter) {
			emitter->emit_case_epilogue();
		}
	} else if (peek_keyword("while")) {
		parse_keyword("while");
		auto condition = parse_expression();
		parse_keyword("do");
		if (emitter) {
			emitter->emit_while_prologue(condition);
		}
		++loop_depth;
		loop_try_targets.push_back({protected_try_depth, protected_try_depth});
		parse_statement();
		loop_try_targets.pop_back();
		--loop_depth;
		if (emitter) {
			emitter->emit_while_epilogue();
		}
	} else if (peek_keyword("for")) {
		// A directive encountered in a bound belongs to that bound's subtree;
		// it must not retroactively change the generated step of the enclosing
		// for statement. Capture the statement policy before consuming its
		// leading token.
		const bool for_overflow_checks = directive_state.switch_enabled('q');
		parse_keyword("for");
		std::string control_name = parse_identifier();
		Node* control = resolve_lvalue(control_name);
		if (!dynamic_cast<StorageSlot*>(control)) {
			raise_value_error("for control variable must be a simple variable", control);
		}
		if (maybe_parse_colon_equals()) {
			Node* initial = cast_for_destination(parse_expression(), control->ty);
			bool descending;
			if (maybe_parse_keyword("to")) {
				descending = false;
			} else if (maybe_parse_keyword("downto")) {
				descending = true;
			} else {
				raise_parse_error("expected 'to' or 'downto' in for statement");
			}
			Node* final = cast_for_destination(parse_expression(), control->ty);
			parse_keyword("do");

			OrdinalBounds bounds;
			if (!intrinsic_ordinal_bounds(control->ty, &bounds) && !dynamic_cast<EnumType*>(control->ty) && !dynamic_cast<SubrangeType*>(control->ty)) {
				raise_type_kind_mismatch("for control variable", "ordinal", control->ty);
			}

			if (emitter) {
				emitter->emit_for_prologue(control, initial, final, descending, for_overflow_checks);
			}
			++loop_depth;
			loop_try_targets.push_back({protected_try_depth, protected_try_depth});
			parse_statement();
			loop_try_targets.pop_back();
			--loop_depth;
			if (emitter) {
				emitter->emit_for_epilogue();
			}
		} else if (maybe_parse_keyword("in")) {
			Node* collection = nullptr;
			Type* ordinal_designator = nullptr;
			// `for X in Name` follows ordinary Pascal name hiding: a visible
			// value is the collection even when a type of the same spelling
			// also exists. Type interpretation is the fallback used only
			// when value lookup finds nothing.
			Node* visible_value = maybe_resolve_value(input_token);
			if (!visible_value && maybe_resolve_type(input_token)) {
				ordinal_designator = parse_type_expression(false);
			} else {
				collection = parse_expression();
			}
			parse_keyword("do");

			auto custom = ordinal_designator ? std::optional<CustomForInResolution>{} : maybe_resolve_custom_for_in(collection, control);
			if (custom) {
				const unsigned enclosing_try_depth = protected_try_depth;
				const unsigned this_try_depth = enclosing_try_depth + 1;
				const unsigned enclosing_exception_block = current_exception_block();
				const bool protected_cleanup = custom->cleanup != nullptr;
				if (emitter) {
					emitter->emit_for_in_custom_setup(custom->get_enumerator, custom->nullable);
				}
				if (protected_cleanup) {
					// The implicit cleanup has the same unwind and
					// goto boundary as a source try/finally, while
					// leaving bare-raise validity unchanged because
					// the user did not enter a nested source handler.
					if (emitter) {
						emitter->emit_try_prologue();
					}
					enter_exception_block();
					++protected_try_depth;
				}
				if (emitter) {
					emitter->emit_for_in_custom_loop_prologue(custom->move_next, custom->current_assignment);
				}
				++loop_depth;
				loop_try_targets.push_back({protected_cleanup ? enclosing_try_depth : protected_try_depth, protected_try_depth});
				parse_statement();
				loop_try_targets.pop_back();
				--loop_depth;
				if (emitter) {
					emitter->emit_for_in_loop_epilogue();
				}
				if (protected_cleanup) {
					--protected_try_depth;
					if (emitter) {
						emitter->emit_try_finally_prologue();
						emitter->emit_statement(custom->cleanup);
						emitter->emit_try_finally_epilogue();
						emitter->emit_for_in_cleanup_control_epilogue(this_try_depth, current_routine ? current_routine->ty : nullptr);
					}
					restore_exception_block(enclosing_exception_block);
				}
				if (emitter) {
					emitter->emit_for_in_custom_epilogue(custom->nullable);
				}
			} else {
				OrdinalRange range;
				std::string range_error;
				Type* element_type = nullptr;

				if (ordinal_designator) {
					if (!ordinal_range_for_type(ordinal_designator, &range, &range_error)) {
						raise_type_error(range_error, ordinal_designator);
					}
					if (enum_range_has_gaps(ordinal_designator, range)) {
						raise_type_error("for-in ordinal type has non-contiguous enum values", ordinal_designator);
					}
					element_type = ordinal_designator;
				} else {
					if (auto bracket = dynamic_cast<BracketLiteral*>(collection)) {
						Type* item_type = bracket->default_set_item_type;
						if (item_type == unknown_type()) {
							item_type = control->ty;
						}
						collection = cast(collection, new FixedSetType(current_location(), item_type));
					}
					Type* collection_type = collection ? collection->ty : nullptr;
					element_type = collection_type ? collection_type->sequence_element_type() : nullptr;
					if (!element_type) {
						auto set = dynamic_cast<FixedSetType*>(collection_type);
						if (!set) {
							raise_type_kind_mismatch("for-in collection", "string, array, or set", collection_type);
						}
						element_type = set->item_type;
						if (!ordinal_range_for_type(element_type, &range, &range_error)) {
							raise_type_error(range_error, element_type);
						}
						if (enum_range_has_gaps(element_type, range)) {
							raise_type_error("for-in set item type has non-contiguous enum values", element_type);
						}
					}
				}

				Node* current = new BuiltinEnumeratorCurrent(element_type);
				// Assign the enumerator's exact element through the ordinary
				// assignment matcher. This keeps loop-control conversion rules
				// identical to an explicit assignment written by the user.
				Node* assignment = mk_assign(control, current);
				if (emitter) {
					if (ordinal_designator) {
						emitter->emit_for_in_ordinal_prologue(ordinal_designator, range.lower_bound, range.upper_bound, assignment);
					} else if (dynamic_cast<FixedSetType*>(collection->ty)) {
						emitter->emit_for_in_set_prologue(collection, range.lower_bound, range.upper_bound, assignment);
					} else {
						emitter->emit_for_in_sequence_prologue(collection, assignment);
					}
				}
				++loop_depth;
				loop_try_targets.push_back({protected_try_depth, protected_try_depth});
				parse_statement();
				loop_try_targets.pop_back();
				--loop_depth;
				if (emitter) {
					emitter->emit_for_in_epilogue();
				}
			}
		} else {
			raise_parse_error("expected ':=' or 'in' after for control variable");
		}
	} else if (peek_keyword("repeat")) {
		parse_keyword("repeat");
		if (emitter) {
			emitter->emit_repeat_prologue();
		}
		++loop_depth;
		loop_try_targets.push_back({protected_try_depth, protected_try_depth});
		parse_block_body();
		loop_try_targets.pop_back();
		--loop_depth;
		parse_keyword("until");
		auto condition = parse_expression();
		if (emitter) {
			emitter->emit_repeat_epilogue(condition);
		}
	} else if (peek_keyword("begin")) {
		parse_keyword("begin");
		parse_block_body();
		parse_keyword("end");
	} else if (peek_keyword("with")) {
		parse_keyword("with");
		// Parse the complete designator before installing its member scope.
		// In particular, `with p^[index] do` must bind the selected record,
		// rather than merely the leading pointer variable. The emitter binds
		// this node once to `auto&&`: places remain references to their
		// original storage, while record-valued calls get a lifetime-extended
		// temporary for the duration of the with body.
		LeadingTokenDirectives target_directives = directive_state.leading_token_directives();
		Node* target = parse_designator(&target_directives);
		target = maybe_auto_call(target, target_directives);
		Frame* body_frame = get_type_body_frame(target->ty);
		if (!body_frame) {
			raise_type_kind_mismatch("with target", "class, record, or object", target->ty);
		}
		parse_keyword("do");
		// emit_with_prologue introduces a C++ block. As with case selectors,
		// nested blocks can safely reuse this tpcc-owned spelling.
		auto alias_slot = new StorageSlot("tpcc_with_target", target->ty);
		if (emitter) {
			emitter->emit_with_prologue(alias_slot->cxx_name, target);
		}
		push_scope(body_frame, alias_slot);
		parse_statement();
		pop_scope();
		if (emitter) {
			emitter->emit_with_epilogue();
		}
	} else {
		// A leading identifier followed by ':' is a Pascal label definition.
		// If the next token is immediately ':=', resolve the identifier in
		// lvalue context before value lookup: Pascal function-name assignment
		// writes the hidden result slot, while expression use stays a call.
		SourceLocation designator_location = current_location();
		LeadingTokenDirectives designator_directives = directive_state.leading_token_directives();
		auto emit_designator_statement = [&](Node* lhs) {
			// A statement here is either an assignment (designator :=
			// expression) or a call (designator, possibly with auto-call).
			// The LHS is still a raw designator here, so assignment never
			// triggers the bare-call transformation.
			if (maybe_parse_colon_equals()) {
				validate_writable_destination(lhs, designator_location, "LHS of ':=' is not assignable");
				Node* rhs = parse_expression();
				auto assign = mk_assign(lhs, rhs);
				if (emitter) {
					emitter->emit_statement(assign);
				}
			} else {
				Node* call = maybe_auto_call(lhs, designator_directives);
				if (emitter) {
					emitter->emit_statement(call);
				}
			}
		};

		if (!input_token.empty() && keywords.find(input_token) == keywords.end()) {
			const LeadingTokenDirectives identifier_directives = designator_directives;
			std::string first = parse_identifier();
			if (maybe_parse_colon()) {
				record_label_definition(first, designator_location);
				if (emitter) {
					emitter->emit_label(cxx_label_name(first));
				}
				parse_statement();
			} else if (input_token == "(" && operator_invocation_identifier(OperatorInvocation::MutatingUnary, first, 1, identifier_directives.overflow_checks, false)) {
				Mutation* mutation = parse_mutation_statement(first, designator_location, identifier_directives);
				if (emitter) {
					emitter->emit_statement(mutation);
				}
			} else {
				Node* lhs = nullptr;
				if (input_token == ":=") {
					lhs = resolve_lvalue(first);
				} else {
					Node* first_value = parse_value_from_identifier(first, identifier_directives, &designator_directives);
					lhs = parse_designator_tail(first_value, designator_directives);
				}
				emit_designator_statement(lhs);
			}
		} else {
			emit_designator_statement(parse_designator(&designator_directives));
		}
	}
}

std::optional<std::string> Parser::maybe_parse_identifier() {
	if (!token_is_identifier(input_token)) {
		return {};
	}
	auto result = input_token;
	consume();
	return result;
}

std::string Parser::parse_identifier() {
	auto result = maybe_parse_identifier();
	if (!result) {
		raise_parse_error("expected identifier");
	}
	return *result;
}

Node* Parser::maybe_parse_numeral() {
	auto input = input_token.data();
	auto input_size = input_token.size();
	int base = 10;
	if (input_size > 0 && (isdigit(*input) || *input == '$' || *input == '%' || *input == '.')) {
		if (*input == '$') {
			base = 16;
			++input;
			--input_size;
		} else if (*input == '%') {
			base = 2;
			++input;
			--input_size;
		}
		if (input_size == 0) {
			raise_parse_error("malformed numeral: " + input_token);
		}
		const bool is_decimal_real = base == 10 && input_token.find_first_of(".eE") != std::string::npos;
		if (!is_decimal_real) {
			uint64_t value;
			auto [ptr, ec] = std::from_chars(input, input + input_size, value, base);
			if (ec != std::errc() || ptr != input + input_size) {
				raise_parse_error("malformed numeral: " + input_token);
			}
			auto lit = new Integer(value, &untyped_integer_type(), false, base != 10);
			consume();
			return lit;
		} else {
			// Preserve the exact source value so the eventual formal type can
			// materialize both a literal and an untyped const alias identically.
			std::string error;
			auto origin = parse_decimal_origin(std::string_view(input, input_size), &error);
			if (!origin) {
				raise_parse_error("malformed real numeral: " + input_token + ": " + error);
			}
			auto lit = new Real(std::move(*origin));
			consume();
			return lit;
		}
	} else {
		return nullptr;
	}
}

Node* Parser::parse_numeral() {
	auto result = maybe_parse_numeral();
	if (!result) {
		raise_parse_error("expected numeral");
	} else {
		return result;
	}
}

bool ScopeEntry::is_receiver_environment() const {
	return qualifier && !dynamic_cast<UnitRef*>(qualifier);
}

ScopeValueLookup ScopeEntry::lookup_value(const std::string& name) const {
	Node* binding = frame->lookup_value(name);
	// A receiver Frame has already consumed every overload that is visible
	// through class/object inheritance. Its completed member binding shadows
	// lower lexical and unit-global scopes exactly like any ordinary scoped
	// declaration. UnitRef is deliberately not a receiver here: used-unit
	// globals share the global overload domain and may attach across entries.
	return ScopeValueLookup{binding, binding && !is_receiver_environment() && callable_binding_opens_parent(binding)};
}

Node* Parser::bind_lookup_result(Node* qualifier, Node* binding) {
	if (!qualifier || dynamic_cast<UnitRef*>(qualifier)) {
		return binding;
	}

	// A class reference and a record type qualifier open a member environment,
	// but neither supplies an instance receiver. Reject an already-selected
	// instance member here; overload sets remain intact until their visible
	// arguments select one declaration in finalize_call.
	const bool class_reference = dynamic_cast<ClassRefType*>(qualifier->ty) != nullptr;
	const bool type_qualifier = dynamic_cast<TypeMemberQualifier*>(qualifier) != nullptr;
	if (class_reference || type_qualifier) {
		if (auto slot = dynamic_cast<StorageSlot*>(binding); slot && slot->kind != StorageSlot::Kind::StaticMember) {
			raise_type_error("instance field cannot be accessed through a " + std::string(type_qualifier ? "type" : "class reference"), slot->ty);
		} else if (auto property = dynamic_cast<Property*>(binding)) {
			raise_type_error("instance property cannot be accessed through a " + std::string(type_qualifier ? "type" : "class reference"), property->ty);
		} else if (auto method = dynamic_cast<Method*>(binding)) {
			const bool allowed = method->is_static || (!type_qualifier && (method->ty->kind == CLASS_METHOD || method->ty->kind == CONSTRUCTOR));
			if (!allowed) {
				raise_type_error("instance method cannot be accessed through a " + std::string(type_qualifier ? "type" : "class reference"), method->ty);
			}
		}
	}
	if (auto property = dynamic_cast<Property*>(binding)) {
		return new PropertyAccess(qualifier, property, {});
	}
	auto access = new MemberAccess(qualifier, binding);
	access->ty = binding->ty;
	return access;
}

/** Ordinary expression lookup is lexical before it is categorical. Inspect
 *  each active environment once and stop at the nearest declaration, whether
 *  it is a Type or a Node. This retained tag is what lets `High(1)` choose a
 *  local type High over the farther System.High value while required-type
 *  lookup can still skip a nearer value for declarations such as `X: X`.
 *
 *  Callable overload families retain their existing exception: an
 *  overload-open value may collect compatible callables from lower
 *  environments, but a nearer type terminates that collection. */
std::optional<Binding> Parser::maybe_resolve_type_or_value(std::string name) {
	std::vector<Callable*> collected;
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (auto unit = dynamic_cast<UnitRef*>(it->qualifier); unit && unit->unit && unit->unit->name == name) {
			if (collected.empty()) {
				return Binding{std::in_place_type<Node*>, unit};
			}
			break;
		}

		auto binding = it->frame->lookup_type_or_value(name);
		if (!binding) {
			continue;
		}
		if (auto type = std::get_if<Type*>(&*binding)) {
			if (collected.empty()) {
				return Binding{std::in_place_type<Type*>, *type};
			}
			break;
		}

		Node* hit = std::get<Node*>(*binding);
		const bool opens_parent = hit && !it->is_receiver_environment() && callable_binding_opens_parent(hit);
		auto as_call = dynamic_cast<Callable*>(hit);
		auto as_set = dynamic_cast<OverloadSet*>(hit);
		if (!opens_parent && collected.empty()) {
			return Binding{std::in_place_type<Node*>, bind_lookup_result(it->qualifier, hit)};
		}
		if (as_call) {
			if (!collected.empty() && !same_callable_lookup_family(collected.front(), as_call)) {
				break;
			}
			collected.push_back(as_call);
		} else if (as_set) {
			if (as_set->members.empty()) {
				continue;
			}
			if (!collected.empty() && !same_callable_lookup_family(collected.front(), as_set->members.front())) {
				break;
			}
			collected.insert(collected.end(), as_set->members.begin(), as_set->members.end());
		} else {
			break;
		}
		if (!opens_parent) {
			break;
		}
	}
	if (collected.empty()) {
		return std::nullopt;
	}
	Node* result = collected.size() == 1 ? static_cast<Node*>(collected.front()) : static_cast<Node*>(new OverloadSet(std::move(collected)));
	return Binding{std::in_place_type<Node*>, result};
}

/** Walk the scope stack top-down looking up a value-position name (variable,
 *  constant, procedure, function, builtin). Return null if not found.
 *
 *  A receiver-qualified hit selects the member lookup domain. Its callable
 *  family is gathered only through structural parents and is returned with
 *  that receiver bound. It must not be merged with same-named routines in
 *  lexical or unit scopes. */
Node* Parser::maybe_resolve_value(std::string name) {
	std::vector<Callable*> collected;
	// Walk top-down. The first hit shadows unless it is overload-marked. Once
	// opened, include the first compatible family from each lower scope; a
	// family without `overload` is included and then terminates the walk.
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		ScopeValueLookup lookup = it->lookup_value(name);
		Node* hit = lookup.binding;
		if (!hit) {
			continue;
		}
		auto as_call = dynamic_cast<Callable*>(hit);
		auto as_set = dynamic_cast<OverloadSet*>(hit);
		if (collected.empty() && !as_call && !as_set) {
			// First (and terminating) hit is a non-callable value.
			return bind_lookup_result(it->qualifier, hit);
		}
		if (!lookup.opens_parent && collected.empty()) {
			return bind_lookup_result(it->qualifier, hit);
		}
		if (as_call) {
			if (!collected.empty() && !same_callable_lookup_family(collected.front(), as_call)) {
				break;
			}
			collected.push_back(as_call);
		} else if (as_set) {
			if (as_set->members.empty()) {
				continue;
			}
			if (!collected.empty() && !same_callable_lookup_family(collected.front(), as_set->members.front())) {
				break;
			}
			// Same-frame sets are homogeneous by registration. The source
			// `overload` bit controls whether lookup reaches this lower
			// environment, not whether its local members coexist.
			for (auto* m : as_set->members) {
				collected.push_back(m);
			}
		} else {
			// Non-callable value below a collected overload block -- stop.
			break;
		}
		if (!lookup.opens_parent) {
			break;
		}
	}
	if (collected.empty()) {
		return nullptr;
	}
	if (collected.size() == 1) {
		return collected[0];
	}
	return new OverloadSet(std::move(collected));
}

/** Walk the scope stack top-down looking up a value-position name. Raise if not found. */
Node* Parser::resolve_value(std::string name) {
	if (Node* hit = maybe_resolve_value(name)) {
		return hit;
	}
	raise_parse_error("unresolved value identifier: " + name);
	return nullptr;
}

Node* Parser::active_function_result_lvalue(Callable* c) const {
	if (!c || !c->body_frame || c->ty->return_type == &unit_type()) {
		return nullptr;
	}
	bool active = false;
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		for (const Frame* frame = it->frame; frame; frame = frame->parent) {
			if (frame == c->body_frame) {
				active = true;
				break;
			}
		}
		if (active) {
			break;
		}
	}
	if (!active) {
		return nullptr;
	}
	return c->body_frame->lookup_value("result");
}

/** value that can be assigned to */
Node* Parser::resolve_lvalue(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Node* hit = it->lookup_value(name).binding) {
			if (auto c = dynamic_cast<Callable*>(hit)) {
				if (Node* result = active_function_result_lvalue(c)) {
					return result;
				}
			} else if (auto os = dynamic_cast<OverloadSet*>(hit)) {
				for (auto* c : os->members) {
					if (Node* result = active_function_result_lvalue(c)) {
						return result;
					}
				}
			}
			return bind_lookup_result(it->qualifier, hit);
		}
	}
	raise_parse_error("unresolved lvalue identifier: " + name);
	return nullptr;
}

/** Resolve a type name for a fresh parser consumer. Once an earlier
 * declaration in the active type block has been published, this peels its
 * resolved IncompleteType chain so inheritance and later declaration syntax
 * can inspect the real Type immediately.
 *
 * This is intentionally not graph normalization: Type* fields stored before
 * publication still point at their old placeholders until TypeBlockResolver
 * rewrites the complete block. Therefore no caller in the open-block phase
 * may compare a fresh result from here with a previously stored edge to decide
 * type identity, overload signatures, property compatibility, C++ carriers,
 * or overriding. Those decisions belong to the post-normalization phase. */
Type* Parser::maybe_resolve_type(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		Type* hit = it->frame->lookup_type(name);
		if (!hit) {
			continue;
		}
		std::unordered_set<IncompleteType*> seen;
		while (auto incomplete = dynamic_cast<IncompleteType*>(hit)) {
			if (!incomplete->resolved) {
				return incomplete;
			}
			if (!seen.insert(incomplete).second) {
				raise_parse_error("cyclic resolved type alias involving '" + incomplete->name + "'");
			}
			hit = incomplete->resolved;
		}
		if (hit) {
			return hit;
		}
	}
	return nullptr;
}

static const BuiltinDesc* builtin_desc_for_node(Node* n) {
	if (auto b = dynamic_cast<Builtin*>(n)) {
		return b->desc;
	} else if (auto c = dynamic_cast<Callable*>(n)) {
		return c->builtin_desc;
	}
	return nullptr;
}

static std::optional<TypeBoundKind> type_bound_kind_for_builtin(Node* n) {
	const BuiltinDesc* desc = builtin_desc_for_node(n);
	return desc ? desc->type_bound_kind : std::optional<TypeBoundKind>{};
}

static BuiltinSyntaxKind syntax_kind_for_builtin(Node* n) {
	if (auto overloads = dynamic_cast<OverloadSet*>(n)) {
		BuiltinSyntaxKind common = BuiltinSyntaxKind::None;
		for (Callable* member : overloads->members) {
			const BuiltinDesc* desc = member->builtin_desc;
			if (!desc || desc->syntax_kind == BuiltinSyntaxKind::None) {
				return BuiltinSyntaxKind::None;
			}
			if (common != BuiltinSyntaxKind::None && common != desc->syntax_kind) {
				return BuiltinSyntaxKind::None;
			}
			common = desc->syntax_kind;
		}
		// This selects only grammar shared by every candidate. An unresolved
		// overload set never supplies lowering metadata; lowering reads the
		// Callable selected by finalize_call.
		return common;
	}
	const BuiltinDesc* desc = builtin_desc_for_node(n);
	return desc ? desc->syntax_kind : BuiltinSyntaxKind::None;
}

static bool is_integer_semantic_type(Type* ty);

static Type* natural_real_origin_type(Node* value) {
	auto real = dynamic_cast<Real*>(value);
	if (!real || !real->is_origin()) {
		return nullptr;
	}
	const std::array<Type*, 3> candidates{{single_type(), double_type(), extended_type()}};
	Type* widest_viable = nullptr;
	// A syntax form such as SizeOf(value) or Write(value) has no formal
	// parameter to materialize an origin. Use the same value rule as overload
	// selection: the smallest exact domain, otherwise the most informative
	// finite domain.
	for (Type* candidate : candidates) {
		RealMaterialization materialized = materialize_decimal_origin(*real->origin, candidate);
		if (materialized.kind == RealMaterializationKind::Exact) {
			return candidate;
		}
		if (materialized.kind == RealMaterializationKind::Rounded) {
			widest_viable = candidate;
		}
	}
	return widest_viable;
}

enum class StrValueFamily {
	Integer,
	ExistingReal,
	EnumerationTodo,
	Unsupported,
};

static StrValueFamily str_value_family(Type* ty) {
	if (is_integer_semantic_type(ty)) {
		return StrValueFamily::Integer;
	}
	ty = distinct_storage_type(ty);
	if (ty == single_type() || ty == double_type() || ty == extended_type()) {
		// Preserve the already-modelled concrete real overloads. Width and
		// precision still have a separate, deliberately unimplemented path.
		return StrValueFamily::ExistingReal;
	} else if (dynamic_cast<EnumType*>(ty)) {
		// TODO: select either enum-name or ordinal-number formatting here once
		// generated enum metadata has an explicit runtime representation.
		return StrValueFamily::EnumerationTodo;
	}
	// TODO: Currency and Comp belong in distinct cases here after their
	// Pascal types and runtime carriers exist. Do not identify them by C++
	// representation or accidentally fold them into the integer/real cases.
	return StrValueFamily::Unsupported;
}

enum class ValDestinationFamily {
	Integer,
	ExistingReal,
	EnumerationTodo,
	Unsupported,
};

static ValDestinationFamily val_destination_family(Type* ty) {
	if (is_integer_semantic_type(ty)) {
		return ValDestinationFamily::Integer;
	}
	ty = distinct_storage_type(ty);
	if (ty == single_type() || ty == double_type() || ty == extended_type()) {
		return ValDestinationFamily::ExistingReal;
	} else if (dynamic_cast<EnumType*>(ty)) {
		// TODO: enum Val needs name-to-ordinal lookup metadata. This explicit
		// family is also where accepting a numeric spelling could be added.
		return ValDestinationFamily::EnumerationTodo;
	}
	// TODO: add target-dependent Real, Currency, and Comp cases only when
	// those semantic types and their distinct runtime contracts exist.
	return ValDestinationFamily::Unsupported;
}

/** Select only the backend implementation of an already-resolved builtin.
 *
 *  Pascal lookup, overload ranking, and argument conversion run before this
 *  helper. The declaration therefore remains the source-visible identity;
 *  the finite compiler-owned operation merely chooses its complete checked
 *  or unchecked RTL entry point from the directive state captured at the
 *  construct's leading token. */
static const BuiltinDesc* builtin_implementation_at_call_site(const BuiltinDesc* descriptor, LeadingTokenDirectives directives) {
	if (!descriptor) {
		return nullptr;
	}
	bool enabled = true;
	switch (descriptor->call_site_switch) {
	case BuiltinCallSiteSwitch::None:
		assert(descriptor->disabled_cxx_name.empty());
		return descriptor;
	case BuiltinCallSiteSwitch::Overflow:
		enabled = directives.overflow_checks;
		break;
	case BuiltinCallSiteSwitch::Io:
		enabled = directives.io_checks;
		break;
	}
	if (enabled) {
		return descriptor;
	}
	assert(!descriptor->disabled_cxx_name.empty());
	const BuiltinDesc* alternate = lookup_builtin_desc(descriptor->disabled_cxx_name);
	assert(alternate);
	assert(alternate->call_site_switch == BuiltinCallSiteSwitch::None);
	return alternate;
}

/** Same as resolve_value but for type-position names.
 *  If allow_forward is true and NAME isn't in scope, register a fresh
 *  IncompleteType in the active type block's owning frame and return it. That
 *  owner remains stable while parsing an aggregate RHS; the aggregate's
 *  member frame is not the declaration environment for sibling type names.
 *  This is legal only while parse_type_block is consuming RHS types; outside
 *  that narrow window Pascal has no general declaration-order backpatching.
 *  If allow_forward is false, or no type block is active, an unresolved name
 *  is a hard error. */
Type* Parser::resolve_type(std::string name, bool allow_forward) {
	if (Type* hit = maybe_resolve_type(name)) {
		return hit;
	}
	if (allow_forward && !type_block_frames.empty()) {
		auto inc = new IncompleteType(current_location(), name);
		if (!type_block_frames.back()->register_type(name, inc)) {
			raise_parse_error("duplicate identifier: " + name);
		}
		return inc;
	}
	raise_parse_error("unresolved type identifier: " + name);
	return nullptr;
}

// Parent of a composite type with single-inheritance, or nullptr. ClassType
// and ObjectType each carry a `super` of their own type; no shared base, so
// two dynamic_casts. InterfaceType is excluded -- interfaces use
// super_interfaces (a vector), not a single parent.
static Type* parent_of(Type* ty) {
	if (auto c = dynamic_cast<ClassType*>(ty)) {
		return c->super;
	} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
		return o->super;
	}
	return nullptr;
}

static Frame* make_aggregate_body_frame(Type* owner) {
	return new Frame(owner ? get_type_body_frame(parent_of(owner)) : nullptr);
}

// Walk the parent chain from STARTING_AT, looking up NAME in each level's
// body Frame. Returns the first hit as Node* (Callable* or OverloadSet*),
// or nullptr if not found. Caller (parse_inherited) routes the result
// through finalize_call, which ranks overload sets by argument cost.
static Node* lookup_method_in_ancestors(std::string name, Type* starting_at) {
	Frame* body = body_frame_of(starting_at);
	return body ? body->lookup_value(name) : nullptr;
}

static bool is_set_item_type(Type* ty) {
	while (auto subrange = dynamic_cast<SubrangeType*>(ty)) {
		ty = subrange->base_type;
	}
	if (dynamic_cast<EnumType*>(ty)) {
		return true;
	}
	OrdinalBounds bounds;
	return intrinsic_ordinal_bounds(ty, &bounds);
}

static bool conversion_is_better(const ValueConversion& a, const ValueConversion& b) {
	if (a.kind != b.kind) {
		return a.kind == ValueConversionClass::Direct;
	}
	return a.distance < b.distance;
}

static Type* infer_set_item_type(Type* current, Type* next) {
	// The caller classifies untyped integer constants by their natural
	// Delphi type before asking for a common set domain. Silently replacing an
	// unresolved untyped value with Integer here would make constructor
	// inference disagree with ordinary argument matching.
	if (current == &untyped_integer_type() || next == &untyped_integer_type()) {
		return nullptr;
	}
	if (current == next) {
		return current;
	}

	auto next_to_current = current->value_conversion_from(next);
	auto current_to_next = next->value_conversion_from(current);
	if (next_to_current && (!current_to_next || conversion_is_better(*next_to_current, *current_to_next))) {
		return current;
	}
	if (current_to_next && (!next_to_current || conversion_is_better(*current_to_next, *next_to_current))) {
		return next;
	}
	// Set constructors need one ordinal storage domain for all their items.
	// This is contextual constructor inference, not binary-operator
	// resolution: neither source operand is rewritten before looking up an
	// operator. If both directions are equally good there is no principled
	// common item type, so require context or reject the constructor.
	return nullptr;
}

Node* Parser::parse_bracket_literal() {
	const SourceLocation location = current_location();
	parse_opening_bracket();
	std::vector<BracketLiteral::Item> items;
	if (input_token != "]") {
		do {
			Node* lower = parse_expression();
			Node* upper = nullptr;
			if (maybe_parse_period_period()) {
				upper = parse_expression();
			}
			items.push_back(BracketLiteral::Item{lower, upper});
		} while (maybe_parse_comma());
	}
	parse_closing_bracket();

	// Brackets are syntax, not an already-selected set operation. Keep every
	// source item unchanged so a set, dynamic-array, or open-array candidate
	// can supply its own element type. A best-effort common type exists only
	// for consumers such as SizeOf which provide no destination at all.
	Type* array_item_type = infer_bracket_common_item_type(items, BracketIntegerPreference::Array);
	Type* set_item_type = infer_bracket_common_item_type(items, BracketIntegerPreference::Set);
	Type* default_set_item_type = set_item_type && set_item_type != unknown_type() && is_set_item_type(set_item_type) ? set_item_type : unknown_type();

	bool has_range = false;
	for (const auto& item : items) {
		has_range = has_range || item.upper != nullptr;
	}

	FixedArrayType* default_array_type = nullptr;
	if (!has_range && !items.empty() && array_item_type && array_item_type != unknown_type()) {
		const uint64_t length = static_cast<uint64_t>(items.size());
		auto lower = new Integer(0, integer_type());
		auto upper = new Integer(length - 1, integer_type());
		OrdinalRange range;
		range.index_type = integer_type();
		range.base_type = integer_type();
		range.lower_bound = lower;
		range.upper_bound = upper;
		range.lower_ordinal = OrdinalRange::Value{false, 0};
		range.upper_ordinal = OrdinalRange::Value{false, length - 1};
		range.length = length;
		default_array_type = new FixedArrayType(location, integer_type(), range, array_item_type);
	}
	return new BracketLiteral(std::move(items), default_set_item_type, default_array_type);
}

Node* Parser::parse_new_or_dispose(bool is_new) {
	SourceLocation operation_location = current_location();
	parse_opening_paren();

	Node* destination_or_pointer = nullptr;
	PointerType* pointer_type = nullptr;
	Type* pointer_operand_type = nullptr;
	bool functional_form = false;

	// The first operand selects one of Pascal's two forms. Ordinary value
	// lookup wins over type lookup, so a nearer variable shadows a pointer
	// type with the same spelling just as it does elsewhere in expression
	// syntax. Only New has a functional type form; Dispose always consumes a
	// pointer value.
	Node* visible_value = maybe_resolve_value(input_token);
	if (visible_value || !is_new) {
		destination_or_pointer = parse_designator();
		pointer_operand_type = destination_or_pointer ? destination_or_pointer->ty : nullptr;
		pointer_type = dynamic_cast<PointerType*>(pointer_operand_type);
	} else if (maybe_resolve_type(input_token)) {
		Type* parsed_type = parse_type_expression(false);
		pointer_operand_type = parsed_type;
		pointer_type = dynamic_cast<PointerType*>(parsed_type);
		functional_form = true;
	} else {
		raise_parse_error("New first operand is neither a pointer "
		                  "variable nor a pointer type");
	}

	if (!pointer_type || pointer_type->is_untyped()) {
		raise_type_kind_mismatch_at(operation_location, std::string(is_new ? "New" : "Dispose") + " first operand", "typed pointer", pointer_operand_type);
	}
	if (is_new && !functional_form && !is_assignable(destination_or_pointer)) {
		raise_value_error_at(operation_location, "New destination is not assignable", destination_or_pointer);
	}

	Type* allocated_type = pointer_type->item_type;

	Method* lifecycle_method = nullptr;
	std::vector<Node*> lifecycle_args;
	if (maybe_parse_comma()) {
		auto object = dynamic_cast<ObjectType*>(allocated_type);
		if (!object) {
			raise_type_kind_mismatch_at(operation_location, std::string(is_new ? "New" : "Dispose") + " lifecycle pointee", "old-style object", allocated_type);
		}

		std::string lifecycle_name = parse_identifier();
		if (maybe_parse_opening_paren()) {
			if (input_token != ")") {
				if (!is_new) {
					raise_type_error_at(operation_location,
					                    "Dispose destructor cannot have "
					                    "arguments",
					                    object);
				}
				lifecycle_args.push_back(parse_expression());
				while (maybe_parse_comma()) {
					lifecycle_args.push_back(parse_expression());
				}
			}
			parse_closing_paren();
		}

		Node* candidates = object->children ? object->children->lookup_value(lifecycle_name) : nullptr;
		if (!candidates) {
			raise_type_error_at(operation_location, "no old-style object member '" + lifecycle_name + "'", object);
		}

		// Reuse ordinary member overload finalization. The receiver exists
		// only to establish the already-known pointed-to object context;
		// NewValue/DisposeValue later supply the actual runtime pointer.
		auto semantic_receiver = new StorageSlot("", pointer_type);
		auto target = new MemberAccess(semantic_receiver, candidates);
		target->ty = candidates->ty;
		auto finalized = finalize_call(target, lifecycle_args, lifecycle_name, operation_location);
		lifecycle_method = dynamic_cast<Method*>(finalized.callee);
		if (!lifecycle_method || lifecycle_method->ty->kind != (is_new ? CONSTRUCTOR : DESTRUCTOR)) {
			raise_type_kind_mismatch_at(operation_location, std::string(is_new ? "New" : "Dispose") + " second operand", is_new ? "constructor" : "destructor", finalized.callee ? finalized.callee->ty : nullptr);
		}
	}
	parse_closing_paren();

	if (is_new) {
		auto value = new NewValue(pointer_type, allocated_type, lifecycle_method, std::move(lifecycle_args));
		if (functional_form) {
			return value;
		}
		return mk_assign(destination_or_pointer, value);
	}
	return new DisposeValue(destination_or_pointer, lifecycle_method);
}

Node* Parser::parse_value(LeadingTokenDirectives* leading_directives) {
	const LeadingTokenDirectives primary_directives = directive_state.leading_token_directives();
	if (leading_directives) {
		*leading_directives = primary_directives;
	}
	if (peek_keyword("inherited")) {
		return parse_inherited();
	} else if (input_token == "[") {
		return parse_bracket_literal();
	} else if (maybe_parse_opening_paren()) { // grouping paren
		auto result = parse_expression();
		parse_closing_paren();
		return result;
	} else if (auto n = maybe_parse_numeral()) {
		return n;
	} else if (peek_keyword("nil")) {
		consume();
		return new NilLiteral();
	} else if (!input_token.empty() && (input_token.front() == '\'' || input_token.front() == '#')) {
		// FPC scans a consecutive run of quoted fragments and numeric character
		// fragments as one literal:
		//
		//   'A'          -> one byte, Char
		//   #13          -> one byte, Char
		//   'A'#0'B'     -> three bytes, string
		//   #13#10       -> two bytes, string
		//
		// tpcc's tokenizer currently returns each fragment separately, so
		// combine the run here before assigning its semantic type. This also
		// preserves embedded zero bytes: String::value is length-bearing and
		// emission passes that explicit length to the RTL instead of using
		// strlen.
		std::string s;
		do {
			if (input_token.front() == '\'') {
				s += extract_string_literal(input_token);
			} else {
				const char* first = input_token.data() + 1;
				const char* last = input_token.data() + input_token.size();
				uint64_t value = 0;
				auto [end, error] = std::from_chars(first, last, value, 10);
				if (first == last || error != std::errc() || end != last || value > 255) {
					raise_parse_error("malformed character-code literal: " + input_token);
				}
				s.push_back(static_cast<char>(static_cast<unsigned char>(value)));
			}
			consume();
		} while (!input_token.empty() && (input_token.front() == '\'' || input_token.front() == '#'));
		Type* literal_type = s.size() == 1 ? char_type() : shortstring_type();
		return new String(std::move(s), literal_type, true);
	} else {
		const LeadingTokenDirectives identifier_directives = directive_state.leading_token_directives();
		return parse_value_from_identifier(parse_identifier(), identifier_directives, leading_directives);
	}
}

Node* Parser::parse_value_from_identifier(std::string id, LeadingTokenDirectives identifier_directives, LeadingTokenDirectives* leading_directives) {
	if (leading_directives) {
		*leading_directives = identifier_directives;
	}
	std::optional<Binding> binding;
	bool qualified_member = false;
	if (maybe_parse_period()) {
		qualified_member = true;
		Node* base = nullptr;
		Type* rejected_qualifier_type = nullptr;
		auto qualifier_binding = maybe_resolve_type_or_value(id);
		if (qualifier_binding) {
			if (auto value = std::get_if<Node*>(&*qualifier_binding)) {
				base = *value;
			} else {
				Type* qualifier_type = std::get<Type*>(*qualifier_binding);
				rejected_qualifier_type = qualifier_type;
				if (auto class_type = dynamic_cast<ClassType*>(qualifier_type)) {
					base = new ClassRefValue(class_type);
				} else if (dynamic_cast<RecordType*>(qualifier_type) || dynamic_cast<PackedRecordType*>(qualifier_type)) {
					base = new TypeMemberQualifier(qualifier_type);
				}
			}
		}
		if (!base && rejected_qualifier_type) {
			raise_type_kind_mismatch("member qualifier '" + id + "'", "class or record", rejected_qualifier_type);
		}
		if (!base) {
			raise_parse_error("unresolved member qualifier: " + id);
		}
		LeadingTokenDirectives member_directives;
		Node* member = parse_member_selection(base, &member_directives);
		identifier_directives = member_directives;
		if (leading_directives) {
			*leading_directives = member_directives;
		}
		// Qualification changes only the lookup input. Once the member has
		// been resolved, declaration-owned builtin grammar must see the same
		// Node as an unqualified lookup; returning here used to send
		// System.Write/New/SizeOf/Low/Str through generic call parsing.
		binding = Binding{std::in_place_type<Node*>, member};
		if (auto callable = dynamic_cast<Callable*>(member); callable && !callable->pas_name.empty()) {
			id = callable->pas_name;
		} else if (auto overloads = dynamic_cast<OverloadSet*>(member); overloads && !overloads->members.empty() && !overloads->members.front()->pas_name.empty()) {
			id = overloads->members.front()->pas_name;
		}
	} else {
		binding = maybe_resolve_type_or_value(id);
	}
	if (binding && std::holds_alternative<Node*>(*binding)) {
		Node* value = std::get<Node*>(*binding);
		// Within a function body, a bare occurrence of that function's name
		// denotes its hidden result variable.  Parentheses still mean a call,
		// which is how recursive calls remain distinguishable.  resolve_lvalue
		// already applies this rule for `FunctionName := value`; it is equally
		// required in value context for representation overlays such as
		// `TWordRec(reverse_word).hi`.
		if (!qualified_member && input_token != "(") {
			if (auto c = dynamic_cast<Callable*>(value)) {
				if (Node* result = active_function_result_lvalue(c)) {
					return result;
				}
			} else if (auto os = dynamic_cast<OverloadSet*>(value)) {
				for (auto* c : os->members) {
					if (Node* result = active_function_result_lvalue(c)) {
						return result;
					}
				}
			}
		}
		// Low/High are type-argument intrinsics, so ordinary call finalization
		// cannot infer their result type from a RoutineType. Dispatch on the
		// resolved builtin object rather than the source spelling: user shadowing
		// still wins, and seeded subrange expressions use the same path as normal
		// value expressions.
		if (auto kind = type_bound_kind_for_builtin(value); kind && input_token == "(") {
			parse_opening_paren();
			// Low/High accept either a type or a value. Their argument syntax
			// is otherwise ambiguous, so use the same rule as New's
			// type-or-value form: an ordinary visible value wins, and a named
			// type is considered only when no value has that spelling.
			const bool explicit_type_start = peek_keyword("array") || peek_keyword("string") || peek_keyword("set") || peek_keyword("file") || peek_keyword("object") || peek_keyword("packed") || peek_keyword("record") || peek_keyword("class") || peek_keyword("interface") || peek_keyword("procedure") || peek_keyword("function") || peek_keyword("operator") || input_token == "^";
			Node* visible_value = maybe_resolve_value(input_token);
			Type* visible_type = visible_value ? nullptr : maybe_resolve_type(input_token);
			if (explicit_type_start || visible_type) {
				Type* target_ty = parse_type_expression(false);
				parse_closing_paren();
				return new TypeBound(*kind, target_ty);
			}
			Node* operand = parse_expression();
			parse_closing_paren();
			Type* operand_type = operand ? operand->ty : nullptr;
			if (operand_type &&
			    is_ordinal_intrinsic_argument(operand_type)) {
				// An ordinal value contributes only its static Pascal type:
				// Low(enum_variable) is exactly Low(EnumType), independent
				// of the variable's current value.  Do not send it through
				// ValueBound, which exists for bounds obtained from a
				// sequence value such as a dynamic array.
				return new TypeBound(*kind, operand_type);
			}
			Type* result_type = operand_type ? operand_type->sequence_index_type() : nullptr;
			if (!result_type) {
				raise_type_kind_mismatch("Low/High value operand", "ordinal, string, or array", operand_type);
			}
			return new ValueBound(*kind, operand, result_type);
		}
		BuiltinSyntaxKind syntax_kind = syntax_kind_for_builtin(value);
		if (syntax_kind == BuiltinSyntaxKind::NewValue || syntax_kind == BuiltinSyntaxKind::DisposeValue) {
			if (input_token != "(") {
				raise_parse_error(syntax_kind == BuiltinSyntaxKind::NewValue ? "New requires an argument list" : "Dispose requires an argument list");
			}
			return parse_new_or_dispose(syntax_kind == BuiltinSyntaxKind::NewValue);
		} else if (syntax_kind == BuiltinSyntaxKind::Str && input_token == "(") {
			// Str's colons belong to its first actual:
			//
			//     Str(value[:width[:precision]], destination)
			//
			// They are not extra routine arguments. Parse that grammar here,
			// through the same formatted-value helper as Write/WriteLn, then
			// run the ordinary overload matcher on the two Pascal arguments.
			// This preserves declaration lookup, shadowing, and overload
			// ranking while retaining the formatting expressions in a
			// compiler-owned semantic node.
			SourceLocation call_location = current_location();
			parse_opening_paren();
			FormattedValue item = parse_formatted_value();
			if (!maybe_parse_comma()) {
				raise_parse_error("Str requires a destination after its formatted value");
			}
			Node* destination = parse_expression();
			parse_closing_paren();

			std::vector<Node*> args{item.value, destination};
			FinalizedCall finalized = finalize_call(value, args, id, call_location);
			const BuiltinDesc* selected = builtin_desc_for_node(finalized.callee);
			if (!selected || selected->generic_kind != BuiltinGenericKind::StrOutput) {
				if (item.width || item.precision) {
					raise_parse_error("colon formatting requires the predefined Str");
				}
				return make_call(finalized, std::move(args), identifier_directives);
			}

			item.value = args[0];
			destination = args[1];
			Type* destination_type =
			    destination ? destination->ty : nullptr;
			if (!dynamic_cast<ShortStringType*>(destination_type) &&
			    destination_type != ansistring_type()) {
				raise_type_kind_mismatch(
				    "Str destination",
				    "ShortString or AnsiString",
				    destination_type);
			}
			const StrValueFamily family = str_value_family(item.value ? item.value->ty : nullptr);
			if (family == StrValueFamily::EnumerationTodo) {
				raise_parse_error("Str enumeration formatting is not implemented");
			}
			if (family == StrValueFamily::Unsupported) {
				raise_type_kind_mismatch("Str value", "integer or predefined real", item.value ? item.value->ty : nullptr);
			}
			if (item.precision &&
			    family != StrValueFamily::ExistingReal) {
				// Pascal's second colon is the fractional-digit count of a
				// real value, not a generic third formatting operand.
				raise_parse_error(
				    "Str precision requires a predefined real value");
			}

			Node* result = new StrCall(item, destination);
			if (finalized.qualifier_effect) {
				result = new EvaluateThen(finalized.qualifier_effect, result);
			}
			return result;
		} else if (syntax_kind == BuiltinSyntaxKind::Write || syntax_kind == BuiltinSyntaxKind::WriteLn) {
			std::vector<FormattedValue> items;
			if (maybe_parse_opening_paren()) {
				if (input_token != ")") {
					do {
						FormattedValue formatted = parse_formatted_value();
						// Unlike an ordinary call, Write has no formal
						// parameter to give an untyped integer literal its
						// default Pascal carrier.
						if (formatted.value->ty == &untyped_integer_type()) {
							formatted.value = cast(formatted.value, integer_type());
						} else if (formatted.value->ty == &untyped_real_type()) {
							Type* natural = natural_real_origin_type(formatted.value);
							if (!natural) {
								raise_value_error("real Write/WriteLn value is outside every predefined real domain", formatted.value);
							}
							formatted.value = cast(formatted.value, natural);
						}
						if (formatted.precision && formatted.value->ty != single_type() && formatted.value->ty != double_type() && formatted.value->ty != extended_type()) {
							raise_type_kind_mismatch("a second Write/WriteLn colon qualifier", "real", formatted.value->ty);
						}
						items.push_back(formatted);
					} while (maybe_parse_comma());
				}
				parse_closing_paren();
			}

			Node* file = nullptr;
			if (!items.empty() && items.front().value->ty == text_type()) {
				if (items.front().width || items.front().precision) {
					raise_parse_error("a Write/WriteLn text-file argument "
					                  "cannot have formatting qualifiers");
				}
				file = items.front().value;
				if (!is_referenceable(file)) {
					raise_value_error("Write/WriteLn text-file argument "
					                  "must be storage-backed",
					                  file);
				}
				items.erase(items.begin());
			}
			const BuiltinDesc* implementation = builtin_implementation_at_call_site(builtin_desc_for_node(value), identifier_directives);
			assert(implementation);
			return new WriteCall(syntax_kind == BuiltinSyntaxKind::WriteLn, file, implementation, std::move(items));
		} else if (syntax_kind_for_builtin(value) == BuiltinSyntaxKind::SizeOf && input_token == "(") {
			parse_opening_paren();
			Type* operand_type = nullptr;
			if (Type* named_type = maybe_resolve_type(input_token); named_type && !maybe_resolve_value(input_token)) {
				operand_type = parse_type_expression(false);
			} else {
				Node* operand = parse_expression();
				if (operand && operand->ty == &untyped_real_type()) {
					Type* natural = natural_real_origin_type(operand);
					if (!natural) {
						raise_value_error("real SizeOf operand is outside every predefined real domain", operand);
					}
					operand = cast(operand, natural);
				}
				operand_type = operand ? operand->ty : nullptr;
			}
			parse_closing_paren();
			if (!operand_type) {
				raise_parse_error("SizeOf operand has no type");
			}
			return new SizeOf(operand_type);
		} else {
			return value;
		}
	}
	if (input_token == "(") {
		Type* target_ty = binding ? (std::get_if<Type*>(&*binding) ? std::get<Type*>(*binding) : nullptr) : nullptr;
		if (target_ty) {
			parse_opening_paren();
			Node* value = parse_expression();
			parse_closing_paren();

			// Pascal typecast syntax is `Type(expr)`, so the requested result
			// type is available before conversion lookup. Conversion operators
			// may overload by that result even though ordinary routines cannot.
			// Try the ordered direct operator contracts before the predefined
			// cast: otherwise a user-defined Explicit conversion could never
			// replace the built-in boundary in the same way Implicit can.
			if (Node* converted = match_explicit_conversion(value, target_ty, false)) {
				return converted;
			}
			if (auto real = dynamic_cast<Real*>(value); real && real->is_origin() && is_real_semantic_type(target_ty)) {
				RealMaterialization converted = materialize_decimal_origin(*real->origin, target_ty);
				if (converted.kind != RealMaterializationKind::InvalidTarget) {
					return new Real(converted.value, target_ty);
				}
			}

			// A declared Explicit operation overrides the predefined type
			// boundary, but an Implicit declaration is only a fallback after
			// direct `Target(value)` construction. Otherwise System's
			// widening rows can intercept an already-valid cast through some
			// equal-domain source formal (for example PtrUInt -> QWord).
			//
			// Preserve the existing predefined explicit casts, which include
			// routine, pointer, ordinal, set, packed-overlay, and
			// class-reference operations.
			auto routine_reference = dynamic_cast<RoutineRef*>(value);
			auto target_routine = dynamic_cast<RoutineType*>(target_ty);
			if (target_ty == pointer_type() && routine_reference) {
				return resolve_routine_code_reference(routine_reference);
			} else if (dynamic_cast<NilLiteral*>(value) && target_ty->is_reference_type()) {
				// Nil has no source Type to query below. Give it the exact
				// requested reference type directly; this is the same
				// predefined null construction used by argument matching,
				// not a user-conversion edge.
				value->ty = target_ty;
				return value;
			} else if (target_routine) {
				if (routine_reference) {
					return resolve_routine_reference(routine_reference, target_routine);
				} else if (dynamic_cast<NilLiteral*>(value)) {
					return cast(value, target_routine);
				} else if (value->ty == tmethod_type()) {
					if (target_routine->kind != METHOD) {
						raise_type_kind_mismatch("TMethod cast target", "of-object routine", target_routine);
					}
					return new ExplicitCast(value, target_routine);
				} else if (auto source_routine = dynamic_cast<RoutineType*>(value->ty)) {
					if (!target_routine->accepts_explicit_routine_cast_from(source_routine)) {
						raise_type_mismatch("explicit cast between "
						                    "incompatible routine types",
						                    target_routine, source_routine);
					}
					// Unlike assignment and contextual @Routine resolution,
					// explicit syntax may retain the routine representation
					// while retyping by-value data-pointer parameters. The
					// RoutineType predicate owns that narrow semantic rule;
					// emission performs the documented ABI reinterpretation.
					return new ExplicitCast(value, target_routine);
				} else {
					raise_type_mismatch("explicit routine-type cast", target_routine, value->ty);
				}
			} else if (target_ty == tmethod_type()) {
				if (value->ty == target_ty) {
					return new ExplicitCast(value, target_ty);
				} else {
					auto source_routine = dynamic_cast<RoutineType*>(value->ty);
					if (!source_routine || source_routine->kind != METHOD) {
						raise_type_kind_mismatch("TMethod view source", "of-object routine", value->ty);
					}
					return new ExplicitCast(value, target_ty);
				}
			} else if (target_ty->predefined_explicit_conversion_from(value->ty)) {
				return new ExplicitCast(value, target_ty);
			} else if (Node* converted = match_explicit_conversion(value, target_ty, true)) {
				return converted;
			}
			raise_type_mismatch("invalid explicit conversion", target_ty, value->ty);
			abort();
		}
	}
	Type* visible_type = binding ? (std::get_if<Type*>(&*binding) ? std::get<Type*>(*binding) : nullptr) : nullptr;
	if (auto class_type = dynamic_cast<ClassType*>(visible_type)) {
		// Pascal keeps type and value lookup distinct. In value context the
		// class name denotes the exact class-reference value; it is not the
		// ClassType object reused as a fake expression. The parenthesized
		// type-cast case above must win for `TFoo(value)`.
		return new ClassRefValue(class_type);
	}
	raise_parse_error("unresolved value identifier: " + id);
	return nullptr;
}

// `inherited Name[(args)]` or anonymous `inherited;`. Calls the parent
// type's method. The parser resolves the target at parse time by walking
// current_routine's owner_class parent chain. The enclosing routine must be
// a Method on a composite type with a parent (else: parse error). For
// destructor-from-destructor the call is redundant in C++ (destructors
// auto-chain); we mark the node `dropped` and emit produces nothing.
// Statement and expression context both flow through here.
Node* Parser::parse_inherited() {
	consume(); // `inherited`
	if (current_routine && (current_routine->ty->kind == CLASS_CONSTRUCTOR || current_routine->ty->kind == CLASS_DESTRUCTOR)) {
		raise_parse_error("inherited is not supported in a class lifecycle hook");
	}

	std::string name;
	auto opt = maybe_parse_identifier();
	if (opt) {
		name = *opt;
	} else if (current_routine) {
		name = current_routine->pas_name;
	} else {
		raise_parse_error("inherited requires an enclosing method");
	}

	if (!current_routine) {
		raise_parse_error("inherited requires an enclosing method");
	}
	auto cur_method = dynamic_cast<Method*>(current_routine);
	if (!cur_method || !cur_method->owner_class) {
		raise_parse_error("inherited requires an enclosing method on a class/object");
	}
	Type* parent = parent_of(cur_method->owner_class);
	if (!parent) {
		raise_parse_error("inherited: enclosing type has no parent");
	}

	// lookup returns Node* (Callable* OR OverloadSet*). For the parens form,
	// finalize_call ranks overload sets by argument cost -- same path direct
	// calls take. For the no-parens form, we require an unambiguous single
	// candidate.
	Node* hit = lookup_method_in_ancestors(name, parent);
	if (!hit) {
		raise_parse_error("inherited: '" + name + "' not found in parent chain");
	}

	std::vector<Node*> args;
	if (maybe_parse_opening_paren()) {
		if (input_token != ")") {
			args.push_back(parse_expression());
			while (maybe_parse_comma()) {
				args.push_back(parse_expression());
			}
		}
		parse_closing_paren();
	} else {
		// No-parens form: must be a single Callable, not a multi-member
		// overload set.
		Callable* resolved = dynamic_cast<Callable*>(hit);
		if (!resolved) {
			if (auto os = dynamic_cast<OverloadSet*>(hit)) {
				if (os->members.size() == 1) {
					resolved = os->members.front();
				} else {
					raise_parse_error("inherited: '" + name + "' is overloaded; supply an argument list to disambiguate");
				}
			}
		}
		if (!resolved) {
			raise_parse_error("inherited: '" + name + "' did not resolve to a method");
		}
		hit = resolved;
	}

	// Ordinary member-call resolution requires a receiver. `inherited`
	// carries the enclosing method's implicit Self even though InheritedCall
	// later emits a qualified Parent::Method(args) expression and therefore
	// does not retain that receiver in its CST node.
	Node* receiver = nullptr;
	if (cur_method->is_static) {
		if (auto owner = dynamic_cast<ClassType*>(cur_method->owner_class)) {
			receiver = new ClassRefValue(owner);
		} else {
			receiver = new TypeMemberQualifier(cur_method->owner_class);
		}
	} else {
		receiver = resolve_value("self");
	}
	Node* call_target = bind_lookup_result(receiver, hit);
	auto fc = finalize_call(call_target, args, name, current_location());
	// fc.receiver stays unused -- InheritedCall uses qualified-id syntax
	// (Parent::X(args)), not member-access.
	auto resolved = dynamic_cast<Callable*>(fc.callee);
	if (!resolved) {
		raise_parse_error("inherited: overload resolution failed");
	}

	auto n = new InheritedCall();
	n->resolved = resolved;
	n->args = std::move(args);
	n->ty = call_result_type(resolved);
	if (current_routine->ty->kind == DESTRUCTOR && resolved->ty->kind == DESTRUCTOR) {
		auto current_method = dynamic_cast<Method*>(current_routine);
		auto resolved_method = dynamic_cast<Method*>(resolved);
		// Class destructors currently use C++ destructor auto-chaining.
		// Old-style object destructors deliberately do not: they are ordinary
		// Pascal methods, and `inherited Done` is the only thing that invokes
		// the ancestor body.
		n->dropped = current_method && resolved_method && dynamic_cast<ClassType*>(current_method->owner_class) && dynamic_cast<ClassType*>(resolved_method->owner_class);
	}
	return n;
}

bool Parser::maybe_parse_at() {
	if (input_token == "@") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_period() {
	if (input_token == ".") {
		consume();
		return true;
	} else {
		return false;
	}
}

void Parser::parse_period() {
	if (!maybe_parse_period()) {
		raise_parse_error("missing period");
	}
}

bool Parser::maybe_parse_period_period() {
	if (input_token == "..") {
		consume();
		return true;
	} else {
		return false;
	}
}

void Parser::parse_period_period() {
	if (!maybe_parse_period_period()) {
		raise_parse_error("missing period period");
	}
}

bool Parser::maybe_parse_less_less() {
	if (input_token == "<<") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_greater_greater() {
	if (input_token == ">>") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_circumflex() {
	if (input_token == "^") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_equal() {
	if (input_token == "=") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_less_greater() {
	if (input_token == "<>") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_less() {
	if (input_token == "<") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_greater() {
	if (input_token == ">") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_less_equal() {
	if (input_token == "<=") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_greater_equal() {
	if (input_token == ">=") {
		consume();
		return true;
	} else {
		return false;
	}
}

// Result type of calling CALLEE. For a Callable, that's its RoutineType's
// return_type; for anything else (Builtin, opaque) we return nullptr so
// callers fall back to whatever they used before. This is the difference
// between "the type of the function value" (Callable::ty, a RoutineType)
// and "the type of what the call evaluates to" (the return type).
static Type* call_result_type(Node* callee) {
	if (auto c = dynamic_cast<Callable*>(callee)) {
		return static_cast<RoutineType*>(c->ty)->return_type;
	} else if (callee) {
		if (auto ty = dynamic_cast<RoutineType*>(callee->ty)) {
			return ty->return_type;
		}
	}
	return nullptr;
}

// Small helper: is NODE a bare callable reference (Callable, OverloadSet, or
// a MemberAccess whose member is either)? Used both for the auto-call check
// and to decide whether to peel a MemberAccess in finalize_call.
static bool node_is_bare_callable(Node* n) {
	if (!n) {
		return false;
	} else if (dynamic_cast<Callable*>(n) || dynamic_cast<OverloadSet*>(n)) {
		return true;
	} else if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b);
	}
	return false;
}

Node* Parser::maybe_auto_call(Node* n, LeadingTokenDirectives directives) {
	if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		if (!property->property->index_types.empty() && property->indexes.empty()) {
			raise_value_error("indexed property '" + property->property->pas_name + "' requires an index argument list", property->property);
		}
		if (!property->property->read_accessor) {
			raise_value_error("write-only property '" + property->property->pas_name + "' cannot be read", property->property);
		}
		return n;
	} else if (!node_is_bare_callable(n)) {
		return n;
	}
	// finalize_call handles the empty-args case: for a Callable it checks
	// that either no formals exist or all remaining formals have defaults;
	// for an OverloadSet it runs ranking and picks the parameterless winner.
	// A candidate that requires args will fail there with a clear error.
	std::vector<Node*> args;
	auto fc = finalize_call(n, args, /*name for error*/ "", current_location());
	return make_call(fc, std::move(args), directives);
}

static Frame* body_frame_of(Type* ty) {
	if (auto r = dynamic_cast<RecordType*>(ty)) {
		return r->children;
	} else if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
		return r->children;
	} else if (auto c = dynamic_cast<ClassType*>(ty)) {
		return c->children;
	} else if (auto c = dynamic_cast<ClassRefType*>(ty)) {
		return body_frame_of(c->target);
	} else if (auto c = dynamic_cast<InterfaceType*>(ty)) {
		return c->children;
	} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
		return o->children;
	}
	return nullptr;
}

static Frame* body_frame_of(Node* value) {
	if (auto unit = dynamic_cast<UnitRef*>(value)) {
		return unit->unit ? unit->unit->frame : nullptr;
	}
	return body_frame_of(value ? value->ty : nullptr);
}

Node* Parser::parse_designator(LeadingTokenDirectives* leading_directives) {
	LeadingTokenDirectives primary_directives = directive_state.leading_token_directives();
	Node* primary = parse_value(&primary_directives);
	Node* result = parse_designator_tail(primary, primary_directives);
	if (leading_directives) {
		*leading_directives = primary_directives;
	}
	return result;
}

Node* Parser::parse_member_selection(Node* base, LeadingTokenDirectives* leading_directives) {
	// A callable base is invoked before selecting a member from its result.
	base = maybe_auto_call(base, leading_directives ? *leading_directives : directive_state.leading_token_directives());
	const LeadingTokenDirectives member_directives = directive_state.leading_token_directives();
	std::string member_name = parse_identifier();
	if (leading_directives) {
		*leading_directives = member_directives;
	}
	if (!body_frame_of(base)) {
		raise_type_kind_mismatch("member access receiver", "class, record, object, interface, or unit", base ? base->ty : nullptr);
	}
	Node* member = maybe_bind_member(base, member_name);
	if (!member) {
		raise_value_error("no member '" + member_name + "'", base);
	}
	return member;
}

Type* Parser::parse_qualified_type_member(std::string lhs_name) {
	auto binding = maybe_resolve_type_or_value(lhs_name);
	if (!binding) {
		raise_type_parse_error("unresolved member qualifier: " + lhs_name);
	}
	Node* base = nullptr;
	Type* rejected_qualifier_type = nullptr;
	if (auto value = std::get_if<Node*>(&*binding)) {
		base = *value;
	} else {
		Type* qt = std::get<Type*>(*binding);
		rejected_qualifier_type = qt;
		if (auto ct = dynamic_cast<ClassType*>(qt)) {
			base = new ClassRefValue(ct);
		} else if (dynamic_cast<RecordType*>(qt) || dynamic_cast<PackedRecordType*>(qt)) {
			base = new TypeMemberQualifier(qt);
		}
	}
	if (!base && rejected_qualifier_type) {
		raise_type_kind_mismatch("member qualifier '" + lhs_name + "'", "class, record, or unit", rejected_qualifier_type);
	}
	if (!base) {
		raise_type_parse_error("unresolved member qualifier: " + lhs_name);
	}
	Frame* members = body_frame_of(base);
	if (!members) {
		raise_type_kind_mismatch("member qualifier '" + lhs_name + "'", "class, record, or unit", base ? base->ty : nullptr);
	}
	Type* result = nullptr;
	while (true) {
		std::string member = parse_identifier();
		auto member_binding = members->lookup_type_or_value(member);
		if (!member_binding) {
			raise_type_parse_error("no type '" + member + "' visible after '" + lhs_name + "'");
		}
		if (auto t = std::get_if<Type*>(&*member_binding)) {
			result = *t;
		} else {
			Node* node = std::get<Node*>(*member_binding);
			result = node ? node->ty : nullptr;
		}
		if (!result) {
			raise_type_parse_error("no type '" + member + "' visible after '" + lhs_name + "'");
		}
		if (!maybe_parse_period()) {
			break;
		}
		members = body_frame_of(result);
		if (!members) {
			raise_type_kind_mismatch("member qualifier '" + member + "'", "class, record, or unit", result);
		}
	}
	return result;
}

Node* Parser::maybe_bind_member(Node* receiver, const std::string& name) {
	if (!receiver) {
		return nullptr;
	}
	Frame* members = body_frame_of(receiver);
	if (!members) {
		return nullptr;
	}
	Node* member = members->lookup_value(name);
	return member ? bind_lookup_result(receiver, member) : nullptr;
}

static bool custom_enumerator_value_type(Type* ty) {
	return dynamic_cast<RecordType*>(ty) || dynamic_cast<PackedRecordType*>(ty) || dynamic_cast<ObjectType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<InterfaceType*>(ty);
}

static void collect_destructor_methods(Node* binding, std::vector<Method*>* out) {
	auto collect = [out](Callable* callable) {
		auto method = dynamic_cast<Method*>(callable);
		if (method && method->ty->kind == DESTRUCTOR) {
			out->push_back(method);
		}
	};
	if (auto callable = dynamic_cast<Callable*>(binding)) {
		collect(callable);
	} else if (auto overloads = dynamic_cast<OverloadSet*>(binding)) {
		for (Callable* callable : overloads->members) {
			collect(callable);
		}
	}
}

static std::vector<Method*> nearest_object_destructors(ObjectType* object) {
	for (ObjectType* current = object; current; current = current->super) {
		std::vector<Method*> found;
		if (current->children) {
			for (const auto& declaration : current->children->value_declarations()) {
				collect_destructor_methods(declaration.second.value, &found);
			}
		}
		// Destruction is a single-dispatch operation. A destructor declared
		// by the nearest object replaces inherited destructor selection just
		// as an ordinary visible member does.
		if (!found.empty()) {
			return found;
		}
	}
	return {};
}

std::optional<Parser::CustomForInResolution> Parser::maybe_resolve_custom_for_in(Node* collection, Node* control) {
	if (!collection || !custom_enumerator_value_type(collection->ty)) {
		return std::nullopt;
	}
	Node* get_member = maybe_bind_member(collection, "getenumerator");
	if (!get_member) {
		return std::nullopt;
	}

	auto parameterless_call = [this](Node* target, const std::string& name, bool allow_destructor) {
		std::vector<Node*> arguments;
		FinalizedCall finalized = finalize_call(target, arguments, name, current_location());
		auto method = dynamic_cast<Method*>(finalized.callee);
		if (!method || method->ty->kind == CONSTRUCTOR || (!allow_destructor && method->ty->kind == DESTRUCTOR) || method->ty->kind == CLASS_CONSTRUCTOR || method->ty->kind == CLASS_DESTRUCTOR) {
			raise_type_kind_mismatch("for-in protocol member '" + name + "'", "function or procedure", finalized.callee ? finalized.callee->ty : nullptr);
		}
		return make_call(finalized, std::move(arguments), directive_state.leading_token_directives());
	};

	Node* get_call = parameterless_call(get_member, "GetEnumerator", false);
	Type* enumerator_type = get_call ? get_call->ty : nullptr;
	if (!custom_enumerator_value_type(enumerator_type)) {
		raise_type_kind_mismatch("GetEnumerator result must be a record, object, class, or interface", "record, object, class, or interface", enumerator_type);
	}

	// This is a semantic reference to the one emitted local, not a source
	// declaration installed in any Frame. Binding protocol members to it lets
	// the ordinary member/call/property machinery retain the exact returned
	// enumerator Type without exposing a compiler name to Pascal lookup.
	auto enumerator = new StorageSlot("tpcc_for_enumerator", enumerator_type);

	Node* move_member = maybe_bind_member(enumerator, "movenext");
	if (!move_member) {
		raise_type_error("for-in enumerator has no MoveNext member", enumerator_type);
	}
	Node* move_call = parameterless_call(move_member, "MoveNext", false);
	if (move_call->ty != boolean_type()) {
		raise_type_mismatch("for-in MoveNext result", boolean_type(), move_call->ty);
	}

	Node* current = maybe_bind_member(enumerator, "current");
	auto current_property = dynamic_cast<PropertyAccess*>(current);
	if (!current_property) {
		if (current) {
			raise_value_error("for-in enumerator Current must be a readable property", current);
		}
		raise_type_error("for-in enumerator has no Current member", enumerator_type);
	}
	// maybe_auto_call performs the existing indexed/write-only property
	// validation; PropertyAccess emission later selects its already-resolved
	// field or getter without repeating member lookup.
	current = maybe_auto_call(current, directive_state.leading_token_directives());
	Node* current_assignment = mk_assign(control, current);

	Node* cleanup = nullptr;
	bool nullable = dynamic_cast<ClassType*>(enumerator_type) != nullptr;
	if (nullable) {
		Node* free_member = maybe_bind_member(enumerator, "free");
		if (!free_member) {
			raise_type_error("class enumerator has no Free member", enumerator_type);
		}
		cleanup = parameterless_call(free_member, "Free", false);
		if (cleanup->ty != &unit_type()) {
			raise_type_mismatch("class enumerator Free result", &unit_type(), cleanup->ty);
		}
	} else if (auto object = dynamic_cast<ObjectType*>(enumerator_type)) {
		std::vector<Method*> destructors = nearest_object_destructors(object);
		if (destructors.size() > 1) {
			raise_type_error("old-object enumerator has multiple destructors in "
			                 "its nearest declaring type; cleanup is ambiguous",
			                 enumerator_type);
		}
		if (!destructors.empty()) {
			Node* destructor = bind_lookup_result(enumerator, destructors.front());
			cleanup = parameterless_call(destructor, destructors.front()->pas_name, true);
			if (cleanup->ty != &unit_type()) {
				raise_type_mismatch("old-object enumerator destructor result", &unit_type(), cleanup->ty);
			}
		}
	}

	return CustomForInResolution{get_call, move_call, current_assignment, cleanup, nullable};
}

Node* Parser::parse_designator_tail(Node* result, LeadingTokenDirectives& leading_directives) {
	while (true) {
		if (maybe_parse_period()) {
			result = parse_member_selection(result, &leading_directives);
		} else if (input_token == "(") {
			// The callable designator is the leading subtree of this call.
			// Directives in its argument list belong to those argument
			// subtrees and cannot change this saved call-site policy.
			const LeadingTokenDirectives call_directives = leading_directives;
			SourceLocation call_location = current_location();
			parse_opening_paren();
			// Bracketed n-ary: RHS is a comma-separated list of expressions.
			// No auto-call before `(` -- this `(` IS the call.
			std::vector<Node*> args;
			if (input_token != ")") {
				args.push_back(parse_expression());
				while (maybe_parse_comma()) {
					args.push_back(parse_expression());
				}
			}
			parse_closing_paren();
			auto fc = finalize_call(result, args, /*name_for_error*/ "", call_location);
			result = make_call(fc, std::move(args), call_directives);
			// If the result itself is subsequently invoked, that postfix
			// call begins at the next token rather than at the completed
			// inner call's original designator.
			leading_directives = directive_state.leading_token_directives();
			continue;
		} else if (maybe_parse_opening_bracket()) {
			// Parse the complete bracket argument list before resolving it.
			// User array properties receive the list as one application;
			// multidimensional native arrays apply their synthesized
			// one-argument property once per nested array dimension.
			auto pending_property = dynamic_cast<PropertyAccess*>(result);
			if (!(pending_property && !pending_property->property->index_types.empty() && pending_property->indexes.empty())) {
				result = maybe_auto_call(result, leading_directives);
			}
			std::vector<Node*> indexes;
			indexes.push_back(parse_expression());
			while (maybe_parse_comma()) {
				indexes.push_back(parse_expression());
			}
			parse_closing_bracket();
			// A later postfix call begins after this completed index
			// expression; directives inside the indexes therefore may affect
			// that later call, but never the callable which preceded them.
			leading_directives = directive_state.leading_token_directives();

			if (auto pending = dynamic_cast<PropertyAccess*>(result); pending && !pending->property->index_types.empty() && pending->indexes.empty()) {
				result = apply_property(pending->receiver, pending->property, std::move(indexes));
				continue;
			}

			if (dynamic_cast<FixedArrayType*>(result->ty) && indexes.size() > 1) {
				for (Node* index : indexes) {
					Property* property = default_property_for_type(result->ty);
					if (!property) {
						raise_type_error("index on type without a default property", result->ty);
					}
					result = apply_property(result, property, {index});
				}
				continue;
			}

			Property* property = default_property_for_type(result->ty);
			if (!property) {
				raise_type_error("index on type without a default property", result->ty);
			}
			result = apply_property(result, property, std::move(indexes));
		} else if (maybe_parse_circumflex()) {
			// Postfix: no RHS. Auto-call bare callable LHS first (deref of a
			// callable reference is nonsense).
			result = maybe_auto_call(result, leading_directives);
			Type* ct = result->ty;
			auto p = dynamic_cast<PointerType*>(ct);
			if (!p) {
				raise_type_kind_mismatch("dereference operand", "pointer", ct);
			}
			auto d = new Dereference(result);
			// Untyped Pointer^ is a Pascal place, not a readable value of
			// some fabricated element type. unknown_type() lets only
			// place-aware consumers such as an omitted-type var formal use
			// it; emission must never attempt C++ unary `*` on void*.
			d->ty = p->is_untyped() ? unknown_type() : p->item_type;
			result = d;
			leading_directives = directive_state.leading_token_directives();
		} else {
			break;
		}
	}
	return result;
}

static bool is_builtin_index_accessor(Builtin* builtin) {
	return builtin && builtin->desc && (builtin->desc->cxx_name == "::u_system::p_index" || builtin->desc->cxx_name == "::u_system::m_unchecked_index");
}

static bool is_typed_pointer_index(PropertyAccess* access, Builtin* builtin) {
	if (!access || !access->receiver || !is_builtin_index_accessor(builtin)) {
		return false;
	}
	auto pointer = dynamic_cast<PointerType*>(access->receiver->ty);
	return pointer && !pointer->is_untyped();
}

bool Parser::is_assignable(Node* n) {
	if (!n) {
		return false;
	}
	if (dynamic_cast<StorageSlot*>(n)) {
		return true;
	} else if (auto dereference = dynamic_cast<Dereference*>(n)) {
		return dereference->ty != unknown_type();
	} else if (dynamic_cast<Index*>(n)) {
		return true;
	} else if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		if (!property->property || !property->property->write_accessor) {
			return false;
		}
		Node* accessor = property->property->write_accessor;
		if (auto builtin = dynamic_cast<Builtin*>(accessor)) {
			// A typed pointer value need not itself occupy storage for its
			// indexed element to do so: PType(expr)[i] denotes *(expr + i).
			if (is_typed_pointer_index(property, builtin)) {
				return true;
			}
			// Reference-backed indexing can write only through a stable base.
			// Packed projections are admitted here solely so the subsequent
			// is_supported_packed_assignment check can select or reject their
			// explicit synchronous copyback lowering.
			return contains_packed_projection(property->receiver) || is_referenceable(property->receiver);
		} else if (dynamic_cast<StorageSlot*>(accessor) || dynamic_cast<Callable*>(accessor)) {
			return (property->receiver->ty && property->receiver->ty->is_reference_type()) || is_referenceable(property->receiver);
		}
		return false;
	} else if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (auto view = dynamic_cast<Cast*>(ma->a)) {
			auto field = dynamic_cast<StorageSlot*>(ma->b);
			if (view->ty == tmethod_type() && (field == tmethod_code_field() || field == tmethod_data_field())) {
				auto routine = dynamic_cast<RoutineType*>(view->a ? view->a->ty : nullptr);
				return routine && routine->kind == METHOD && is_assignable(view->a);
			}
		}
		return dynamic_cast<StorageSlot*>(ma->b) != nullptr;
	} else if (auto cast = dynamic_cast<Cast*>(n)) {
		// FPC treats an explicit ordinal cast as a view of its operand's
		// storage when both ordinal carriers have the same size. Restrict this
		// to tpcc intrinsic ordinal carriers: C++ enum/Boolean objects cannot
		// safely hold every bit pattern that FPC permits through such a view.
		auto intrinsic_ordinal_carrier = [](Type* ty) -> IntrinsicType* {
			while (auto subrange = dynamic_cast<SubrangeType*>(ty)) {
				ty = subrange->base_type;
			}
			auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
			return intrinsic && intrinsic->ordinal_bounds ? intrinsic : nullptr;
		};
		if (!intrinsic_ordinal_carrier(cast->a ? cast->a->ty : nullptr) || !intrinsic_ordinal_carrier(cast->ty)) {
			return false;
		}
		auto source_layout = type_layout(false, cast->a->ty);
		auto target_layout = type_layout(false, cast->ty);
		return source_layout && target_layout && source_layout->size == target_layout->size && is_referenceable(cast->a) && !contains_packed_projection(cast->a);
	}
	return false;
}

bool Parser::property_read_is_place(PropertyAccess* access) {
	if (!access || !access->property || !access->property->read_accessor) {
		return false;
	}
	Node* accessor = access->property->read_accessor;
	if (dynamic_cast<StorageSlot*>(accessor)) {
		return access->receiver->ty && access->receiver->ty->is_reference_type() ? true : is_referenceable(access->receiver);
	} else if (auto builtin = dynamic_cast<Builtin*>(accessor)) {
		return is_builtin_index_accessor(builtin) && (is_typed_pointer_index(access, builtin) || is_referenceable(access->receiver));
	}
	return false; // ordinary Pascal getter calls return values
}

bool Parser::is_referenceable(Node* n) {
	if (!n) {
		return false;
	}
	if (dynamic_cast<StorageSlot*>(n) || dynamic_cast<Dereference*>(n)) {
		return true;
	} else if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		return property_read_is_place(property);
	} else if (auto index = dynamic_cast<Index*>(n)) {
		return is_referenceable(index->a);
	} else if (auto member = dynamic_cast<MemberAccess*>(n)) {
		if (!dynamic_cast<StorageSlot*>(member->b)) {
			return false;
		}
		if (dynamic_cast<UnitRef*>(member->a)) {
			return true;
		}
		if (member->a->ty && member->a->ty->is_reference_type()) {
			return true;
		}
		return is_referenceable(member->a);
	}
	return false;
}

bool Parser::contains_packed_projection(Node* n) {
	if (!n) {
		return false;
	}
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (ma->a && dynamic_cast<PackedRecordType*>(ma->a->ty)) {
			return true;
		}
		return contains_packed_projection(ma->a);
	} else if (auto ix = dynamic_cast<Index*>(n)) {
		return contains_packed_projection(ix->a);
	} else if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		return contains_packed_projection(property->receiver);
	} else if (dynamic_cast<Dereference*>(n)) {
		return false;
	} else if (auto ca = dynamic_cast<Cast*>(n)) {
		return contains_packed_projection(ca->a);
	}
	return false;
}

bool Parser::is_supported_packed_assignment(Node* n) {
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (!ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty)) {
			return false;
		}
		if (auto overlay = dynamic_cast<Cast*>(ma->a)) {
			return is_assignable(overlay->a) && !contains_packed_projection(overlay->a);
		}
		return is_assignable(ma->a) && !contains_packed_projection(ma->a);
	} else if (auto ix = dynamic_cast<Index*>(n)) {
		auto ma = dynamic_cast<MemberAccess*>(ix->a);
		if (!ma || !ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty)) {
			return false;
		}
		if (auto overlay = dynamic_cast<Cast*>(ma->a)) {
			return is_assignable(overlay->a) && !contains_packed_projection(overlay->a);
		}
		return is_assignable(ma->a) && !contains_packed_projection(ma->a);
	} else if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		auto ma = dynamic_cast<MemberAccess*>(property->receiver);
		if (!ma || !ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty)) {
			return false;
		}
		if (auto overlay = dynamic_cast<Cast*>(ma->a)) {
			return is_assignable(overlay->a) && !contains_packed_projection(overlay->a);
		}
		return is_assignable(ma->a) && !contains_packed_projection(ma->a);
	}
	return false;
}

void Parser::validate_writable_destination(Node* target, SourceLocation error_location, std::string not_assignable_message) {
	if (!is_assignable(target)) {
		raise_value_error_at(error_location, std::move(not_assignable_message), target);
	}

	// A packed overlay is writable only when its source is a real assignable
	// place. In particular, never accept `TPacked(F()).X` and then silently
	// mutate a copied C++ temporary. Both plain assignment and mutation must
	// enter the same synchronous copyback paths admitted here.
	Node* overlay_source = nullptr;
	MemberAccess* overlay_member = dynamic_cast<MemberAccess*>(target);
	if (auto index = dynamic_cast<Index*>(target)) {
		overlay_member = dynamic_cast<MemberAccess*>(index->a);
	}
	if (auto property = dynamic_cast<PropertyAccess*>(target)) {
		overlay_member = dynamic_cast<MemberAccess*>(property->receiver);
	}
	if (overlay_member) {
		if (auto overlay = dynamic_cast<Cast*>(overlay_member->a)) {
			if (dynamic_cast<PackedRecordType*>(overlay->ty)) {
				overlay_source = overlay->a;
			}
		}
	}
	if (overlay_source && !is_assignable(overlay_source)) {
		raise_value_error_at(error_location, "writable packed-record overlay requires an assignable source", overlay_source);
	}
	if (contains_packed_projection(target) && !is_supported_packed_assignment(target)) {
		raise_value_error_at(error_location, "write through a nested or indexed packed-record field is not implemented", target);
	}
}

Mutation* Parser::parse_mutation_statement(std::string spelling, SourceLocation call_location, LeadingTokenDirectives directives) {
	// directives belongs to the Inc/Dec identifier already consumed by
	// the caller. Parsing the destination or distance may change scanner
	// directives for those child expressions, but never this mutation node.
	const bool increment = spelling == "inc";
	assert(increment || spelling == "dec");
	parse_opening_paren();
	if (input_token == ")") {
		emit_parse_error_at(call_location, spelling + " requires a destination");
	}
	Node* source_target = parse_expression();
	Node* amount = nullptr;
	if (maybe_parse_comma()) {
		amount = parse_expression();
	}
	parse_closing_paren();

	validate_writable_destination(source_target, call_location, spelling + " destination is not assignable");

	std::vector<Mutation::Binding> bindings;
	auto bind_once = [&bindings](Node* initializer) {
		auto alias = new StorageSlot("tpcc_mutation_place_" + std::to_string(bindings.size()), initializer ? initializer->ty : nullptr);
		bindings.push_back({alias, initializer});
		return alias;
	};

	// Rebuild the destination from stable aliases rather than snapshotting the
	// destination value itself. Properties and packed fields are get/set
	// projections, not C++ lvalues; aliasing only their receiver/index/source
	// lets the existing read and assignment paths run once each while every
	// source designator component is still evaluated exactly once.
	std::function<Node*(Node*)> stabilize = [&](Node* target) -> Node* {
		if (dynamic_cast<StorageSlot*>(target) || dynamic_cast<UnitRef*>(target)) {
			return target;
		} else if (auto dereference = dynamic_cast<Dereference*>(target)) {
			auto result = new Dereference(bind_once(dereference->a));
			result->ty = dereference->ty;
			return result;
		} else if (auto index = dynamic_cast<Index*>(target)) {
			Node* base = contains_packed_projection(index->a) ? stabilize(index->a) : static_cast<Node*>(bind_once(index->a));
			auto result = new Index(base, bind_once(index->b));
			result->ty = index->ty;
			return result;
		} else if (auto property = dynamic_cast<PropertyAccess*>(target)) {
			Node* receiver = contains_packed_projection(property->receiver) ? stabilize(property->receiver) : static_cast<Node*>(bind_once(property->receiver));
			std::vector<Node*> indexes;
			indexes.reserve(property->indexes.size());
			for (Node* index : property->indexes) {
				indexes.push_back(bind_once(index));
			}
			return new PropertyAccess(receiver, property->property, std::move(indexes));
		} else if (auto member = dynamic_cast<MemberAccess*>(target)) {
			if (dynamic_cast<UnitRef*>(member->a)) {
				return member;
			}
			Node* receiver = nullptr;
			auto method_view = dynamic_cast<Cast*>(member->a);
			auto method_field = dynamic_cast<StorageSlot*>(member->b);
			if (method_view && method_view->ty == tmethod_type() && (method_field == tmethod_code_field() || method_field == tmethod_data_field())) {
				// TMethod fields are writable views of a method-routine
				// value. Preserve that Cast shape so the existing
				// assignment emitter stores the selected word back into
				// the original method value instead of mutating a copied
				// public TMethod snapshot.
				receiver = new Cast(bind_once(method_view->a), method_view->ty);
			} else if (dynamic_cast<PackedRecordType*>(member->a->ty)) {
				if (auto overlay = dynamic_cast<Cast*>(member->a)) {
					receiver = new Cast(bind_once(overlay->a), overlay->ty);
				} else {
					receiver = bind_once(member->a);
				}
			} else {
				receiver = bind_once(member->a);
			}
			auto result = new MemberAccess(receiver, member->b);
			result->ty = member->ty;
			return result;
		} else if (auto view = dynamic_cast<Cast*>(target)) {
			return new Cast(bind_once(view->a), view->ty);
		}
		raise_value_error_at(call_location, "internal error: assignable mutation destination has no stabilization rule", target);
	};

	Node* target = stabilize(source_target);
	auto current = new StorageSlot("tpcc_mutation_value", source_target->ty);
	Node* operation = nullptr;
	if (amount) {
		// Delphi exposes only a unary Inc/Dec custom operator. The distance
		// form is therefore the ordinary Add/Subtract operation followed by
		// the same store; it must not silently call unary Inc/Dec repeatedly.
		operation = mk_arith(increment ? "+" : "-", current, amount, directives, true);
	} else {
		auto catalog_identifier = operator_invocation_identifier(OperatorInvocation::MutatingUnary, spelling, 1, directives.overflow_checks, false);
		assert(catalog_identifier);
		const std::string identifier(*catalog_identifier);
		Node* family = resolve_value(identifier);
		std::vector<Node*> arguments{current};
		FinalizedCall finalized = finalize_call(family, arguments, spelling, call_location);
		operation = make_call(finalized, std::move(arguments), directives);
	}

	if (operation && operation->ty == unknown_type()) {
		raise_value_error_at(call_location, "internal error: mutation operator has an unknown result type", operation);
	}
	// Inc/Dec has already selected both its arithmetic operator and writable
	// destination. Byte arithmetic, for example, produces Integer and stores
	// it back into Byte; that storage conversion must not become an
	// Integer -> Byte edge for unrelated overloads.
	Node* stored = cast_for_destination(operation, source_target->ty);
	if (directive_state.switch_enabled('r') && operation->ty == source_target->ty && (dynamic_cast<SubrangeType*>(source_target->ty) || dynamic_cast<EnumType*>(source_target->ty)) && !dynamic_cast<RangeCheckedCast*>(stored)) {
		// The operator produces a value before the mutation stores it. Even
		// an exact enum/subrange carrier can contain a value outside the
		// destination's declared Pascal bounds (notably after an unchecked
		// operation), so caller {$R+} checks this assignment boundary.
		stored = new RangeCheckedCast(operation, source_target->ty);
	}
	auto assignment = new Assign(target, stored);
	return new Mutation(source_target, std::move(bindings), target, current, assignment);
}

static std::string operator_expression_identifier(OperatorInvocation invocation, const std::string& source_token, size_t arity, bool overflow_checks, bool logical_operands = false) {
	auto identifier = operator_invocation_identifier(invocation, source_token, arity, overflow_checks, logical_operands);
	if (!identifier) {
		fprintf(stderr,
		        "internal compiler error: operator catalog has no invocation "
		        "for '%s' with arity %zu\n",
		        source_token.c_str(), arity);
		abort();
	}
	return std::string(*identifier);
}

Node* Parser::mk_arith(std::string id, Node* a, Node* b, LeadingTokenDirectives directives, bool mutation_step) {
	const std::string pascal_identifier = operator_expression_identifier(OperatorInvocation::BinaryToken, id, 2, directives.overflow_checks, (a && a->ty == boolean_type()) || (b && b->ty == boolean_type()));
	auto fn = resolve_value(pascal_identifier);
	// Operators and named routines share one argument matcher. Preserve the
	// source operands until a declaration has been selected; choosing a
	// "common" type first changes which overload is exact and makes operator
	// calls obey a different language from ordinary calls.
	std::vector<Node*> args{a, b};
	OverloadResolutionPolicy policy = OverloadResolutionPolicy::Ordinary;
	if (id == "+" || id == "-") {
		policy = mutation_step ? OverloadResolutionPolicy::CommonBinaryPointerLeftOrEnumStep : OverloadResolutionPolicy::CommonBinaryPointerLeft;
	} else if (id == "div" || id == "mod" || id == "and" || id == "or" || id == "xor") {
		policy = OverloadResolutionPolicy::CommonIntegerBinary;
	} else if (id == "*" || id == "/" || id == "><") {
		policy = OverloadResolutionPolicy::CommonBinary;
	}
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location(), nullptr, policy);
	return make_call(fc, std::move(args), directives);
}

Node* Parser::mk_assign(Node* a, Node* b) {
	return new Assign(a, cast_for_destination(b, a->ty));
}

Node* Parser::mk_compare(std::string id, Node* a, Node* b, LeadingTokenDirectives directives) {
	RoutineType* a_routine = a ? dynamic_cast<RoutineType*>(a->ty) : nullptr;
	RoutineType* b_routine = b ? dynamic_cast<RoutineType*>(b->ty) : nullptr;
	if (!a_routine && dynamic_cast<NilLiteral*>(a) && b_routine) {
		a = cast(a, b_routine);
		a_routine = b_routine;
	}
	if (!b_routine && dynamic_cast<NilLiteral*>(b) && a_routine) {
		b = cast(b, a_routine);
		b_routine = a_routine;
	}
	if (a_routine || b_routine) {
		if (id != "=") {
			raise_type_error("routine values can only be compared for equality", a_routine ? static_cast<Type*>(a_routine) : static_cast<Type*>(b_routine));
		}
		if (!a_routine) {
			raise_type_mismatch("routine-value comparison left operand", b_routine, a ? a->ty : nullptr);
		}
		if (!b_routine) {
			raise_type_mismatch("routine-value comparison right operand", a_routine, b ? b->ty : nullptr);
		}
		if (!a_routine->accepts_routine_value_from(b_routine)) {
			raise_type_mismatch("routine-value comparison", a_routine, b_routine);
		}
		auto equal = new RoutineEqual(a, b);
		equal->ty = boolean_type();
		return equal;
	}

	const std::string pascal_identifier = operator_expression_identifier(OperatorInvocation::BinaryToken, id, 2, directives.overflow_checks, (a && a->ty == boolean_type()) || (b && b->ty == boolean_type()));
	auto fn = resolve_value(pascal_identifier);
	// Keep comparison operands in source order and source type. `nil` and
	// ordinary conversions are contextual matches against each candidate,
	// exactly as for a named call; the selected formal types are applied only
	// after overload resolution.
	std::vector<Node*> args{a, b};
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location(), nullptr, OverloadResolutionPolicy::CommonBinary);
	Node* call = make_call(fc, std::move(args), directives);
	/*	if (call->ty->return_type != boolean_type()) {
	                raise_type_mismatch("custom comparison operator '" + id + "' has wrong return type",
	   boolean_type(), call->ty); } FIXME */
	return call;
}

Node* Parser::mk_membership(Node* item, Node* set, LeadingTokenDirectives directives) {
	const std::string pascal_identifier = operator_expression_identifier(OperatorInvocation::BinaryToken, "in", 2, directives.overflow_checks);
	Node* fn = resolve_value(pascal_identifier);
	// Preserve both source operands until the ordinary operator family has
	// selected a declaration. A typed custom `operator In(T, TContainer)`
	// must see TContainer rather than being rejected or coerced by the
	// predefined set interpretation. System's otherwise-unspellable
	// `(T, set of T)` fallback is recovered candidate-locally by its
	// SetMembership BuiltinDesc in match_callable_arguments().
	std::vector<Node*> args{item, set};
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	return make_call(fc, std::move(args), directives);
}

Node* Parser::mk_unary_same(std::string id, Node* x, LeadingTokenDirectives directives) {
	if (auto integer = untyped_integer_constant(x)) {
		if (id == "-") {
			if (!integer->negative && integer->value > (uint64_t{1} << 63)) {
				raise_value_error("integer constant is below Int64 minimum", integer);
			}
			return new Integer(integer->value, &untyped_integer_type(), !integer->negative);
		}
		if (id == "+") {
			return new Integer(integer->value, &untyped_integer_type(), integer->negative);
		}
		if (id == "not") {
			// Unary `not` deliberately has Integer semantics (`not 1 = -2`).
			// Construct a contextual value rather than changing a literal or
			// constant declaration that another expression may also reference.
			x = new Integer(integer->value, integer_type(), integer->negative);
		}
	}
	if (auto real = dynamic_cast<Real*>(x); real && real->is_origin() && (id == "-" || id == "+")) {
		DecimalOrigin origin = *real->origin;
		if (id == "-") {
			origin.negative = !origin.negative;
		}
		return new Real(std::move(origin));
	}
	const std::string pascal_identifier = operator_expression_identifier(OperatorInvocation::UnaryToken, id, 1, directives.overflow_checks);
	auto fn = resolve_value(pascal_identifier);
	std::vector<Node*> args;
	args.push_back(x);
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	Node* call = make_call(fc, std::move(args), directives);
	/*	if (call->ty->return_type != x->ty) {
	                raise_type_mismatch("custom unary operator '" + id + "' has wrong return type", x->ty,
	   call->ty); } FIXME */
	return call;
}

Node* Parser::parse_power_tail(Node* result, LeadingTokenDirectives leading_directives) {
	result = maybe_auto_call(result, leading_directives);
	while (true) {
		const LeadingTokenDirectives operation_directives = directive_state.leading_token_directives();
		if (!maybe_parse_star_star()) {
			break;
		}
		result = mk_arith("**", result, parse_power(), operation_directives);
	}
	return result;
}

Node* Parser::parse_power() {
	const LeadingTokenDirectives operation_directives = directive_state.leading_token_directives();
	if (maybe_parse_keyword("not")) {
		return mk_unary_same("not", parse_power(), operation_directives);
	} else if (maybe_parse_at()) {
		// Do not let parse_power_tail turn a routine designator into an
		// implicit no-argument call. A routine address is contextual: its
		// destination type selects both the overload and plain-vs-of-object
		// representation.
		auto x = parse_designator();
		if (node_is_bare_callable(x)) {
			Node* receiver = nullptr;
			Node* candidates = x;
			if (auto member = dynamic_cast<MemberAccess*>(x)) {
				candidates = member->b;
				bool has_instance_method = dynamic_cast<Method*>(candidates);
				if (auto overloads = dynamic_cast<OverloadSet*>(candidates)) {
					for (Callable* candidate : overloads->members) {
						has_instance_method = has_instance_method || dynamic_cast<Method*>(candidate);
					}
				}
				if (has_instance_method) {
					receiver = member->a;
				}
			}
			return parse_power_tail(new RoutineRef(receiver, candidates), operation_directives);
		}
		if (contains_packed_projection(x)) {
			raise_value_error("address of a packed-record field is not available", x);
		}
		if (!is_referenceable(x)) {
			raise_value_error("address requires a storage-backed expression", x);
		}
		auto n = new AddrOf(x);
		n->ty = x->ty ? static_cast<Type*>(new PointerType(current_location(), x->ty)) : nullptr;
		return parse_power_tail(n, operation_directives);
	} else if (maybe_parse_minus()) {
		return mk_unary_same("-", parse_power(), operation_directives);
	} else if (maybe_parse_plus()) {
		return mk_unary_same("+", parse_power(), operation_directives);
	}

	// Mirror FPC's quirk. `-1 ** 4` parses as `-(1 ** 4)`, not `(-1) ** 4`.
	// FIXME: Fix it later.
	LeadingTokenDirectives designator_directives = operation_directives;
	Node* designator = parse_designator(&designator_directives);
	return parse_power_tail(designator, designator_directives);
}

Node* Parser::parse_product_tail(Node* result) {
	while (true) {
		const LeadingTokenDirectives operation_directives = directive_state.leading_token_directives();
		if (maybe_parse_star()) {
			result = mk_arith("*", result, parse_power(), operation_directives);
		} else if (maybe_parse_slash()) {
			result = mk_arith("/", result, parse_power(), operation_directives);
		} else if (maybe_parse_keyword("div")) {
			result = mk_arith("div", result, parse_power(), operation_directives);
		} else if (maybe_parse_keyword("mod")) {
			result = mk_arith("mod", result, parse_power(), operation_directives);
		} else if (maybe_parse_keyword("and") || maybe_parse_ampersand()) {
			auto b = parse_power();
			if (result->ty == boolean_type() && b->ty == boolean_type()) {
				auto n = new ShortCircuitOperation(AND, result, b);
				n->ty = boolean_type();
				result = n;
			} else {
				result = mk_arith("and", result, b, operation_directives);
			}
		} else if (maybe_parse_keyword("shl")) {
			result = mk_arith("shl", result, parse_power(), operation_directives);
		} else if (maybe_parse_keyword("shr")) {
			result = mk_arith("shr", result, parse_power(), operation_directives);
		} else if (maybe_parse_keyword("as")) {
			// FPC's `as` is a class/interface cast only (ObjFPC mode); it
			// rejects real operands with "Class or interface type expected".
			Type* target = parse_type_expression(false);
			bool checked_reference = (dynamic_cast<ClassType*>(result->ty) || dynamic_cast<InterfaceType*>(result->ty)) && (dynamic_cast<ClassType*>(target) || dynamic_cast<InterfaceType*>(target));
			if (!checked_reference) {
				raise_type_mismatch("'as' requires compatible real-number or class/interface types", target, result->ty);
			}
			result = new Coerce(result, target);
		} else if (maybe_parse_keyword("is")) {
			// FPC RELEASED BUG: `_OP_IS` sits in opmultiply in FPC 3.x,
			// making `is` bind tighter than `+` (a Delphi-compatibility bug,
			// fixed in FPC trunk). Match FPC 3.2.x behavior here for parity.
			Type* target = parse_type_expression(false);
			if (!(dynamic_cast<ClassType*>(result->ty) || dynamic_cast<InterfaceType*>(result->ty))) {
				raise_type_kind_mismatch("'is' left operand", "class or interface", result->ty);
			}
			if (!(dynamic_cast<ClassType*>(target) || dynamic_cast<InterfaceType*>(target))) {
				raise_type_kind_mismatch("'is' target", "class or interface", target);
			}
			auto n = new CoerceCheck(result, target);
			n->ty = boolean_type();
			result = n;
		} else if (maybe_parse_less_less()) {
			result = mk_arith("shl", result, parse_power(), operation_directives);
		} else if (maybe_parse_greater_greater()) {
			result = mk_arith("shr", result, parse_power(), operation_directives);
		} else if (maybe_parse_symdiff()) {
			result = mk_arith("><", result, parse_power(), operation_directives);
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_product() {
	return parse_product_tail(parse_power());
}

Node* Parser::parse_sum_tail(Node* result) {
	while (true) {
		const LeadingTokenDirectives operation_directives = directive_state.leading_token_directives();
		if (maybe_parse_plus()) {
			result = mk_arith("+", result, parse_product(), operation_directives);
		} else if (maybe_parse_minus()) {
			result = mk_arith("-", result, parse_product(), operation_directives);
		} else if (maybe_parse_keyword("or") || maybe_parse_pipe()) {
			auto b = parse_product();
			if (result->ty == boolean_type() && b->ty == boolean_type()) {
				auto n = new ShortCircuitOperation(OR, result, b);
				n->ty = boolean_type();
				result = n;
			} else {
				result = mk_arith("or", result, b, operation_directives);
			}
		} else if (maybe_parse_keyword("xor")) {
			auto b = parse_product();
			// FIXME: constant fold; check result type; if bool: emit LogicalOperation(XOR, ...) instead;
			result = mk_arith("xor", result, b, operation_directives);
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_sum() {
	return parse_sum_tail(parse_product());
}

Node* Parser::parse_subrange_bound_expression_after_identifier(std::string id, LeadingTokenDirectives identifier_directives) {
	LeadingTokenDirectives leading_directives = identifier_directives;
	Node* result = parse_value_from_identifier(std::move(id), identifier_directives, &leading_directives);
	result = parse_designator_tail(result, leading_directives);
	return parse_sum_tail(parse_product_tail(parse_power_tail(result, leading_directives)));
}

Node* Parser::parse_subrange_bound_expression() {
	return parse_sum();
}

Node* Parser::parse_comparison_tail(Node* result) {
	while (true) {
		const LeadingTokenDirectives operation_directives = directive_state.leading_token_directives();
		if (maybe_parse_equal()) {
			result = mk_compare("=", result, parse_sum(), operation_directives);
		} else if (maybe_parse_less_greater()) {
			// Pascal <> is inequality. Do not require or expose a separate custom
			// operator<> declaration; derive it from equality and boolean not so user
			// equality overloads participate consistently.
			Node* right = parse_sum();
			result = mk_unary_same("not", mk_compare("=", result, right, operation_directives), operation_directives);
		} else if (maybe_parse_less()) {
			result = mk_compare("<", result, parse_sum(), operation_directives);
		} else if (maybe_parse_greater()) {
			result = mk_compare(">", result, parse_sum(), operation_directives);
		} else if (maybe_parse_less_equal()) {
			result = mk_compare("<=", result, parse_sum(), operation_directives);
		} else if (maybe_parse_greater_equal()) {
			result = mk_compare(">=", result, parse_sum(), operation_directives);
		} else if (maybe_parse_keyword("in")) {
			result = mk_membership(result, parse_sum(), operation_directives);
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_comparison() {
	return parse_comparison_tail(parse_sum());
}

Node* Parser::parse_expression_after_identifier(std::string id, LeadingTokenDirectives identifier_directives) {
	LeadingTokenDirectives leading_directives = identifier_directives;
	Node* result = parse_value_from_identifier(std::move(id), identifier_directives, &leading_directives);
	result = parse_designator_tail(result, leading_directives);
	return parse_comparison_tail(parse_sum_tail(parse_product_tail(parse_power_tail(result, leading_directives))));
}

Node* Parser::parse_expression() {
	return parse_comparison();
}

FormattedValue Parser::parse_formatted_value() {
	FormattedValue result{
	    .value = parse_expression(),
	    .width = nullptr,
	    .precision = nullptr,
	};
	if (maybe_parse_colon()) {
		result.width = cast(parse_expression(), sizeint_type());
		if (maybe_parse_colon()) {
			result.precision = cast(parse_expression(), sizeint_type());
		}
	}
	return result;
}

Property* Parser::default_property_for_type(Type* ty) {
	if (!ty) {
		return nullptr;
	}
	if (ty->default_property) {
		return ty->default_property;
	} else if (Type* element = ty->sequence_element_type()) {
		// Every semantic sequence supplies its element and index types through
		// Type. The compiler-synthesized property is therefore the same ordinary
		// Property representation used by source declarations; C++ carrier
		// spellings never participate in index lookup.
		Node* read_accessor = create_builtin_value("::u_system::p_index");
		Node* write_accessor = ty == ansistring_type() ? create_builtin_value("::u_system::tpcc_index_write") : read_accessor;
		ty->default_property = new Property("items", element, {ty->sequence_index_type()}, read_accessor, write_accessor, true);
		return ty->default_property;
	} else if (auto pointer = dynamic_cast<PointerType*>(ty)) {
		if (pointer->is_untyped()) {
			return nullptr;
		}
		auto accessor = create_builtin_value("::u_system::p_index");
		pointer->default_property = new Property("items", pointer->item_type, {integer_type()}, accessor, accessor, true);
		return pointer->default_property;
	} else if (auto c = dynamic_cast<ClassType*>(ty)) {
		// FPC selects the nearest default property from the expression's static
		// type hierarchy. It does not dynamically dispatch property
		// declarations.
		return c->super ? default_property_for_type(c->super) : nullptr;
	} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
		return o->super ? default_property_for_type(o->super) : nullptr;
	} else if (auto i = dynamic_cast<InterfaceType*>(ty)) {
		for (auto* parent : i->super_interfaces) {
			if (auto* property = default_property_for_type(parent)) {
				return property;
			}
		}
	}
	return nullptr;
}

PropertyAccess* Parser::apply_property(Node* receiver, Property* property, std::vector<Node*> indexes) {
	if (!property) {
		raise_parse_error("internal error: missing property");
	}
	if (indexes.size() != property->index_types.size()) {
		std::ostringstream message;
		message << "property '" << property->pas_name << "' expects " << property->index_types.size() << " index argument(s), got " << indexes.size();
		raise_value_error(message.str(), property);
	}
	for (size_t i = 0; i < indexes.size(); ++i) {
		indexes[i] = cast_for_destination(indexes[i], property->index_types[i]);
	}
	if (!directive_state.switch_enabled('r') && receiver && !dynamic_cast<PointerType*>(receiver->ty)) {
		auto builtin = dynamic_cast<Builtin*>(property->read_accessor);
		if (builtin && builtin->desc && builtin->desc->cxx_name == "::u_system::p_index") {
			Node* unchecked = create_builtin_value("::u_system::m_unchecked_index");
			// Built-in indexing and source properties use the same
			// PropertyAccess representation. Select a different ordinary
			// accessor only for the synthesized built-in property; a user
			// getter keeps the definition-site behavior of its own body.
			property = new Property(property->pas_name, property->ty, property->index_types, unchecked, unchecked, property->is_default);
		}
	}
	return new PropertyAccess(receiver, property, std::move(indexes));
}

void Parser::validate_property_declaration(Property* property) {
	assert(property);
	const std::string& property_name = property->pas_name;
	const std::vector<Type*>& index_types = property->index_types;
	Type* property_type = property->ty;

	// This routine is deliberately separate from property parsing. A property
	// in a type block can mention an accessor whose signature still stores the
	// block's old IncompleteType placeholder while the property spelling,
	// parsed later, receives the resolved Type*. Arity and syntax can be
	// collected earlier, but raw Type* identity is meaningful only after the
	// complete block has been recursively normalized.
	auto validate_index_formals = [&](RoutineType* routine, size_t count, const char* which) {
		if (routine->formals.size() != count) {
			raise_type_error(std::string(which) + " accessor for property '" + property_name + "' has the wrong number of parameters", routine);
		}
		for (size_t i = 0; i < index_types.size(); ++i) {
			if (routine->formals[i].ty != index_types[i]) {
				raise_type_mismatch(std::string(which) + " property index parameter", index_types[i], routine->formals[i].ty);
			}
		}
	};
	if (property->read_accessor) {
		if (auto field = dynamic_cast<StorageSlot*>(property->read_accessor)) {
			if (!index_types.empty()) {
				raise_type_kind_mismatch("indexed property read accessor", "method", field->ty);
			}
			if (field->ty != property_type) {
				raise_type_mismatch("property read field", property_type, field->ty);
			}
		} else if (auto getter = dynamic_cast<Callable*>(property->read_accessor)) {
			auto routine = static_cast<RoutineType*>(getter->ty);
			validate_index_formals(routine, index_types.size(), "read");
			if (routine->return_type != property_type) {
				raise_type_mismatch("property getter return type", property_type, routine->return_type);
			}
		} else {
			raise_type_kind_mismatch("property read accessor", "field or method", property->read_accessor->ty);
		}
	}
	if (property->write_accessor) {
		if (auto field = dynamic_cast<StorageSlot*>(property->write_accessor)) {
			if (!index_types.empty()) {
				raise_type_kind_mismatch("indexed property write accessor", "method", field->ty);
			}
			if (field->ty != property_type) {
				raise_type_mismatch("property write field", property_type, field->ty);
			}
		} else if (auto setter = dynamic_cast<Callable*>(property->write_accessor)) {
			auto routine = static_cast<RoutineType*>(setter->ty);
			validate_index_formals(routine, index_types.size() + 1, "write");
			if (routine->return_type != &unit_type()) {
				raise_type_mismatch("property setter return type", &unit_type(), routine->return_type);
			}
			if (routine->formals.back().ty != property_type) {
				raise_type_mismatch("property setter value parameter", property_type, routine->formals.back().ty);
			}
			auto mode = routine->formals.back().mode;
			if (mode != ParamMode::Value && mode != ParamMode::Const) {
				raise_type_error("property setter value parameter must be a value or const parameter", routine);
			}
		} else {
			raise_type_kind_mismatch("property write accessor", "field or method", property->write_accessor->ty);
		}
	}
}

Node* Parser::parse_property_accessor_reference(Frame* body) {
	std::string name = parse_identifier();
	Node* result = body->lookup_value(name);
	if (maybe_parse_period()) {
		// Not allowing any expression here on purpose.
		do {
			std::string name = parse_identifier();
			body = body_frame_of(result);
			if (body == nullptr) {
				raise_parse_error("invalid property reference"); // FIXME
				return nullptr;
			}
			result = body->lookup_value(name);
		} while (maybe_parse_period());
		if (result && dynamic_cast<StorageSlot*>(result)) {
			return result;
		} else {
			raise_type_parse_error("expected storageslot");
			return nullptr;
		}
	} else {
		return result;
	}
}

void Parser::parse_property_declaration(Frame* body, Type* owner_type) {
	parse_keyword("property");
	std::string property_name = parse_identifier();
	std::vector<Type*> index_types;
	if (maybe_parse_opening_bracket()) {
		if (input_token == "]") {
			raise_parse_error("property index parameter list cannot be empty");
		}
		do {
			// FPC accepts normal value and const index parameters. Their mode is
			// checked against the accessor signature after the containing
			// aggregate's type graph has been normalized; property resolution
			// itself needs only the declared index types.
			maybe_parse_keyword("const");
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma()) {
				names.push_back(parse_identifier());
			}
			parse_colon();
			Type* index_type = parse_type_expression(false);
			for (size_t i = 0; i < names.size(); ++i) {
				index_types.push_back(index_type);
			}
		} while (maybe_parse_semicolon());
		parse_closing_bracket();
	}
	parse_colon();
	Type* property_type = parse_type_expression(false);

	Node* read_accessor = nullptr;
	Node* write_accessor = nullptr;
	while (input_token != ";") {
		if (maybe_parse_directive("read")) {
			read_accessor = parse_property_accessor_reference(body);
			if (!read_accessor) {
				raise_parse_error("unknown read accessor for property '" + property_name + "'");
			}
		} else if (maybe_parse_directive("write")) {
			write_accessor = parse_property_accessor_reference(body);
			if (!write_accessor) {
				raise_parse_error("unknown write accessor for property '" + property_name + "'");
			}
		} else {
			raise_parse_error("expected read or write accessor in property '" + property_name + "'");
		}
	}
	if (!read_accessor && !write_accessor) {
		raise_parse_error("property '" + property_name + "' has no accessor");
	}

	parse_semicolon();

	bool is_default = false;
	if (maybe_parse_directive("default")) {
		is_default = true;
		if (index_types.empty()) {
			raise_type_error("default property must have index parameters", owner_type);
		}
		if (owner_type->default_property) {
			raise_value_error("only one default property may be declared per type", owner_type->default_property);
		}
		parse_semicolon();
	}

	auto property = new Property(property_name, property_type, std::move(index_types), read_accessor, write_accessor, is_default);
	if (!body->register_variable(property_name, property, property_type)) {
		raise_parse_error("duplicate property '" + property_name + "'");
	}
	if (is_default) {
		owner_type->default_property = property;
	}
}

void Parser::validate_method_ancestor_semantics(Method* method, Frame* owner_body) {
	assert(method && method->ty && owner_body);
	if (method->ty->kind != METHOD && method->ty->kind != CLASS_METHOD && !method->is_static) {
		return;
	}

	const std::string& pas_name = method->pas_name;
	const bool old_object_method = dynamic_cast<ObjectType*>(method->owner_class) != nullptr;
	bool override_target_found = false;
	auto inspect_ancestor_binding = [&](Node* binding) {
		auto inspect = [&](Callable* callable) {
			auto ancestor = dynamic_cast<Method*>(callable);
			if (ancestor && (ancestor->virtual_kind != Method::VirtualKind::None || ancestor->is_final)) {
				bool exact_signature = method->ty->same_signature_as(ancestor->ty) && method->is_static == ancestor->is_static;
				auto raise_ancestor_error = [&](std::string message) {
					ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
					std::stringstream sst;
					sst << message << "\n  method: " << ctx.value_ref(method) << "\n  ancestor: " << ctx.value_ref(ancestor);
					emit_parse_error_at(callable_source_location(method), sst.str(), ctx);
				};
				if (exact_signature && !method->is_static) {
					override_target_found = true;
					if (ancestor->is_final) {
						raise_ancestor_error("method '" + pas_name + "' overrides a final method");
					}
				}
				if (cxx_callable_signatures_collide(method, ancestor)) {
					bool intended_override = exact_signature && ((old_object_method && method->virtual_kind != Method::VirtualKind::None) || (!old_object_method && method->virtual_kind == Method::VirtualKind::Override));
					if (!intended_override) {
						// C++ virtual overriding is based on the emitted name
						// and carrier signature even when Pascal selected a
						// distinct signature or requested a fresh virtual
						// slot. This check runs only after recursive type-block
						// normalization: otherwise an old self/forward
						// placeholder and its resolved Type* would manufacture
						// the very distinction diagnosed here. Old-style
						// objects are the exception to the spelling rule:
						// repeating `virtual` on the exact declaration is
						// their normal override syntax.
						raise_ancestor_error("method '" + pas_name +
						                     "' would accidentally override an "
						                     "ancestor after C++ carrier erasure");
					}
				}
			}
		};
		if (auto callable = dynamic_cast<Callable*>(binding)) {
			inspect(callable);
		} else if (auto overloads = dynamic_cast<OverloadSet*>(binding)) {
			for (Callable* callable : overloads->members) {
				inspect(callable);
			}
		}
	};

	// Override compatibility is independent of Pascal name hiding. A C++
	// virtual can be overridden through any depth of the base chain, so
	// inspect each ancestor's own declarations rather than performing one
	// structural name lookup that could stop at an intermediate class.
	for (Frame* ancestor = owner_body->parent; ancestor; ancestor = ancestor->parent) {
		for (const auto& declaration : ancestor->value_declarations()) {
			if (declaration.first == pas_name) {
				inspect_ancestor_binding(declaration.second.value);
			}
		}
	}
	if (method->virtual_kind == Method::VirtualKind::Override && !override_target_found) {
		raise_value_error_at(callable_source_location(method), "method '" + pas_name + "' has no exact virtual ancestor to override", method);
	}
}

void Parser::validate_aggregate_declaration_semantics(Frame* owner_body) {
	assert(owner_body);

	// A Frame is useful during aggregate parsing as a declaration collection
	// and lookup structure, but an open Pascal type block does not yet provide
	// canonical Type* identity. Establish all local callable-family invariants
	// here, after TypeBlockResolver has rewritten every stored edge and before
	// statements or C++ emission can consume the aggregate.
	for (const auto& declaration : owner_body->value_declarations()) {
		auto overloads = dynamic_cast<OverloadSet*>(declaration.second.value);
		if (!overloads) {
			continue;
		}
		for (std::size_t i = 1; i < overloads->members.size(); ++i) {
			Callable* incoming = overloads->members[i];
			std::vector<Callable*> prior_members(overloads->members.begin(), overloads->members.begin() + i);
			Node* prior_binding = prior_members.size() == 1 ? static_cast<Node*>(prior_members.front()) : static_cast<Node*>(new OverloadSet(std::move(prior_members)));
			for (std::size_t j = 0; j < i; ++j) {
				Callable* conflicting = overloads->members[j];
				auto kind = validate_callable_pair(conflicting, incoming);
				if (kind == CallableRegistration::Kind::Added) {
					continue;
				}
				raise_callable_registration_error(declaration.first, incoming, CallableRegistration{kind, prior_binding, conflicting});
			}
		}
	}

	for (const auto& declaration : owner_body->value_declarations()) {
		Node* binding = declaration.second.value;
		auto inspect = [&](Callable* callable) {
			if (auto method = dynamic_cast<Method*>(callable)) {
				validate_method_ancestor_semantics(method, owner_body);
			}
		};
		if (auto callable = dynamic_cast<Callable*>(binding)) {
			inspect(callable);
		} else if (auto overloads = dynamic_cast<OverloadSet*>(binding)) {
			for (Callable* callable : overloads->members) {
				inspect(callable);
			}
		}

		if (auto property = dynamic_cast<Property*>(binding)) {
			validate_property_declaration(property);
		}
	}
}

/** Parse the body of a class/record/object. When `owner_class` is non-null,
 *  procedure/function declarations inside are parsed as method prototypes
 *  and registered with owner_class as their owner. */
Frame* Parser::parse_aggregate_type_body(Type* owner_class) {
	bool is_class = false; // "class method" etc.
	// Member lookup on a class/object has to see inherited members. Keep that
	// as the Frame's structural parent relation instead of teaching every
	// caller (`Self.X`, `class of T`.X, unqualified method-body lookup, etc.)
	// to walk superclasses separately.
	Frame* body = make_aggregate_body_frame(owner_class);
	push_scope(body);
	push_declaration_frame(body);
	std::string visibility = "published";
	auto at_visibility_section = [&]() { return peek_directive("published") || peek_directive("public") || peek_directive("protected") || peek_directive("private") || peek_directive("strict"); };
	do {
		if (peek_keyword("end")) {
			break;
		}
		if (maybe_parse_directive("published")) {
			visibility = "published";
			continue;
		} else if (maybe_parse_directive("public")) {
			visibility = "public";
			continue;
		} else if (maybe_parse_directive("protected")) {
			visibility = "protected";
			continue;
		} else if (maybe_parse_directive("private")) {
			visibility = "private";
			continue;
		} else if (maybe_parse_directive("strict")) {
			// Member access is not enforced yet, but retain the exact current
			// visibility just like the existing one-word directives do.
			// Recognizing `strict` only at this aggregate-section boundary
			// also leaves it usable as an identifier elsewhere.
			if (maybe_parse_directive("private")) {
				visibility = "strict private";
			} else if (maybe_parse_directive("protected")) {
				visibility = "strict protected";
			} else {
				raise_parse_error("expected private or protected after strict");
			}
			continue;
		} else if (peek_keyword("type")) {
			if (is_class) {
				raise_type_error("class type unsupported", owner_class);
			}
			parse_type_block(true);
			is_class = false;
			continue; // type declarations consume their terminating semicolons
		} else if (peek_keyword("const")) {
			if (is_class) {
				raise_type_error("class const unsupported", owner_class);
			}
			parse_const_block(owner_class);
			is_class = false;
			continue; // const declarations consume their terminating semicolons
		} else if (peek_keyword("var")) {
			bool class_variables = is_class;
			if (class_variables && !dynamic_cast<ClassType*>(owner_class)) {
				raise_type_kind_mismatch("class variable container", "class", owner_class);
			}
			parse_keyword("var");
			// This is an aggregate field section, not a declaration-scope var
			// block.  Keep every field in Pascal declaration order so RecordType
			// layout reconstruction and emitted C++ field order use the same
			// authoritative sequence.
			while (!at_visibility_section()) {
				auto first_name = maybe_parse_identifier();
				if (!first_name) {
					break;
				}
				std::vector<std::string> member_names{*first_name};
				while (maybe_parse_comma()) {
					member_names.push_back(parse_identifier());
				}
				parse_colon();
				auto ty = parse_type_expression(false);
				if (dynamic_cast<PackedRecordType*>(owner_class) && ty->has_managed_lifetime()) {
					raise_type_error("managed fields inside packed records are not implemented", ty);
				}
				for (const auto& member_name : member_names) {
					// A class variable has one storage location owned by its
					// declaring class. It must not become a data member of
					// every metaclass instance: that would give Base.X and
					// Child.X different storage, unlike Pascal.
					auto kind = class_variables ? StorageSlot::Kind::StaticMember : StorageSlot::Kind::AggregateMember;
					auto slot = new StorageSlot(cxx_value_name(member_name), ty, kind, owner_class);
					if (!body->register_variable(member_name, slot, ty)) {
						raise_parse_error("duplicate member identifier: " + member_name);
					}
					if (auto packed = dynamic_cast<PackedRecordType*>(owner_class)) {
						packed->fields.push_back({member_name, slot, ty});
					} else if (auto record = dynamic_cast<RecordType*>(owner_class)) {
						record->fields.push_back({member_name, slot, ty});
					} else if (auto object = dynamic_cast<ObjectType*>(owner_class)) {
						object->fields.push_back({member_name, slot, ty});
					}
				}
				parse_semicolon();
			}
			is_class = false;
			continue;
		} else if (peek_keyword("class")) {
			if (is_class) {
				raise_parse_error("internal error: someone forgot to consume 'class'");
			}
			is_class = true;
			consume();
			if (dynamic_cast<ClassType*>(owner_class) != nullptr || dynamic_cast<RecordType*>(owner_class) != nullptr || dynamic_cast<PackedRecordType*>(owner_class) != nullptr) {
				continue;
			} else {
				raise_type_kind_mismatch("class method container", "class or record", owner_class);
			}
		} else if (peek_keyword("procedure") || peek_keyword("function") || peek_keyword("destructor") || peek_keyword("constructor")) {
			if (dynamic_cast<PackedRecordType*>(owner_class) && !is_class) {
				raise_type_error("instance methods inside packed records are not implemented", owner_class);
			}
			if (is_class && (peek_keyword("constructor") || peek_keyword("destructor"))) {
				auto class_type = dynamic_cast<ClassType*>(owner_class);
				if (!class_type) {
					raise_type_kind_mismatch("class lifecycle hook owner", "class", owner_class);
				}
				parse_class_lifecycle_prototype(class_type, peek_keyword("constructor") ? CLASS_CONSTRUCTOR : CLASS_DESTRUCTOR);
			} else {
				parse_method_prototype(body, owner_class, peek_keyword("function"), peek_keyword("destructor"), peek_keyword("constructor"), is_class);
			}
			is_class = false;
			continue; // parse_method_prototype consumes its terminating ';'
		} else if (peek_keyword("property")) {
			if (is_class) {
				raise_type_error("'class property' is not implemented", owner_class);
			}
			parse_property_declaration(body, owner_class);
			continue; // property parser consumes its terminating semicolon(s)
		} else if (peek_keyword("case")) {
			auto rt = dynamic_cast<RecordType*>(owner_class);
			auto packed = dynamic_cast<PackedRecordType*>(owner_class);
			if (is_class) {
				raise_type_error("variant part only valid in a record, not in a metaclass", owner_class);
			}
			if (!rt && !packed) {
				raise_type_kind_mismatch("variant part owner", "record", owner_class);
			}
			auto variant = parse_record_variant(owner_class, body);
			if (rt) {
				rt->variant = variant;
			} else {
				packed->variant = variant;
			}
			break; // variant part must come last; do not require a trailing ';'
		} else {
			if (is_class) {
				raise_type_error("class var unsupported", owner_class);
			}
			// parse_var_block inlined
			std::vector<std::string> member_names;
			do {
				auto member_name = parse_identifier();
				member_names.push_back(member_name);
			} while (maybe_parse_comma());
			parse_colon();
			auto ty = parse_type_expression(false);
			if (dynamic_cast<PackedRecordType*>(owner_class) && ty->has_managed_lifetime()) {
				raise_type_error("managed fields inside packed records are not implemented", ty);
			}
			for (auto member_name : member_names) {
				auto slot = new StorageSlot(cxx_value_name(member_name), ty, StorageSlot::Kind::AggregateMember, owner_class);
				if (!body->register_variable(member_name, slot, ty)) {
					raise_parse_error("duplicate member identifier: " + member_name);
				}
				if (auto packed = dynamic_cast<PackedRecordType*>(owner_class)) {
					packed->fields.push_back({member_name, slot, ty});
				} else if (auto record = dynamic_cast<RecordType*>(owner_class)) {
					record->fields.push_back({member_name, slot, ty});
				} else if (auto object = dynamic_cast<ObjectType*>(owner_class)) {
					object->fields.push_back({member_name, slot, ty});
				}
			}
		}
		if (input_token.size() && input_token != "end") {
			if (!maybe_parse_semicolon()) {
				raise_parse_error("missing semicolon");
			}
		} else {
			break;
		}
	} while (true);
	validate_class_forwards(body);
	pop_declaration_frame();
	pop_scope();
	if (is_class) {
		raise_parse_error("internal error: someone forgot to consume 'class'");
	}
	if (type_block_frames.empty()) {
		// An anonymous aggregate parsed outside a named type block cannot
		// contain an implicit forward reference, so its graph is already
		// canonical when its body closes.
		validate_aggregate_declaration_semantics(body);
	} else {
		assert(!type_block_deferred_aggregates.empty());
		// Do not validate signatures merely because this aggregate's own body
		// is complete. Earlier declarations in the surrounding type block can
		// still hold placeholders for types published later in that block.
		type_block_deferred_aggregates.back().push_back(body);
	}
	return body;
}

VariantPart* Parser::parse_record_variant(Type* owner, Frame* body) {
	auto variant = new VariantPart;
	const bool packed = dynamic_cast<PackedRecordType*>(owner);
	parse_keyword("case");
	// `case <sel_name> ':' <TagType> of ...` introduces a selector field;
	// `case <TagType> of ...` is tag-less. Single-token lookahead: take an
	// identifier first; if ':' follows, it's the selector name (consume ':'
	// and parse the real tag type after it). Otherwise the identifier WAS
	// the tag type -- resolve it directly.
	auto first = parse_identifier();
	Type* tag_type;
	if (maybe_parse_colon()) {
		variant->has_selector = true;
		variant->selector_name = first;
		variant->selector_cxx_name = cxx_value_name(first);
		tag_type = parse_type_expression(false);

		auto slot = new StorageSlot(variant->selector_cxx_name, tag_type, StorageSlot::Kind::AggregateMember, owner);
		if (!body->register_variable(first, slot, tag_type)) {
			raise_parse_error("duplicate member identifier: " + first);
		}
		variant->selector_slot = slot;
	} else {
		tag_type = resolve_type(first, false);
	}
	variant->selector_type = tag_type;
	parse_keyword("of");
	while (!peek_keyword("end") && input_token != ")") {
		// Case label list -- comma-separated constant expressions. Values
		// are discarded: layout is a flat overlapping union regardless of
		// which label is active.
		parse_expression();
		while (maybe_parse_comma()) {
			parse_expression();
		}
		parse_colon();
		parse_opening_paren();
		VariantArm arm;
		if (input_token != ")") {
			do {
				if (peek_keyword("case")) {
					arm.variant = parse_record_variant(owner, body);
					break;
				}
				std::vector<std::string> names{parse_identifier()};
				while (maybe_parse_comma()) {
					names.push_back(parse_identifier());
				}
				parse_colon();
				auto fty = parse_type_expression(false);
				if (packed && fty->has_managed_lifetime()) {
					raise_type_error("managed fields inside packed records are not implemented", fty);
				}
				for (const auto& fname : names) {
					auto slot = new StorageSlot(cxx_value_name(fname), fty, StorageSlot::Kind::AggregateMember, owner);
					// Every variant field shares the record's one member
					// namespace. The recursive arm tree exists only for
					// layout and emission.
					if (!body->register_variable(fname, slot, fty)) {
						raise_parse_error("duplicate member identifier: " + fname);
					}
					arm.fields.push_back({fname, slot, fty});
				}
				if (!maybe_parse_semicolon()) {
					break;
				}
			} while (input_token != ")");
		}
		parse_closing_paren();
		variant->arms.push_back(std::move(arm));
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
	return variant;
}

Type* Parser::parse_class_type(ClassType* completing_forward, bool allow_forward_declaration) {
	parse_keyword("class");
	if (maybe_parse_keyword("of")) {
		if (completing_forward) {
			raise_type_error("forward class declaration must be completed by a class definition", completing_forward);
		}
		auto target_ty = parse_type_expression(true);
		if (auto target_class_ty = dynamic_cast<IncompleteType*>(target_ty)) {
			return new ClassRefType(current_location(), target_class_ty);
		} else if (auto target_class_ty = dynamic_cast<ClassType*>(target_ty)) {
			return new ClassRefType(current_location(), target_class_ty);
		} else {
			return raise_type_kind_mismatch("parse_class_type: type after 'class of' is not a class", "class", target_ty);
		}
		// FIXME: return lookup_builtin_type("::u_system::m_iobject");
		// return somehow target_ty->cxx_name + "::m_meta" but that would make the metaclass first-class;
	}
	if (input_token == ";") {
		if (!allow_forward_declaration) {
			raise_parse_error("class forward declaration is only valid as a named type declaration");
		}
		if (completing_forward) {
			raise_type_error("duplicate forward class declaration", completing_forward);
		}
		auto ct = new ClassType(current_location(), nullptr, {}, nullptr);
		ct->is_forward_declaration = true;
		return ct;
	}
	// Native FPC's class-level abstract option is metadata, independent of
	// abstract methods. It is parsed before the ancestor list, is not
	// inherited, and has no C++ emission effect. The later construction
	// warning can consult the stored flag without changing class lowering.
	const bool is_abstract = maybe_parse_keyword("abstract");
	ClassType* super_ty = nullptr;
	std::vector<InterfaceType*> implemented_interfaces;
	const bool has_ancestor_list = maybe_parse_opening_paren();
	if (has_ancestor_list) {
		auto s_ty = parse_type_expression(false);
		super_ty = dynamic_cast<ClassType*>(s_ty);
		if (super_ty && super_ty->is_forward_declaration) {
			raise_type_error("superclass forward declaration '" + super_ty->forward_name + "' must be resolved before it is inherited", super_ty);
		}
		if (super_ty == nullptr) {
			if (auto incomplete = dynamic_cast<IncompleteType*>(s_ty); incomplete && !incomplete->resolved) {
				raise_type_error("superclass forward declaration '" + incomplete->name + "' must be resolved before it is inherited", incomplete);
			}
			raise_type_kind_mismatch("parse_class_type: superclass is not a class", "class", s_ty);
		}
		while (maybe_parse_comma()) {
			auto i_ty = parse_type_expression(false);
			if (auto interface_ty = dynamic_cast<InterfaceType*>(i_ty)) {
				implemented_interfaces.push_back(interface_ty);
			} else {
				raise_type_kind_mismatch("parse_class_type: type is not an interface", "interface", i_ty);
			}
		}
		parse_closing_paren();
	} else {
		// TObject is the sole root class. Every other bare `class` gets its
		// implicit parent from the System unit itself, rather than whichever
		// lexical scope might happen to contain a shadowing `TObject`.
		bool defining_system_tobject = current_unit && current_unit->name == "system" && current_type_declaration_name == "tobject";
		if (!defining_system_tobject) {
			super_ty = lookup_implicit_tobject_superclass();
		}
	}
	auto ct = completing_forward ? completing_forward : new ClassType(current_location(), nullptr, {}, nullptr);
	ct->implemented_interfaces = std::move(implemented_interfaces);
	ct->super = super_ty;
	ct->is_forward_declaration = false;
	ct->is_abstract = is_abstract;
	if (has_ancestor_list && input_token == ";") {
		// `TChild = class(TParent);` is FPC's completed empty-descendant
		// shorthand. Give it the same inherited member Frame as an explicit
		// `class(TParent) end`; leave the semicolon for the enclosing type
		// declaration parser. Bare `TChild = class;` is a forward declaration
		// and deliberately does not enter this path.
		ct->children = make_aggregate_body_frame(ct);
		return ct;
	}
	ct->children = parse_aggregate_type_body(ct);
	parse_keyword("end");
	return ct;
}

ClassType* Parser::lookup_implicit_tobject_superclass() {
	Unit* system_unit = unit_registry ? unit_registry->lookup("system") : nullptr;
	if (!system_unit) {
		raise_type_parse_error("implicit class inheritance requires the System unit");
		return nullptr;
	}

	Frame* system_frame = system_unit->frame;
	if (!system_frame) {
		raise_type_parse_error("implicit class inheritance requires a member frame on the System unit");
		return nullptr;
	}

	Type* tobject_type = system_frame->lookup_type("tobject");
	if (!tobject_type) {
		raise_type_parse_error("implicit class inheritance requires System.TObject");
		return nullptr;
	}

	std::unordered_set<IncompleteType*> seen;
	while (auto incomplete = dynamic_cast<IncompleteType*>(tobject_type)) {
		if (!seen.insert(incomplete).second) {
			raise_type_error("System.TObject has a cyclic type definition", incomplete);
			return nullptr;
		}
		if (!incomplete->resolved) {
			raise_type_error("System.TObject is unresolved while applying implicit class inheritance", incomplete);
			return nullptr;
		}
		tobject_type = incomplete->resolved;
	}

	auto tobject_class = dynamic_cast<ClassType*>(tobject_type);
	if (!tobject_class) {
		raise_type_kind_mismatch("System.TObject used for implicit class inheritance is not a class", "class", tobject_type);
		return nullptr;
	}
	return tobject_class;
}

Type* Parser::parse_interface_type() {
	// InterfaceType currently denotes TPCC's non-refcounted interface ABI.
	// Reject COM at the declaration boundary: accepting it here would make a
	// source-visible COM type silently use CORBA ownership and conversion
	// semantics throughout type checking and C++ lowering.
	if (directive_state.get_interface_model() == InterfaceModel::COM) {
		raise_type_parse_error("COM interfaces are not implemented; use {$interfaces corba}");
	}
	parse_keyword("interface");
	std::vector<InterfaceType*> implemented_interfaces;
	if (maybe_parse_opening_paren()) {
		do {
			auto i_ty = parse_type_expression(false);
			if (auto interface_ty = dynamic_cast<InterfaceType*>(i_ty)) {
				implemented_interfaces.push_back(interface_ty);
			} else {
				raise_type_kind_mismatch("parse_interface_type: type is not an interface", "interface", i_ty);
			}
		} while (maybe_parse_comma());
		parse_closing_paren();
	}
	std::optional<std::string> guid_literal;
	if (maybe_parse_opening_bracket()) {
		// Object Pascal places an interface's GUID clause after its optional
		// ancestor list and before the first member. Preserve the literal for
		// interface conversions; it has no effect on C++ inheritance or layout.
		guid_literal = parse_string_literal();
		parse_closing_bracket();
	}
	auto ct = new InterfaceType(current_location(), nullptr, std::move(implemented_interfaces));
	ct->guid_literal = std::move(guid_literal);
	ct->children = parse_aggregate_type_body(ct);
	parse_keyword("end");
	return ct;
}

Type* Parser::parse_record_type() {
	if (maybe_parse_keyword("packed")) {
		parse_keyword("record");
		if (maybe_parse_opening_paren()) {
			return raise_type_parse_error("packed record with parenthesized header not implemented");
		}
		auto rt = new PackedRecordType(current_location(), nullptr);
		rt->children = parse_aggregate_type_body(rt);
		parse_keyword("end");
		return rt;
	}
	parse_keyword("record");
	if (maybe_parse_opening_paren()) {
		return raise_type_parse_error("record with parenthesized header not implemented yet");
	}
	auto rt = new RecordType(current_location(), nullptr);
	rt->children = parse_aggregate_type_body(rt);
	parse_keyword("end");
	return rt;
}

Type* Parser::parse_object_type() {
	parse_keyword("object");
	ObjectType* super_ty = nullptr;
	if (maybe_parse_opening_paren()) {
		auto s_ty = parse_type_expression(false);
		super_ty = dynamic_cast<ObjectType*>(s_ty);
		if (super_ty == nullptr) {
			raise_type_kind_mismatch("parse_object_type: super is not an object", "object", s_ty);
		}
		parse_closing_paren();
	}
	auto ot = new ObjectType(current_location(), nullptr, super_ty);
	ot->children = parse_aggregate_type_body(ot);
	parse_keyword("end");
	return ot;
}

Type* Parser::parse_array_type(bool direct_formal) {
	parse_keyword("array");
	if (!maybe_parse_opening_bracket()) {
		parse_keyword("of");
		// Only the outer array constructor written directly in a formal is an
		// open-array contract. Its element is parsed normally, so
		// `const X: array of array of T` means an open array of dynamic arrays
		// rather than two nested open contracts.
		Type* item_type = parse_type_expression(false);
		if (direct_formal) {
			return new OpenArrayType(current_location(), item_type);
		}
		return new DynamicArrayType(current_location(), item_type);
	}
	std::vector<Type*> bounds_types;
	do {
		bounds_types.push_back(parse_type_expression(false));
	} while (maybe_parse_comma());
	parse_closing_bracket();
	parse_keyword("of");
	// Pascal forward-type syntax does not make arbitrary by-value array
	// elements order-independent. Only pointer targets and `class of` targets
	// open the implicit-forward path.
	Type* item_type = parse_type_expression(false);
	for (auto it = bounds_types.rbegin(); it != bounds_types.rend(); ++it) {
		OrdinalRange range;
		if (type_block_frames.empty()) {
			std::string error;
			if (!ordinal_range_for_type(*it, &range, &error)) {
				return raise_type_error(error, *it);
			}
		}
		item_type = new FixedArrayType(current_location(), *it, range, item_type);
	}
	return item_type;
}

Type* Parser::parse_formal_type_expression() {
	// Open-array meaning belongs only to the outer `array of` written
	// directly as a formal type. Keeping this decision at that syntactic
	// boundary prevents a parser mode from leaking into nested element types.
	if (peek_keyword("array")) {
		return parse_array_type(true);
	}
	return parse_type_expression(false);
}

Type* Parser::parse_enum_type() {
	// Caller already consumed the `(` via maybe_parse_opening_paren in
	// parse_type_expression.
	auto et = new EnumType(current_location());
	et->owning_unit = declaration_unit(current_declaration_frame());
	int64_t next_value = 0;
	do {
		auto pas = parse_identifier();
		auto cxx = cxx_value_name(pas);
		int64_t value = next_value;
		bool explicit_value = false;
		if (input_token == ":=" || input_token == "=") {
			explicit_value = true;
			consume();
			Node* expression = parse_expression();
			ConstEvalContext ctx;
			ConstEvalResult folded = expression->const_eval(ctx);
			if (folded.kind == ConstEvalResult::Kind::NotConstant) {
				raise_value_error("explicit enum value must be constant", expression);
			}
			if (folded.kind == ConstEvalResult::Kind::Error) {
				raise_value_error(folded.message, expression);
			}

			if (auto integer = dynamic_cast<Integer*>(folded.node)) {
				const uint64_t maximum = integer->negative ? uint64_t{static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1} : static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
				if (integer->value > maximum) {
					raise_value_error("explicit enum value is outside "
					                  "signed 32-bit range",
					                  integer);
				}
				value = integer->negative ? -static_cast<int64_t>(integer->value) : static_cast<int64_t>(integer->value);
			} else if (auto member = dynamic_cast<EnumMemberRef*>(folded.node)) {
				if (member->ty != et) {
					raise_type_mismatch("explicit enum member value", et, member->ty);
				}
				value = member->value;
			} else if (auto character = dynamic_cast<String*>(folded.node); character && character->ty == char_type() && character->value.size() == 1) {
				value = static_cast<unsigned char>(character->value[0]);
			} else {
				raise_type_kind_mismatch("explicit enum value", "integer, character, or member of the same enum", folded.node ? folded.node->ty : nullptr);
			}
		} else if (next_value > std::numeric_limits<int32_t>::max()) {
			raise_type_error("implicit enum value is outside signed 32-bit "
			                 "range",
			                 et);
		}

		et->members.push_back({pas, cxx, value, explicit_value});
		// Register the member as a value in the enclosing scope so bare uses
		// (`c := Red`) resolve. Pascal's default is unscoped enum members:
		// they live in the same scope as the enum type itself, NOT inside
		// the type. (A future compiler might add `{$scopedenums+}` and route
		// them through the type; that is not this compiler.)
		auto ref = new EnumMemberRef(cxx, value, et);
		ref->owning_unit = et->owning_unit;
		if (!current_declaration_frame()->register_variable(pas, ref, et)) {
			raise_parse_error("duplicate identifier: " + pas);
		}
		next_value = value + 1;
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	parse_closing_paren();

	int64_t min_value = et->members.front().value;
	int64_t max_value = et->members.front().value;
	for (const auto& m : et->members) {
		if (m.value < min_value) {
			min_value = m.value;
		}
		if (m.value > max_value) {
			max_value = m.value;
		}
	}
	const int packenum = directive_state.get_packenum();
	int savesize = 1;
	if (min_value >= std::numeric_limits<int8_t>::min() && max_value <= std::numeric_limits<int8_t>::max()) {
		savesize = 1;
	} else if (min_value >= std::numeric_limits<int16_t>::min() && max_value <= std::numeric_limits<int16_t>::max()) {
		savesize = 2;
	} else if (min_value >= std::numeric_limits<int32_t>::min() && max_value <= std::numeric_limits<int32_t>::max()) {
		savesize = 4;
	} else {
		raise_parse_error("enum range too big");
	}
	if (savesize < packenum) {
		savesize = packenum;
	}
	et->carrier_bits = savesize * 8;
	return et;
}

std::string Parser::parse_string_literal() {
	if (input_token.empty() || input_token.front() != '\'') {
		raise_parse_error("expected string literal");
	}
	auto result = extract_string_literal(input_token);
	consume();
	return result;
}

static Type* subrange_range_type(Type* ty) {
	for (;;) {
		if (auto distinct = dynamic_cast<DistinctType*>(ty)) {
			ty = distinct->base_type;
			continue;
		}
		if (auto range = dynamic_cast<SubrangeType*>(ty)) {
			ty = range->base_type;
			continue;
		}
		break;
	}
	return ty;
}

static bool is_integer_semantic_type(Type* ty) {
	ty = subrange_range_type(ty);
	if (ty == &untyped_integer_type()) {
		return true;
	}
	auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
	return intrinsic && intrinsic->rank;
}

static bool ordinal_bounds_contains(const OrdinalBounds& bounds, bool negative, uint64_t magnitude) {
	if (negative) {
		return bounds.signed_type && magnitude <= bounds.min_magnitude;
	}
	return magnitude <= bounds.max_positive;
}

static OrdinalRange::Value ordinal_value(bool negative, uint64_t magnitude) {
	return OrdinalRange::Value{magnitude != 0 && negative, magnitude};
}

static OrdinalRange::Value ordinal_value(int64_t value) {
	if (value < 0) {
		return ordinal_value(true, static_cast<uint64_t>(-(value + 1)) + 1);
	}
	return ordinal_value(false, static_cast<uint64_t>(value));
}

static int compare_ordinal_value(OrdinalRange::Value a, OrdinalRange::Value b) {
	if (a.negative != b.negative) {
		return a.negative ? -1 : 1;
	}
	if (a.magnitude == b.magnitude) {
		return 0;
	}
	if (a.negative) {
		return a.magnitude > b.magnitude ? -1 : 1;
	}
	return a.magnitude < b.magnitude ? -1 : 1;
}

static bool checked_add(uint64_t a, uint64_t b, uint64_t* out) {
	if (a > UINT64_MAX - b) {
		return false;
	}
	*out = a + b;
	return true;
}

static bool checked_add_one(uint64_t value, uint64_t* out) {
	if (value == UINT64_MAX) {
		return false;
	}
	*out = value + 1;
	return true;
}

struct FoldedSubrangeBound {
	enum class Kind { Integer, Char, Enum } kind;
	Node* node = nullptr;
	Type* ty = nullptr;
	bool negative = false;
	uint64_t magnitude = 0;
	OrdinalRange::Value ordinal_value;
};

static std::optional<FoldedSubrangeBound> classify_subrange_bound(Node* node, std::string* error) {
	if (auto i = dynamic_cast<Integer*>(node)) {
		Type* ty = subrange_range_type(i->ty);
		if (ty == char_type()) {
			OrdinalBounds bounds;
			if (!intrinsic_ordinal_bounds(ty, &bounds) || !ordinal_bounds_contains(bounds, i->negative, i->value)) {
				*error = "character subrange bound is outside Char range";
				return {};
			}
			return FoldedSubrangeBound{
			    FoldedSubrangeBound::Kind::Char, node, ty, false, i->value, ordinal_value(false, i->value),
			};
		}
		if (!is_integer_semantic_type(ty)) {
			*error = "integer subrange bound has a non-integer type";
			return {};
		}
		return FoldedSubrangeBound{
		    FoldedSubrangeBound::Kind::Integer, node, ty, i->negative, i->value, ordinal_value(i->negative, i->value),
		};
	} else if (auto s = dynamic_cast<String*>(node)) {
		if (s->value.size() != 1) {
			*error = "string literal subrange bound must contain exactly one character";
			return {};
		}
		auto value = static_cast<unsigned char>(s->value[0]);
		OrdinalBounds bounds;
		if (!intrinsic_ordinal_bounds(char_type(), &bounds) || !ordinal_bounds_contains(bounds, false, value)) {
			*error = "character subrange bound is outside Char range";
			return {};
		}
		auto as_char = new Integer(value, char_type());
		return FoldedSubrangeBound{
		    FoldedSubrangeBound::Kind::Char, as_char, char_type(), false, value, ordinal_value(false, value),
		};
	} else if (auto e = dynamic_cast<EnumMemberRef*>(node)) {
		Type* ty = subrange_range_type(e->ty);
		if (!dynamic_cast<EnumType*>(ty)) {
			*error = "enum subrange bound has a non-enum type";
			return {};
		}
		return FoldedSubrangeBound{
		    FoldedSubrangeBound::Kind::Enum, node, ty, false, 0, ordinal_value(e->value),
		};
	}
	*error = "subrange bound must fold to an ordinal constant";
	return {};
}

static bool ordinal_length(OrdinalRange::Value lo, OrdinalRange::Value hi, uint64_t* out, std::string* error) {
	if (compare_ordinal_value(lo, hi) > 0) {
		*error = "array index type has an empty range";
		return false;
	}
	uint64_t distance = 0;
	if (lo.negative && hi.negative) {
		distance = lo.magnitude - hi.magnitude;
	} else if (lo.negative) {
		if (!checked_add(lo.magnitude, hi.magnitude, &distance)) {
			*error = "array index range is too large";
			return false;
		}
	} else {
		distance = hi.magnitude - lo.magnitude;
	}
	if (!checked_add_one(distance, out)) {
		*error = "array index range is too large";
		return false;
	}
	return true;
}

static bool make_ordinal_range(Type* index_type, Type* base_type, Node* lower_bound, Node* upper_bound, OrdinalRange::Value lower_ordinal, OrdinalRange::Value upper_ordinal, OrdinalRange* out, std::string* error) {
	uint64_t length = 0;
	if (!ordinal_length(lower_ordinal, upper_ordinal, &length, error)) {
		return false;
	}
	out->index_type = index_type;
	out->base_type = base_type;
	out->lower_bound = lower_bound;
	out->upper_bound = upper_bound;
	out->lower_ordinal = lower_ordinal;
	out->upper_ordinal = upper_ordinal;
	out->length = length;
	return true;
}

static bool ordinal_range_for_type(Type* ty, OrdinalRange* out, std::string* error) {
	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ConstEvalContext ctx;
		ConstEvalResult lower_folded = s->lower_bound->const_eval(ctx);
		ConstEvalResult upper_folded = s->upper_bound->const_eval(ctx);
		if (lower_folded.kind != ConstEvalResult::Kind::Success || upper_folded.kind != ConstEvalResult::Kind::Success) {
			*error = "array subrange bounds must be constant";
			return false;
		}

		auto lower = classify_subrange_bound(lower_folded.node, error);
		if (!lower) {
			return false;
		}
		auto upper = classify_subrange_bound(upper_folded.node, error);
		if (!upper) {
			return false;
		}
		if (lower->kind != upper->kind) {
			*error = "array subrange bounds must be compatible ordinal constants";
			return false;
		}
		return make_ordinal_range(ty, subrange_range_type(s->base_type), lower->node, upper->node, lower->ordinal_value, upper->ordinal_value, out, error);
	} else if (auto e = dynamic_cast<EnumType*>(ty)) {
		const auto* lo = e->min_member();
		const auto* hi = e->max_member();
		if (!lo || !hi) {
			*error = "array enum index type has no members";
			return false;
		}
		return make_ordinal_range(ty, ty, new EnumMemberRef(lo->cxx_name, lo->value, ty), new EnumMemberRef(hi->cxx_name, hi->value, ty), ordinal_value(lo->value), ordinal_value(hi->value), out, error);
	} else {
		OrdinalBounds bounds;
		if (intrinsic_ordinal_bounds(ty, &bounds)) {
			Node* lower_bound = bounds.signed_type ? new Integer(bounds.min_magnitude, ty, true) : new Integer(0, ty);
			Node* upper_bound = new Integer(bounds.max_positive, ty);
			return make_ordinal_range(ty, ty, lower_bound, upper_bound, ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0), ordinal_value(false, bounds.max_positive), out, error);
		}
		*error = "fixed array bounds must be an ordinal type";
		return false;
	}
}

static bool enum_range_has_gaps(Type* ty, const OrdinalRange& range) {
	while (auto subrange = dynamic_cast<SubrangeType*>(ty)) {
		ty = subrange->base_type;
	}
	auto enumeration = dynamic_cast<EnumType*>(ty);
	if (!enumeration) {
		return false;
	}
	// Builtin ordinal iteration advances the carrier by one. Sparse or
	// duplicate enum ordinals would therefore either manufacture values that
	// have no Pascal enumerator or make one carrier value name two members.
	std::vector<int64_t> values;
	for (const EnumType::Member& member : enumeration->members) {
		OrdinalRange::Value value = ordinal_value(member.value);
		if (compare_ordinal_value(value, range.lower_ordinal) >= 0 && compare_ordinal_value(value, range.upper_ordinal) <= 0) {
			values.push_back(member.value);
		}
	}
	std::sort(values.begin(), values.end());
	if (values.size() != range.length || values.empty()) {
		return range.length != 0;
	}
	for (std::size_t i = 1; i < values.size(); ++i) {
		if (values[i] != values[i - 1] + 1) {
			return true;
		}
	}
	return false;
}

static Type* infer_integer_subrange_host(OrdinalRange::Value lo, OrdinalRange::Value hi, std::string* error) {
	// Match the BP/FPC-style storage choice: choose the smallest builtin
	// integer type that can represent the whole range, preferring signed
	// carriers before same-width unsigned carriers. The concrete C++ subrange
	// struct contains exactly one value of this type; it does not erase the
	// Pascal definition's overload identity.
	Type* candidates[] = {
	    shortint_type(), byte_type(), smallint_type(), word_type(), integer_type(), cardinal_type(), int64_type(), qword_type(),
	};
	for (Type* candidate : candidates) {
		OrdinalBounds bounds;
		if (!integer_bounds(candidate, &bounds)) {
			continue;
		}
		OrdinalRange::Value min_value = ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0);
		OrdinalRange::Value max_value = ordinal_value(false, bounds.max_positive);
		if (compare_ordinal_value(lo, min_value) >= 0 && compare_ordinal_value(hi, max_value) <= 0) {
			return candidate;
		}
	}
	*error = "integer subrange bounds are outside the supported integer range";
	return nullptr;
}

static Node* convert_integer_subrange_bound(const FoldedSubrangeBound& bound, Type* host, std::string* error) {
	ConstEvalResult converted = const_convert_integer(bound.magnitude, bound.negative, bound.ty, host);
	if (converted.kind == ConstEvalResult::Kind::Success) {
		return converted.node;
	}
	if (converted.kind == ConstEvalResult::Kind::Error) {
		*error = converted.message;
	} else {
		*error = "integer subrange bound could not be converted to host type";
	}
	return nullptr;
}

std::string Parser::next_subrange_cxx_name() {
	/*
	 * Every Pascal subrange definition receives a named C++ carrier, including
	 * a definition used only by one local object. Do not replace that uniform
	 * rule with an unnamed-struct/decltype special case:
	 *
	 * - one TPCC Type* must be re-denotable by the same C++ type in routine
	 *   declarations and definitions, results, pointers, template arguments,
	 *   conversions, aggregate fields, and cross-unit references;
	 * - `auto` cannot spell prototypes or explicit target construction, and an
	 *   `auto` parameter would instead create a C++ template;
	 * - repeating an unnamed struct definition creates a different C++ type;
	 * - an unnamed class has no injected class name with which to denote its
	 *   own type;
	 * - `this` denotes an object, not the surrounding class namespace, and is
	 *   unavailable in static contexts;
	 * - C++20 forbids static data members in unnamed and local classes; and
	 * - a local-only representation would create a second lowering path whose
	 *   behavior depends on incidental use sites, making generated code and
	 *   reviews less uniform.
	 *
	 * Pascal source identifiers lower with t_/p_/o_ prefixes, so the m_
	 * compiler-private namespace also prevents a source declaration from
	 * colliding with these generated tags.
	 */
	return "m_subrange_" + std::to_string(++next_subrange_type_number);
}

Type* Parser::parse_subrange_type(Node* lower_bound, Node* upper_bound) {
	ConstEvalContext ctx;
	ConstEvalResult lower_folded = lower_bound->const_eval(ctx);
	ConstEvalResult upper_folded = upper_bound->const_eval(ctx);
	if (lower_folded.kind == ConstEvalResult::Kind::NotConstant || upper_folded.kind == ConstEvalResult::Kind::NotConstant) {
		raise_values_error("subrange bounds must be constant expressions", {{"lower bound", lower_bound}, {"upper bound", upper_bound}});
	}
	if (lower_folded.kind == ConstEvalResult::Kind::Error) {
		raise_value_error(lower_folded.message, lower_bound);
	}
	if (upper_folded.kind == ConstEvalResult::Kind::Error) {
		raise_value_error(upper_folded.message, upper_bound);
	}

	std::string error;
	auto lower = classify_subrange_bound(lower_folded.node, &error);
	if (!lower) {
		raise_value_error(error, lower_folded.node);
	}
	auto upper = classify_subrange_bound(upper_folded.node, &error);
	if (!upper) {
		raise_value_error(error, upper_folded.node);
	}
	if (lower->kind != upper->kind) {
		raise_values_error("subrange bounds must be compatible ordinal constants", {{"lower bound", lower->node}, {"upper bound", upper->node}});
	}

	// A subrange expression is generative. Its chosen host type and bounds
	// determine compatibility and lowering, but never replace this definition
	// with an earlier subrange of the same shape.
	auto make_subrange = [&](Type* base, Node* typed_lower, Node* typed_upper) -> SubrangeType* {
		auto result = new SubrangeType(current_location(), next_subrange_cxx_name(), base, typed_lower, typed_upper);
		// Top-level and aggregate-contained unit types are declared in
		// that unit's generated namespace. Routine-local definitions are
		// emitted as C++ local classes and therefore have no unit
		// qualifier. Programs likewise emit in the global namespace.
		if (!current_routine && current_unit && !current_unit->is_program) {
			result->owning_unit = current_unit;
		}
		return result;
	};
	switch (lower->kind) {
	case FoldedSubrangeBound::Kind::Integer: {
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0) {
			raise_values_error("subrange upper bound is lower than lower bound", {{"lower bound", lower->node}, {"upper bound", upper->node}});
		}
		Type* host = infer_integer_subrange_host(lower->ordinal_value, upper->ordinal_value, &error);
		if (!host) {
			raise_values_error(error, {{"lower bound", lower->node}, {"upper bound", upper->node}});
		}
		Node* typed_lower = convert_integer_subrange_bound(*lower, host, &error);
		if (!typed_lower) {
			raise_value_error(error, lower->node);
		}
		Node* typed_upper = convert_integer_subrange_bound(*upper, host, &error);
		if (!typed_upper) {
			raise_value_error(error, upper->node);
		}
		return make_subrange(host, typed_lower, typed_upper);
	}
	case FoldedSubrangeBound::Kind::Char:
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0) {
			raise_values_error("subrange upper bound is lower than lower bound", {{"lower bound", lower->node}, {"upper bound", upper->node}});
		}
		return make_subrange(char_type(), lower->node, upper->node);
	case FoldedSubrangeBound::Kind::Enum:
		if (lower->ty != upper->ty) {
			return raise_type_mismatch("subrange constructor with bounds from the same enum type", lower->ty, upper->ty);
		}
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0) {
			raise_values_error("subrange upper bound is lower than lower bound", {{"lower bound", lower->node}, {"upper bound", upper->node}});
		}
		return make_subrange(lower->ty, lower->node, upper->node);
	}
	raise_values_error("unsupported subrange bound kind", {{"lower bound", lower->node}, {"upper bound", upper->node}});
}

static bool token_continues_subrange_bound_after_primary(const std::string& token) {
	return token == "." || token == "(" || token == "[" || token == "^" || token == "**" || token == "*" || token == "/" || token == "div" || token == "mod" || token == "and" || token == "shl" || token == "shr" || token == "as" || token == "is" || token == "<<" || token == ">>" || token == "><" || token == "+" || token == "-" || token == "or" || token == "|" || token == "xor";
}

/** allow_forward: if true, an unresolved identifier at this parse position may
 *  become an IncompleteType, but only while parse_type_block is active. The
 *  valid starts stay explicit here: identifiers are resolved as types unless
 *  `..`/expression continuation makes them subrange bounds; we do not parse a
 *  general expression and backtrack into a type name. */
Type* Parser::parse_type_expression(bool allow_forward) {
	if (maybe_parse_opening_paren()) {
		return parse_enum_type();
	} else if (peek_keyword("bitpacked")) {
		parse_keyword("bitpacked");
		return raise_type_parse_error("bitpacked record is not implemented");
	} else if (maybe_parse_circumflex()) {
		return new PointerType(current_location(), parse_type_expression(true));
	} else if (peek_keyword("string")) {
		// $H controls only an unqualified String token. Capture its state
		// before consuming the token: consume() also processes following
		// directives, and `String {$H+};` must retain the String meaning in
		// effect where the token itself occurred.
		const bool long_strings = directive_state.switch_enabled('h');
		parse_keyword("string");
		if (!maybe_parse_opening_bracket()) {
			return long_strings ? ansistring_type() : shortstring_type();
		}
		Node* capacity_expression = parse_expression();
		parse_closing_bracket();
		ConstEvalContext ctx;
		ConstEvalResult folded = capacity_expression->const_eval(ctx);
		if (folded.kind == ConstEvalResult::Kind::NotConstant) {
			raise_value_error("shortstring capacity must be a constant integer", capacity_expression);
		}
		if (folded.kind == ConstEvalResult::Kind::Error) {
			raise_value_error(folded.message, capacity_expression);
		}
		auto capacity = dynamic_cast<Integer*>(folded.node);
		if (!capacity || capacity->negative || capacity->value == 0 || capacity->value > 255) {
			raise_value_error("shortstring capacity must be in 1..255", folded.node ? folded.node : capacity_expression);
		}
		// Bracketed string syntax is a type constructor. Do not reuse the
		// builtin capacity cache: identical constructor operands still
		// create distinct Pascal definitions, with compatibility handled
		// separately from identity.
		return new ShortStringType(current_location(), static_cast<uint8_t>(capacity->value));
	} else if (peek_keyword("set")) {
		parse_keyword("set");
		parse_keyword("of");
		return new FixedSetType(current_location(), parse_type_expression(false));
	} else if (peek_keyword("file")) {
		parse_keyword("file");
		if (!maybe_parse_keyword("of")) {
			return file_type();
		}
		// `file of T` is generative even when another definition has the same
		// element type. Such definitions may erase to the same backend carrier,
		// but they remain distinct Pascal types, particularly for typed var/out.
		return new TypedFileType(current_location(), parse_type_expression(false));
	} else if (peek_keyword("array")) {
		return parse_array_type(false);
	} else if (peek_keyword("object")) {
		return parse_object_type();
	} else if (peek_keyword("packed") || peek_keyword("record")) {
		return parse_record_type();
	} else if (peek_keyword("class")) {
		return parse_class_type();
	} else if (peek_keyword("interface")) {
		return parse_interface_type();
	} else if (peek_keyword("procedure")) {
		return parse_procedure_type();
	} else if (peek_keyword("function")) {
		return parse_function_type();
	} else if (peek_keyword("operator")) {
		return parse_operator_type();
	} else if (peek_directive("external")) { // usually primitive; otherwise we would miss a lot of info
		parse_directive("external");
		parse_directive("name");
		std::string cxx_name = parse_string_literal();

		auto intrinsic = lookup_external_type(nullptr, cxx_name);

		// auto intrinsic = new IntrinsicType(cxx_name); // FIXME: what? reuse or what?
		// lhs_placeholder->resolved = intrinsic;
		// scope->rebind_type(name, intrinsic);
		return intrinsic;
	} else {
		// Simple-type syntax is ambiguous at an identifier:
		//
		//   TColor              { type identifier }
		//   Red..Blue           { enum-member subrange }
		//   MaxListSize - 1..N  { constant-expression subrange }
		//
		// Do not speculatively parse a full expression and then backtrack to a
		// type name. Consume the identifier once, then use the token already in
		// input_token to decide whether the identifier is bare (a type name) or
		// the seed of a constant expression whose lower bound must be followed by
		// `..`. Subrange-bound parsing deliberately stops before comparison
		// operators so an enclosing `=` in `const X: T = ...` remains visible.
		if (token_is_identifier(input_token)) {
			const LeadingTokenDirectives identifier_directives = directive_state.leading_token_directives();
			std::string id = parse_identifier();
			if (maybe_parse_period_period()) {
				LeadingTokenDirectives leading_directives = identifier_directives;
				return parse_subrange_type(parse_value_from_identifier(id, identifier_directives, &leading_directives), parse_subrange_bound_expression());
			}
			if (maybe_parse_period()) {
				return parse_qualified_type_member(id);
			}
			if (token_continues_subrange_bound_after_primary(input_token)) {
				Node* lower_bound = parse_subrange_bound_expression_after_identifier(id, identifier_directives);
				parse_period_period();
				return parse_subrange_type(lower_bound, parse_subrange_bound_expression());
			}
			return resolve_type(id, allow_forward);
		}

		Node* lower_bound = parse_subrange_bound_expression();
		parse_period_period();
		return parse_subrange_type(lower_bound, parse_subrange_bound_expression());
	}
}

void Parser::parse_statement() {
	maybe_parse_statement();
}

void Parser::parse_block_body() {
	while (input_token.size()) {
		maybe_parse_statement();
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
}

void Parser::parse_unit_statement_sequence(bool stop_at_finalization) {
	while (!input_token.empty() && !peek_keyword("end") && !(stop_at_finalization && peek_keyword("finalization"))) {
		maybe_parse_statement();
		if (peek_keyword("end") || (stop_at_finalization && peek_keyword("finalization"))) {
			break;
		}
		if (!maybe_parse_semicolon()) {
			break;
		}
	}
}

void Parser::push_statement_control_context() {
	statement_control_contexts.emplace_back();
}

void Parser::pop_statement_control_context() {
	if (statement_control_contexts.empty()) {
		raise_parse_error("internal parser error: statement control context underflow");
	}
	statement_control_contexts.pop_back();
}

unsigned Parser::current_exception_block() const {
	return statement_control_contexts.empty() ? 0 : statement_control_contexts.back().current_exception_block;
}

unsigned Parser::enter_exception_block() {
	if (statement_control_contexts.empty()) {
		push_statement_control_context();
	}
	auto& context = statement_control_contexts.back();
	context.current_exception_block = ++context.next_exception_block;
	return context.current_exception_block;
}

void Parser::restore_exception_block(unsigned block) {
	if (statement_control_contexts.empty()) {
		return;
	}
	statement_control_contexts.back().current_exception_block = block;
}

void Parser::record_label_definition(const std::string& name, SourceLocation location) {
	if (statement_control_contexts.empty()) {
		push_statement_control_context();
	}
	auto& state = statement_control_contexts.back().labels[name];
	if (state.definition_block) {
		emit_parse_error_at(location, "duplicate statement label '" + name + "'");
	}
	state.definition_block = current_exception_block();
	for (const auto& use : state.goto_blocks) {
		if (use.first != *state.definition_block) {
			emit_parse_error_at(use.second, "goto may not enter or leave a Pascal exception block");
		}
	}
}

void Parser::record_goto(const std::string& name, SourceLocation location) {
	if (statement_control_contexts.empty()) {
		push_statement_control_context();
	}
	auto& state = statement_control_contexts.back().labels[name];
	const unsigned block = current_exception_block();
	if (state.definition_block && *state.definition_block != block) {
		emit_parse_error_at(location, "goto may not enter or leave a Pascal exception block");
	}
	state.goto_blocks.push_back({block, std::move(location)});
}

void Parser::parse_label_block() {
	parse_keyword("label");
	do {
		// Pascal label declarations introduce statement labels, not values. For
		// now they are validation-light because C++ also has function-local labels;
		// emission prefixes them separately from value identifiers.
		parse_identifier();
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	parse_semicolon();
}

static bool ordinal_constant_matches_range_type(Type* base_type, const FoldedSubrangeBound& value) {
	base_type = subrange_range_type(base_type);
	if (base_type == char_type()) {
		return value.kind == FoldedSubrangeBound::Kind::Char;
	}
	if (dynamic_cast<EnumType*>(base_type)) {
		return value.kind == FoldedSubrangeBound::Kind::Enum && value.ty == base_type;
	}
	return is_integer_semantic_type(base_type) && value.kind == FoldedSubrangeBound::Kind::Integer;
}

Node* Parser::parse_storage_initializer(Type* ty) {
	while (auto incomplete = dynamic_cast<IncompleteType*>(ty)) {
		if (!incomplete->resolved) {
			raise_type_error("initialized storage uses unresolved type '" + incomplete->name + "'", incomplete);
		}
		ty = incomplete->resolved;
	}

	if (auto arr = dynamic_cast<FixedArrayType*>(ty)) {
		parse_opening_paren();
		std::vector<Node*> elements;
		if (input_token != ")") {
			do {
				elements.push_back(parse_storage_initializer(arr->item_type));
			} while (maybe_parse_comma());
		}
		parse_closing_paren();
		if (elements.size() != arr->range.length) {
			raise_type_error("array initializer length mismatch: expected " + std::to_string(arr->range.length) + " elements but got " + std::to_string(elements.size()), arr);
		}
		return new FixedArrayLiteral(std::move(elements), ty);
	}

	auto parse_record = [&](const auto& declared_fields) -> Node* {
		parse_opening_paren();
		std::vector<RecordLiteral::Field> fields;
		std::size_t next_field = 0;
		while (input_token != ")") {
			const std::string name = parse_identifier();
			auto found = std::find_if(declared_fields.begin(), declared_fields.end(), [&](const auto& field) { return field.pas_name == name; });
			if (found == declared_fields.end()) {
				raise_type_error("unknown record initializer field '" + name + "'", ty);
			}
			const std::size_t field_index = static_cast<std::size_t>(found - declared_fields.begin());
			if (field_index < next_field) {
				raise_type_error("record initializer field '" + name + "' is repeated or out of declaration order", ty);
			}
			if (field_index > next_field) {
				raise_type_error("record initializer skips field(s) before '" + name + "'", ty);
			}

			parse_colon();
			fields.push_back(RecordLiteral::Field{
			    found->slot,
			    parse_storage_initializer(found->ty),
			});
			next_field = field_index + 1;

			if (input_token != ")") {
				parse_semicolon();
			}
		}
		parse_closing_paren();
		return new RecordLiteral(std::move(fields), ty);
	};

	auto parse_variant_record = [&](Frame* members, const RecordLayout& layout) -> Node* {
		parse_opening_paren();
		std::vector<RecordLiteral::Field> fields;
		uint64_t initialized_through = 0;
		while (input_token != ")") {
			const std::string name = parse_identifier();
			StorageSlot* slot = nullptr;
			if (auto binding = members ? members->lookup_type_or_value_local(name) : std::nullopt) {
				if (auto value = std::get_if<Node*>(&*binding)) {
					slot = dynamic_cast<StorageSlot*>(*value);
				}
			}
			auto found = std::find_if(layout.fields.begin(), layout.fields.end(), [&](const auto& field) { return field.slot == slot; });
			if (!slot || found == layout.fields.end()) {
				raise_type_error("unknown record initializer field '" + name + "'", ty);
			}

			uint64_t next_offset = layout.type.size;
			bool has_next = false;
			for (const auto& candidate : layout.fields) {
				if (candidate.offset >= initialized_through && (!has_next || candidate.offset < next_offset)) {
					next_offset = candidate.offset;
					has_next = true;
				}
			}
			if (found->offset < initialized_through || !has_next) {
				raise_type_error("record initializer field '" + name + "' is repeated or out of declaration order", ty);
			}
			if (found->offset != next_offset) {
				raise_type_error("record initializer skips field(s) before '" + name + "'", ty);
			}

			parse_colon();
			fields.push_back(RecordLiteral::Field{
			    found->slot,
			    parse_storage_initializer(found->ty),
			});
			// Variant arms overlap. Advancing by the selected field's
			// actual storage extent lets the next initializer choose any
			// arm field at the next byte position, while rejecting a
			// second field that overlaps storage already initialized.
			initialized_through = found->offset + found->size;

			if (input_token != ")") {
				parse_semicolon();
			}
		}
		parse_closing_paren();
		return new RecordLiteral(std::move(fields), ty);
	};

	if (auto record = dynamic_cast<RecordType*>(ty)) {
		if (record->variant) {
			auto layout = record_layout(record);
			if (!layout) {
				raise_type_error("cannot determine variant record "
				                 "initializer layout",
				                 record);
			}
			return parse_variant_record(record->children, *layout);
		}
		return parse_record(record->fields);
	} else if (auto record = dynamic_cast<PackedRecordType*>(ty)) {
		if (record->variant) {
			auto layout = packed_record_layout(record);
			if (!layout) {
				raise_type_error("cannot determine packed variant record "
				                 "initializer layout",
				                 record);
			}
			return parse_variant_record(record->children, *layout);
		}
		return parse_record(record->fields);
	}

	Node* expr = parse_expression();
	if (dynamic_cast<BracketLiteral*>(expr) && dynamic_cast<FixedSetType*>(ty)) {
		// An empty `[]` has no element type of its own. Storage
		// initializers already supply the exact destination type, so commit
		// bracket syntax to that set before constant evaluation just as
		// ordinary assignment/argument matching does.
		expr = cast(expr, ty);
	} else if (dynamic_cast<RoutineRef*>(expr)) {
		// `@Routine` is contextual: the destination selects an overload and
		// fixes plain-routine versus method representation. Perform that
		// operator resolution before asking whether the resulting address is
		// a constant.
		expr = cast(expr, ty);
	}
	ConstEvalContext ctx;
	ConstEvalResult folded = expr->const_eval(ctx);
	if (folded.kind == ConstEvalResult::Kind::NotConstant) {
		raise_value_error("constant expression expected", expr);
	} else if (folded.kind == ConstEvalResult::Kind::Error) {
		raise_value_error(folded.message, expr);
	}
	Node* value = folded.node;

	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		OrdinalRange range;
		std::string error;
		if (!ordinal_range_for_type(s, &range, &error)) {
			raise_type_error(error, s);
		}
		auto bound = classify_subrange_bound(value, &error);
		if (!bound) {
			raise_value_error(error, value);
		}
		if (!ordinal_constant_matches_range_type(range.base_type, *bound)) {
			raise_type_mismatch("constant initializer has incompatible ordinal type", range.base_type, bound->node ? bound->node->ty : nullptr);
		}
		if (compare_ordinal_value(bound->ordinal_value, range.lower_ordinal) < 0 || compare_ordinal_value(bound->ordinal_value, range.upper_ordinal) > 0) {
			raise_type_error("constant initializer out of range for target type", s);
		}
		// The constant has already been proven inside this exact declaration's
		// bounds. Retain the SubrangeType on the initializer so the emitter
		// constructs its concrete carrier instead of initializing that carrier
		// from an unrelated base-type C++ value.
		return new Cast(bound->node, s);
	}

	Node* converted = cast_for_destination(value, ty);
	ConstEvalResult checked = converted->const_eval(ctx);
	if (checked.kind == ConstEvalResult::Kind::Error) {
		raise_value_error(checked.message, converted);
	}
	if (checked.kind == ConstEvalResult::Kind::NotConstant) {
		raise_value_error("constant expression expected", converted);
	}
	return checked.node;
}

void Parser::parse_const_block(Type* aggregate_owner) {
	parse_keyword("const");
	// See parse_var_block: register into the enclosing decl scope, no sub-frame.
	Frame* scope = current_declaration_frame();
	do {
		auto name = parse_identifier();
		if (maybe_parse_equal()) {
			Node* expr = parse_expression();
			if (maybe_parse_directive("deprecated")) {
				// TODO: Store deprecated-ness.
				// parse_expression();
				parse_string_literal();
			}
			ConstEvalContext ctx;
			ConstEvalResult folded = expr->const_eval(ctx);
			if (folded.kind == ConstEvalResult::Kind::NotConstant) {
				raise_value_error("constant expression expected", expr);
			}
			if (folded.kind == ConstEvalResult::Kind::Error) {
				raise_value_error(folded.message, expr);
			}
			if (aggregate_owner) {
				auto constant = new ConstantDecl(cxx_value_name(name), folded.node ? folded.node->ty : nullptr, folded.node, aggregate_owner);
				if (!scope->register_variable(name, constant, constant->ty)) {
					raise_parse_error("duplicate identifier: " + name);
				}
			} else {
				if (!scope->register_variable(name, folded.node, folded.node ? folded.node->ty : nullptr)) {
					raise_parse_error("duplicate identifier: " + name);
				}
			}
			parse_semicolon();
			continue;
		}
		parse_colon();
		auto ty = parse_type_expression(false);
		if (aggregate_owner) {
			if (!maybe_parse_equal()) {
				raise_type_error("aggregate storage declaration requires an initializer", ty);
			}
			Node* initializer = parse_storage_initializer(ty);
			auto slot = new StorageSlot(cxx_value_name(name), ty, StorageSlot::Kind::StaticMember, aggregate_owner);
			slot->initializer = initializer;
			if (!scope->register_variable(name, slot, ty)) {
				raise_parse_error("duplicate identifier: " + name);
			}
			parse_semicolon();
			continue;
		}
		auto slot = new StorageSlot(cxx_value_name(name), ty);
		slot->owning_unit = declaration_unit(scope);
		if (!scope->register_variable(name, slot, ty)) {
			raise_parse_error("duplicate identifier: " + name);
		}
		if (maybe_parse_equal()) {
			Node* initializer = parse_storage_initializer(ty);
			if (emitter) {
				emitter->emit_initialized_storage_decl(slot->cxx_name, ty, initializer, current_routine != nullptr);
			}
		}
		parse_semicolon();
	} while (input_token.size() && keywords.find(input_token) == keywords.end());
}

void Parser::maybe_parse_const_block() {
	if (peek_keyword("const")) {
		parse_const_block();
	}
}

/** Canonicalize the temporary graph built by one Pascal type block.
 *
 * normalize_type() mutates every stored Type* edge it visits, including edges
 * reached through aggregate frames, overload collections, properties, and
 * mutually recursive type constructors. Its successful completion is the
 * boundary after which raw Type* identity and backend-carrier relations are
 * semantic. It does not make the graph canonical incrementally while parsing;
 * code which needs those relations must be scheduled after this resolver, not
 * taught to unwrap one convenient edge on demand. */
struct TypeBlockResolver {
	std::unordered_set<Type*> visiting_types;
	std::unordered_set<Type*> done_types;
	std::unordered_set<IncompleteType*> resolving_incomplete;
	std::unordered_set<Node*> done_nodes;
	std::unordered_set<Frame*> done_frames;
	std::unordered_map<Type*, std::size_t> completing_types;
	std::vector<Type*> completion_path;
	std::unordered_set<Type*> completed_types;
	std::unordered_set<Type*> validating_storage;
	std::unordered_set<Type*> validated_storage;
	std::unordered_set<Type*> validated_definitions;
	std::string error;
	std::string validating_type_name;

	bool fail(std::string message) {
		error = std::move(message);
		return false;
	}

	bool normalize_type(Type*& ty) {
		if (!ty) {
			return true;
		}
		if (auto inc = dynamic_cast<IncompleteType*>(ty)) {
			if (!inc->resolved) {
				return fail("forward-referenced type not defined in this type block: " + inc->name);
			}
			if (!resolving_incomplete.insert(inc).second) {
				// A class is already a reference-shaped Pascal type, so
				// returning or accepting the class currently being defined
				// does not recursively embed its C++ object. Keep rejecting
				// the corresponding by-value record cycle.
				if (dynamic_cast<ClassType*>(inc->resolved) || dynamic_cast<InterfaceType*>(inc->resolved)) {
					ty = inc->resolved;
					return true;
				}
				return fail("cyclic forward type reference involving: " + inc->name);
			}
			Type* resolved = inc->resolved;
			if (!normalize_type(resolved)) {
				return false;
			}
			resolving_incomplete.erase(inc);
			ty = resolved;
			return true;
		}
		if (done_types.count(ty)) {
			return true;
		}
		if (visiting_types.count(ty)) {
			return true;
		}

		visiting_types.insert(ty);
		bool ok = normalize_type_contents(ty);
		visiting_types.erase(ty);
		if (ok) {
			done_types.insert(ty);
		}
		return ok;
	}

	template <typename T> bool normalize_type_as(T*& ty, const char* role) {
		if (!ty) {
			return true;
		}
		Type* resolved = ty;
		if (!normalize_type(resolved)) {
			return false;
		}
		if (auto typed = dynamic_cast<T*>(resolved)) {
			ty = typed;
			return true;
		}
		return fail(std::string(role) + " resolved to " + (resolved ? resolved->diagnostic_kind() : "<null>"));
	}

	bool normalize_node(Node* node) {
		if (!node) {
			return true;
		}
		if (!done_nodes.insert(node).second) {
			return true;
		}
		if (!normalize_type(node->ty)) {
			return false;
		}

		// Normalize inherited CST edges once, independently of concrete-node
		// dispatch. Cast/Coerce are UnaryOperation nodes, Assign/MemberAccess
		// are BinaryOperation nodes, and Method is a Callable; treating these
		// base classes as competing alternatives would skip part of the tree.
		if (auto unary = dynamic_cast<UnaryOperation*>(node)) {
			if (!normalize_node(unary->a)) {
				return false;
			}
		}
		if (auto binary = dynamic_cast<BinaryOperation*>(node)) {
			if (!normalize_node(binary->a) || !normalize_node(binary->b)) {
				return false;
			}
		}
		if (auto callable = dynamic_cast<Callable*>(node)) {
			Type* routine_type = callable->ty;
			if (!normalize_type(routine_type)) {
				return false;
			}
			auto normalized_routine = dynamic_cast<RoutineType*>(routine_type);
			if (!normalized_routine) {
				return fail("callable type resolved to non-routine type");
			}
			callable->ty = normalized_routine;
			if (auto method = dynamic_cast<Method*>(callable)) {
				if (!normalize_type(method->owner_class)) {
					return false;
				}
			}
		}

		if (auto n = dynamic_cast<Block*>(node)) {
			for (Node* statement : n->statements) {
				if (!normalize_node(statement)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<TypeBound*>(node)) {
			if (!normalize_type(n->operand_type)) {
				return false;
			}
			n->ty = n->operand_type;
		} else if (auto n = dynamic_cast<SizeOf*>(node)) {
			if (!normalize_type(n->operand_type)) {
				return false;
			}
			n->ty = sizeint_type();
		} else if (auto n = dynamic_cast<FixedArrayLiteral*>(node)) {
			for (Node* element : n->elements) {
				if (!normalize_node(element)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<ArrayLiteral*>(node)) {
			for (Node* element : n->elements) {
				if (!normalize_node(element)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<BracketLiteral*>(node)) {
			if (!normalize_type(n->default_set_item_type) || !normalize_type_as(n->default_array_type, "bracket default array")) {
				return false;
			}
			for (auto& item : n->items) {
				if (!normalize_node(item.lower) || !normalize_node(item.upper)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<RecordLiteral*>(node)) {
			for (auto& field : n->fields) {
				if (!normalize_node(field.slot) || !normalize_node(field.value)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<SetLiteral*>(node)) {
			for (auto& item : n->items) {
				if (!normalize_node(item.lower) || !normalize_node(item.upper)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<Construct*>(node)) {
			if (!normalize_node(n->class_reference) || !normalize_node(n->initializer)) {
				return false;
			}
			for (Node* arg : n->args) {
				if (!normalize_node(arg)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<NewValue*>(node)) {
			if (!normalize_type(n->allocated_type) || !normalize_node(n->initializer)) {
				return false;
			}
			for (Node* arg : n->args) {
				if (!normalize_node(arg)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<DisposeValue*>(node)) {
			if (!normalize_node(n->pointer) || !normalize_node(n->finalizer)) {
				return false;
			}
		} else if (auto n = dynamic_cast<ConstantDecl*>(node)) {
			if (!normalize_type(n->owner_type) || !normalize_node(n->initializer)) {
				return false;
			}
		} else if (auto n = dynamic_cast<StorageSlot*>(node)) {
			if (!normalize_type(n->owner_type) || !normalize_node(n->initializer)) {
				return false;
			}
		} else if (auto n = dynamic_cast<Property*>(node)) {
			for (Type*& index_type : n->index_types) {
				if (!normalize_type(index_type)) {
					return false;
				}
			}
			if (!normalize_node(n->read_accessor) || !normalize_node(n->write_accessor)) {
				return false;
			}
		} else if (auto n = dynamic_cast<PropertyAccess*>(node)) {
			if (!normalize_node(n->receiver) || !normalize_node(n->property)) {
				return false;
			}
			for (Node* index : n->indexes) {
				if (!normalize_node(index)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<Coerce*>(node)) {
			if (!normalize_type(n->target_type)) {
				return false;
			}
			n->ty = n->target_type;
		} else if (auto n = dynamic_cast<CoerceCheck*>(node)) {
			if (!normalize_type(n->target_type)) {
				return false;
			}
		} else if (auto n = dynamic_cast<RoutineRef*>(node)) {
			if (!normalize_node(n->receiver) || !normalize_node(n->candidates) || !normalize_node(n->resolved)) {
				return false;
			}
		} else if (auto n = dynamic_cast<ProcCall*>(node)) {
			if (!normalize_node(n->receiver) || !normalize_node(n->callee)) {
				return false;
			}
			for (auto* arg : n->args) {
				if (!normalize_node(arg)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<ClassRefValue*>(node)) {
			Type* target = n->target;
			if (!normalize_type(target)) {
				return false;
			}
			n->target = dynamic_cast<ClassType*>(target);
			if (!n->target) {
				return fail("class-reference value target resolved to non-class type");
			}
		} else if (auto n = dynamic_cast<TypeMemberQualifier*>(node)) {
			if (!normalize_type(n->target)) {
				return false;
			}
			if (!dynamic_cast<RecordType*>(n->target) && !dynamic_cast<PackedRecordType*>(n->target)) {
				return fail("type member qualifier target resolved to "
				            "non-record type");
			}
			n->ty = n->target;
		} else if (auto n = dynamic_cast<WriteCall*>(node)) {
			if (!normalize_node(n->file)) {
				return false;
			}
			for (auto& item : n->items) {
				if (!normalize_node(item.value) || !normalize_node(item.width) || !normalize_node(item.precision)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<StrCall*>(node)) {
			if (!normalize_node(n->formatted.value) || !normalize_node(n->formatted.width) || !normalize_node(n->formatted.precision) || !normalize_node(n->destination)) {
				return false;
			}
		} else if (auto n = dynamic_cast<ValCall*>(node)) {
			if (!normalize_node(n->source) || !normalize_node(n->destination) || !normalize_node(n->code)) {
				return false;
			}
		} else if (auto n = dynamic_cast<InheritedCall*>(node)) {
			if (!normalize_node(n->resolved)) {
				return false;
			}
			for (auto* arg : n->args) {
				if (!normalize_node(arg)) {
					return false;
				}
			}
		} else if (auto n = dynamic_cast<OverloadSet*>(node)) {
			for (auto* member : n->members) {
				if (!normalize_node(member)) {
					return false;
				}
			}
		}
		return true;
	}

	bool normalize_frame(Frame* frame) {
		if (!frame) {
			return true;
		}
		if (!done_frames.insert(frame).second) {
			return true;
		}

		std::vector<std::pair<std::string, Type*>> local_types;
		for (const auto& item : frame->type_declarations()) {
			local_types.push_back(item);
		}
		for (auto& item : local_types) {
			Type* ty = item.second;
			if (!normalize_type(ty)) {
				return false;
			}
			frame->rebind_type(item.first, ty);
		}

		std::vector<std::pair<std::string, FrameValueEntry>> local_values;
		for (const auto& item : frame->value_declarations()) {
			local_values.push_back(item);
		}
		for (auto& item : local_values) {
			Node* value = item.second.value;
			if (!normalize_node(value)) {
				return false;
			}
			Type* ty = item.second.ty;
			if (!normalize_type(ty)) {
				return false;
			}
			if (auto slot = dynamic_cast<StorageSlot*>(value)) {
				ty = slot->ty;
			} else if (auto call = dynamic_cast<Callable*>(value)) {
				ty = call->ty ? call->ty->return_type : nullptr;
			} else if (value && value->ty) {
				ty = value->ty;
			}
			frame->rebind_value_type(item.first, ty);
		}
		return true;
	}

	bool normalize_variant(VariantPart* variant) {
		if (!variant) {
			return true;
		}
		if (!normalize_type(variant->selector_type) || !normalize_node(variant->selector_slot)) {
			return false;
		}
		if (variant->selector_slot) {
			variant->selector_slot->ty = variant->selector_type;
		}
		for (auto& arm : variant->arms) {
			for (auto& field : arm.fields) {
				if (!normalize_type(field.ty) || !normalize_node(field.slot)) {
					return false;
				}
				if (field.slot) {
					field.slot->ty = field.ty;
				}
			}
			if (!normalize_variant(arm.variant)) {
				return false;
			}
		}
		return true;
	}

	bool normalize_type_contents(Type* ty) {
		// A source-declared default property is also present in the aggregate
		// frame, but the Type pointer is a semantic edge in its own right.
		// Visiting it here keeps normalization exhaustive even for lazily
		// synthesized/default-property representations.
		if (!normalize_node(ty->default_property)) {
			return false;
		}
		if (dynamic_cast<IntrinsicType*>(ty) || dynamic_cast<ShortStringType*>(ty) || dynamic_cast<UnitType*>(ty) || dynamic_cast<UntypedIntegerType*>(ty) || dynamic_cast<EnumType*>(ty)) {
			return true;
		} else if (auto distinct = dynamic_cast<DistinctType*>(ty)) {
			return normalize_type(distinct->base_type);
		} else if (auto s = dynamic_cast<SubrangeType*>(ty)) {
			return normalize_type(s->base_type) && normalize_node(s->lower_bound) && normalize_node(s->upper_bound);
		} else if (auto a = dynamic_cast<FixedArrayType*>(ty)) {
			if (!normalize_type(a->bounds) || !normalize_type(a->item_type)) {
				return false;
			}
			std::string range_error;
			OrdinalRange range;
			if (!ordinal_range_for_type(a->bounds, &range, &range_error)) {
				return fail(range_error);
			}
			a->range = range;
			return normalize_type(a->range.index_type) && normalize_type(a->range.base_type) && normalize_node(a->range.lower_bound) && normalize_node(a->range.upper_bound);
		} else if (auto a = dynamic_cast<DynamicArrayType*>(ty)) {
			return normalize_type(a->item_type);
		} else if (auto a = dynamic_cast<OpenArrayType*>(ty)) {
			return normalize_type(a->item_type);
		} else if (auto s = dynamic_cast<FixedSetType*>(ty)) {
			return normalize_type(s->item_type);
		} else if (auto f = dynamic_cast<TypedFileType*>(ty)) {
			return normalize_type(f->item_type);
		} else if (auto p = dynamic_cast<PointerType*>(ty)) {
			return p->is_untyped() || normalize_type(p->item_type);
		} else if (auto r = dynamic_cast<ClassRefType*>(ty)) {
			if (!normalize_type(r->target)) {
				return false;
			}
			if (!dynamic_cast<ClassType*>(r->target)) {
				return fail("'class of' target resolved to " + std::string(r->target ? r->target->diagnostic_kind() : "<null>"));
			}
			return true;
		} else if (auto rt = dynamic_cast<RoutineType*>(ty)) {
			if (!normalize_type(rt->return_type)) {
				return false;
			}
			for (auto& formal : rt->formals) {
				if (!normalize_type(formal.ty) || !normalize_node(formal.default_value)) {
					return false;
				}
			}
			return true;
		} else if (auto r = dynamic_cast<RecordType*>(ty)) {
			if (!normalize_frame(r->children) || !normalize_variant(r->variant)) {
				return false;
			}
			for (auto& field : r->fields) {
				if (!normalize_type(field.ty)) {
					return false;
				}
				if (field.slot) {
					field.slot->ty = field.ty;
				}
				if (!normalize_node(field.slot)) {
					return false;
				}
			}
			return true;
		} else if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
			if (!normalize_frame(r->children) || !normalize_variant(r->variant)) {
				return false;
			}
			for (auto& field : r->fields) {
				if (!normalize_type(field.ty)) {
					return false;
				}
				if (field.slot) {
					field.slot->ty = field.ty;
				}
				if (!normalize_node(field.slot)) {
					return false;
				}
			}
			return true;
		} else if (auto c = dynamic_cast<ClassType*>(ty)) {
			if (!normalize_type_as(c->super, "class superclass")) {
				return false;
			}
			for (auto*& iface : c->implemented_interfaces) {
				if (!normalize_type_as(iface, "implemented interface")) {
					return false;
				}
			}
			return normalize_node(c->class_constructor) && normalize_node(c->class_destructor) && normalize_frame(c->children);
		} else if (auto i = dynamic_cast<InterfaceType*>(ty)) {
			for (auto*& iface : i->super_interfaces) {
				if (!normalize_type_as(iface, "interface ancestor")) {
					return false;
				}
			}
			return normalize_frame(i->children);
		} else if (auto o = dynamic_cast<ObjectType*>(ty)) {
			if (!normalize_type_as(o->super, "object superclass")) {
				return false;
			}
			return normalize_frame(o->children);
		} else if (auto m = dynamic_cast<ModuleType*>(ty)) {
			return normalize_frame(m->children);
		}
		return true;
	}

	static bool nominal_type_anchor(Type* ty) {
		return dynamic_cast<RecordType*>(ty) || dynamic_cast<PackedRecordType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<InterfaceType*>(ty) || dynamic_cast<ObjectType*>(ty);
	}

	bool validate_complete_frame(Frame* frame) {
		if (!frame) {
			return true;
		}
		for (const auto& item : frame->value_declarations()) {
			auto slot = dynamic_cast<StorageSlot*>(item.second.value);
			if (!slot || slot->kind != StorageSlot::Kind::AggregateMember) {
				continue;
			}
			if (!validate_complete_type(slot->ty)) {
				return false;
			}
		}
		return true;
	}

	bool validate_complete_type(Type* ty) {
		if (!ty || completed_types.count(ty)) {
			return true;
		}
		if (auto found = completing_types.find(ty); found != completing_types.end()) {
			// A legal recursive definition must be anchored by a nominal
			// aggregate. `P = ^R; R = record Next: P end` has the Record
			// identity that completes P's target. Equations made solely from
			// constructors (`T = ^T`, `file of T`, recursive routine aliases,
			// and mutual aliases) never define the promised target type.
			for (std::size_t i = found->second; i < completion_path.size(); ++i) {
				if (nominal_type_anchor(completion_path[i])) {
					return true;
				}
			}
			return fail("type '" + validating_type_name + "' is not completely defined");
		}

		completing_types.emplace(ty, completion_path.size());
		completion_path.push_back(ty);
		bool ok = true;
		if (auto array = dynamic_cast<FixedArrayType*>(ty)) {
			ok = validate_complete_type(array->bounds) && validate_complete_type(array->item_type);
		} else if (auto array = dynamic_cast<DynamicArrayType*>(ty)) {
			ok = validate_complete_type(array->item_type);
		} else if (auto array = dynamic_cast<OpenArrayType*>(ty)) {
			ok = validate_complete_type(array->item_type);
		} else if (auto set = dynamic_cast<FixedSetType*>(ty)) {
			ok = validate_complete_type(set->item_type);
		} else if (auto file = dynamic_cast<TypedFileType*>(ty)) {
			ok = validate_complete_type(file->item_type);
		} else if (auto pointer = dynamic_cast<PointerType*>(ty)) {
			ok = pointer->is_untyped() || validate_complete_type(pointer->item_type);
		} else if (auto class_ref = dynamic_cast<ClassRefType*>(ty)) {
			ok = validate_complete_type(class_ref->target);
		} else if (auto routine = dynamic_cast<RoutineType*>(ty)) {
			ok = validate_complete_type(routine->return_type);
			for (const auto& formal : routine->formals) {
				if (ok && !validate_complete_type(formal.ty)) {
					ok = false;
				}
			}
		} else if (auto subrange = dynamic_cast<SubrangeType*>(ty)) {
			ok = validate_complete_type(subrange->base_type);
		} else if (auto distinct = dynamic_cast<DistinctType*>(ty)) {
			ok = validate_complete_type(distinct->base_type);
		} else if (auto record = dynamic_cast<RecordType*>(ty)) {
			ok = validate_complete_frame(record->children);
		} else if (auto record = dynamic_cast<PackedRecordType*>(ty)) {
			ok = validate_complete_frame(record->children);
		} else if (auto class_type = dynamic_cast<ClassType*>(ty)) {
			ok = validate_complete_type(class_type->super);
			for (InterfaceType* iface : class_type->implemented_interfaces) {
				if (ok && !validate_complete_type(iface)) {
					ok = false;
				}
			}
			if (ok) {
				ok = validate_complete_frame(class_type->children);
			}
		} else if (auto interface_type = dynamic_cast<InterfaceType*>(ty)) {
			for (InterfaceType* iface : interface_type->super_interfaces) {
				if (ok && !validate_complete_type(iface)) {
					ok = false;
				}
			}
			if (ok) {
				ok = validate_complete_frame(interface_type->children);
			}
		} else if (auto object = dynamic_cast<ObjectType*>(ty)) {
			ok = validate_complete_type(object->super) && validate_complete_frame(object->children);
		} else if (auto module = dynamic_cast<ModuleType*>(ty)) {
			ok = validate_complete_frame(module->children);
		}

		completion_path.pop_back();
		completing_types.erase(ty);
		if (ok) {
			completed_types.insert(ty);
		}
		return ok;
	}

	bool validate_aggregate_storage(Frame* frame) {
		if (!frame) {
			return true;
		}
		for (const auto& item : frame->value_declarations()) {
			auto slot = dynamic_cast<StorageSlot*>(item.second.value);
			if (!slot || slot->kind != StorageSlot::Kind::AggregateMember) {
				continue;
			}
			if (!validate_storage_type(slot->ty)) {
				return false;
			}
		}
		return true;
	}

	bool validate_storage_type(Type* ty) {
		if (!ty) {
			return true;
		}

		// These carriers do not contain an object of the referenced type.
		// Recursion through them is therefore finite: a pointer/class value,
		// routine value, set, or file handle has a fixed representation
		// independent of the referent/element definition.
		if (dynamic_cast<PointerType*>(ty) || dynamic_cast<ClassType*>(ty) || dynamic_cast<InterfaceType*>(ty) || dynamic_cast<ClassRefType*>(ty) || dynamic_cast<RoutineType*>(ty) || dynamic_cast<DynamicArrayType*>(ty) || dynamic_cast<FixedSetType*>(ty) || dynamic_cast<TypedFileType*>(ty)) {
			return true;
		}

		if (validated_storage.count(ty)) {
			return true;
		}
		if (!validating_storage.insert(ty).second) {
			return fail("type '" + validating_type_name + "' contains itself by value");
		}

		bool ok = true;
		if (auto array = dynamic_cast<FixedArrayType*>(ty)) {
			ok = validate_storage_type(array->item_type);
		} else if (auto record = dynamic_cast<RecordType*>(ty)) {
			ok = validate_aggregate_storage(record->children);
		} else if (auto record = dynamic_cast<PackedRecordType*>(ty)) {
			ok = validate_aggregate_storage(record->children);
		} else if (auto object = dynamic_cast<ObjectType*>(ty)) {
			ok = validate_aggregate_storage(object->children);
		}

		validating_storage.erase(ty);
		if (ok) {
			validated_storage.insert(ty);
		}
		return ok;
	}

	bool validate_definition(Type* ty, const std::string& name) {
		if (!ty || !validated_definitions.insert(ty).second) {
			return true;
		}
		validating_type_name = name;
		if (!validate_complete_type(ty)) {
			return false;
		}
		if (auto packed = dynamic_cast<PackedRecordType*>(ty); packed && packed->has_managed_lifetime()) {
			return fail("packed record type '" + name + "' contains managed storage");
		}

		// A class/interface value is a reference, but its declaration still
		// owns fields which must themselves have finite storage. Inspect the
		// definition once; when the same class type is encountered as a field,
		// validate_storage_type correctly stops at the reference carrier.
		if (auto class_type = dynamic_cast<ClassType*>(ty)) {
			return validate_aggregate_storage(class_type->children);
		} else if (auto interface_type = dynamic_cast<InterfaceType*>(ty)) {
			return validate_aggregate_storage(interface_type->children);
		}
		return validate_storage_type(ty);
	}
};

/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually.

DELPHI_AUTO_END: will automatically stop at some aggregate control directives (like "public" etc).
 */
// Special cases this handles:
//   type PX = ^TX; TX = record ... end;        (cross-decl forward via pointer)
//   type TMeta = class of TObject; TObject = class ... end;
// Pascal grants implicit forward-reference latitude to pointer targets and
// `class of` targets inside a type block. Arrays, sets, files, aliases, and
// other by-value constructions still require their element/target type to
// have been declared already.
// The parser therefore opens a narrow placeholder-creation window while
// parsing this block, then closes it before semantic normalization/emission.
// No emitter path should need to unwrap IncompleteType as normal control flow.
void Parser::parse_type_block(bool delphi_auto_end) {
	parse_keyword("type");
	// Type blocks declare directly in the explicit declaration owner. Lookup
	// overlays (`uses`, `with`, implicit Self) cannot redirect these bindings.
	Frame* scope = current_declaration_frame();

	struct PendingTypeDecl {
		enum class Kind {
			Definition,
			ClassForward,
		};
		std::string name;
		std::string cxx;
		IncompleteType* lhs_placeholder;
		Type* rhs;
		Kind kind;
		bool needs_cxx_forward = false;
		bool alias = false;
	};

	std::vector<PendingTypeDecl> pending;
	type_block_frames.push_back(scope);
	type_block_deferred_aggregates.emplace_back();
	do {
		auto name_optional = maybe_parse_identifier();
		if (!name_optional) {
			break;
		}
		auto name = *name_optional;
		parse_equals();
		auto existing_binding = scope->lookup_type_or_value_local(name);
		Type* existing = nullptr;
		if (existing_binding) {
			auto existing_type = std::get_if<Type*>(&*existing_binding);
			if (!existing_type) {
				raise_value_error("duplicate identifier: " + name, std::get<Node*>(*existing_binding));
			}
			existing = *existing_type;
		}
		IncompleteType* lhs_placeholder = nullptr;
		ClassType* completing_forward = nullptr;
		bool needs_cxx_forward = false;
		if (existing) {
			lhs_placeholder = dynamic_cast<IncompleteType*>(existing);
			needs_cxx_forward = lhs_placeholder && !lhs_placeholder->resolved;
			completing_forward = dynamic_cast<ClassType*>(existing);
			if (completing_forward && !completing_forward->is_forward_declaration) {
				completing_forward = nullptr;
			}
			if ((!lhs_placeholder && !completing_forward) || (lhs_placeholder && lhs_placeholder->resolved)) {
				raise_type_error("duplicate type name: " + name, existing);
			}
		} else {
			lhs_placeholder = new IncompleteType(current_location(), name);
			if (!scope->register_type(name, lhs_placeholder)) {
				raise_type_parse_error("duplicate identifier: " + name);
			}
		}
		std::string saved_type_declaration_name = std::move(current_type_declaration_name);
		current_type_declaration_name = name;
		Type* rhs = nullptr;
		const SourceLocation rhs_location = current_location();
		const bool distinct_definition = maybe_parse_keyword("type");
		if (completing_forward && (distinct_definition || !peek_keyword("class"))) {
			raise_type_error("duplicate type name: " + name, completing_forward);
		}
		if (!distinct_definition && peek_keyword("class")) {
			rhs = parse_class_type(completing_forward, true);
		} else {
			rhs = parse_type_expression(false);
			if (distinct_definition) {
				rhs = new DistinctType(rhs_location, cxx_type_name(name), rhs);
			}
		}
		current_type_declaration_name = std::move(saved_type_declaration_name);
		auto forward_class = dynamic_cast<ClassType*>(rhs);
		if (forward_class && forward_class->is_forward_declaration) {
			// Unlike an implicit same-block IncompleteType, an explicit class
			// forward remains usable across later declaration sections. Give
			// it stable class identity and its emitted name immediately.
			forward_class->cxx_name = cxx_type_name(name);
			forward_class->forward_name = name;
			forward_class->owning_unit = declaration_unit(scope);
			if (lhs_placeholder) {
				lhs_placeholder->resolved = forward_class;
			}
			scope->rebind_type(name, forward_class);
			pending.push_back(PendingTypeDecl{name, cxx_type_name(name), nullptr, forward_class, PendingTypeDecl::Kind::ClassForward, needs_cxx_forward});
		} else {
			// Publish a completed declaration before parsing the next one.
			// References already holding this placeholder remain valid and are
			// normalized at block end; fresh lookups peel the resolved
			// placeholder. This is essential for inheritance, whose parser
			// needs the earlier class body immediately.
			if (lhs_placeholder) {
				lhs_placeholder->resolved = rhs;
			}
			pending.push_back(PendingTypeDecl{name, cxx_type_name(name), lhs_placeholder, rhs, PendingTypeDecl::Kind::Definition, needs_cxx_forward});
		}
		parse_semicolon();
	} while (true);
	std::vector<Frame*> deferred_aggregates = std::move(type_block_deferred_aggregates.back());
	type_block_deferred_aggregates.pop_back();
	type_block_frames.pop_back();

	// PHASE INVARIANT:
	//
	// While the parse loop above is open, the graph intentionally has mixed
	// representation: an earlier stored edge can still be
	// IncompleteType(T), while a fresh lookup peels its now-published
	// resolution and returns T. That phase may collect declarations and apply
	// syntax/arity rules, but it must not decide raw Type* identity, overload
	// signatures, property contracts, C++ carriers, or overriding.
	//
	// Normalize every reachable edge first. Only then validate definitions and
	// the deferred aggregate semantics. Emission is last. Keeping these three
	// phases explicit prevents a new parser consumer from treating the
	// temporary graph as the final Pascal type algebra.
	TypeBlockResolver resolver;
	for (auto& decl : pending) {
		if (decl.kind == PendingTypeDecl::Kind::ClassForward) {
			continue;
		}
		if (!resolver.normalize_type(decl.rhs)) {
			raise_type_error(resolver.error, decl.rhs);
		}
		if (decl.lhs_placeholder) {
			decl.lhs_placeholder->resolved = decl.rhs;
		}
	}
	for (auto& decl : pending) {
		if (decl.kind == PendingTypeDecl::Kind::ClassForward) {
			continue;
		}
		if (!resolver.validate_definition(decl.rhs, decl.name)) {
			raise_type_error(resolver.error, decl.rhs);
		}
	}
	for (auto& decl : pending) {
		if (decl.kind == PendingTypeDecl::Kind::Definition) {
			scope->rebind_type(decl.name, decl.rhs);
		}
	}

	for (Frame* aggregate : deferred_aggregates) {
		validate_aggregate_declaration_semantics(aggregate);
	}

	for (auto& decl : pending) {
		if (decl.kind == PendingTypeDecl::Kind::ClassForward) {
			continue;
		}
		Type* rhs = decl.rhs;
		// Attach the LHS Pascal name (as its C++ identifier) to record-family
		// types and enums so emit_type_ref has a name to spell instead of
		// re-emitting the body inline at every use site.
		// If rhs is an aggregate/enum that already carries a C++ name, this
		// binding is an alias (`type B = A;` where A was defined above). In
		// that case do NOT overwrite rhs->cxx_name (that would rename A's
		// canonical definition) and do NOT re-emit the body (ODR violation).
		// Emit a `using t_B = t_A;` instead. First-wins for the canonical
		// name; subsequent Pascal names become C++ aliases.
		std::string existing_cxx;
		if (auto r = dynamic_cast<RecordType*>(rhs)) {
			existing_cxx = r->cxx_name;
		} else if (auto r = dynamic_cast<PackedRecordType*>(rhs)) {
			existing_cxx = r->cxx_name;
		} else if (auto c = dynamic_cast<ClassType*>(rhs)) {
			existing_cxx = c->cxx_name;
		} else if (auto c = dynamic_cast<ClassRefType*>(rhs)) {
			existing_cxx = c->cxx_name;
		} else if (auto c = dynamic_cast<InterfaceType*>(rhs)) {
			existing_cxx = c->cxx_name;
		} else if (auto o = dynamic_cast<ObjectType*>(rhs)) {
			existing_cxx = o->cxx_name;
		} else if (auto e = dynamic_cast<EnumType*>(rhs)) {
			existing_cxx = e->cxx_name;
		} else if (auto s = dynamic_cast<SubrangeType*>(rhs)) {
			existing_cxx = s->cxx_name;
		} else if (auto d = dynamic_cast<DistinctType*>(rhs)) {
			existing_cxx = d->cxx_name;
		}
		if (!existing_cxx.empty() && existing_cxx != decl.cxx) {
			decl.alias = true;
		} else {
			// The first source name owns the canonical emitted definition.
			// Aliases retain their target's owner instead of moving that
			// declaration into the aliasing unit.
			const bool has_named_definition = dynamic_cast<RecordType*>(rhs) || dynamic_cast<PackedRecordType*>(rhs) || dynamic_cast<ClassType*>(rhs) || dynamic_cast<ClassRefType*>(rhs) || dynamic_cast<InterfaceType*>(rhs) || dynamic_cast<ObjectType*>(rhs) || dynamic_cast<EnumType*>(rhs) || dynamic_cast<SubrangeType*>(rhs) || dynamic_cast<DistinctType*>(rhs);
			if (has_named_definition && !rhs->owning_unit) {
				rhs->owning_unit = declaration_unit(scope);
			}
			if (auto r = dynamic_cast<RecordType*>(rhs)) {
				r->cxx_name = decl.cxx;
			} else if (auto r = dynamic_cast<PackedRecordType*>(rhs)) {
				r->cxx_name = decl.cxx;
			} else if (auto c = dynamic_cast<ClassType*>(rhs)) {
				c->cxx_name = decl.cxx;
			} else if (auto c = dynamic_cast<ClassRefType*>(rhs)) {
				c->cxx_name = decl.cxx;
			} else if (auto c = dynamic_cast<InterfaceType*>(rhs)) {
				c->cxx_name = decl.cxx;
			} else if (auto o = dynamic_cast<ObjectType*>(rhs)) {
				o->cxx_name = decl.cxx;
			} else if (auto e = dynamic_cast<EnumType*>(rhs)) {
				e->cxx_name = decl.cxx;
			} else if (auto s = dynamic_cast<SubrangeType*>(rhs)) {
				s->cxx_name = decl.cxx;
			} else if (auto d = dynamic_cast<DistinctType*>(rhs)) {
				d->cxx_name = decl.cxx;
			}
		}
	}
	if (!emitter) {
		return;
	}
	// C++ needs the tag declaration before an earlier Pascal pointer field
	// can name a later aggregate from the same type block. Emit all aggregate
	// tags first; by-value recursion has already been rejected above, while
	// legal pointer/class-reference recursion now has exactly the declaration
	// boundary C++20 requires.
	for (auto& decl : pending) {
		if (decl.alias || !decl.needs_cxx_forward) {
			continue;
		}
		if (decl.kind == PendingTypeDecl::Kind::ClassForward || dynamic_cast<RecordType*>(decl.rhs) || dynamic_cast<PackedRecordType*>(decl.rhs) || dynamic_cast<ClassType*>(decl.rhs) || dynamic_cast<InterfaceType*>(decl.rhs) || dynamic_cast<ObjectType*>(decl.rhs)) {
			emitter->emit_class_forward_declaration(decl.cxx);
		}
	}
	for (auto& decl : pending) {
		if (decl.kind == PendingTypeDecl::Kind::ClassForward) {
			emitter->emit_class_forward_declaration(decl.cxx);
		} else if (decl.alias) {
			emitter->emit_type_alias(decl.cxx, decl.rhs);
		} else {
			emitter->emit_type_definition(decl.cxx, decl.rhs);
		}
	}
}

/** Postcondition: this has a side effect of push_scope, so you should do pop_scope eventually */
void Parser::maybe_parse_type_block(bool delphi_auto_end) {
	if (peek_keyword("type")) {
		parse_type_block(delphi_auto_end);
	}
}

void Parser::parse_var_block() {
	parse_keyword("var");
	// Register each var directly into the enclosing declaration scope
	// (unit/program frame or procedure body_frame).
	// We deliberately do NOT create a sub-frame: the var decls must persist
	// past this block parse so callers in other compilation units can resolve
	// them after `uses`.
	Frame* scope = current_declaration_frame();
	do {
		std::vector<std::string> names;
		auto name_optional = maybe_parse_identifier();
		if (!name_optional) {
			break;
		}
		auto name = *name_optional;
		names.push_back(name);
		while (maybe_parse_comma()) {
			auto name = parse_identifier();
			names.push_back(name);
		}
		parse_colon();
		auto ty = parse_type_expression(false);
		std::optional<std::string> external_cxx_name;
		if (maybe_parse_directive("external")) {
			if (names.size() != 1) {
				raise_parse_error("an external variable declaration must have exactly one name");
			}
			parse_directive("name");
			external_cxx_name = parse_string_literal();
		}
		Node* initializer = nullptr;
		if (maybe_parse_equal()) {
			if (external_cxx_name) {
				raise_parse_error("an external variable cannot have an initializer");
			}
			if (names.size() != 1) {
				raise_parse_error("an initialized variable declaration must have exactly one name");
			}
			initializer = cast_for_destination(parse_expression(), ty);
		}
		for (auto iter : names) {
			auto name = iter;
			// External variables name existing C++ storage, so their Pascal
			// declaration registers that name but emits no definition.
			auto slot = new StorageSlot(external_cxx_name ? *external_cxx_name : cxx_value_name(name), ty);
			if (!external_cxx_name) {
				slot->owning_unit = declaration_unit(scope);
			}
			if (!scope->register_variable(name, slot, ty)) {
				raise_parse_error("duplicate identifier: " + name);
			}
			if (emitter && !external_cxx_name) {
				emitter->emit_var_decl(slot->cxx_name, ty, initializer);
			}
		}
		parse_semicolon();
	} while (true);
}

void Parser::maybe_parse_var_block() {
	if (peek_keyword("var")) {
		parse_var_block();
	}
}

bool Parser::maybe_parse_semicolon() {
	if (input_token == ";") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_plus() {
	if (input_token == "+") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_minus() {
	if (input_token == "-") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_star() {
	if (input_token == "*") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_star_star() {
	if (input_token == "**") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_ampersand() {
	if (input_token == "&") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_pipe() {
	if (input_token == "|") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_symdiff() {
	if (input_token == "><") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_slash() {
	if (input_token == "/") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_comma() {
	if (input_token == ",") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_colon() {
	if (input_token == ":") {
		consume();
		return true;
	} else {
		return false;
	}
}

bool Parser::maybe_parse_colon_equals() {
	if (input_token == ":=") {
		consume();
		return true;
	} else {
		return false;
	}
}

void Parser::parse_semicolon() {
	if (!maybe_parse_semicolon()) {
		raise_parse_error("missing semicolon");
	}
}

bool Parser::maybe_parse_opening_paren() {
	if (input_token == "(") {
		consume();
		return true;
	} else {
		return false;
	}
}

void Parser::parse_opening_paren() {
	if (!maybe_parse_opening_paren()) {
		raise_parse_error("missing opening paren");
	}
}

void Parser::parse_closing_paren() {
	if (input_token == ")") {
		consume();
	} else {
		raise_parse_error("missing closing paren");
	}
}

bool Parser::maybe_parse_opening_bracket() {
	if (input_token == "[") {
		consume();
		return true;
	} else {
		return false;
	}
}

void Parser::parse_opening_bracket() {
	if (!maybe_parse_opening_bracket()) {
		raise_parse_error("missing opening bracket");
	}
}

void Parser::parse_closing_bracket() {
	if (input_token == "]") {
		consume();
	} else {
		raise_parse_error("missing closing bracket");
	}
}

void Parser::parse_colon_equals() {
	if (input_token == ":=") {
		consume();
	} else {
		raise_parse_error("missing colon equals");
	}
}

void Parser::parse_colon() {
	if (input_token == ":") {
		consume();
	} else {
		raise_parse_error("missing colon");
	}
}

void Parser::parse_equals() {
	if (input_token == "=") {
		consume();
	} else {
		raise_parse_error("missing equals");
	}
}

void Parser::validate_class_forwards(Frame* frame) {
	if (!frame) {
		return;
	}
	for (const auto& declaration : frame->type_declarations()) {
		auto class_type = dynamic_cast<ClassType*>(declaration.second);
		if (!class_type || !class_type->is_forward_declaration) {
			continue;
		}
		raise_type_error_at(class_type->source_location, "forward class declaration '" + class_type->forward_name + "' was not resolved", class_type);
	}
}

void Parser::parse_decl_blocks(bool is_decl_only) {
	bool is_class = false;
	while (true) {
		if (peek_keyword("label")) {
			parse_label_block();
			is_class = false;
		} else if (peek_keyword("type")) {
			if (is_class) {
				raise_parse_error("'class type' is not supported");
			}
			parse_type_block(false);
			// parse_type_block no longer pushes a sub-frame: it registers into
			// the enclosing decl scope. No push, no pop to account for.
		} else if (maybe_parse_keyword("class")) {
			if (is_class) {
				raise_parse_error("'class' prefix wasn't consumed");
			}
			is_class = true;
		} else if (peek_keyword("const")) {
			if (is_class) {
				raise_parse_error("'class const' is not supported");
			}
			parse_const_block();
		} else if (peek_keyword("var")) {
			if (is_class) {
				raise_parse_error("'class var' is not supported");
			}
			parse_var_block();
		} else if (peek_keyword("procedure")) {
			parse_procedure_or_function(is_class, false, is_decl_only);
			is_class = false;
		} else if (peek_keyword("function")) {
			parse_procedure_or_function(is_class, true, is_decl_only);
			is_class = false;
		} else if (peek_keyword("constructor")) {
			parse_procedure_or_function(is_class, false, is_decl_only);
			is_class = false;
		} else if (peek_keyword("destructor")) {
			parse_procedure_or_function(is_class, false, is_decl_only);
			is_class = false;
		} else if (peek_keyword("operator")) {
			if (is_class) {
				raise_parse_error("'class operator' is not supported");
			}
			parse_procedure_or_function(is_class, true, is_decl_only);
			is_class = false;
		} else {
			break;
		}
	}
	if (is_class) {
		raise_type_parse_error("'class' prefix wasn't consumed");
	}
	// FPC permits a class forward to survive across multiple `type` sections
	// and intervening declarations, but not past the declaration part that
	// owns it. Ordinary implicit IncompleteType references remain confined to
	// one type block and are checked there.
	validate_class_forwards(current_declaration_frame());
}

void Parser::parse_block() {
	parse_decl_blocks(false);
	parse_keyword("begin");
	parse_block_body();
	parse_keyword("end");
}

void Parser::maybe_parse_proc_attributes() {
	// parse_semicolon();
	// TODO: inline
}

std::vector<Parameter> Parser::parse_proc_formal_parameters() {
	std::vector<Parameter> result;
	parse_opening_paren();
	if (input_token != ")") {
		do {
			ParamMode mode = ParamMode::Value;
			if (maybe_parse_keyword("var")) {
				mode = ParamMode::Var;
			} else if (maybe_parse_keyword("out")) {
				mode = ParamMode::Out;
			} else if (maybe_parse_keyword("const")) {
				mode = ParamMode::Const;
			}
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma()) {
				names.push_back(parse_identifier());
			}
			Type* ty = maybe_parse_colon() ? parse_formal_type_expression() : unknown_type();
			Node* default_value = nullptr;
			if (maybe_parse_equal()) {
				if (names.size() > 1) {
					raise_parse_error("default value not allowed with comma-grouped parameter names");
				}
				default_value = parse_expression();
			}
			for (auto& n : names) {
				result.push_back(Parameter{n, cxx_value_name(n), ty, mode, default_value});
			}
		} while (maybe_parse_semicolon());
	}
	parse_closing_paren();
	return result;
}

// is_class is whether there was a "class" prefix token
RoutineType* Parser::parse_routine_signature(bool is_class, bool is_function, bool allow_of_object, RoutineKind kind, Type* owner) {
	std::vector<Parameter> formals;
	if (input_token == "(") {
		formals = parse_proc_formal_parameters();
	}
	Type* ret_ty = &unit_type();
	if (kind == CONSTRUCTOR) {
		if (!dynamic_cast<ClassType*>(owner) && !dynamic_cast<ObjectType*>(owner)) {
			raise_type_kind_mismatch("expected class or object as constructor owner", "class or object", owner);
		}
	} else if (kind == CLASS_CONSTRUCTOR || kind == CLASS_DESTRUCTOR) {
		// Lifecycle hooks neither construct nor destroy an object instance and
		// have no result. Their internal receiver is a metaclass only so their
		// bodies can reuse class-method lowering; they never enter ordinary
		// constructor allocation or object-destructor lowering.
		ret_ty = &unit_type();
	} else if (is_function) {
		parse_colon();
		ret_ty = parse_type_expression(false);
	}

	// A routine TYPE has no category until its late optional `of object`
	// suffix has been consumed. Routine declarations take the other path:
	// their category comes from the declaration grammar and they never accept
	// this suffix.
	if (allow_of_object) {
		if (owner || kind != ROUTINE) {
			raise_type_kind_mismatch("routine type signature", "routine", owner);
		}
		if (peek_keyword("of")) {
			parse_keyword("of");
			parse_keyword("object");
			kind = METHOD;
		}
		return new RoutineType(current_location(), std::move(formals), ret_ty, kind);
	}

	if (owner) {
		if (is_class && kind == METHOD) {
			// Records permit only the receiverless `class ... static` form.
			// The directive follows the signature, so retain the provisional
			// class-method category until parse_method_prototype validates the
			// directives and changes its ABI to ROUTINE.
			if (dynamic_cast<ClassType*>(owner) || dynamic_cast<RecordType*>(owner) || dynamic_cast<PackedRecordType*>(owner)) {
				kind = CLASS_METHOD;
			} else {
				raise_type_kind_mismatch("class method owner", "class or record", owner);
			}
		}
	} else {
		if (kind != ROUTINE) {
			raise_type_kind_mismatch("expected routine", "routine", owner);
		}
		kind = ROUTINE;
	}
	return new RoutineType(current_location(), std::move(formals), ret_ty, kind);
}

Type* Parser::parse_procedure_type() {
	parse_keyword("procedure");
	return parse_routine_signature(false, false, true, ROUTINE);
}

Type* Parser::parse_function_type() {
	parse_keyword("function");
	return parse_routine_signature(false, true, true, ROUTINE);
}

Type* Parser::parse_operator_type() {
	parse_keyword("operator");
	// For now this is very similar to function.  Note: even parse_routine_signature uses parse_identifier() instead
	// of parse_operator(), sigh.
	return parse_routine_signature(false, true, true, ROUTINE);
}

void Parser::parse_class_lifecycle_prototype(ClassType* owner_class, RoutineKind kind) {
	if (kind != CLASS_CONSTRUCTOR && kind != CLASS_DESTRUCTOR) {
		raise_parse_error("internal error: invalid class lifecycle kind");
	}
	const bool constructing = kind == CLASS_CONSTRUCTOR;
	const char* lifecycle_name = constructing ? "class constructor" : "class destructor";
	parse_keyword(constructing ? "constructor" : "destructor");
	std::string pas_name = parse_identifier();
	RoutineType* sig = parse_routine_signature(true, false, false, kind, owner_class);
	if (!sig->formals.empty()) {
		raise_type_error(std::string(lifecycle_name) + " cannot have parameters", sig);
	}
	parse_semicolon();

	// These directives all describe user-callable or dispatchable overloads;
	// lifecycle hooks are instead compiler-scheduled, nonvirtual operations.
	if (peek_keyword("overload") || peek_keyword("virtual") || peek_keyword("dynamic") || peek_keyword("override") || peek_keyword("abstract") || peek_keyword("final") || peek_keyword("static")) {
		raise_type_error(std::string(lifecycle_name) + " cannot have routine directives", sig);
	}
	Method*& slot = constructing ? owner_class->class_constructor : owner_class->class_destructor;
	if (slot) {
		raise_type_error(std::string("only one ") + lifecycle_name + " may be declared in a class", owner_class);
	}

	auto method = new Method(constructing ? "m_init" : "m_fini", pas_name, sig, false, owner_class, Method::VirtualKind::None);
	method->ty = sig;
	slot = method;
	if (!current_unit) {
		raise_type_error(std::string(lifecycle_name) + " declared outside a unit or program", sig);
	}
	auto& scheduled = constructing ? current_unit->class_constructors : current_unit->class_destructors;
	scheduled.push_back(method);
}

// Class Member Prototype Registration (Value Level)
void Parser::parse_method_prototype(Frame* body, Type* owner_class, bool is_function, bool is_destructor, bool is_constructor, bool is_class) {
	if (is_constructor) {
		parse_keyword("constructor");
	} else if (is_destructor) {
		parse_keyword("destructor");
	} else {
		parse_keyword(is_function ? "function" : "procedure");
	}
	std::string pas_name = parse_identifier();
	RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_destructor ? DESTRUCTOR : is_constructor ? CONSTRUCTOR : METHOD, owner_class);
	parse_semicolon();
	bool has_overload = false;
	bool is_static = false;
	bool is_final = false;
	Method::VirtualKind vk = Method::VirtualKind::None;
	while (true) {
		if (maybe_parse_keyword("overload")) {
			has_overload = true;
			parse_semicolon();
		} else if (maybe_parse_keyword("virtual")) {
			vk = Method::VirtualKind::Virtual;
			parse_semicolon();
		} else if (maybe_parse_keyword("override")) {
			vk = Method::VirtualKind::Override;
			parse_semicolon();
		} else if (maybe_parse_keyword("abstract")) {
			// A class/object abstract method occupies an existing virtual
			// dispatch slot; `abstract` does not itself create that slot.
			// Interface methods are the separate implicitly-pure case.
			if (vk == Method::VirtualKind::None && !dynamic_cast<InterfaceType*>(owner_class)) {
				raise_type_error("only virtual methods can be abstract", sig);
			}
			vk = Method::VirtualKind::Abstract;
			parse_semicolon();
		} else if (maybe_parse_keyword("dynamic")) {
			vk = Method::VirtualKind::Dynamic;
			parse_semicolon();
		} else if (maybe_parse_keyword("final")) {
			is_final = true;
			parse_semicolon();
		} else if (maybe_parse_keyword("static")) {
			is_static = true;
			parse_semicolon();
		} else if (maybe_parse_keyword("inline")) {
			// C++ emission already places the declaration in the class
			// definition; retain Pascal acceptance without changing ABI.
			parse_semicolon();
		} else if (maybe_parse_keyword("noreturn")) {
			// FIXME: retain this directive for C++ attributes.
			parse_semicolon();
		} else {
			break;
		}
	}
	if (is_static) {
		if (!is_class) {
			raise_type_error("static requires a class method declaration", sig);
		}
		if (is_constructor || is_destructor) {
			raise_type_error("constructors and destructors cannot be static methods", sig);
		}
		if (vk != Method::VirtualKind::None || is_final) {
			raise_type_error("static methods cannot be virtual, dynamic, override, "
			                 "abstract, or final",
			                 sig);
		}
		// Static methods remain class-owned Method declarations, but their
		// value and call ABI is exactly the ordinary receiverless routine ABI.
		sig->kind = ROUTINE;
	} else if (is_class && (dynamic_cast<RecordType*>(owner_class) || dynamic_cast<PackedRecordType*>(owner_class))) {
		raise_type_error("record class methods must be static", sig);
	}
	// FPC accepts final only for a method which is virtual already. Keep final
	// independent of VirtualKind because `override; final` is the normal
	// spelling and both properties must reach the C++ declaration.
	//
	// Interface `final` is deliberately outside this rule: FPC accepts that
	// spelling without preventing an implementing class from supplying the
	// method, whereas C++ final would prohibit the implementation. This
	// lowering therefore supports final only on explicit class/object virtual
	// slots.
	if (is_final) {
		if (dynamic_cast<InterfaceType*>(owner_class)) {
			raise_type_error("final interface methods are not supported", sig);
		}
		if (vk == Method::VirtualKind::None) {
			raise_type_error("only virtual methods can be final", sig);
		}
	}
	if (auto object = dynamic_cast<ObjectType*>(owner_class)) {
		if (is_constructor && vk != Method::VirtualKind::None) {
			raise_type_error("old-style object constructors cannot be "
			                 "virtual, dynamic, override, abstract",
			                 sig);
		}
		if (is_constructor || is_destructor || vk != Method::VirtualKind::None) {
			object->needs_vmt = true;
		}
	}
	std::string cxx_name = cxx_value_name(pas_name);
	bool external = false;
	if (maybe_parse_keyword("external")) {
		parse_directive("name");
		cxx_name = parse_string_literal();
		parse_semicolon();
		external = true;
	}
	auto m = new Method(cxx_name, pas_name, sig, has_overload, owner_class, vk);
	m->is_static = is_static;
	m->is_final = is_final;
	m->ty = sig; // The node's type IS the prototype.
	// Install an AbstractError VMT stub.
	// The emitter supplies that body, so a source implementation would be
	// a second implementation of the same method.
	if (vk == Method::VirtualKind::Abstract) {
		m->has_body = true;
	}
	if (external) {
		m->has_body = true;
		m->is_external = true;
		// An external method normally names a C++ member supplied by the
		// external provider (for example TObject.ClassType/p_classtype).
		// A registered builtin may instead declare an explicit receiver ABI;
		// retain its descriptor so call emission can honor that ABI without
		// recognizing a Pascal method name.
		m->builtin_desc = lookup_builtin_desc(cxx_name);
	}
	// Aggregate parsing is a declaration-collection phase. In a named type
	// block, an earlier method signature may still store an IncompleteType
	// edge even though a fresh lookup now returns the resolved declaration.
	// collect_callable therefore performs no identity/carrier comparisons;
	// parse_aggregate_type_body arranges validation after recursive
	// normalization and before emission.
	auto registration = body->collect_callable(pas_name, m);
	if (registration.kind != CallableRegistration::Kind::Added) {
		raise_callable_registration_error(pas_name, m, registration);
	}
}

// Helper to handle overload matching and short-form implementation resolution
Procedure* Parser::match_or_create_procedure(const std::string& pas_name, const std::vector<std::string>& frame_names, const std::string& cxx_name, RoutineType* sig, bool had_paren, bool has_overload, bool short_form_implementation) {
	Frame* enclosing = current_declaration_frame();
	if (frame_names.empty()) {
		raise_parse_error("routine has no declaration identity");
	}
	Node* existing = enclosing->lookup_value(frame_names.front());

	auto family_contains = [](Node* family, Callable* candidate) {
		if (family == candidate) {
			return true;
		}
		if (auto overloads = dynamic_cast<OverloadSet*>(family)) {
			for (Callable* member : overloads->members) {
				if (member == candidate) {
					return true;
				}
			}
		}
		return false;
	};

	auto has_every_frame_binding = [&](Callable* candidate) {
		for (const std::string& frame_name : frame_names) {
			if (!family_contains(enclosing->lookup_value(frame_name), candidate)) {
				return false;
			}
		}
		return true;
	};

	auto sig_matches = [&](Callable* c) -> bool {
		auto rty = static_cast<RoutineType*>(c->ty);
		// Implementation matching is exact routine-signature identity.
		// Assignment conversion, carrier equality, and parameter names/defaults
		// answer different questions and must not attach a body to another
		// overload. Operator aliases make the source declaration contract
		// independently significant: a checked `operator Implicit` body must
		// not attach to a legacy `operator :=` prototype merely because both
		// are visible through &op_CheckedImplicit. The legacy declaration also
		// promises the unchecked family, which that body did not declare.
		return c->pas_name == pas_name && rty->same_signature_as(sig);
	};

	auto attach_to = [&](Callable* c) -> Procedure* {
		if (c->has_body) {
			raise_type_error("duplicate implementation of '" + pas_name + "'", c->ty);
		}
		if (had_paren) {
			// Pascal allows impl parameter names to differ from interface names.
			// We update the prototype's names so local scope bindings match the body text.
			static_cast<RoutineType*>(c->ty)->formals = sig->formals;
		}
		auto p = dynamic_cast<Procedure*>(c);
		if (!p) {
			raise_type_kind_mismatch("'" + pas_name + "'", "standalone procedure", c->ty);
		}
		return p;
	};

	Procedure* target = nullptr;
	auto consider_existing = [&](Node* node) {
		if (node && !target) {
			if (auto ec = dynamic_cast<Callable*>(node)) {
				if (ec->pas_name == pas_name && (short_form_implementation || sig_matches(ec)) && !ec->has_body && has_every_frame_binding(ec)) {
					target = attach_to(ec);
				}
			} else if (auto os = dynamic_cast<OverloadSet*>(node)) {
				if (short_form_implementation) {
					Callable* pick = nullptr;
					for (auto* m : os->members) {
						if (m->pas_name == pas_name && !m->has_body && has_every_frame_binding(m)) {
							if (pick) {
								raise_values_error("ambiguous short-form impl", {{"candidate 1", pick}, {"candidate 2", m}});
							}
							pick = m;
						}
					}
					if (pick) {
						target = attach_to(pick);
					}
				} else {
					for (auto* m : os->members) {
						if (!m->has_body && sig_matches(m) && has_every_frame_binding(m)) {
							if (target) {
								raise_values_error("ambiguous overload match", {{"candidate 1", target}, {"candidate 2", m}});
							}
							target = attach_to(m);
						}
					}
				}
			}
		}
	};

	consider_existing(existing);
	// A unit's interface and implementation deliberately mutate one owning
	// Frame, so every prototype eligible for this body is already in
	// ENCLOSING. Searching the name-lookup stack here would be wrong: those
	// lower entries are used-unit dependencies, not declarations owned by
	// the unit whose implementation is being parsed.
	if (!target && short_form_implementation && existing) {
		raise_type_error("no unimplemented prototype", sig);
	}

	if (!target) {
		target = new Procedure(cxx_name, pas_name, sig, has_overload);
		target->ty = sig;
		target->owning_unit = declaration_unit(enclosing);
		for (const std::string& frame_name : frame_names) {
			auto registration = enclosing->register_callable(frame_name, target);
			if (registration.kind != CallableRegistration::Kind::Added) {
				raise_callable_registration_error(pas_name, target, registration);
			}
		}
	}
	return target;
}

void Parser::parse_routine_body(Callable* target, Frame* owner_frame) {
	// Lexical lookup is represented by Parser::scopes. Giving the body frame
	// the owner frame as a structural parent would let members win before
	// formals (notably `property Value` versus a setter parameter named
	// `Value`). Keep it parentless and push the implicit-Self member scope
	// below the body scope instead.
	Frame* body_frame = new Frame(nullptr);
	target->body_frame = body_frame;
	Callable* saved_routine = current_routine;
	bool nested_lambda = saved_routine && dynamic_cast<Procedure*>(target);
	current_routine = target;
	push_statement_control_context();
	StorageSlot* receiver_slot = nullptr;
	bool pushed_owner_scope = false;
	if (auto m = dynamic_cast<Method*>(target)) {
		if (m->is_static) {
			// A static method has no Pascal Self and no hidden C++ receiver.
			// Its declaring member environment nevertheless remains the first
			// lookup domain. A class designator lets unqualified ordinary class
			// methods bind to the declaring class; a record designator permits
			// only static members. Neither designator is passed to this method.
			Node* owner_qualifier = nullptr;
			if (auto owner = dynamic_cast<ClassType*>(m->owner_class)) {
				owner_qualifier = new ClassRefValue(owner);
			} else {
				owner_qualifier = new TypeMemberQualifier(m->owner_class);
			}
			push_scope(owner_frame, owner_qualifier);
			pushed_owner_scope = true;
		} else {
			// Pascal class-method Self is the class reference, not an instance.
			// Keep that as the same Type used for `class of Foo`. Its C++ carrier
			// is Foo's empty metaclass marker base; the emitter recovers the exact
			// Foo::m_meta receiver only when applying a class operation.
			//
			// Instance Self is a reference to the owner instance. ClassType and
			// InterfaceType are already reference-shaped; ObjectType is value-shaped
			// and needs a pointer wrapper for member-access emission.
			Type* self_ty = nullptr;
			if (target->ty->kind == CLASS_METHOD || target->ty->kind == CLASS_CONSTRUCTOR || target->ty->kind == CLASS_DESTRUCTOR) {
				self_ty = new ClassRefType(current_location(), m->owner_class);
			} else if (dynamic_cast<ClassType*>(m->owner_class) || dynamic_cast<InterfaceType*>(m->owner_class)) {
				self_ty = m->owner_class;
			} else {
				self_ty = new PointerType(current_location(), m->owner_class);
			}
			receiver_slot = new StorageSlot("this", self_ty);
			// The generated m_meta lifecycle body needs a C++ receiver so class
			// members can be lowered through the exact class reference. FPC treats
			// both lifecycle hooks as static class methods, so neither has a
			// Pascal-visible Self identifier.
			if (target->ty->kind != CLASS_CONSTRUCTOR && target->ty->kind != CLASS_DESTRUCTOR) {
				if (!body_frame->register_variable("self", receiver_slot, self_ty)) {
					raise_parse_error("duplicate identifier: self");
				}
			}
			push_scope(owner_frame, receiver_slot);
			pushed_owner_scope = true;
		}
	}
	push_scope(body_frame);
	push_declaration_frame(body_frame);
	if (target->ty->return_type != &unit_type()) { // function
		auto result_slot = new StorageSlot("p_result", target->ty->return_type);
		if (!body_frame->register_variable("result", result_slot, target->ty->return_type)) {
			raise_parse_error("duplicate identifier: result");
		}
	}
	auto rty = static_cast<RoutineType*>(target->ty);
	for (auto& p : rty->formals) {
		if (!body_frame->register_variable(p.pas_name, new StorageSlot(p.cxx_name, p.ty), p.ty)) {
			raise_parse_error("duplicate parameter identifier: " + p.pas_name);
		}
	}
	if (emitter) {
		emitter->emit_procedure_open(target, nested_lambda);
	}
	parse_decl_blocks(false);
	parse_keyword("begin");
	parse_block_body();
	target->has_body = true;
	parse_keyword("end");
	parse_semicolon();
	if (emitter) {
		emitter->emit_procedure_close(target, nested_lambda);
	}
	pop_declaration_frame();
	pop_scope(); // pop body_frame
	if (pushed_owner_scope) {
		pop_scope(); // pop the with_scope
	}
	pop_statement_control_context();
	current_routine = saved_routine;
}

Type* Parser::lookup_external_type(const char* lib, std::string cxx_name) {
	if (lib == nullptr) {
		auto intrinsic = lookup_builtin_type(cxx_name);
		if (intrinsic == nullptr) {
			return raise_type_parse_error("unknown external type " + cxx_name);
		} else {
			return intrinsic;
		}
	} else {
		return raise_type_parse_error("unknown external library '" + std::string(lib) + "'");
	}
}

Builtin* Parser::lookup_external_value(const char* lib, std::string cxx_name) {
	if (lib == nullptr) {
		auto builtin = create_builtin_value(cxx_name);
		if (builtin == nullptr) {
			raise_parse_error("builtin '" + cxx_name + "' not found");
			return nullptr;
		} else {
			return builtin;
		}
	} else {
		raise_parse_error("external library not implemented");
		return nullptr;
	}
}

struct ParsedOperatorIdentity {
	std::vector<std::string> pascal_identifiers;
	std::string cxx_name;
	bool boolean_result = false;
	bool declaration_supported = true;
};

static ParsedOperatorIdentity parsed_operator_identity(const std::string& source_name, size_t arity) {
	ParsedOperatorIdentity result;
	for (const OperatorSpec* spec : operator_declaration_specs(source_name, arity)) {
		if (result.cxx_name.empty()) {
			result.cxx_name = std::string(spec->cxx_name);
		} else {
			assert(result.cxx_name == spec->cxx_name);
		}
		result.boolean_result = result.boolean_result || spec->boolean_result;
		result.declaration_supported = result.declaration_supported && spec->declaration_supported;
		std::string identifier(spec->pascal_identifier);
		if (std::find(result.pascal_identifiers.begin(), result.pascal_identifiers.end(), identifier) == result.pascal_identifiers.end()) {
			result.pascal_identifiers.push_back(std::move(identifier));
		}
	}
	return result;
}

void Parser::parse_procedure_or_function(bool is_class, bool is_function, bool is_decl_only) {
	bool has_overload = false;
	std::string first_name;
	std::string operator_declaration_name;
	bool is_operator = false;
	bool is_destructor = false;
	bool is_constructor = false;
	if (peek_keyword("operator")) {
		is_operator = true;
		if (!is_function) {
			raise_parse_error("custom operator should have a return value");
		}
		parse_keyword("operator");
		// Assumption: there are no method operators. Keep the source spelling
		// for the catalog, but give conversion Callables reserved internal
		// names. An ordinary function named Explicit, Implicit, or
		// UncheckedImplicit is not a conversion and therefore must not receive
		// the destination-tag ABI or result-type overload rules merely because
		// its source name resembles an operator declaration.
		operator_declaration_name = input_token;
		// Known-but-unsupported lifecycle operators are resultless. Reject
		// them at their declaration identity, before the currently supported
		// expression-operator grammar tries to parse every operator as a
		// function and emits the misleading "missing colon" diagnostic.
		// This is only an early diagnostic for catalog rows that are already
		// unsupported; it does not add lifecycle declaration semantics.
		bool known_operator = false;
		bool supported_operator = false;
		for (const OperatorSpec& spec : operator_catalog()) {
			if (spec.declaration_name != operator_declaration_name) {
				continue;
			}
			known_operator = true;
			supported_operator = supported_operator || spec.declaration_supported;
		}
		if (known_operator && !supported_operator) {
			raise_parse_error("operator '" + operator_declaration_name + "' is recognized but not supported");
		}
		if (operator_declaration_name == "implicit") {
			first_name = ":implicit";
		} else if (operator_declaration_name == "uncheckedimplicit") {
			first_name = ":uncheckedimplicit";
		} else if (operator_declaration_name == "explicit") {
			first_name = ":explicit";
		} else {
			first_name = operator_declaration_name;
		}
		consume();
		has_overload = true; // I think those should be implicitly "overload;"
	} else if (maybe_parse_keyword("destructor")) {
		is_destructor = true;
		first_name = parse_identifier();
	} else if (maybe_parse_keyword("constructor")) {
		is_constructor = true;
		first_name = parse_identifier();
	} else {
		parse_keyword(is_function ? "function" : "procedure");
		first_name = parse_identifier();
	}

	// `procedure TFoo.Bar;`
	if (maybe_parse_period()) {
		std::string method_name = parse_identifier();
		Type* owner_ty = resolve_type(first_name, false);
		Frame* owner_frame = get_type_body_frame(owner_ty);
		if (!owner_frame) {
			raise_type_kind_mismatch("'" + first_name + "'", "class, record, or object", owner_ty);
		}
		if (is_class && (is_constructor || is_destructor)) {
			auto class_type = dynamic_cast<ClassType*>(owner_ty);
			if (!class_type) {
				raise_type_kind_mismatch("class lifecycle implementation owner", "class", owner_ty);
			}
			const bool constructing = is_constructor;
			const char* lifecycle_name = constructing ? "class constructor" : "class destructor";
			RoutineKind kind = constructing ? CLASS_CONSTRUCTOR : CLASS_DESTRUCTOR;
			Method* method = constructing ? class_type->class_constructor : class_type->class_destructor;
			if (!method || method->pas_name != method_name) {
				raise_type_error("no " + std::string(lifecycle_name) + " '" + method_name + "' on '" + first_name + "'", class_type);
			}
			RoutineType* sig = parse_routine_signature(true, false, false, kind, owner_ty);
			if (!sig->formals.empty()) {
				raise_type_error(std::string(lifecycle_name) + " cannot have parameters", sig);
			}
			parse_semicolon();
			if (method->has_body) {
				raise_type_error("duplicate implementation of " + std::string(lifecycle_name) + " '" + method_name + "'", method->ty);
			}
			parse_routine_body(method, owner_frame);
		} else {
			RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_constructor ? CONSTRUCTOR : is_destructor ? DESTRUCTOR : METHOD, owner_ty);
			parse_semicolon();
			while (maybe_parse_keyword("inline")) {
				// FIXME: use
				parse_semicolon();
			}
			while (maybe_parse_keyword("noreturn")) {
				// FIXME: use
				parse_semicolon();
			}
			Node* hit = owner_frame->lookup_value(method_name);
			Method* m = nullptr;
			auto consider_method = [&](Callable* callable) {
				auto candidate = dynamic_cast<Method*>(callable);
				if (candidate && candidate->owner_class == owner_ty) {
					bool declaration_is_class_method = candidate->is_static || candidate->ty->kind == CLASS_METHOD;
					// `class ... static` is class-owned in Pascal but has
					// receiverless ROUTINE ABI. Its out-of-line spelling does
					// not repeat `static`, so Method metadata selects that
					// category; the visible parameter modes/types and result
					// remain exact.
					if (is_class == declaration_is_class_method && candidate->ty->same_parameter_and_result_types_as(sig)) {
						if (m) {
							raise_values_error("ambiguous method implementation '" + method_name + "'", {{"candidate 1", m}, {"candidate 2", candidate}});
						}
						m = candidate;
					}
				}
			};
			if (auto callable = dynamic_cast<Callable*>(hit)) {
				consider_method(callable);
			} else if (auto overloads = dynamic_cast<OverloadSet*>(hit)) {
				for (Callable* callable : overloads->members) {
					consider_method(callable);
				}
			}

			// Structural lookup may return an inherited family. An out-of-line
			// `T.Method` body can only attach to a declaration owned by exactly
			// T, and only to its exact Pascal signature.
			if (!m) {
				raise_type_error("no matching method declaration '" + method_name + "' on '" + first_name + "'", sig);
			}
			if (m->has_body) {
				raise_type_error("duplicate implementation of '" + method_name + "'", m->ty);
			}
			// Pascal permits implementation parameter names to differ from the
			// prototype, but defaults belong to the prototype/call site.
			// Replacing the complete formal objects here erased those defaults
			// as soon as an out-of-line body was parsed.
			auto declaration_sig = static_cast<RoutineType*>(m->ty);
			for (size_t i = 0; i < sig->formals.size(); ++i) {
				declaration_sig->formals[i].pas_name = sig->formals[i].pas_name;
				declaration_sig->formals[i].cxx_name = sig->formals[i].cxx_name;
			}
			parse_routine_body(m, owner_frame);
		}
	} else { // Standalone Routine
		bool had_paren = (input_token == "(");
		if (is_class) {
			raise_parse_error("expected class method, not class routine");
		}
		RoutineType* sig = parse_routine_signature(is_class, is_function, false, ROUTINE);
		parse_semicolon();
		bool body_follows = !is_decl_only;
		std::optional<std::string> external_cxx_name;
		while (true) {
			if (maybe_parse_keyword("overload")) {
				has_overload = true;
				parse_semicolon();
			} else if (maybe_parse_keyword("inline")) {
				// FIXME: use
				parse_semicolon();
			} else if (maybe_parse_keyword("noreturn")) {
				// FIXME: use
				parse_semicolon();
			} else if (maybe_parse_keyword("forward")) {
				body_follows = false;
				parse_semicolon();
			} else if (maybe_parse_directive("external")) {
				parse_directive("name");
				external_cxx_name = parse_string_literal();
				body_follows = false;
				parse_semicolon();
			} else {
				break;
			}
		}
		// Distinct standalone signatures in this declaration Frame already
		// form one local family without `overload`, as in FPC. Only the
		// explicit directive opens lookup into an outer/unit scope.
		/*
		In an INTERFACE section there is this:
		  function x: Integer;
		  const Foo = 'Hello';
		That const Foo is supposed to be global, not in the function.
		*/
		body_follows = body_follows && (peek_keyword("begin") || peek_keyword("label") || peek_keyword("var") || peek_keyword("const") || peek_keyword("type") || peek_keyword("procedure") || peek_keyword("function"));
		// A routine declaration in an interface section has the signature it
		// writes, including zero parameters when parentheses are absent. Only
		// an implementation with a body may omit an earlier prototype's
		// parameter list.
		const bool short_form_implementation = !is_decl_only && body_follows && !had_paren;
		ParsedOperatorIdentity operator_identity;
		if (is_operator) {
			operator_identity = parsed_operator_identity(operator_declaration_name, sig->formals.size());
			if (operator_identity.pascal_identifiers.empty()) {
				if (operator_declaration_name_known(operator_declaration_name)) {
					raise_type_error("operator '" + operator_declaration_name + "' does not accept " + std::to_string(sig->formals.size()) + " parameter(s)", sig);
				}
				raise_type_error("unknown custom operator '" + operator_declaration_name + "'", sig);
			}
			if (!operator_identity.declaration_supported) {
				raise_type_error("operator '" + operator_declaration_name + "' is recognized but not supported", sig);
			}
			if (operator_identity.boolean_result && sig->return_type != boolean_type()) {
				raise_type_mismatch("operator '" + operator_declaration_name + "' return type", boolean_type(), sig->return_type);
			}
		} else {
			operator_identity = {{first_name}, cxx_value_name(first_name)};
		}
		Procedure* target = match_or_create_procedure(first_name, operator_identity.pascal_identifiers, operator_identity.cxx_name, sig, had_paren, has_overload, short_form_implementation);
		if (external_cxx_name) {
			target->owning_unit = nullptr;
			auto builtin = lookup_external_value(nullptr, *external_cxx_name);
			if (builtin != nullptr) {
				// This is basically making TARGET an ALIAS for BUILTIN.
				target->has_body = true;
				target->is_external = true;
				if (auto qbuiltin = dynamic_cast<Builtin*>(builtin)) { // used
					// These Builtins are all polymorphic and C++ overloads will just have to adjust
					// to us.
					auto desc = qbuiltin->desc;
					target->cxx_name = desc->cxx_name;
					target->builtin_desc = desc;
				} else {
					raise_parse_error("unknown intrinsic via external '" + *external_cxx_name + "'");
				}
			}
		} else if (body_follows) {
			parse_routine_body(target, nullptr);
		} else {
			// Interface prototype or `forward` decl.
			// No body at this site, but the signature needs to emit so callers can see it.
			// Top-level prototypes get empty decorations (no virtual or override).
			// The parser passes NO whitespace; emit_callable_prototype owns its own `\n` separator.
			if (emitter) {
				emitter->emit_callable_prototype(target, "", "", "");
			}
		}
	}
}

// Pascal binds a method receiver separately and performs single dispatch
// after selecting a declaration from its member family, so Self is
// deliberately not an overload-ranking position. Only source-visible
// parameters enter these ranks.
static bool is_ordinal_intrinsic_argument(Type* ty) {
	ty = subrange_range_type(ty);
	if (dynamic_cast<EnumType*>(ty)) {
		return true;
	}
	OrdinalBounds bounds;
	return intrinsic_ordinal_bounds(ty, &bounds);
}

// Omitted-type candidate matching and final-call validation must enforce one
// identical operand domain. Keeping that contract here prevents a root
// fallback from being ranked as viable and then interpreted under a different
// enum/pointer rule after selection.
static bool is_generic_ordinal_operation(BuiltinGenericKind kind) {
	return kind == BuiltinGenericKind::OrdinalValue || kind == BuiltinGenericKind::UnaryOrdinalOrPointerStep || kind == BuiltinGenericKind::EnumOrPointerStep || kind == BuiltinGenericKind::OrdinalSuccessorOrPredecessor;
}

static bool generic_ordinal_operation_accepts(BuiltinGenericKind kind, Type* operand) {
	if (kind == BuiltinGenericKind::OrdinalValue || kind == BuiltinGenericKind::OrdinalSuccessorOrPredecessor) {
		return is_ordinal_intrinsic_argument(operand);
	}
	if (kind == BuiltinGenericKind::UnaryOrdinalOrPointerStep) {
		if (auto pointer = dynamic_cast<PointerType*>(operand)) {
			// `Pointer` is carried as void*. With no element type or
			// allocation metadata, C++20 cannot step it while preserving
			// provenance. Do not replace that missing information with an
			// integer address.
			return !pointer->is_untyped();
		} else {
			return is_ordinal_intrinsic_argument(operand);
		}
	}
	if (kind == BuiltinGenericKind::EnumOrPointerStep) {
		if (auto pointer = dynamic_cast<PointerType*>(operand)) {
			return !pointer->is_untyped();
		}
		operand = subrange_range_type(operand);
		return dynamic_cast<EnumType*>(operand) != nullptr || operand == char_type();
	}
	return false;
}

static bool generic_absolute_value_accepts(Type* operand) {
	operand = subrange_range_type(operand);
	OrdinalBounds bounds;
	return integer_bounds(operand, &bounds) || operand == single_type() || operand == double_type() || operand == extended_type();
}

static bool integer_type_contains_literal(Type* target, const Integer* literal) {
	if (auto range = dynamic_cast<SubrangeType*>(target)) {
		ConstEvalContext ctx;
		ConstEvalResult lower = range->lower_bound->const_eval(ctx);
		ConstEvalResult upper = range->upper_bound->const_eval(ctx);
		if (lower.kind != ConstEvalResult::Kind::Success || upper.kind != ConstEvalResult::Kind::Success) {
			return false;
		}
		std::string error;
		auto classified_lower = classify_subrange_bound(lower.node, &error);
		auto classified_upper = classify_subrange_bound(upper.node, &error);
		if (!classified_lower || !classified_upper) {
			return false;
		}
		auto value = ordinal_value(literal->negative, literal->value);
		return compare_ordinal_value(value, classified_lower->ordinal_value) >= 0 && compare_ordinal_value(value, classified_upper->ordinal_value) <= 0;
	}
	OrdinalBounds bounds;
	return intrinsic_ordinal_bounds(target, &bounds) && ordinal_bounds_contains(bounds, literal->negative, literal->value);
}

static Type* integer_literal_natural_type(const Integer* literal) {
	if (!literal) {
		return nullptr;
	}
	if (literal->negative) {
		if (literal->value <= 128) {
			return shortint_type();
		} else if (literal->value <= 32768) {
			return smallint_type();
		} else if (literal->value <= uint64_t{2147483648}) {
			return integer_type();
		} else {
			return int64_type();
		}
	} else if (literal->value <= INT8_MAX) {
		return shortint_type();
	} else if (literal->value <= UINT8_MAX) {
		return byte_type();
	} else if (literal->value <= INT16_MAX) {
		return smallint_type();
	} else if (literal->value <= UINT16_MAX) {
		return word_type();
	} else if (literal->value <= INT32_MAX) {
		return integer_type();
	} else if (literal->value <= UINT32_MAX) {
		return cardinal_type();
	} else if (literal->value <= INT64_MAX) {
		return int64_type();
	} else {
		return qword_type();
	}
}

static std::optional<bool> integer_carrier_is_signed(Type* type) {
	while (auto range = dynamic_cast<SubrangeType*>(type)) {
		type = range->base_type;
	}
	OrdinalBounds bounds;
	if (!integer_bounds(type, &bounds)) {
		return std::nullopt;
	}
	return bounds.signed_type;
}

/** Subranges constrain selected storage but are not overload identities or
 * source-ranking subtypes. Preserve every other nominal type, including
 * enums and DistinctType. */
static Type* overload_rank_type(Type* type) {
	while (auto range = dynamic_cast<SubrangeType*>(type)) {
		type = range->base_type;
	}
	return type;
}

static Integer* untyped_integer_constant(Node* expression) {
	if (!expression || expression->ty != &untyped_integer_type()) {
		return nullptr;
	}
	if (auto integer = dynamic_cast<Integer*>(expression)) {
		return integer;
	}

	// Named constants remain untyped until context selects a carrier. Fold the
	// constant declaration here so a reference to `const N = 3` has exactly
	// the same range and overload behavior as the source literal `3`.
	ConstEvalContext ctx;
	ConstEvalResult folded = expression->const_eval(ctx);
	if (folded.kind != ConstEvalResult::Kind::Success) {
		return nullptr;
	}
	auto integer = dynamic_cast<Integer*>(folded.node);
	return integer && integer->ty == &untyped_integer_type() ? integer : nullptr;
}

static Real* untyped_real_constant(Node* expression) {
	if (!expression || expression->ty != &untyped_real_type()) {
		return nullptr;
	}
	if (auto real = dynamic_cast<Real*>(expression); real && real->is_origin()) {
		return real;
	}

	ConstEvalContext ctx;
	ConstEvalResult folded = expression->const_eval(ctx);
	if (folded.kind != ConstEvalResult::Kind::Success) {
		return nullptr;
	}
	auto real = dynamic_cast<Real*>(folded.node);
	return real && real->ty == &untyped_real_type() && real->is_origin() ? real : nullptr;
}

static std::optional<uint64_t> integer_literal_target_preference(Type* target) {
	// The literal remains untyped. Rank fitting destinations by Pascal's
	// magnitude-driven predefined carrier order, rather than pretending the
	// literal has a source carrier whose signedness must be preserved. This
	// also orders the equal-cardinality signed/unsigned pairs without exposing
	// a 2^32 or saturated 2^64 range size as a fictional "distance".
	Type* carrier = overload_rank_type(target);
	carrier = distinct_storage_type(carrier);
	const std::array<Type*, 8> preference{{
	    shortint_type(),
	    byte_type(),
	    smallint_type(),
	    word_type(),
	    integer_type(),
	    cardinal_type(),
	    int64_type(),
	    qword_type(),
	}};
	for (size_t index = 0; index < preference.size(); ++index) {
		if (carrier == preference[index]) {
			return static_cast<uint64_t>(index);
		}
	}
	return std::nullopt;
}

static Node* contextual_based_integer_value(Node* expression, Type* target) {
	Integer* literal = untyped_integer_constant(expression);
	if (!literal || !literal->based_literal || literal->negative) {
		return nullptr;
	}

	OrdinalBounds bounds;
	if (!integer_bounds(target, &bounds) || !bounds.signed_type || literal->value <= bounds.max_positive) {
		return nullptr;
	}
	const uint64_t unsigned_max = bounds.min_magnitude == (uint64_t{1} << 63) ? UINT64_MAX : bounds.min_magnitude * 2 - 1;
	if (literal->value > unsigned_max) {
		return nullptr;
	}

	// Pascal based notation can construct the destination carrier's bit
	// pattern. Treat that construction as one assignment-compatible value
	// match so a value formal and an assignment destination accept it equally.
	ConstEvalResult converted = const_explicit_ordinal_cast(literal->value, false, target);
	return converted.kind == ConstEvalResult::Kind::Success ? converted.node : nullptr;
}

static bool rank_less(const MatchRank& a, Type* a_formal, const MatchRank& b, Type* b_formal, const std::function<bool(Type*, Type*)>& direct_assignment_edge) {
	if (a.tier != b.tier) {
		return static_cast<unsigned>(a.tier) < static_cast<unsigned>(b.tier);
	}
	if (a.contextual_construction != b.contextual_construction) {
		return static_cast<unsigned>(a.contextual_construction) < static_cast<unsigned>(b.contextual_construction);
	}
	a_formal = overload_rank_type(a_formal);
	b_formal = overload_rank_type(b_formal);
	if (a.rounded_real_origin && b.rounded_real_origin) {
		// Ordinary overloads still need a deterministic context for an inexact
		// decimal origin: prefer the declaration which discards fewer bits of
		// the origin. Do not apply this reversal to typed integer/real values;
		// their information loss is considered jointly only by common-domain
		// operator policy below.
		const int a_real = real_semantic_rank(a_formal);
		const int b_real = real_semantic_rank(b_formal);
		if (a_real >= 0 && b_real >= 0 && a_real != b_real) {
			return a_real > b_real;
		}
	}
	if (a_formal && b_formal && a_formal != b_formal) {
		const bool a_to_b = direct_assignment_edge(a_formal, b_formal);
		const bool b_to_a = direct_assignment_edge(b_formal, a_formal);
		if (a_to_b != b_to_a) {
			// The one-way non-narrowing assignment edge identifies the more
			// specific destination. Following A -> X -> B or consulting
			// narrowing reachability would erase that direction.
			return a_to_b;
		}
	}
	if (a.integer_sign_mismatch != b.integer_sign_mismatch) {
		return !a.integer_sign_mismatch;
	}
	if (a.distance != b.distance) {
		return a.distance < b.distance;
	}
	return false;
}

static int real_range_rank(Type* type) {
	type = distinct_storage_type(type);
	if (type == single_type()) {
		return 0;
	} else if (type == double_type()) {
		return 1;
	} else if (type == extended_type()) {
		return 2;
	}
	return -1;
}

static bool ordinal_interval_for_conversion(Type* type, OrdinalRange::Value* lower, OrdinalRange::Value* upper) {
	if (auto range = dynamic_cast<SubrangeType*>(type)) {
		ConstEvalContext context;
		ConstEvalResult folded_lower = range->lower_bound->const_eval(context);
		ConstEvalResult folded_upper = range->upper_bound->const_eval(context);
		if (folded_lower.kind != ConstEvalResult::Kind::Success || folded_upper.kind != ConstEvalResult::Kind::Success) {
			return false;
		}
		std::string error;
		auto classified_lower = classify_subrange_bound(folded_lower.node, &error);
		auto classified_upper = classify_subrange_bound(folded_upper.node, &error);
		if (!classified_lower || !classified_upper) {
			return false;
		}
		*lower = classified_lower->ordinal_value;
		*upper = classified_upper->ordinal_value;
		return true;
	} else if (auto enumeration = dynamic_cast<EnumType*>(type)) {
		const EnumType::Member* minimum = enumeration->min_member();
		const EnumType::Member* maximum = enumeration->max_member();
		if (!minimum || !maximum) {
			return false;
		}
		*lower = ordinal_value(minimum->value);
		*upper = ordinal_value(maximum->value);
		return true;
	} else {
		OrdinalBounds bounds;
		if (!intrinsic_ordinal_bounds(type, &bounds)) {
			return false;
		}
		*lower = ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0);
		*upper = ordinal_value(false, bounds.max_positive);
		return true;
	}
}

static Type* infer_bracket_common_item_type(const std::vector<BracketLiteral::Item>& items, BracketIntegerPreference preference) {
	Type* exact_type = nullptr;
	bool exact_type_still_common = true;
	bool saw_untyped_integer = false;
	bool all_integer = true;
	bool have_integer_interval = false;
	OrdinalRange::Value common_lower;
	OrdinalRange::Value common_upper;

	for (const auto& item : items) {
		for (Node* bound : {item.lower, item.upper}) {
			if (!bound) {
				continue;
			}

			OrdinalRange::Value lower;
			OrdinalRange::Value upper;
			if (auto literal = untyped_integer_constant(bound)) {
				saw_untyped_integer = true;
				lower = ordinal_value(literal->negative, literal->value);
				upper = lower;
			} else {
				if (!exact_type) {
					exact_type = bound->ty;
				} else if (exact_type != bound->ty) {
					exact_type_still_common = false;
				}
				if (!is_integer_semantic_type(bound->ty) || !ordinal_interval_for_conversion(bound->ty, &lower, &upper)) {
					all_integer = false;
					continue;
				}
			}

			if (!have_integer_interval) {
				common_lower = lower;
				common_upper = upper;
				have_integer_interval = true;
			} else {
				if (compare_ordinal_value(lower, common_lower) < 0) {
					common_lower = lower;
				}
				if (compare_ordinal_value(upper, common_upper) > 0) {
					common_upper = upper;
				}
			}
		}
	}

	if (!saw_untyped_integer && exact_type && exact_type_still_common) {
		return exact_type;
	}
	if (!all_integer || !have_integer_interval) {
		return unknown_type();
	}

	// This is a true range union, not FPC's order-dependent "replace the
	// accumulator only when it fits the next type" scan. Arrays follow the
	// signed-first ordinary constructor convention. Historical Pascal sets
	// instead use an unsigned domain for nonnegative byte/word/cardinal
	// constants. They remain separate interpretations of the same syntax.
	const std::array<Type*, 8> signed_first{{
	    shortint_type(),
	    byte_type(),
	    smallint_type(),
	    word_type(),
	    integer_type(),
	    cardinal_type(),
	    int64_type(),
	    qword_type(),
	}};
	const std::array<Type*, 8> unsigned_first{{
	    byte_type(),
	    shortint_type(),
	    word_type(),
	    smallint_type(),
	    cardinal_type(),
	    integer_type(),
	    qword_type(),
	    int64_type(),
	}};
	const auto& candidates = preference == BracketIntegerPreference::Array ? signed_first : unsigned_first;
	for (Type* candidate : candidates) {
		OrdinalBounds bounds;
		if (!integer_bounds(candidate, &bounds)) {
			continue;
		}
		const auto lower = ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0);
		const auto upper = ordinal_value(false, bounds.max_positive);
		if (compare_ordinal_value(common_lower, lower) >= 0 && compare_ordinal_value(common_upper, upper) <= 0) {
			return candidate;
		}
	}
	return unknown_type();
}

static bool conversion_requires_range_check(Type* source, Type* target) {
	const int source_real = real_range_rank(source);
	const int target_real = real_range_rank(target);
	if (source_real >= 0 && target_real >= 0) {
		return source_real > target_real;
	}

	OrdinalRange::Value source_lower;
	OrdinalRange::Value source_upper;
	OrdinalRange::Value target_lower;
	OrdinalRange::Value target_upper;
	if (!source || !target || !ordinal_interval_for_conversion(source, &source_lower, &source_upper) || !ordinal_interval_for_conversion(target, &target_lower, &target_upper)) {
		return false;
	}
	// The classified assignment relation owns family/nominal compatibility.
	// This helper answers the distinct range-containment question used by
	// {$R}. Overload preference is deliberately separate: a usual arithmetic
	// promotion such as
	// QWord -> Int64 can still require this check. Do not use
	// ordinal_range_for_type here: that helper also computes an array element
	// count and necessarily rejects a complete 64-bit domain of 2^64 values,
	// while a conversion needs only endpoints.
	return compare_ordinal_value(source_lower, target_lower) < 0 || compare_ordinal_value(source_upper, target_upper) > 0;
}

// A bracket-literal item matched against a subrange element formal is
// contextual: a compile-time-constant ordinal of the subrange's base type is
// admissible if its value lies within the subrange endpoints. This mirrors
// parse_storage_initializer's direct-subrange path so the BracketLiteral
// branch doesn't have to commit to a narrowing conversion at type level.
// Returns nullopt when the situation doesn't apply; callers fall back to the
// ordinary match.
static std::optional<ArgumentMatch> contextual_subrange_constant_match(const Parameter& formal, Node* actual) {
	auto subrange = dynamic_cast<SubrangeType*>(formal.ty);
	if (!subrange) {
		return std::nullopt;
	}
	if (formal.mode == ParamMode::Var || formal.mode == ParamMode::Out) {
		return std::nullopt;
	}
	ConstEvalContext ctx;
	ConstEvalResult folded = actual->const_eval(ctx);
	if (folded.kind != ConstEvalResult::Kind::Success) {
		return std::nullopt;
	}
	std::string error;
	auto bound = classify_subrange_bound(folded.node, &error);
	if (!bound) {
		return std::nullopt;
	}
	OrdinalRange range;
	if (!ordinal_range_for_type(subrange, &range, &error)) {
		return std::nullopt;
	}
	if (!ordinal_constant_matches_range_type(range.base_type, *bound)) {
		return std::nullopt;
	}
	if (compare_ordinal_value(bound->ordinal_value, range.lower_ordinal) < 0 || compare_ordinal_value(bound->ordinal_value, range.upper_ordinal) > 0) {
		return std::nullopt;
	}
	return ArgumentMatch{{MatchRank::Tier::Equal, 0}, new Cast(bound->node, subrange)};
}

std::optional<ArgumentMatch> Parser::match_argument(const Parameter& formal, Node* actual, const BuiltinDesc* builtin, size_t parameter_index, bool allow_declared_conversion, MatchFailure* failure, DeclaredConversionFailure* conversion_failure) {
	if (failure) {
		*failure = MatchFailure::Incompatible;
	}
	Type* source = actual ? actual->ty : nullptr;
	Type* target = formal.ty;
	Integer* untyped_integer = untyped_integer_constant(actual);

	if (builtin && builtin->generic_kind == BuiltinGenericKind::Assigned && parameter_index == 0 && (dynamic_cast<RoutineType*>(source) || (source && source->is_reference_type()))) {
		return ArgumentMatch{{MatchRank::Tier::Equal, 0}, actual};
	}

	if (target == unknown_type()) {
		if (builtin && builtin->generic_kind == BuiltinGenericKind::ShortStringMutation && !dynamic_cast<ShortStringType*>(source)) {
			// Only the mutable String[N] formal is omitted in System's
			// Delete/Insert declarations. Preserve its exact capacity without
			// making arbitrary mutable arguments viable.
			return std::nullopt;
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::StrOutput) {
			if (parameter_index == 0 && str_value_family(source) != StrValueFamily::Integer && str_value_family(source) != StrValueFamily::EnumerationTodo) {
				// The concrete Extended declaration handles the real family.
				// The generic source exists only as the future enum
				// extension point, not as an accept-anything escape hatch.
				return std::nullopt;
			}
			if (parameter_index == 1 && !dynamic_cast<ShortStringType*>(source)) {
				// Only the omitted destination form models the String[N]
				// family. AnsiString has concrete declarations so its var
				// parameter remains exact during ordinary overload ranking.
				return std::nullopt;
			}
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::ValOutput) {
			if (parameter_index == 1 && val_destination_family(source) != ValDestinationFamily::Integer && val_destination_family(source) != ValDestinationFamily::EnumerationTodo) {
				// Concrete Single/Double/Extended overloads are ranked
				// independently. Enumeration reaches the semantic handler
				// solely so it can report its deliberate TODO family.
				return std::nullopt;
			}
			if (parameter_index == 2 && !is_integer_semantic_type(source)) {
				return std::nullopt;
			}
		}
		if (builtin && parameter_index == 0 && (builtin->generic_kind == BuiltinGenericKind::AbsoluteValue || builtin->generic_kind == BuiltinGenericKind::OrdinalSuccessorOrPredecessor) && untyped_integer) {
			// An untyped integer literal is not yet a Pascal value of some
			// carrier type. These exact T -> T declarations need one before
			// selection can determine their result, so commit the literal to
			// the same natural carrier used elsewhere by the conversion
			// algebra. This is contextual literal construction, not an
			// implicit conversion edge and cannot form a conversion chain.
			Type* natural = integer_literal_natural_type(untyped_integer);
			actual = new Integer(untyped_integer->value, natural, untyped_integer->negative);
			source = natural;
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::EnumOrPointerStep && parameter_index == 1) {
			// The fallback relation is (T, Integer) -> T, but leaving only
			// its first formal generic would let a concrete candidate win
			// one argument while the fallback wins the other under the
			// existing Pareto matcher. Validate and convert the distance as
			// Integer here, then deliberately retain Generic rank for this
			// omitted formal. Thus every complete concrete declaration still
			// dominates the fallback without changing overload algebra.
			Parameter distance("amount", "p_amount", integer_type(), ParamMode::Value, nullptr);
			auto converted = match_argument(distance, actual, nullptr, 0, allow_declared_conversion, failure, conversion_failure);
			if (!converted) {
				return std::nullopt;
			}
			converted->rank = {MatchRank::Tier::Generic, 0};
			return converted;
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::SequenceLength && parameter_index == 0 && (!source || !source->sequence_element_type())) {
			return std::nullopt;
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::SequenceResize && parameter_index == 0 && (!source || !source->sequence_is_resizable())) {
			return std::nullopt;
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::AbsoluteValue && parameter_index == 0 && !generic_absolute_value_accepts(source)) {
			return std::nullopt;
		}
		const bool constrained_ordinal = builtin && is_generic_ordinal_operation(builtin->generic_kind) && parameter_index == 0;
		if (constrained_ordinal && !generic_ordinal_operation_accepts(builtin->generic_kind, source)) {
			if (failure) {
				*failure = MatchFailure::OrdinalRequired;
			}
			return std::nullopt;
		}
		if ((formal.mode == ParamMode::Var || formal.mode == ParamMode::Out)) {
			if (!is_referenceable(actual)) {
				if (failure) {
					*failure = MatchFailure::NotStorageBacked;
				}
				return std::nullopt;
			}
			if (contains_packed_projection(actual)) {
				if (failure) {
					*failure = MatchFailure::PackedProjection;
				}
				return std::nullopt;
			}
		}
		// An omitted Pascal formal type is a catch-all storage/value
		// coordinate. Generic is the worst rank at this position; it is not a
		// candidate-wide fallback category.
		return ArgumentMatch{{MatchRank::Tier::Generic, 0}, actual};
	}

	if (auto literal = dynamic_cast<BracketLiteral*>(actual)) {
		// A bracket expression is intentionally uncommitted while overloads
		// are ranked. Build a fresh set or array node for this candidate so
		// testing one candidate cannot change the expression seen by another.
		auto target_set = dynamic_cast<FixedSetType*>(target);
		auto target_dynamic = dynamic_cast<DynamicArrayType*>(target);
		auto target_open = dynamic_cast<OpenArrayType*>(target);
		if (!target_set && !target_dynamic && !target_open) {
			// The bracket expression is still uncommitted: although the
			// required final type is not a container, a single user
			// conversion may declare a set or array source formal and construct
			// this syntax directly in that context. Re-enter the ordinary
			// conversion-family lookup only from the outer match; matching the
			// operator's source formal passes allow_declared_conversion=false and
			// therefore cannot form an A -> B -> C chain.
			if (allow_declared_conversion) {
				return match_declared_conversion(actual, target, implicit_operator_identifier(directive_state.switch_enabled('r')), failure, conversion_failure);
			}
			return std::nullopt;
		}
		if (formal.mode == ParamMode::Var || formal.mode == ParamMode::Out) {
			return std::nullopt;
		}

		Type* item_type = target_set ? target_set->item_type : target_dynamic ? target_dynamic->item_type : target_open->item_type;
		Parameter item_formal("", "", item_type, ParamMode::Value, nullptr);
		MatchRank combined{MatchRank::Tier::Equal, 0};
		if (!target_set) {
			combined.contextual_construction = MatchRank::ContextualConstruction::Array;
		}
		std::vector<SetLiteral::Item> set_items;
		std::vector<Node*> array_items;
		if (target_set) {
			set_items.reserve(literal->items.size());
		} else {
			array_items.reserve(literal->items.size());
		}

		auto combine = [&](const MatchRank& rank) {
			if (static_cast<unsigned>(rank.tier) > static_cast<unsigned>(combined.tier)) {
				combined.tier = rank.tier;
			}
			combined.integer_sign_mismatch = combined.integer_sign_mismatch || rank.integer_sign_mismatch;
		};
		auto match_bracket_item = [&](Node* in_node) -> std::optional<ArgumentMatch> {
			if (auto m = contextual_subrange_constant_match(item_formal, in_node)) {
				return m;
			}
			return match_argument(item_formal, in_node, nullptr, 0, false, failure);
		};
		for (const BracketLiteral::Item& item : literal->items) {
			if (!target_set && item.upper) {
				return std::nullopt;
			}
			auto lower = match_bracket_item(item.lower);
			if (!lower) {
				return std::nullopt;
			}
			combine(lower->rank);
			if (!target_set) {
				array_items.push_back(lower->value);
				continue;
			}
			Node* upper_value = nullptr;
			if (item.upper) {
				auto upper = match_bracket_item(item.upper);
				if (!upper) {
					return std::nullopt;
				}
				combine(upper->rank);
				upper_value = upper->value;
			}
			set_items.push_back(SetLiteral::Item{lower->value, upper_value});
		}
		if (target_set) {
			return ArgumentMatch{combined, new SetLiteral(std::move(set_items), target)};
		}
		return ArgumentMatch{combined, new ArrayLiteral(std::move(array_items), target)};
	}

	if (auto open = dynamic_cast<OpenArrayType*>(target)) {
		// Open arrays expose element storage, not the nominal identity of the
		// source array variable. Exact element Type* is nevertheless required:
		// a view cannot perform an element conversion while preserving direct
		// reads and writes.
		if (!source || source->array_element_type() != open->item_type) {
			return std::nullopt;
		}
		if (formal.mode == ParamMode::Var || formal.mode == ParamMode::Out) {
			if (!is_referenceable(actual)) {
				if (failure) {
					*failure = MatchFailure::NotStorageBacked;
				}
				return std::nullopt;
			}
			if (contains_packed_projection(actual)) {
				if (failure) {
					*failure = MatchFailure::PackedProjection;
				}
				return std::nullopt;
			}
		}
		if (formal.mode == ParamMode::Const && contains_packed_projection(actual)) {
			if (failure) {
				*failure = MatchFailure::PackedProjection;
			}
			return std::nullopt;
		}
		Node* converted = nullptr;
		// These are distinct semantic operations, not cosmetic casts: const
		// aliases read-only storage, var aliases mutable storage, out resets
		// that storage before entry, and value owns a call-lifetime copy.
		switch (formal.mode) {
		case ParamMode::Const:
			converted = new OpenArrayConstView(actual, target);
			break;
		case ParamMode::Var:
			converted = new OpenArrayMutableView(actual, target);
			break;
		case ParamMode::Out:
			converted = new OpenArrayOutView(actual, target);
			break;
		case ParamMode::Value:
			converted = new OpenArrayValueCopy(actual, target);
			break;
		}
		return ArgumentMatch{{MatchRank::Tier::Equal, 0}, converted};
	}

	if (formal.mode == ParamMode::Var || formal.mode == ParamMode::Out) {
		if (!is_referenceable(actual)) {
			if (failure) {
				*failure = MatchFailure::NotStorageBacked;
			}
			return std::nullopt;
		}
		if (contains_packed_projection(actual)) {
			if (failure) {
				*failure = MatchFailure::PackedProjection;
			}
			return std::nullopt;
		}
		if (source == target) {
			return ArgumentMatch{{MatchRank::Tier::Exact, 0}, actual};
		}
		if ((dynamic_cast<DistinctType*>(source) || dynamic_cast<DistinctType*>(target)) && distinct_storage_type(source) == distinct_storage_type(target)) {
			// FPC `type Base` creates a separate overload identity, but
			// deliberately retains Base's storage identity for var/out.
			// This is a language relation of DistinctType, not a general
			// relaxation to equal-looking C++ carriers.
			return ArgumentMatch{{MatchRank::Tier::Equal, 0}, actual};
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::PointerStorage && target == pointer_type() && dynamic_cast<PointerType*>(source)) {
			// GetMem(out Pointer, ...) and ReAllocMem(var Pointer, ...)
			// explicitly operate on raw pointer storage. Keeping that contract
			// in builtin metadata prevents an RTL exception from weakening
			// every typed mutable-reference parameter in the language.
			return ArgumentMatch{{MatchRank::Tier::Equal, 0}, actual};
		}
		if (builtin && builtin->generic_kind == BuiltinGenericKind::ValOutput && parameter_index == 1 && formal.mode == ParamMode::Out) {
			auto range = dynamic_cast<SubrangeType*>(source);
			if (range && range->base_type == target) {
				return ArgumentMatch{{MatchRank::Tier::Equal, 0}, actual};
			}
		}
		return std::nullopt;
	}

	if (dynamic_cast<NilLiteral*>(actual) && (target->is_reference_type() || dynamic_cast<RoutineType*>(target))) {
		auto value = new NilLiteral();
		value->ty = target;
		return ArgumentMatch{{MatchRank::Tier::Equal, 0}, value};
	}

	if (auto reference = dynamic_cast<RoutineRef*>(actual)) {
		if (target == pointer_type()) {
			Node* resolved = try_resolve_routine_code_reference(reference);
			return resolved ? std::optional<ArgumentMatch>(ArgumentMatch{{MatchRank::Tier::Equal, 0}, resolved}) : std::nullopt;
		}
		auto routine = dynamic_cast<RoutineType*>(target);
		if (!routine) {
			return std::nullopt;
		}
		bool ambiguous = false;
		Node* resolved = try_resolve_routine_reference(reference, routine, &ambiguous);
		if (!resolved) {
			if (ambiguous && failure) {
				*failure = MatchFailure::AmbiguousConversion;
			}
			return std::nullopt;
		}
		return ArgumentMatch{{MatchRank::Tier::Equal, 0}, resolved};
	}

	if (auto literal = dynamic_cast<String*>(actual); literal && literal->contextual_literal && (dynamic_cast<ShortStringType*>(target) || target == ansistring_type())) {
		// A quoted literal is constructed directly in its selected string
		// context. This is not a unary conversion call: no source string value
		// has been materialized yet. Return a candidate-local, fully converted
		// value so testing one overload cannot alter another candidate.
		ConstEvalResult converted = const_convert_string(literal->value, target);
		if (converted.kind != ConstEvalResult::Kind::Success) {
			return std::nullopt;
		}
		return ArgumentMatch{{MatchRank::Tier::Equal, 0}, converted.node};
	}

	if (auto literal = untyped_real_constant(actual); literal && is_real_semantic_type(target)) {
		// Materialize separately for every candidate. The source node remains an
		// exact decimal origin, which makes a literal and an untyped const alias
		// genuinely substitution-equivalent during overload testing.
		RealMaterialization converted = materialize_decimal_origin(*literal->origin, target);
		if (converted.kind == RealMaterializationKind::InvalidTarget) {
			return std::nullopt;
		}
		MatchRank rank{
		    converted.kind == RealMaterializationKind::Exact        ? MatchRank::Tier::Equal
		    : converted.kind == RealMaterializationKind::OutOfRange ? MatchRank::Tier::ConvertNarrowing
		                                                            : MatchRank::Tier::Convert,
		    0,
		};
		rank.rounded_real_origin = converted.kind == RealMaterializationKind::Rounded;
		rank.information_losing = converted.kind != RealMaterializationKind::Exact;
		if (converted.kind == RealMaterializationKind::OutOfRange && directive_state.switch_enabled('r')) {
			// R+ changes only the conversion node executed after selection.
			// `rank` and candidate viability are identical under R+ and R-.
			return ArgumentMatch{rank, new RangeCheckedCast(actual, target)};
		}
		return ArgumentMatch{rank, new Real(converted.value, target)};
	}

	if (source == target) {
		return ArgumentMatch{{MatchRank::Tier::Exact, 0}, actual};
	}

	if (untyped_integer) {
		if (is_integer_semantic_type(target)) {
			if (Node* based = contextual_based_integer_value(actual, target)) {
				auto preference = integer_literal_target_preference(target);
				if (!preference) {
					return std::nullopt;
				}
				MatchRank rank{MatchRank::Tier::Equal, *preference};
				// The assignment is legal, but it changes the origin's
				// positive mathematical value into a signed carrier bit
				// pattern. A common domain which preserves the magnitude is
				// preferred when one is visible.
				rank.information_losing = true;
				return ArgumentMatch{rank, based};
			}
			// Literal fit needs only the ordinal endpoints. Reusing fixed-array
			// range construction here incorrectly rejects the complete Int64
			// domain because its element count is 2^64 and cannot fit in the
			// array length field.
			if (!integer_type_contains_literal(target, untyped_integer)) {
				return std::nullopt;
			}

			auto preference = integer_literal_target_preference(target);
			if (!preference) {
				return std::nullopt;
			}
			Type* natural = integer_literal_natural_type(untyped_integer);
			MatchRank rank{target == natural ? MatchRank::Tier::Exact : MatchRank::Tier::Equal, *preference};
			// An untyped constant has no source carrier and therefore no
			// signedness to preserve. Its magnitude-derived natural type
			// determines only Exact versus Equal. Applying the typed-source
			// signedness tie-breaker here can make different arguments prefer
			// different destinations even though every literal fits directly.
			return ArgumentMatch{rank, new Integer(untyped_integer->value, target, untyped_integer->negative)};
		}
		if (is_real_semantic_type(target)) {
			// An integer origin has a value, not a carrier-wide domain. Classify
			// this particular magnitude directly in each real destination:
			// e.g. 1 is exact in Single, 16777217 first becomes exact in Double,
			// and QWord.Max first becomes exact in binary80 Extended.
			DecimalOrigin origin;
			origin.negative = untyped_integer->negative;
			origin.digits = std::to_string(untyped_integer->value);
			RealMaterialization converted = materialize_decimal_origin(origin, target);
			if (converted.kind == RealMaterializationKind::InvalidTarget) {
				return std::nullopt;
			}
			MatchRank rank{
			    converted.kind == RealMaterializationKind::Exact        ? MatchRank::Tier::Equal
			    : converted.kind == RealMaterializationKind::OutOfRange ? MatchRank::Tier::ConvertNarrowing
			                                                            : MatchRank::Tier::Convert,
			    0,
			};
			rank.information_losing = converted.kind != RealMaterializationKind::Exact;
			if (converted.kind == RealMaterializationKind::OutOfRange && directive_state.switch_enabled('r')) {
				// R+ changes only the selected conversion's execution. It must
				// never reject or re-rank this overload candidate.
				return ArgumentMatch{rank, new RangeCheckedCast(actual, target)};
			}
			return ArgumentMatch{rank, new Real(converted.value, target)};
		}
	}
	if (source == &untyped_integer_type() && !untyped_integer) {
		// Untyped integer is a constant-expression category, never a runtime
		// storage type. If it cannot be folded here, accepting it through the
		// type-only conversion table would lose the magnitude required for
		// range checking and ranking.
		return std::nullopt;
	}
	if (source == &untyped_real_type()) {
		// Untyped real is a constant-expression category, never a runtime
		// storage type. Its exact decimal payload must be materialized by the
		// contextual branch above.
		return std::nullopt;
	}

	Type* assignment_source = overload_rank_type(source);
	std::optional<AssignmentConversion> assignment;
	if (assignment_source == target && assignment_source != source) {
		// A subrange actual is its base type for source ranking. Passing it to
		// that base formal is therefore Exact, while the candidate-local cast
		// still exposes the selected formal carrier to emission.
		return ArgumentMatch{{MatchRank::Tier::Exact, 0}, make_implicit_cast(actual, target)};
	} else {
		assignment = target->assignment_conversion_from(assignment_source);
	}

	if (allow_declared_conversion) {
		MatchFailure local_failure = MatchFailure::Incompatible;
		MatchFailure* declared_failure = failure ? failure : &local_failure;
		if (auto declared = match_declared_conversion(actual, target, implicit_operator_identifier(directive_state.switch_enabled('r')), declared_failure, conversion_failure)) {
			// A declaration implements the same source-to-result conversion edge
			// classified by the predefined relation. It produces a value and does
			// not perform the eventual assignment store. Rank that selected
			// conversion by its value-domain effect, not by whether its
			// implementation came from System, user source, or the compiler.
			if (assignment) {
				declared->rank.tier = assignment->kind == AssignmentConversionClass::Equal ? MatchRank::Tier::Equal : assignment->kind == AssignmentConversionClass::Narrowing ? MatchRank::Tier::ConvertNarrowing : MatchRank::Tier::Convert;
				declared->rank.distance = assignment->distance;
			}
			return declared;
		}
		// An ambiguous ordinary conversion is a failure at its own quality;
		// do not silently replace it with a worse predefined narrowing.
		if (*declared_failure == MatchFailure::AmbiguousConversion) {
			return std::nullopt;
		}
	}

	if (assignment) {
		MatchRank rank{
		    assignment->kind == AssignmentConversionClass::Equal       ? MatchRank::Tier::Equal
		    : assignment->kind == AssignmentConversionClass::Narrowing ? MatchRank::Tier::ConvertNarrowing
		                                                               : MatchRank::Tier::Convert,
		    assignment->distance,
		};
		rank.information_losing = assignment->kind == AssignmentConversionClass::Narrowing;
		auto source_signed = integer_carrier_is_signed(assignment_source);
		auto target_signed = integer_carrier_is_signed(target);
		rank.integer_sign_mismatch = source_signed && target_signed && *source_signed != *target_signed;
		if (is_integer_semantic_type(assignment_source) && is_real_semantic_type(target)) {
			// Unlike an origin, a typed actual can hold every value in its
			// declared carrier. Record whether the complete carrier survives;
			// common-domain operator selection consumes this bit jointly.
			rank.information_losing = !integer_domain_is_exact_in_real(assignment_source, target);
		}
		return ArgumentMatch{rank, make_implicit_cast(actual, target)};
	}
	return std::nullopt;
}

std::optional<CallableMatch> Parser::match_callable_arguments(Callable* callable, const std::vector<Node*>& args, bool allow_declared_conversion, OverloadResolutionPolicy resolution_policy) {
	auto signature = static_cast<RoutineType*>(callable->ty);
	if (args.size() > signature->formals.size()) {
		return std::nullopt;
	}
	for (size_t i = args.size(); i < signature->formals.size(); ++i) {
		if (!signature->formals[i].default_value) {
			return std::nullopt;
		}
	}

	const BuiltinDesc* builtin = callable->builtin_desc ? callable->builtin_desc : lookup_builtin_desc(callable->cxx_name);
	if (builtin && builtin->generic_kind == BuiltinGenericKind::EnumComparison) {
		// Validate the synthesized (E, E) relation before it enters the
		// overload cohort. A catch-all candidate for unrelated operands would
		// otherwise create a false ambiguity and report its constraint only
		// after selection.
		if (args.size() != 2 || !args[0] || !args[1]) {
			return std::nullopt;
		}
		Type* left = overload_rank_type(args[0]->ty);
		Type* right = overload_rank_type(args[1]->ty);
		if (!left || left != right || !dynamic_cast<EnumType*>(left)) {
			return std::nullopt;
		}
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::EnumOrDynamicArrayEquality) {
		if (args.size() != 2 || !args[0] || !args[1]) {
			return std::nullopt;
		}
		Type* left = overload_rank_type(args[0]->ty);
		Type* right = overload_rank_type(args[1]->ty);
		const bool same_enum = left && left == right && dynamic_cast<EnumType*>(left);
		auto left_array = dynamic_cast<DynamicArrayType*>(left);
		auto right_array = dynamic_cast<DynamicArrayType*>(right);
		DynamicArrayType* array_type = nullptr;
		if (left_array && right_array && left_array == right_array) {
			array_type = left_array;
		} else if (left_array && dynamic_cast<NilLiteral*>(args[1])) {
			array_type = left_array;
		} else if (right_array && dynamic_cast<NilLiteral*>(args[0])) {
			array_type = right_array;
		}
		if (!same_enum && !array_type) {
			return std::nullopt;
		}
		if (array_type) {
			Node* first = args[0];
			Node* second = args[1];
			if (dynamic_cast<NilLiteral*>(first)) {
				auto nil = new NilLiteral();
				nil->ty = array_type;
				first = nil;
			}
			if (dynamic_cast<NilLiteral*>(second)) {
				auto nil = new NilLiteral();
				nil->ty = array_type;
				second = nil;
			}
			return CallableMatch{
			    {{MatchRank::Tier::Generic, 0}, {MatchRank::Tier::Generic, 0}},
			    {first, second},
			    {nullptr, nullptr},
			};
		}
	}
	if (resolution_policy != OverloadResolutionPolicy::Ordinary && resolution_policy != OverloadResolutionPolicy::CommonBinaryPointerLeftOrEnumStep && builtin && builtin->generic_kind == BuiltinGenericKind::EnumOrPointerStep && !args.empty() && args[0]) {
		// Expression arithmetic never provides predefined Enum +/- Integer or
		// Char +/- Integer operations. The same internal relation remains
		// available to the separately specified Inc/Dec mutation operations,
		// while expression syntax requires an explicitly matching declaration.
		Type* first = overload_rank_type(args[0]->ty);
		if (dynamic_cast<EnumType*>(first) || first == char_type()) {
			return std::nullopt;
		}
	}
	if (builtin && (builtin->generic_kind == BuiltinGenericKind::SetBinaryOperation || builtin->generic_kind == BuiltinGenericKind::SetComparison)) {
		// This is one ordinary root-frame candidate whose Pascal declaration
		// cannot spell `(set of T, set of T) -> set of T`. Determine T only
		// for this candidate, then use the existing argument matcher to
		// construct bracket literals and perform admitted set widening.
		if (args.size() != 2 || !args[0] || !args[1]) {
			return std::nullopt;
		}
		auto first_literal = dynamic_cast<BracketLiteral*>(args[0]);
		auto second_literal = dynamic_cast<BracketLiteral*>(args[1]);
		auto first_set = dynamic_cast<FixedSetType*>(args[0]->ty);
		auto second_set = dynamic_cast<FixedSetType*>(args[1]->ty);
		if (!first_literal && !first_set) {
			return std::nullopt;
		}
		if (!second_literal && !second_set) {
			return std::nullopt;
		}

		FixedSetType* common_set = nullptr;
		if (first_set && second_set) {
			auto first_accepts_second = first_set->value_conversion_from(second_set);
			auto second_accepts_first = second_set->value_conversion_from(first_set);
			if (!first_accepts_second && !second_accepts_first) {
				return std::nullopt;
			}
			if (first_accepts_second && (!second_accepts_first || conversion_is_better(*first_accepts_second, *second_accepts_first))) {
				common_set = first_set;
			} else if (second_accepts_first && (!first_accepts_second || conversion_is_better(*second_accepts_first, *first_accepts_second))) {
				common_set = second_set;
			} else {
				// Equal conversions mean structurally equivalent set
				// domains. Retaining the left type makes the result
				// deterministic without introducing a preference into
				// overload ranking.
				common_set = first_set;
			}
		} else if (first_set) {
			common_set = first_set;
		} else if (second_set) {
			common_set = second_set;
		} else {
			Type* first_item = first_literal->default_set_item_type;
			Type* second_item = second_literal->default_set_item_type;
			Type* common_item = nullptr;
			if (first_item == unknown_type()) {
				common_item = second_item;
			} else if (second_item == unknown_type()) {
				common_item = first_item;
			} else {
				common_item = infer_set_item_type(first_item, second_item);
			}
			if (!common_item || common_item == unknown_type()) {
				return std::nullopt;
			}
			common_set = new FixedSetType(current_location(), common_item);
		}

		Parameter set_formal("set", "", common_set, ParamMode::Const, nullptr);
		auto first_match = match_argument(set_formal, args[0], nullptr, 0, false);
		auto second_match = match_argument(set_formal, args[1], nullptr, 1, false);
		if (!first_match || !second_match) {
			return std::nullopt;
		}

		// The declaration omits the complete set type, so this fallback stays
		// below every viable typed custom operator. Candidate-local literal
		// construction is not a reason to raise its overload rank.
		return CallableMatch{
		    {
		        {MatchRank::Tier::Generic, 0},
		        {MatchRank::Tier::Generic, 0},
		    },
		    {
		        first_match->value,
		        second_match->value,
		    },
		    {nullptr, nullptr},
		};
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::EnumOrPointerStep && args.size() == 2 && args[0] && args[1]) {
		auto pointer = dynamic_cast<PointerType*>(args[0]->ty);
		if (pointer) {
			// Pascal cannot spell the generic declaration
			//
			//   (^T, U: integer ordinal) -> ^T
			//
			// used by pointer +/- distance and two-argument
			// Inc/Dec. In particular, forcing U through Integer would
			// reject an unsigned Cardinal variable (and would truncate
			// wider pointer-sized distances). Preserve U exactly; the
			// pointer RTL overload extracts its ordinal storage.
			if (pointer->is_untyped()) {
				return std::nullopt;
			}
			Node* amount = args[1];
			if (auto literal = untyped_integer_constant(amount)) {
				Type* natural = integer_literal_natural_type(literal);
				amount = new Integer(literal->value, natural, literal->negative);
			}
			if (!is_integer_semantic_type(amount->ty)) {
				return std::nullopt;
			}

			// Both Pascal formals are unspellable, so this fallback
			// remains below every viable fully typed custom operator.
			// No conversion has occurred: a typed amount remains the
			// same typed expression for the selected RTL template.
			return CallableMatch{
			    {
			        {MatchRank::Tier::Generic, 0},
			        {MatchRank::Tier::Generic, 0},
			    },
			    {args[0], amount},
			    {nullptr, nullptr},
			};
		}
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::PointerDifference) {
		// The root declaration's Pointer formals distinguish this overload
		// from `(T, Integer) -> T`; they must not erase typed operands before
		// emission. Recover the unspellable `(^T, ^T) -> PtrInt` relation
		// candidate-locally, using the same ordinary overload family and
		// dominance rules as every concrete custom Subtract declaration.
		if (args.size() != 2 || !args[0] || !args[1]) {
			return std::nullopt;
		}
		auto first_type = dynamic_cast<PointerType*>(args[0]->ty);
		auto second_type = dynamic_cast<PointerType*>(args[1]->ty);
		if (!first_type || !second_type) {
			return std::nullopt;
		}

		if (first_type->is_untyped() || second_type->is_untyped() || first_type->item_type != second_type->item_type) {
			// Element distance has no coherent unit for two different
			// typed pointees. Untyped Pointer supplies no C++ array element
			// or allocation metadata at all. Do not silently turn either
			// case into integer-address or byte-pointer arithmetic.
			return std::nullopt;
		}
		PointerType* common_type = first_type;

		Parameter pointer_formal("pointer", "", common_type, ParamMode::Value, nullptr);
		auto first_match = match_argument(pointer_formal, args[0], nullptr, 0, false);
		auto second_match = match_argument(pointer_formal, args[1], nullptr, 1, false);
		if (!first_match || !second_match) {
			return std::nullopt;
		}

		// The Pascal declaration omits T, so this fallback remains below
		// every viable fully typed declaration. Converted arguments preserve
		// T for the selected C++ pointer-difference overload.
		return CallableMatch{
		    {
		        {MatchRank::Tier::Generic, 0},
		        {MatchRank::Tier::Generic, 0},
		    },
		    {
		        first_match->value,
		        second_match->value,
		    },
		    {nullptr, nullptr},
		};
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::SetMembership) {
		// Only System's omitted-type declaration enters here. Custom In
		// declarations have complete Pascal formals and use the ordinary loop
		// below. Build this candidate's missing integer-domain/set-domain or
		// nominal `(T, set of T)` relationship without changing either source
		// node or introducing a lookup path.
		if (args.size() != 2 || !args[0] || !args[1]) {
			return std::nullopt;
		}

		Type* item_type = nullptr;
		Node* values = args[1];
		if (auto literal = dynamic_cast<BracketLiteral*>(values)) {
			item_type = literal->default_set_item_type;
			if (item_type == unknown_type()) {
				if (auto integer = untyped_integer_constant(args[0])) {
					item_type = integer_literal_natural_type(integer);
				} else {
					item_type = args[0]->ty;
				}
			}
			if (!is_set_item_type(item_type)) {
				return std::nullopt;
			}
			auto contextual_set = new FixedSetType(current_location(), item_type);
			Parameter values_formal("values", "", contextual_set, ParamMode::Const, nullptr);
			auto values_match = match_argument(values_formal, values, nullptr, 1, false);
			if (!values_match) {
				return std::nullopt;
			}
			values = values_match->value;
		} else {
			auto set_type = dynamic_cast<FixedSetType*>(values->ty);
			if (!set_type) {
				return std::nullopt;
			}
			item_type = set_type->item_type;
		}
		if (!is_set_item_type(item_type)) {
			return std::nullopt;
		}

		Node* item = args[0];
		const bool heterogeneous_integer_membership = item->ty != &untyped_integer_type() && is_integer_semantic_type(item->ty) && is_integer_semantic_type(item_type);
		if (!heterogeneous_integer_membership) {
			Parameter item_formal("item", "", item_type, ParamMode::Const, nullptr);
			auto item_match = match_argument(item_formal, item, nullptr, 0, allow_declared_conversion);
			if (!item_match) {
				return std::nullopt;
			}
			item = item_match->value;
		}
		// Do not narrow an Integer value to a smaller integer set carrier.
		// Membership is a comparison, not storage: a value outside the set's
		// representable domain is simply absent. Keeping the source value also
		// matches the RTL's deliberately heterogeneous
		// o_in(Value, t_set<Element>) contract.

		// Both declared formals are omitted, so both retain Generic rank
		// regardless of the candidate-local contextual construction. The
		// ordinary product comparison therefore makes a fully typed candidate
		// better at each of these coordinates; no preference is hard-coded for
		// System or users.
		return CallableMatch{
		    {
		        {MatchRank::Tier::Generic, 0},
		        {MatchRank::Tier::Generic, 0},
		    },
		    {item, values},
		    {nullptr, nullptr},
		};
	}
	CallableMatch result;
	result.ranks.reserve(args.size());
	result.arguments.reserve(args.size());
	result.formal_types.reserve(args.size());
	for (size_t i = 0; i < args.size(); ++i) {
		auto match = match_argument(signature->formals[i], args[i], builtin, i, allow_declared_conversion);
		if (!match) {
			return std::nullopt;
		}
		result.ranks.push_back(match->rank);
		result.arguments.push_back(match->value);
		result.formal_types.push_back(signature->formals[i].ty);
	}
	return result;
}

bool Parser::has_direct_assignment_edge(Type* source, Type* target) {
	// When two overloads both need one conversion, the narrower destination
	// is the one which itself assigns directly to the other destination.
	// Query the same directed relation used for actual arguments: intrinsic
	// type relations and operator := declarations contribute indistinguishable
	// edges. Requiring the operator's source formal to be exactly SOURCE is
	// essential; accepting a merely convertible formal here would turn this
	// one-edge question into an accidental SOURCE -> FORMAL -> TARGET chain.
	if (!source || !target) {
		return false;
	}
	source = overload_rank_type(source);
	target = overload_rank_type(target);
	if (source == target || source->is_subtype_of(target) || target->value_conversion_from(source)) {
		return true;
	}

	Node* family = maybe_resolve_value(std::string(implicit_operator_identifier(directive_state.switch_enabled('r'))));
	if (auto member = dynamic_cast<MemberAccess*>(family)) {
		if (!dynamic_cast<UnitRef*>(member->a)) {
			return false;
		}
		family = member->b;
	}

	std::vector<Callable*> candidates;
	if (auto callable = dynamic_cast<Callable*>(family)) {
		candidates.push_back(callable);
	} else if (auto overloads = dynamic_cast<OverloadSet*>(family)) {
		candidates = overloads->members;
	}
	for (Callable* candidate : candidates) {
		if (!dynamic_cast<Procedure*>(candidate) || candidate->ty->kind != ROUTINE || candidate->ty->return_type != target || candidate->ty->formals.size() != 1) {
			continue;
		}
		if (candidate->ty->formals[0].ty == source) {
			return true;
		}
	}
	return false;
}

std::optional<ArgumentMatch> Parser::match_declared_conversion(Node* actual, Type* target, std::string_view operator_identifier, MatchFailure* failure, DeclaredConversionFailure* conversion_failure) {
	// The source construct chooses one canonical conversion identity before
	// ordinary frame lookup: implicit contexts pass their {$R}-selected
	// family, while explicit syntax invokes this same function once per
	// ordered fallback family. Nothing below distinguishes System, user, or
	// compiler-provided declarations. Keeping that boundary here prevents a
	// second conversion resolver from giving built-in and source-defined
	// operations different exact-result or single-edge behavior.
	Node* family = maybe_resolve_value(std::string(operator_identifier));
	if (auto member = dynamic_cast<MemberAccess*>(family)) {
		// Unit qualification opens a standalone operator environment; an
		// object member environment would be a different, unsupported
		// operator model.
		if (!dynamic_cast<UnitRef*>(member->a)) {
			return std::nullopt;
		}
		family = member->b;
	}

	std::vector<Callable*> candidates;
	if (auto callable = dynamic_cast<Callable*>(family)) {
		candidates.push_back(callable);
	} else if (auto overloads = dynamic_cast<OverloadSet*>(family)) {
		candidates = overloads->members;
	}
	if (conversion_failure) {
		conversion_failure->candidates = candidates;
		conversion_failure->viable.clear();
		conversion_failure->non_dominated.clear();
		conversion_failure->ambiguous = false;
	}

	Callable* best_candidate = nullptr;
	std::optional<ArgumentMatch> best_source;
	Type* best_source_formal = nullptr;
	std::vector<Callable*> best_candidates;
	auto source_is_single_edge = [&](const Parameter& formal, const ArgumentMatch& match) {
		if (match.rank.tier == MatchRank::Tier::Exact) {
			return true;
		}
		// An uncommitted literal has no typed A value which must first
		// undergo an A -> B conversion. Candidate-local construction of
		// that literal directly as the declared source formal B therefore
		// leaves the declared B -> C operation as the one conversion edge.
		// Keep this list tied to the CST forms which match_argument
		// actually contextualizes; a favorable Direct rank by itself
		// must never reopen typed widening, narrowing, or subtyping.
		if (untyped_integer_constant(actual)) {
			return match.value && match.value->ty == formal.ty && dynamic_cast<Integer*>(match.value);
		}
		if (dynamic_cast<BracketLiteral*>(actual)) {
			return match.value && match.value != actual && match.value->ty == formal.ty && (dynamic_cast<SetLiteral*>(match.value) || dynamic_cast<ArrayLiteral*>(match.value));
		}
		if (dynamic_cast<NilLiteral*>(actual)) {
			return match.value && match.value != actual && match.value->ty == formal.ty && dynamic_cast<NilLiteral*>(match.value);
		}
		if (dynamic_cast<String*>(actual)) {
			return match.value && match.value != actual && match.value->ty == formal.ty && dynamic_cast<String*>(match.value);
		}
		return false;
	};
	for (Callable* candidate : candidates) {
		if (!dynamic_cast<Procedure*>(candidate) || candidate->ty->kind != ROUTINE || candidate->ty->return_type != target || candidate->ty->formals.size() != 1) {
			continue;
		}
		const BuiltinDesc* builtin = candidate->builtin_desc ? candidate->builtin_desc : lookup_builtin_desc(candidate->cxx_name);
		auto source_match = match_argument(candidate->ty->formals[0], actual, builtin, 0, false);
		if (!source_match || !source_is_single_edge(candidate->ty->formals[0], *source_match)) {
			// One implicit conversion is one edge. A conversion operator
			// declared B -> C cannot first convert an A actual to its B
			// source formal; only an exact A -> C declaration is a candidate.
			// The sole exception above directly constructs an uncommitted
			// literal as B, because no typed A exists in that case.
			continue;
		}
		if (conversion_failure) {
			conversion_failure->viable.push_back({candidate, CallableMatch{{source_match->rank}, {source_match->value}, {candidate->ty->formals[0].ty}}});
		}
		if (!best_source || rank_less(source_match->rank, candidate->ty->formals[0].ty, best_source->rank, best_source_formal, [this](Type* source, Type* destination) { return has_direct_assignment_edge(source, destination); })) {
			best_candidate = candidate;
			best_source = *source_match;
			best_source_formal = candidate->ty->formals[0].ty;
			best_candidates = {candidate};
		} else if (!rank_less(best_source->rank, best_source_formal, source_match->rank, candidate->ty->formals[0].ty, [this](Type* source, Type* destination) { return has_direct_assignment_edge(source, destination); })) {
			best_candidates.push_back(candidate);
		}
	}
	if (conversion_failure) {
		conversion_failure->non_dominated = best_candidates;
	}
	if (!best_source) {
		return std::nullopt;
	}
	if (best_candidates.size() != 1) {
		if (failure) {
			*failure = MatchFailure::AmbiguousConversion;
		}
		if (conversion_failure) {
			conversion_failure->ambiguous = true;
		}
		return std::nullopt;
	}

	// The selected declaration supplies one ordinary implicit-conversion edge.
	// Its origin does not create another overload rank.
	auto call = new ProcCall(nullptr, best_candidate, std::vector<Node*>{best_source->value});
	call->ty = target;
	MatchRank rank{MatchRank::Tier::Convert, best_source->rank.distance};
	auto source_signed = integer_carrier_is_signed(actual ? actual->ty : nullptr);
	auto target_signed = integer_carrier_is_signed(target);
	rank.integer_sign_mismatch = source_signed && target_signed && *source_signed != *target_signed;
	return ArgumentMatch{rank, call};
}

Node* Parser::match_explicit_conversion(Node* actual, Type* target, bool implicit_fallback) {
	struct Family {
		std::string_view diagnostic_name;
		std::string_view identifier;
	};

	const std::array<Family, 1> explicit_families{{
	    {"explicit", explicit_operator_identifier()},
	}};
	const std::array<Family, 2> implicit_families{{
	    {"implicit", implicit_operator_identifier(true)},
	    {"uncheckedimplicit", implicit_operator_identifier(false)},
	}};

	// Explicit is searched before the predefined Target(value) boundary.
	// Implicit and UncheckedImplicit are searched afterward, as fallback
	// contracts. Keeping those phases separate prevents a System widening
	// declaration from intercepting a direct predefined cast while still
	// allowing a declared Explicit operation to override it. Within the
	// fallback phase checked Implicit precedes UncheckedImplicit; the cast
	// itself never consults {$R}. Every family enters
	// match_declared_conversion(), whose source match forbids A -> B -> T
	// chaining.
	auto search = [&](const auto& families) -> Node* {
		for (const Family& family : families) {
			MatchFailure failure = MatchFailure::Incompatible;
			DeclaredConversionFailure conversion_failure;
			auto match = match_declared_conversion(actual, target, family.identifier, &failure, &conversion_failure);
			if (match) {
				return match->value;
			}
			if (failure != MatchFailure::AmbiguousConversion) {
				continue;
			}

			std::vector<Node*> args{actual};
			raise_overload_resolution_error(current_location(), std::string(family.diagnostic_name), nullptr, args, target, conversion_failure.candidates, conversion_failure.viable, conversion_failure.non_dominated, true, "ambiguous explicit conversion");
		}
		return nullptr;
	};
	return implicit_fallback ? search(implicit_families) : search(explicit_families);
}

static bool dominates(const CallableMatch& a, const CallableMatch& b, const std::function<bool(Type*, Type*)>& direct_assignment_edge) {
	if (a.ranks.size() != b.ranks.size() || a.formal_types.size() != a.ranks.size() || b.formal_types.size() != b.ranks.size()) {
		return false;
	}
	// Candidate quality is the product order of the per-argument conversion
	// relations. No argument may compensate for another: A dominates B only
	// when A is no worse at every corresponding position and strictly better
	// somewhere. Consequently appending an equally compatible formal/actual
	// coordinate cannot change an existing overload choice.
	bool strict = false;
	for (size_t i = 0; i < a.ranks.size(); ++i) {
		if (rank_less(b.ranks[i], b.formal_types[i], a.ranks[i], a.formal_types[i], direct_assignment_edge)) {
			return false;
		}
		if (rank_less(a.ranks[i], a.formal_types[i], b.ranks[i], b.formal_types[i], direct_assignment_edge)) {
			strict = true;
		}
	}
	return strict;
}

static bool callable_match_has_generic(const CallableMatch& match) {
	return std::ranges::any_of(match.ranks, [](const MatchRank& rank) { return rank.tier == MatchRank::Tier::Generic; });
}

static MatchRank::Tier callable_match_phase(const CallableMatch& match) {
	MatchRank::Tier result = MatchRank::Tier::Exact;
	for (const MatchRank& rank : match.ranks) {
		if (static_cast<unsigned>(rank.tier) > static_cast<unsigned>(result)) {
			result = rank.tier;
		}
	}
	return result;
}

static bool pointer_offset_formals(Callable* callable, OverloadResolutionPolicy policy) {
	if (!callable || (policy != OverloadResolutionPolicy::CommonBinaryPointerLeft && policy != OverloadResolutionPolicy::CommonBinaryPointerLeftOrEnumStep)) {
		return false;
	}
	auto signature = dynamic_cast<RoutineType*>(callable->ty);
	if (!signature || signature->formals.size() != 2) {
		return false;
	}
	Type* first = overload_rank_type(signature->formals[0].ty);
	Type* second = overload_rank_type(signature->formals[1].ty);
	const bool pointer_then_integer = dynamic_cast<PointerType*>(first) && is_integer_semantic_type(second);
	return pointer_then_integer;
}

static bool candidate_admitted_in_phase(Callable* callable, const CallableMatch& match, MatchRank::Tier phase, OverloadResolutionPolicy policy) {
	if (policy == OverloadResolutionPolicy::Ordinary || phase == MatchRank::Tier::Exact || phase == MatchRank::Tier::Equal) {
		return true;
	}
	if (phase != MatchRank::Tier::Convert && phase != MatchRank::Tier::ConvertNarrowing) {
		return false;
	}
	auto signature = callable ? dynamic_cast<RoutineType*>(callable->ty) : nullptr;
	if (!signature || signature->formals.size() != 2 || match.formal_types.size() != 2) {
		return false;
	}
	Type* first = overload_rank_type(match.formal_types[0]);
	Type* second = overload_rank_type(match.formal_types[1]);
	const bool homogeneous = first && first == second;
	if (policy == OverloadResolutionPolicy::CommonIntegerBinary) {
		return homogeneous && is_integer_semantic_type(first);
	}
	return homogeneous || pointer_offset_formals(callable, policy);
}

static bool common_domain_policy(OverloadResolutionPolicy policy) {
	return policy == OverloadResolutionPolicy::CommonBinary || policy == OverloadResolutionPolicy::CommonIntegerBinary || policy == OverloadResolutionPolicy::CommonBinaryPointerLeft || policy == OverloadResolutionPolicy::CommonBinaryPointerLeftOrEnumStep;
}

static Type* homogeneous_common_formal(const CallableMatch& match) {
	if (match.formal_types.size() != 2) {
		return nullptr;
	}
	Type* first = overload_rank_type(match.formal_types[0]);
	Type* second = overload_rank_type(match.formal_types[1]);
	return first && first == second ? first : nullptr;
}

/** Select the cohort which participates in product-order dominance.
 * Fully-typed candidates are phase-filtered. Catch-all coordinates remain
 * ordinary per-argument ranks and join the best typed cohort rather than
 * creating a candidate-wide Generic phase. */
static std::vector<size_t> overload_resolution_cohort(const std::vector<std::pair<Callable*, CallableMatch>>& viable, OverloadResolutionPolicy policy) {
	std::vector<size_t> exact;
	std::vector<size_t> generic;
	std::optional<MatchRank::Tier> best_typed_phase;

	for (size_t i = 0; i < viable.size(); ++i) {
		const CallableMatch& match = viable[i].second;
		if (callable_match_has_generic(match)) {
			generic.push_back(i);
			continue;
		}
		const MatchRank::Tier phase = callable_match_phase(match);
		if (phase == MatchRank::Tier::Exact) {
			exact.push_back(i);
			continue;
		}
		if (!candidate_admitted_in_phase(viable[i].first, match, phase, policy)) {
			continue;
		}
		if (!best_typed_phase || static_cast<unsigned>(phase) < static_cast<unsigned>(*best_typed_phase)) {
			best_typed_phase = phase;
		}
	}

	if (!exact.empty()) {
		return exact;
	}

	std::vector<size_t> result;
	if (best_typed_phase) {
		// Compare a homogeneous proposal once for the whole operand pair.
		// This applies in Equal as well as conversion phases: contextual
		// origins can otherwise make each coordinate prefer a different
		// carrier even though an operator needs one domain for both.
		bool have_lossless_common_domain = false;
		int best_lossy_real_domain = -1;
		if (common_domain_policy(policy)) {
			for (size_t i = 0; i < viable.size(); ++i) {
				const CallableMatch& match = viable[i].second;
				if (callable_match_has_generic(match) || callable_match_phase(match) != *best_typed_phase || !candidate_admitted_in_phase(viable[i].first, match, *best_typed_phase, policy)) {
					continue;
				}
				Type* common = homogeneous_common_formal(match);
				if (!common) {
					continue;
				}
				const bool loses_information = std::ranges::any_of(match.ranks, [](const MatchRank& rank) { return rank.information_losing; });
				if (!loses_information) {
					have_lossless_common_domain = true;
				} else {
					best_lossy_real_domain = std::max(best_lossy_real_domain, real_semantic_rank(common));
				}
			}
		}
		for (size_t i = 0; i < viable.size(); ++i) {
			const CallableMatch& match = viable[i].second;
			if (!callable_match_has_generic(match) && callable_match_phase(match) == *best_typed_phase && candidate_admitted_in_phase(viable[i].first, match, *best_typed_phase, policy)) {
				const bool loses_information = std::ranges::any_of(match.ranks, [](const MatchRank& rank) { return rank.information_losing; });
				Type* common = homogeneous_common_formal(match);
				if (common) {
					if (have_lossless_common_domain && loses_information) {
						continue;
					}
					if (!have_lossless_common_domain && best_lossy_real_domain >= 0 && loses_information && real_semantic_rank(common) != best_lossy_real_domain) {
						continue;
					}
				}
				result.push_back(i);
			}
		}
	}
	result.insert(result.end(), generic.begin(), generic.end());
	return result;
}

Node* Parser::make_implicit_cast(Node* value, Type* target) {
	if (value && value->ty && directive_state.switch_enabled('r') && conversion_requires_range_check(value->ty, target)) {
		// Preserve the check only when the source type's complete ordinal
		// domain or real exponent range is not already known to fit. This is a
		// semantic no-op optimization: overload viability and the conversion
		// selected above are unchanged.
		return new RangeCheckedCast(value, target);
	}
	return new Cast(value, target);
}

Node* Parser::cast(Node* a, Type* target_ty) {
	return cast_impl(a, target_ty);
}

Node* Parser::cast_for_destination(Node* a, Type* target_ty) {
	return cast_impl(a, target_ty);
}

Node* Parser::cast_impl(Node* a, Type* target_ty) {
	if (target_ty == unknown_type()) {
		// An omitted-type value is a raw view of the source expression, not a
		// value conversion to a fictional semantic type.
		return new Cast(new AddrOf(a), target_ty);
	}
	if (!target_ty) {
		raise_parse_error("implicit conversion has no target type");
	}
	// A fixed destination is the one-coordinate form of a value-parameter
	// call. Reusing match_argument here keeps acceptance and the constructed
	// conversion identical; overload resolution merely repeats this operation
	// for each candidate's proposed formal type.
	Parameter formal("", "", target_ty, ParamMode::Value, nullptr);
	MatchFailure failure = MatchFailure::Incompatible;
	DeclaredConversionFailure conversion_failure;
	auto match = match_argument(formal, a, nullptr, 0, true, &failure, &conversion_failure);
	if (match) {
		return match->value;
	}
	if (auto reference = dynamic_cast<RoutineRef*>(a)) {
		// Matching above is the sole compatibility decision. These calls only
		// reconstruct the richer final-context diagnostic on failure.
		if (target_ty == pointer_type()) {
			return resolve_routine_code_reference(reference);
		}
		if (auto routine = dynamic_cast<RoutineType*>(target_ty)) {
			return resolve_routine_reference(reference, routine);
		}
	}
	if (dynamic_cast<NilLiteral*>(a)) {
		raise_type_kind_mismatch("'nil' conversion target", "reference or routine-value", target_ty);
	}
	std::vector<Node*> args{a};
	const std::string conversion_name = directive_state.switch_enabled('r') ? "implicit" : "uncheckedimplicit";
	// Every failed contextual conversion went through the same selected
	// conversion-family search, even when that family contains no declaration
	// with the required exact result. Preserve its candidate set and the
	// actual expression as diagnostic roots; falling back to a two-Type*
	// mismatch discards the callable family and most of the value/type graph.
	raise_overload_resolution_error(current_location(), conversion_name, nullptr, args, target_ty, conversion_failure.candidates, conversion_failure.viable, conversion_failure.non_dominated, conversion_failure.ambiguous, conversion_failure.ambiguous ? "ambiguous implicit conversion" : "no implicit conversion to the required type");
}

static std::vector<Callable*> routine_reference_candidates(Node* candidates_node) {
	if (auto callable = dynamic_cast<Callable*>(candidates_node)) {
		return {callable};
	} else if (auto overloads = dynamic_cast<OverloadSet*>(candidates_node)) {
		return overloads->members;
	}
	return {};
}

Node* Parser::try_resolve_routine_reference(RoutineRef* reference, RoutineType* target_ty, bool* ambiguous) {
	if (ambiguous) {
		*ambiguous = false;
	}
	if (!reference || !target_ty || (target_ty->kind != ROUTINE && target_ty->kind != METHOD)) {
		return nullptr;
	}
	std::vector<Callable*> candidates = routine_reference_candidates(reference->candidates);
	std::vector<Callable*> compatible;
	for (Callable* candidate : candidates) {
		bool category_matches = false;
		if (target_ty->kind == ROUTINE) {
			category_matches = (dynamic_cast<Procedure*>(candidate) && candidate->ty->kind == ROUTINE && reference->receiver == nullptr) || (dynamic_cast<Method*>(candidate) && static_cast<Method*>(candidate)->is_static && candidate->ty->kind == ROUTINE);
		} else {
			category_matches = dynamic_cast<Method*>(candidate) && !static_cast<Method*>(candidate)->is_static && (candidate->ty->kind == METHOD || candidate->ty->kind == CLASS_METHOD) && reference->receiver != nullptr;
		}
		if (category_matches && target_ty->accepts_routine_value_from(candidate->ty)) {
			compatible.push_back(candidate);
		}
	}

	if (compatible.size() != 1) {
		if (ambiguous && compatible.size() > 1) {
			*ambiguous = true;
		}
		return nullptr;
	}
	if (target_ty->kind == METHOD && reference->receiver && !(reference->receiver->ty && reference->receiver->ty->is_reference_type()) && !is_referenceable(reference->receiver)) {
		return nullptr;
	}

	auto result = new RoutineRef(reference->receiver, reference->candidates);
	result->resolved = compatible.front();
	result->ty = target_ty;
	if (auto method = dynamic_cast<Method*>(result->resolved); method && method->is_static) {
		Node* qualifier = result->receiver;
		result->receiver = nullptr;
		if (qualifier && !dynamic_cast<ClassRefValue*>(qualifier) && !dynamic_cast<TypeMemberQualifier*>(qualifier)) {
			return new EvaluateThen(qualifier, result);
		}
	}
	return result;
}

Node* Parser::resolve_routine_reference(RoutineRef* reference, RoutineType* target_ty) {
	if (!reference || !target_ty) {
		raise_routine_reference_error("invalid contextual routine reference", reference, target_ty);
	}
	if (target_ty->kind != ROUTINE && target_ty->kind != METHOD) {
		raise_routine_reference_error("routine reference target is not a routine-value type", reference, target_ty);
	}
	bool ambiguous = false;
	if (Node* result = try_resolve_routine_reference(reference, target_ty, &ambiguous)) {
		return result;
	}
	if (ambiguous) {
		raise_routine_reference_error("routine reference is ambiguous for the destination "
		                              "routine type",
		                              reference, target_ty);
	}
	std::string category = target_ty->kind == METHOD ? "procedure/function of object" : "plain procedure/function";
	raise_routine_reference_error("no overload of the routine reference is compatible with " + category + " target", reference, target_ty);
}

Node* Parser::try_resolve_routine_code_reference(RoutineRef* reference) {
	if (!reference) {
		return nullptr;
	}
	std::vector<Callable*> candidates = routine_reference_candidates(reference->candidates);
	if (candidates.size() != 1) {
		return nullptr;
	}
	Callable* candidate = candidates.front();
	bool valid_plain = (dynamic_cast<Procedure*>(candidate) && candidate->ty->kind == ROUTINE && reference->receiver == nullptr) || (dynamic_cast<Method*>(candidate) && static_cast<Method*>(candidate)->is_static && candidate->ty->kind == ROUTINE);
	bool valid_method = dynamic_cast<Method*>(candidate) && !static_cast<Method*>(candidate)->is_static && (candidate->ty->kind == METHOD || candidate->ty->kind == CLASS_METHOD) && reference->receiver != nullptr;
	if (!valid_plain && !valid_method) {
		return nullptr;
	}
	if (valid_method && !(reference->receiver->ty && reference->receiver->ty->is_reference_type()) && !is_referenceable(reference->receiver)) {
		return nullptr;
	}

	auto result = new RoutineRef(reference->receiver, reference->candidates);
	result->resolved = candidate;
	result->code_only = true;
	result->ty = pointer_type();
	if (auto method = dynamic_cast<Method*>(candidate); method && method->is_static) {
		Node* qualifier = result->receiver;
		result->receiver = nullptr;
		if (qualifier && !dynamic_cast<ClassRefValue*>(qualifier) && !dynamic_cast<TypeMemberQualifier*>(qualifier)) {
			return new EvaluateThen(qualifier, result);
		}
	}
	return result;
}

Node* Parser::resolve_routine_code_reference(RoutineRef* reference) {
	if (!reference) {
		raise_routine_reference_error("invalid routine code reference", reference, pointer_type());
	}
	if (Node* result = try_resolve_routine_code_reference(reference)) {
		return result;
	}
	std::vector<Callable*> candidates = routine_reference_candidates(reference->candidates);
	if (candidates.size() != 1) {
		raise_routine_reference_error("a Pointer routine reference requires one non-overloaded routine", reference, pointer_type());
	}
	Callable* candidate = candidates.front();
	bool valid_method = dynamic_cast<Method*>(candidate) && !static_cast<Method*>(candidate)->is_static && (candidate->ty->kind == METHOD || candidate->ty->kind == CLASS_METHOD) && reference->receiver != nullptr;
	if (valid_method && !(reference->receiver->ty && reference->receiver->ty->is_reference_type()) && !is_referenceable(reference->receiver)) {
		raise_routine_reference_error("a bound method code reference requires a stable object receiver", reference, pointer_type());
	}
	raise_routine_reference_error("Pointer routine reference does not name a plain routine or bound "
	                              "instance method",
	                              reference, pointer_type());
}

static bool callable_accepts_receiver(Callable* callable, Node* receiver) {
	auto method = dynamic_cast<Method*>(callable);
	if (!method) {
		return receiver == nullptr;
	}
	if (method->is_static) {
		return receiver != nullptr;
	}
	if (!receiver || !method->owner_class) {
		return false;
	}

	Type* actual_type = receiver->ty;
	if (auto class_reference = dynamic_cast<ClassRefType*>(actual_type)) {
		actual_type = class_reference->target;
	} else if (auto pointer = dynamic_cast<PointerType*>(actual_type); pointer && (dynamic_cast<RecordType*>(pointer->item_type) || dynamic_cast<PackedRecordType*>(pointer->item_type) || dynamic_cast<ObjectType*>(pointer->item_type))) {
		// Records and old-style objects are value types, so their method Self
		// is ^Owner. Receiver viability is a relation between the referenced
		// aggregate type and the declared owner, not between the pointer
		// carrier and Owner. C++ member application performs the corresponding
		// `->`; do not give this receiver a separate overload score.
		actual_type = pointer->item_type;
	}
	const bool type_qualifier = dynamic_cast<TypeMemberQualifier*>(receiver) != nullptr;
	switch (method->ty->kind) {
	case CLASS_METHOD:
		return dynamic_cast<ClassType*>(actual_type) && actual_type->is_subtype_of(method->owner_class);
	case CONSTRUCTOR:
		return !type_qualifier && actual_type && actual_type->is_subtype_of(method->owner_class);
	case METHOD:
	case DESTRUCTOR:
		return !type_qualifier && !dynamic_cast<ClassRefType*>(receiver->ty) && actual_type && actual_type->is_subtype_of(method->owner_class);
	case CLASS_CONSTRUCTOR:
	case CLASS_DESTRUCTOR:
		return false;
	case ROUTINE:
		return false;
	}
	return false;
}

Parser::FinalizedCall Parser::finalize_call(Node* target, std::vector<Node*>& args, std::string name_for_error, SourceLocation error_location, Type* expected_return_type, OverloadResolutionPolicy resolution_policy) {
	// Peel MemberAccess: if the member is callable, its container is the
	// receiver and the member is the effective callee.
	Node* receiver = nullptr;
	if (auto ma = dynamic_cast<MemberAccess*>(target)) {
		if (dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b)) {
			if (!dynamic_cast<UnitRef*>(ma->a)) {
				receiver = ma->a;
			}
			target = ma->b;
		}
	}
	Callable* chosen = nullptr;
	std::optional<CallableMatch> chosen_match;
	RoutineType* value_rty = nullptr;
	Node* qualifier_effect = nullptr;
	if (auto c = dynamic_cast<Callable*>(target)) {
		if (name_for_error.empty() && !c->pas_name.empty()) {
			name_for_error = c->pas_name;
		}
		if (args.size() > c->ty->formals.size()) {
			raise_value_error_at(error_location, "too many arguments to '" + name_for_error + "'", c);
		}
		for (size_t i = args.size(); i < c->ty->formals.size(); ++i) {
			if (!c->ty->formals[i].default_value) {
				// A singleton has no competing arity to rank. Report the
				// missing source parameter directly while still using the
				// shared matcher for every supplied argument.
				raise_value_error_at(error_location, "missing argument for parameter '" + c->ty->formals[i].pas_name + "' in call to '" + name_for_error + "'", c);
			}
		}
		auto match = callable_accepts_receiver(c, receiver) ? match_callable_arguments(c, args, true, resolution_policy) : std::nullopt;
		if (!match) {
			const BuiltinDesc* builtin = c->builtin_desc ? c->builtin_desc : lookup_builtin_desc(c->cxx_name);
			for (size_t i = 0; i < args.size() && i < c->ty->formals.size(); ++i) {
				MatchFailure failure;
				if (match_argument(c->ty->formals[i], args[i], builtin, i, true, &failure)) {
					continue;
				}
				if (failure == MatchFailure::PackedProjection) {
					raise_value_error_at(error_location,
					                     "packed-record field cannot yet be passed "
					                     "as var/out parameter '" +
					                         c->ty->formals[i].pas_name + "'",
					                     args[i]);
				}
				if (failure == MatchFailure::NotStorageBacked) {
					raise_value_error_at(error_location, "argument for var/out parameter '" + c->ty->formals[i].pas_name + "' is not a storage-backed expression", args[i]);
				}
				if (failure == MatchFailure::OrdinalRequired) {
					auto rejected_pointer = dynamic_cast<PointerType*>(args[i] ? args[i]->ty : nullptr);
					raise_type_kind_mismatch_at(error_location, name_for_error + (rejected_pointer && rejected_pointer->is_untyped() ? " requires an ordinal or typed pointer argument" : " requires an ordinal argument"), rejected_pointer && rejected_pointer->is_untyped() ? "ordinal or typed pointer" : "ordinal", args[i] ? args[i]->ty : nullptr);
				}
			}
		}
		bool admitted = false;
		if (match && (!expected_return_type || static_cast<RoutineType*>(c->ty)->return_type == expected_return_type)) {
			std::vector<std::pair<Callable*, CallableMatch>> probe{{c, *match}};
			admitted = !overload_resolution_cohort(probe, resolution_policy).empty();
		}
		if (!match || !admitted || (expected_return_type && static_cast<RoutineType*>(c->ty)->return_type != expected_return_type)) {
			std::vector<Callable*> candidates{c};
			std::vector<std::pair<Callable*, CallableMatch>> viable;
			if (match) {
				viable.push_back({c, *match});
			}
			std::vector<Callable*> none;
			raise_overload_resolution_error(error_location, name_for_error, receiver, args, expected_return_type, candidates, viable, none, false);
		}
		chosen = c;
		chosen_match = std::move(match);
	} else if (auto os = dynamic_cast<OverloadSet*>(target)) {
		std::vector<Callable*> candidates = os->members;
		std::vector<std::pair<Callable*, CallableMatch>> viable;
		for (auto* c : candidates) {
			if (name_for_error.empty() && !c->pas_name.empty()) {
				name_for_error = c->pas_name;
			}
			if (!callable_accepts_receiver(c, receiver)) {
				continue;
			}
			auto match = match_callable_arguments(c, args, true, resolution_policy);
			if (match && (!expected_return_type || static_cast<RoutineType*>(c->ty)->return_type == expected_return_type)) {
				viable.push_back({c, std::move(*match)});
			}
		}
		if (viable.empty()) {
			std::vector<Callable*> none;
			raise_overload_resolution_error(error_location, name_for_error, receiver, args, expected_return_type, candidates, viable, none, false);
		}
		const std::vector<size_t> cohort = overload_resolution_cohort(viable, resolution_policy);
		if (cohort.empty()) {
			std::vector<Callable*> none;
			raise_overload_resolution_error(error_location, name_for_error, receiver, args, expected_return_type, candidates, viable, none, false);
		}
		std::vector<Callable*> non_dominated;
		for (size_t i_index = 0; i_index < cohort.size(); ++i_index) {
			const size_t i = cohort[i_index];
			bool dom = false;
			for (size_t j_index = 0; j_index < cohort.size(); ++j_index) {
				const size_t j = cohort[j_index];
				if (i != j && dominates(viable[j].second, viable[i].second, [this](Type* source, Type* destination) { return has_direct_assignment_edge(source, destination); })) {
					dom = true;
					break;
				}
			}
			if (!dom) {
				non_dominated.push_back(viable[i].first);
			}
		}
		if (non_dominated.size() != 1) {
			raise_overload_resolution_error(error_location, name_for_error, receiver, args, expected_return_type, candidates, viable, non_dominated, true);
		}
		chosen = non_dominated[0];
		for (auto& entry : viable) {
			if (entry.first == chosen) {
				chosen_match = std::move(entry.second);
				break;
			}
		}
	} else {
		value_rty = target ? dynamic_cast<RoutineType*>(target->ty) : nullptr;
		if (!value_rty) {
			// Builtin or other opaque callable -- no ranking/default checks.
			return FinalizedCall{receiver, target};
		}
		if (expected_return_type && value_rty->return_type != expected_return_type) {
			raise_type_mismatch_at(error_location, "routine value result", expected_return_type, value_rty->return_type);
		}
	}
	if (chosen_match) {
		args = chosen_match->arguments;
	}
	if (auto method = dynamic_cast<Method*>(chosen)) {
		if (!callable_accepts_receiver(method, receiver)) {
			raise_value_error_at(error_location,
			                     "internal error: selected method has an "
			                     "incompatible receiver",
			                     method);
		}
		if (method->is_static) {
			// FPC evaluates an explicit object/class-reference qualifier
			// exactly once even though a static method neither dereferences
			// nor receives it. Exact type/class designators are compile-time
			// member selectors and have no runtime evaluation.
			if (receiver && !dynamic_cast<ClassRefValue*>(receiver) && !dynamic_cast<TypeMemberQualifier*>(receiver)) {
				qualifier_effect = receiver;
			}
			receiver = nullptr;
		}
	}
	// Materialize missing args from defaults.
	auto rty = chosen ? static_cast<RoutineType*>(chosen->ty) : value_rty;
	const BuiltinDesc* builtin = chosen ? (chosen->builtin_desc ? chosen->builtin_desc : lookup_builtin_desc(chosen->cxx_name)) : nullptr;
	while (args.size() < rty->formals.size()) {
		auto& p = rty->formals[args.size()];
		if (!p.default_value) {
			raise_value_error_at(error_location, "missing argument for parameter '" + p.pas_name + "' in call to '" + name_for_error + "'", chosen ? static_cast<Node*>(chosen) : target);
		}
		MatchFailure failure;
		auto match = match_argument(p, p.default_value, builtin, args.size(), true, &failure);
		if (!match) {
			raise_type_mismatch_at(error_location, "default value for parameter '" + p.pas_name + "' is not applicable", p.ty, p.default_value ? p.default_value->ty : nullptr);
		}
		args.push_back(match->value);
	}
	if (args.size() > rty->formals.size()) {
		raise_value_error_at(error_location, "too many arguments to '" + name_for_error + "'", chosen ? static_cast<Node*>(chosen) : target);
	}
	if (!chosen && value_rty) {
		std::vector<Node*> converted;
		converted.reserve(args.size());
		for (size_t i = 0; i < args.size(); ++i) {
			MatchFailure failure;
			auto match = match_argument(value_rty->formals[i], args[i], nullptr, i, true, &failure);
			if (!match) {
				raise_type_mismatch_at(error_location, "routine-value argument", value_rty->formals[i].ty, args[i] ? args[i]->ty : nullptr);
			}
			converted.push_back(match->value);
		}
		args = std::move(converted);
	}
	if (builtin && is_generic_ordinal_operation(builtin->generic_kind)) {
		Type* operand_type = args.empty() || !args[0] ? nullptr : args[0]->ty;
		if (!generic_ordinal_operation_accepts(builtin->generic_kind, operand_type)) {
			raise_type_kind_mismatch_at(error_location, name_for_error + (builtin->generic_kind == BuiltinGenericKind::UnaryOrdinalOrPointerStep || builtin->generic_kind == BuiltinGenericKind::EnumOrPointerStep ? " requires an ordinal or pointer argument" : " requires an ordinal argument"), builtin->generic_kind == BuiltinGenericKind::UnaryOrdinalOrPointerStep || builtin->generic_kind == BuiltinGenericKind::EnumOrPointerStep ? "ordinal or pointer" : "ordinal", operand_type);
		}
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::AbsoluteValue && (args.empty() || !args[0] || !generic_absolute_value_accepts(args[0]->ty))) {
		raise_type_kind_mismatch_at(error_location, name_for_error + " requires a predefined numeric argument", "predefined numeric", args.empty() || !args[0] ? nullptr : args[0]->ty);
	}
	if (builtin && (builtin->generic_kind == BuiltinGenericKind::EnumComparison || builtin->generic_kind == BuiltinGenericKind::EnumOrDynamicArrayEquality)) {
		// match_callable_arguments already established the synthesized
		// (E, E) -> Boolean relation before ranking. Retain this defensive
		// validation at the final call boundary.
		Type* left = args.empty() || !args[0] ? nullptr : overload_rank_type(args[0]->ty);
		Type* right = args.size() < 2 || !args[1] ? nullptr : overload_rank_type(args[1]->ty);
		const bool same_enum = left && left == right && dynamic_cast<EnumType*>(left);
		const bool same_dynamic_array = left && left == right && dynamic_cast<DynamicArrayType*>(left);
		if (!same_enum && !(builtin->generic_kind == BuiltinGenericKind::EnumOrDynamicArrayEquality && same_dynamic_array)) {
			raise_type_kind_mismatch_at(error_location, name_for_error + " requires compatible enum or dynamic-array operands", "compatible enum or dynamic array", left && right ? right : nullptr);
		}
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::SetMutation) {
		// Until tpcc supports generic routine declarations, system.pp has to
		// spell these as `(var values; const item)`. Recover the otherwise
		// unexpressed `set of T`/`T` relationship from the first argument's
		// actual FixedSetType.
		auto set_type = args.empty() ? nullptr : dynamic_cast<FixedSetType*>(args[0] ? args[0]->ty : nullptr);
		if (!set_type) {
			raise_type_kind_mismatch_at(error_location, name_for_error + " requires a set variable as its first argument", "set", args.empty() || !args[0] ? nullptr : args[0]->ty);
		}
		if (args.size() < 2 || !args[1]) {
			raise_value_error_at(error_location, name_for_error + " requires a set element as its second argument", chosen ? static_cast<Node*>(chosen) : target);
		}
		// FPC converts the element expression to the concrete set element
		// type before generating the bit mutation. Do that here while the
		// Pascal type is available; both omitted-type RTL formals can then
		// retain their exact, related types at the C++ call boundary.
		args[1] = cast_for_destination(args[1], set_type->item_type);
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::SequenceLength && (args.empty() || !args[0] || !args[0]->ty->sequence_element_type())) {
		raise_type_kind_mismatch_at(error_location, name_for_error + " requires a string or array argument", "string or array", args.empty() || !args[0] ? nullptr : args[0]->ty);
	}
	if (builtin && builtin->generic_kind == BuiltinGenericKind::SequenceResize && (args.empty() || !args[0] || !args[0]->ty->sequence_is_resizable())) {
		raise_type_kind_mismatch_at(error_location, name_for_error + " requires a writable ShortString, AnsiString, or dynamic array", "writable ShortString, AnsiString, or dynamic array", args.empty() || !args[0] ? nullptr : args[0]->ty);
	}
	return FinalizedCall{receiver, chosen ? static_cast<Node*>(chosen) : target, qualifier_effect};
}

Node* Parser::make_call(FinalizedCall finalized, std::vector<Node*> args, LeadingTokenDirectives directives) {
	if (auto initializer = dynamic_cast<Method*>(finalized.callee); initializer && initializer->ty->kind == CONSTRUCTOR && finalized.receiver) {
		if (auto class_reference = dynamic_cast<ClassRefType*>(finalized.receiver->ty)) {
			auto result_type = dynamic_cast<ClassType*>(class_reference->target);
			if (!result_type) {
				raise_type_kind_mismatch("constructor class-reference target", "class", class_reference->target);
			}
			return new Construct(finalized.receiver, initializer, std::move(args), result_type);
		}
	}
	auto callable = dynamic_cast<Callable*>(finalized.callee);
	const BuiltinDesc* descriptor = callable ? (callable->builtin_desc ? callable->builtin_desc : lookup_builtin_desc(callable->cxx_name)) : nullptr;
	if (descriptor && descriptor->generic_kind == BuiltinGenericKind::ValOutput) {
		if (args.size() != 2 && args.size() != 3) {
			raise_parse_error("internal error: selected Val declaration has invalid arity");
		}
		Type* source_type = args[0] ? args[0]->ty : nullptr;
		if (!dynamic_cast<ShortStringType*>(source_type) && source_type != ansistring_type()) {
			raise_type_kind_mismatch("Val source", "ShortString or AnsiString", source_type);
		}

		const ValDestinationFamily family = val_destination_family(args[1] ? args[1]->ty : nullptr);
		if (family == ValDestinationFamily::EnumerationTodo) {
			raise_parse_error("Val enumeration destinations are not implemented");
		}
		if (family == ValDestinationFamily::Unsupported) {
			raise_type_kind_mismatch("Val destination", "integer, subrange, or predefined real", args[1] ? args[1]->ty : nullptr);
		}
		if (args.size() == 3 && !is_integer_semantic_type(args[2] ? args[2]->ty : nullptr)) {
			raise_type_kind_mismatch("Val code destination", "integer", args[2] ? args[2]->ty : nullptr);
		}

		Node* result = new ValCall(args[0], args[1], args.size() == 3 ? args[2] : nullptr);
		if (finalized.qualifier_effect) {
			result = new EvaluateThen(finalized.qualifier_effect, result);
		}
		return result;
	}
	auto call = new ProcCall(finalized.receiver, finalized.callee, std::move(args));
	call->ty = call_result_type(finalized.callee);
	if (call->ty == unknown_type()) {
		if (descriptor && (descriptor->generic_kind == BuiltinGenericKind::EnumComparison || descriptor->generic_kind == BuiltinGenericKind::EnumOrDynamicArrayEquality || descriptor->generic_kind == BuiltinGenericKind::SetComparison)) {
			// Enum, dynamic-array, and set comparisons restore otherwise
			// unspellable operand relations with Boolean result.
			call->ty = boolean_type();
		} else if (descriptor && (descriptor->generic_kind == BuiltinGenericKind::UnaryOrdinalOrPointerStep || descriptor->generic_kind == BuiltinGenericKind::EnumOrPointerStep || descriptor->generic_kind == BuiltinGenericKind::SetBinaryOperation || descriptor->generic_kind == BuiltinGenericKind::AbsoluteValue || descriptor->generic_kind == BuiltinGenericKind::OrdinalSuccessorOrPredecessor) && !call->args.empty() && call->args[0]) {
			// These root declarations omit a result type equal to their
			// converted first argument: T for Abs and ordinal/pointer
			// stepping, or `set of T` for set algebra. Candidate matching
			// has already established that exact type. Restore the relation
			// immediately after ordinary selection so direct function and
			// operator expressions plus Inc/Dec mutation share one
			// result-typing mechanism.
			call->ty = call->args[0]->ty;
		}
	}
	const BuiltinDesc* implementation = builtin_implementation_at_call_site(descriptor, directives);
	if (implementation != descriptor) {
		// Retain only an alternate implementation here. The original
		// Callable remains the declaration used by diagnostics, result
		// typing, and argument emission.
		call->lowering_builtin_desc = implementation;
	}
	Node* result = call;
	if (descriptor && directive_state.switch_enabled('r') && (descriptor->generic_kind == BuiltinGenericKind::AbsoluteValue || descriptor->generic_kind == BuiltinGenericKind::OrdinalSuccessorOrPredecessor) && (dynamic_cast<SubrangeType*>(call->ty) || dynamic_cast<EnumType*>(call->ty))) {
		// {$Q} protects the operation's finite carrier. {$R} independently
		// protects the declared enum/subrange domain at the point where the
		// predefined exact-T result is formed; the carrier can represent
		// values which the Pascal type cannot.
		result = new RangeCheckedCast(call, call->ty);
	}
	if (!finalized.qualifier_effect) {
		return result;
	}
	return new EvaluateThen(finalized.qualifier_effect, result);
}

Unit* Parser::load_or_get_unit(std::string name) {
	if (Unit* existing = unit_registry->lookup(name)) {
		return existing;
	}
	std::vector<std::string> empty;
	const auto& paths = options ? options->unit_search_paths : empty;
	FILE* f = nullptr;
	std::string opened;
	for (auto ext : {".pp", ".pas"}) {
		std::tie(f, opened) = search_for_file(name + ext, input_file_name, paths);
		if (f) {
			break;
		}
	}
	if (!f) {
		raise_parse_error("cannot find unit file for: " + name);
	}
	// Per-unit Emitter writes <output_dir>/<name>.h and .cc. Keep both it and
	// the nested Parser alive after publishing the interface: a legal
	// implementation dependency may point back to the interface whose uses
	// clause caused this load, so crossing immediately into this unit's
	// implementation would observe that parent frame only half populated.
	std::string dir = options ? options->output_dir : "";
	auto unit_emitter = new Emitter;
	unit_emitter->open_for_unit(name, dir);
	auto sub = new Parser(unit_registry, unit_emitter, options);
	sub->push_input_file(f, opened, 1);
	sub->start();
	if (!sub->maybe_parse_keyword("unit")) {
		sub->raise_parse_error("used source file must declare a unit");
	}
	Unit* loaded = sub->parse_unit_interface_body();
	if (!loaded || strcasecmp(loaded->name.c_str(), name.c_str()) != 0) {
		raise_parse_error("file '" + opened + "' did not declare 'unit " + name + ";'");
	}
	loaded->pending_parser = sub;
	loaded->pending_emitter = unit_emitter;
	return loaded;
}

void Parser::complete_unit(Unit* unit) {
	if (!unit || unit->phase == UnitPhase::Done || unit->phase == UnitPhase::ImplementationInProgress) {
		return;
	}
	if (unit->phase != UnitPhase::InterfaceDone) {
		raise_parse_error("cannot complete unit '" + unit->name + "' before its interface");
	}
	Parser* pending = unit->pending_parser;
	Emitter* pending_emitter = unit->pending_emitter;
	if (!pending || !pending_emitter) {
		raise_parse_error("unit '" + unit->name + "' has no suspended implementation parser");
	}

	pending->parse_unit_implementation_body(unit);
	pending_emitter->close();
	while (!pending->input_files.empty()) {
		pending->pop_input_file();
	}
	unit->pending_parser = nullptr;
	unit->pending_emitter = nullptr;
	delete pending;
	delete pending_emitter;
}

std::vector<Unit*> Parser::parse_uses_clause(bool in_interface, std::string current_name) {
	std::vector<Unit*> loaded;
	do {
		std::string name = parse_identifier();
		Unit* used = load_or_get_unit(name);
		if (in_interface && used->phase == UnitPhase::InterfaceInProgress) {
			raise_parse_error("circular interface dependency between '" + current_name + "' and '" + name + "'");
		}
		loaded.push_back(used);
		if (!maybe_parse_comma()) {
			break;
		}
	} while (true);
	return loaded;
}

Unit* Parser::implicit_uses(std::string user_name) {
	if (strcasecmp(user_name.c_str(), "system") == 0) {
		return nullptr;
	}
	return load_or_get_unit("system");
}

Unit* Parser::parse_unit_interface_body() {
	std::string name = parse_identifier();
	parse_semicolon();
	// Top-level invocation: open the emitter for the unit shape (.h + .cc).
	// Sub-parsers spawned by load_or_get_unit arrive with an already-open
	// unit emitter; only the top-level parser (main -> parse_program_or_unit
	// -> "unit" branch) hits this path with an unopened emitter.
	if (emitter && !emitter->is_open()) {
		emitter->open_for_unit(name, options ? options->output_dir : "");
	}
	Frame* unit_frame = new Frame(nullptr);
	Unit* unit = unit_registry->register_new(name, unit_frame);
	current_unit = unit;
	unit->phase = UnitPhase::InterfaceInProgress;
	// Self-bind the unit name to its UnitRef so `Unit.X` resolves through
	// the ordinary scope walk: from inside this unit (self-qualification)
	// and from any unit that `uses` this one (cross-unit qualification,
	// because this frame is pushed onto the using scope's chain).
	unit_frame->register_variable(name, unit->reference, /*ty=*/nullptr);

	parse_keyword("interface");
	// Install the interface lookup path in increasing precedence. Declaration
	// ownership is independent and is set explicitly after this path exists.
	// Used-unit frames remain separate lookup entries: their declarations are
	// never inserted into this unit frame and therefore cannot be re-exported
	// through it.
	std::vector<Unit*> iface_units;
	if (Unit* sys = implicit_uses(name)) {
		iface_units.push_back(sys);
	}
	if (maybe_parse_keyword("uses")) {
		auto used = parse_uses_clause(true, name);
		for (Unit* u : used) {
			iface_units.push_back(u);
		}
		parse_semicolon();
	}
	for (Unit* used : iface_units) {
		push_scope(used->frame, used->reference);
	}
	push_scope(unit->frame);
	push_declaration_frame(unit->frame);
	if (emitter) {
		emitter->set_section(Emitter::Section::Header);
		std::vector<std::string> h_files;
		for (Unit* u : iface_units) {
			h_files.push_back(u->name + ".h");
		}
		emitter->emit_unit_interface_prologue(unit->cxx_namespace, h_files);
	}
	parse_decl_blocks(true);
	if (emitter) {
		emitter->emit_unit_interface_epilogue();
	}
	unit->phase = UnitPhase::InterfaceDone;
	unit->interface_uses = iface_units;
	return unit;
}

void Parser::parse_unit_implementation_body(Unit* unit) {
	if (!unit || current_unit != unit || unit->phase != UnitPhase::InterfaceDone) {
		raise_parse_error("internal error: invalid unit implementation continuation");
	}
	parse_keyword("implementation");
	unit->phase = UnitPhase::ImplementationInProgress;
	// Mark this unit active before completing dependencies. If one of their
	// implementations uses this unit, its interface is already complete and
	// the recursive completion request terminates here instead of treating a
	// legal implementation cycle as an interface cycle.
	for (Unit* used : unit->interface_uses) {
		complete_unit(used);
	}

	std::vector<Unit*> impl_units;
	if (maybe_parse_keyword("uses")) {
		impl_units = parse_uses_clause(false, unit->name);
		parse_semicolon();
	}
	for (Unit* used : impl_units) {
		complete_unit(used);
	}
	// Rebuild only the lookup path for the implementation's precedence:
	// this unit, implementation uses, interface uses. Interface and
	// implementation declarations deliberately keep the same Frame identity.
	pop_declaration_frame();
	pop_scope(); // unit
	for (Unit* used : impl_units) {
		push_scope(used->frame, used->reference);
	}
	push_scope(unit->frame);
	push_declaration_frame(unit->frame);
	if (emitter) {
		emitter->set_section(Emitter::Section::Implementation);
		std::vector<std::string> h_files;
		for (Unit* u : impl_units) {
			h_files.push_back(u->name + ".h");
		}
		emitter->emit_unit_implementation_prologue(unit->cxx_namespace, unit->name + ".h", h_files);
	}
	// Implementation section: same dispatcher; procedures with bodies attach
	// to the interface prototypes via the lookup-then-adopt path in
	// parse_procedure_or_function.
	parse_decl_blocks(false);

	bool consumed_end = false;
	if (emitter) {
		emitter->emit_unit_lifecycle_open(unit->initialization_cxx_name);
	}
	if (!unit->class_constructors.empty()) {
		unit->has_initialization = true;
		for (Method* method : unit->class_constructors) {
			if (emitter) {
				emitter->emit_class_lifecycle_call(method);
			}
		}
	}
	if (maybe_parse_keyword("begin")) {
		unit->has_initialization = true;
		parse_unit_statement_sequence(false);
		parse_keyword("end");
		consumed_end = true;
	} else if (maybe_parse_keyword("initialization")) {
		unit->has_initialization = true;
		parse_unit_statement_sequence(true);
	}
	if (emitter) {
		emitter->emit_unit_lifecycle_close();
	}

	if (emitter) {
		emitter->emit_unit_lifecycle_open(unit->finalization_cxx_name);
	}
	if (!consumed_end && maybe_parse_keyword("finalization")) {
		unit->has_finalization = true;
		parse_unit_statement_sequence(false);
	}
	if (!unit->class_destructors.empty()) {
		unit->has_finalization = true;
		// FPC appends lifecycle destructors after the user's finalization
		// statements. It uses the same declaration-order structure walk as
		// constructors, so parents precede descendants here as well.
		for (Method* method : unit->class_destructors) {
			if (emitter) {
				emitter->emit_class_lifecycle_call(method);
			}
		}
	}
	if (emitter) {
		emitter->emit_unit_lifecycle_close();
	}

	if (!consumed_end) {
		parse_keyword("end");
	}
	parse_period();
	if (emitter) {
		emitter->emit_unit_implementation_epilogue();
	}

	pop_declaration_frame();
	pop_scope(); // unit
	for (size_t i = 0; i < impl_units.size(); i++) {
		pop_scope();
	}
	for (size_t i = 0; i < unit->interface_uses.size(); i++) {
		pop_scope();
	}

	unit->phase = UnitPhase::Done;
	unit_registry->record_completed(unit);
}

void Parser::parse_program_or_unit() {
	if (maybe_parse_keyword("program")) {
		auto name = parse_identifier();
		parse_semicolon();
		// Top-level invocation: open the emitter for the program shape (one
		// .cc, no header). Sub-parsers spawned by load_or_get_unit always
		// parse units and arrive with an already-open unit emitter.
		if (emitter && !emitter->is_open()) {
			emitter->open_for_program(options ? options->program_output_path : "");
		}
		Frame* program_frame = new Frame(nullptr);
		Unit* unit = unit_registry->register_new(name, program_frame, true);
		current_unit = unit;
		unit->phase = UnitPhase::InterfaceInProgress;
		// Load dependencies, then install the lookup path in increasing
		// precedence. The program's declaration owner is set independently.
		std::vector<Unit*> prog_units;
		if (Unit* sys = implicit_uses(name)) {
			prog_units.push_back(sys);
		}
		if (maybe_parse_keyword("uses")) {
			auto used = parse_uses_clause(false, name);
			for (Unit* u : used) {
				prog_units.push_back(u);
			}
			parse_semicolon();
		}
		for (Unit* used : prog_units) {
			complete_unit(used);
		}
		for (Unit* used : prog_units) {
			push_scope(used->frame, used->reference);
		}
		push_scope(program_frame);
		push_declaration_frame(program_frame);
		push_statement_control_context();
		if (emitter) {
			std::vector<std::string> h_files;
			for (Unit* u : prog_units) {
				h_files.push_back(u->name + ".h");
			}
			emitter->emit_program_prologue(h_files);
		}
		// Inlined equivalent of parse_block; we need to bracket the body-block
		// with main() emission hooks, which parse_block itself doesn't know
		// about (it's also called from procedure bodies).
		parse_decl_blocks(false);
		parse_keyword("begin");
		if (emitter) {
			std::vector<UnitLifecycleNames> lifecycle_hooks;
			for (Unit* used : unit_registry->completed_units()) {
				if (!used->has_initialization && !used->has_finalization) {
					continue;
				}
				lifecycle_hooks.push_back(UnitLifecycleNames{used->cxx_namespace, used->initialization_cxx_name, used->finalization_cxx_name});
			}
			emitter->emit_main_prologue(lifecycle_hooks, unit->class_destructors);
			// Program-local class hooks have the same position as unit class
			// hooks: dependency units are ready, while user program statements
			// have not begun.
			for (Method* method : unit->class_constructors) {
				emitter->emit_class_lifecycle_call(method);
			}
			// Arm program finalization only after every program class
			// constructor completed. This mirrors the per-unit rule that a
			// partially initialized lifecycle is not finalized.
			if (!unit->class_destructors.empty()) {
				emitter->emit_program_finalizer_registration();
			}
		}
		parse_block_body();
		parse_keyword("end");
		if (emitter) {
			emitter->emit_main_epilogue(!unit->class_destructors.empty());
		}
		pop_statement_control_context();
		pop_declaration_frame();
		pop_scope(); // program
		for (size_t i = 0; i < prog_units.size(); i++) {
			pop_scope();
		}
		parse_period();
		unit->phase = UnitPhase::Done;
	} else if (maybe_parse_keyword("unit")) {
		Unit* unit = parse_unit_interface_body();
		parse_unit_implementation_body(unit);
	} else {
		raise_parse_error("unknown input token");
	}
}
