#pragma once

class Node;
class Type;

struct Builtin: public Node {
    virtual std::
    virtual std::optional<uint64_t> evaluate(Node* args) {
        return {};
    }
    virtual std::optional<uint64_t> evaluate(Type* args) {
        return {};
    }
};

struct Ord: public Builtin {
};

struct Inc: public Bulitin {
};

struct Dec: public Builtin {
};
