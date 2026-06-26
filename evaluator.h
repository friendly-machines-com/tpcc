#pragma once
#include <optional>

struct Scope;

std::optional<uint64_t> evaluate(Scope* scope, Node* v);
