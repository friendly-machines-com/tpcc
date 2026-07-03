#include "units.h"
#include "frame.h"
#include <cstdio>
#include <cstdlib>

Unit::Unit(std::string name, Frame* interface_frame, Frame* implementation_frame)
    : name(name),
      interface_frame(interface_frame),
      implementation_frame(implementation_frame),
      phase(UnitPhase::Unparsed) {}

Unit* UnitRegistry::register_new(std::string name, Frame* interface_frame, Frame* implementation_frame) {
	if (units.count(name)) {
		fprintf(stderr, "internal compiler error: unit '%s' already registered\n", name.c_str());
		abort();
	}
	Unit* u = new Unit(name, interface_frame, implementation_frame);
	units[name] = u;
	return u;
}

Unit* UnitRegistry::lookup(std::string name) const {
	auto it = units.find(name);
	if (it == units.end())
		return nullptr;
	return it->second;
}
