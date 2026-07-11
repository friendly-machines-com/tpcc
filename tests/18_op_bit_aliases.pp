// Verifies `&` (alias for `and`) and `|` (alias for `or`) -- symbol spellings
// of the word operators, for code that prefers C-style bit/bool syntax.
//
//   1. `&` resolves to the same operator as `and` (same precedence as `*`
//      and `/`, and same runtime symbol).
//   2. `|` resolves to the same operator as `or` (same precedence as `+`
//      and `-`, and same runtime symbol).
//   3. Precedence is preserved: `a & b | c & d` parses as `(a & b) | (c & d)`,
//      matching the `and`/`or` grouping, since `&` binds tighter than `|`.
//
// Aliases exist as syntactic convenience, not as separate operators: they
// share their word-form's precedence and runtime.
program p;
var
  a, b, c, d: Boolean;
begin
  a := true; b := false; c := true; d := false;
  a := a & b;            // case 1: & = and; expect false
  a := a | b;            // case 2: | = or;  expect true
  a := a & b | c & d     // case 3: precedence; (a&b)|(c&d) = false|false = false
end.
