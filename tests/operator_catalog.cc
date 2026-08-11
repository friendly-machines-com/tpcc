#include "../src/operators.h"

#include <cassert>

int main() {
	auto explicit_conversion =
	    explicit_operator_identifier();
	assert(explicit_conversion ==
	       "&op_Explicit");

	auto bitwise_xor = operator_invocation_identifier(
	    OperatorInvocation::BinaryToken,
	    "xor", 2, false, false);
	assert(bitwise_xor);
	assert(*bitwise_xor == "&op_BitwiseXor");

	bool saw_initialize = false;
	for (const OperatorSpec& spec : operator_catalog()) {
		assert(spec.invocation_spelling != "<>");
		assert(spec.declaration_name != "notequal");
		if (spec.declaration_name != "initialize")
			continue;
		saw_initialize = true;
		assert(!spec.declaration_supported);
	}
	assert(saw_initialize);
}
