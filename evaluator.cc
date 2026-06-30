#include <cstdint>
#include <optional>
#include "evaluator.h"
#include "cst.h"
#include "scope.h"

/** if it can be evaluated, return the result.  Otherwise, not. */
std::optional<uint64_t> evaluate(Scope* scope, Node* v) {
	// TODO: stub. Implement strict evaluation, argument-first.
	return {};
}
