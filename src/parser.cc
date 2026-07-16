#include "parser.h"
#include "builtins.h"
#include "cst.h"
#include "diagnostic.h"
#include "directive_expr.h"
#include "emit.h"
#include "evaluator.h"
#include "frame.h"
#include "units.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>
#include <chrono>
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
static Type* subrange_range_type(Type* ty);

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
    "array",
    "as",
    "begin",
    "bitpacked",
    "break",
    "case",
    "class",
    "const",
    "constructor",
    "continue",
    "destructor",
    "div", // operator
    "do",
    "downto",
    "dynamic", // FIXME
    "else",
    "end",
    "except",
    "file",
    "final",
    "finally",
    "forward", // FIXME directive ?
    "for",
    "function",
    "goto",
    "if",
    "in", // operator
    "inline",
    "implementation",
    "inherited",
    "interface",
    "is", // operator
    "label",
    "mod", // operator
    "nil",
    "noreturn",
    "not", // operator
    "object",
    "of",
    "operator",
    "or",  // operator
    "out", // FIXME directive ?
    "overload",
    "override",
    "packed",
    "procedure",
    "program",
    "raise",
    "record",
    "repeat",
    "set",
    "shl", // operator
    "shr", // operator
    "string",
    "then",
    "to",
    "try",
    "type",
    "unit",
    "until",
    "uses",
    "var",
    "virtual", // FIXME directive ?
    "while",
    "with",
    "xor", // operator
};

Parser::Parser(UnitRegistry* unit_registry, Emitter* emitter, CompilerOptions* options)
    : unit_registry(unit_registry), emitter(emitter), options(options) {
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
		return;
	}
	auto& p = input_files.back();
	input_file = p.input_file;
	input_file_name = p.input_file_name;
	input_file_line_number = p.input_file_line_number;
	input_char = fgetc(input_file);
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
		fprintf(stderr,
		    "internal compiler error: null declaration frame\n");
		abort();
	}
	declaration_frames.push_back(frame);
}

void Parser::pop_declaration_frame() {
	if (declaration_frames.empty()) {
		fprintf(stderr,
		    "internal compiler error: pop on empty declaration-frame stack\n");
		abort();
	}
	declaration_frames.pop_back();
}

Frame* Parser::current_declaration_frame() const {
	if (declaration_frames.empty()) {
		fprintf(stderr,
		    "internal compiler error: no current declaration frame\n");
		abort();
	}
	return declaration_frames.back();
}

Unit* Parser::declaration_unit(Frame* frame) const {
	if (!current_unit || current_unit->is_program)
		return nullptr; // program or no active source unit
	if (frame == current_unit->frame)
		return current_unit;
	return nullptr;
}

SourceLocation Parser::current_location() const {
	return SourceLocation(input_file_name, input_file_line_number);
}

[[noreturn]] static void emit_parse_error_at(const SourceLocation& loc, const std::string& message) {
	std::stringstream sst;
	if (!loc.file_name.empty()) {
		sst << loc.file_name;
		if (loc.line_number != 0)
			sst << '(' << loc.line_number << ')';
		sst << ": ";
	}
	sst << "error: " << message << std::endl;
	std::string r = sst.str();
	fprintf(stderr, "%s\n", r.c_str());
	fflush(stderr);
	exit(1);
}

[[noreturn]] void Parser::emit_parse_error_at(SourceLocation loc, std::string message) {
	::emit_parse_error_at(loc, message);
}

[[noreturn]] void Parser::raise_parse_error(std::string message) {
	emit_parse_error_at(current_location(), message);
}

[[noreturn]] Type* Parser::raise_type_parse_error(std::string message) {
	emit_parse_error_at(current_location(), message);
}

Type* Parser::raise_type_mismatch(std::string message, Type* expected, Type* got) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::string expected_ref = ctx.type_ref(expected);
	std::string got_ref = ctx.type_ref(got);
	std::stringstream sst;
	sst << message << ": expected type " << expected_ref << " but got type " << got_ref;
	sst << ctx.notes();
	emit_parse_error_at(current_location(), sst.str());
	return expected; // future non-fatal diagnostics can continue with the expected type
}

Type* Parser::raise_type_kind_mismatch(std::string message, const char* expected_kind, Type* got) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::string got_ref = ctx.type_ref(got);
	std::stringstream sst;
	sst << message << ": expected " << expected_kind << " type but got " << got_ref;
	sst << ctx.notes();
	emit_parse_error_at(current_location(), sst.str());
	return got; // future non-fatal diagnostics can continue with the parsed type
}

static void append_cost_vector(std::stringstream& sst, const std::vector<int>& costs) {
	sst << "[";
	for (size_t i = 0; i < costs.size(); ++i) {
		if (i)
			sst << ", ";
		sst << costs[i];
	}
	sst << "]";
}

static const SourceLocation& callable_source_location(Callable* c) {
	static const SourceLocation unknown;
	if (!c || !c->ty)
		return unknown;
	return c->ty->source_location;
}

static bool callable_source_less(Callable* a, Callable* b) {
	const SourceLocation& la = callable_source_location(a);
	const SourceLocation& lb = callable_source_location(b);
	if (la < lb)
		return true;
	if (lb < la)
		return false;
	return false;
}

static void append_callable_source_prefix(std::stringstream& sst, Callable* c, bool is_viable) {
	const SourceLocation& loc = callable_source_location(c);
	if (!loc.file_name.empty())
		sst << loc.file_name << "(" << loc.line_number << ")";
	if (is_viable)
		sst << "[viable]";
	if (!loc.file_name.empty() || is_viable)
		sst << ": ";
}

[[noreturn]] void Parser::raise_overload_resolution_error(SourceLocation error_location,
							  std::string name,
							  Node* receiver,
							  const std::vector<Node*>& args,
							  Type* expected_return_type,
							  const std::vector<Callable*>& candidates,
							  const std::vector<std::pair<Callable*, std::vector<int>>>& viable,
							  const std::vector<Callable*>& non_dominated,
							  bool ambiguous) {
	ErrorLetContext ctx = make_error_let_context_from_scopes(scopes, 4);
	std::stringstream sst;
	sst << (ambiguous ? "ambiguous overload" : "no matching overload") << " for '" << name << "'";

	if (receiver) {
		sst << "\n  receiver: " << ctx.value_ref(receiver) << " : " << ctx.type_ref(receiver->ty);
	}
	if (expected_return_type)
		sst << "\n  expected return type: " << ctx.type_ref(expected_return_type);
	for (size_t i = 0; i < args.size(); ++i) {
		sst << "\n  arg " << (i + 1) << ": " << ctx.value_ref(args[i]) << " : " << ctx.type_ref(args[i] ? args[i]->ty : nullptr);
	}

	(void)non_dominated;

	std::vector<Callable*> sorted_candidates = candidates;
	std::stable_sort(sorted_candidates.begin(), sorted_candidates.end(), callable_source_less);

	sst << "\n  all candidates:";
	for (Callable* c : sorted_candidates) {
		const std::vector<int>* viable_cost = nullptr;
		for (const auto& v : viable) {
			if (v.first == c) {
				viable_cost = &v.second;
				break;
			}
		}

		sst << "\n    ";
		append_callable_source_prefix(sst, c, viable_cost != nullptr);
		sst << ctx.value_ref(c) << " : " << ctx.type_ref(c ? c->ty : nullptr);
		if (viable_cost) {
			sst << " viable cost ";
			append_cost_vector(sst, *viable_cost);
		} else {
			sst << " not viable";
		}
	}

	sst << ctx.notes();
	emit_parse_error_at(error_location, sst.str());
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
	sst << ctx.notes();
	emit_parse_error_at(current_location(), sst.str());
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
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t'))
		p++;
	std::string name;
	while (p < body.size() && (isalnum((unsigned char)body[p]) || body[p] == '_')) {
		name.push_back((char)tolower((unsigned char)body[p]));
		p++;
	}
	while (p < body.size() && (body[p] == ' ' || body[p] == '\t'))
		p++;
	std::string rest = body.substr(p);
	while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t' || rest.back() == '\r' || rest.back() == '\n'))
		rest.pop_back();
	return {name, rest};
}

static std::string compact_directive_argument(
    const std::string& argument) {
	std::string compact;
	for (unsigned char ch : argument)
		if (!std::isspace(ch))
			compact.push_back(
			    static_cast<char>(std::tolower(ch)));
	return compact;
}

// Extract the content of a single-quoted string literal token (with '' escape).
// FIXME: Remove and use evaluate().
static std::string extract_string_literal(const std::string& token) {
	std::string s;
	for (size_t i = 1; i + 1 < token.size(); ++i) {
		s.push_back(token[i]);
		if (token[i] == '\'' && i + 2 < token.size() && token[i + 1] == '\'')
			++i;
	}
	return s;
}

// Try to open NAME (used verbatim -- extension is the caller's job) by
// searching the directory of CURRENT_INPUT first, then each entry of
// SEARCH_PATHS (normalized to end in '/'). Returns the opened FILE and the
// path that worked, or {nullptr, ""}.
static std::pair<FILE*, std::string> search_for_file(
    const std::string& name,
    const std::string& current_input,
    const std::vector<std::string>& search_paths) {
	std::vector<std::string> dirs;
	std::string cur_dir;
	auto slash = current_input.find_last_of('/');
	if (slash != std::string::npos)
		cur_dir = current_input.substr(0, slash + 1);
	dirs.push_back(cur_dir);
	for (auto& d : search_paths) {
		std::string s = d;
		if (!s.empty() && s.back() != '/')
			s.push_back('/');
		dirs.push_back(s);
	}
	for (auto& d : dirs) {
		std::string candidate = d + name;
		if (FILE* f = fopen(candidate.c_str(), "r"))
			return {f, candidate};
	}
	return {nullptr, ""};
}

std::string Parser::expand_include_macro(const std::string& rest) {
	if (rest != "%DATE%")
		raise_parse_error("unsupported include macro: " + rest);
	auto now = std::chrono::system_clock::now();
	auto zoned = std::chrono::current_zone()->to_local(now);
	auto today = std::chrono::floor<std::chrono::days>(zoned);
	std::chrono::year_month_day ymd{today};
	return std::format("'{:%Y/%m/%d}'", ymd);
}

void Parser::handle_directive(const std::string& body) {
	auto [name, rest] = split_directive(body);
	if (name == "ifdef" || name == "ifndef") {
		bool outer = current_active();
		bool cond = is_defined(rest);
		if (name == "ifndef")
			cond = !cond;
		ifdef_stack.push_back({outer, cond, outer && cond});
		return;
	}
	if (name == "if") {
		bool outer = current_active();
		bool cond = eval_directive_expr(rest);
		ifdef_stack.push_back({outer, cond, outer && cond});
		return;
	}
	if (name == "ifopt") {
		const std::string option =
		    compact_directive_argument(rest);
		if (option.size() != 2 ||
		    option[0] < 'a' || option[0] > 'z' ||
		    (option[1] != '+' && option[1] != '-'))
			raise_parse_error(
			    "$ifopt expects one option letter followed by + or -");
		const bool requested = option[1] == '+';
		const bool cond =
		    option_switches[option[0] - 'a'] == requested;
		const bool outer = current_active();
		ifdef_stack.push_back({outer, cond, outer && cond});
		return;
	}
	if (name == "else") {
		if (ifdef_stack.empty())
			raise_parse_error("$else without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken;
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "elseif") {
		if (ifdef_stack.empty())
			raise_parse_error("$elseif without matching $ifdef");
		auto& f = ifdef_stack.back();
		bool now = f.outer && !f.taken && eval_directive_expr(rest);
		f.active = now;
		f.taken = f.taken || now;
		return;
	}
	if (name == "endif" || name == "ifend") {
		if (ifdef_stack.empty())
			raise_parse_error("$endif without matching $ifdef");
		ifdef_stack.pop_back();
		return;
	}
	if (!current_active())
		return;
	if (name == "define") {
		// `{$define X}` sets X with no value; `{$define X := VALUE}` stores
		// VALUE (trimmed) so numeric-compare {$if X < N} etc. can consume it.
		if (options) {
			auto eq = rest.find(":=");
			if (eq == std::string::npos) {
				options->defines[rest] = "";
			} else {
				std::string sym = rest.substr(0, eq);
				while (!sym.empty() && (sym.back() == ' ' || sym.back() == '\t'))
					sym.pop_back();
				std::string val = rest.substr(eq + 2);
				size_t v0 = 0;
				while (v0 < val.size() && (val[v0] == ' ' || val[v0] == '\t'))
					v0++;
				val.erase(0, v0);
				while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
					val.pop_back();
				options->defines[sym] = val;
			}
		}
		return;
	}
	if (name == "undef") {
		if (options)
			options->defines.erase(rest);
		return;
	}
	if (name == "i" &&
	    (rest == "+" || rest == "-")) {
		// This is the {$I+}/{$I-} I/O-checking switch, not the short
		// spelling of {$INCLUDE file}. File operations currently preserve
		// their status in IOResult in either mode; recognizing the switch
		// here prevents the tokenizer from treating "+" or "-" as an
		// include filename.
		option_switches['i' - 'a'] = rest == "+";
		return;
	}
	if (name.size() == 1 &&
	    name[0] >= 'a' && name[0] <= 'z') {
		const std::string state =
		    compact_directive_argument(rest);
		if (state == "+" || state == "-") {
			// Conditional compilation observes option directives even when
			// the corresponding runtime/code-generation behavior is not yet
			// implemented. Like {$define}, a switch in an inactive branch
			// cannot change the state seen after that branch.
			option_switches[name[0] - 'a'] =
			    state == "+";
			return;
		}
	}
	if (name == "i" || name == "include") {
		if (rest.size() >= 2 && rest.front() == '%' && rest.back() == '%') {
			std::string literal = expand_include_macro(rest);
			size_t n = literal.size();
			auto buf = std::make_unique<char[]>(n);
			memcpy(buf.get(), literal.data(), n);
			FILE* f = fmemopen(buf.get(), n, "r");
			if (!f)
				raise_parse_error("fmemopen failed for {$I " + rest + "}");
			push_input_file_and_buffer(f, "<" + rest + ">", 1, std::move(buf), n);
			return;
		}
		std::vector<std::string> empty;
		auto [f, path] = search_for_file(rest, input_file_name,
						 options ? options->include_search_paths : empty);
		if (!f)
			raise_parse_error("cannot open include file: " + rest);
		push_input_file(f, path, 1);
		return;
	}
	// Other directives are accepted here for now. As we hit code where the
	// current no-op is wrong, tighten by name.
}

std::string Parser::consume() {
	std::stringstream sst;
	sst.str("");
	while (input_char == ' ' || input_char == '\n' || input_char == '\r') {
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
				consume_lowlevel();	 // Consume the '.' so input_char becomes the digit
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
	} else if (input_char != EOF && strchr("=;,[]()@+-^|&", input_char)) {
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
			if (input_char != '\'')
				break;
			sst << (char)input_char;
			consume_lowlevel();
		}
	} else if (input_char == '{') {
		sst << (char)input_char;
		consume_lowlevel();
		if (input_char == '$') {
			consume_lowlevel(); // skip $
			std::string body;
			while (input_char != EOF && input_char != '}') {
				body.push_back((char)input_char);
				consume_lowlevel();
			}
			if (input_char != '}')
				raise_parse_error("missing end comment");
			consume_lowlevel(); // skip }
			handle_directive(body);
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
		raise_parse_error("unknown input character");
	}
	auto text = sst.str();
	input_token = text;
	// Drop any token produced while an outer `{$ifdef}`/`{$if}` frame is
	// inactive. Directives are already handled in-line and never reach
	// here, so they still update the ifdef stack correctly.
	if (!current_active() && !text.empty())
		return consume();
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
	if (auto r = dynamic_cast<RecordType*>(ty))
		return r->children;
	if (auto r = dynamic_cast<PackedRecordType*>(ty))
		return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->children;
	if (auto i = dynamic_cast<InterfaceType*>(ty))
		return i->children;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->children;
	return nullptr;
}

void Parser::maybe_parse_statement() {
	if (peek_keyword("end") || peek_keyword("until") ||
	    peek_keyword("except") || peek_keyword("finally")) {
		return;
	}
	if (peek_keyword("raise")) {
		parse_keyword("raise");
		Node* object = nullptr;
		Node* address = nullptr;
		Node* frame = nullptr;
		const bool bare =
		    input_token == ";" ||
		    peek_keyword("end") ||
		    peek_keyword("else") ||
		    peek_keyword("until") ||
		    peek_keyword("except") ||
		    peek_keyword("finally");
		if (bare) {
			if (!bare_raise_allowed)
				raise_parse_error(
				    "re-raise is only valid directly inside an except handler");
		} else {
			ClassType* tobject =
			    lookup_implicit_tobject_superclass();
			object = cast(
			    parse_expression(), tobject);
			if (maybe_parse_directive("at")) {
				address = cast(
				    parse_expression(),
				    pointer_type());
				if (maybe_parse_comma())
					frame = cast(
					    parse_expression(),
					    pointer_type());
			}
		}
		if (emitter)
			emitter->emit_statement(
			    new Raise(
			        object, address, frame));
	} else if (peek_directive("fail") &&
	    current_routine &&
	    current_routine->ty->kind == CONSTRUCTOR) {
		parse_directive("fail");
		if (emitter)
			emitter->emit_statement(
			    new ConstructorFail());
	} else if (peek_keyword("break") || peek_keyword("continue")) {
		bool is_break = peek_keyword("break");
		consume();
		if (loop_depth == 0)
			raise_parse_error(is_break ? "break outside loop" : "continue outside loop");
		if (!finally_loop_depths.empty() &&
		    loop_depth <= finally_loop_depths.back())
			raise_parse_error(
			    is_break
			        ? "break cannot leave a finally block"
			        : "continue cannot leave a finally block");
		if (emitter)
			emitter->emit_loop_control(
			    is_break, protected_try_depth,
			    loop_try_depths.back());
	} else if (peek_keyword("return")) { // FIXME Exit
		if (!finally_loop_depths.empty())
			raise_parse_error("return cannot leave a finally block");
		consume();
		parse_expression();
	} else if (peek_directive("exit")) {
		parse_directive("exit");
		if (!finally_loop_depths.empty())
			raise_parse_error("exit cannot leave a finally block");
		if (!current_routine)
			raise_parse_error("exit outside routine");
		Type* ret_ty = current_routine->ty->return_type;
		Node* value = nullptr;
		if (maybe_parse_opening_paren()) {
			if (input_token != ")")
				value = parse_expression();
			parse_closing_paren();
			if (ret_ty == &unit_type()) {
				if (value)
					raise_parse_error("exit(value) in procedure");
			} else {
				value = value ? cast(value, ret_ty) : resolve_value("result");
			}
		} else if (ret_ty != &unit_type()) {
			value = resolve_value("result");
		}
		if (emitter)
			emitter->emit_statement(
			    new Return(
			        value, protected_try_depth));
	} else if (peek_keyword("goto")) {
		SourceLocation goto_location =
		    current_location();
		parse_keyword("goto");
		std::string label = parse_identifier();
		record_goto(label, goto_location);
		if (emitter)
			emitter->emit_goto(cxx_label_name(label));
	} else if (peek_keyword("try")) {
		const unsigned enclosing_exception_block =
		    current_exception_block();
		parse_keyword("try");
		const unsigned this_try_depth =
		    protected_try_depth + 1;
		const bool try_is_inside_loop =
		    loop_depth != 0;
		const bool saved_bare_raise =
		    bare_raise_allowed;
		if (emitter)
			emitter->emit_try_prologue();
		enter_exception_block();
		++protected_try_depth;
		bare_raise_allowed = false;
		parse_block_body();
		--protected_try_depth;

		if (maybe_parse_keyword("except")) {
			enter_exception_block();
			if (emitter)
				emitter->emit_try_except_prologue();
			bool typed_handlers = false;
			bool has_default = false;
			if (peek_directive("on")) {
				typed_handlers = true;
				bool first_handler = true;
				while (peek_directive("on")) {
					parse_directive("on");
					std::string first =
					    parse_identifier();
					std::optional<std::string>
					    variable_name;
					Type* exception_type = nullptr;
					if (maybe_parse_colon()) {
						variable_name = first;
						exception_type =
						    parse_type_expression(
						        false);
					} else if (
					    maybe_parse_period()) {
						UnitRef* unit =
						    resolve_unit_type_qualifier(
						        first);
						std::string member =
						    parse_identifier();
						exception_type =
						    unit->unit->frame
						        ->lookup_type(
						            member);
						if (!exception_type)
							raise_parse_error(
							    "unit '" +
							    first +
							    "' has no type '" +
							    member + "'");
					} else {
						exception_type =
						    resolve_type(
						        first, false);
					}
					if (!dynamic_cast<ClassType*>(
					        exception_type))
						raise_type_kind_mismatch(
						    "exception handler type must be a class",
						    "class",
						    exception_type);
					parse_keyword("do");

					auto handler_frame =
					    new Frame(nullptr);
					StorageSlot* variable =
					    nullptr;
					if (variable_name) {
						variable =
						    new StorageSlot(
						        cxx_value_name(
						            *variable_name),
						        exception_type);
						handler_frame
						    ->register_variable(
						        *variable_name,
						        variable,
						        exception_type);
					}
					if (emitter)
						emitter
						    ->emit_exception_handler_prologue(
						        exception_type,
						        variable
						            ? variable
						                  ->cxx_name
						            : "",
						        first_handler);
					push_scope(handler_frame);
					bare_raise_allowed = true;
					const bool empty_handler =
					    input_token == ";" ||
					    peek_directive("on") ||
					    peek_keyword("else") ||
					    peek_keyword("end");
					if (!empty_handler)
						parse_statement();
					bare_raise_allowed = false;
					pop_scope();
					if (emitter)
						emitter
						    ->emit_exception_handler_epilogue();
					first_handler = false;

					const bool separated =
					    maybe_parse_semicolon();
					while (maybe_parse_semicolon()) {
					}
					if (peek_directive("on") &&
					    !separated)
						raise_parse_error(
						    "missing semicolon between exception handlers");
				}
				if (maybe_parse_keyword("else")) {
					has_default = true;
					if (emitter)
						emitter
						    ->emit_exception_default_prologue();
					bare_raise_allowed = true;
					parse_block_body();
					bare_raise_allowed = false;
					if (emitter)
						emitter
						    ->emit_exception_default_epilogue();
				}
			} else {
				has_default = true;
				bare_raise_allowed = true;
				parse_block_body();
				bare_raise_allowed = false;
			}
			bare_raise_allowed = saved_bare_raise;
			parse_keyword("end");
			if (emitter)
				emitter->emit_try_except_epilogue(
				    typed_handlers, has_default);
		} else if (maybe_parse_keyword("finally")) {
			enter_exception_block();
			if (emitter)
				emitter->emit_try_finally_prologue();
			bare_raise_allowed = false;
			finally_loop_depths.push_back(loop_depth);
			parse_block_body();
			finally_loop_depths.pop_back();
			bare_raise_allowed =
			    saved_bare_raise;
			parse_keyword("end");
			if (emitter)
				emitter->emit_try_finally_epilogue();
		} else {
			raise_parse_error(
			    "expected except or finally after try block");
		}
		restore_exception_block(
		    enclosing_exception_block);
		if (emitter)
			emitter->emit_try_control_epilogue(
			    this_try_depth,
			    current_routine
			        ? current_routine->ty
			        : nullptr,
			    try_is_inside_loop);
	} else if (peek_keyword("if")) {
		parse_keyword("if");
		auto condition = parse_expression();
		parse_keyword("then");
		if (emitter)
			emitter->emit_if_prologue(condition);
		// Match FPC's pstatmnt.if_statement: a token in `endtokens`
		// means the then branch is absent. Leave the delimiter unconsumed
		// for the surrounding if, block, repeat, or exception parser.
		const bool empty_then =
		    input_token == ";" ||
		    peek_keyword("end") ||
		    peek_keyword("else") ||
		    peek_keyword("until") ||
		    peek_keyword("except") ||
		    peek_keyword("finally");
		if (!empty_then)
			parse_statement();
		if (maybe_parse_keyword("else")) {
			if (emitter)
				emitter->emit_if_else();
			parse_statement();
		}
		if (emitter)
			emitter->emit_if_epilogue();
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
		if (emitter)
			emitter->emit_case_prologue(selector_name, selector);

		bool has_arm = false;
		while (!peek_keyword("end") &&
		       !peek_keyword("else") &&
		       !peek_directive("otherwise")) {
			Node* arm_condition = nullptr;
			do {
				Node* lower = parse_subrange_bound_expression();
				lower = cast(lower, selector->ty);
				Node* label_condition;
				if (maybe_parse_period_period()) {
					Node* upper = parse_subrange_bound_expression();
					upper = cast(upper, selector->ty);
					auto lower_test = mk_compare(">=", selector_slot, lower);
					auto upper_test = mk_compare("<=", selector_slot, upper);
					auto both = new ShortCircuitOperation(AND, lower_test, upper_test);
					both->ty = boolean_type();
					label_condition = both;
				} else {
					label_condition = mk_compare("=", selector_slot, lower);
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

			if (emitter)
				emitter->emit_case_arm_prologue(arm_condition, !has_arm);
			parse_statement();
			if (emitter)
				emitter->emit_case_arm_epilogue();
			has_arm = true;

			// A semicolon separates arms. It is also accepted immediately
			// before ELSE/OTHERWISE, as in normal Pascal source.
			if (!maybe_parse_semicolon() &&
			    !peek_keyword("end") &&
			    !peek_keyword("else") &&
			    !peek_directive("otherwise"))
				raise_parse_error("missing semicolon between case arms");
		}

		if (peek_keyword("else") || peek_directive("otherwise")) {
			if (peek_keyword("else"))
				parse_keyword("else");
			else
				parse_directive("otherwise");
			if (emitter)
				emitter->emit_case_else_prologue(has_arm);
			parse_block_body();
			if (emitter)
				emitter->emit_case_arm_epilogue();
		}
		parse_keyword("end");
		if (emitter)
			emitter->emit_case_epilogue();
	} else if (peek_keyword("while")) {
		parse_keyword("while");
		auto condition = parse_expression();
		parse_keyword("do");
		if (emitter)
			emitter->emit_while_prologue(condition);
		++loop_depth;
		loop_try_depths.push_back(
		    protected_try_depth);
		parse_statement();
		loop_try_depths.pop_back();
		--loop_depth;
		if (emitter)
			emitter->emit_while_epilogue();
	} else if (peek_keyword("for")) {
		parse_keyword("for");
		std::string control_name = parse_identifier();
		Node* control = resolve_lvalue(control_name);
		if (!dynamic_cast<StorageSlot*>(control))
			raise_parse_error("for control variable must be a simple variable");
		if (!maybe_parse_colon_equals())
			raise_parse_error("expected ':=' after for control variable");
		Node* initial = cast(parse_expression(), control->ty);
		bool descending;
		if (maybe_parse_keyword("to"))
			descending = false;
		else if (maybe_parse_keyword("downto"))
			descending = true;
		else
			raise_parse_error("expected 'to' or 'downto' in for statement");
		Node* final = cast(parse_expression(), control->ty);
		parse_keyword("do");

		OrdinalBounds bounds;
		if (!intrinsic_ordinal_bounds(control->ty, &bounds) &&
		    !dynamic_cast<EnumType*>(control->ty) &&
		    !dynamic_cast<SubrangeType*>(control->ty))
			raise_parse_error("for control variable must have an ordinal type");

		if (emitter)
			emitter->emit_for_prologue(control, initial, final, descending);
		++loop_depth;
		loop_try_depths.push_back(
		    protected_try_depth);
		parse_statement();
		loop_try_depths.pop_back();
		--loop_depth;
		if (emitter)
			emitter->emit_for_epilogue();
	} else if (peek_keyword("repeat")) {
		parse_keyword("repeat");
		if (emitter)
			emitter->emit_repeat_prologue();
		++loop_depth;
		loop_try_depths.push_back(
		    protected_try_depth);
		parse_block_body();
		loop_try_depths.pop_back();
		--loop_depth;
		parse_keyword("until");
		auto condition = parse_expression();
		if (emitter)
			emitter->emit_repeat_epilogue(condition);
	} else if (peek_keyword("begin")) {
		parse_keyword("begin");
		parse_block_body();
		parse_keyword("end");
	} else if (peek_keyword("with")) {
		parse_keyword("with");
		// TODO: complex targets (`p^`, `arr[i]`, `f()`). For now the
		// target must be a simple variable so we can read Type* off its
		// StorageSlot; the alias-emission below is already correct for
		// arbitrary targets when we lift this restriction.
		auto id = parse_identifier();
		Node* target = resolve_value(id);
		auto target_slot = dynamic_cast<StorageSlot*>(target);
		if (!target_slot)
			raise_parse_error("with target must currently be a simple variable");
		Frame* body_frame = get_type_body_frame(target_slot->ty);
		if (!body_frame)
			raise_parse_error("with target's type has no field body");
		parse_keyword("do");
		// emit_with_prologue introduces a C++ block. As with case selectors,
		// nested blocks can safely reuse this tpcc-owned spelling.
		auto alias_slot = new StorageSlot("tpcc_with_target", target_slot->ty);
		if (emitter)
			emitter->emit_with_prologue(alias_slot->cxx_name, target);
		push_scope(body_frame, alias_slot);
		parse_statement();
		pop_scope();
		if (emitter)
			emitter->emit_with_epilogue();
	} else {
		// A leading identifier followed by ':' is a Pascal label definition.
		// If the next token is immediately ':=', resolve the identifier in
		// lvalue context before value lookup: Pascal function-name assignment
		// writes the hidden result slot, while expression use stays a call.
		Node* lhs = nullptr;
		if (!input_token.empty() && keywords.find(input_token) == keywords.end()) {
			SourceLocation designator_location =
			    current_location();
			std::string first = parse_identifier();
			if (maybe_parse_colon()) {
				record_label_definition(
				    first, designator_location);
				if (emitter)
					emitter->emit_label(cxx_label_name(first));
				parse_statement();
				return;
			}
			if (input_token == ":=")
				lhs = resolve_lvalue(first);
			else
				lhs = parse_designator_tail(parse_value_from_identifier(first));
		} else {
			lhs = parse_designator();
		}
		// A statement here is either an assignment (designator := expression)
		// or a call (designator, possibly with auto-call). Parse the LHS as
		// a raw designator so we don't auto-call in the assignment case.
		if (maybe_parse_colon_equals()) {
			if (!is_assignable(lhs)) {
				raise_parse_error("LHS of ':=' is not assignable");
			}
			// A packed overlay is writable only when its source is a real
			// assignable place. In particular, never accept `TPacked(F()).X`
			// and then silently mutate a copied C++ temporary.
			Node* overlay_source = nullptr;
			MemberAccess* overlay_member = dynamic_cast<MemberAccess*>(lhs);
			if (auto ix = dynamic_cast<Index*>(lhs))
				overlay_member = dynamic_cast<MemberAccess*>(ix->a);
			if (auto property = dynamic_cast<PropertyAccess*>(lhs))
				overlay_member = dynamic_cast<MemberAccess*>(property->receiver);
			if (overlay_member) {
				if (auto overlay = dynamic_cast<Cast*>(overlay_member->a))
					if (dynamic_cast<PackedRecordType*>(overlay->ty))
						overlay_source = overlay->a;
			}
			if (overlay_source && !is_assignable(overlay_source))
				raise_parse_error("writable packed-record overlay requires an assignable source");
			if (contains_packed_projection(lhs) && !is_supported_packed_assignment(lhs)) {
				raise_parse_error("write through a nested or indexed packed-record field is not implemented");
			}
			Node* rhs = parse_expression();
			auto assign = mk_assign(lhs, rhs);
			if (emitter)
				emitter->emit_statement(assign);
			return;
		}
		// Any expression may stand in statement position (evaluate, discard).
		// parse_designator already built the ProcCall for explicit `foo(x)`;
		// for bare `foo`, maybe_auto_call wraps it; an InheritedCall or other
		// expression flows through unchanged.
		Node* call = maybe_auto_call(lhs);
		if (emitter)
			emitter->emit_statement(call);
	}
}

std::optional<std::string> Parser::maybe_parse_identifier() {
	auto result = input_token;
	if (keywords.find(result) != keywords.end()) {
		return {};
	}
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
		const bool is_decimal_real =
		    base == 10 &&
		    input_token.find_first_of(".eE") !=
		        std::string::npos;
		if (!is_decimal_real) {
			uint64_t value;
			auto [ptr, ec] = std::from_chars(input, input + input_size, value, base);
			if (ec != std::errc() || ptr != input + input_size) {
				raise_parse_error("malformed numeral: " + input_token);
			}
			auto lit = new Integer(value, &untyped_integer_type());
			consume();
			return lit;
		} else {
			long double value;
			auto [ptr, ec] = std::from_chars(
			    input, input + input_size, value,
			    std::chars_format::general);
			if (ec != std::errc() ||
			    ptr != input + input_size) {
				raise_parse_error(
				    "malformed real numeral: " +
				    input_token);
			}
			auto lit = new Real(value, extended_type());
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
	return qualifier &&
	       !dynamic_cast<UnitRef*>(qualifier);
}

ScopeValueLookup ScopeEntry::lookup_value(
    const std::string& name) const {
	Node* binding = frame->lookup_value(name);
	// A receiver Frame has already consumed every overload that is visible
	// through class/object inheritance. Its completed member binding shadows
	// lower lexical and unit-global scopes exactly like any ordinary scoped
	// declaration. UnitRef is deliberately not a receiver here: used-unit
	// globals share the global overload domain and may attach across entries.
	return ScopeValueLookup{
	    binding,
	    binding &&
	        !is_receiver_environment() &&
	        callable_binding_opens_parent(binding)};
}

Node* Parser::bind_lookup_result(
    Node* qualifier, Node* binding) {
	if (!qualifier || dynamic_cast<UnitRef*>(qualifier))
		return binding;

	// A class reference and a record type qualifier open a member environment,
	// but neither supplies an instance receiver. Reject an already-selected
	// instance member here; overload sets remain intact until their visible
	// arguments select one declaration in finalize_call.
	const bool class_reference =
	    dynamic_cast<ClassRefType*>(
	        qualifier->ty) != nullptr;
	const bool type_qualifier =
	    dynamic_cast<TypeMemberQualifier*>(
	        qualifier) != nullptr;
	if (class_reference || type_qualifier) {
		if (auto slot =
		        dynamic_cast<StorageSlot*>(binding);
		    slot &&
		    slot->kind !=
		        StorageSlot::Kind::StaticMember)
			raise_parse_error(
			    "instance field cannot be accessed through a " +
			    std::string(type_qualifier
			            ? "type"
			            : "class reference"));
		if (dynamic_cast<Property*>(binding))
			raise_parse_error(
			    "instance property cannot be accessed through a " +
			    std::string(type_qualifier
			            ? "type"
			            : "class reference"));
		if (auto method =
		        dynamic_cast<Method*>(binding)) {
			const bool allowed =
			    method->is_static ||
			    (!type_qualifier &&
			     (method->ty->kind == CLASS_METHOD ||
			      method->ty->kind == CONSTRUCTOR));
			if (!allowed)
				raise_parse_error(
				    "instance method cannot be accessed through a " +
				    std::string(type_qualifier
				            ? "type"
				            : "class reference"));
		}
	}
	if (auto property = dynamic_cast<Property*>(binding))
		return new PropertyAccess(qualifier, property, {});
	auto access = new MemberAccess(qualifier, binding);
	access->ty = binding->ty;
	return access;
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
	// Walk top-down. First hit shadows unless it's overload-marked; then
	// keep walking to aggregate additional overload-marked hits from lower
	// scopes (cross-unit overloading).
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (auto unit =
		        dynamic_cast<UnitRef*>(it->qualifier);
		    unit && unit->unit &&
		    unit->unit->name == name) {
			if (collected.empty())
				return unit;
			break;
		}
		ScopeValueLookup lookup =
		    it->lookup_value(name);
		Node* hit = lookup.binding;
		if (!hit)
			continue;
		if (!lookup.opens_parent) {
			if (collected.empty())
				return bind_lookup_result(
				    it->qualifier, hit);
			break;
		}
		auto as_call = dynamic_cast<Callable*>(hit);
		auto as_set = dynamic_cast<OverloadSet*>(hit);
		if (collected.empty() && !as_call && !as_set) {
			// First (and terminating) hit is a non-callable value.
			return bind_lookup_result(
			    it->qualifier, hit);
		}
		if (as_call) {
			if (!as_call->has_overload_directive) {
				if (collected.empty())
					return as_call; // plain callable, first-hit wins
				break;			// shadowed by collected overloads above
			}
			collected.push_back(as_call);
		} else if (as_set) {
			// Every member of an OverloadSet already has has_overload_directive.
			for (auto* m : as_set->members)
				collected.push_back(m);
		} else {
			// Non-callable value below a collected overload block -- stop.
			break;
		}
	}
	if (collected.empty())
		return nullptr;
	if (collected.size() == 1)
		return collected[0];
	return new OverloadSet(std::move(collected));
}

UnitRef* Parser::resolve_unit_type_qualifier(
    std::string name) {
	if (maybe_resolve_type(name))
		raise_parse_error(
		    "type identifier '" + name +
		    "' is not a unit qualifier");
	Node* binding = maybe_resolve_value(name);
	if (auto unit = dynamic_cast<UnitRef*>(binding))
		return unit;
	raise_parse_error(
	    "identifier '" + name +
	    "' is not a unit qualifier");
	return nullptr;
}

/** Walk the scope stack top-down looking up a value-position name. Raise if not found. */
Node* Parser::resolve_value(std::string name) {
	if (Node* hit = maybe_resolve_value(name))
		return hit;
	raise_parse_error("unresolved value identifier: " + name);
	return nullptr;
}

Node* Parser::active_function_result_lvalue(Callable* c) const {
	if (!c || !c->body_frame || c->ty->return_type == &unit_type())
		return nullptr;
	bool active = false;
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		for (const Frame* frame = it->frame; frame; frame = frame->parent) {
			if (frame == c->body_frame) {
				active = true;
				break;
			}
		}
		if (active)
			break;
	}
	if (!active)
		return nullptr;
	return c->body_frame->lookup_value("result");
}

/** value that can be assigned to */
Node* Parser::resolve_lvalue(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		if (Node* hit =
		        it->lookup_value(name).binding) {
			if (auto c = dynamic_cast<Callable*>(hit)) {
				if (Node* result = active_function_result_lvalue(c))
					return result;
			} else if (auto os = dynamic_cast<OverloadSet*>(hit)) {
				for (auto* c : os->members) {
					if (Node* result = active_function_result_lvalue(c))
						return result;
				}
			}
			return bind_lookup_result(
			    it->qualifier, hit);
		}
	}
	raise_parse_error("unresolved lvalue identifier: " + name);
	return nullptr;
}

Type* Parser::maybe_resolve_type(std::string name) {
	for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
		Type* hit = it->frame->lookup_type(name);
		if (!hit)
			continue;
		std::unordered_set<IncompleteType*> seen;
		while (auto incomplete =
		           dynamic_cast<IncompleteType*>(hit)) {
			if (!incomplete->resolved)
				return incomplete;
			if (!seen.insert(incomplete).second)
				raise_parse_error(
				    "cyclic resolved type alias involving '" +
				    incomplete->name + "'");
			hit = incomplete->resolved;
		}
		if (hit)
			return hit;
	}
	return nullptr;
}

static const BuiltinDesc* builtin_desc_for_node(Node* n) {
	if (auto b = dynamic_cast<Builtin*>(n))
		return b->desc;
	if (auto c = dynamic_cast<Callable*>(n))
		return c->builtin_desc;
	return nullptr;
}

static std::optional<TypeBoundKind> type_bound_kind_for_builtin(Node* n) {
	const BuiltinDesc* desc = builtin_desc_for_node(n);
	return desc ? desc->type_bound_kind : std::optional<TypeBoundKind>{};
}

static BuiltinSyntaxKind syntax_kind_for_builtin(Node* n) {
	const BuiltinDesc* desc = builtin_desc_for_node(n);
	return desc ? desc->syntax_kind : BuiltinSyntaxKind::None;
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
	if (Type* hit = maybe_resolve_type(name))
		return hit;
	if (allow_forward && !type_block_frames.empty()) {
		auto inc = new IncompleteType(current_location(), name);
		type_block_frames.back()->register_type(name, inc);
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
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->super;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->super;
	return nullptr;
}

static Frame* make_aggregate_body_frame(Type* owner) {
	return new Frame(
	    owner ? get_type_body_frame(parent_of(owner)) : nullptr);
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
	while (auto subrange = dynamic_cast<SubrangeType*>(ty))
		ty = subrange->base_type;
	if (dynamic_cast<EnumType*>(ty))
		return true;
	OrdinalBounds bounds;
	return intrinsic_ordinal_bounds(ty, &bounds);
}

Node* Parser::parse_set_literal() {
	parse_opening_bracket();
	std::vector<SetLiteral::Item> items;
	if (input_token != "]") {
		do {
			Node* lower = parse_expression();
			Node* upper = nullptr;
			if (maybe_parse_period_period())
				upper = parse_expression();
			items.push_back(SetLiteral::Item{lower, upper});
		} while (maybe_parse_comma());
	}
	parse_closing_bracket();

	// An empty set acquires its item type from assignment, a formal parameter,
	// or the left operand of `in`. Non-empty constructors infer one common
	// ordinal item type, then convert every bound to it.
	Type* item_type = unknown_type();
	for (const SetLiteral::Item& item : items) {
		for (Node* bound : {item.lower, item.upper}) {
			if (!bound)
				continue;
			Type* bound_type = bound->ty == &untyped_integer_type()
			    ? integer_type()
			    : bound->ty;
			if (!is_set_item_type(bound_type))
				raise_parse_error("set literal item is not ordinal");
			if (item_type == unknown_type()) {
				item_type = bound_type;
			} else {
				Type* common = common_arith_type(item_type, bound_type);
				if (!common)
					raise_type_mismatch("set literal items with a common ordinal type",
						item_type, bound_type);
				item_type = common;
			}
		}
	}
	if (item_type != unknown_type()) {
		for (SetLiteral::Item& item : items) {
			item.lower = cast(item.lower, item_type);
			if (item.upper)
				item.upper = cast(item.upper, item_type);
		}
	}
	return new SetLiteral(std::move(items),
	    new FixedSetType(current_location(), item_type));
}

Node* Parser::parse_new_or_dispose(bool is_new) {
	SourceLocation operation_location =
	    current_location();
	parse_opening_paren();

	Node* destination_or_pointer = nullptr;
	PointerType* pointer_type = nullptr;
	bool functional_form = false;

	// The first operand selects one of Pascal's two forms. Ordinary value
	// lookup wins over type lookup, so a nearer variable shadows a pointer
	// type with the same spelling just as it does elsewhere in expression
	// syntax. Only New has a functional type form; Dispose always consumes a
	// pointer value.
	Node* visible_value =
	    maybe_resolve_value(input_token);
	if (visible_value || !is_new) {
		destination_or_pointer =
		    parse_designator();
		pointer_type =
		    dynamic_cast<PointerType*>(
		        destination_or_pointer
		            ? destination_or_pointer->ty
		            : nullptr);
	} else if (maybe_resolve_type(input_token)) {
		Type* parsed_type =
		    parse_type_expression(false);
		pointer_type =
		    dynamic_cast<PointerType*>(parsed_type);
		functional_form = true;
	} else {
		raise_parse_error(
		    "New first operand is neither a pointer "
		    "variable nor a pointer type");
	}

	if (!pointer_type || pointer_type->is_untyped())
		emit_parse_error_at(
		    operation_location,
		    std::string(is_new ? "New" : "Dispose") +
		        " requires a typed pointer");
	if (is_new && !functional_form &&
	    !is_assignable(destination_or_pointer))
		emit_parse_error_at(
		    operation_location,
		    "New destination is not assignable");

	Type* allocated_type =
	    pointer_type->item_type;

	Method* lifecycle_method = nullptr;
	std::vector<Node*> lifecycle_args;
	if (maybe_parse_comma()) {
		auto object =
		    dynamic_cast<ObjectType*>(allocated_type);
		if (!object)
			emit_parse_error_at(
			    operation_location,
			    std::string(is_new ? "New" : "Dispose") +
			        " lifecycle form requires a pointer "
			        "to an old-style object");

		std::string lifecycle_name =
		    parse_identifier();
		if (maybe_parse_opening_paren()) {
			if (input_token != ")") {
				if (!is_new)
					emit_parse_error_at(
					    operation_location,
					    "Dispose destructor cannot have "
					    "arguments");
				lifecycle_args.push_back(
				    parse_expression());
				while (maybe_parse_comma())
					lifecycle_args.push_back(
					    parse_expression());
			}
			parse_closing_paren();
		}

		Node* candidates =
		    object->children
		        ? object->children->lookup_value(
		              lifecycle_name)
		        : nullptr;
		if (!candidates)
			emit_parse_error_at(
			    operation_location,
			    "no old-style object member '" +
			        lifecycle_name + "'");

		// Reuse ordinary member overload finalization. The receiver exists
		// only to establish the already-known pointed-to object context;
		// NewValue/DisposeValue later supply the actual runtime pointer.
		auto semantic_receiver =
		    new StorageSlot("", pointer_type);
		auto target =
		    new MemberAccess(
		        semantic_receiver, candidates);
		target->ty = candidates->ty;
		auto finalized =
		    finalize_call(
		        target, lifecycle_args,
		        lifecycle_name,
		        operation_location);
		lifecycle_method =
		    dynamic_cast<Method*>(
		        finalized.callee);
		if (!lifecycle_method ||
		    lifecycle_method->ty->kind !=
		        (is_new ? CONSTRUCTOR : DESTRUCTOR))
			emit_parse_error_at(
			    operation_location,
			    std::string(is_new ? "New" : "Dispose") +
			        (is_new
			             ? " second operand must select "
			               "a constructor"
			             : " second operand must select "
			               "a destructor"));
	}
	parse_closing_paren();

	if (is_new) {
		auto value =
		    new NewValue(
		        pointer_type, allocated_type,
		        lifecycle_method,
		        std::move(lifecycle_args));
		if (functional_form)
			return value;
		return mk_assign(
		    destination_or_pointer, value);
	}
	return new DisposeValue(
	    destination_or_pointer,
	    lifecycle_method);
}

Node* Parser::parse_value() {
	if (peek_keyword("inherited"))
		return parse_inherited();
	if (input_token == "[")
		return parse_set_literal();
	if (maybe_parse_opening_paren()) { // grouping paren
		auto result = parse_expression();
		parse_closing_paren();
		return result;
	}
	if (auto n = maybe_parse_numeral())
		return n;
	if (peek_keyword("nil")) {
		consume();
		return new NilLiteral();
	}
	// FPC scans a consecutive run of quoted fragments and numeric character
	// fragments as one literal:
	//
	//   'A'          -> one byte, Char
	//   #13          -> one byte, Char
	//   'A'#0'B'     -> three bytes, string
	//   #13#10       -> two bytes, string
	//
	// tpcc's tokenizer currently returns each fragment separately, so combine
	// the run here before assigning its semantic type. This also preserves
	// embedded zero bytes: String::value is length-bearing and emission passes
	// that explicit length to the RTL instead of using strlen.
	if (!input_token.empty() &&
	    (input_token.front() == '\'' || input_token.front() == '#')) {
		std::string s;
		do {
			if (input_token.front() == '\'') {
				s += extract_string_literal(input_token);
			} else {
				const char* first = input_token.data() + 1;
				const char* last = input_token.data() + input_token.size();
				uint64_t value = 0;
				auto [end, error] = std::from_chars(first, last, value, 10);
				if (first == last || error != std::errc() || end != last || value > 255)
					raise_parse_error("malformed character-code literal: " + input_token);
				s.push_back(static_cast<char>(static_cast<unsigned char>(value)));
			}
			consume();
		} while (!input_token.empty() &&
		         (input_token.front() == '\'' || input_token.front() == '#'));
		Type* literal_type = s.size() == 1 ? char_type() : shortstring_type();
		return new String(std::move(s), literal_type);
	}
	// FIXME: bool literals also belong here (need enum-member support).
	return parse_value_from_identifier(parse_identifier());
}

Node* Parser::parse_value_from_identifier(std::string id) {
	if (maybe_parse_period()) {
		Node* base = maybe_resolve_value(id);
		if (!base) {
			Type* qualifier_type =
			    maybe_resolve_type(id);
			if (auto class_type =
			        dynamic_cast<ClassType*>(
			            qualifier_type))
				base =
				    new ClassRefValue(class_type);
			else if (dynamic_cast<RecordType*>(
			             qualifier_type) ||
			         dynamic_cast<PackedRecordType*>(
			             qualifier_type))
				base =
				    new TypeMemberQualifier(
				        qualifier_type);
		}
		if (!base)
			raise_parse_error(
			    "unresolved member qualifier: " +
			    id);
		return parse_member_selection(base);
	}
	if (Node* value = maybe_resolve_value(id)) {
		// Within a function body, a bare occurrence of that function's name
		// denotes its hidden result variable.  Parentheses still mean a call,
		// which is how recursive calls remain distinguishable.  resolve_lvalue
		// already applies this rule for `FunctionName := value`; it is equally
		// required in value context for representation overlays such as
		// `TWordRec(reverse_word).hi`.
		if (input_token != "(") {
			if (auto c = dynamic_cast<Callable*>(value)) {
				if (Node* result = active_function_result_lvalue(c))
					return result;
			} else if (auto os = dynamic_cast<OverloadSet*>(value)) {
				for (auto* c : os->members)
					if (Node* result = active_function_result_lvalue(c))
						return result;
			}
		}
		// Low/High are type-argument intrinsics, so ordinary call finalization
		// cannot infer their result type from a RoutineType. Dispatch on the
		// resolved builtin object rather than the source spelling: user shadowing
		// still wins, and seeded subrange expressions use the same path as normal
		// value expressions.
		if (auto kind = type_bound_kind_for_builtin(value); kind && input_token == "(") {
			parse_opening_paren();
			Type* target_ty = parse_type_expression(false);
			parse_closing_paren();
			return new TypeBound(*kind, target_ty);
		}
		BuiltinSyntaxKind syntax_kind = syntax_kind_for_builtin(value);
		if (syntax_kind == BuiltinSyntaxKind::NewValue ||
		    syntax_kind == BuiltinSyntaxKind::DisposeValue) {
			if (input_token != "(")
				raise_parse_error(
				    syntax_kind ==
				            BuiltinSyntaxKind::NewValue
				        ? "New requires an argument list"
				        : "Dispose requires an argument list");
			return parse_new_or_dispose(
			    syntax_kind ==
			    BuiltinSyntaxKind::NewValue);
		}
		if (syntax_kind == BuiltinSyntaxKind::Write ||
		    syntax_kind == BuiltinSyntaxKind::WriteLn) {
			std::vector<WriteCall::Item> items;
			if (maybe_parse_opening_paren()) {
				if (input_token != ")") {
					do {
						Node* item = parse_expression();
						// Unlike an ordinary call, Write has no formal
						// parameter to give an untyped integer literal its
						// default Pascal carrier.
						if (item->ty == &untyped_integer_type())
							item = cast(item, integer_type());
						Node* width = nullptr;
						Node* precision = nullptr;
						if (maybe_parse_colon()) {
							width = cast(parse_expression(), sizeint_type());
							if (maybe_parse_colon())
								precision = cast(
								    parse_expression(),
								    sizeint_type());
						}
						if (precision &&
						    item->ty != single_type() &&
						    item->ty != double_type() &&
						    item->ty != extended_type()) {
							raise_parse_error(
							    "a second Write/WriteLn colon qualifier "
							    "requires a real value");
						}
						items.push_back(
						    WriteCall::Item{
						        item, width, precision});
					} while (maybe_parse_comma());
				}
				parse_closing_paren();
			}

			Node* file = nullptr;
			if (!items.empty() &&
			    items.front().value->ty == text_type()) {
				if (items.front().width ||
				    items.front().precision) {
					raise_parse_error(
					    "a Write/WriteLn text-file argument "
					    "cannot have formatting qualifiers");
				}
				file = items.front().value;
				if (!is_referenceable(file))
					raise_parse_error(
					    "Write/WriteLn text-file argument "
					    "must be storage-backed");
				items.erase(items.begin());
			}
			return new WriteCall(
			    syntax_kind == BuiltinSyntaxKind::WriteLn,
			    file, std::move(items));
		}
		if (syntax_kind_for_builtin(value) == BuiltinSyntaxKind::SizeOf &&
		    input_token == "(") {
			parse_opening_paren();
			Type* operand_type = nullptr;
			if (Type* named_type = maybe_resolve_type(input_token);
			    named_type && !maybe_resolve_value(input_token)) {
				operand_type = parse_type_expression(false);
			} else {
				Node* operand = parse_expression();
				operand_type = operand ? operand->ty : nullptr;
			}
			parse_closing_paren();
			if (!operand_type)
				raise_parse_error("SizeOf operand has no type");
			return new SizeOf(operand_type);
		}
		return value;
	}
	if (input_token == "(") {
		if (Type* target_ty = maybe_resolve_type(id)) {
			parse_opening_paren();
			Node* value = parse_expression();
			parse_closing_paren();

			// Pascal typecast syntax is `Type(expr)`. This is an explicit cast,
			// not a value call and not the implicit-conversion helper `cast()`, so
			// it must be parsed from the type namespace and represented directly.
			if (target_ty == pointer_type()) {
				if (auto reference =
				        dynamic_cast<RoutineRef*>(value))
					return resolve_routine_code_reference(
					    reference);
			}
			if (auto target_routine =
			        dynamic_cast<RoutineType*>(target_ty)) {
				if (auto reference =
				        dynamic_cast<RoutineRef*>(value))
					return resolve_routine_reference(
					    reference, target_routine);
				if (dynamic_cast<NilLiteral*>(value))
					return cast(value, target_routine);
				if (value->ty == tmethod_type()) {
					if (target_routine->kind != METHOD)
						raise_parse_error(
						    "TMethod can only be cast to an "
						    "of-object routine type");
					return new Cast(value, target_routine);
				}
				if (auto source_routine =
				        dynamic_cast<RoutineType*>(
				            value->ty)) {
					if (!routine_types_compatible(
					        source_routine,
					        target_routine))
						raise_parse_error(
						    "explicit cast between "
						    "incompatible routine types");
					return value;
				}
				raise_parse_error(
				    "explicit routine-type cast requires a "
				    "routine value, TMethod, nil, or routine "
				    "reference");
			}
			if (target_ty == tmethod_type()) {
				auto source_routine =
				    dynamic_cast<RoutineType*>(value->ty);
				if (!source_routine ||
				    source_routine->kind != METHOD)
					raise_parse_error(
					    "TMethod can only view an of-object "
					    "routine value");
			}
			return new Cast(value, target_ty);
		}
	}
	if (auto class_type =
	        dynamic_cast<ClassType*>(maybe_resolve_type(id))) {
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
	if (current_routine &&
	    (current_routine->ty->kind == CLASS_CONSTRUCTOR ||
	     current_routine->ty->kind == CLASS_DESTRUCTOR))
		raise_parse_error(
		    "inherited is not supported in a class lifecycle hook");

	std::string name;
	auto opt = maybe_parse_identifier();
	if (opt) {
		name = *opt;
	} else if (current_routine) {
		name = current_routine->pas_name;
	} else {
		raise_parse_error("inherited requires an enclosing method");
	}

	if (!current_routine)
		raise_parse_error("inherited requires an enclosing method");
	auto cur_method = dynamic_cast<Method*>(current_routine);
	if (!cur_method || !cur_method->owner_class)
		raise_parse_error("inherited requires an enclosing method on a class/object");
	Type* parent = parent_of(cur_method->owner_class);
	if (!parent)
		raise_parse_error("inherited: enclosing type has no parent");

	// lookup returns Node* (Callable* OR OverloadSet*). For the parens form,
	// finalize_call ranks overload sets by argument cost -- same path direct
	// calls take. For the no-parens form, we require an unambiguous single
	// candidate.
	Node* hit = lookup_method_in_ancestors(name, parent);
	if (!hit)
		raise_parse_error("inherited: '" + name + "' not found in parent chain");

	Callable* resolved = nullptr;
	std::vector<Node*> args;
	if (maybe_parse_opening_paren()) {
		if (input_token != ")") {
			args.push_back(parse_expression());
			while (maybe_parse_comma())
				args.push_back(parse_expression());
		}
		parse_closing_paren();
		auto fc = finalize_call(hit, args, name, current_location());
		// fc.receiver stays unused -- InheritedCall uses qualified-id syntax
		// (Parent::X(args)), not member-access.
		resolved = dynamic_cast<Callable*>(fc.callee);
		if (!resolved)
			raise_parse_error("inherited: overload resolution failed");
	} else {
		// No-parens form: must be a single Callable, not a multi-member
		// overload set.
		resolved = dynamic_cast<Callable*>(hit);
		if (!resolved) {
			if (auto os = dynamic_cast<OverloadSet*>(hit)) {
				if (os->members.size() == 1)
					resolved = os->members.front();
				else
					raise_parse_error("inherited: '" + name +
							  "' is overloaded; supply an argument list to disambiguate");
			}
		}
		if (!resolved)
			raise_parse_error("inherited: '" + name + "' did not resolve to a method");
	}

	auto n = new InheritedCall();
	n->resolved = resolved;
	n->args = std::move(args);
	n->ty = call_result_type(resolved);
	if (current_routine->ty->kind == DESTRUCTOR &&
	    resolved->ty->kind == DESTRUCTOR) {
		auto current_method =
		    dynamic_cast<Method*>(current_routine);
		auto resolved_method =
		    dynamic_cast<Method*>(resolved);
		// Class destructors currently use C++ destructor auto-chaining.
		// Old-style object destructors deliberately do not: they are ordinary
		// Pascal methods, and `inherited Done` is the only thing that invokes
		// the ancestor body.
		n->dropped =
		    current_method && resolved_method &&
		    dynamic_cast<ClassType*>(
		        current_method->owner_class) &&
		    dynamic_cast<ClassType*>(
		        resolved_method->owner_class);
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
	if (auto c = dynamic_cast<Callable*>(callee))
		return static_cast<RoutineType*>(c->ty)->return_type;
	if (callee)
		if (auto ty = dynamic_cast<RoutineType*>(callee->ty))
			return ty->return_type;
	return nullptr;
}

// Small helper: is NODE a bare callable reference (Callable, OverloadSet, or
// a MemberAccess whose member is either)? Used both for the auto-call check
// and to decide whether to peel a MemberAccess in finalize_call.
static bool node_is_bare_callable(Node* n) {
	if (!n)
		return false;
	if (dynamic_cast<Callable*>(n) || dynamic_cast<OverloadSet*>(n))
		return true;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		return dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b);
	}
	return false;
}

Node* Parser::maybe_auto_call(Node* n) {
	if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		if (!property->property->index_types.empty() && property->indexes.empty())
			raise_parse_error("indexed property '" + property->property->pas_name +
				"' requires an index argument list");
		if (!property->property->read_accessor)
			raise_parse_error("write-only property '" + property->property->pas_name +
				"' cannot be read");
		return n;
	}
	if (!node_is_bare_callable(n))
		return n;
	// finalize_call handles the empty-args case: for a Callable it checks
	// that either no formals exist or all remaining formals have defaults;
	// for an OverloadSet it runs ranking and picks the parameterless winner.
	// A candidate that requires args will fail there with a clear error.
	std::vector<Node*> args;
	auto fc = finalize_call(n, args, /*name for error*/ "", current_location());
	return make_call(fc, std::move(args));
}

static Frame* body_frame_of(Type* ty) {
	if (auto r = dynamic_cast<RecordType*>(ty))
		return r->children;
	if (auto r = dynamic_cast<PackedRecordType*>(ty))
		return r->children;
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->children;
	if (auto c = dynamic_cast<ClassRefType*>(ty))
		return body_frame_of(c->target);
	if (auto c = dynamic_cast<InterfaceType*>(ty))
		return c->children;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->children;
	return nullptr;
}

static Frame* body_frame_of(Node* value) {
	if (auto unit = dynamic_cast<UnitRef*>(value))
		return unit->unit ? unit->unit->frame : nullptr;
	return body_frame_of(value ? value->ty : nullptr);
}

Node* Parser::parse_designator() {
	return parse_designator_tail(parse_value());
}

Node* Parser::parse_member_selection(Node* base) {
	// A callable base is invoked before selecting a member from its result.
	base = maybe_auto_call(base);
	std::string member_name = parse_identifier();
	Frame* members = body_frame_of(base);
	if (!members)
		raise_parse_error(
		    "member access on non-composite type");
	Node* member =
	    members->lookup_value(member_name);
	if (!member)
		raise_parse_error(
		    "no member '" + member_name + "'");
	return bind_lookup_result(base, member);
}

Node* Parser::parse_designator_tail(Node* result) {
	while (true) {
		if (maybe_parse_period()) {
			result = parse_member_selection(result);
		} else if (input_token == "(") {
			SourceLocation call_location = current_location();
			parse_opening_paren();
			// Bracketed n-ary: RHS is a comma-separated list of expressions.
			// No auto-call before `(` -- this `(` IS the call.
			std::vector<Node*> args;
			if (input_token != ")") {
				args.push_back(parse_expression());
				while (maybe_parse_comma())
					args.push_back(parse_expression());
			}
			parse_closing_paren();
			auto fc = finalize_call(result, args, /*name_for_error*/ "", call_location);
			result = make_call(fc, std::move(args));
			continue;
		} else if (maybe_parse_opening_bracket()) {
			// Parse the complete bracket argument list before resolving it.
			// User array properties receive the list as one application;
			// multidimensional native arrays apply their synthesized
			// one-argument property once per nested array dimension.
			auto pending_property = dynamic_cast<PropertyAccess*>(result);
			if (!(pending_property &&
			      !pending_property->property->index_types.empty() &&
			      pending_property->indexes.empty()))
				result = maybe_auto_call(result);
			std::vector<Node*> indexes;
			indexes.push_back(parse_expression());
			while (maybe_parse_comma())
				indexes.push_back(parse_expression());
			parse_closing_bracket();

			if (auto pending = dynamic_cast<PropertyAccess*>(result);
			    pending && !pending->property->index_types.empty() && pending->indexes.empty()) {
				result = apply_property(pending->receiver, pending->property, std::move(indexes));
				continue;
			}

			if (dynamic_cast<FixedArrayType*>(result->ty) && indexes.size() > 1) {
				for (Node* index : indexes) {
					Property* property = default_property_for_type(result->ty);
					if (!property)
						raise_parse_error("index on type without a default property");
					result = apply_property(result, property, {index});
				}
				continue;
			}

			Property* property = default_property_for_type(result->ty);
			if (!property)
				raise_parse_error("index on type without a default property");
			result = apply_property(result, property, std::move(indexes));
		} else if (maybe_parse_circumflex()) {
			// Postfix: no RHS. Auto-call bare callable LHS first (deref of a
			// callable reference is nonsense).
			result = maybe_auto_call(result);
			Type* ct = result->ty;
			auto p = dynamic_cast<PointerType*>(ct);
			if (!p)
				raise_parse_error("deref of non-pointer type");
			auto d = new Dereference(result);
			// Untyped Pointer^ is a Pascal place, not a readable value of
			// some fabricated element type. unknown_type() lets only
			// place-aware consumers such as an omitted-type var formal use
			// it; emission must never attempt C++ unary `*` on void*.
			d->ty = p->is_untyped()
			    ? unknown_type()
			    : p->item_type;
			result = d;
		} else {
			break;
		}
	}
	return result;
}

bool Parser::is_assignable(Node* n) {
	if (!n)
		return false;
	if (dynamic_cast<StorageSlot*>(n))
		return true;
	if (auto dereference = dynamic_cast<Dereference*>(n))
		return dereference->ty != unknown_type();
	if (dynamic_cast<Index*>(n))
		return true;
	if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		if (!property->property || !property->property->write_accessor)
			return false;
		Node* accessor = property->property->write_accessor;
		if (dynamic_cast<Builtin*>(accessor)) {
			// Reference-backed indexing can write only through a stable base.
			// Packed projections are admitted here solely so the subsequent
			// is_supported_packed_assignment check can select or reject their
			// explicit synchronous copyback lowering.
			return contains_packed_projection(property->receiver) ||
			       is_referenceable(property->receiver);
		}
		if (dynamic_cast<StorageSlot*>(accessor) ||
		    dynamic_cast<Callable*>(accessor))
			return (property->receiver->ty && property->receiver->ty->is_reference_type()) ||
			       is_referenceable(property->receiver);
		return false;
	}
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (auto view = dynamic_cast<Cast*>(ma->a)) {
			auto field =
			    dynamic_cast<StorageSlot*>(ma->b);
			if (view->ty == tmethod_type() &&
			    (field == tmethod_code_field() ||
			     field == tmethod_data_field())) {
				auto routine = dynamic_cast<RoutineType*>(
				    view->a ? view->a->ty : nullptr);
				return routine &&
				       routine->kind == METHOD &&
				       is_assignable(view->a);
			}
		}
		return dynamic_cast<StorageSlot*>(ma->b) != nullptr;
	}
	if (auto cast = dynamic_cast<Cast*>(n)) {
		// FPC treats an explicit ordinal cast as a view of its operand's
		// storage when both ordinal carriers have the same size. Restrict this
		// to tpcc intrinsic ordinal carriers: C++ enum/Boolean objects cannot
		// safely hold every bit pattern that FPC permits through such a view.
		auto intrinsic_ordinal_carrier = [](Type* ty) -> IntrinsicType* {
			while (auto subrange = dynamic_cast<SubrangeType*>(ty))
				ty = subrange->base_type;
			auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
			return intrinsic && intrinsic->ordinal_bounds
			    ? intrinsic
			    : nullptr;
		};
		if (!intrinsic_ordinal_carrier(cast->a ? cast->a->ty : nullptr) ||
		    !intrinsic_ordinal_carrier(cast->ty))
			return false;
		auto source_layout = type_layout(cast->a->ty);
		auto target_layout = type_layout(cast->ty);
		return source_layout && target_layout &&
		       source_layout->size == target_layout->size &&
		       is_referenceable(cast->a) &&
		       !contains_packed_projection(cast->a);
	}
	return false;
}

bool Parser::property_read_is_place(PropertyAccess* access) {
	if (!access || !access->property || !access->property->read_accessor)
		return false;
	Node* accessor = access->property->read_accessor;
	if (dynamic_cast<StorageSlot*>(accessor))
		return access->receiver->ty && access->receiver->ty->is_reference_type()
		    ? true
		    : is_referenceable(access->receiver);
	if (auto builtin = dynamic_cast<Builtin*>(accessor))
		return builtin->desc && builtin->desc->cxx_name == "::u_system::p_index" &&
		       is_referenceable(access->receiver);
	return false; // ordinary Pascal getter calls return values
}

bool Parser::is_referenceable(Node* n) {
	if (!n)
		return false;
	if (dynamic_cast<StorageSlot*>(n) ||
	    dynamic_cast<Dereference*>(n))
		return true;
	if (auto property = dynamic_cast<PropertyAccess*>(n))
		return property_read_is_place(property);
	if (auto index = dynamic_cast<Index*>(n))
		return is_referenceable(index->a);
	if (auto member = dynamic_cast<MemberAccess*>(n)) {
		if (!dynamic_cast<StorageSlot*>(member->b))
			return false;
		if (dynamic_cast<UnitRef*>(member->a))
			return true;
		if (member->a->ty && member->a->ty->is_reference_type())
			return true;
		return is_referenceable(member->a);
	}
	return false;
}

bool Parser::contains_packed_projection(Node* n) {
	if (!n)
		return false;
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (ma->a && dynamic_cast<PackedRecordType*>(ma->a->ty))
			return true;
		return contains_packed_projection(ma->a);
	}
	if (auto ix = dynamic_cast<Index*>(n))
		return contains_packed_projection(ix->a);
	if (auto property = dynamic_cast<PropertyAccess*>(n))
		return contains_packed_projection(property->receiver);
	if (dynamic_cast<Dereference*>(n))
		return false;
	if (auto ca = dynamic_cast<Cast*>(n))
		return contains_packed_projection(ca->a);
	return false;
}

bool Parser::is_supported_packed_assignment(Node* n) {
	if (auto ma = dynamic_cast<MemberAccess*>(n)) {
		if (!ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty))
			return false;
		if (auto overlay = dynamic_cast<Cast*>(ma->a))
			return is_assignable(overlay->a) && !contains_packed_projection(overlay->a);
		return is_assignable(ma->a) && !contains_packed_projection(ma->a);
	}
	if (auto ix = dynamic_cast<Index*>(n)) {
		auto ma = dynamic_cast<MemberAccess*>(ix->a);
		if (!ma || !ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty))
			return false;
		auto overlay = dynamic_cast<Cast*>(ma->a);
		return overlay && is_assignable(overlay->a) &&
		       !contains_packed_projection(overlay->a);
	}
	if (auto property = dynamic_cast<PropertyAccess*>(n)) {
		auto ma = dynamic_cast<MemberAccess*>(property->receiver);
		if (!ma || !ma->a || !dynamic_cast<PackedRecordType*>(ma->a->ty))
			return false;
		auto overlay = dynamic_cast<Cast*>(ma->a);
		return overlay && is_assignable(overlay->a) &&
		       !contains_packed_projection(overlay->a);
	}
	return false;
}

Node* Parser::mk_arith(std::string id, Node* a, Node* b) {
	auto fn = resolve_value(id);
	std::vector<Node*> args;
	auto common_ty = common_arith_type(a->ty, b->ty);
	if (common_ty == nullptr) {
		args.push_back(a);
		args.push_back(b);
	} else {
		args.push_back(cast(a, common_ty));
		args.push_back(cast(b, common_ty));
	}
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	return make_call(fc, std::move(args));
}

Node* Parser::mk_assign(Node* a, Node* b) {
	return new Assign(a, cast(b, a->ty));
}

Node* Parser::mk_compare(std::string id, Node* a, Node* b) {
	// `nil` has no type of its own. For an ordinary pointer comparison, the
	// other operand supplies its exact pointer type before normal operator
	// resolution runs. Do not widen this to every reference type: classes and
	// class references have their own comparison rules and are not Pascal
	// Pointer values.
	auto a_pointer = a ? dynamic_cast<PointerType*>(a->ty) : nullptr;
	auto b_pointer = b ? dynamic_cast<PointerType*>(b->ty) : nullptr;
	if (!a_pointer && dynamic_cast<NilLiteral*>(a) && b_pointer) {
		a = cast(a, b_pointer);
		a_pointer = b_pointer;
	}
	if (!b_pointer && dynamic_cast<NilLiteral*>(b) && a_pointer) {
		b = cast(b, a_pointer);
		b_pointer = a_pointer;
	}

	RoutineType* a_routine =
	    a ? dynamic_cast<RoutineType*>(a->ty) : nullptr;
	RoutineType* b_routine =
	    b ? dynamic_cast<RoutineType*>(b->ty) : nullptr;
	if (!a_routine && dynamic_cast<NilLiteral*>(a) && b_routine) {
		a = cast(a, b_routine);
		a_routine = b_routine;
	}
	if (!b_routine && dynamic_cast<NilLiteral*>(b) && a_routine) {
		b = cast(b, a_routine);
		b_routine = a_routine;
	}
	if (a_routine || b_routine) {
		if (id != "=" || !a_routine || !b_routine ||
		    !routine_types_compatible(a_routine, b_routine))
			raise_parse_error(
			    "routine values can only be compared for equality "
			    "with a compatible routine type");
		auto equal = new RoutineEqual(a, b);
		equal->ty = boolean_type();
		return equal;
	}

	auto fn = resolve_value(id);
	std::vector<Node*> args;
	auto common_ty = common_arith_type(a->ty, b->ty);
	if (common_ty == nullptr) {
		args.push_back(a);
		args.push_back(b);
	} else {
		args.push_back(cast(a, common_ty));
		args.push_back(cast(b, common_ty));
	}
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	Node* call = make_call(fc, std::move(args));
	/*	if (call->ty->return_type != boolean_type()) {
			raise_type_mismatch("custom comparison operator '" + id + "' has wrong return type", boolean_type(), call->ty);
		} FIXME */
	return call;
}

Node* Parser::mk_membership(Node* item, Node* set) {
	auto set_type = dynamic_cast<FixedSetType*>(set ? set->ty : nullptr);
	if (!set_type)
		raise_parse_error("right operand of 'in' is not a set");
	if (set_type->item_type == unknown_type()) {
		if (!is_set_item_type(item->ty))
			raise_parse_error("left operand of 'in' is not ordinal");
		set = cast(set, new FixedSetType(current_location(), item->ty));
		set_type = static_cast<FixedSetType*>(set->ty);
	}

	std::vector<Node*> args{
	    cast(item, set_type->item_type),
	    set,
	};
	Node* fn = resolve_value("in");
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	return make_call(fc, std::move(args));
}

Node* Parser::mk_unary_same(std::string id, Node* x) {
	if (auto i = dynamic_cast<Integer*>(x)) {
		if (x->ty == &untyped_integer_type()) {
			if (id == "-")
				return new Integer(i->value, i->ty, !i->negative);
			if (id == "+")
				return x;
			if (id == "not") {
				// An untyped Pascal integer literal has Integer semantics in
				// unary `not` (`not 1 = -2`). Pin it before overload
				// resolution; otherwise every width-preserving integer
				// LogicalNot signature is an equally exact literal match.
				x->ty = integer_type();
			}
		}
	}
	auto fn = resolve_value(id);
	std::vector<Node*> args;
	args.push_back(x);
	auto fc = finalize_call(fn, args, /*name for error*/ "", current_location());
	Node* call = make_call(fc, std::move(args));
	/*	if (call->ty->return_type != x->ty) {
			raise_type_mismatch("custom unary operator '" + id + "' has wrong return type", x->ty, call->ty);
		} FIXME */
	return call;
}

Node* Parser::parse_power_tail(Node* result) {
	result = maybe_auto_call(result);
	while (maybe_parse_star_star()) {
		result = mk_arith("**", result, parse_power());
	}
	return result;
}

Node* Parser::parse_power() {
	if (maybe_parse_keyword("not")) {
		return mk_unary_same("not", parse_power());
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
				bool has_instance_method =
				    dynamic_cast<Method*>(candidates);
				if (auto overloads =
				        dynamic_cast<OverloadSet*>(
				            candidates))
					for (Callable* candidate :
					     overloads->members)
						has_instance_method =
						    has_instance_method ||
						    dynamic_cast<Method*>(
						        candidate);
				if (has_instance_method)
					receiver = member->a;
			}
			return parse_power_tail(
			    new RoutineRef(receiver, candidates));
		}
		if (contains_packed_projection(x))
			raise_parse_error("address of a packed-record field is not available");
		if (!is_referenceable(x))
			raise_parse_error("address requires a storage-backed expression");
		auto n = new AddrOf(x);
		n->ty = x->ty ? static_cast<Type*>(new PointerType(current_location(), x->ty)) : nullptr;
		return parse_power_tail(n);
	} else if (maybe_parse_minus()) {
		return mk_unary_same("-", parse_power());
	} else if (maybe_parse_plus()) {
		return mk_unary_same("+", parse_power());
	}

	// Mirror FPC's quirk. `-1 ** 4` parses as `-(1 ** 4)`, not `(-1) ** 4`.
	// FIXME: Fix it later.
	return parse_power_tail(parse_designator());
}

Node* Parser::parse_product_tail(Node* result) {
	while (true) {
		if (maybe_parse_star()) {
			result = mk_arith("*", result, parse_power());
		} else if (maybe_parse_slash()) {
			result = mk_arith("/", result, parse_power());
		} else if (maybe_parse_keyword("div")) {
			result = mk_arith("div", result, parse_power());
		} else if (maybe_parse_keyword("mod")) {
			result = mk_arith("mod", result, parse_power());
		} else if (maybe_parse_keyword("and") || maybe_parse_ampersand()) {
			auto b = parse_power();
			if (result->ty == boolean_type() && b->ty == boolean_type()) {
				auto n = new ShortCircuitOperation(AND, result, b);
				n->ty = boolean_type();
				result = n;
			} else {
				result = mk_arith("and", result, b);
			}
		} else if (maybe_parse_keyword("shl")) {
			result = mk_arith("shl", result, parse_power());
		} else if (maybe_parse_keyword("shr")) {
			result = mk_arith("shr", result, parse_power());
		} else if (maybe_parse_keyword("as")) {
			Type* target = parse_type_expression(false);
			bool numeric =
			    target == single_type() ||
			    target == double_type() ||
			    target == extended_type();
			bool checked_reference =
			    (dynamic_cast<ClassType*>(result->ty) ||
			     dynamic_cast<InterfaceType*>(result->ty)) &&
			    (dynamic_cast<ClassType*>(target) ||
			     dynamic_cast<InterfaceType*>(target));
			if ((!numeric ||
			     conversion_cost(result->ty, target) < 0) &&
			    !checked_reference)
				raise_parse_error(
				    "'as' requires compatible real-number "
				    "or class/interface types");
			result = new Coerce(result, target);
		} else if (maybe_parse_keyword("is")) {
			// FPC RELEASED BUG: `_OP_IS` sits in opmultiply in FPC 3.x,
			// making `is` bind tighter than `+` (a Delphi-compatibility bug,
			// fixed in FPC trunk). Match FPC 3.2.x behavior here for parity.
			Type* target = parse_type_expression(false);
			if (!(dynamic_cast<ClassType*>(result->ty) ||
			      dynamic_cast<InterfaceType*>(result->ty)) ||
			    !(dynamic_cast<ClassType*>(target) ||
			      dynamic_cast<InterfaceType*>(target)))
				raise_parse_error(
				    "'is' requires class/interface types");
			auto n = new CoerceCheck(result, target);
			n->ty = boolean_type();
			result = n;
		} else if (maybe_parse_less_less()) {
			result = mk_arith("shl", result, parse_power());
		} else if (maybe_parse_greater_greater()) {
			result = mk_arith("shr", result, parse_power());
		} else if (maybe_parse_symdiff()) {
			result = mk_arith("><", result, parse_power());
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
		if (maybe_parse_plus()) {
			result = mk_arith("+", result, parse_product());
		} else if (maybe_parse_minus()) {
			result = mk_arith("-", result, parse_product());
		} else if (maybe_parse_keyword("or") || maybe_parse_pipe()) {
			auto b = parse_product();
			if (result->ty == boolean_type() && b->ty == boolean_type()) {
				auto n = new ShortCircuitOperation(OR, result, b);
				n->ty = boolean_type();
				result = n;
			} else {
				result = mk_arith("or", result, b);
			}
		} else if (maybe_parse_keyword("xor")) {
			auto b = parse_product();
			// FIXME: constant fold; check result type; if bool: emit LogicalOperation(XOR, ...) instead;
			result = mk_arith("xor", result, b);
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_sum() {
	return parse_sum_tail(parse_product());
}

Node* Parser::parse_subrange_bound_expression_after_identifier(std::string id) {
	Node* result = parse_designator_tail(parse_value_from_identifier(std::move(id)));
	return parse_sum_tail(parse_product_tail(parse_power_tail(result)));
}

Node* Parser::parse_subrange_bound_expression() {
	return parse_sum();
}

Node* Parser::parse_comparison_tail(Node* result) {
	while (true) {
		if (maybe_parse_equal()) {
			result = mk_compare("=", result, parse_sum());
		} else if (maybe_parse_less_greater()) {
			// Pascal <> is inequality. Do not require or expose a separate custom
			// operator<> declaration; derive it from equality and boolean not so user
			// equality overloads participate consistently.
			result = mk_unary_same("not", mk_compare("=", result, parse_sum()));
		} else if (maybe_parse_less()) {
			result = mk_compare("<", result, parse_sum());
		} else if (maybe_parse_greater()) {
			result = mk_compare(">", result, parse_sum());
		} else if (maybe_parse_less_equal()) {
			result = mk_compare("<=", result, parse_sum());
		} else if (maybe_parse_greater_equal()) {
			result = mk_compare(">=", result, parse_sum());
		} else if (maybe_parse_keyword("in")) {
			result = mk_membership(result, parse_sum());
		} else {
			break;
		}
	}
	return result;
}

Node* Parser::parse_comparison() {
	return parse_comparison_tail(parse_sum());
}

Node* Parser::parse_expression_after_identifier(std::string id) {
	Node* result = parse_designator_tail(parse_value_from_identifier(std::move(id)));
	return parse_comparison_tail(parse_sum_tail(parse_product_tail(parse_power_tail(result))));
}

Node* Parser::parse_expression() {
	return parse_comparison();
}

Property* Parser::default_property_for_type(Type* ty) {
	if (!ty)
		return nullptr;
	if (ty->default_property)
		return ty->default_property;

	// Built-in containers participate through the same Property node used by
	// source declarations. Their accessor is the RTL reference operation.
	if (auto array = dynamic_cast<FixedArrayType*>(ty)) {
		auto accessor = create_builtin_value("::u_system::p_index");
		array->default_property = new Property(
		    "items", array->item_type, {array->range.base_type},
		    accessor, accessor, true);
		return array->default_property;
	}
		if (dynamic_cast<ShortStringType*>(ty) ||
		    ty == ansistring_type()) {
		Node* read_accessor = create_builtin_value("::u_system::p_index");
		Node* write_accessor = ty == ansistring_type()
		    ? create_builtin_value("::u_system::tpcc_index_write")
		    : read_accessor;
		ty->default_property = new Property(
		    "items", char_type(), {integer_type()},
		    read_accessor, write_accessor, true);
		return ty->default_property;
	}
	if (auto pointer = dynamic_cast<PointerType*>(ty)) {
		if (pointer->is_untyped())
			return nullptr;
		auto accessor = create_builtin_value("::u_system::p_index");
		pointer->default_property = new Property(
		    "items", pointer->item_type, {integer_type()},
		    accessor, accessor, true);
		return pointer->default_property;
	}

	// FPC selects the nearest default property from the expression's static
	// type hierarchy. It does not dynamically dispatch property declarations.
	if (auto c = dynamic_cast<ClassType*>(ty))
		return c->super ? default_property_for_type(c->super) : nullptr;
	if (auto o = dynamic_cast<ObjectType*>(ty))
		return o->super ? default_property_for_type(o->super) : nullptr;
	if (auto i = dynamic_cast<InterfaceType*>(ty)) {
		for (auto* parent : i->super_interfaces)
			if (auto* property = default_property_for_type(parent))
				return property;
	}
	return nullptr;
}

PropertyAccess* Parser::apply_property(Node* receiver, Property* property, std::vector<Node*> indexes) {
	if (!property)
		raise_parse_error("internal error: missing property");
	if (indexes.size() != property->index_types.size()) {
		std::ostringstream message;
		message << "property '" << property->pas_name << "' expects "
		        << property->index_types.size() << " index argument(s), got "
		        << indexes.size();
		raise_parse_error(message.str());
	}
	for (size_t i = 0; i < indexes.size(); ++i) {
		if (conversion_cost(indexes[i]->ty, property->index_types[i]) < 0)
			raise_type_mismatch("property index argument", property->index_types[i], indexes[i]->ty);
		indexes[i] = cast(indexes[i], property->index_types[i]);
	}
	return new PropertyAccess(receiver, property, std::move(indexes));
}

void Parser::parse_property_declaration(Frame* body, Type* owner_type) {
	parse_keyword("property");
	std::string property_name = parse_identifier();
	std::vector<Type*> index_types;
	if (maybe_parse_opening_bracket()) {
		if (input_token == "]")
			raise_parse_error("property index parameter list cannot be empty");
		do {
			// FPC accepts normal value and const index parameters. Their mode is
			// checked against the accessor signature later; property resolution
			// itself needs only the declared index types.
			maybe_parse_keyword("const");
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma())
				names.push_back(parse_identifier());
			parse_colon();
			Type* index_type = parse_type_expression(false);
			for (size_t i = 0; i < names.size(); ++i)
				index_types.push_back(index_type);
		} while (maybe_parse_semicolon());
		parse_closing_bracket();
	}
	parse_colon();
	Type* property_type = parse_type_expression(false);

	Node* read_accessor = nullptr;
	Node* write_accessor = nullptr;
	while (input_token != ";") {
		if (maybe_parse_directive("read")) {
			std::string name = parse_identifier();
			read_accessor = body->lookup_value(name);
			if (!read_accessor)
				raise_parse_error("unknown read accessor '" + name + "' for property '" + property_name + "'");
		} else if (maybe_parse_directive("write")) {
			std::string name = parse_identifier();
			write_accessor = body->lookup_value(name);
			if (!write_accessor)
				raise_parse_error("unknown write accessor '" + name + "' for property '" + property_name + "'");
		} else {
			raise_parse_error("expected read or write accessor in property '" + property_name + "'");
		}
	}
	if (!read_accessor && !write_accessor)
		raise_parse_error("property '" + property_name + "' has no accessor");

	auto validate_index_formals = [&](RoutineType* routine, size_t count, const char* which) {
		if (routine->formals.size() != count)
			raise_parse_error(std::string(which) + " accessor for property '" + property_name +
				"' has the wrong number of parameters");
		for (size_t i = 0; i < index_types.size(); ++i)
			if (routine->formals[i].ty != index_types[i])
				raise_type_mismatch(std::string(which) + " property index parameter",
					index_types[i], routine->formals[i].ty);
	};
	if (read_accessor) {
		if (auto field = dynamic_cast<StorageSlot*>(read_accessor)) {
			if (!index_types.empty())
				raise_parse_error("indexed property read accessor must be a method");
			if (field->ty != property_type)
				raise_type_mismatch("property read field", property_type, field->ty);
		} else if (auto getter = dynamic_cast<Callable*>(read_accessor)) {
			auto routine = static_cast<RoutineType*>(getter->ty);
			validate_index_formals(routine, index_types.size(), "read");
			if (routine->return_type != property_type)
				raise_type_mismatch("property getter return type", property_type, routine->return_type);
		} else {
			raise_parse_error("property read accessor must be a field or method");
		}
	}
	if (write_accessor) {
		if (auto field = dynamic_cast<StorageSlot*>(write_accessor)) {
			if (!index_types.empty())
				raise_parse_error("indexed property write accessor must be a method");
			if (field->ty != property_type)
				raise_type_mismatch("property write field", property_type, field->ty);
		} else if (auto setter = dynamic_cast<Callable*>(write_accessor)) {
			auto routine = static_cast<RoutineType*>(setter->ty);
			validate_index_formals(routine, index_types.size() + 1, "write");
			if (routine->return_type != &unit_type())
				raise_parse_error("property setter must be a procedure");
			if (routine->formals.back().ty != property_type)
				raise_type_mismatch("property setter value parameter",
					property_type, routine->formals.back().ty);
			auto mode = routine->formals.back().mode;
			if (mode != ParamMode::Value && mode != ParamMode::Const)
				raise_parse_error("property setter value parameter must be a value or const parameter");
		} else {
			raise_parse_error("property write accessor must be a field or method");
		}
	}
	parse_semicolon();

	bool is_default = false;
	if (maybe_parse_directive("default")) {
		is_default = true;
		if (index_types.empty())
			raise_parse_error("default property must have index parameters");
		if (owner_type->default_property)
			raise_parse_error("only one default property may be declared per type");
		parse_semicolon();
	}

	auto property = new Property(
	    property_name, property_type, std::move(index_types),
	    read_accessor, write_accessor, is_default);
	if (!body->register_variable(property_name, property, property_type))
		raise_parse_error("duplicate property '" + property_name + "'");
	if (is_default)
		owner_type->default_property = property;
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
	do {
		if (peek_keyword("end"))
			break;
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
			if (maybe_parse_directive("private"))
				visibility = "strict private";
			else if (maybe_parse_directive("protected"))
				visibility = "strict protected";
			else
				raise_parse_error(
				    "expected private or protected after strict");
			continue;
		} else if (peek_keyword("type")) {
			if (is_class) {
				raise_parse_error("class type unsupported");
			}
			parse_type_block(true);
			is_class = false;
			continue; // type declarations consume their terminating semicolons
		} else if (peek_keyword("const")) {
			if (is_class) {
				raise_parse_error("class const unsupported");
			}
			parse_const_block(owner_class);
			is_class = false;
			continue; // const declarations consume their terminating semicolons
		} else if (peek_keyword("var")) {
			bool class_variables = is_class;
			if (class_variables &&
			    !dynamic_cast<ClassType*>(owner_class))
				raise_parse_error(
				    "class variables require a class container");
			parse_keyword("var");
			// This is an aggregate field section, not a declaration-scope var
			// block.  Keep every field in Pascal declaration order so RecordType
			// layout reconstruction and emitted C++ field order use the same
			// authoritative sequence.
			while (auto first_name = maybe_parse_identifier()) {
				std::vector<std::string> member_names{*first_name};
				while (maybe_parse_comma())
					member_names.push_back(parse_identifier());
				parse_colon();
				auto ty = parse_type_expression(false);
				if (auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
				    dynamic_cast<PackedRecordType*>(owner_class) &&
				    intrinsic && intrinsic->cxx_name == "::u_system::t_ansistring") {
					raise_parse_error("managed fields inside packed records are not implemented");
				}
				for (const auto& member_name : member_names) {
					// A class variable has one storage location owned by its
					// declaring class. It must not become a data member of
					// every metaclass instance: that would give Base.X and
					// Child.X different storage, unlike Pascal.
					auto kind = class_variables
					    ? StorageSlot::Kind::StaticMember
					    : StorageSlot::Kind::AggregateMember;
					auto slot = new StorageSlot(
					    cxx_value_name(member_name), ty, kind,
					    owner_class);
					body->register_variable(member_name, slot, ty);
					if (auto packed = dynamic_cast<PackedRecordType*>(owner_class))
						packed->fields.push_back({member_name, slot, ty});
					else if (auto record = dynamic_cast<RecordType*>(owner_class))
						record->fields.push_back({member_name, slot, ty});
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
			if (dynamic_cast<ClassType*>(
			        owner_class) != nullptr ||
			    dynamic_cast<RecordType*>(
			        owner_class) != nullptr ||
			    dynamic_cast<PackedRecordType*>(
			        owner_class) != nullptr) {
				continue;
			} else {
				raise_parse_error(
				    "class method requires a class or record container");
			}
		} else if (peek_keyword("procedure") || peek_keyword("function") || peek_keyword("destructor") || peek_keyword("constructor")) {
			if (dynamic_cast<PackedRecordType*>(
			        owner_class) &&
			    !is_class)
				raise_parse_error(
				    "instance methods inside packed records are not implemented");
			if (is_class &&
			    (peek_keyword("constructor") ||
			     peek_keyword("destructor"))) {
				auto class_type =
				    dynamic_cast<ClassType*>(owner_class);
				if (!class_type)
					raise_parse_error(
					    "class lifecycle hook requires a class");
				parse_class_lifecycle_prototype(
				    class_type,
				    peek_keyword("constructor")
				        ? CLASS_CONSTRUCTOR
				        : CLASS_DESTRUCTOR);
			} else {
				parse_method_prototype(body, owner_class,
				    peek_keyword("function"),
				    peek_keyword("destructor"),
				    peek_keyword("constructor"), is_class);
			}
			is_class = false;
			continue; // parse_method_prototype consumes its terminating ';'
		} else if (peek_keyword("property")) {
			if (is_class)
				raise_parse_error("'class property' is not implemented");
			parse_property_declaration(body, owner_class);
			continue; // property parser consumes its terminating semicolon(s)
		} else if (peek_keyword("case")) {
			auto rt = dynamic_cast<RecordType*>(owner_class);
			auto packed =
			    dynamic_cast<PackedRecordType*>(
			        owner_class);
			if (is_class) {
				raise_parse_error("variant part only valid in a record, not in a metaclass");
			}
			if (!rt && !packed)
				raise_parse_error("variant part only valid in a record");
			auto variant =
			    parse_record_variant(
			        owner_class, body);
			if (rt)
				rt->variant = variant;
			else
				packed->variant = variant;
			break; // variant part must come last; do not require a trailing ';'
		} else {
			if (is_class) {
				raise_parse_error("class var unsupported");
			}
			// parse_var_block inlined
			std::vector<std::string> member_names;
			do {
				auto member_name = parse_identifier();
				member_names.push_back(member_name);
			} while (maybe_parse_comma());
			parse_colon();
			auto ty = parse_type_expression(false);
			if (auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
			    dynamic_cast<PackedRecordType*>(owner_class) &&
			    intrinsic && intrinsic->cxx_name == "::u_system::t_ansistring") {
				raise_parse_error("managed fields inside packed records are not implemented");
			}
			for (auto member_name : member_names) {
				auto slot = new StorageSlot(
				    cxx_value_name(member_name), ty,
				    StorageSlot::Kind::AggregateMember,
				    owner_class);
				body->register_variable(member_name, slot, ty);
				if (auto packed = dynamic_cast<PackedRecordType*>(owner_class))
					packed->fields.push_back({member_name, slot, ty});
				else if (auto record = dynamic_cast<RecordType*>(owner_class))
					record->fields.push_back({member_name, slot, ty});
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
	return body;
}

VariantPart* Parser::parse_record_variant(
    Type* owner, Frame* body) {
	auto variant = new VariantPart;
	const bool packed =
	    dynamic_cast<PackedRecordType*>(owner);
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
		variant->selector_cxx_name =
		    cxx_value_name(first);
		tag_type = parse_type_expression(false);

		auto slot = new StorageSlot(
		    variant->selector_cxx_name, tag_type,
		    StorageSlot::Kind::AggregateMember, owner);
		body->register_variable(first, slot, tag_type);
		variant->selector_slot = slot;
	} else {
		tag_type = resolve_type(first, false);
	}
	variant->selector_type = tag_type;
	parse_keyword("of");
	while (!peek_keyword("end") &&
	       input_token != ")") {
		// Case label list -- comma-separated constant expressions. Values
		// are discarded: layout is a flat overlapping union regardless of
		// which label is active.
		parse_expression();
		while (maybe_parse_comma())
			parse_expression();
		parse_colon();
		parse_opening_paren();
		VariantArm arm;
		if (input_token != ")") {
			do {
				if (peek_keyword("case")) {
					arm.variant =
					    parse_record_variant(
					        owner, body);
					break;
				}
				std::vector<std::string> names{
				    parse_identifier()};
				while (maybe_parse_comma())
					names.push_back(
					    parse_identifier());
				parse_colon();
				auto fty = parse_type_expression(false);
				if (auto intrinsic =
				        dynamic_cast<IntrinsicType*>(fty);
				    packed && intrinsic &&
				    intrinsic->cxx_name ==
				        "::u_system::t_ansistring")
					raise_parse_error(
					    "managed fields inside packed records are not implemented");
				for (const auto& fname : names) {
					auto slot = new StorageSlot(
					    cxx_value_name(fname), fty,
					    StorageSlot::Kind::AggregateMember,
					    owner);
					// Every variant field shares the record's one member
					// namespace. The recursive arm tree exists only for
					// layout and emission.
					body->register_variable(
					    fname, slot, fty);
					arm.fields.push_back(
					    {fname, slot, fty});
				}
				if (!maybe_parse_semicolon())
					break;
			} while (input_token != ")");
		}
		parse_closing_paren();
		variant->arms.push_back(
		    std::move(arm));
		if (!maybe_parse_semicolon())
			break;
	}
	return variant;
}

Type* Parser::parse_class_type(
    ClassType* completing_forward,
    bool allow_forward_declaration) {
	parse_keyword("class");
	if (maybe_parse_keyword("of")) {
		if (completing_forward)
			raise_parse_error(
			    "forward class declaration must be completed by a class definition");
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
		if (!allow_forward_declaration)
			raise_parse_error(
			    "class forward declaration is only valid as a named type declaration");
		if (completing_forward)
			raise_parse_error(
			    "duplicate forward class declaration");
		auto ct = new ClassType(
		    current_location(), nullptr, {}, nullptr);
		ct->is_forward_declaration = true;
		return ct;
	}
	// Native FPC's class-level abstract option is metadata, independent of
	// abstract methods. It is parsed before the ancestor list, is not
	// inherited, and has no C++ emission effect. The later construction
	// warning can consult the stored flag without changing class lowering.
	const bool is_abstract = maybe_parse_keyword(
	    "abstract");
	ClassType* super_ty = nullptr;
	std::vector<InterfaceType*> implemented_interfaces;
	const bool has_ancestor_list = maybe_parse_opening_paren();
	if (has_ancestor_list) {
		auto s_ty = parse_type_expression(false);
		super_ty = dynamic_cast<ClassType*>(s_ty);
		if (super_ty &&
		    super_ty->is_forward_declaration) {
			raise_parse_error(
			    "superclass forward declaration '" +
			    super_ty->forward_name +
			    "' must be resolved before it is inherited");
		}
		if (super_ty == nullptr) {
			if (auto incomplete =
			        dynamic_cast<IncompleteType*>(s_ty);
			    incomplete && !incomplete->resolved)
				raise_parse_error(
				    "superclass forward declaration '" +
				    incomplete->name +
				    "' must be resolved before it is inherited");
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
		bool defining_system_tobject =
		    current_unit && current_unit->name == "system" &&
		    current_type_declaration_name == "tobject";
		if (!defining_system_tobject)
			super_ty = lookup_implicit_tobject_superclass();
	}
	auto ct = completing_forward
	    ? completing_forward
	    : new ClassType(
	          current_location(), nullptr, {},
	          nullptr);
	ct->implemented_interfaces =
	    std::move(implemented_interfaces);
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
	Unit* system_unit =
	    unit_registry ? unit_registry->lookup("system") : nullptr;
	if (!system_unit) {
		raise_type_parse_error(
		    "implicit class inheritance requires the System unit");
		return nullptr;
	}

	Frame* system_frame = system_unit->frame;
	if (!system_frame) {
		raise_type_parse_error(
		    "implicit class inheritance requires a member frame on the System unit");
		return nullptr;
	}

	Type* tobject_type = system_frame->lookup_type("tobject");
	if (!tobject_type) {
		raise_type_parse_error(
		    "implicit class inheritance requires System.TObject");
		return nullptr;
	}

	std::unordered_set<IncompleteType*> seen;
	while (auto incomplete =
	           dynamic_cast<IncompleteType*>(tobject_type)) {
		if (!seen.insert(incomplete).second) {
			raise_type_parse_error(
			    "System.TObject has a cyclic type definition");
			return nullptr;
		}
		if (!incomplete->resolved) {
			raise_type_parse_error(
			    "System.TObject is unresolved while applying implicit class inheritance");
			return nullptr;
		}
		tobject_type = incomplete->resolved;
	}

	auto tobject_class = dynamic_cast<ClassType*>(tobject_type);
	if (!tobject_class) {
		raise_type_kind_mismatch(
		    "System.TObject used for implicit class inheritance is not a class",
		    "class", tobject_type);
		return nullptr;
	}
	return tobject_class;
}

Type* Parser::parse_interface_type() {
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

Type* Parser::parse_array_type() {
	parse_keyword("array");
	parse_opening_bracket();
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
			if (!ordinal_range_for_type(*it, &range, &error))
				return raise_type_parse_error(error);
		}
		item_type = new FixedArrayType(current_location(), *it, range, item_type);
	}
	return item_type;
}

Type* Parser::parse_enum_type() {
	// Caller already consumed the `(` via maybe_parse_opening_paren in
	// parse_type_expression.
	auto et = new EnumType(current_location());
	et->owning_unit =
	    declaration_unit(current_declaration_frame());
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
			ConstEvalResult folded =
			    expression->const_eval(ctx);
			if (folded.kind ==
			    ConstEvalResult::Kind::NotConstant)
				raise_parse_error(
				    "explicit enum value must be constant");
			if (folded.kind ==
			    ConstEvalResult::Kind::Error)
				raise_parse_error(folded.message);

			if (auto integer =
			        dynamic_cast<Integer*>(folded.node)) {
				const uint64_t maximum =
				    integer->negative
				    ? uint64_t{
				          static_cast<uint64_t>(
				              std::numeric_limits<
				                  int32_t>::max()) +
				          1}
				    : static_cast<uint64_t>(
				          std::numeric_limits<
				              int32_t>::max());
				if (integer->value > maximum)
					raise_parse_error(
					    "explicit enum value is outside "
					    "signed 32-bit range");
				value = integer->negative
				    ? -static_cast<int64_t>(
				          integer->value)
				    : static_cast<int64_t>(
				          integer->value);
			} else if (auto member =
			               dynamic_cast<EnumMemberRef*>(
			                   folded.node)) {
				if (member->ty != et)
					raise_parse_error(
					    "explicit enum value uses a member "
					    "of another enum type");
				value = member->value;
			} else if (auto character =
			               dynamic_cast<String*>(
			                   folded.node);
			           character &&
			           character->ty == char_type() &&
			           character->value.size() == 1) {
				value = static_cast<unsigned char>(
				    character->value[0]);
			} else {
				raise_parse_error(
				    "explicit enum value must be an integer, "
				    "character, or member of the same enum");
			}
		} else if (
		    next_value >
		    std::numeric_limits<int32_t>::max()) {
			raise_parse_error(
			    "implicit enum value is outside signed 32-bit "
			    "range");
		}

		et->members.push_back(
		    {pas, cxx, value, explicit_value});
		// Register the member as a value in the enclosing scope so bare uses
		// (`c := Red`) resolve. Pascal's default is unscoped enum members:
		// they live in the same scope as the enum type itself, NOT inside
		// the type. (A future compiler might add `{$scopedenums+}` and route
		// them through the type; that is not this compiler.)
		auto ref = new EnumMemberRef(cxx, value, et);
		ref->owning_unit = et->owning_unit;
		if (!current_declaration_frame()->register_variable(
		        pas, ref, et))
			raise_parse_error("duplicate identifier: " + pas);
		next_value = value + 1;
		if (!maybe_parse_comma())
			break;
	} while (true);
	parse_closing_paren();
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
	while (auto s = dynamic_cast<SubrangeType*>(ty)) {
		ty = s->base_type;
	}
	return ty;
}

static bool is_integer_semantic_type(Type* ty) {
	ty = subrange_range_type(ty);
	if (ty == &untyped_integer_type())
		return true;
	auto intrinsic = dynamic_cast<IntrinsicType*>(ty);
	return intrinsic && intrinsic->rank;
}

static bool ordinal_bounds_contains(const OrdinalBounds& bounds, bool negative, uint64_t magnitude) {
	if (negative)
		return bounds.signed_type && magnitude <= bounds.min_magnitude;
	return magnitude <= bounds.max_positive;
}

static OrdinalRange::Value ordinal_value(bool negative, uint64_t magnitude) {
	return OrdinalRange::Value{magnitude != 0 && negative, magnitude};
}

static OrdinalRange::Value ordinal_value(int64_t value) {
	if (value < 0)
		return ordinal_value(true, static_cast<uint64_t>(-(value + 1)) + 1);
	return ordinal_value(false, static_cast<uint64_t>(value));
}

static int compare_ordinal_value(OrdinalRange::Value a, OrdinalRange::Value b) {
	if (a.negative != b.negative)
		return a.negative ? -1 : 1;
	if (a.magnitude == b.magnitude)
		return 0;
	if (a.negative)
		return a.magnitude > b.magnitude ? -1 : 1;
	return a.magnitude < b.magnitude ? -1 : 1;
}

static bool checked_add(uint64_t a, uint64_t b, uint64_t* out) {
	if (a > UINT64_MAX - b)
		return false;
	*out = a + b;
	return true;
}

static bool checked_add_one(uint64_t value, uint64_t* out) {
	if (value == UINT64_MAX)
		return false;
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
			    FoldedSubrangeBound::Kind::Char,
			    node,
			    ty,
			    false,
			    i->value,
			    ordinal_value(false, i->value),
			};
		}
		if (!is_integer_semantic_type(ty)) {
			*error = "integer subrange bound has a non-integer type";
			return {};
		}
		return FoldedSubrangeBound{
		    FoldedSubrangeBound::Kind::Integer,
		    node,
		    ty,
		    i->negative,
		    i->value,
		    ordinal_value(i->negative, i->value),
		};
	}
	if (auto s = dynamic_cast<String*>(node)) {
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
		    FoldedSubrangeBound::Kind::Char,
		    as_char,
		    char_type(),
		    false,
		    value,
		    ordinal_value(false, value),
		};
	}
	if (auto e = dynamic_cast<EnumMemberRef*>(node)) {
		Type* ty = subrange_range_type(e->ty);
		if (!dynamic_cast<EnumType*>(ty)) {
			*error = "enum subrange bound has a non-enum type";
			return {};
		}
		return FoldedSubrangeBound{
		    FoldedSubrangeBound::Kind::Enum,
		    node,
		    ty,
		    false,
		    0,
		    ordinal_value(e->value),
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

static bool make_ordinal_range(Type* index_type,
			       Type* base_type,
			       Node* lower_bound,
			       Node* upper_bound,
			       OrdinalRange::Value lower_ordinal,
			       OrdinalRange::Value upper_ordinal,
			       OrdinalRange* out,
			       std::string* error) {
	uint64_t length = 0;
	if (!ordinal_length(lower_ordinal, upper_ordinal, &length, error))
		return false;
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
		if (!lower)
			return false;
		auto upper = classify_subrange_bound(upper_folded.node, error);
		if (!upper)
			return false;
		if (lower->kind != upper->kind) {
			*error = "array subrange bounds must be compatible ordinal constants";
			return false;
		}
		return make_ordinal_range(ty,
					  subrange_range_type(s->base_type),
					  lower->node,
					  upper->node,
					  lower->ordinal_value,
					  upper->ordinal_value,
					  out,
					  error);
	}
	if (auto e = dynamic_cast<EnumType*>(ty)) {
		const auto* lo = e->min_member();
		const auto* hi = e->max_member();
		if (!lo || !hi) {
			*error = "array enum index type has no members";
			return false;
		}
		return make_ordinal_range(ty,
					  ty,
					  new EnumMemberRef(lo->cxx_name, lo->value, ty),
					  new EnumMemberRef(hi->cxx_name, hi->value, ty),
					  ordinal_value(lo->value),
					  ordinal_value(hi->value),
					  out,
					  error);
	}
	OrdinalBounds bounds;
	if (intrinsic_ordinal_bounds(ty, &bounds)) {
		Node* lower_bound = bounds.signed_type ? new Integer(bounds.min_magnitude, ty, true) : new Integer(0, ty);
		Node* upper_bound = new Integer(bounds.max_positive, ty);
		return make_ordinal_range(ty,
					  ty,
					  lower_bound,
					  upper_bound,
					  ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0),
					  ordinal_value(false, bounds.max_positive),
					  out,
					  error);
	}
	*error = "fixed array bounds must be an ordinal type";
	return false;
}

static Type* infer_integer_subrange_host(OrdinalRange::Value lo, OrdinalRange::Value hi, std::string* error) {
	// Match the BP/FPC-style representation choice: choose the smallest
	// builtin integer type that can represent the whole range, preferring
	// signed carriers before same-width unsigned carriers. The backend can
	// erase SubrangeType to this host type while runtime range checks remain
	// a later semantic feature.
	Type* candidates[] = {
	    shortint_type(),
	    byte_type(),
	    smallint_type(),
	    word_type(),
	    integer_type(),
	    cardinal_type(),
	    int64_type(),
	    qword_type(),
	};
	for (Type* candidate : candidates) {
		OrdinalBounds bounds;
		if (!integer_bounds(candidate, &bounds))
			continue;
		OrdinalRange::Value min_value = ordinal_value(bounds.signed_type, bounds.signed_type ? bounds.min_magnitude : 0);
		OrdinalRange::Value max_value = ordinal_value(false, bounds.max_positive);
		if (compare_ordinal_value(lo, min_value) >= 0 && compare_ordinal_value(hi, max_value) <= 0)
			return candidate;
	}
	*error = "integer subrange bounds are outside the supported integer range";
	return nullptr;
}

static Node* convert_integer_subrange_bound(const FoldedSubrangeBound& bound, Type* host, std::string* error) {
	ConstEvalResult converted = const_convert_integer(bound.magnitude, bound.negative, bound.ty, host);
	if (converted.kind == ConstEvalResult::Kind::Success)
		return converted.node;
	if (converted.kind == ConstEvalResult::Kind::Error)
		*error = converted.message;
	else
		*error = "integer subrange bound could not be converted to host type";
	return nullptr;
}

Type* Parser::reuse_subrange_type(Node* lower_bound, Node* upper_bound) {
	ConstEvalContext ctx;
	ConstEvalResult lower_folded = lower_bound->const_eval(ctx);
	ConstEvalResult upper_folded = upper_bound->const_eval(ctx);
	if (lower_folded.kind == ConstEvalResult::Kind::NotConstant || upper_folded.kind == ConstEvalResult::Kind::NotConstant)
		return raise_type_parse_error("subrange bounds must be constant expressions");
	if (lower_folded.kind == ConstEvalResult::Kind::Error)
		return raise_type_parse_error(lower_folded.message);
	if (upper_folded.kind == ConstEvalResult::Kind::Error)
		return raise_type_parse_error(upper_folded.message);

	std::string error;
	auto lower = classify_subrange_bound(lower_folded.node, &error);
	if (!lower)
		return raise_type_parse_error(error);
	auto upper = classify_subrange_bound(upper_folded.node, &error);
	if (!upper)
		return raise_type_parse_error(error);
	if (lower->kind != upper->kind)
		return raise_type_parse_error("subrange bounds must be compatible ordinal constants");

	// FIXME: reuse existing subranges structurally.
	switch (lower->kind) {
	case FoldedSubrangeBound::Kind::Integer: {
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0)
			return raise_type_parse_error("subrange upper bound is lower than lower bound");
		Type* host = infer_integer_subrange_host(lower->ordinal_value, upper->ordinal_value, &error);
		if (!host)
			return raise_type_parse_error(error);
		Node* typed_lower = convert_integer_subrange_bound(*lower, host, &error);
		if (!typed_lower)
			return raise_type_parse_error(error);
		Node* typed_upper = convert_integer_subrange_bound(*upper, host, &error);
		if (!typed_upper)
			return raise_type_parse_error(error);
		return new SubrangeType(current_location(), host, typed_lower, typed_upper);
	}
	case FoldedSubrangeBound::Kind::Char:
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0)
			return raise_type_parse_error("subrange upper bound is lower than lower bound");
		return new SubrangeType(current_location(), char_type(), lower->node, upper->node);
	case FoldedSubrangeBound::Kind::Enum:
		if (lower->ty != upper->ty)
			return raise_type_mismatch("subrange constructor with bounds from the same enum type", lower->ty, upper->ty);
		if (compare_ordinal_value(upper->ordinal_value, lower->ordinal_value) < 0)
			return raise_type_parse_error("subrange upper bound is lower than lower bound");
		return new SubrangeType(current_location(), lower->ty, lower->node, upper->node);
	}
	return raise_type_parse_error("unsupported subrange bound kind");
}

static bool token_continues_subrange_bound_after_primary(const std::string& token) {
	return token == "." || token == "(" || token == "[" || token == "^" ||
	       token == "**" || token == "*" || token == "/" ||
	       token == "div" || token == "mod" || token == "and" ||
	       token == "shl" || token == "shr" || token == "as" ||
	       token == "is" || token == "<<" || token == ">>" ||
	       token == "><" || token == "+" || token == "-" ||
	       token == "or" || token == "|" || token == "xor";
}

static bool token_is_identifier_start(const std::string& token) {
	if (token.empty() || keywords.find(token) != keywords.end())
		return false;
	unsigned char first = static_cast<unsigned char>(token.front());
	return std::isalpha(first) || token.front() == '_';
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
			parse_keyword("string");
			if (!maybe_parse_opening_bracket())
				return shortstring_type();
			Node* capacity_expression = parse_expression();
			parse_closing_bracket();
			ConstEvalContext ctx;
			ConstEvalResult folded =
			    capacity_expression->const_eval(ctx);
			if (folded.kind == ConstEvalResult::Kind::NotConstant)
				return raise_type_parse_error(
				    "shortstring capacity must be a constant integer");
			if (folded.kind == ConstEvalResult::Kind::Error)
				return raise_type_parse_error(folded.message);
			auto capacity = dynamic_cast<Integer*>(folded.node);
			if (!capacity || capacity->negative ||
			    capacity->value == 0 || capacity->value > 255)
				return raise_type_parse_error(
				    "shortstring capacity must be in 1..255");
			return shortstring_type(
			    static_cast<uint8_t>(capacity->value));
	} else if (peek_keyword("set")) {
		parse_keyword("set");
		parse_keyword("of");
		return new FixedSetType(current_location(), parse_type_expression(false));
	} else if (peek_keyword("file")) {
		parse_keyword("file");
		if (!maybe_parse_keyword("of"))
			return file_type();
		return typed_file_type(
		    current_location(), parse_type_expression(false));
	} else if (peek_keyword("array")) {
		return parse_array_type();
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
		if (token_is_identifier_start(input_token)) {
			std::string id = parse_identifier();
			if (maybe_parse_period_period())
				return reuse_subrange_type(parse_value_from_identifier(id), parse_subrange_bound_expression());
			if (maybe_parse_period()) {
				UnitRef* unit =
				    resolve_unit_type_qualifier(id);
				std::string member =
				    parse_identifier();
				Type* result =
				    unit->unit->frame->lookup_type(
				        member);
				if (!result)
					raise_type_parse_error(
					    "unit '" + id +
					    "' has no type '" +
					    member + "'");
				return result;
			}
			if (token_continues_subrange_bound_after_primary(input_token)) {
				Node* lower_bound = parse_subrange_bound_expression_after_identifier(id);
				parse_period_period();
				return reuse_subrange_type(lower_bound, parse_subrange_bound_expression());
			}
			return resolve_type(id, allow_forward);
		}

		Node* lower_bound = parse_subrange_bound_expression();
		parse_period_period();
		return reuse_subrange_type(lower_bound, parse_subrange_bound_expression());
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
	while (!input_token.empty() &&
	       !peek_keyword("end") &&
	       !(stop_at_finalization &&
	         peek_keyword("finalization"))) {
		maybe_parse_statement();
		if (peek_keyword("end") ||
		    (stop_at_finalization &&
		     peek_keyword("finalization")))
			break;
		if (!maybe_parse_semicolon())
			break;
	}
}

void Parser::push_statement_control_context() {
	statement_control_contexts.emplace_back();
}

void Parser::pop_statement_control_context() {
	if (statement_control_contexts.empty())
		raise_parse_error(
		    "internal parser error: statement control context underflow");
	statement_control_contexts.pop_back();
}

unsigned Parser::current_exception_block() const {
	return statement_control_contexts.empty()
	    ? 0
	    : statement_control_contexts.back()
	          .current_exception_block;
}

unsigned Parser::enter_exception_block() {
	if (statement_control_contexts.empty())
		push_statement_control_context();
	auto& context =
	    statement_control_contexts.back();
	context.current_exception_block =
	    ++context.next_exception_block;
	return context.current_exception_block;
}

void Parser::restore_exception_block(
    unsigned block) {
	if (statement_control_contexts.empty())
		return;
	statement_control_contexts.back()
	    .current_exception_block = block;
}

void Parser::record_label_definition(
    const std::string& name, SourceLocation location) {
	if (statement_control_contexts.empty())
		push_statement_control_context();
	auto& state =
	    statement_control_contexts.back().labels[name];
	if (state.definition_block)
		emit_parse_error_at(
		    location,
		    "duplicate statement label '" + name + "'");
	state.definition_block =
	    current_exception_block();
	for (const auto& use : state.goto_blocks)
		if (use.first != *state.definition_block)
			emit_parse_error_at(
			    use.second,
			    "goto may not enter or leave a Pascal exception block");
}

void Parser::record_goto(
    const std::string& name, SourceLocation location) {
	if (statement_control_contexts.empty())
		push_statement_control_context();
	auto& state =
	    statement_control_contexts.back().labels[name];
	const unsigned block =
	    current_exception_block();
	if (state.definition_block &&
	    *state.definition_block != block)
		emit_parse_error_at(
		    location,
		    "goto may not enter or leave a Pascal exception block");
	state.goto_blocks.push_back(
	    {block, std::move(location)});
}

void Parser::parse_label_block() {
	parse_keyword("label");
	do {
		// Pascal label declarations introduce statement labels, not values. For
		// now they are validation-light because C++ also has function-local labels;
		// emission prefixes them separately from value identifiers.
		(void)parse_identifier();
		if (!maybe_parse_comma())
			break;
	} while (true);
	parse_semicolon();
}

static bool ordinal_constant_matches_range_type(Type* base_type, const FoldedSubrangeBound& value) {
	base_type = subrange_range_type(base_type);
	if (base_type == char_type())
		return value.kind == FoldedSubrangeBound::Kind::Char;
	if (dynamic_cast<EnumType*>(base_type))
		return value.kind == FoldedSubrangeBound::Kind::Enum && value.ty == base_type;
	return is_integer_semantic_type(base_type) && value.kind == FoldedSubrangeBound::Kind::Integer;
}

Node* Parser::parse_storage_initializer(Type* ty) {
	while (auto incomplete = dynamic_cast<IncompleteType*>(ty)) {
		if (!incomplete->resolved)
			raise_parse_error(
			    "initialized storage uses unresolved type '" +
			    incomplete->name + "'");
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
			raise_parse_error("array initializer length mismatch: expected " +
					  std::to_string(arr->range.length) + " elements but got " +
					  std::to_string(elements.size()));
		}
		return new FixedArrayLiteral(std::move(elements), ty);
	}

	auto parse_record =
	    [&](const auto& declared_fields) -> Node* {
		parse_opening_paren();
		std::vector<RecordLiteral::Field> fields;
		std::size_t next_field = 0;
		while (input_token != ")") {
			const std::string name = parse_identifier();
			auto found = std::find_if(
			    declared_fields.begin(), declared_fields.end(),
			    [&](const auto& field) {
				    return field.pas_name == name;
			    });
			if (found == declared_fields.end())
				raise_parse_error(
				    "unknown record initializer field '" +
				    name + "'");
			const std::size_t field_index =
			    static_cast<std::size_t>(
			        found - declared_fields.begin());
			if (field_index < next_field)
				raise_parse_error(
				    "record initializer field '" + name +
				    "' is repeated or out of declaration order");
			if (field_index > next_field)
				raise_parse_error(
				    "record initializer skips field(s) before '" +
				    name + "'");

			parse_colon();
			fields.push_back(RecordLiteral::Field{
			    found->slot,
			    parse_storage_initializer(found->ty),
			});
			next_field = field_index + 1;

			if (input_token != ")")
				parse_semicolon();
		}
		parse_closing_paren();
		return new RecordLiteral(std::move(fields), ty);
	};

	if (auto record = dynamic_cast<RecordType*>(ty)) {
		if (record->variant)
			raise_parse_error(
			    "variant record constant initializers are not "
			    "implemented");
		return parse_record(record->fields);
	}
	if (auto record = dynamic_cast<PackedRecordType*>(ty)) {
		if (record->variant)
			raise_parse_error(
			    "variant record constant initializers are not "
			    "implemented");
		return parse_record(record->fields);
	}

	Node* expr = parse_expression();
	ConstEvalContext ctx;
	ConstEvalResult folded = expr->const_eval(ctx);
	if (folded.kind == ConstEvalResult::Kind::NotConstant)
		raise_parse_error("constant expression expected");
	if (folded.kind == ConstEvalResult::Kind::Error)
		raise_parse_error(folded.message);
	Node* value = folded.node;

	if (auto s = dynamic_cast<SubrangeType*>(ty)) {
		OrdinalRange range;
		std::string error;
		if (!ordinal_range_for_type(s, &range, &error))
			raise_parse_error(error);
		auto bound = classify_subrange_bound(value, &error);
		if (!bound)
			raise_parse_error(error);
		if (!ordinal_constant_matches_range_type(range.base_type, *bound))
			raise_parse_error("constant initializer has incompatible ordinal type");
		if (compare_ordinal_value(bound->ordinal_value, range.lower_ordinal) < 0 ||
		    compare_ordinal_value(bound->ordinal_value, range.upper_ordinal) > 0)
			raise_parse_error("constant initializer out of range for target type");
		return cast(bound->node, subrange_range_type(s->base_type));
	}

	Node* converted = cast(value, ty);
	ConstEvalResult checked = converted->const_eval(ctx);
	if (checked.kind == ConstEvalResult::Kind::Error)
		raise_parse_error(checked.message);
	if (checked.kind == ConstEvalResult::Kind::NotConstant)
		raise_parse_error("constant expression expected");
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
			if (folded.kind == ConstEvalResult::Kind::NotConstant)
				raise_parse_error("constant expression expected");
			if (folded.kind == ConstEvalResult::Kind::Error)
				raise_parse_error(folded.message);
			if (aggregate_owner) {
				auto constant = new ConstantDecl(
				    cxx_value_name(name),
				    folded.node ? folded.node->ty : nullptr,
				    folded.node,
				    aggregate_owner);
				scope->register_variable(
				    name, constant,
				    constant->ty);
			} else {
				scope->register_variable(
				    name, folded.node,
				    folded.node ? folded.node->ty : nullptr);
			}
			parse_semicolon();
			continue;
		}
		parse_colon();
		auto ty = parse_type_expression(false);
		if (aggregate_owner) {
			if (!maybe_parse_equal())
				raise_parse_error(
				    "aggregate storage declaration requires an initializer");
			Node* initializer =
			    parse_storage_initializer(ty);
			auto slot = new StorageSlot(
			    cxx_value_name(name), ty,
			    StorageSlot::Kind::StaticMember,
			    aggregate_owner);
			slot->initializer = initializer;
			scope->register_variable(
			    name, slot, ty);
			parse_semicolon();
			continue;
		}
		auto slot = new StorageSlot(
		    cxx_value_name(name), ty);
		slot->owning_unit =
		    declaration_unit(scope);
		scope->register_variable(name, slot, ty);
		if (maybe_parse_equal()) {
			Node* initializer = parse_storage_initializer(ty);
			if (emitter)
				emitter->emit_initialized_storage_decl(
				    slot->cxx_name, ty, initializer,
				    current_routine != nullptr);
		}
		parse_semicolon();
	} while (input_token.size() && keywords.find(input_token) == keywords.end());
}
void Parser::maybe_parse_const_block() {
	if (peek_keyword("const")) {
		parse_const_block();
	}
}

struct TypeBlockResolver {
	std::unordered_set<Type*> visiting_types;
	std::unordered_set<Type*> done_types;
	std::unordered_set<IncompleteType*> resolving_incomplete;
	std::unordered_set<Node*> done_nodes;
	std::unordered_set<Frame*> done_frames;
	std::unordered_map<Type*, std::size_t>
	    completing_types;
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
		if (!ty)
			return true;
		if (auto inc = dynamic_cast<IncompleteType*>(ty)) {
			if (!inc->resolved)
				return fail("forward-referenced type not defined in this type block: " + inc->name);
			if (!resolving_incomplete.insert(inc).second) {
				// A class is already a reference-shaped Pascal type, so
				// returning or accepting the class currently being defined
				// does not recursively embed its C++ object. Keep rejecting
				// the corresponding by-value record cycle.
				if (dynamic_cast<ClassType*>(
				        inc->resolved) ||
				    dynamic_cast<InterfaceType*>(
				        inc->resolved)) {
					ty = inc->resolved;
					return true;
				}
				return fail("cyclic forward type reference involving: " + inc->name);
			}
			Type* resolved = inc->resolved;
			if (!normalize_type(resolved))
				return false;
			resolving_incomplete.erase(inc);
			ty = resolved;
			return true;
		}
		if (done_types.count(ty))
			return true;
		if (visiting_types.count(ty))
			return true;

		visiting_types.insert(ty);
		bool ok = normalize_type_contents(ty);
		visiting_types.erase(ty);
		if (ok) {
			done_types.insert(ty);
			if (auto file =
			        dynamic_cast<TypedFileType*>(ty)) {
				ty = typed_file_type(
				    file->source_location,
				    file->item_type);
				done_types.insert(ty);
			}
		}
		return ok;
	}

	template <typename T>
	bool normalize_type_as(T*& ty, const char* role) {
		if (!ty)
			return true;
		Type* resolved = ty;
		if (!normalize_type(resolved))
			return false;
		if (auto typed = dynamic_cast<T*>(resolved)) {
			ty = typed;
			return true;
		}
		return fail(std::string(role) + " resolved to " +
			    (resolved ? resolved->diagnostic_kind() : "<null>"));
	}

	bool normalize_node(Node* node) {
		if (!node)
			return true;
		if (!done_nodes.insert(node).second)
			return true;
		if (!normalize_type(node->ty))
			return false;
		if (auto n = dynamic_cast<Block*>(node))
			for (Node* statement : n->statements)
				if (!normalize_node(statement))
					return false;
		if (auto n = dynamic_cast<TypeBound*>(node)) {
			if (!normalize_type(n->operand_type))
				return false;
			n->ty = n->operand_type;
		}
		if (auto n = dynamic_cast<SizeOf*>(node)) {
			if (!normalize_type(n->operand_type))
				return false;
			n->ty = sizeint_type();
		}
		if (auto n = dynamic_cast<FixedArrayLiteral*>(node))
			for (Node* element : n->elements)
				if (!normalize_node(element))
					return false;
		if (auto n = dynamic_cast<RecordLiteral*>(node))
			for (auto& field : n->fields)
				if (!normalize_node(field.slot) ||
				    !normalize_node(field.value))
					return false;
		if (auto n = dynamic_cast<SetLiteral*>(node))
			for (auto& item : n->items)
				if (!normalize_node(item.lower) ||
				    !normalize_node(item.upper))
					return false;
		if (auto n = dynamic_cast<Construct*>(node)) {
			if (!normalize_node(n->class_reference) ||
			    !normalize_node(n->initializer))
				return false;
			for (Node* arg : n->args)
				if (!normalize_node(arg))
					return false;
		}
		if (auto n = dynamic_cast<NewValue*>(node)) {
			if (!normalize_type(n->allocated_type) ||
			    !normalize_node(n->initializer))
				return false;
			for (Node* arg : n->args)
				if (!normalize_node(arg))
					return false;
		}
		if (auto n = dynamic_cast<DisposeValue*>(node)) {
			if (!normalize_node(n->pointer) ||
			    !normalize_node(n->finalizer))
				return false;
		}
		if (auto n = dynamic_cast<UnaryOperation*>(node)) {
			if (!normalize_node(n->a))
				return false;
		}
		if (auto n = dynamic_cast<BinaryOperation*>(node)) {
			if (!normalize_node(n->a) || !normalize_node(n->b))
				return false;
		}
		if (auto n = dynamic_cast<ConstantDecl*>(node)) {
			if (!normalize_type(n->owner_type) ||
			    !normalize_node(n->initializer))
				return false;
		}
		if (auto n = dynamic_cast<StorageSlot*>(node)) {
			if (!normalize_type(n->owner_type) ||
			    !normalize_node(n->initializer))
				return false;
		}
		if (auto n = dynamic_cast<Property*>(node)) {
			for (Type*& index_type : n->index_types)
				if (!normalize_type(index_type))
					return false;
			if (!normalize_node(n->read_accessor) ||
			    !normalize_node(n->write_accessor))
				return false;
		}
		if (auto n = dynamic_cast<PropertyAccess*>(node)) {
			if (!normalize_node(n->receiver) ||
			    !normalize_node(n->property))
				return false;
			for (Node* index : n->indexes)
				if (!normalize_node(index))
					return false;
		}
		if (auto n = dynamic_cast<Coerce*>(node)) {
			if (!normalize_type(n->target_type))
				return false;
			n->ty = n->target_type;
		}
		if (auto n = dynamic_cast<CoerceCheck*>(node)) {
			if (!normalize_type(n->target_type))
				return false;
		}
		if (auto n = dynamic_cast<RoutineRef*>(node)) {
			if (!normalize_node(n->receiver) ||
			    !normalize_node(n->candidates) ||
			    !normalize_node(n->resolved))
				return false;
		}
		if (auto n = dynamic_cast<ProcCall*>(node)) {
			if (!normalize_node(n->receiver) || !normalize_node(n->callee))
				return false;
			for (auto* arg : n->args)
				if (!normalize_node(arg))
					return false;
		}
		if (auto n = dynamic_cast<ClassRefValue*>(node)) {
			Type* target = n->target;
			if (!normalize_type(target))
				return false;
			n->target = dynamic_cast<ClassType*>(target);
			if (!n->target)
				return fail(
				    "class-reference value target resolved to non-class type");
		}
		if (auto n =
		        dynamic_cast<TypeMemberQualifier*>(
		            node)) {
			if (!normalize_type(n->target))
				return false;
			if (!dynamic_cast<RecordType*>(
			        n->target) &&
			    !dynamic_cast<PackedRecordType*>(
			        n->target))
				return fail(
				    "type member qualifier target resolved to "
				    "non-record type");
			n->ty = n->target;
		}
		if (auto n = dynamic_cast<WriteCall*>(node)) {
			if (!normalize_node(n->file))
				return false;
			for (auto& item : n->items)
				if (!normalize_node(item.value) ||
				    !normalize_node(item.width) ||
				    !normalize_node(item.precision))
					return false;
		}
		if (auto n = dynamic_cast<InheritedCall*>(node)) {
			if (!normalize_node(n->resolved))
				return false;
			for (auto* arg : n->args)
				if (!normalize_node(arg))
					return false;
		}
		if (auto n = dynamic_cast<Callable*>(node)) {
			Type* routine_ty = n->ty;
			if (!normalize_type(routine_ty))
				return false;
			auto normalized_routine = dynamic_cast<RoutineType*>(routine_ty);
			if (!normalized_routine)
				return fail("callable type resolved to non-routine type");
			n->ty = normalized_routine;
			if (auto m = dynamic_cast<Method*>(n)) {
				if (!normalize_type(m->owner_class))
					return false;
			}
		}
		if (auto n = dynamic_cast<OverloadSet*>(node)) {
			for (auto* member : n->members)
				if (!normalize_node(member))
					return false;
		}
		return true;
	}

	bool normalize_frame(Frame* frame) {
		if (!frame)
			return true;
		if (!done_frames.insert(frame).second)
			return true;

		std::vector<std::pair<std::string, Type*>> local_types;
		for (const auto& item :
		     frame->type_declarations())
			local_types.push_back(item);
		for (auto& item : local_types) {
			Type* ty = item.second;
			if (!normalize_type(ty))
				return false;
			frame->rebind_type(item.first, ty);
		}

		std::vector<std::pair<std::string, FrameValueEntry>> local_values;
		for (const auto& item :
		     frame->value_declarations())
			local_values.push_back(item);
		for (auto& item : local_values) {
			Node* value = item.second.value;
			if (!normalize_node(value))
				return false;
			Type* ty = item.second.ty;
			if (!normalize_type(ty))
				return false;
			if (auto slot = dynamic_cast<StorageSlot*>(value))
				ty = slot->ty;
			else if (auto call = dynamic_cast<Callable*>(value))
				ty = call->ty ? call->ty->return_type : nullptr;
			else if (value && value->ty)
				ty = value->ty;
			frame->rebind_value_type(item.first, ty);
		}
		return true;
	}

	bool normalize_variant(VariantPart* variant) {
		if (!variant)
			return true;
		if (!normalize_type(variant->selector_type) ||
		    !normalize_node(variant->selector_slot))
			return false;
		if (variant->selector_slot)
			variant->selector_slot->ty =
			    variant->selector_type;
		for (auto& arm : variant->arms) {
			for (auto& field : arm.fields) {
				if (!normalize_type(field.ty) ||
				    !normalize_node(field.slot))
					return false;
				if (field.slot)
					field.slot->ty = field.ty;
			}
			if (!normalize_variant(arm.variant))
				return false;
		}
		return true;
	}

	bool normalize_type_contents(Type* ty) {
		// A source-declared default property is also present in the aggregate
		// frame, but the Type pointer is a semantic edge in its own right.
		// Visiting it here keeps normalization exhaustive even for lazily
		// synthesized/default-property representations.
		if (!normalize_node(ty->default_property))
			return false;
		if (dynamic_cast<IntrinsicType*>(ty) ||
		    dynamic_cast<ShortStringType*>(ty) ||
		    dynamic_cast<UnitType*>(ty) ||
		    dynamic_cast<UntypedIntegerType*>(ty) ||
		    dynamic_cast<EnumType*>(ty))
			return true;
		if (auto s = dynamic_cast<SubrangeType*>(ty)) {
			return normalize_type(s->base_type) &&
			       normalize_node(s->lower_bound) &&
			       normalize_node(s->upper_bound);
		}
		if (auto a = dynamic_cast<FixedArrayType*>(ty)) {
			if (!normalize_type(a->bounds) ||
			    !normalize_type(a->item_type))
				return false;
			std::string range_error;
			OrdinalRange range;
			if (!ordinal_range_for_type(a->bounds, &range, &range_error))
				return fail(range_error);
			a->range = range;
			return normalize_type(a->range.index_type) &&
			       normalize_type(a->range.base_type) &&
			       normalize_node(a->range.lower_bound) &&
			       normalize_node(a->range.upper_bound);
		}
		if (auto s = dynamic_cast<FixedSetType*>(ty))
			return normalize_type(s->item_type);
		if (auto f = dynamic_cast<TypedFileType*>(ty))
			return normalize_type(f->item_type);
		if (auto p = dynamic_cast<PointerType*>(ty))
			return p->is_untyped() ||
			       normalize_type(p->item_type);
		if (auto r = dynamic_cast<ClassRefType*>(ty)) {
			if (!normalize_type(r->target))
				return false;
			if (!dynamic_cast<ClassType*>(r->target))
				return fail("'class of' target resolved to " +
					    std::string(r->target ? r->target->diagnostic_kind() : "<null>"));
			return true;
		}
		if (auto rt = dynamic_cast<RoutineType*>(ty)) {
			if (!normalize_type(rt->return_type))
				return false;
			for (auto& formal : rt->formals) {
				if (!normalize_type(formal.ty) ||
				    !normalize_node(formal.default_value))
					return false;
			}
			return true;
		}
		if (auto r = dynamic_cast<RecordType*>(ty)) {
			if (!normalize_frame(r->children) ||
			    !normalize_variant(r->variant))
				return false;
			for (auto& field : r->fields) {
				if (!normalize_type(field.ty))
					return false;
				if (field.slot)
					field.slot->ty = field.ty;
				if (!normalize_node(field.slot))
					return false;
			}
			return true;
		}
		if (auto r = dynamic_cast<PackedRecordType*>(ty)) {
			if (!normalize_frame(r->children) ||
			    !normalize_variant(r->variant))
				return false;
			for (auto& field : r->fields) {
				if (!normalize_type(field.ty))
					return false;
				if (field.slot)
					field.slot->ty = field.ty;
				if (!normalize_node(field.slot))
					return false;
			}
			return true;
		}
		if (auto c = dynamic_cast<ClassType*>(ty)) {
			if (!normalize_type_as(c->super, "class superclass"))
				return false;
			for (auto*& iface : c->implemented_interfaces)
				if (!normalize_type_as(iface, "implemented interface"))
					return false;
			return normalize_node(c->class_constructor) &&
			       normalize_node(c->class_destructor) &&
			       normalize_frame(c->children);
		}
		if (auto i = dynamic_cast<InterfaceType*>(ty)) {
			for (auto*& iface : i->super_interfaces)
				if (!normalize_type_as(iface, "interface ancestor"))
					return false;
			return normalize_frame(i->children);
		}
		if (auto o = dynamic_cast<ObjectType*>(ty)) {
			if (!normalize_type_as(o->super, "object superclass"))
				return false;
			return normalize_frame(o->children);
		}
		if (auto m = dynamic_cast<ModuleType*>(ty))
			return normalize_frame(m->children);
		return true;
	}

	static bool nominal_type_anchor(Type* ty) {
		return dynamic_cast<RecordType*>(ty) ||
		       dynamic_cast<PackedRecordType*>(ty) ||
		       dynamic_cast<ClassType*>(ty) ||
		       dynamic_cast<InterfaceType*>(ty) ||
		       dynamic_cast<ObjectType*>(ty);
	}

	bool validate_complete_frame(Frame* frame) {
		if (!frame)
			return true;
		for (const auto& item :
		     frame->value_declarations()) {
			auto slot =
			    dynamic_cast<StorageSlot*>(
			        item.second.value);
			if (!slot ||
			    slot->kind !=
			        StorageSlot::Kind::AggregateMember)
				continue;
			if (!validate_complete_type(slot->ty))
				return false;
		}
		return true;
	}

	bool validate_complete_type(Type* ty) {
		if (!ty || completed_types.count(ty))
			return true;
		if (auto found =
		        completing_types.find(ty);
		    found != completing_types.end()) {
			// A legal recursive definition must be anchored by a nominal
			// aggregate. `P = ^R; R = record Next: P end` has the Record
			// identity that completes P's target. Equations made solely from
			// constructors (`T = ^T`, `file of T`, recursive routine aliases,
			// and mutual aliases) never define the promised target type.
			for (std::size_t i = found->second;
			     i < completion_path.size(); ++i)
				if (nominal_type_anchor(
				        completion_path[i]))
					return true;
			return fail(
			    "type '" + validating_type_name +
			    "' is not completely defined");
		}

		completing_types.emplace(
		    ty, completion_path.size());
		completion_path.push_back(ty);
		bool ok = true;
		if (auto array =
		        dynamic_cast<FixedArrayType*>(ty))
			ok = validate_complete_type(array->bounds) &&
			     validate_complete_type(array->item_type);
		else if (auto set =
		             dynamic_cast<FixedSetType*>(ty))
			ok = validate_complete_type(set->item_type);
		else if (auto file =
		             dynamic_cast<TypedFileType*>(ty))
			ok = validate_complete_type(file->item_type);
		else if (auto pointer =
		             dynamic_cast<PointerType*>(ty))
			ok = pointer->is_untyped() ||
			     validate_complete_type(
			         pointer->item_type);
		else if (auto class_ref =
		             dynamic_cast<ClassRefType*>(ty))
			ok = validate_complete_type(
			    class_ref->target);
		else if (auto routine =
		             dynamic_cast<RoutineType*>(ty)) {
			ok = validate_complete_type(
			    routine->return_type);
			for (const auto& formal :
			     routine->formals)
				if (ok &&
				    !validate_complete_type(
				        formal.ty))
					ok = false;
		} else if (auto subrange =
		            dynamic_cast<SubrangeType*>(ty))
			ok = validate_complete_type(
			    subrange->base_type);
		else if (auto record =
		             dynamic_cast<RecordType*>(ty))
			ok = validate_complete_frame(
			    record->children);
		else if (auto record =
		             dynamic_cast<PackedRecordType*>(ty))
			ok = validate_complete_frame(
			    record->children);
		else if (auto class_type =
		             dynamic_cast<ClassType*>(ty)) {
			ok = validate_complete_type(
			    class_type->super);
			for (InterfaceType* iface :
			     class_type->implemented_interfaces)
				if (ok &&
				    !validate_complete_type(iface))
					ok = false;
			if (ok)
				ok = validate_complete_frame(
				    class_type->children);
		} else if (auto interface_type =
		            dynamic_cast<InterfaceType*>(ty)) {
			for (InterfaceType* iface :
			     interface_type->super_interfaces)
				if (ok &&
				    !validate_complete_type(iface))
					ok = false;
			if (ok)
				ok = validate_complete_frame(
				    interface_type->children);
		} else if (auto object =
		            dynamic_cast<ObjectType*>(ty))
			ok = validate_complete_type(
			         object->super) &&
			     validate_complete_frame(
			         object->children);
		else if (auto module =
		            dynamic_cast<ModuleType*>(ty))
			ok = validate_complete_frame(
			    module->children);

		completion_path.pop_back();
		completing_types.erase(ty);
		if (ok)
			completed_types.insert(ty);
		return ok;
	}

	bool validate_aggregate_storage(Frame* frame) {
		if (!frame)
			return true;
		for (const auto& item :
		     frame->value_declarations()) {
			auto slot =
			    dynamic_cast<StorageSlot*>(
			        item.second.value);
			if (!slot ||
			    slot->kind !=
			        StorageSlot::Kind::AggregateMember)
				continue;
			if (!validate_storage_type(slot->ty))
				return false;
		}
		return true;
	}

	bool validate_storage_type(Type* ty) {
		if (!ty)
			return true;

		// These carriers do not contain an object of the referenced type.
		// Recursion through them is therefore finite: a pointer/class value,
		// routine value, set, or file handle has a fixed representation
		// independent of the referent/element definition.
		if (dynamic_cast<PointerType*>(ty) ||
		    dynamic_cast<ClassType*>(ty) ||
		    dynamic_cast<InterfaceType*>(ty) ||
		    dynamic_cast<ClassRefType*>(ty) ||
		    dynamic_cast<RoutineType*>(ty) ||
		    dynamic_cast<FixedSetType*>(ty) ||
		    dynamic_cast<TypedFileType*>(ty))
			return true;

		if (validated_storage.count(ty))
			return true;
		if (!validating_storage.insert(ty).second)
			return fail(
			    "type '" + validating_type_name +
			    "' contains itself by value");

		bool ok = true;
		if (auto array =
		        dynamic_cast<FixedArrayType*>(ty))
			ok = validate_storage_type(
			    array->item_type);
		else if (auto record =
		             dynamic_cast<RecordType*>(ty))
			ok = validate_aggregate_storage(
			    record->children);
		else if (auto record =
		             dynamic_cast<PackedRecordType*>(ty))
			ok = validate_aggregate_storage(
			    record->children);
		else if (auto object =
		             dynamic_cast<ObjectType*>(ty))
			ok = validate_aggregate_storage(
			    object->children);

		validating_storage.erase(ty);
		if (ok)
			validated_storage.insert(ty);
		return ok;
	}

	bool validate_definition(
	    Type* ty, const std::string& name) {
		if (!ty ||
		    !validated_definitions.insert(ty).second)
			return true;
		validating_type_name = name;
		if (!validate_complete_type(ty))
			return false;

		// A class/interface value is a reference, but its declaration still
		// owns fields which must themselves have finite storage. Inspect the
		// definition once; when the same class type is encountered as a field,
		// validate_storage_type correctly stops at the reference carrier.
		if (auto class_type =
		        dynamic_cast<ClassType*>(ty))
			return validate_aggregate_storage(
			    class_type->children);
		if (auto interface_type =
		        dynamic_cast<InterfaceType*>(ty))
			return validate_aggregate_storage(
			    interface_type->children);
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
	do {
		auto name_optional = maybe_parse_identifier();
		if (!name_optional)
			break;
		auto name = *name_optional;
		parse_equals();
		Type* existing = scope->lookup_type(name); // FIXME: WTF
		IncompleteType* lhs_placeholder = nullptr;
		ClassType* completing_forward = nullptr;
		bool needs_cxx_forward = false;
		if (existing) {
			lhs_placeholder = dynamic_cast<IncompleteType*>(existing);
			needs_cxx_forward =
			    lhs_placeholder &&
			    !lhs_placeholder->resolved;
			completing_forward =
			    dynamic_cast<ClassType*>(existing);
			if (completing_forward &&
			    !completing_forward
			         ->is_forward_declaration)
				completing_forward = nullptr;
			if ((!lhs_placeholder &&
			     !completing_forward) ||
			    (lhs_placeholder &&
			     lhs_placeholder->resolved)) {
				raise_type_parse_error("duplicate type name: " + name);
			}
		} else {
			lhs_placeholder = new IncompleteType(current_location(), name);
			scope->register_type(name, lhs_placeholder);
		}
		std::string saved_type_declaration_name =
		    std::move(current_type_declaration_name);
		current_type_declaration_name = name;
		Type* rhs = nullptr;
		if (completing_forward &&
		    !peek_keyword("class"))
			raise_type_parse_error(
			    "duplicate type name: " + name);
		if (peek_keyword("class"))
			rhs = parse_class_type(
			    completing_forward, true);
		else
			rhs = parse_type_expression(false);
		current_type_declaration_name =
		    std::move(saved_type_declaration_name);
		auto forward_class =
		    dynamic_cast<ClassType*>(rhs);
		if (forward_class &&
		    forward_class->is_forward_declaration) {
			// Unlike an implicit same-block IncompleteType, an explicit class
			// forward remains usable across later declaration sections. Give
			// it stable class identity and its emitted name immediately.
			forward_class->cxx_name =
			    cxx_type_name(name);
			forward_class->forward_name = name;
			forward_class->owning_unit =
			    declaration_unit(scope);
			if (lhs_placeholder)
				lhs_placeholder->resolved =
				    forward_class;
			scope->rebind_type(
			    name, forward_class);
			pending.push_back(PendingTypeDecl{
			    name, cxx_type_name(name), nullptr,
			    forward_class,
			    PendingTypeDecl::Kind::ClassForward,
			    needs_cxx_forward});
		} else {
			// Publish a completed declaration before parsing the next one.
			// References already holding this placeholder remain valid and are
			// normalized at block end; fresh lookups peel the resolved
			// placeholder. This is essential for inheritance, whose parser
			// needs the earlier class body immediately.
			if (lhs_placeholder)
				lhs_placeholder->resolved = rhs;
			pending.push_back(PendingTypeDecl{
			    name, cxx_type_name(name),
			    lhs_placeholder, rhs,
			    PendingTypeDecl::Kind::Definition,
			    needs_cxx_forward});
		}
		parse_semicolon();
	} while (true);
	type_block_frames.pop_back();

	// Every completed declaration was published during the parse loop. Resolve
	// the forward references carried inside their RHS types now that the whole
	// block has been seen, and reject missing or cyclic definitions.
	TypeBlockResolver resolver;
	for (auto& decl : pending) {
		if (decl.kind ==
		    PendingTypeDecl::Kind::ClassForward)
			continue;
		if (!resolver.normalize_type(decl.rhs))
			raise_type_parse_error(resolver.error);
		if (decl.lhs_placeholder)
			decl.lhs_placeholder->resolved =
			    decl.rhs;
	}
	for (auto& decl : pending) {
		if (decl.kind ==
		    PendingTypeDecl::Kind::ClassForward)
			continue;
		if (!resolver.validate_definition(
		        decl.rhs, decl.name))
			raise_type_parse_error(
			    resolver.error);
	}
	for (auto& decl : pending)
		if (decl.kind ==
		    PendingTypeDecl::Kind::Definition)
			scope->rebind_type(
			    decl.name, decl.rhs);

	for (auto& decl : pending) {
		if (decl.kind ==
		    PendingTypeDecl::Kind::ClassForward)
			continue;
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
		if (auto r = dynamic_cast<RecordType*>(rhs))
			existing_cxx = r->cxx_name;
		else if (auto r = dynamic_cast<PackedRecordType*>(rhs))
			existing_cxx = r->cxx_name;
		else if (auto c = dynamic_cast<ClassType*>(rhs))
			existing_cxx = c->cxx_name;
		else if (auto c = dynamic_cast<ClassRefType*>(rhs)) {
			existing_cxx = c->cxx_name;
		} else if (auto c = dynamic_cast<InterfaceType*>(rhs))
			existing_cxx = c->cxx_name;
		else if (auto o = dynamic_cast<ObjectType*>(rhs))
			existing_cxx = o->cxx_name;
		else if (auto e = dynamic_cast<EnumType*>(rhs))
			existing_cxx = e->cxx_name;
		if (!existing_cxx.empty() && existing_cxx != decl.cxx) {
			decl.alias = true;
		} else {
			// The first source name owns the canonical emitted definition.
			// Aliases retain their target's owner instead of moving that
			// declaration into the aliasing unit.
			const bool has_named_definition =
			    dynamic_cast<RecordType*>(rhs) ||
			    dynamic_cast<PackedRecordType*>(rhs) ||
			    dynamic_cast<ClassType*>(rhs) ||
			    dynamic_cast<ClassRefType*>(rhs) ||
			    dynamic_cast<InterfaceType*>(rhs) ||
			    dynamic_cast<ObjectType*>(rhs) ||
			    dynamic_cast<EnumType*>(rhs);
			if (has_named_definition &&
			    !rhs->owning_unit)
				rhs->owning_unit =
				    declaration_unit(scope);
			if (auto r = dynamic_cast<RecordType*>(rhs))
				r->cxx_name = decl.cxx;
			else if (auto r = dynamic_cast<PackedRecordType*>(rhs))
				r->cxx_name = decl.cxx;
			else if (auto c = dynamic_cast<ClassType*>(rhs))
				c->cxx_name = decl.cxx;
			else if (auto c = dynamic_cast<ClassRefType*>(rhs)) {
				c->cxx_name = decl.cxx;
			} else if (auto c = dynamic_cast<InterfaceType*>(rhs))
				c->cxx_name = decl.cxx;
			else if (auto o = dynamic_cast<ObjectType*>(rhs))
				o->cxx_name = decl.cxx;
			else if (auto e = dynamic_cast<EnumType*>(rhs))
				e->cxx_name = decl.cxx;
		}
	}
	if (!emitter)
		return;
	// C++ needs the tag declaration before an earlier Pascal pointer field
	// can name a later aggregate from the same type block. Emit all aggregate
	// tags first; by-value recursion has already been rejected above, while
	// legal pointer/class-reference recursion now has exactly the declaration
	// boundary C++20 requires.
	for (auto& decl : pending) {
		if (decl.alias ||
		    !decl.needs_cxx_forward)
			continue;
		if (decl.kind ==
		        PendingTypeDecl::Kind::ClassForward ||
		    dynamic_cast<RecordType*>(decl.rhs) ||
		    dynamic_cast<PackedRecordType*>(decl.rhs) ||
		    dynamic_cast<ClassType*>(decl.rhs) ||
		    dynamic_cast<InterfaceType*>(decl.rhs) ||
		    dynamic_cast<ObjectType*>(decl.rhs))
			emitter->emit_class_forward_declaration(
			    decl.cxx);
	}
	for (auto& decl : pending) {
		if (decl.kind ==
		    PendingTypeDecl::Kind::ClassForward)
			emitter
			    ->emit_class_forward_declaration(
			        decl.cxx);
		else if (decl.alias)
			emitter->emit_type_alias(
			    decl.cxx, decl.rhs);
		else
			emitter->emit_type_definition(decl.cxx, decl.rhs);
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
			if (names.size() != 1)
				raise_parse_error(
				    "an external variable declaration must have exactly one name");
			parse_directive("name");
			external_cxx_name = parse_string_literal();
		}
		Node* initializer = nullptr;
		if (maybe_parse_equal()) {
			if (external_cxx_name)
				raise_parse_error(
				    "an external variable cannot have an initializer");
			if (names.size() != 1)
				raise_parse_error(
				    "an initialized variable declaration must have exactly one name");
			initializer = cast(
			    parse_expression(), ty);
		}
		for (auto iter : names) {
			auto name = iter;
			// External variables name existing C++ storage, so their Pascal
			// declaration registers that name but emits no definition.
			auto slot = new StorageSlot(
			    external_cxx_name
			        ? *external_cxx_name
			        : cxx_value_name(name),
			    ty);
			if (!external_cxx_name)
				slot->owning_unit =
				    declaration_unit(scope);
			scope->register_variable(name, slot, ty);
			if (emitter && !external_cxx_name)
				emitter->emit_var_decl(
				    slot->cxx_name, ty,
				    initializer);
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
	if (!frame)
		return;
	for (const auto& declaration :
	     frame->type_declarations()) {
		auto class_type =
		    dynamic_cast<ClassType*>(
		        declaration.second);
		if (!class_type ||
		    !class_type->is_forward_declaration)
			continue;
		emit_parse_error_at(
		    class_type->source_location,
		    "forward class declaration '" +
		        class_type->forward_name +
		        "' was not resolved");
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
	validate_class_forwards(
	    current_declaration_frame());
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
			if (maybe_parse_keyword("var"))
				mode = ParamMode::Var;
			else if (maybe_parse_keyword("out"))
				mode = ParamMode::Out;
			else if (maybe_parse_keyword("const"))
				mode = ParamMode::Const;
			std::vector<std::string> names;
			names.push_back(parse_identifier());
			while (maybe_parse_comma())
				names.push_back(parse_identifier());
			Type* ty = maybe_parse_colon() ? parse_type_expression(false) : unknown_type();
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
		if (!dynamic_cast<ClassType*>(owner) &&
		    !dynamic_cast<ObjectType*>(owner)) {
			raise_type_kind_mismatch(
			    "expected class or object as constructor owner",
			    "class or object", owner);
		}
	} else if (kind == CLASS_CONSTRUCTOR ||
	           kind == CLASS_DESTRUCTOR) {
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
		if (owner || kind != ROUTINE)
			raise_type_kind_mismatch(
			    "routine type signature", "routine", owner);
		if (peek_keyword("of")) {
			parse_keyword("of");
			parse_keyword("object");
			kind = METHOD;
		}
		return new RoutineType(
		    current_location(), std::move(formals), ret_ty, kind);
	}

	if (owner) {
		if (is_class && kind == METHOD) {
			// Records permit only the receiverless `class ... static` form.
			// The directive follows the signature, so retain the provisional
			// class-method category until parse_method_prototype validates the
			// directives and changes its ABI to ROUTINE.
			if (dynamic_cast<ClassType*>(owner) ||
			    dynamic_cast<RecordType*>(owner) ||
			    dynamic_cast<PackedRecordType*>(owner))
				kind = CLASS_METHOD;
			else
				raise_type_kind_mismatch(
				    "class method owner", "class or record", owner);
		}
	} else {
		if (kind != ROUTINE)
			raise_type_kind_mismatch(
			    "expected routine", "routine", owner);
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
	// For now this is very similar to function.  Note: even parse_routine_signature uses parse_identifier() instead of parse_operator(), sigh.
	return parse_routine_signature(false, true, true, ROUTINE);
}

void Parser::parse_class_lifecycle_prototype(
    ClassType* owner_class, RoutineKind kind) {
	if (kind != CLASS_CONSTRUCTOR &&
	    kind != CLASS_DESTRUCTOR)
		raise_parse_error(
		    "internal error: invalid class lifecycle kind");
	const bool constructing =
	    kind == CLASS_CONSTRUCTOR;
	const char* lifecycle_name =
	    constructing ? "class constructor" : "class destructor";
	parse_keyword(
	    constructing ? "constructor" : "destructor");
	std::string pas_name = parse_identifier();
	RoutineType* sig = parse_routine_signature(
	    true, false, false, kind, owner_class);
	if (!sig->formals.empty())
		raise_parse_error(
		    std::string(lifecycle_name) +
		    " cannot have parameters");
	parse_semicolon();

	// These directives all describe user-callable or dispatchable overloads;
	// lifecycle hooks are instead compiler-scheduled, nonvirtual operations.
	if (peek_keyword("overload") || peek_keyword("virtual") ||
	    peek_keyword("dynamic") || peek_keyword("override") ||
	    peek_keyword("abstract") || peek_keyword("final") ||
	    peek_keyword("static"))
		raise_parse_error(
		    std::string(lifecycle_name) +
		    " cannot have routine directives");
	Method*& slot = constructing
	    ? owner_class->class_constructor
	    : owner_class->class_destructor;
	if (slot)
		raise_parse_error(
		    std::string("only one ") +
		    lifecycle_name +
		    " may be declared in a class");

	auto method = new Method(
	    constructing ? "m_init" : "m_fini",
	    pas_name, sig, false, owner_class,
	    Method::VirtualKind::None);
	method->ty = sig;
	slot = method;
	if (!current_unit)
		raise_parse_error(
		    std::string(lifecycle_name) +
		    " declared outside a unit or program");
	auto& scheduled = constructing
	    ? current_unit->class_constructors
	    : current_unit->class_destructors;
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
	RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_destructor ? DESTRUCTOR : is_constructor ? CONSTRUCTOR
															     : METHOD,
						   owner_class);
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
			if (vk == Method::VirtualKind::None &&
			    !dynamic_cast<InterfaceType*>(
			        owner_class))
				raise_parse_error(
				    "only virtual methods can be abstract");
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
		} else
			break;
	}
	if (is_static) {
		if (!is_class)
			raise_parse_error(
			    "static requires a class method declaration");
		if (is_constructor || is_destructor)
			raise_parse_error(
			    "constructors and destructors cannot be static methods");
		if (vk != Method::VirtualKind::None ||
		    is_final)
			raise_parse_error(
			    "static methods cannot be virtual, dynamic, override, "
			    "abstract, or final");
		// Static methods remain class-owned Method declarations, but their
		// value and call ABI is exactly the ordinary receiverless routine ABI.
		sig->kind = ROUTINE;
	} else if (is_class &&
	           (dynamic_cast<RecordType*>(
	                owner_class) ||
	            dynamic_cast<PackedRecordType*>(
	                owner_class))) {
		raise_parse_error(
		    "record class methods must be static");
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
		if (dynamic_cast<InterfaceType*>(
		        owner_class))
			raise_parse_error(
			    "final interface methods are not supported");
		if (vk == Method::VirtualKind::None)
			raise_parse_error(
			    "only virtual methods can be final");
	}
	if (auto object =
	        dynamic_cast<ObjectType*>(owner_class)) {
		if (is_constructor &&
		    vk != Method::VirtualKind::None)
			raise_parse_error(
			    "old-style object constructors cannot be "
			    "virtual, dynamic, override, abstract");
		if (is_constructor || is_destructor ||
		    vk != Method::VirtualKind::None)
			object->needs_vmt = true;
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
	if (vk == Method::VirtualKind::Abstract)
		m->has_body = true;
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
	if (!body->register_callable(pas_name, m)) {
		raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
	}
}

// Helper to handle overload matching and short-form implementation resolution
Procedure* Parser::match_or_create_procedure(
    const std::string& pas_name, RoutineType* sig,
    bool had_paren, bool has_overload,
    bool short_form_implementation) {
	Frame* enclosing = current_declaration_frame();
	Node* existing = enclosing->lookup_value(pas_name);

	auto sig_matches = [&](Callable* c) -> bool {
		auto rty = static_cast<RoutineType*>(c->ty);
		if (rty->formals.size() != sig->formals.size())
			return false;
		if (rty->return_type != sig->return_type)
			return false;
		for (size_t i = 0; i < sig->formals.size(); i++) {
			if (rty->formals[i].ty != sig->formals[i].ty)
				return false;
			if (rty->formals[i].mode != sig->formals[i].mode)
				return false;
		}
		return true;
	};

	auto attach_to = [&](Callable* c) -> Procedure* {
		if (c->has_body)
			raise_parse_error("duplicate implementation of '" + pas_name + "'");
		if (had_paren) {
			// Pascal allows impl parameter names to differ from interface names.
			// We update the prototype's names so local scope bindings match the body text.
			static_cast<RoutineType*>(c->ty)->formals = sig->formals;
		}
		auto p = dynamic_cast<Procedure*>(c);
		if (!p)
			raise_parse_error("'" + pas_name + "' is not a standalone procedure");
		return p;
	};

	Procedure* target = nullptr;
	auto consider_existing = [&](Node* node) {
		if (!node || target)
			return;
		if (auto ec = dynamic_cast<Callable*>(node)) {
			if ((short_form_implementation || sig_matches(ec)) &&
			    !ec->has_body)
				target = attach_to(ec);
		} else if (auto os = dynamic_cast<OverloadSet*>(node)) {
			if (short_form_implementation) {
				Callable* pick = nullptr;
				for (auto* m : os->members) {
					if (!m->has_body) {
						if (pick)
							raise_parse_error("ambiguous short-form impl");
						pick = m;
					}
				}
				if (pick)
					target = attach_to(pick);
			} else {
				for (auto* m : os->members) {
					if (!m->has_body && sig_matches(m)) {
						if (target)
							raise_parse_error("ambiguous overload match");
						target = attach_to(m);
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
	if (!target && short_form_implementation && existing)
		raise_parse_error("no unimplemented prototype");

	if (!target) {
		target = new Procedure(cxx_value_name(pas_name), pas_name, sig, has_overload);
		target->ty = sig;
		target->owning_unit =
		    declaration_unit(enclosing);
		if (!enclosing->register_callable(pas_name, target)) {
			raise_parse_error("duplicate identifier or overload directive mismatch: " + pas_name);
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
			if (auto owner =
			        dynamic_cast<ClassType*>(
			            m->owner_class))
				owner_qualifier =
				    new ClassRefValue(owner);
			else
				owner_qualifier =
				    new TypeMemberQualifier(
				        m->owner_class);
			push_scope(
			    owner_frame, owner_qualifier);
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
			if (target->ty->kind == CLASS_METHOD ||
			    target->ty->kind == CLASS_CONSTRUCTOR ||
			    target->ty->kind == CLASS_DESTRUCTOR) {
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
			if (target->ty->kind != CLASS_CONSTRUCTOR &&
			    target->ty->kind != CLASS_DESTRUCTOR)
				body_frame->register_variable(
				    "self", receiver_slot, self_ty);
			push_scope(owner_frame, receiver_slot);
			pushed_owner_scope = true;
		}
	}
	push_scope(body_frame);
	push_declaration_frame(body_frame);
	if (target->ty->return_type != &unit_type()) { // function
		auto result_slot = new StorageSlot("p_result", target->ty->return_type);
		body_frame->register_variable("result", result_slot, target->ty->return_type);
	}
	auto rty = static_cast<RoutineType*>(target->ty);
	for (auto& p : rty->formals) {
		body_frame->register_variable(p.pas_name, new StorageSlot(p.cxx_name, p.ty), p.ty);
	}
	if (emitter)
		emitter->emit_procedure_open(target, nested_lambda);
	parse_decl_blocks(false);
	parse_keyword("begin");
	parse_block_body();
	target->has_body = true;
	parse_keyword("end");
	parse_semicolon();
	if (emitter)
		emitter->emit_procedure_close(target, nested_lambda);
	pop_declaration_frame();
	pop_scope(); // pop body_frame
	if (pushed_owner_scope)
		pop_scope(); // pop the with_scope
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

void Parser::parse_procedure_or_function(bool is_class, bool is_function, bool is_decl_only) {
	bool has_overload = false;
	std::string first_name;
	bool is_destructor = false;
	bool is_constructor = false;
	if (peek_keyword("operator")) {
		if (!is_function) {
			raise_parse_error("custom operator should have a return value");
		}
		parse_keyword("operator");
		// Assumption: there are no method operators.
		first_name = input_token; // TODO: well, parse_operator();
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
		if (!owner_frame)
			raise_parse_error("'" + first_name + "' is not a class/record/object");
		if (is_class &&
		    (is_constructor || is_destructor)) {
			auto class_type = dynamic_cast<ClassType*>(owner_ty);
			if (!class_type)
				raise_parse_error(
				    "class lifecycle implementation requires a class");
			const bool constructing = is_constructor;
			const char* lifecycle_name =
			    constructing
			        ? "class constructor"
			        : "class destructor";
			RoutineKind kind = constructing
			    ? CLASS_CONSTRUCTOR
			    : CLASS_DESTRUCTOR;
			Method* method = constructing
			    ? class_type->class_constructor
			    : class_type->class_destructor;
			if (!method || method->pas_name != method_name)
				raise_parse_error(
				    "no " +
				    std::string(lifecycle_name) +
				    " '" + method_name +
				    "' on '" + first_name + "'");
			RoutineType* sig = parse_routine_signature(
			    true, false, false, kind,
			    owner_ty);
			if (!sig->formals.empty())
				raise_parse_error(
				    std::string(lifecycle_name) +
				    " cannot have parameters");
			parse_semicolon();
			if (method->has_body)
				raise_parse_error(
				    "duplicate implementation of " +
				    std::string(lifecycle_name) +
				    " '" +
				    method_name + "'");
			parse_routine_body(method, owner_frame);
			return;
		}
		// if (is_destructor) {
		//	if (auto class_type = dynamic_cast<ClassType*>(owner_ty)) {
		//		method_name = "~" + class_type->cxx_name;
		//	}
		// }
		Node* hit =
		    owner_frame->lookup_value(method_name);
		auto m = dynamic_cast<Method*>(hit);
		// An out-of-line `T.Method` body implements a declaration owned by
		// exactly T. Normal Frame lookup may find an inherited method, but
		// owner identity rejects it without bypassing structural lookup or
		// exposing the frame's declaration table by name.
		if (!m || m->owner_class != owner_ty)
			raise_parse_error("no method '" + method_name + "' on '" + first_name + "'");
		const bool declaration_is_class_method =
		    m->is_static ||
		    m->ty->kind == CLASS_METHOD;
		if (is_class != declaration_is_class_method)
			raise_parse_error(
			    "method implementation does not match its class "
			    "method declaration");
		RoutineType* sig = parse_routine_signature(is_class, is_function, false, is_constructor ? CONSTRUCTOR : is_destructor ? DESTRUCTOR
																      : METHOD,
							   owner_ty);
		parse_semicolon();
		while (maybe_parse_keyword("inline")) {
			// FIXME: use
			parse_semicolon();
		}
		while (maybe_parse_keyword("noreturn")) {
			// FIXME: use
			parse_semicolon();
		}
		if (m->has_body)
			raise_parse_error("duplicate implementation of '" + method_name + "'");
		// Pascal permits implementation parameter names to differ from the
		// prototype, but defaults belong to the prototype/call site. Replacing
		// the complete formal objects here erased those defaults as soon as an
		// out-of-line body was parsed.
		auto declaration_sig =
		    static_cast<RoutineType*>(m->ty);
		if (sig->formals.size() !=
		    declaration_sig->formals.size())
			raise_parse_error(
			    "implementation parameter count does not match "
			    "method declaration");
		for (size_t i = 0;
		     i < sig->formals.size(); ++i) {
			declaration_sig->formals[i].pas_name =
			    sig->formals[i].pas_name;
			declaration_sig->formals[i].cxx_name =
			    sig->formals[i].cxx_name;
		}
		parse_routine_body(m, owner_frame);
		return;
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
		// FPC mode permits overloaded standalone/global routines without an
		// explicit `overload` directive. Keep this policy at the parser call site:
		// Frame is also used as class/object/record member storage, and making
		// Frame::register_callable globally permissive would silently change method
		// overload rules. Method prototypes still use their parsed directive bit.
		has_overload = true;
		/*
		In an INTERFACE section there is this:
		  function x: Integer;
		  const Foo = 'Hello';
		That const Foo is supposed to be global, not in the function.
		*/
		body_follows = body_follows && (peek_keyword("begin") || peek_keyword("label") ||
						peek_keyword("var") || peek_keyword("const") || peek_keyword("type") ||
						peek_keyword("procedure") || peek_keyword("function"));
		// A routine declaration in an interface section has the signature it
		// writes, including zero parameters when parentheses are absent. Only
		// an implementation with a body may omit an earlier prototype's
		// parameter list.
		const bool short_form_implementation =
		    !is_decl_only && body_follows && !had_paren;
		Procedure* target = match_or_create_procedure(
		    first_name, sig, had_paren, has_overload,
		    short_form_implementation);
		if (external_cxx_name) {
			target->owning_unit = nullptr;
			auto builtin = lookup_external_value(nullptr, *external_cxx_name);
			if (builtin != nullptr) {
				// This is basically making TARGET an ALIAS for BUILTIN.
				target->has_body = true;
				target->is_external = true;
				if (auto qbuiltin = dynamic_cast<Builtin*>(builtin)) { // used
					// These Builtins are all polymorphic and C++ overloads will just have to adjust to us.
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
			if (emitter)
				emitter->emit_callable_prototype(target, "", "", "");
		}
	}
}

// Per-visible-argument conversion costs for a candidate. Returns an empty
// vector when the candidate is not viable. Pascal binds a method receiver
// separately and performs single dispatch after selecting a declaration from
// its member family, so Self is deliberately not an overload-ranking
// position. Entries past args.size() are zero for default-supplied formals.
static bool is_ordinal_intrinsic_argument(Type* ty) {
	while (auto subrange = dynamic_cast<SubrangeType*>(ty))
		ty = subrange->base_type;
	if (dynamic_cast<EnumType*>(ty))
		return true;
	OrdinalBounds bounds;
	return intrinsic_ordinal_bounds(ty, &bounds);
}

static std::vector<int> per_arg_costs(
    Callable* c, const std::vector<Node*>& args) {
	auto rty = static_cast<RoutineType*>(c->ty);
	const BuiltinDesc* builtin = lookup_builtin_desc(c->cxx_name);

	if (args.size() > rty->formals.size())
		return {};
	for (size_t i = args.size(); i < rty->formals.size(); i++) {
		if (!rty->formals[i].default_value)
			return {};
	}
	std::vector<int> costs(rty->formals.size(), 0);
	for (size_t i = 0; i < args.size(); i++) {
		Type* from = args[i] ? args[i]->ty : nullptr;
		if (builtin &&
		    builtin->generic_kind == BuiltinGenericKind::Assigned &&
		    i == 0 &&
		    (dynamic_cast<RoutineType*>(from) ||
		     (from && from->is_reference_type()))) {
			costs[i] = 0;
			continue;
		}
		if (rty->formals[i].ty == unknown_type()) {
			if (builtin &&
			    (builtin->generic_kind == BuiltinGenericKind::OrdinalValue ||
			     builtin->generic_kind == BuiltinGenericKind::OrdinalMutation)) {
				if (!is_ordinal_intrinsic_argument(from))
					return {};
			}
			// This is a constrained generic match. It is viable for every
			// accepted type but should lose to a concrete exact overload.
			costs[i] = 1000;
			continue;
		}
			if (auto literal = dynamic_cast<String*>(args[i])) {
				if (literal->value.size() == 1 &&
				    dynamic_cast<ShortStringType*>(
				        rty->formals[i].ty)) {
				// One-byte quoted literals begin as Char, but Pascal also
				// permits them in a string context. Keep this worse than an
				// exact Char overload; cast() pins the selected string type.
				costs[i] = 1;
				continue;
			}
		}
		auto mode = rty->formals[i].mode;
		if (mode == ParamMode::Var || mode == ParamMode::Out) {
			if (from == rty->formals[i].ty) {
				costs[i] = 0;
				continue;
			}
			if (rty->formals[i].ty == pointer_type() &&
			    dynamic_cast<PointerType*>(from)) {
				// FPC's GetMem(out Pointer, ...) accepts storage of any
				// typed pointer. Preserve that storage type for C++ template
				// deduction instead of casting the out argument to void*&.
				costs[i] = 1;
				continue;
			}
			if (auto subrange = dynamic_cast<SubrangeType*>(from)) {
				if (subrange->base_type == rty->formals[i].ty) {
					costs[i] = 1;
					continue;
				}
			}
			return {};
		}
		int cc = conversion_cost(from, rty->formals[i].ty);
		if (cc < 0)
			return {};
		costs[i] = cc;
	}
	return costs;
}

Node* Parser::cast(Node* a, Type* target_ty) {
	// `nil` literal: legal for reference types and both routine-value
	// categories. Adopt the surrounding target so emission can choose nullptr
	// for a plain procedure or a two-null-word method value.
	if (dynamic_cast<NilLiteral*>(a)) {
		if (target_ty &&
		    (target_ty->is_reference_type() ||
		     dynamic_cast<RoutineType*>(target_ty))) {
			a->ty = target_ty;
			return a;
		}
		raise_parse_error(
		    "'nil' is only valid in a reference or routine-value context");
	}
	if (auto reference = dynamic_cast<RoutineRef*>(a)) {
		if (target_ty == pointer_type())
			return resolve_routine_code_reference(reference);
		auto routine = dynamic_cast<RoutineType*>(target_ty);
		if (!routine)
			raise_parse_error(
			    "a routine reference requires a routine-type context");
		return resolve_routine_reference(reference, routine);
	}
	if (auto literal = dynamic_cast<SetLiteral*>(a)) {
		if (auto target_set = dynamic_cast<FixedSetType*>(target_ty)) {
			if (!is_set_item_type(target_set->item_type))
				raise_parse_error("set target item type is not ordinal");
			for (SetLiteral::Item& item : literal->items) {
				item.lower = cast(item.lower, target_set->item_type);
				if (item.upper)
					item.upper = cast(item.upper, target_set->item_type);
			}
			literal->ty = target_ty;
			return literal;
		}
	}
	if (a->ty == target_ty) {
		return a;
	}
	// A one-character Pascal quoted literal is contextually a Char as well as
	// a one-character string. Preserve one literal node and pin its type when
	// the surrounding assignment/case/formal requires Char.
	if (auto literal = dynamic_cast<String*>(a)) {
		if (target_ty == char_type() && literal->value.size() == 1) {
			literal->ty = target_ty;
			return literal;
		}
			if (dynamic_cast<ShortStringType*>(target_ty)) {
				literal->ty = target_ty;
			return literal;
		}
	}
	// Built-in implicit conversions are already described by conversion_cost().
	// Do not route them through user-visible operator := overloads: those overloads
	// are ordinary Pascal conversion operators with their own result type, while a
	// contextual cast has already chosen TARGET_TY. Sending e.g. qword -> int64
	// through the global := overload set lets unrelated qword -> Tconstexprint
	// operators compete and produces bogus ambiguities.
	if (target_ty && conversion_cost(a->ty, target_ty) >= 0) {
		if (a->ty == &untyped_integer_type()) {
			a->ty = target_ty;
			return a;
		}
		return new Cast(a, target_ty);
	} else if (target_ty == unknown_type()) { // this target is void* but the formal parameter is more like a reference
		return new Cast(new AddrOf(a), target_ty);
	} else {
		auto fn = resolve_value(":=");
		std::vector<Node*> args;
		args.push_back(a);
		auto fc = finalize_call(fn, args, /*name for error*/ "", current_location(), target_ty);
		return make_call(fc, std::move(args));

		// return new Cast(a, target_ty); // FIXME.
	}
}

static std::vector<Callable*> routine_reference_candidates(
    Node* candidates_node) {
	if (auto callable =
	        dynamic_cast<Callable*>(candidates_node))
		return {callable};
	if (auto overloads =
	        dynamic_cast<OverloadSet*>(candidates_node))
		return overloads->members;
	return {};
}

Node* Parser::resolve_routine_reference(
    RoutineRef* reference, RoutineType* target_ty) {
	if (!reference || !target_ty)
		raise_parse_error("invalid contextual routine reference");
	if (target_ty->kind != ROUTINE && target_ty->kind != METHOD)
		raise_parse_error(
		    "routine reference target is not a routine-value type");

	std::vector<Callable*> candidates =
	    routine_reference_candidates(
	        reference->candidates);
	if (candidates.empty())
		raise_parse_error(
		    "routine reference does not name a routine or method");

	std::vector<Callable*> compatible;
	for (Callable* candidate : candidates) {
		bool category_matches = false;
		if (target_ty->kind == ROUTINE) {
			category_matches =
			    (dynamic_cast<Procedure*>(
			         candidate) &&
			     candidate->ty->kind == ROUTINE &&
			     reference->receiver == nullptr) ||
			    (dynamic_cast<Method*>(
			         candidate) &&
			     static_cast<Method*>(
			         candidate)->is_static &&
			     candidate->ty->kind == ROUTINE);
		} else {
			category_matches =
			    dynamic_cast<Method*>(candidate) &&
			    !static_cast<Method*>(
			         candidate)->is_static &&
			    (candidate->ty->kind == METHOD ||
			     candidate->ty->kind == CLASS_METHOD) &&
			    reference->receiver != nullptr;
		}
		if (category_matches &&
		    routine_types_compatible(candidate->ty, target_ty))
			compatible.push_back(candidate);
	}

	if (compatible.empty()) {
		std::string category = target_ty->kind == METHOD
		    ? "procedure/function of object"
		    : "plain procedure/function";
		raise_parse_error(
		    "no overload of the routine reference is compatible with " +
		    category + " target");
	}
	if (compatible.size() != 1)
		raise_parse_error(
		    "routine reference is ambiguous for the destination "
		    "routine type");
	if (target_ty->kind == METHOD &&
	    reference->receiver &&
	    !(reference->receiver->ty &&
	      reference->receiver->ty->is_reference_type()) &&
	    !is_referenceable(reference->receiver))
		raise_parse_error(
		    "an of-object routine reference requires a stable "
		    "object receiver");

	reference->resolved = compatible.front();
	reference->ty = target_ty;
	if (auto method =
	        dynamic_cast<Method*>(
	            reference->resolved);
	    method && method->is_static) {
		Node* qualifier = reference->receiver;
		reference->receiver = nullptr;
		if (qualifier &&
		    !dynamic_cast<ClassRefValue*>(
		        qualifier) &&
		    !dynamic_cast<TypeMemberQualifier*>(
		        qualifier))
			return new EvaluateThen(
			    qualifier, reference);
	}
	return reference;
}

Node* Parser::resolve_routine_code_reference(
    RoutineRef* reference) {
	if (!reference)
		raise_parse_error("invalid routine code reference");
	std::vector<Callable*> candidates =
	    routine_reference_candidates(
	        reference->candidates);
	if (candidates.size() != 1)
		raise_parse_error(
		    "a Pointer routine reference requires one "
		    "non-overloaded routine");
	Callable* candidate = candidates.front();
	bool valid_plain =
	    (dynamic_cast<Procedure*>(candidate) &&
	     candidate->ty->kind == ROUTINE &&
	     reference->receiver == nullptr) ||
	    (dynamic_cast<Method*>(candidate) &&
	     static_cast<Method*>(candidate)->is_static &&
	     candidate->ty->kind == ROUTINE);
	bool valid_method =
	    dynamic_cast<Method*>(candidate) &&
	    !static_cast<Method*>(candidate)->is_static &&
	    (candidate->ty->kind == METHOD ||
	     candidate->ty->kind == CLASS_METHOD) &&
	    reference->receiver != nullptr;
	if (!valid_plain && !valid_method)
		raise_parse_error(
		    "Pointer routine reference does not name a plain "
		    "routine or bound instance method");
	if (valid_method &&
	    !(reference->receiver->ty &&
	      reference->receiver->ty->is_reference_type()) &&
	    !is_referenceable(reference->receiver))
		raise_parse_error(
		    "a bound method code reference requires a stable "
		    "object receiver");

	reference->resolved = candidate;
	reference->code_only = true;
	reference->ty = pointer_type();
	if (auto method =
	        dynamic_cast<Method*>(candidate);
	    method && method->is_static) {
		Node* qualifier = reference->receiver;
		reference->receiver = nullptr;
		if (qualifier &&
		    !dynamic_cast<ClassRefValue*>(
		        qualifier) &&
		    !dynamic_cast<TypeMemberQualifier*>(
		        qualifier))
			return new EvaluateThen(
			    qualifier, reference);
	}
	return reference;
}

Parser::FinalizedCall Parser::finalize_call(Node* target,
					   std::vector<Node*>& args,
					   std::string name_for_error,
					   SourceLocation error_location,
					   Type* expected_return_type) {
	// Peel MemberAccess: if the member is callable, its container is the
	// receiver and the member is the effective callee.
	Node* receiver = nullptr;
	if (auto ma = dynamic_cast<MemberAccess*>(target)) {
		if (dynamic_cast<Callable*>(ma->b) || dynamic_cast<OverloadSet*>(ma->b)) {
			if (!dynamic_cast<UnitRef*>(ma->a))
				receiver = ma->a;
			target = ma->b;
		}
	}
	Callable* chosen = nullptr;
	RoutineType* value_rty = nullptr;
	Node* qualifier_effect = nullptr;
	if (auto c = dynamic_cast<Callable*>(target)) {
		if (name_for_error.empty() && !c->pas_name.empty()) {
			name_for_error = c->pas_name;
		}
		if (expected_return_type &&
		    static_cast<RoutineType*>(c->ty)->return_type != expected_return_type) {
			std::vector<Callable*> candidates{c};
			std::vector<std::pair<Callable*, std::vector<int>>> viable;
			std::vector<Callable*> none;
			raise_overload_resolution_error(error_location, name_for_error, receiver, args,
				expected_return_type, candidates, viable, none, false);
		}
		chosen = c;
	} else if (auto os = dynamic_cast<OverloadSet*>(target)) {
		std::vector<Callable*> candidates = os->members;
		std::vector<std::pair<Callable*, std::vector<int>>> viable;
		for (auto* c : candidates) {
			if (name_for_error.empty() && !c->pas_name.empty()) {
				name_for_error = c->pas_name;
			}
			auto costs = per_arg_costs(c, args);
			if (!costs.empty() &&
			    (!expected_return_type ||
			     static_cast<RoutineType*>(c->ty)->return_type == expected_return_type))
				viable.push_back({c, std::move(costs)});
		}
		if (viable.empty()) {
			std::vector<Callable*> none;
			raise_overload_resolution_error(error_location, name_for_error, receiver, args,
				expected_return_type, candidates, viable, none, false);
		}
		std::vector<Callable*> non_dominated;
		for (size_t i = 0; i < viable.size(); i++) {
			bool dom = false;
			for (size_t j = 0; j < viable.size(); j++) {
				if (i != j && dominates(viable[j].second, viable[i].second)) {
					dom = true;
					break;
				}
			}
			if (!dom)
				non_dominated.push_back(viable[i].first);
		}
		if (non_dominated.size() != 1) {
			raise_overload_resolution_error(error_location, name_for_error, receiver, args,
				expected_return_type, candidates, viable, non_dominated, true);
		}
		chosen = non_dominated[0];
	} else {
		value_rty = target
		    ? dynamic_cast<RoutineType*>(target->ty)
		    : nullptr;
		if (!value_rty) {
			// Builtin or other opaque callable -- no ranking/default checks.
			return FinalizedCall{receiver, target};
		}
		if (expected_return_type &&
		    value_rty->return_type != expected_return_type)
			emit_parse_error_at(
			    error_location,
			    "routine value has the wrong result type");
	}
	if (auto method = dynamic_cast<Method*>(chosen)) {
		if (method->is_static) {
			if (method->ty->kind != ROUTINE)
				emit_parse_error_at(
				    error_location,
				    "static method does not have routine ABI");
			// FPC evaluates an explicit object/class-reference qualifier
			// exactly once even though a static method neither dereferences
			// nor receives it. Exact type/class designators are compile-time
			// member selectors and have no runtime evaluation.
			if (receiver &&
			    !dynamic_cast<ClassRefValue*>(
			        receiver) &&
			    !dynamic_cast<TypeMemberQualifier*>(
			        receiver))
				qualifier_effect = receiver;
			receiver = nullptr;
		} else if (!receiver) {
			emit_parse_error_at(
			    error_location,
			    "method '" + name_for_error +
			        "' has no bound receiver");
		}
		auto receiver_class_ref =
		    receiver
		    ? dynamic_cast<ClassRefType*>(
		          receiver->ty)
		    : nullptr;
		switch (method->ty->kind) {
		case CLASS_METHOD: {
			auto owner =
			    dynamic_cast<ClassType*>(
			        method->owner_class);
			ClassType* actual = receiver_class_ref
			    ? dynamic_cast<ClassType*>(
			          receiver_class_ref->target)
			    : dynamic_cast<ClassType*>(
			          receiver->ty);
			bool compatible = false;
			for (ClassType* current = actual; current;
			     current = current->super)
				if (current == owner) {
					compatible = true;
					break;
				}
			if (!owner || !compatible)
				emit_parse_error_at(
				    error_location,
				    "class method '" +
				        name_for_error +
				        "' has an incompatible receiver");
			break;
		}
		case CONSTRUCTOR:
			// A class reference forms a construction expression; an
			// existing object invokes the same declaration as an
			// initializer method. The two applications are separated
			// after overload selection.
			break;
		case METHOD:
		case DESTRUCTOR:
			if (dynamic_cast<TypeMemberQualifier*>(
			        receiver))
				emit_parse_error_at(
				    error_location,
				    "instance method '" +
				        name_for_error +
				        "' cannot be called through a type");
			if (receiver_class_ref)
				emit_parse_error_at(
				    error_location,
				    "instance method '" +
				        name_for_error +
				        "' cannot be called through a class reference");
			break;
		case CLASS_CONSTRUCTOR:
		case CLASS_DESTRUCTOR:
			emit_parse_error_at(
			    error_location,
			    "class lifecycle hook is not user-callable");
			break;
		case ROUTINE:
			if (!method->is_static)
				emit_parse_error_at(
				    error_location,
				    "non-static method declaration has routine ABI");
			break;
		}
	}
	// Materialize missing args from defaults.
	auto rty = chosen
	    ? static_cast<RoutineType*>(chosen->ty)
	    : value_rty;
	while (args.size() < rty->formals.size()) {
		auto& p = rty->formals[args.size()];
		if (!p.default_value) {
			emit_parse_error_at(error_location, "missing argument for parameter '" + p.pas_name + "' in call to '" + name_for_error + "'");
		}
		args.push_back(p.default_value);
	}
	if (args.size() > rty->formals.size()) {
		emit_parse_error_at(error_location, "too many arguments to '" + name_for_error + "'");
	}
	const BuiltinDesc* builtin = chosen
	    ? lookup_builtin_desc(chosen->cxx_name)
	    : nullptr;
	if (builtin &&
	    (builtin->generic_kind == BuiltinGenericKind::OrdinalValue ||
	     builtin->generic_kind == BuiltinGenericKind::OrdinalMutation)) {
		if (args.empty() || !is_ordinal_intrinsic_argument(args[0] ? args[0]->ty : nullptr)) {
			emit_parse_error_at(error_location,
				name_for_error + " requires an ordinal argument");
		}
	}
	if (builtin &&
	    builtin->generic_kind == BuiltinGenericKind::SetMutation) {
		// Until tpcc supports generic routine declarations, system.pp has to
		// spell these as `(var values; const item)`. Recover the otherwise
		// unexpressed `set of T`/`T` relationship from the first argument's
		// actual FixedSetType.
		auto set_type = args.empty()
		    ? nullptr
		    : dynamic_cast<FixedSetType*>(args[0] ? args[0]->ty : nullptr);
		if (!set_type) {
			emit_parse_error_at(error_location,
			    name_for_error + " requires a set variable as its first argument");
		}
		if (args.size() < 2 || !args[1]) {
			emit_parse_error_at(error_location,
			    name_for_error + " requires a set element as its second argument");
		}
		// FPC converts the element expression to the concrete set element
		// type before generating the bit mutation. Do that here while the
		// Pascal type is available; both omitted-type RTL formals can then
		// retain their exact, related types at the C++ call boundary.
		args[1] = cast(args[1], set_type->item_type);
	}
	for (size_t i = 0; i < args.size(); i++) {
		if (!chosen) {
			Type* from = args[i] ? args[i]->ty : nullptr;
			Type* to = rty->formals[i].ty;
			ParamMode mode = rty->formals[i].mode;
			bool compatible =
			    dynamic_cast<NilLiteral*>(args[i]) ||
			    (dynamic_cast<RoutineRef*>(args[i]) &&
			     dynamic_cast<RoutineType*>(to));
			if (mode == ParamMode::Var ||
			    mode == ParamMode::Out) {
				compatible = from == to;
				if (auto subrange =
				        dynamic_cast<SubrangeType*>(from))
					compatible =
					    subrange->base_type == to;
			} else if (!compatible) {
				compatible =
				    conversion_cost(from, to) >= 0;
			}
			if (!compatible)
				emit_parse_error_at(
				    error_location,
				    "argument for routine value parameter '" +
				    rty->formals[i].pas_name +
				    "' has an incompatible type");
		}
		auto mode = rty->formals[i].mode;
		if (mode != ParamMode::Var && mode != ParamMode::Out)
			continue;
		if (!is_referenceable(args[i])) {
			emit_parse_error_at(error_location,
				"argument for var/out parameter '" + rty->formals[i].pas_name + "' is not a storage-backed expression");
		}
		if (contains_packed_projection(args[i])) {
			emit_parse_error_at(error_location,
				"packed-record field cannot yet be passed as var/out parameter '" +
				rty->formals[i].pas_name + "'");
		}
	}
	// Insert Cast for any arg whose type differs from the formal.
	for (size_t i = 0; i < args.size(); i++) {
		Type* t = rty->formals[i].ty;
		if (builtin &&
		    builtin->generic_kind == BuiltinGenericKind::Assigned &&
		    args[i] &&
		    (dynamic_cast<RoutineType*>(args[i]->ty) ||
		     (args[i]->ty &&
		      args[i]->ty->is_reference_type())))
			continue;
		if (t == unknown_type())
			continue; // untyped formal preserves the argument's exact Pascal type
		auto mode = rty->formals[i].mode;
		if (mode == ParamMode::Var || mode == ParamMode::Out) {
			// Integer subranges use their base type as their C++ storage
			// carrier. Preserve that storage expression: casting it would
			// manufacture an rvalue and lose the var/out destination.
			if (auto subrange = dynamic_cast<SubrangeType*>(args[i]->ty))
				if (subrange->base_type == t)
					continue;
			// FPC permits a typed pointer variable for an out Pointer formal.
			// The RTL template receives T*& and performs a real C++ pointer
			// conversion from malloc's void* result.
			if (t == pointer_type() &&
			    dynamic_cast<PointerType*>(args[i]->ty))
				continue;
		}
		args[i] = cast(args[i], t);
	}
	return FinalizedCall{
	    receiver, chosen ? static_cast<Node*>(chosen) : target,
	    qualifier_effect};
}

Node* Parser::make_call(
    FinalizedCall finalized, std::vector<Node*> args) {
	if (auto initializer =
	        dynamic_cast<Method*>(finalized.callee);
	    initializer &&
	    initializer->ty->kind == CONSTRUCTOR &&
	    finalized.receiver) {
		if (auto class_reference =
		        dynamic_cast<ClassRefType*>(
		            finalized.receiver->ty)) {
			auto result_type =
			    dynamic_cast<ClassType*>(
			        class_reference->target);
			if (!result_type)
				raise_parse_error(
				    "constructor class reference does not target a class");
			return new Construct(
			    finalized.receiver, initializer,
			    std::move(args), result_type);
		}
	}
	auto call = new ProcCall(
	    finalized.receiver, finalized.callee,
	    std::move(args));
	call->ty = call_result_type(finalized.callee);
	if (!finalized.qualifier_effect)
		return call;
	return new EvaluateThen(
	    finalized.qualifier_effect, call);
}

Unit* Parser::load_or_get_unit(std::string name) {
	if (Unit* existing = unit_registry->lookup(name))
		return existing;
	std::vector<std::string> empty;
	const auto& paths = options ? options->unit_search_paths : empty;
	FILE* f = nullptr;
	std::string opened;
	for (auto ext : {".pp", ".pas"}) {
		std::tie(f, opened) = search_for_file(name + ext, input_file_name, paths);
		if (f)
			break;
	}
	if (!f)
		raise_parse_error("cannot find unit file for: " + name);
	// Per-unit Emitter writes <output_dir>/<name>.h and .cc. The nested Parser
	// gets its own token/scope state plus this emitter; the shared
	// unit_registry is what lets circular-dep detection work across the two.
	std::string dir = options ? options->output_dir : "";
	Emitter unit_emitter;
	unit_emitter.open_for_unit(name, dir);
	Parser sub(unit_registry, &unit_emitter, options);
	sub.push_input_file(f, opened, 1);
	sub.start();
	sub.parse_program_or_unit();
	unit_emitter.close();
	Unit* loaded = unit_registry->lookup(name);
	if (!loaded)
		raise_parse_error("file '" + opened + "' did not declare 'unit " + name + ";'");
	return loaded;
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
		if (!maybe_parse_comma())
			break;
	} while (true);
	return loaded;
}

Unit* Parser::implicit_uses(std::string user_name) {
	if (strcasecmp(user_name.c_str(), "system") == 0)
		return nullptr;
	return load_or_get_unit("system");
}

void Parser::parse_unit_body() {
	std::string name = parse_identifier();
	parse_semicolon();
	// Top-level invocation: open the emitter for the unit shape (.h + .cc).
	// Sub-parsers spawned by load_or_get_unit arrive with an already-open
	// unit emitter; only the top-level parser (main -> parse_program_or_unit
	// -> "unit" branch) hits this path with an unopened emitter.
	if (emitter && !emitter->is_open())
		emitter->open_for_unit(name, options ? options->output_dir : "");
	Frame* unit_frame = new Frame(nullptr);
	Unit* unit = unit_registry->register_new(name, unit_frame);
	current_unit = unit;
	unit->phase = UnitPhase::InterfaceInProgress;

	parse_keyword("interface");
	// Install the interface lookup path in increasing precedence. Declaration
	// ownership is independent and is set explicitly after this path exists.
	// Used-unit frames remain separate lookup entries: their declarations are
	// never inserted into this unit frame and therefore cannot be re-exported
	// through it.
	std::vector<Unit*> iface_units;
	if (Unit* sys = implicit_uses(name))
		iface_units.push_back(sys);
	if (maybe_parse_keyword("uses")) {
		auto used = parse_uses_clause(true, name);
		for (Unit* u : used)
			iface_units.push_back(u);
		parse_semicolon();
	}
	for (Unit* used : iface_units)
		push_scope(used->frame, used->reference);
	push_scope(unit_frame);
	push_declaration_frame(unit_frame);
	if (emitter) {
		emitter->set_section(Emitter::Section::Header);
		std::vector<std::string> h_files;
		for (Unit* u : iface_units)
			h_files.push_back(u->name + ".h");
		emitter->emit_unit_interface_prologue(
		    unit->cxx_namespace, h_files);
	}
	parse_decl_blocks(true);
	if (emitter)
		emitter->emit_unit_interface_epilogue();
	unit->phase = UnitPhase::InterfaceDone;

	parse_keyword("implementation");
	unit->phase = UnitPhase::ImplementationInProgress;
	std::vector<Unit*> impl_units;
	if (maybe_parse_keyword("uses")) {
		impl_units = parse_uses_clause(false, name);
		parse_semicolon();
	}
	// Rebuild only the lookup path for the implementation's precedence:
	// this unit, implementation uses, interface uses. Interface and
	// implementation declarations deliberately keep the same Frame identity.
	pop_declaration_frame();
	pop_scope(); // unit
	for (Unit* used : impl_units)
		push_scope(used->frame, used->reference);
	push_scope(unit_frame);
	push_declaration_frame(unit_frame);
	if (emitter) {
		emitter->set_section(Emitter::Section::Implementation);
		std::vector<std::string> h_files;
		for (Unit* u : impl_units)
			h_files.push_back(u->name + ".h");
		emitter->emit_unit_implementation_prologue(
		    unit->cxx_namespace, name + ".h", h_files);
	}
	// Implementation section: same dispatcher; procedures with bodies attach
	// to the interface prototypes via the lookup-then-adopt path in
	// parse_procedure_or_function.
	parse_decl_blocks(false);

	bool consumed_end = false;
	if (emitter)
		emitter->emit_unit_lifecycle_open(
		    unit->initialization_cxx_name);
	if (!unit->class_constructors.empty()) {
		unit->has_initialization = true;
		for (Method* method : unit->class_constructors)
			if (emitter)
				emitter->emit_class_lifecycle_call(method);
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
	if (emitter)
		emitter->emit_unit_lifecycle_close();

	if (emitter)
		emitter->emit_unit_lifecycle_open(
		    unit->finalization_cxx_name);
	if (!consumed_end &&
	    maybe_parse_keyword("finalization")) {
		unit->has_finalization = true;
		parse_unit_statement_sequence(false);
	}
	if (!unit->class_destructors.empty()) {
		unit->has_finalization = true;
		// FPC appends lifecycle destructors after the user's finalization
		// statements. It uses the same declaration-order structure walk as
		// constructors, so parents precede descendants here as well.
		for (Method* method : unit->class_destructors)
			if (emitter)
				emitter->emit_class_lifecycle_call(method);
	}
	if (emitter)
		emitter->emit_unit_lifecycle_close();

	if (!consumed_end)
		parse_keyword("end");
	parse_period();
	if (emitter)
		emitter->emit_unit_implementation_epilogue();

	pop_declaration_frame();
	pop_scope(); // unit
	for (size_t i = 0; i < impl_units.size(); i++)
		pop_scope();
	for (size_t i = 0; i < iface_units.size(); i++)
		pop_scope();

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
		if (emitter && !emitter->is_open())
			emitter->open_for_program(options ? options->program_output_path : "");
		Frame* program_frame = new Frame(nullptr);
		Unit* unit =
		    unit_registry->register_new(
		        name, program_frame, true);
		current_unit = unit;
		unit->phase = UnitPhase::InterfaceInProgress;
		// Load dependencies, then install the lookup path in increasing
		// precedence. The program's declaration owner is set independently.
		std::vector<Unit*> prog_units;
		if (Unit* sys = implicit_uses(name))
			prog_units.push_back(sys);
		if (maybe_parse_keyword("uses")) {
			auto used = parse_uses_clause(false, name);
			for (Unit* u : used)
				prog_units.push_back(u);
			parse_semicolon();
		}
		for (Unit* used : prog_units)
			push_scope(used->frame, used->reference);
		push_scope(program_frame);
		push_declaration_frame(program_frame);
		push_statement_control_context();
		if (emitter) {
			std::vector<std::string> h_files;
			for (Unit* u : prog_units)
				h_files.push_back(u->name + ".h");
			emitter->emit_program_prologue(h_files);
		}
		// Inlined equivalent of parse_block; we need to bracket the body-block
		// with main() emission hooks, which parse_block itself doesn't know
		// about (it's also called from procedure bodies).
		parse_decl_blocks(false);
		parse_keyword("begin");
		if (emitter) {
			std::vector<UnitLifecycleNames>
			    lifecycle_hooks;
			for (Unit* used :
			     unit_registry->completed_units()) {
				if (!used->has_initialization &&
				    !used->has_finalization)
					continue;
				lifecycle_hooks.push_back(
				    UnitLifecycleNames{
				        used->cxx_namespace,
				        used->initialization_cxx_name,
				        used->finalization_cxx_name});
			}
			emitter->emit_main_prologue(
			    lifecycle_hooks,
			    unit->class_destructors);
			// Program-local class hooks have the same position as unit class
			// hooks: dependency units are ready, while user program statements
			// have not begun.
			for (Method* method : unit->class_constructors)
				emitter->emit_class_lifecycle_call(method);
			// Arm program finalization only after every program class
			// constructor completed. This mirrors the per-unit rule that a
			// partially initialized lifecycle is not finalized.
			if (!unit->class_destructors.empty())
				emitter
				    ->emit_program_finalizer_registration();
		}
		parse_block_body();
		parse_keyword("end");
		if (emitter)
			emitter->emit_main_epilogue(
			    !unit->class_destructors.empty());
		pop_statement_control_context();
		pop_declaration_frame();
		pop_scope(); // program
		for (size_t i = 0; i < prog_units.size(); i++)
			pop_scope();
		parse_period();
		unit->phase = UnitPhase::Done;
	} else if (maybe_parse_keyword("unit")) {
		parse_unit_body();
	} else {
		raise_parse_error("unknown input token");
	}
}
