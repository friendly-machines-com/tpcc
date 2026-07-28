#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "types.h"

class Node;
class StorageSlot;
class Callable;

struct CallableRegistration {
	enum class Kind {
		Added,
		Rejected,
		CxxCarrierCollision,
	};

	Kind kind;
	/** The declaration already stored under this name. For an overload set,
	 * this is the complete set, so a diagnostic can show every declaration
	 * that was available when registration failed. */
	Node* existing_binding = nullptr;
	/** The exact member of existing_binding which conflicts with the incoming
	 * declaration. This is retained separately because an overload-set
	 * diagnostic must identify the relevant pair as well as show the family. */
	Callable* conflicting_callable = nullptr;
};

struct FrameValueEntry {
	Node* value;
	Type* ty;
	FrameValueEntry();
	FrameValueEntry(Node* value, Type* ty);
};

/** One declaration bound to one spelling in one physical Frame. Type and
 * value objects deliberately remain unrelated C++ hierarchies; the variant
 * is the type-safe tag saying which kind of declaration the name denotes. */
using Binding = std::variant<Type*, Node*>;
using NamedBinding = std::pair<std::string, Binding>;

/** A Frame is the storage for one declaration block (the result of `var x,y,z:
 *  Integer;` or the body of a record/class/object/unit). A frame's optional
 *  `parent` pointer captures STRUCTURAL relationships (nested class,
 *  subclass-to-superclass), not lexical lookup chains; those live in the
 *  Parser's `scopes` stack of frames.
 *
 *  Required-type lookup and ordinary expression lookup are distinct:
 *  lookup_type() skips values while walking outward, whereas
 *  lookup_type_or_value() stops at the nearest declaration of either kind.
 *  That distinction permits `X: X` when the field/parameter X is nearer than
 *  an outer type X, while making a nearer local type High win over a farther
 *  System.High in the ambiguous expression `High(1)`. */
class Frame {
private:
	std::map<std::string, Binding> items;
public:
	Frame* parent; // NOT invasive from Parser
public:
    // TODO: kind of frame (unit, record, class, ...); maybe also bool auto_unwrap; for "uses" and "with" blocks

    Frame(Frame* parent);
    /** Nearest binding of either kind through this structural Frame chain. */
    std::optional<Binding> lookup_type_or_value(std::string name) const;
    /** Local-only form used by Parser while walking lexical scope entries. */
    std::optional<Binding> lookup_type_or_value_local(std::string name) const;
    Type* lookup_type(std::string name) const;
    /** Resolve NAME in this structural frame chain. The current frame is
     *  searched first; a callable marked overload may extend its family into
     *  parent frames, while every other hit shadows the remaining parents. */
    Node* lookup_value(std::string name) const; /* result: usually a StorageSlot */
    bool register_type(std::string name, Type* ty);
    /** Replace an existing type binding (used when patching a placeholder with
     *  its real Type* at type-block-end). No-op-safe for a fresh name. */
    void rebind_type(std::string name, Type* ty);
    /** Compatibility hook for type-block normalization. A Frame no longer
     * stores a second value-type cache; the Node owns its normalized type. */
    void rebind_value_type(std::string name, Type* ty);
    bool register_variable(std::string name, Node* v, Type* ty); // FIXME: StorageSlot would already have ty
    /** Collect a Callable while an aggregate declaration is still being
     *  constructed inside an open Pascal type block. This operation performs
     *  no signature, type-identity, or C++-carrier comparisons: previously
     *  stored signature edges may still be IncompleteType placeholders even
     *  when a fresh lookup of the same Pascal name already returns its
     *  resolved Type. The owning aggregate must validate the resulting local
     *  family after TypeBlockResolver has recursively normalized the block and
     *  before any emission or statement lookup can observe it. */
    CallableRegistration collect_callable(
        std::string name, Callable* c);
    /** Register an already-normalized Callable (Procedure or Method) under
     *  NAME. Distinct Pascal signatures owned by this one Frame form a local
     *  OverloadSet regardless of the `overload` directive; the retained
     *  per-Callable directive bit controls whether lookup may extend that
     *  completed family into a parent Frame or lexical scope. The result
     *  retains the existing binding and exact conflicting callable:
     *  discarding them here would prevent declaration diagnostics from
     *  printing the prior source location and complete overload family.
     *
     *  Do not call this on signatures stored in an open type block. Use
     *  collect_callable(), normalize the complete block, and then apply
     *  validate_callable_pair() to the collected family. */
    CallableRegistration register_callable(
        std::string name, Callable* c);
    /** Declaration ownership query, not name lookup. This intentionally
     *  returns only a boolean: callers that need a value must use
     *  lookup_value(), which applies the structural parent chain. */
    bool declares_value(const std::string& name) const {
	    auto found = items.find(name);
	    return found != items.end() &&
		   std::holds_alternative<Node*>(found->second);
    }
    /** Read-only snapshots for existing emission/diagnostic walks. The sole
     * associative table remains private, so these cannot become lookup APIs. */
    std::vector<std::pair<std::string, Type*>>
    type_declarations() const;
    std::vector<std::pair<std::string, FrameValueEntry>>
    value_declarations() const;
    std::vector<NamedBinding> bindings_local() const;
};

/** Whether BINDING's Pascal overload directive opens the family into the
 *  next enclosing environment. Shared by structural Frame lookup and the
 *  parser's lexical scope walk so this rule has one implementation. */
bool callable_binding_opens_parent(Node* binding);

/** Whether two declarations may be members of one Pascal OverloadSet.
 * Parameter signatures select members only after this declaration category
 * agrees; incompatible routine kinds never become candidates of one family. */
bool same_callable_overload_category(
    Callable* a, Callable* b);

/** Validate one pair of declarations which have already been collected under
 * one Pascal name. Every Type* edge reachable from either signature must have
 * passed type-block normalization first; raw pointer identity and C++ carrier
 * equivalence are deliberately final semantic decisions, not operations on
 * the parser's temporary IncompleteType graph. */
CallableRegistration::Kind validate_callable_pair(
    Callable* existing, Callable* incoming);

/** Backend-only collision test for two already-distinct Pascal callables.
 * It compares the emitted member/free-function name and C++ parameter
 * carriers; callers use it to reject an unrepresentable overload or an
 * accidental C++ virtual override, never to perform Pascal lookup. Both
 * signatures must already have been recursively normalized. */
bool cxx_callable_signatures_collide(
    Callable* a, Callable* b);
