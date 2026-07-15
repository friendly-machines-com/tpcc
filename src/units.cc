#include "units.h"
#include "frame.h"
#include <cstdio>
#include <cstdlib>

std::string cxx_unit_name(const std::string& pascal_name) {
	return "u_" + pascal_name;
}

Unit::Unit(std::string name, Frame* frame, bool is_program)
    : name(name),
      cxx_namespace(cxx_unit_name(name)),
      frame(frame),
      is_program(is_program),
      phase(UnitPhase::Unparsed),
      initialization_cxx_name("tpcc_initialize_" + name),
      finalization_cxx_name("tpcc_finalize_" + name) {}

Unit* UnitRegistry::register_new(
    std::string name, Frame* frame, bool is_program) {
	if (units.count(name)) {
		fprintf(stderr, "internal compiler error: unit '%s' already registered\n", name.c_str());
		abort();
	}
	Unit* u = new Unit(name, frame, is_program);
	units[name] = u;
	return u;
}

Unit* UnitRegistry::lookup(std::string name) const {
	auto it = units.find(name);
	if (it == units.end())
		return nullptr;
	return it->second;
}

void UnitRegistry::record_completed(Unit* unit) {
	if (!unit || unit->phase != UnitPhase::Done) {
		fprintf(stderr, "internal compiler error: recording an incomplete unit\n");
		abort();
	}
	for (Unit* existing : completed) {
		if (existing == unit) {
			fprintf(stderr, "internal compiler error: unit '%s' completed twice\n",
			        unit->name.c_str());
			abort();
		}
	}
	completed.push_back(unit);
}
