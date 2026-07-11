// Verifies the `**` exponentiation operator. Three behaviors pinned:
//
//   1. Basic binary use: 2 ** 10 = 1024.
//
//   2. Unary-minus-vs-** parsing: `-2 ** 4` reads as `-(2 ** 4)` = -16,
//      not `(-2) ** 4` = +16. Math convention is ambiguous here (both
//      readings have defenders); mini-pascal picks the FPC-compatible
//      reading so existing Pascal code transplants cleanly.
//
//   3. Right-associativity: `2 ** 3 ** 2` = `2 ** (3 ** 2)` = 512, not
//      `(2 ** 3) ** 2` = 64. Math: `(a^b)^c = a^(b*c)`, so left-associativity
//      would make the chained form redundant with `a ** (b*c)`. Universal
//      convention in math and most languages (Python, Fortran, R).
program p;
var
  a: Integer;
begin
  a := 2 ** 10;       // case 1: basic; expect 1024
  a := -2 ** 4;       // case 2: unary-minus reads as -(2**4); expect -16
  a := 2 ** 3 ** 2    // case 3: right-assoc per math; expect 512
end.
