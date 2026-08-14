#define TPCC_TEST_GENERATED_PROGRAM "function_pointers.cc"
#include "generated_program_runtime.h"

namespace {

struct AdapterPrefix {
	int prefix = 0;
};

struct AdapterOwner {
	int value = 0;

	int add(int amount) {
		value += amount;
		return value;
	}
};

struct AdapterDerived : AdapterPrefix, AdapterOwner {};

} // namespace

int main() {
	AdapterDerived adjusted_object;
	auto adjusted_method = ::u_system::m_bind_method<static_cast<int (AdapterOwner::*)(int)>(&AdapterOwner::add)>(&adjusted_object);
	auto adjusted_raw = ::u_system::m_method_to_tmethod(adjusted_method);
	if (adjusted_raw.p_data != static_cast<void*>(static_cast<AdapterOwner*>(&adjusted_object))) {
		return 8;
	}
	if (adjusted_method(3) != 3) {
		return 9;
	}

	p_receiver = new t_tchild;
	p_otherreceiver = new t_tchild;
	p_receiver->p_factor = 10;
	p_otherreceiver->p_factor = 20;
	static_cast<t_tchild*>(p_receiver)->p_childfactor = 10;
	static_cast<t_tchild*>(p_otherreceiver)->p_childfactor = 20;

	int result = tpcc_run_generated_program();
	if (result != 0) {
		return 1;
	}
	if (p_functionresult != 61 || p_mutationresult != 8) {
		return 2;
	}
	if (p_autocallvalueargument != 42 ||
	    p_autocallroutineargument != 42) {
		return 11;
	}
	if (p_resultvalue != 126) {
		return 3;
	}
	if (p_methodfunctionresult != 130) {
		return 4;
	}
	if (p_plaincodeequal != ::u_system::p_true ||
	    p_methodcodeequal != ::u_system::p_true ||
	    p_autocallequal != ::u_system::p_true) {
		return 5;
	}
	if (p_plainnil != ::u_system::p_true || p_plainnilequal != ::u_system::p_true || p_methodnil != ::u_system::p_true || p_methodnilequal != ::u_system::p_true) {
		return 6;
	}
	if (p_rawcode == nullptr || p_rawdata != static_cast<::u_system::t_pointer>(p_receiver) || p_globalcode == nullptr || p_globalcodefromvalue == nullptr || p_methodcode == nullptr) {
		return 7;
	}
	if (p_staticpointercastresult != static_cast<::u_system::t_pointer>(p_receiver) || p_methodpointercastresult != static_cast<::u_system::t_pointer>(p_otherreceiver)) {
		return 10;
	}

	delete p_receiver;
	delete p_otherreceiver;
	p_receiver = nullptr;
	p_otherreceiver = nullptr;
	return 0;
}
