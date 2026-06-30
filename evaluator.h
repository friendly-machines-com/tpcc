#pragma once
#include <cstdint>
#include <optional>

class Node;
class Scope;

std::optional<uint64_t> evaluate(Scope* scope, Node* v);
