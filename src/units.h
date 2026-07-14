#pragma once
#include <map>
#include <string>
#include <vector>

class Frame;
class Method;

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

/** A Unit is a named container of declarations. Programs and units share this
 *  representation:
 *   - unit:    interface_frame = exported decls, implementation_frame = private
 *              decls + routine bodies (parent = interface_frame).
 *   - program: interface_frame = null (nothing to export), implementation_frame
 *              = everything (main block + local decls). */
class Unit {
public:
	std::string name;
	Frame* interface_frame;      // null for programs
	Frame* implementation_frame; // always present
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
	Unit(std::string name, Frame* interface_frame, Frame* implementation_frame);
};

/** Global-per-compilation map of unit name -> Unit*. Loading via `uses` goes
 *  through here so units are shared and circular deps are detectable. */
class UnitRegistry {
private:
	std::map<std::string, Unit*> units;
	std::vector<Unit*> completed;
public:
	/** Insert a new Unit for NAME. Aborts if NAME is already present. */
	Unit* register_new(std::string name, Frame* interface_frame, Frame* implementation_frame);
	/** Returns the Unit for NAME, or nullptr if not registered. */
	Unit* lookup(std::string name) const;
	/** Record a fully parsed unit. Recursive parsing makes this the unit
	 *  initialization order: every dependency is recorded before its user. */
	void record_completed(Unit* unit);
	const std::vector<Unit*>& completed_units() const { return completed; }
};
