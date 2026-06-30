#pragma once
#include <cstdint>
#include <optional>

class Node;
class Frame;

std::optional<uint64_t> evaluate(Frame* frame, Node* v);
