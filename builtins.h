// Builtins and intrinsics: things that exist in the root Frame before any
// Pascal source is parsed.
//
// Two descriptor tables (defined in builtins.cc) bind each Pascal name to a
// C++ symbol from rtl.h. Adding an entry means appending a row here AND
// implementing the rtl.h symbol; the linker catches a mismatch when emitted
// output is compiled.
//
//   kIntrinsicTypes   - Pascal type names visible at the root (Integer, etc.)
//   kBuiltins         - Pascal procedures/functions visible at the root
//                       (Ord, Inc, Dec, ...)
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include "cst.h"
#include "frame.h"

struct BuiltinDesc {
	std::string_view pas_name;    // lowercase, matches tokenizer output
	std::string_view rtl_name;    // e.g. "pas::p_ord"
	Type* (*build_type)();        // signature; called once at registration
	// Constant-folder; nullable when a row has no folding rule (see also the
	// TODO next to kBuiltins in builtins.cc).
	std::optional<uint64_t> (*const_fold)(Frame*, Node* args);
};

struct IntrinsicTypeDesc {
	std::string_view pas_name;    // lowercase
	std::string_view rtl_name;    // e.g. "pas::t_integer"
};

class IntrinsicType: public Type {
public:
	std::string_view pas_name;
	std::string_view rtl_name;
	IntrinsicType(std::string_view pas_name, std::string_view rtl_name);
};

class Builtin: public Node {
public:
	const BuiltinDesc* desc;
	Builtin(const BuiltinDesc* desc);
};

// The single, program-wide root frame. Holds intrinsic types (Integer,
// Boolean, ...) and builtin procedures/functions (Ord, Inc, Dec, ...).
// Function-local static: initialized on first call, no cross-TU static-init
// order dependency. Every Parser pushes this frame at the bottom of its
// scope stack, so intrinsic Type* identities are shared across parsers.
const Frame& root_frame();
