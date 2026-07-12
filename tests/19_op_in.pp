// Verifies `in` (set membership) placement at the relational precedence
// level -- same level as `=`, `<>`, `<`, `>`, `<=`, `>=`.
//
// Why this precedence: `in` is a relationship test (returns Boolean, like
// the comparison operators). It belongs with them so that typical
// expressions like `(a + b) in S` group naturally (arithmetic binds
// tighter, set membership tests the result), and `x in S` is available
// wherever a Boolean expression is expected.
//
// Pinned case: `n in [1, 2, 3]` parses with `in` at the relational level
// and lowers the typed set constructor plus membership test through the RTL.
program p;
var
  n: Integer;
  flag: Boolean;
begin
  n := 2;
  flag := n in [1, 2, 3]   // `in` at relational level; RHS is a set literal
end.
