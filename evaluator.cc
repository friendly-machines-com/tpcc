#include <string>
#include <optional>
#include <cstdint>

/** if it can be evaluated, return the result.  Otherwise, not. */
std::optional<uint64_t> evaluate(Scope* scope, Node* v) {
	// note: needs to do strict evaluation, with argument first.
}
