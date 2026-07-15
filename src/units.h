#pragma once
#include <map>
#include <string>
#include <vector>

class Frame;
class Method;

/** C++ namespace token used to lower a Pascal unit. Pascal identifiers have
 * already been normalized to lowercase by the tokenizer. */
std::string cxx_unit_name(const std::string& pascal_name);

/** Where along the parse timeline a Unit currently is. Values are ordered so
 *  transitions only go forward. A `uses` at interface time that resolves to a
 *  Unit still in InterfaceInProgress is the circular-interface-dependency
 *  signal (enforced by Parser::parse_uses_clause). */
enum class UnitPhase {
	Unparsed,
	InterfaceInProgress,
	InterfaceDone,
	ImplementationInProgress,
	Done,
};

/** A Unit is a named container of declarations. A Pascal unit has one member
 *  frame: the implementation continues mutating the frame populated by the
 *  interface, just as reopening the generated C++ namespace continues adding
 *  members to the same namespace. General Pascal member visibility is not
 *  represented yet; `interface` versus `implementation` currently controls
 *  emission into the header versus the .cc, not a second symbol identity.
 *
 *  Programs use the same storage shape, but `is_program` prevents their local
 *  declarations from being treated as names owned by a Pascal unit namespace. */
class Unit {
public:
	std::string name;
	std::string cxx_namespace;
	Frame* frame;
	bool is_program;
	UnitPhase phase;
	std::string initialization_cxx_name;
	std::string finalization_cxx_name;
	bool has_initialization = false;
	bool has_finalization = false;
	// Source order is also parent-before-child for classes declared in one
	// unit: Pascal requires a superclass to be complete before it is used as
	// a parent. Dependency units are initialized before this unit, so their
	// parent hooks have already run.
	std::vector<Method*> class_constructors;
	Unit(std::string name, Frame* frame, bool is_program);
};

/** Global-per-compilation map of unit name -> Unit*. Loading via `uses` goes
 *  through here so units are shared and circular deps are detectable. */
class UnitRegistry {
private:
	std::map<std::string, Unit*> units;
	std::vector<Unit*> completed;
public:
	/** Insert a new Unit for NAME. Aborts if NAME is already present. */
	Unit* register_new(std::string name, Frame* frame, bool is_program = false);
	/** Returns the Unit for NAME, or nullptr if not registered. */
	Unit* lookup(std::string name) const;
	/** Record a fully parsed unit. Recursive parsing makes this the unit
	 *  initialization order: every dependency is recorded before its user. */
	void record_completed(Unit* unit);
	const std::vector<Unit*>& completed_units() const { return completed; }
};
