#pragma once
#include <cstdint>
#include <optional>
#include "cst.h"

class Type;

struct Builtin: public Node {
    virtual std::optional<uint64_t> evaluate(Node* args) {
        return {};
    }
    virtual std::optional<uint64_t> evaluate_type(Type* args) {
        return {};
    }
};

struct Ord: public Builtin {
};

struct Inc: public Builtin {
};

struct Dec: public Builtin {
};
