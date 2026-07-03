#include "evaluator.h"
#include "cst.h"
#include "frame.h"
#include <cstdint>
#include <optional>

/** if it can be evaluated, return the result.  Otherwise, not. */
std::optional<uint64_t> evaluate(Frame* frame, Node* v) {
	// TODO: stub. Implement strict evaluation, argument-first.
	return {};
}
